#ifndef __ARCH_H__
#define __ARCH_H__

/**** Utility functions to detect the current environment ****/

/* Function to detect the current architecture */
extern const char *detect_arch(void);

/* Returns the OS string */
extern const char *detect_os(void);

/* Function to detect the current version of libc */
extern const char *detect_libc(void);

/* Function that returns the current user's home directory */
extern const char *detect_home(void);

#endif /* __ARCH_H__ */
