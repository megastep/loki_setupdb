/* Tool to convert old 'uninstall' scripts generated by setup into an XML file */
/* $Id: convert.c,v 1.3 2000-10-12 02:12:47 megastep Exp $ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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

int convert_uninstall(const char *path, const char *name, const char *vername)
{
    FILE *script = fopen(path, "r");
    if ( script ) {
        product_t *product;
        product_component_t *component;
        product_option_t *option;
        const char *root = get_path(path);

        chdir(root);

        product = loki_create_product(name, root, NULL, "http://www.lokigames.com/products/");
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
                    fscanf(script, " -f \"%s\"", file);
                    str = remove_root(root, file);
                    loki_register_file(option, str, NULL);
                } else if ( !strcmp(word, "rmdir") ) {
                    fscanf(script, " \"%s\"", file);
                    str = remove_root(root, file);
                    loki_register_file(option, str, NULL);
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
        printf("Usage: %s /path/to/uninstall product version\n", argv[0]);
        return 1;
    }

    return convert_uninstall(argv[1], argv[2], argv[3]);
}
