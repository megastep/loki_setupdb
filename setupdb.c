/* Implementation of the Loki Product DB API */
/* $Id: setupdb.c,v 1.15 2000-10-17 08:55:43 megastep Exp $ */

#include <glob.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <string.h>
#include <parser.h>
#include <tree.h>

#include "setupdb.h"
#include "arch.h"
#include "md5.h"

struct _loki_product_t
{
    xmlDocPtr doc;
    product_info_t info;
    int changed;
    product_component_t *components, *default_comp;
};

struct _loki_product_component_t
{
    xmlNodePtr node;
    product_t *product;
    char *name;
    char *version;
    char *url;
    int is_default;
    product_option_t *options;
    product_file_t *scripts;
    product_component_t *next;
};

struct _loki_product_option_t
{
    xmlNodePtr node;
    product_component_t *component;
    char *name;
    product_option_t *next;    
    product_file_t *files;
};

struct _loki_product_file_t {
    xmlNodePtr node;
    product_option_t *option;
    char *path;
    file_type_t type;
    unsigned int mode;
    int patched;
    union {        
        unsigned char md5sum[16];
        script_type_t scr_type;
    } data;
    product_file_t *next;
};


/* Enumerate all products, returns name or NULL if end of list */

static glob_t globbed;
static int glob_index;

static const char *script_types[] = { "pre-uninstall", "post-uninstall" };

static const char *get_productname(char *inipath)
{
    char *ret = strrchr(inipath, '/') + 1, *ptr;
    
    ptr = strstr(ret, ".xml");
    *ptr = '\0';
    return ret;
}

static const char *expand_path(product_t *prod, const char *path, char *buf, size_t len)
{
    if ( *path == '/' ) {
        strncpy(buf, path, len);
    } else {
        snprintf(buf, len, "%s/%s", prod->info.root, path);
    }
    return buf;
}

static const char *get_xml_string(product_t *product, xmlNodePtr node)
{
    const char *text = xmlNodeListGetString(product->doc, node->childs, 1);
    return text;
}

static void insert_end_file(product_file_t *file, product_file_t **opt)
{
    file->next = NULL;
    if ( *opt ) {
        product_file_t *it;
        for( it = *opt; it; it = it->next ) {
            if ( it->next == NULL ) {
                it->next = file;
                return;
            }
        }
    } else {
        *opt = file;
    }
}

const char *loki_getfirstproduct(void)
{
    char buf[PATH_MAX];

    snprintf(buf, sizeof(buf), "%s/.loki/installed/*.xml", getenv("HOME"));
    if ( glob(buf, GLOB_ERR, NULL, &globbed) != 0 ) {
        return NULL;
    } else {
        if ( globbed.gl_pathc == 0 ) {
            return NULL;
        } else {
            glob_index = 1;
            return get_productname(globbed.gl_pathv[0]);
        }
    }
}

const char *loki_getnextproduct(void)
{
    if ( glob_index >= globbed.gl_pathc ) {
        return NULL;
    } else {
        return get_productname(globbed.gl_pathv[glob_index++]);
    }
}

/* Open a product by name*/

product_t *loki_openproduct(const char *name)
{
    char buf[PATH_MAX];
    int major, minor;
    const char *str;
    xmlDocPtr doc;
    xmlNodePtr node;
    product_t *prod;

    if ( *name == '/' ) { /* Absolute path to a manifest.ini file */
        doc = xmlParseFile(name);
    } else {
        snprintf(buf, sizeof(buf), "%s/.loki/installed/%s.xml", getenv("HOME"), name);
        doc = xmlParseFile(buf);
    }
    if ( !doc )
        return NULL;
    prod = (product_t *)malloc(sizeof(product_t));
    prod->doc = doc;
    prod->changed = 0;

    str = xmlGetProp(doc->root, "name");
    strncpy(prod->info.name, str, sizeof(prod->info.name));
    str = xmlGetProp(doc->root, "desc");
    if ( str ) {
        strncpy(prod->info.description, str, sizeof(prod->info.description));
    } else {
        *prod->info.description = '\0';
    }
    str = xmlGetProp(doc->root, "root");
    strncpy(prod->info.root, str, sizeof(prod->info.root));
    str = xmlGetProp(doc->root, "update_url");
    strncpy(prod->info.url, str, sizeof(prod->info.url));
    snprintf(prod->info.registry_path, sizeof(prod->info.registry_path), "%s/.manifest/%s.xml",
             prod->info.root, prod->info.name);

    /* Check for the xmlversion attribute for backwards compatibility */
    str = xmlGetProp(doc->root, "xmlversion");
    sscanf(str, "%d.%d", &major, &minor);
    if ( (major > SETUPDB_VERSION_MAJOR) || 
         ((major == SETUPDB_VERSION_MAJOR) && (minor > SETUPDB_VERSION_MINOR)) ) {
        fprintf(stderr, "Warning: This XML file was generated with a later version of setupdb (%d.%d).\n"
                "Problems may occur.\n", major, minor);
    }

    prod->components = prod->default_comp = NULL;

    /* Parse the XML tags and build a tree. Water every day so that it grows steadily. */
    
    for ( node = doc->root->childs; node; node = node->next ) {
        if ( !strcmp(node->name, "component") ) {
            xmlNodePtr optnode;
            product_component_t *comp = (product_component_t *) malloc(sizeof(product_component_t));

            comp->node = node;
            comp->product = prod;
            comp->name = strdup(xmlGetProp(node, "name"));
            comp->version = strdup(xmlGetProp(node, "version"));
            str = xmlGetProp(node, "update_url");
            comp->url = str ? strdup(str) : NULL;
            comp->is_default = (xmlGetProp(node, "default") != NULL);
            if ( comp->is_default ) {
                prod->default_comp = comp;
            }
            comp->options = NULL;
            comp->scripts = NULL;
            comp->next = prod->components;
            prod->components = comp;

            for ( optnode = node->childs; optnode; optnode = optnode->next ) {
                if ( !strcmp(optnode->name, "option") ) {
                    xmlNodePtr filenode;
                    product_option_t *opt = (product_option_t *)malloc(sizeof(product_option_t));

                    opt->node = optnode;
                    opt->component = comp;
                    opt->name = strdup(xmlGetProp(node, "name"));
                    opt->files = NULL;
                    opt->next = comp->options;
                    comp->options = opt;

                    for( filenode = optnode->childs; filenode; filenode = filenode->next ) {
                        file_type_t t;
                        product_file_t *file = (product_file_t *) malloc(sizeof(product_file_t));

                        memset(file->data.md5sum, 0, 16);
                        if ( !strcmp(filenode->name, "file") ) {
                            const char *md5 = xmlGetProp(filenode, "md5");
                            t = LOKI_FILE_REGULAR;
                            if ( md5 ) 
                                memcpy(file->data.md5sum, get_md5_bin(md5), 16);
                        } else if ( !strcmp(filenode->name, "directory") ) {
                            t = LOKI_FILE_DIRECTORY;
                        } else if ( !strcmp(filenode->name, "symlink") ) {
                            t = LOKI_FILE_SYMLINK;
                        } else if ( !strcmp(filenode->name, "fifo") ) {
                            t = LOKI_FILE_FIFO;
                        } else if ( !strcmp(filenode->name, "device") ) {
                            t = LOKI_FILE_DEVICE;
                        } else if ( !strcmp(filenode->name, "rpm") ) {
                            t = LOKI_FILE_RPM;
                        } else if ( !strcmp(filenode->name, "script") ) {
                            t = LOKI_FILE_SCRIPT;
                            str = xmlGetProp(filenode, "type");
                            if ( str ) {
                                if ( !strcmp(str, script_types[LOKI_SCRIPT_PREUNINSTALL]) ) {
                                    file->data.scr_type = LOKI_SCRIPT_PREUNINSTALL;
                                } else if ( !strcmp(str, script_types[LOKI_SCRIPT_POSTUNINSTALL]) ) {
                                    file->data.scr_type = LOKI_SCRIPT_POSTUNINSTALL;
                                }
                            }
                        } else {
                            t = LOKI_FILE_NONE;
                        }
                        file->node = filenode;
                        file->option = opt;
                        file->type = t;
                        str = xmlGetProp(filenode, "patched");
                        if ( str && *str=='y' ) {
                            file->patched = 1;
                        } else {
                            file->patched = 0;
                        }
                        str = xmlGetProp(filenode, "mode");
                        if ( str ) {
                            sscanf(str,"%o", &file->mode);
                        } else {
                            file->mode = 0644;
                        }
                        str = get_xml_string(prod, filenode);
                        file->path = strdup(str); /* The expansion is done in loki_getname_file() */

                        insert_end_file(file, &opt->files);
                    }
                } else if ( !strcmp(optnode->name, "script") ) {
                    product_file_t *file = (product_file_t *) malloc(sizeof(product_file_t));
                    file->node = optnode;
                    file->type = LOKI_FILE_SCRIPT;

                    str = xmlGetProp(optnode, "type");
                    if ( str ) {
                        if ( !strcmp(str, script_types[LOKI_SCRIPT_PREUNINSTALL]) ) {
                            file->data.scr_type = LOKI_SCRIPT_PREUNINSTALL;
                        } else if ( !strcmp(str, script_types[LOKI_SCRIPT_POSTUNINSTALL]) ) {
                            file->data.scr_type = LOKI_SCRIPT_POSTUNINSTALL;
                        }
                    }
                    file->path = strdup(get_xml_string(prod, optnode));
                    file->next = comp->scripts;                    
                    comp->scripts = file;
                }
            }
        }
    }

    return prod;
}

/* Create a new product entry */

product_t *loki_create_product(const char *name, const char *root, const char *desc, const char *url)
{
    char homefile[PATH_MAX], manifest[PATH_MAX];
    xmlDocPtr doc;
    product_t *prod;

	/* Create hierarchy if it doesn't exist already */
	snprintf(homefile, sizeof(homefile), "%s/.loki", getenv("HOME"));
	mkdir(homefile, 0700);

	strncat(homefile, "/installed", sizeof(homefile));
	mkdir(homefile, 0700);

    snprintf(manifest, sizeof(manifest), "%s/.manifest", root);
    mkdir(manifest, 0755);

    /* Create a new XML tree from scratch; we'll have to dump it to disk later */
    doc = xmlNewDoc("1.0");

    if ( !doc ) {
        return NULL;
    }

	strncat(homefile, "/", sizeof(homefile));
	strncat(homefile, name, sizeof(homefile));
	strncat(homefile, ".xml", sizeof(homefile));

	strncat(manifest, "/", sizeof(manifest));
	strncat(manifest, name, sizeof(manifest));
	strncat(manifest, ".xml", sizeof(manifest));


    /* Symlink the file in the 'installed' per-user directory */
    if ( symlink(manifest, homefile) < 0 )
        perror("symlink");

	/* Create the scripts subdirectory */
	snprintf(manifest, sizeof(manifest), "%s/.manifest/scripts", root);
	mkdir(manifest, 0755);

    prod = (product_t *)malloc(sizeof(product_t));
    prod->doc = doc;
    prod->changed = 1;
    prod->components = prod->default_comp = NULL;

    doc->root = xmlNewDocNode(doc, NULL, "product", NULL);

    strncpy(prod->info.name, name, sizeof(prod->info.name));
    xmlSetProp(doc->root, "name", name);
    if ( desc ) {
        strncpy(prod->info.description, desc, sizeof(prod->info.description));
        xmlSetProp(doc->root, "desc", desc);
    } else {
        *prod->info.description = '\0';
    }
    snprintf(homefile, sizeof(homefile), "%d.%d", SETUPDB_VERSION_MAJOR, SETUPDB_VERSION_MINOR);
    xmlSetProp(doc->root, "xmlversion", homefile);
    strncpy(prod->info.root, root, sizeof(prod->info.root));
    xmlSetProp(doc->root, "root", root);
    strncpy(prod->info.url, url, sizeof(prod->info.url));
    xmlSetProp(doc->root, "update_url", url);
    snprintf(prod->info.registry_path, sizeof(prod->info.registry_path), "%s/.manifest/%s.xml",
             root, name);

    return prod;
}

/* Close a product entry and free all allocated memory.
   Also writes back to the database all changes that may have been made.
 */

int loki_closeproduct(product_t *product)
{
    product_component_t *comp, *next;

    if ( product->changed ) {
        /* Write XML file to disk if it has changed */
        xmlSaveFile(product->info.registry_path, product->doc);
    }

    /* Go through all the allocated structs */
    comp = product->components;
    while ( comp ) {
        product_option_t *opt, *nextopt;
        product_file_t   *scr, *nextscr;

        next = comp->next;
        
        opt = comp->options;
        while ( opt ) {
            product_file_t *file, *nextfile;
            nextopt = opt->next;

            file = opt->files;
            while ( file ) {
                nextfile = file->next;
                free(file->path);
                free(file);
                file = nextfile;
            }

            free(opt->name);
            free(opt);
            opt = nextopt;
        }

        scr = comp->scripts;
        while ( scr ){
            nextscr = scr->next;

            free(scr->path);
            free(scr);
            scr = nextscr;
        }
        
        free(comp->name);
        free(comp->version);
        free(comp->url);
        free(comp);
        comp = next;
    }

    free(product);
    return 0;
}

/* Clean up a product from the registry, i.e. removes all support files and directories */
int loki_removeproduct(product_t *product)
{
    char buf[PATH_MAX];

    /* Remove the XML file */
    unlink(product->info.registry_path);

    /* TODO: Remove the scripts for each component */

    /* Remove the directories */
    snprintf(buf, sizeof(buf), "%s/.manifest/scripts", product->info.root);
    if(rmdir(buf)<0)
        perror("Could not remove scripts directory");

    snprintf(buf, sizeof(buf), "%s/.manifest", product->info.root);
    if(rmdir(buf)<0)
        perror("Could not remove .manifest directory");


    if(rmdir(product->info.root) < 0)
        perror("Could not remove install directory");

    /* Remove the symlink */
    snprintf(buf, sizeof(buf), "%s/.loki/installed/%s.xml", getenv("HOME"), product->info.name);
    unlink(buf);
    
    loki_closeproduct(product);

    return 0;
}

/* Get a pointer to the product info */

product_info_t *loki_getinfo_product(product_t *product)
{
    return &product->info;
}

/* Enumerate the installed options */

product_component_t *loki_getfirst_component(product_t *product)
{
    return product->components;
}

product_component_t *loki_getnext_component(product_component_t *component)
{
    return component ? component->next : NULL;
}

product_component_t *loki_getdefault_component(product_t *product)
{
    return product->default_comp;
}

product_component_t *loki_find_component(product_t *product, const char *name)
{
    product_component_t *ret = product->components;
    while ( ret ) {
        if ( !strcmp(ret->name, name) ) {
            return ret;
        }
        ret = ret->next;
    }
    return NULL;
}

const char *loki_getname_component(product_component_t *component)
{
    return component->name;
}

const char *loki_getversion_component(product_component_t *component)
{
    return component->version;
}

int loki_isdefault_component(product_component_t *comp)
{
    return comp->is_default;
}

product_component_t *loki_create_component(product_t *product, const char *name, const char *version)
{
    xmlNodePtr node = xmlNewChild(product->doc->root, NULL, "component", NULL);
    if ( node ) {
        product_component_t *ret = (product_component_t *)malloc(sizeof(product_component_t));
        ret->node = node;
        ret->product = product;
        ret->name = strdup(name);
        ret->version = strdup(version);
        ret->url = NULL;
        ret->is_default = (product->default_comp == NULL);
        product->changed = 1;
        xmlSetProp(node, "name", name);
        xmlSetProp(node, "version", version);
        if(ret->is_default) {
            xmlSetProp(node, "default", "yes");
            product->default_comp = ret;
        }
        ret->scripts = NULL;
        ret->options = NULL;
        ret->next = product->components;
        product->components = ret;
        return ret;
    }
    return NULL;
}


/* Set a specific URL for updates to that component */
void loki_seturl_component(product_component_t *comp, const char *url)
{
    xmlSetProp(comp->node, "update_url", url);
    free(comp->url);
    if ( url ) {
        comp->url = strdup(url);
    } else {
        comp->url = NULL;
    }
}

void loki_setversion_component(product_component_t *comp, const char *version)
{
    xmlSetProp(comp->node, "version", version);
    free(comp->version);
    comp->version = strdup(version);
    comp->product->changed = 1;
}

/* Get the URL for updates for a component; defaults to the product's URL if not defined */
const char *loki_geturl_component(product_component_t *comp)
{
    return comp->url ? comp->url : comp->product->info.url;
}

/* Enumerate options from components */

product_option_t *loki_getfirst_option(product_component_t *component)
{
    return component->options;
}

product_option_t *loki_getnext_option(product_option_t *opt)
{
    return opt ? opt->next : NULL;
}

product_option_t *loki_find_option(product_component_t *comp, const char *name)
{
    product_option_t *ret = comp->options;
    while ( ret ) {
        if ( !strcmp(ret->name, name) ) {
            return ret;
        }
        ret = ret->next;
    }
    return NULL;
}

const char *loki_getname_option(product_option_t *opt)
{
    return opt->name;
}

product_option_t *loki_create_option(product_component_t *component, const char *name)
{
    xmlNodePtr node = xmlNewChild(component->node, NULL, "option", NULL);
    if ( node ) {
        product_option_t *ret = (product_option_t *)malloc(sizeof(product_option_t));
        ret->node = node;
        ret->component = component;
        ret->name = strdup(name);
        ret->next = component->options;
        ret->files = NULL;
        component->options = ret;
        component->product->changed = 1;
        xmlSetProp(node, "name", name);
        return ret;
    }
    return NULL;
}

/* Enumerate files from options */

product_file_t *loki_getfirst_file(product_option_t *opt)
{
    return opt->files;
}

product_file_t *loki_getnext_file(product_file_t *file)
{
    return file ? file->next : NULL;
}

static product_file_t *find_file_by_name(product_option_t *opt, const char *path)
{
    product_file_t *file = opt->files;
    while ( file ) {
        if ( !strcmp(path, file->path) ) {
            return file;
        }
        file = file->next;
    }
    return NULL;
}

/* Get informations from a file */
file_type_t loki_gettype_file(product_file_t *file)
{
    return file->type;
}

/* This returns the expanded full path of the file if applicable */
const char *loki_getpath_file(product_file_t *file)
{
    if ( file->type == LOKI_FILE_RPM || file->type == LOKI_FILE_SCRIPT ) {
        return file->path;
    } else {
        static char buf[PATH_MAX]; /* FIXME: Evil static buffer */
        expand_path(file->option->component->product, file->path, buf, sizeof(buf));
        return buf;
    }
}

unsigned int loki_getmode_file(product_file_t *file)
{
    return file->mode;
}

/* Set the UNIX mode for the file */
void loki_setmode_file(product_file_t *file, unsigned int mode)
{
    char buf[20];
    file->mode = mode;
    snprintf(buf, sizeof(buf), "%04o", mode);
    xmlSetProp(file->node, "mode", buf);
    file->option->component->product->changed = 1;
}

/* Get / set the 'patched' attribute of a flag, i.e. it should not be removed unless
   the whole application is removed */
int loki_getpatched_file(product_file_t *file)
{
    return file->patched;
}

void loki_setpatched_file(product_file_t *file, int flag)
{
    file->patched = flag;
    xmlSetProp(file->node, "patched", flag ? "yes" : "no");
    file->option->component->product->changed = 1;
}

product_option_t *loki_getoption_file(product_file_t *file)
{
    return file->option;
}

product_component_t *loki_getcomponent_file(product_file_t *file)
{
    return file->option->component;
}

product_t *loki_getproduct_file(product_file_t *file)
{
    return file->option->component->product;
}

unsigned char *loki_getmd5_file(product_file_t *file)
{
    return file->type==LOKI_FILE_REGULAR ? file->data.md5sum : NULL;
}

/* Enumerate all files of an option through a callback function */
int loki_enumerate_files(product_option_t *opt, product_file_cb cb)
{
    char buf[PATH_MAX];
    int count = 0;
    product_file_t *file = opt->files;

    while ( file ) {
        if ( file->type == LOKI_FILE_RPM || file->type == LOKI_FILE_SCRIPT ) {
            strncpy(buf, file->path, sizeof(buf));
        } else {
            expand_path(opt->component->product, file->path, buf, sizeof(buf));
        }
        cb(buf, file->type, opt->component, opt);
        count ++;
        file = file->next;
    }
    return count;
}

product_file_t *loki_findpath(const char *path, product_t *product)
{
    if ( product ) {
        product_component_t *comp;
        product_option_t *opt;
        product_file_t *file;

        for( comp = product->components; comp; comp = comp->next ) {
            for( opt = comp->options; opt; opt = opt->next ) {
                for( file = opt->files; file; file = file->next ) {
                    if ( file->type!=LOKI_FILE_SCRIPT && file->type!=LOKI_FILE_RPM ) {
                        if ( ! strcmp(path, file->path) ) {
                            /* We found our match */
                            return file;
                        }
                    }
                }
            }
        }
    } else {
        /* TODO: Try for each available product */
    }
    return NULL;
}

static product_file_t *registerfile_new(product_option_t *option, const char *path, const char *md5)
{
    struct stat st;
    char dev[10];
    char full[PATH_MAX];
    product_file_t *file;

    expand_path(option->component->product, path, full, sizeof(full));
    if ( lstat(full, &st) < 0 ) {
        return NULL;
    }
    file = (product_file_t *)malloc(sizeof(product_file_t));
    file->path = strdup(path);
    memset(file->data.md5sum, 0, 16);
    if ( S_ISREG(st.st_mode) ) {
        file->type = LOKI_FILE_REGULAR;
        if ( md5 ) {
            file->node = xmlNewChild(option->node, NULL, "file", path);
            xmlSetProp(file->node, "md5", md5);
            memcpy(file->data.md5sum, get_md5_bin(md5), 16);
        } else {
            char md5sum[33];
            if ( *path == '/' ) {
                md5_compute(path, md5sum, 1);
            } else {
                char fpath[PATH_MAX];
                snprintf(fpath, sizeof(fpath), "%s/%s", option->component->product->info.root, path);
                md5_compute(fpath, md5sum, 1);
            }
            file->node = xmlNewChild(option->node, NULL, "file", path);
            xmlSetProp(file->node, "md5", md5sum);
            memcpy(file->data.md5sum, get_md5_bin(md5sum), 16);
        }
    } else if ( S_ISDIR(st.st_mode) ) {
        file->type = LOKI_FILE_DIRECTORY;
        file->node = xmlNewChild(option->node, NULL, "directory", path);
    } else if ( S_ISLNK(st.st_mode) ) {
        char buf[PATH_MAX];
        int count;

        file->type = LOKI_FILE_SYMLINK;
        file->node = xmlNewChild(option->node, NULL, "symlink", path);
        count = readlink(path, buf, sizeof(buf));
        if ( count < 0 ) {
            perror("readlink");
        } else {
            buf[count] = '\0';
            xmlSetProp(file->node, "dest", buf);
        }
    } else if ( S_ISFIFO(st.st_mode) ) {
        file->type = LOKI_FILE_FIFO;
        file->node = xmlNewChild(option->node, NULL, "fifo", path);
    } else if ( S_ISBLK(st.st_mode) ) {
        file->type = LOKI_FILE_DEVICE;
        file->node = xmlNewChild(option->node, NULL, "device", path);
        xmlSetProp(file->node, "type", "block");
        /* Get the major/minor device number info */
        snprintf(dev,sizeof(dev),"%d", major(st.st_rdev));
        xmlSetProp(file->node, "major", dev);
        snprintf(dev,sizeof(dev),"%d", minor(st.st_rdev));
        xmlSetProp(file->node, "minor", dev);
    } else if ( S_ISCHR(st.st_mode) ) {
        file->type = LOKI_FILE_DEVICE;
        file->node = xmlNewChild(option->node, NULL, "device", path);
        xmlSetProp(file->node, "type", "char");
        /* Get the major/minor device number info */
        snprintf(dev,sizeof(dev),"%d", major(st.st_rdev));
        xmlSetProp(file->node, "major", dev);
        snprintf(dev,sizeof(dev),"%d", minor(st.st_rdev));
        xmlSetProp(file->node, "minor", dev);
    } else {
        file->node = NULL;
        file->type = LOKI_FILE_NONE;
        /* TODO: Warning? */
        return NULL;
    }
    file->patched = 0;
    file->mode = 0644;
    file->option = option;
    insert_end_file(file, &option->files);

    option->component->product->changed = 1;
    return file;
}

static product_file_t *registerfile_update(product_option_t *option, product_file_t *file,
                                           const char *md5)
{
    char buf[PATH_MAX];
    int count;
    unsigned char *md5bin;

    switch(file->type) {
    case LOKI_FILE_REGULAR:
        /* Compare MD5 checksums; if different then the 'patched' attribute is set automatically */
        if ( md5 ) {
            md5bin = get_md5_bin(md5);
            xmlSetProp(file->node, "md5", md5);
            if ( memcmp(file->data.md5sum, md5bin, 16) ) {
                loki_setpatched_file(file, 1);
            }
            memcpy(file->data.md5sum, md5bin, 16);
        } else {
            char md5sum[33];
            if ( *file->path == '/' ) {
                md5_compute(file->path, md5sum, 1);
            } else {
                snprintf(buf, sizeof(buf), "%s/%s", option->component->product->info.root, file->path);
                md5_compute(buf, md5sum, 1);
            }
            xmlSetProp(file->node, "md5", md5sum);
            md5bin = get_md5_bin(md5sum);
            if ( memcmp(file->data.md5sum, md5bin, 16) ) {
                loki_setpatched_file(file, 1);
            }
            memcpy(file->data.md5sum, md5bin, 16);
        }
        option->component->product->changed = 1;
        break;
    case LOKI_FILE_SYMLINK:
        count = readlink(file->path, buf, sizeof(buf));
        if ( count < 0 ) {
            perror("readlink");
        } else {
            buf[count] = '\0';
            xmlSetProp(file->node, "dest", buf);
        }   
        option->component->product->changed = 1;
        break;
    default:        
        /* TODO: Do we need to update any other element type ? */
    }
    return file;
}

/* Register a file, returns 0 if OK */
product_file_t *loki_register_file(product_option_t *option, const char *path, const char *md5)
{
    product_file_t *file = find_file_by_name(option, path);

    /* Check if this file is already registered for this option */
    if ( file ) {
        return registerfile_update(option, file, md5);
    } else {
        return registerfile_new(option, path, md5);
    }
}

static void unregister_file(product_file_t *file, product_file_t **opt)
{
    xmlUnlinkNode(file->node);
    xmlFreeNode(file->node);
    /* Remove the file from the list */
    if ( *opt == file ) {
        *opt = file->next;
    } else {
        product_file_t *f;
        for(f = *opt; f; f = f->next) {
            if (f->next == file ) {
                f->next = file->next;
                break;
            }
        }
    }
    free(file->path);
    free(file);
}

/* Remove a file from the registry. */
int loki_unregister_path(product_option_t *option, const char *path)
{
    product_file_t *file = find_file_by_name(option, path);
    if ( file ) {
        unregister_file(file, &option->files);
        option->component->product->changed = 1;
        return 0;
    }
    return -1;
}

int loki_unregister_file(product_file_t *file)
{
    product_option_t *option = file->option;
    if ( option ) { /* Does not work for scripts anyway */
        unregister_file(file, &option->files);
        option->component->product->changed = 1;
        return 0;
    }
    return -1;
}

/* Register a new RPM as having been installed by this product */
int loki_register_rpm(product_option_t *option, const char *name, const char *version, int revision,
                     int autoremove)
{
    product_file_t *rpm;
    char rev[10];

    rpm = (product_file_t *) malloc(sizeof(product_file_t));
    rpm->node = xmlNewChild(option->node, NULL, "rpm", name);    
    xmlSetProp(rpm->node, "version", version);
    snprintf(rev, sizeof(rev), "%d", revision);
    xmlSetProp(rpm->node, "revision", rev);
    xmlSetProp(rpm->node, "autoremove", autoremove ? "yes" : "no");

    rpm->option = option;
    rpm->path = strdup(name);
    rpm->type = LOKI_FILE_RPM;

    rpm->next = option->files;
    option->files = rpm;
    option->component->product->changed = 1;

    return 0;
}

int loki_unregister_rpm(product_t *product, const char *name)
{
    /* TODO: Search for a match */

    product->changed = 1;

    return -1;
}

static product_file_t *registerscript(xmlNodePtr parent, script_type_t type, const char *name,
                          const char *script, product_t *product)
{
    char buf[PATH_MAX];
    FILE *fd;
    
    snprintf(buf, sizeof(buf), "%s/.manifest/scripts/%s.sh", product->info.root, name);
    fd = fopen(buf, "w");
    if (fd) {
        product_file_t *scr;
        scr = (product_file_t *) malloc(sizeof(product_file_t));

        fprintf(fd, "#! /bin/sh\n");
        fprintf(fd, "%s", script);
        fchmod(fileno(fd), 0755);
        fclose(fd);

        scr->node = xmlNewChild(parent, NULL, "script", name);
        xmlSetProp(scr->node, "type", script_types[type]);
        product->changed = 1;

        scr->path = strdup(name);
        scr->type = LOKI_FILE_SCRIPT;
        scr->data.scr_type = type;
        return scr;
    }
    return NULL;
}

/* Register a new script for the component. 'script' holds the whole script in one string */
int loki_registerscript_component(product_component_t *comp, script_type_t type, const char *name,
                                  const char *script)
{
    product_file_t *file = registerscript(comp->node, type, name, script, comp->product);
    if ( file ) {
        file->option = NULL;
        file->next = comp->scripts;
        comp->scripts = file;
        return 0;
    }
    return -1;
}

int loki_registerscript(product_option_t *opt, script_type_t type, const char *name,
                                  const char *script)
{
    product_file_t *file = registerscript(opt->node, type, name, script, opt->component->product);
    if ( file ) {
        file->option = opt;
        file->next = opt->files;
        opt->files = file;
        return 0;
    }
    return -1;
}

int loki_registerscript_fromfile_component(product_component_t *comp, script_type_t type,
                                           const char *name, const char *path)
{
    struct stat st;
    int ret = -1;

    if ( !stat(path, &st) ) {
        FILE *fd;
        char *script = (char *)malloc(st.st_size);

        fd = fopen(path, "r");
        if ( fd ) {
            product_file_t *file;
            fread(script, st.st_size, 1, fd);
            fclose(fd);
            file = registerscript(comp->node, type, name, script, comp->product);
            if ( file ) {
                file->option = NULL;
                file->next = comp->scripts;
                comp->scripts = file;
                ret = 0;
            }
        }
        free(script);
    }
    return ret;
}

int loki_registerscript_fromfile(product_option_t *opt, script_type_t type,
                                 const char *name, const char *path)
{
    struct stat st;
    int ret = -1;

    if ( !stat(path, &st) ) {
        FILE *fd;
        char *script = (char *)malloc(st.st_size);

        fd = fopen(path, "r");
        if ( fd ) {
            product_file_t *file;
            fread(script, st.st_size, 1, fd);
            fclose(fd);
            file = registerscript(opt->node, type, name, script, opt->component->product);
            if ( file ) {
                file->option = opt;
                file->next = opt->files;
                opt->files = file;
                ret = 0;
            }
        }
        free(script);
    }
    return ret;
}

/* Unregister a registered script, and remove the file */
int loki_unregister_script(product_component_t *comp, const char *name)
{
    char buf[PATH_MAX];
    int ret = 0;
    product_file_t *file;
    product_option_t *opt;

    snprintf(buf, sizeof(buf), "%s/.manifest/scripts/%s.sh", comp->product->info.root, name);

    if ( !access(buf, W_OK) && unlink(buf)<0 ) {
        perror("unlink");
        ret ++;
    }

    /* First look at global scripts */
    for ( file = comp->scripts; file; file = file->next ) {
        if( !strcmp(file->path, name) ) {
            unregister_file(file, &comp->scripts);
            comp->product->changed = 1;
            return ret;
        }
    }

    for ( opt = comp->options; opt; opt = opt->next ) {
        for ( file = opt->files; file; file = file->next ) {
            if( !strcmp(file->path, name) ) {
                unregister_file(file, &opt->files);
                comp->product->changed = 1;
                return ret;
            }
        }
    }

    return ++ret;
}

static int run_script(product_t *prod, const char *name)
{
    char cmd[PATH_MAX];
    
    if ( name ) {
        snprintf(cmd, sizeof(cmd), "/bin/sh %s/.manifest/scripts/%s", prod->info.root, name);
        system(cmd);
        return 1;
    }
    return 0;
}

/* Run all scripts of a given type, returns the number of scripts successfully run */
int loki_runscripts(product_component_t *comp, script_type_t type)
{
    int count = 0;
    product_file_t *file;
    product_option_t *opt;

    /* First look at global scripts */
    for ( file = comp->scripts; file; file = file->next ) {
        if( file->data.scr_type == type ) {
            count += run_script(comp->product, file->path);
        }
    }

    for ( opt = comp->options; opt; opt = opt->next ) {
        for ( file = opt->files; file; file = file->next ) {
            if( file->type==LOKI_FILE_SCRIPT && file->data.scr_type==type ) {
                count += run_script(comp->product, file->path);
            }
        }
    }

    return count;
}

/* This copies a binary that might be from a CD mounted with noexec attributes to 
   a temporary place where we are sure to be able to run it */
static const char *copy_binary_to_tmp(const char *path)
{
    int dst, src;
    static char tmppath[PATH_MAX];
    struct stat st;
    void *buffer;

    strcpy(tmppath, "/tmp/setupdb-bin.XXXXXX");

    dst = mkstemp(tmppath);
    if ( dst < 0 ) {
        perror("mkstemp");
        return NULL;
    }

    src = open(path, O_RDONLY);
    if ( src < 0 ) {
        perror("open");
        return NULL;
    }

    fstat(src, &st);
    buffer = malloc(st.st_size);

    read(src, buffer, st.st_size);
    write(dst, buffer, st.st_size);

    free(buffer);
    close(src);
    fchmod(dst, 0755);
    close(dst);

    return tmppath;
}

/* Perform automatic update of the uninstaller binaries and script.
   'src' is the path where updated binaries can be copied from.
 */
int loki_upgrade_uninstall(product_t *product, const char *src_bins)
{
    char binpath[PATH_MAX];
    const char *os_name = detect_os();
    int perform_upgrade = 0;
    product_info_t *pinfo;
    FILE *scr;


    /* TODO: Locate global loki-uninstall and upgrade it if we have sufficient permissions */    

    snprintf(binpath, sizeof(binpath), "%s/.loki/installed/bin", getenv("HOME"));
    mkdir(binpath, 0755);

    strncat(binpath, "/", sizeof(binpath));
    strncat(binpath, os_name, sizeof(binpath));
    mkdir(binpath, 0755);

    strncat(binpath, "/", sizeof(binpath));
    strncat(binpath, detect_arch(), sizeof(binpath));
    mkdir(binpath, 0755);

    strncat(binpath, "/uninstall", sizeof(binpath));

    if ( !access(binpath, F_OK) ) {
        char cmd[PATH_MAX];
        FILE *pipe;
        
        snprintf(cmd, sizeof(cmd), "%s --version", binpath);
        pipe = popen(cmd, "r");
        if ( pipe ) {
            int major, minor, rel;
            const char *tmpbin;

            /* Try to see if we have to update it */
            fscanf(pipe, "%d.%d.%d", &major, &minor, &rel);
            pclose(pipe);

            /* Now check against the version of the binaries we have */
            tmpbin = copy_binary_to_tmp(src_bins);
            if ( tmpbin ) {
                snprintf(cmd, sizeof(cmd), "%s --version", tmpbin);
            } else { /* We still try to run it if the copy failed for some reason */
                snprintf(cmd, sizeof(cmd), "%s --version", src_bins);
            }
            pipe = popen(cmd, "r");
            if ( pipe ) {
                int our_maj, our_min, our_rel;
                fscanf(pipe, "%d.%d.%d", &our_maj, &our_min, &our_rel);
                pclose(pipe);
                if ( (major < our_maj) || 
                     ((major==our_maj) && (minor < our_min)) ||
                     ((major==our_maj) && (minor == our_min) &&
                      (rel < our_rel )) ) {
                    /* Perform the upgrade, overwrite the uninstall binary */
                    perform_upgrade = 1;
                }
            }
            if ( tmpbin ) { /* Remove the copied binary */
                unlink(tmpbin);
            }
        } else {
            perror("popen");
        }
    } else {
        perform_upgrade = 1;
    }

    /* TODO: Try to install global command in the symlinks path */

    if ( perform_upgrade ) {
        FILE *src, *dst;
        void *data;
        struct stat st;
        
        stat(src_bins, &st);
        src = fopen(src_bins, "rb");
        if ( src ) {
            dst = fopen(binpath, "wb");
            if ( dst ) {
                data = malloc(st.st_size);
                fread(data, 1, st.st_size, src);
                fwrite(data, 1, st.st_size, dst);
                free(data);
                fchmod(fileno(dst), 0755);
                fclose(dst);                
            } else {
                fprintf(stderr, "Couldn't write to %s!\n", binpath);
            }
            fclose(src);
        } else {
            fprintf(stderr, "Couldn't open %s to be copied!\n", src_bins);
        }
    }

    /* Now we create an 'uninstall' shell script in the game's installation directory */
    pinfo = loki_getinfo_product(product);
    snprintf(binpath, sizeof(binpath), "%s/uninstall", pinfo->root);
    scr = fopen(binpath, "w");
    if ( scr ) {
        fprintf(scr,
                "#! /bin/sh\n"
                "#### UNINSTALL SCRIPT - Generated by SetupDB %d.%d #####\n"
                "DetectARCH()\n"
                "{\n"
                "        status=1\n"
                "        case `uname -m` in\n"
                "                i?86)  echo \"x86\"\n"
                "                        status=0;;\n"
                "                *)     echo \"`uname -m`\"\n"
                "                        status=0;;\n"
                "        esac\n"
                "        return $status\n"
                "}\n\n"

                "if which loki_uninstall > /dev/null 2>&1; then\n"
                "    UNINSTALL=loki_uninstall\n"
                "else\n"
                "    UNINSTALL=\"$HOME/.loki/installed/bin/`uname -s`/`DetectARCH`/uninstall\"\n"
                "    if [ ! -x \"$UNINSTALL\" ]; then\n"
                "        echo Could not find a usable uninstall program. Aborting.\n"
                "        exit 1\n"
                "    fi\n"
                "fi\n"
                "\"$UNINSTALL\" \"%s\"",
                SETUPDB_VERSION_MAJOR, SETUPDB_VERSION_MINOR,
                pinfo->registry_path);
        fchmod(fileno(scr), 0755);
        fclose(scr);
    } else {
        fprintf(stderr, "Could not write uninstall script: %s\n", binpath);
    }
    return 0;
}
