/* Tool to convert old 'uninstall' scripts generated by setup into an XML file */
/* $Id: convert.c,v 1.12 2000-10-24 20:14:50 megastep Exp $ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "setupdb.h"

static const char *remove_root(const char *root, char *path)
{
    /* While we're at it, remove the ending quote that scanf left */
    path[strlen(path)-1] = '\0';

	if(strcmp(path, root) &&
       strstr(path, root) == path) {
		return path + strlen(root) + 1;
	}
	return path;
}


/* Find the path in which the uninstall script is */
const char *get_path(const char *path)
{
    static char buf[PATH_MAX];
    char *ptr;

    if ( *path == '/' ) {
        strncpy(buf, path, sizeof(buf));
    } else {
        getcwd(buf, sizeof(buf));
        strncat(buf, "/", sizeof(buf));
        strncat(buf, path, sizeof(buf));
    }
    ptr = strrchr(buf, '/');
    if ( ptr ) {
        *ptr = '\0';
    }
    return buf;
}

void parse_line(const char *head, char *buf, size_t len, FILE *f)
{
    char line[4096];

    fgets(line, sizeof(line), f);
    /* Chop newline */
    line[strlen(line)-1] = '\0';

    /* Skip head if it's there */
    if ( strncmp(line, head, strlen(head)) == 0 ) {
        strncpy(buf, line+strlen(head), len);
    } else {
        strncpy(buf, line, len);
    }
}

/* Check whether the uninstall script is an old-style script */
int check_uninstall(const char *path)
{
    FILE *f = fopen(path, "r");
    if ( f ) {
        char line[256];

        /* Skip the first line : #!/bin/sh */
        fgets(line, sizeof(line), f);
        /* We check for a special comment on the second line */
        fgets(line, sizeof(line), f);
        fclose(f);

        return strstr(line, "#### UNINSTALL SCRIPT") == NULL;
    }
    return 0;
}

int convert_uninstall(const char *path, const char *name, const char *vername,
                      const char *uninstall, const char *update_url)
{
    FILE *script = fopen(path, "r");
    if ( script ) {
        product_t *product;
        product_component_t *component;
        product_option_t *option;
        const char *root = get_path(path);
        char url[PATH_MAX], curdir[PATH_MAX], desc[200];

        if ( access(root, W_OK) < 0 ) {
            fprintf(stderr, "No write access to the installation directory.\nAborting.\n");
            return 1;
        }

        getcwd(curdir, sizeof(curdir));
        chdir(root);

        if ( update_url ) {
            strncpy(url, update_url, sizeof(url));
        } else {
            snprintf(url, sizeof(url), "ftp://ftp.lokigames.com/pub/patches/%s/patchlist.txt", name);
        }

        /* Skip the first line */
        fgets(desc, sizeof(desc), script);
        /* Get the description from the comment */
        parse_line("# Uninstall script for ", desc, sizeof(desc), script);
        if ( *desc == '#' ) {
            /* Didn't change, it's probably an old Myth2 uninstall script */
            strncpy(desc, "Myth II: Soulblighter", sizeof(desc));
        }

        product = loki_create_product(name, root, desc, url);
        if ( product ) {
            char word[10], file[PATH_MAX];

            component = loki_create_component(product, "Default Component", vername);
            option = loki_create_option(component, "Default Option");

            /* Parse the file, 'rm -f' lines are files, 'rmdir' are directories */
            while ( ! feof(script) ) {
                const char *str;
                fscanf(script, "%10s", word);
                /* Warning: the following code will not work if spaces are in the path name */
                if ( !strcmp(word, "rm") ) {
                    parse_line(" -f \"", file, sizeof(file), script);
                    str = remove_root(root, file);
                    if ( strcmp(str, "uninstall") ) { /* Do not register the uninstall script */
                        struct stat st;
                        product_file_t *pf = loki_register_file(option, str, NULL);
                        if ( !stat(str, &st) ) { /* Set the mode for executable files */
                            loki_setmode_file(pf, st.st_mode & 0777);
                        }
                    }
                } else if ( !strcmp(word, "rmdir") ) {
                    parse_line(" \"", file, sizeof(file), script);
                    str = remove_root(root, file);
                    if ( strncmp(str, root, strlen(str)) ) { /* ... and the install directory */
                        loki_register_file(option, str, NULL);
                    }
                } else {
                    /* Skip till the end of the line */
                    char c;
                    do {
                        c = fgetc(script);
                    } while ( (c!='\n') && (c!=EOF) );
                }
            }
        }

        fclose(script);
        chdir(curdir);
        loki_upgrade_uninstall(product, uninstall);
        loki_closeproduct(product);

    } else {
        perror("fopen");
        return 1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    if ( argc < 4 ) {
        printf("Usage: %s /path/to/uninstall product version [url]\n", argv[0]);
        return 1;
    }
    
    /* The uninstall binary must be in the same directory as convert */
    if ( check_uninstall(argv[1]) ) { 
        char uninst[PATH_MAX];
        snprintf(uninst, sizeof(uninst), "%s/uninstall", get_path(argv[0]));
        return convert_uninstall(argv[1], argv[2], argv[3], uninst, argv[4]);
    } else {
        // printf("The uninstall script has already been upgraded. Skipping.\n");
        return 0;
    }
}
