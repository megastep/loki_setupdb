/* Command-line utility to manipulate product entries from scripts */

/* $Id: register.c,v 1.10 2003-03-07 04:04:52 megastep Exp $ */

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
		   "   script component type name path-to-script\n"
		   "                 Register a new pre/post-install script for the component.\n"
           "   update component option files Updates registration information\n"
           "   remove files                  Remove specified files from the product\n"
		   "   printtags [component]          Print installed option tags\n",
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
			return 1;
		}
	}
	if ( option_name ) {
		opt = loki_find_option(comp, option_name);
		if ( ! opt ) {
			opt = loki_create_option(comp, option_name, option_tag);
			if ( ! opt ) {
				fprintf(stderr, "Failed to create option '%s' in component '%s'\n", option_name, component);
				return 1;
			}
		}
	}
	return 0;
}

int printtags(const char *component)
{
    product_component_t *comp;
    product_option_t *opt;
	const char *tag;

	if ( component ) {
		comp = loki_find_component(product, component);
		if ( ! comp ) {
			fprintf(stderr,"Unable to find component %s !\n", component);
			return 1;
		}
		for ( opt = loki_getfirst_option(comp); opt; opt = loki_getnext_option(opt) ) {
			tag = loki_gettag_option(opt);
			if ( tag ) {
				printf("%s ", tag);
			}
		}
	} else { /* Do all components */
		for ( comp = loki_getfirst_component(product); comp; comp = loki_getnext_component(comp) ) {
			for ( opt = loki_getfirst_option(comp); opt; opt = loki_getnext_option(opt) ) {
				tag = loki_gettag_option(opt);
				if ( tag ) {
					printf("%s ", tag);
				}
			}
		}
	}
	return 0;
}


int register_script(const char *component, script_type_t type, const char *name, const char *script)
{
    product_component_t *comp;

    comp = loki_find_component(product, component);
    if ( ! comp ) {
        fprintf(stderr,"Unable to find component %s !\n", component);
        return 1;
    }

	if ( loki_registerscript_fromfile_component(comp, type, name, script) < 0 ){
		fprintf(stderr,"Error trying to register script for component %s !\n", component);
		return 1;
	}
	return 0;
}

int register_files(const char *component, const char *option, char **files)
{
    product_component_t *comp;
    product_option_t *opt;

    comp = loki_find_component(product, component);
    if ( ! comp ) {
        fprintf(stderr,"Unable to find component %s !\n", component);
        return 1;
    }
    opt = loki_find_option(comp, option);
    if ( ! opt ) {
        fprintf(stderr,"Unable to find option %s !\n", option);
        return 1;
    }

    for ( ; *files; files ++ ) {
        if ( ! loki_register_file(opt, *files, NULL) ) {
            fprintf(stderr,"Error while registering %s\n", *files);
            return 1;
        }
    }

    return 0;
}

int remove_files(char **files)
{
    for ( ; *files; files ++ ) {
        if ( loki_unregister_file(loki_findpath(*files, product)) < 0) {
            fprintf(stderr,"Error while removing %s\n", *files);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
	int ret = 1;
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
            ret = register_files(argv[3], argv[4], &argv[5]);
        }
    } else if ( !strcmp(argv[2], "remove") ) {
        if ( argc < 4 ) {
            print_usage(argv[0]);
        } else {
            ret = remove_files(&argv[3]);
        }
    } else if ( !strcmp(argv[2], "create") ) {
        if ( argc < 5 ) {
            print_usage(argv[0]);
        } else {
			ret = create_option(argv[3], argv[4], argc>5 ? argv[5] : NULL, argc>6 ? argv[6] : NULL);
		}
	} else if ( !strcmp(argv[2], "script") ) {
        if ( argc < 7 ) {
            print_usage(argv[0]);
        } else {
			if ( !strcmp(argv[4], "pre") ) {
				ret = register_script(argv[3], LOKI_SCRIPT_PREUNINSTALL, argv[5], argv[6]);
			} else if ( !strcmp(argv[4], "post") ) {
				ret = register_script(argv[3], LOKI_SCRIPT_POSTUNINSTALL, argv[5], argv[6]);
			} else {
				print_usage(argv[0]);
			}
		}
    } else if ( !strcmp(argv[2], "printtags") ) {
		ret = printtags(argv[3]);
    } else {
        print_usage(argv[0]);
    }
    loki_closeproduct(product);
    return ret;
}
