
#ifndef __CONDOR_DISCOVERY_H
#define __CONDOR_DISCOVERY_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

char * findCondorScratch(pid_t);
int getParentIDs(pid_t, uid_t*, gid_t*);

#ifdef __cplusplus
}
#endif

#endif

