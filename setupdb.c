/* Implementation of the Loki Product DB API */
/* $Id: setupdb.c,v 1.2 2000-10-11 02:13:00 hercules Exp $ */

#include <glob.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <parser.h>
#include <tree.h>

#include "setupdb.h"
#include "md5.h"

struct _loki_product_t
{
    xmlDocPtr doc;
    product_info_t info;
    product_file_t file;
    int changed;
};

struct _loki_product_component_t
{
    xmlNodePtr node;
    product_t *product;
    char *name;
    char *version;
    char *url;
    int is_default;
};

struct _loki_product_option_t
{
    xmlNodePtr node;
    product_component_t *component;
    char *name;
    product_file_t file;
    xmlNodePtr curfile;
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
    const char *str;
    xmlDocPtr doc;
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

    /* TODO: Check for the xmlversion attribute for backwards compatibility */

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

    return prod;
}

/* Close a product entry and free all allocated memory.
   Also writes back to the database all changes that may have been made.
 */

int loki_closeproduct(product_t *product)
{
    char path[PATH_MAX];

    if ( product->changed ) {
        /* Write XML file to disk if it has changed */
        snprintf(path, sizeof(path), "%s/.manifest/%s.xml", product->info.root, product->info.name);
        xmlSaveFile(path, product->doc);
    }
    free(product);
    return 0;
}

/* Clean up a product from the registry, i.e. removes all support files and directories */
int loki_removeproduct(product_t *product)
{
    char buf[PATH_MAX];

    /* Remove the XML file */
    snprintf(buf, sizeof(buf), "%s/.manifest/%s.xml", product->info.root, product->info.name);
    unlink(buf);

    /* TODO: Remove the scripts for each version */
    // loki_iterate_iniline(product->ini, PRODUCT_SCRIPTS_SECTION, remove_script_cb, product->info.root);

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
    
    free(product);

    return 0;
}

/* Get a pointer to the product info */

product_info_t *loki_getproductinfo(product_t *product)
{
    return &product->info;
}

/* Enumerate the installed options */

product_component_t *loki_getfirst_component(product_t *product)
{
    xmlNodePtr node = product->doc->root->childs;
    while(node) {
        if(!strcmp(node->name, "component")) {
            const char *txt;
            product_component_t *ret = (product_component_t *)malloc(sizeof(product_component_t));

            ret->node = node;
            ret->product = product;
            ret->name = strdup(xmlGetProp(node, "name"));
            ret->version = strdup(xmlGetProp(node, "version"));
            txt = xmlGetProp(node, "update_url");
            ret->url = txt ? strdup(txt) : NULL;
            ret->is_default = (xmlGetProp(node, "default") != NULL);
            return ret;
        }
        node = node->next;
    }
    return NULL;
}

product_component_t *loki_getnext_component(product_component_t *component)
{
    while(component->node) {
        component->node = component->node->next;
        if(component->node && !strcmp(component->node->name, "component")) {
            const char *txt;

            free(component->name);
            free(component->version);
            free(component->url);
            component->name = strdup(xmlGetProp(component->node, "name"));
            component->version = strdup(xmlGetProp(component->node, "version"));
            txt = xmlGetProp(component->node, "update_url");
            component->url = txt ? strdup(txt) : NULL;
            component->is_default = (xmlGetProp(component->node, "default") != NULL);
            return component;
        }
    }
    return NULL;
}

product_component_t *loki_getdefault_component(product_t *product)
{
    xmlNodePtr node = product->doc->root->childs;
    while ( node ) {
        if(!strcmp(node->name, "component")) {
            const char *txt = xmlGetProp(node, "default");
            if (txt && !strcmp(txt,"yes")) {
                product_component_t *ret = (product_component_t *)malloc(sizeof(product_component_t));
                ret->node = node;
                ret->product = product;
                ret->name = strdup(xmlGetProp(node, "name"));
                ret->version = strdup(xmlGetProp(node, "version"));
                txt = xmlGetProp(node, "update_url");
                ret->url = txt ? strdup(txt) : NULL;
                ret->is_default = 1;
                return ret;                
            }
        }
        node = node->next;
    }
    return NULL;
}

static int has_default_component(product_t *product)
{
    xmlNodePtr node = product->doc->root->childs;
    while ( node ) {
        if(!strcmp(node->name, "component")) {
            const char *txt = xmlGetProp(node, "default");
            if (txt && !strcmp(txt,"yes")) {
                return 1;
            }
        }
        node = node->next;
    }
    return 0;
}

product_component_t *loki_find_component(product_t *product, const char *name)
{
    xmlNodePtr node = product->doc->root->childs;
    while ( node ) {
        if(!strcmp(node->name, "component")) {
            const char *txt = xmlGetProp(node, "name");
            if (txt && !strcmp(txt, name) ) {
                product_component_t *ret = (product_component_t *)malloc(sizeof(product_component_t));
                ret->node = node;
                ret->product = product;
                ret->name = strdup(txt);
                txt = xmlGetProp(node, "update_url");
                ret->url = txt ? strdup(txt) : NULL;
                ret->version = strdup(xmlGetProp(node, "version"));
                ret->is_default = (xmlGetProp(node, "default") != NULL);
                return ret;                
            }
        }
        node = node->next;
    }
    return NULL;
}

void loki_free_component(product_component_t *component)
{
    free(component->name);
    free(component->version);
    free(component->url);
    free(component);
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
        ret->is_default = !has_default_component(product);
        product->changed = 1;
        xmlSetProp(node, "name", name);
        xmlSetProp(node, "version", version);
        if(ret->is_default)
            xmlSetProp(node, "default", "yes");
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

/* Get the URL for updates for a component; defaults to the product's URL if not defined */
const char *loki_geturl_component(product_component_t *comp)
{
    return comp->url ? comp->url : comp->product->info.url;
}

/* Enumerate options from components */

product_option_t *loki_getfirst_option(product_component_t *component)
{
    xmlNodePtr node = component->node->childs;
    while(node) {
        if(!strcmp(node->name, "option")) {
            product_option_t *ret = (product_option_t *)malloc(sizeof(product_option_t));
            ret->node = node;
            ret->component = component;
            ret->name = strdup(xmlGetProp(node, "name"));
            return ret;
        }
        node = node->next;
    }
    return NULL;
}

product_option_t *loki_getnext_option(product_option_t *opt)
{
    while(opt->node) {
        opt->node = opt->node->next;
        if(opt->node && !strcmp(opt->node->name, "option")) {
            free(opt->name);
            opt->name = strdup(xmlGetProp(opt->node, "name"));
            return opt;
        }
    }
    return NULL;
}

product_option_t *loki_find_option(product_component_t *comp, const char *name)
{
    xmlNodePtr node = comp->node->childs;
    while(node) {
        if(!strcmp(node->name, "option")) {
            const char *txt = xmlGetProp(node, "name");
            if ( txt && !strcmp(txt, name) ) {
                product_option_t *ret = (product_option_t *)malloc(sizeof(product_option_t));
                ret->node = node;
                ret->component = comp;
                ret->name = strdup(txt);
                return ret;
            }
        }
        node = node->next;
    }
    return NULL;
}

const char *loki_getoptionname(product_option_t *opt)
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
        component->product->changed = 1;
        xmlSetProp(node, "name", name);
        return ret;
    }
    return NULL;
}

void loki_free_option(product_option_t *option)
{
    free(option->name);
    free(option);
}

/* Enumerate files from options */

static product_file_t *process_file(product_option_t *opt)
{
    const char *txt = get_xml_string(opt->component->product, opt->curfile);
    xmlNodePtr node = opt->curfile;
    file_type_t t;

    memset(opt->file.md5sum, 0, 16);
    if ( !strcmp(node->name, "file") ) {
        const char *md5 = xmlGetProp(node, "md5sum");
        t = LOKI_FILE_REGULAR;
        if ( md5 ) 
            memcpy(opt->file.md5sum, get_md5_bin(md5), 16);
    } else if ( !strcmp(node->name, "directory") ) {
        t = LOKI_FILE_DIRECTORY;
    } else if ( !strcmp(node->name, "symlink") ) {
        t = LOKI_FILE_SYMLINK;
    } else if ( !strcmp(node->name, "fifo") ) {
        t = LOKI_FILE_FIFO;
    } else if ( !strcmp(node->name, "device") ) {
        t = LOKI_FILE_DEVICE;
    } else if ( !strcmp(node->name, "rpm") ) {
        t = LOKI_FILE_RPM;
    } else if ( !strcmp(node->name, "script") ) {
        t = LOKI_FILE_SCRIPT;
    } else {
        t = LOKI_FILE_NONE;
    }
    opt->file.type = t;

    if ( t == LOKI_FILE_RPM || t == LOKI_FILE_SCRIPT ) {
        strncpy(opt->file.path, txt, sizeof(opt->file.path));
    } else {
        expand_path(opt->component->product, txt, opt->file.path, sizeof(opt->file.path));
    }
    return &opt->file;
}

product_file_t *loki_getfirst_file(product_option_t *opt)
{
    opt->curfile = opt->node->childs;
    return opt->curfile ? process_file(opt) : NULL;
}

product_file_t *loki_getnext_file(product_option_t *opt)
{
    opt->curfile = opt->curfile->next;
    return opt->curfile ? process_file(opt) : NULL;
}

static xmlNodePtr find_file_by_name(product_option_t *opt, const char *path)
{
    xmlNodePtr node = opt->node;
    const char *txt;
    while( node ) {
        txt = get_xml_string(opt->component->product, node);
        if(txt && !strcmp(path, txt))
           return node;
        node = node->next;
    }
    return NULL;
}

/* Enumerate all files of a component through a callback function */
int loki_enumerate_files(product_option_t *opt, product_file_cb cb)
{
    xmlNodePtr node = opt->node->childs;
    file_type_t t;
    char buf[PATH_MAX];
    int count = 0;

    while ( node ) {
        const char *txt = get_xml_string(opt->component->product, node);

        if ( !strcmp(node->name, "file") ) {
            t = LOKI_FILE_REGULAR;
        } else if ( !strcmp(node->name, "directory") ) {
            t = LOKI_FILE_DIRECTORY;
        } else if ( !strcmp(node->name, "symlink") ) {
            t = LOKI_FILE_SYMLINK;
        } else if ( !strcmp(node->name, "fifo") ) {
            t = LOKI_FILE_FIFO;
        } else if ( !strcmp(node->name, "device") ) {
            t = LOKI_FILE_DEVICE;
        } else if ( !strcmp(node->name, "rpm") ) {
            t = LOKI_FILE_RPM;
        } else if ( !strcmp(node->name, "script") ) {
            t = LOKI_FILE_SCRIPT;
        } else {
            t = LOKI_FILE_NONE;
        }

        if ( t == LOKI_FILE_RPM || t == LOKI_FILE_SCRIPT ) {
            strncpy(buf, txt, sizeof(buf));
        } else {
            expand_path(opt->component->product, txt, buf, sizeof(buf));
        }
        cb(buf, t, opt->component, opt);
        count ++;
        node = node->next;
    }
    return count;
}

int loki_updatemd5filecurrent(product_option_t *opt)
{    
    if (opt->curfile) {
        char md5sum[33];
        const char *path = get_xml_string(opt->component->product, opt->curfile);
        char fpath[PATH_MAX];

        md5_compute(expand_path(opt->component->product, path, fpath, sizeof(fpath)), md5sum, 1);
        xmlSetProp(opt->curfile, "md5", md5sum);
    }
    return -1;
}

file_info_t *loki_findpath(const char *path, product_t *product)
{
    if ( product ) {
        xmlNodePtr comp, opt, file;
        char fpath[PATH_MAX], abspath[PATH_MAX];

        if ( *path == '/' ) {
            strncpy(abspath, path, sizeof(abspath));
        } else {
            getcwd(abspath, sizeof(abspath));
            strncat(abspath, "/", sizeof(abspath));
            strncat(abspath, path, sizeof(abspath));
        }

        comp = product->doc->root->childs;
        while ( comp ) {
            if(!strcmp(comp->name, "component")) {
                opt = comp->childs;
                while ( opt ) {
                    if (!strcmp(opt->name, "option")) {
                        file = opt->childs;
                        while ( file ) {
                            if ( strcmp(file->name, "script") && strcmp(file->name, "rpm") ) {
                                const char *txt = get_xml_string(product, file);
                                if ( ! strcmp(abspath, expand_path(product, txt, fpath, sizeof(fpath))) ) {
                                    /* We found our match */
                                    file_info_t *ret = (file_info_t *)malloc(sizeof(file_info_t));

                                    /* Build component and option structures */
                                    ret->component = (product_component_t *)malloc(sizeof(product_component_t));
                                    ret->component->node = comp;
                                    ret->component->product = product;
                                    ret->component->name = strdup(xmlGetProp(comp,"name"));
                                    ret->component->version = strdup(xmlGetProp(comp,"version"));
                                    ret->component->is_default = (xmlGetProp(comp, "default") != NULL);

                                    ret->option = (product_option_t *)malloc(sizeof(product_option_t));
                                    ret->option->component = ret->component;
                                    ret->option->node = opt;
                                    ret->option->name = strdup(xmlGetProp(opt, "name"));

                                    /* Build file_info structure */
                                    ret->path = strdup(abspath);
                                    return ret;
                                }
                            }
                            file = file->next;
                        }
                    }
                    opt = opt->next;
                }
            }
            comp = comp->next;
        }
    } else {
        /* Try for each available product */
    }
    return NULL;
}

void loki_free_fileinfo(file_info_t *file)
{
    free(file->path);
    loki_free_component(file->component);
    loki_free_option(file->option);
    free(file);
}

static int registerfile_new(product_option_t *option, const char *path, const char *md5)
{
    struct stat st;
    char dev[10];
    xmlNodePtr node;

    if ( lstat(path, &st) < 0 ) {
        return -1;
    }
    if ( S_ISREG(st.st_mode) ) {
        if ( md5 ) {
            node = xmlNewChild(option->node, NULL, "file", path);
            xmlSetProp(node, "md5", md5);
        } else {
            char md5sum[33];
            if ( *path == '/' ) {
                md5_compute(path, md5sum, 1);
            } else {
                char fpath[PATH_MAX];
                snprintf(fpath, sizeof(fpath), "%s/%s", option->component->product->info.root, path);
                md5_compute(fpath, md5sum, 1);
            }
            node = xmlNewChild(option->node, NULL, "file", path);
            xmlSetProp(node, "md5", md5sum);
        }
    } else if ( S_ISDIR(st.st_mode) ) {
        node = xmlNewChild(option->node, NULL, "directory", path);
    } else if ( S_ISLNK(st.st_mode) ) {
        char buf[PATH_MAX];
        int count;

        node = xmlNewChild(option->node, NULL, "symlink", path);
        count = readlink(path, buf, sizeof(buf));
        if ( count < 0 ) {
            perror("readlink");
        } else {
            buf[count] = '\0';
            xmlSetProp(node, "dest", buf);
        }
    } else if ( S_ISFIFO(st.st_mode) ) {
        node = xmlNewChild(option->node, NULL, "fifo", path);
    } else if ( S_ISBLK(st.st_mode) ) {
        node = xmlNewChild(option->node, NULL, "device", path);
        xmlSetProp(node, "type", "block");
        /* Get the major/minor device number info */
        snprintf(dev,sizeof(dev),"%d", major(st.st_rdev));
        xmlSetProp(node, "major", dev);
        snprintf(dev,sizeof(dev),"%d", minor(st.st_rdev));
        xmlSetProp(node, "minor", dev);
    } else if ( S_ISCHR(st.st_mode) ) {
        node = xmlNewChild(option->node, NULL, "device", path);
        xmlSetProp(node, "type", "char");
        /* Get the major/minor device number info */
        snprintf(dev,sizeof(dev),"%d", major(st.st_rdev));
        xmlSetProp(node, "major", dev);
        snprintf(dev,sizeof(dev),"%d", minor(st.st_rdev));
        xmlSetProp(node, "minor", dev);
    } else {
        /* TODO: Warning? */
        return -1;
    }
    option->component->product->changed = 1;
    return 0;
}

static int registerfile_update(product_option_t *option, xmlNodePtr node,
                               const char *path, const char *md5)
{
    if ( !strcmp(node->name, "file") ) {
        if ( md5 ) {
            xmlSetProp(node, "md5", md5);
        } else {
            char md5sum[33];
            if ( *path == '/' ) {
                md5_compute(path, md5sum, 1);
            } else {
                char fpath[PATH_MAX];
                snprintf(fpath, sizeof(fpath), "%s/%s", option->component->product->info.root, path);
                md5_compute(fpath, md5sum, 1);
            }
            xmlSetProp(node, "md5", md5sum);
        }
    } else if ( !strcmp(node->name, "symlink") ) {
        char buf[PATH_MAX];
        int count;

        count = readlink(path, buf, sizeof(buf));
        if ( count < 0 ) {
            perror("readlink");
        } else {
            buf[count] = '\0';
            xmlSetProp(node, "dest", buf);
        }        
    }
    /* TODO: Do we need to update any other element type ? */
    return 0;
}

/* Register a file, returns 0 if OK */
int loki_registerfile(product_option_t *option, const char *path, const char *md5)
{
    xmlNodePtr node;

    /* Check if this file is already registered for this option */
    node = option->node->childs;
    while ( node ) {
        const char *txt = get_xml_string(option->component->product, node);
        if ( txt && !strcmp(txt, path) ) {
            /* Found it */
            return registerfile_update(option, node, path, md5);
        }
        node = node->next;
    }

    return registerfile_new(option, path, md5);
}

/* Remove a file from the registry. */
int loki_unregisterfile(product_option_t *option, const char *path)
{
    xmlNodePtr node = find_file_by_name(option, path);
    if ( node ) {
        xmlUnlinkNode(node);
        xmlFreeNode(node);
        option->component->product->changed = 1;
        return 0;
    }
    return -1;
}

/* Register a new RPM as having been installed by this product */
int loki_registerrpm(product_option_t *option, const char *name, const char *version, int revision,
                     int autoremove)
{
    xmlNodePtr node;
    char rev[10];

    node = xmlNewChild(option->node, NULL, "rpm", name);
    xmlSetProp(node, "version", version);
    snprintf(rev, sizeof(rev), "%d", revision);
    xmlSetProp(node, "revision", rev);
    xmlSetProp(node, "autoremove", autoremove ? "yes" : "no");

    option->component->product->changed = 1;

    return 0;
}

int loki_unregisterprm(product_t *product, const char *name)
{
    /* TODO: Search for a match */

    product->changed = 1;
    //    return !loki_remove_iniline(product->ini, "_RPM_", name);
    return -1;
}

static int registerscript(xmlNodePtr parent, script_type_t type, const char *name,
                          const char *script, product_t *product)
{
    char buf[PATH_MAX];
    FILE *fd;
    
    snprintf(buf, sizeof(buf), "%s/.manifest/scripts/%s.sh", product->info.root, name);
    fd = fopen(buf, "w");
    if (fd) {
        xmlNodePtr node;

        fprintf(fd, "#! /bin/sh\n");
        fprintf(fd, "%s", script);
        fchmod(fileno(fd), 0755);
        fclose(fd);

        node = xmlNewChild(parent, NULL, "script", name);
        xmlSetProp(node, "type", script_types[type]);
        product->changed = 1;

        return 0;
    }
    return -1;
}

/* Register a new script for the component. 'script' holds the whole script in one string */
int loki_registerscript_component(product_component_t *comp, script_type_t type, const char *name,
                                  const char *script)
{
    return registerscript(comp->node, type, name, script, comp->product);
}

int loki_registerscript(product_option_t *opt, script_type_t type, const char *name,
                                  const char *script)
{
    return registerscript(opt->node, type, name, script, opt->component->product);
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
            fread(script, st.st_size, 1, fd);
            fclose(fd);
            ret = registerscript(comp->node, type, name, script, comp->product);
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
            fread(script, st.st_size, 1, fd);
            fclose(fd);
            ret = registerscript(opt->node, type, name, script, opt->component->product);
        }
        free(script);
    }
    return ret;
}

/* Unregister a registered script, and remove the file */
int loki_unregisterscript(product_component_t *comp, const char *name)
{
    char buf[PATH_MAX];
    int ret = 0;
    xmlNodePtr node = comp->node->childs;

    snprintf(buf, sizeof(buf), "%s/.manifest/scripts/%s.sh", comp->product->info.root, name);

    if ( !access(buf, W_OK) && unlink(buf)<0 ) {
        perror("unlink");
        ret ++;
    }

    while (node) {
        if ( !strcmp(node->name, "option") ) {
            xmlNodePtr file = node->childs;
            while ( file ) {
                if ( !strcmp(file->name, "script") ) {
                    const char *str = xmlGetProp(file, "name");
                    if ( str && !strcmp(str, name) ) {
                        xmlUnlinkNode(file);
                        xmlFreeNode(file);
                        comp->product->changed = 1;
                        return ret;
                    }
                }
                file = file->next;
            }
        } else if ( !strcmp(node->name, "script") ) {
            const char *str = xmlGetProp(node, "name");
            if ( str && !strcmp(str, name) ) {
                xmlUnlinkNode(node);
                xmlFreeNode(node);
                comp->product->changed = 1;
                return ret;
            }
        }
        node = node->next;
    }
    return ++ret;
}

static int run_script(product_t *prod, xmlNodePtr node)
{
    char cmd[PATH_MAX];
    const char *txt = get_xml_string(prod, node);
    
    if ( txt ) {
        snprintf(cmd, sizeof(cmd), "/bin/sh %s/.manifest/scripts/%s", prod->info.root, txt);
        system(cmd);
        return 1;
    }
    return 0;
}

/* Run all scripts of a given type, returns the number of scripts successfully run */
int loki_runscripts(product_component_t *comp, script_type_t type)
{
    int count = 0;
    xmlNodePtr node = comp->node->childs;

    while (node) {
        if ( !strcmp(node->name, "option") ) {
            xmlNodePtr file = node->childs;
            while ( file ) {
                if ( !strcmp(file->name, "script") ) {
                    count += run_script(comp->product, node);
                }
                file = file->next;
            }
        } else if ( !strcmp(node->name, "script") ) {
            count += run_script(comp->product, node);
        }
        node = node->next;
    }
    return count;
}
