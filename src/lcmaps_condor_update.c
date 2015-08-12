
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
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>

#include "lcmaps/lcmaps_modules.h"
#include "lcmaps/lcmaps_cred_data.h"
#include "lcmaps/lcmaps_arguments.h"

#include "condor_discovery.h"

#define CONDOR_CHIRP_PATH "/usr/libexec/condor/condor_chirp"
#define CONDOR_CHIRP_NAME "condor_chirp"
#define CONDOR_SCRATCH_DIR "_CONDOR_SCRATCH_DIR"

static const char * logstr = "lcmaps-condor-update";

// ClassAd attributes we'll use for the update
#define CLASSAD_GLEXEC_DN "glexec_x509userproxysubject"
#define CLASSAD_GLEXEC_USER "glexec_user"
#define CLASSAD_GLEXEC_TIME "glexec_time"

#define TIME_BUFFER_SIZE 12

static int update_starter_child(const char * attr, const char *val, int fd, const char * scratch_dir, pid_t ppid) {
  size_t len;
  int result = 1;
  char result_buf[TIME_BUFFER_SIZE];
  uid_t uid;
  gid_t gid;

  if (getParentIDs(ppid, &uid, &gid)) {
    lcmaps_log(0, "%s: Unable to determine target user UID/GID\n", logstr);
    return 1;
  } 
  if (setgid(gid) == -1) {
    lcmaps_log(0, "%s: Unable to switch to user's GID (%d): %d %s\n", logstr, gid, errno, strerror(errno));
    result = errno;
    goto condor_update_fail_child;
  }
  if (setuid(uid) == -1) {
    lcmaps_log(0, "%s: Unable to switch to user's UID (%d): %d %s\n", logstr, uid, errno, strerror(errno));
    result = errno;
    goto condor_update_fail_child;
  }

  int use_chirp_config = 0;
  char path[PATH_MAX];
  struct stat chirp_file;
  if (stat(scratch_dir, &chirp_file) == -1)
  {
    lcmaps_log(0, "%s: Scratch location %s not found (errno=%d, %s).\n", logstr, scratch_dir, errno, strerror(errno));
    goto condor_update_fail_child;
  }
  if (S_ISREG(chirp_file.st_mode))
  {
    if (snprintf(path, PATH_MAX, "%s", scratch_dir) >= PATH_MAX)
    {
      lcmaps_log(0, "%s: Chirp config filename overly long.\n", logstr);
      goto condor_update_fail_child;
    }
    use_chirp_config = 1;
  }
  else if (snprintf(path, PATH_MAX, "%s/chirp.config", scratch_dir) >= PATH_MAX)
  {
    lcmaps_log(0, "%s: Overly long scratch dir: %s\n", logstr, scratch_dir);
    goto condor_update_fail_child;
  }
  else if (stat(path, &chirp_file) == -1)
  {
    if (snprintf(path, PATH_MAX, "%s/.chirp.config", scratch_dir) >= PATH_MAX)
    {
      lcmaps_log(0, "%s: Overly long scratch dir: %s\n", logstr, scratch_dir);
      goto condor_update_fail_child;
    }
  }

  if (access(path, O_RDONLY) == -1) {
    lcmaps_log(0, "%s: Unable to access chirp config %s\n", logstr, path);
    goto condor_update_fail_child;
  }
  char environ_tmp[PATH_MAX];
  if (use_chirp_config)
  {
    if (snprintf(environ_tmp, PATH_MAX, "_CONDOR_CHIRP_CONFIG=%s", scratch_dir) >= PATH_MAX)
    {
      lcmaps_log(0, "%s: Overly long chirp config path: %s\n", logstr, scratch_dir);
      goto condor_update_fail_child;
    }
  }
  else if (snprintf(environ_tmp, PATH_MAX, "_CONDOR_SCRATCH_DIR=%s", scratch_dir) >= PATH_MAX) {
    lcmaps_log(0, "%s: Overly long scratch dir: %s\n", logstr, scratch_dir);
    goto condor_update_fail_child;
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

  // Nuke fd 1 and 2 to prevent condor_chirp from spilling out information to stdout/err
  // Writing to stdout/err for a successful execution causes condor glexec integration to choke.
  int fd_null;
  if ((fd_null = open("/dev/null", O_WRONLY)) == -1) {
    lcmaps_log(0, "%s: Opening of /dev/null failed: %d %s\n", logstr, errno, strerror(errno));
    result = errno;
    goto condor_update_fail_child;
  }
  if (dup2(fd_null, 1) == -1) {
    lcmaps_log(0, "%s: Duping of /dev/null to stdout failed: %d %s\n", logstr, errno, strerror(errno));
    result = errno;
    goto condor_update_fail_child;
  }
  if (dup2(fd_null, 2) == -1) {
    lcmaps_log(0, "%s: Duping of /dev/null to stderr failed: %d %s\n", logstr, errno, strerror(errno));
    result = errno;
    goto condor_update_fail_child;
  }

  // Cheap daemonize - causes condor_chirp to attach to init to avoid zombies
  int fork_pid = fork();
  if (fork_pid == -1) {
    lcmaps_log(0, "%s: Daemonization of condor_chirp failed: %d %s\n", logstr, errno, strerror(errno));
    result = errno;
    goto condor_update_fail_child;
  } else if (fork_pid) { // Parent
    _exit(0);
  }

  if ((result = execve(CONDOR_CHIRP_PATH, argv, environ)) == -1) {
    lcmaps_log(0, "%s: Exec of condor_chirp failed: %d %s\n", logstr, errno, strerror(errno));
  }
  result = errno;

condor_update_fail_child:
  len = snprintf(result_buf, TIME_BUFFER_SIZE, "%d", result);
  if (write(fd, result_buf, len) == -1) {
    lcmaps_log(0, "%s: Unable to return failed result to parent: %d %s\n", logstr, errno, strerror(errno));
  }
  _exit(result);
}

int get_user_ids(uid_t *uid, gid_t *gid, char ** username) {
  int count = 0;
  uid_t internal_uid;
  struct passwd *user_info;
  if (!uid)
    uid = &internal_uid;
  uid_t *uid_array;
  lcmaps_log_debug(2, "%s: Acquiring the UID from LCMAPS\n", logstr);
  uid_array = (uid_t *)getCredentialData(UID, &count);
  if (count != 1) {
    lcmaps_log(0, "%s: No UID set yet; must map to a UID before running the process tracking module.\n", logstr);
    return 1;
  }
  *uid = uid_array[0];
  if ((user_info = getpwuid(*uid)) == NULL) {
    lcmaps_log(0, "%s: Fatal error: unable to find corresponding username for UID %d.\n", logstr, *uid);
    return 1;
  }
  if (username)
    *username = user_info->pw_name;

  if (!gid)
    return 0;

  gid_t *gid_array = (gid_t *)getCredentialData(PRI_GID, &count);
  if (count <= 0) {
      *gid = user_info->pw_gid;
  } else {
      *gid = gid_array[0];
  }
  return 0;
}

int update_starter(const char * attr, const char * val) {
  int fork_pid;
  int fd_flags;
  int rc, exit_code;
  int status;
  int p2c[2];
  int result = 0;
  FILE * fh;

  pid_t pid = getpid();

  char * scratch_dir = findCondorScratch(pid);
  if (!scratch_dir) {
    lcmaps_log(0, "%s: Environment error - unable to determine the starter's scratch directory\n", logstr);
    return 1;
  }

  if (attr == NULL) {
    lcmaps_log(0, "%s: Internal error - passed a NULL attribute\n", attr);
    result = 1;
    goto finalize;
  }
  if (val == NULL) {
    lcmaps_log(0, "%s: Internal error - passed a NULL value\n", attr);
    result = 1;
    goto finalize;
  }

  if (pipe(p2c) < 0) {
    lcmaps_log(0, "%s: Failed to create an internal pipe: %d %s\n", logstr, errno, strerror(errno));
    result = errno;
    goto finalize;
  }
  if ((fd_flags = fcntl(p2c[1], F_GETFD, NULL)) == -1) {
    lcmaps_log(0, "%s: Failed to get fd flags: %d %s\n", logstr, errno, strerror(errno));
    result = errno;
    goto finalize;
  }
  if (fcntl(p2c[1], F_SETFD, fd_flags | FD_CLOEXEC) == -1) {
    lcmaps_log(0, "%s: Failed to set new fd flags: %d %s\n", logstr, errno, strerror(errno));
    result = errno;
    goto finalize;
  }

  fork_pid = fork();
  if (fork_pid == -1) {
    lcmaps_log(0, "%s: Failed to fork a new child process: %d %s\n", logstr, errno, strerror(errno));
    close(p2c[0]); close(p2c[1]);
    result = errno;
    goto finalize;
  } else if (fork_pid == 0) { // Child
    close(p2c[0]);
    update_starter_child(attr, val, p2c[1], scratch_dir, pid);
    // Does not return.  Just in case:
    _exit(1);
  }

  close(p2c[1]);
  if ((fh = fdopen(p2c[0], "r")) == NULL) {
    lcmaps_log(0, "%s: Failed to reopen file descriptor as file handle: %d %s", logstr, errno, strerror(errno));
    close(p2c[0]);
    result = errno;
    goto finalize;
  }
  rc = fscanf(fh, "%d", &exit_code);
  close(p2c[0]);

  if (rc != 1) {
    // exec succeeded!  Let's check the exit status
    // Note that we just check to see if condor_chirp daemonized, not whether
    // it succeeded.  The problem is that the starter will block on us, and
    // we block on condor_starter, and condor_starter blocks on the single-threaded starter.
    // See the issue?
    waitpid(fork_pid, &status, 0);
    if (WIFEXITED(status)) {
      if (!(exit_code = WEXITSTATUS(status))) {
        lcmaps_log(2, "%s: ClassAd update %s=%s successful\n", logstr, attr, val);
        result = 0;
      } else {
        lcmaps_log(0, "%s: ClassAd update %s=%s failed.\n", logstr, attr, val);
        result = 1;
      }
    } else {
      lcmaps_log(0, "%s: Unrecognized condor_chirp status: %d\n", logstr, status);
      result = 1;
    }
  } else {
    lcmaps_log(0, "%s: Update of %s returned error before exec: %d\n", logstr, attr, exit_code);
    result = 1;
  }

finalize:

  free(scratch_dir);
  return result;

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
  lcmaps_log_debug(2, "%s: address first argument: %p\n", logstr, argList);

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
  uid_t uid;
  char * username;
  char **dn_array, *dn;
  char time_string[TIME_BUFFER_SIZE];
  time_t curtime;
  size_t len;

  // Update the user name.
  get_user_ids(&uid, NULL, &username);
  size_t username_len = strlen(username);
  char * quoted_username = (char *)malloc(username_len + 2 + 1);
  if (quoted_username == NULL) {
    lcmaps_log(0, "%s: Malloc failed for quoted username.\n", logstr);
    goto condor_update_failure;
  }
  snprintf(quoted_username, username_len + 3, "\"%s\"", username);

  update_starter(CLASSAD_GLEXEC_USER, quoted_username);

  // Update the DN.
  lcmaps_log_debug(2, "%s: Acquiring information from LCMAPS framework\n", logstr);
  dn_array = (char **)lcmaps_getArgValue("user_dn", "char *",argc, argv);
  if ((dn_array == NULL) || ((dn = *dn_array) == NULL)) {
    lcmaps_log(0, "%s: value of user_dn is empty. No user DN found by the framework in the proxy chain.\n", logstr);
    goto condor_update_failure;
  } else {
    lcmaps_log_debug(5, "%s: user_dn = %s\n", logstr, dn);
  }
  size_t dn_len = strlen(dn);
  char * quoted_dn = (char *)malloc(dn_len + 2 + 1);
  if (quoted_dn == NULL) {
    lcmaps_log(0, "%s: Malloc failed for quoted DN.\n", logstr);
    goto condor_update_failure;
  }
  snprintf(quoted_dn, dn_len + 3, "\"%s\"", dn);

  update_starter(CLASSAD_GLEXEC_DN, quoted_dn);

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
