/* $Id: arch.c,v 1.3 2001-03-08 22:55:35 hercules Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <string.h>

#include "arch.h"


/* Function to detect the current architecture */
const char *detect_arch(void)
{
    const char *arch;

    /* See if there is an environment override */
    arch = getenv("SETUP_ARCH");
    if ( arch == NULL ) {
#ifdef __i386
        arch = "x86";
#elif defined(powerpc)
        arch = "ppc";
#elif defined(__alpha__)
        arch = "alpha";
#elif defined(__sparc__)
        arch = "sparc64";
#elif defined(__arm__)
        arch = "arm";
#else
        arch = "unknown";
#endif
    }
    return arch;
}

/* Function to detect the operating system */
const char *detect_os(void)
{
    static struct utsname buf;

    uname(&buf);
    return buf.sysname;
}

/* Function to detect the current version of libc */
const char *detect_libc(void)
{
#ifdef __FreeBSD__
	return "glibc-2.1";
#else
    static const char *libclist[] = {
        "/lib/libc.so.6",
        "/lib/libc.so.6.1",
        NULL
    };
    int i;
    const char *libc;
    const char *libcfile;

    /* See if there is an environment override */
    libc = getenv("SETUP_LIBC");
    if ( libc != NULL ) {
        return(libc);
    }

    /* Look for the highest version of libc */
    for ( i=0; libclist[i]; ++i ) {
        if ( access(libclist[i], F_OK) == 0 ) {
            break;
        }
    }
    libcfile = libclist[i];

    if ( libcfile ) {
      char buffer[1024];
      snprintf( buffer, sizeof(buffer), 
           "fgrep GLIBC_2.1 %s 2>&1 >/dev/null",
           libcfile );
      
      if ( system(buffer) == 0 )
		  return "glibc-2.1";
      else
		  return "glibc-2.0";
    }
    /* Default to version 5 */
    return "libc5";
#endif
}

/* Function that returns the current user's home directory */
const char *detect_home(void)
{
    static const char *home = NULL;

    /* First look up the user's home directory in the password file,
       based on effective user id.
     */
    if ( ! home ) {
        struct passwd *pwent;

        pwent = getpwuid(geteuid());
        if ( pwent ) {
            /* Small memory leak, don't worry about it */
            home = strdup(pwent->pw_dir);
        }
    }

    /* We didn't find the user in the password file?
       Something wierd is going on, but use the HOME environment anyway.
     */
    if ( ! home ) {
        home = getenv("HOME");
    }

    /* Uh oh, this system is probably hosed, write to / if we can.
     */
    if ( ! home ) {
        home = "";
    }

    return home;
}
