/* Command-line utility to manipulate product entries from scripts */

/* $Id: register.c,v 1.7 2003-02-27 06:15:10 megastep Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "setupdb.h"

product_t *product;

void print_usage(const char *argv0)
{
    printf("Usage: %s product [command] [args]\n"
           "Recognized commands are :\n"
		   "   create component version [option_name [option_tag]]\n"
		   "      Create a new component and/or option in the component\n"
           "   add component option files    Register files in the component / option\n"
           "   update component option files Updates registration information\n"
           "   remove files                  Remove specified files from the product\n",
           argv0);
}

/* Create - does not update the version or tags ! */
int create_option(const char *component, const char *ver, const char *option_name, const char *option_tag)
{
    product_component_t *comp;
    product_option_t *opt;

    comp = loki_find_component(product, component);
    if ( ! comp ) {
		comp = loki_create_component(product, component, ver);
		if ( ! comp ) {
			fprintf(stderr, "Failed to create component '%s' version '%s'\n", component, ver);
			return 0;
		}
	}
	if ( option_name ) {
		opt = loki_find_option(comp, option_name);
		if ( ! opt ) {
			opt = loki_create_option(comp, option_name, option_tag);
			if ( ! opt ) {
				fprintf(stderr, "Failed to create option '%s' in component '%s'\n", option_name, component);
				return 0;
			}
		}
	}
	return 1;
}

int register_files(const char *component, const char *option, char **files)
{
    product_component_t *comp;
    product_option_t *opt;

    comp = loki_find_component(product, component);
    if ( ! comp ) {
        fprintf(stderr,"Unable to find component %s !\n", component);
        return 0;
    }
    opt = loki_find_option(comp, option);
    if ( ! opt ) {
        fprintf(stderr,"Unable to find option %s !\n", option);
        return 0;
    }

    for ( ; *files; files ++ ) {
        if ( ! loki_register_file(opt, *files, NULL) ) {
            fprintf(stderr,"Error while registering %s\n", *files);
            return 0;
        }
    }

    return 1;
}

int remove_files(char **files)
{
    for ( ; *files; files ++ ) {
        if ( loki_unregister_file(loki_findpath(*files, product)) < 0) {
            fprintf(stderr,"Error while removing %s\n", *files);
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    if ( argc < 3 ) {
        print_usage(argv[0]);
        return 1;
    }

    product = loki_openproduct(argv[1]);
    if ( ! product ) {
        fprintf(stderr,"Unable to open product %s\n", argv[1]);
        return 1;
    }
    if ( !strcmp(argv[2], "add") || !strcmp(argv[2], "update")) {
        if ( argc < 6 ) {
            print_usage(argv[0]);
        } else {
            register_files(argv[3], argv[4], &argv[5]);
        }
    } else if ( !strcmp(argv[2], "remove") ) {
        if ( argc < 4 ) {
            print_usage(argv[0]);
        } else {
            remove_files(&argv[3]);
        }
    } else if ( !strcmp(argv[2], "create") ) {
        if ( argc < 5 ) {
            print_usage(argv[0]);
        } else {
			create_option(argv[3], argv[4], argc>5 ? argv[5] : NULL, argc>6 ? argv[6] : NULL);
		}
    } else {
        print_usage(argv[0]);
    }
    loki_closeproduct(product);
    return 0;
}
