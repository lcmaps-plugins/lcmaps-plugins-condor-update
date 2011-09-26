
/*
 * lcmaps-condor-update
 * By Brian Bockelman, 2011 
 * This code is under the public domain
 */

/*****************************************************************************
                            Include header files
******************************************************************************/

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/fcntl.h>

#include "lcmaps/lcmaps_modules.h"
#include "lcmaps/lcmaps_cred_data.h"
#include "lcmaps/lcmaps_arguments.h"

#define CONDOR_CHIRP_PATH "/usr/libexec/condor/condor_chirp"
#define CONDOR_CHIRP_NAME "condor_chirp"
#define CONDOR_SCRATCH_DIR "_CONDOR_SCRATCH_DIR"

char * logstr = "lcmaps-condor-update";

// ClassAd attributes we'll use for the update
#define CLASSAD_GLEXEC_DN "glexec_x509userproxysubject"
#define CLASSAD_GLEXEC_USER "glexec_user"
#define CLASSAD_GLEXEC_TIME "glexec_time"

#define TIME_BUFFER_SIZE 12

int update_starter_child(const char * attr, const char *val, int fd) {
  size_t len;
  char *environ_tmp = NULL;
  int result = 1;
  char result_buf[TIME_BUFFER_SIZE];

  char *current_scratch = getenv(CONDOR_SCRATCH_DIR);
  if (current_scratch) {
    len = strlen(CONDOR_SCRATCH_DIR) + 1 + strlen(current_scratch) + 1;
    if ((environ_tmp=(char *)malloc(len)) == NULL) {
      lcmaps_log(0, "%s: Unable to malloc memory for environment entry in child.\n", logstr);
      goto condor_update_fail_child;
    }
    if (snprintf(environ_tmp, len, "%s=%s", CONDOR_SCRATCH_DIR, environ_tmp) >= len) {
      lcmaps_log(0, "%s: Buffer overflow when constructing environment entry.\n", logstr);
      goto condor_update_fail_child;
    }
  }
  char * environ[2] = {environ_tmp, NULL};

  char * my_attr = (char *)malloc(strlen(attr) + 1); strcpy(my_attr, attr);
  char *my_val = (char *)malloc(strlen(val) + 1); strcpy(my_val, val);
  char *const argv[] = {CONDOR_CHIRP_NAME,
               "set_job_attr",
               my_attr,
               my_val,
               NULL
              };

  if ((result = execve(CONDOR_CHIRP_PATH, argv, environ)) == -1) {
    lcmaps_log(0, "%s: Exec of condor_chirp failed: %d %s\n", errno, strerror(errno));
  }

condor_update_fail_child:
  len = snprintf(result_buf, TIME_BUFFER_SIZE, "%d", result);
  write(fd, result_buf, len);
  flush(fd);
  if (environ_tmp)
    free(environ_tmp); // Considering the next line, this might be the most useless free ever.
  _exit(result);
}

int update_starter(const char * attr, const char * val) {
  int fork_pid;
  int fd_flags;
  int rc, exit_code;
  int p2c[2];
  FILE * fh;

  if (attr == NULL) {
    lcmaps_log(0, "%s: Internal error - passed a NULL attribute\n", attr);
    return 1;
  }
  if (val == NULL) {
    lcmaps_log(0, "%s: Internal error - passed a NULL value\n", attr);
    return 1;
  }

  if (pipe(p2c) < 0) {
    lcmaps_log(0, "%s: Failed to create an internal pipe: %d %s\n", logstr, errno, strerror(errno));
    return errno;
  }
  if ((fd_flags = fcntl(p2c[1], F_GETFD, NULL)) == -1) {
    lcmaps_log(0, "%s: Failed to get fd flags: %d %s\n", logstr, errno, strerror(errno));
    return errno;
  }
  if (fcntl(p2c[1], F_SETFD, fd_flags | O_CLOEXEC) == -1) {
    lcmaps_log(0, "%s: Failed to set new fd flags: %d %s\n", logstr, errno, strerror(errno));
    return errno;
  }

  fork_pid = fork();
  if (fork_pid == -1) {
    lcmaps_log(0, "%s: Failed to fork a new child process: %d %s\n", logstr, errno, strerror(errno));
    close(p2c[0]); close(p2c[1]);
    return errno;
  } else if (fork_pid == 0) { // Child
    close(p2c[0]);
    update_starter_child(attr, val, p2c[1]);
    // Does not return.  Just in case:
    _exit(1);
  }

  close(p2c[1]);
  if ((fh = fdopen(p2c[0], "r")) == NULL) {
    lcmaps_log(0, "%s: Failed to reopen file descriptor as file handle: %d %s", logstr, errno, strerror(errno));
    close(p2c[0]);
    return errno;
  }
  rc = fscanf(fh, "%d", &exit_code);
  close(p2c[0]);

  if (rc != 1) {
    // exec succeeded!
    lcmaps_log(2, "%s: ClassAd update %s=%s successful\n", logstr, attr, val);
    return 0;
  } else {
    lcmaps_log(0, "%s: Update of %s returned error before exec: %d\n", logstr, attr, exit_code);
    return 1;
  }
}


/******************************************************************************
Function:   plugin_initialize
Description:
    Initialize plugin; a no-op, but required by LCMAPS
Parameters:
    argc, argv
    argv[0]: the name of the plugin
Returns:
    LCMAPS_MOD_SUCCESS : success
******************************************************************************/
int plugin_initialize(int argc, char **argv)
{
  return LCMAPS_MOD_SUCCESS;
}


/******************************************************************************
Function:   plugin_introspect
Description:
    return list of required arguments
Parameters:

Returns:
    LCMAPS_MOD_SUCCESS : success
******************************************************************************/
int plugin_introspect(int *argc, lcmaps_argument_t **argv)
{
  char *logstr = "\tlcmaps_plugins_condor_update-plugin_introspect()";
  static lcmaps_argument_t argList[] = {
    { "user_dn"        , "char *"                ,  1, NULL},
    {NULL        ,  NULL    , -1, NULL}
  };

  lcmaps_log_debug(2, "%s: introspecting\n", logstr);

  *argv = argList;
  *argc = lcmaps_cntArgs(argList);
  lcmaps_log_debug(2, "%s: address first argument: 0x%x\n", logstr, argList);

  lcmaps_log_debug(2, "%s: Introspect succeeded\n", logstr);

  return LCMAPS_MOD_SUCCESS;
}




/******************************************************************************
Function:   plugin_run
Description:
    Launch a process tracking daemon for LCMAPS.
    Basic boilerplate for a LCMAPS plugin.
Parameters:
    argc: number of arguments
    argv: list of arguments
Returns:
    LCMAPS_MOD_SUCCESS: authorization succeeded
    LCMAPS_MOD_FAIL   : authorization failed
******************************************************************************/
int plugin_run(int argc, lcmaps_argument_t *argv)
{
  int uid_count;
  uid_t uid, *uid_array;
  struct passwd *user_info;
  char **dn_array, *dn;
  char time_string[TIME_BUFFER_SIZE];
  time_t curtime;
  size_t len;

  // Update the user name.
  uid_count = 0;
  uid_array;
  lcmaps_log_debug(2, "%s: Acquiring the username from LCMAPS\n", logstr);
  uid_array = (uid_t *)getCredentialData(UID, &uid_count);
  if (uid_count != 1) {
    lcmaps_log(0, "%s: No UID set yet; must map to a UID before running the process tracking module.\n", logstr);
    goto condor_update_failure;
  }
  uid = uid_array[0];
  if ((user_info = getpwuid(uid)) == NULL) {
    lcmaps_log(0, "%s: Fatal error: unable to find corresponding username for UID %d.\n", logstr, uid);
    goto condor_update_failure;
  }
  char *username = user_info->pw_name;

  update_starter(CLASSAD_GLEXEC_USER, username);

  // Update the DN.
  lcmaps_log_debug(2, "%s: Acquiring information from LCMAPS framework\n", logstr);
  dn_array = (char **)lcmaps_getArgValue("user_dn", "char *",argc, argv);
  if ((dn_array == NULL) || ((dn = *dn_array) == NULL)) {
    lcmaps_log(0, "%s: value of user_dn is empty. No user DN found by the framework in the proxy chain.\n", logstr);
    goto condor_update_failure;
  } else {
    lcmaps_log_debug(5, "%s: user_dn = %s\n", logstr, dn);
  }

  update_starter(CLASSAD_GLEXEC_DN, dn);

  // Update the invocation time.
  lcmaps_log_debug(2, "%s: Logging time of invocation\n", logstr);
  curtime = time(NULL);
  if ((len = snprintf(time_string, TIME_BUFFER_SIZE, "%ld", curtime)) >= TIME_BUFFER_SIZE) {
    lcmaps_log(0, "%s: Unexpected failure in converting time to string.\n", logstr);
    goto condor_update_failure;
  }
  update_starter(CLASSAD_GLEXEC_TIME, time_string);

  return LCMAPS_MOD_SUCCESS;


condor_update_failure:
  lcmaps_log_time(0, "%s: monitor process launch failed\n", logstr);

  return LCMAPS_MOD_FAIL;
}

int plugin_verify(int argc, lcmaps_argument_t * argv)
{
    return plugin_run(argc, argv);
}

/******************************************************************************
Function:   plugin_terminate
Description:
    Terminate plugin.  Boilerplate - doesn't do anything
Parameters:

Returns:
    LCMAPS_MOD_SUCCESS : success
******************************************************************************/
int plugin_terminate()
{
  return LCMAPS_MOD_SUCCESS;
}
