/* $Id: arch.c,v 1.1 2000-10-17 03:33:06 megastep Exp $ */

#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>

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

const char *detect_os(void)
{
    static struct utsname buf;

    uname(&buf);
    return buf.sysname;
}
