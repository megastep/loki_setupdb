#ifndef __SETUPDB_H__
#define __SETUPDB_H__

/* High-level library for product management */

#include <sys/types.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Types definition */

struct _loki_product_t;
typedef struct _loki_product_t product_t;

struct _loki_product_component_t;
typedef struct _loki_product_component_t product_component_t;

struct _loki_product_option_t;
typedef struct _loki_product_option_t product_option_t;

struct _loki_product_file_t;
typedef struct _loki_product_file_t product_file_t;

typedef struct {
    char name[64];
    char description[128];
    char root[PATH_MAX];
    char url[PATH_MAX];
    char registry_path[PATH_MAX];
} product_info_t;

typedef enum {
    LOKI_FILE_REGULAR,
    LOKI_FILE_DIRECTORY,
    LOKI_FILE_SYMLINK,
    LOKI_FILE_DEVICE,
    LOKI_FILE_SOCKET,
    LOKI_FILE_FIFO,
    LOKI_FILE_RPM,
    LOKI_FILE_SCRIPT,
    LOKI_FILE_NONE /* The file is not accessible or does not exist */
} file_type_t;

typedef enum {
    LOKI_SCRIPT_PREUNINSTALL = 0,
    LOKI_SCRIPT_POSTUNINSTALL
} script_type_t;

/* Enumerate all products, returns name or NULL if end of list */

const char *loki_getfirstproduct(void);

const char *loki_getnextproduct(void);

/* Open a product by name, or by the absolute path of its INI file */

product_t *loki_openproduct(const char *name);

/* Create a new product entry */

product_t *loki_create_product(const char *name, const char *root, const char *desc, const char *url);

/* Close a product entry and free all allocated memory.
   Also writes back to the database all changes that may have been made.
 */

int loki_closeproduct(product_t *product);

/* Clean up a product from the registry, i.e. removes all support files and directories.
   No need to close the procuct after that */
int loki_removeproduct(product_t *product);

/* Get a pointer to the product info */

product_info_t *loki_getinfo_product(product_t *product);

/* Enumerate the installed components */

product_component_t *loki_getfirst_component(product_t *product);
product_component_t *loki_getnext_component(product_component_t *comp);

/* Note: the following functions return allocated objects that have to 
   be freed by the caller */
product_component_t *loki_getdefault_component(product_t *product);
product_component_t *loki_find_component(product_t *product, const char *name);

const char *loki_getname_component(product_component_t *comp);
const char *loki_getversion_component(product_component_t *comp);
size_t loki_getsize_component(product_component_t *comp);
void loki_setversion_component(product_component_t *comp, const char *version);
int loki_isdefault_component(product_component_t *comp);

/* Change the default component */
void loki_setdefault_component(product_component_t *comp);

product_component_t *loki_create_component(product_t *product, const char *name, const char *version);
void loki_remove_component(product_component_t *comp);

/* Set a specific URL for updates to that component */
void loki_seturl_component(product_component_t *comp, const char *url);

/* Get the URL for updates for a component; defaults to the product's URL if not defined */
const char *loki_geturl_component(product_component_t *comp);

/* Enumerate options from components */

product_option_t *loki_getfirst_option(product_component_t *comp);
product_option_t *loki_getnext_option(product_option_t *option);

/* The returned enumerator has to be explicitly freed */
product_option_t *loki_find_option(product_component_t *comp, const char *name);

const char *loki_getname_option(product_option_t *opt);
size_t loki_getsize_option(product_option_t *opt);

product_option_t *loki_create_option(product_component_t *comp, const char *name);

void loki_remove_option(product_option_t *opt);

/* Enumerate files from components */

product_file_t *loki_getfirst_file(product_option_t *opt);
product_file_t *loki_getnext_file(product_file_t *file);

/* Get informations from a file */
file_type_t loki_gettype_file(product_file_t *file);
/* This returns the expanded full path of the file if applicable */
const char *loki_getpath_file(product_file_t *file);
unsigned char *loki_getmd5_file(product_file_t *file);
unsigned int loki_getmode_file(product_file_t *file);

/* Set the UNIX mode for the file */
void loki_setmode_file(product_file_t *file, unsigned int mode);

/* Get / set the 'patched' attribute of a flag, i.e. it should not be removed unless
   the whole application is removed */
int loki_getpatched_file(product_file_t *file);
void loki_setpatched_file(product_file_t *file, int flag);

product_option_t *loki_getoption_file(product_file_t *file);
product_component_t *loki_getcomponent_file(product_file_t *file);
product_t *loki_getproduct_file(product_file_t *file);

/* Callback function type for file enumerations */
typedef void (*product_file_cb)(const char *path, file_type_t type,
                                product_component_t *comp,
                                product_option_t *option);

/* Enumerate all files of a option through a callback function */
int loki_enumerate_files(product_option_t *opt, product_file_cb cb);

/* Find to which product / option a file belongs; 'product' is optional */

product_file_t *loki_findpath(const char *path, product_t *product);

/* Register a new file, returns 0 if OK. The option is created if it didn't exist before.
   The md5 argument can be NULL, and the checksum will be computed if necesary.
 */
product_file_t *loki_register_file(product_option_t *option, const char *path, const char *md5);

/* Remove a file from the registry. Actually removing the file is up to the caller. */
int loki_unregister_path(product_option_t *option, const char *path);
/* Variant using an iterator */
int loki_unregister_file(product_file_t *file);

/* Update the MD5 sum of a specific file */
int loki_updatemd5_file(product_t *product, const char *path);

/* Register a new RPM as having been installed by this product */
int loki_register_rpm(product_option_t *option, const char *name, const char *version, int revision,
                     int autoremove);

/* Remove a RPM from the registry */
int loki_unregister_rpm(product_t *product, const char *name);

  /**** Script registration: option variant *****/
/* Register a new script for the product. 'script' holds the whole script in one string */
int loki_registerscript(product_option_t *opt, script_type_t type, const char *name, 
                        const char *script);
    
/* Register a new script for the product. 'path' is the name of a script file */
int loki_registerscript_fromfile(product_option_t *opt, script_type_t type,
                                 const char *name, const char *path);
    
  /**** Script registration: component variant *****/
/* Register a new script for the product. 'script' holds the whole script in one string */
int loki_registerscript_component(product_component_t *comp, script_type_t type, const char *name, 
                                  const char *script);

/* Register a new script for the product. 'path' is the name of a script file */
int loki_registerscript_fromfile_component(product_component_t *comp, script_type_t type,
                                           const char *name, const char *path);

/* Unregister a registered script, and remove the file */
int loki_unregister_script(product_component_t *component, const char *name);

/* Run all scripts of a given type, returns the number of scripts successfully run */
int loki_runscripts(product_component_t *component, script_type_t type);

/* Perform automatic update of the uninstaller binaries and script.
   'src' is the path where updated binaries can be copied from.
   'locale_path' is the path where locale files can be found.
 */
int loki_upgrade_uninstall(product_t *prod, const char *src, const char *locale_path);

/* Extract base and extension from a version string */
extern void loki_split_version(const char *version,
                               char *base, int maxbase,
                               char *ext, int maxext);

/* Compare two version strings, return true if version1 newer than version2 */
extern int loki_newer_version(const char *version1, const char *version2);

#ifdef __cplusplus
};
#endif

#endif
