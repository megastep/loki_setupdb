#ifndef __ARCH_H__
#define __ARCH_H__

/**** Utility functions to detect the current environment ****/

/* Function to detect the current architecture */
const char *detect_arch(void);

/* Returns the OS string */
const char *detect_os(void);

/* Function to detect the current version of libc */
const char *detect_libc(void);

#endif /* __ARCH_H__ */
