#ifndef __ARCH_H__
#define __ARCH_H__

/**** Utility functions to detect the current environment ****/

/*
 * List of currently recognized distributions
 */
typedef enum {
	DISTRO_NONE = 0, /* Unrecognized */
	DISTRO_REDHAT,
	DISTRO_MANDRAKE,
	DISTRO_SUSE,
	DISTRO_DEBIAN,
	DISTRO_SLACKWARE,
	DISTRO_CALDERA,
	DISTRO_LINUXPPC,
	DISTRO_SOLARIS,
	NUM_DISTRIBUTIONS
} distribution;

/* Map between the distro code and its real name */
extern const char *distribution_name[NUM_DISTRIBUTIONS], *distribution_symbol[NUM_DISTRIBUTIONS];

/* Detect the distribution type and version */
extern distribution detect_distro(int *maj_ver, int *min_ver);

/* Function to detect the current architecture */
extern const char *detect_arch(void);

/* Returns the OS string */
extern const char *detect_os(void);

/* Function to detect the current version of libc */
extern const char *detect_libc(void);

/* Function that returns the current user's home directory */
extern const char *detect_home(void);

#endif /* __ARCH_H__ */
