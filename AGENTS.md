# Repository Guidelines

## Project Structure

Loki SetupDB is the C library that Loki’s installer, updater, and uninstaller use to track installed products. Its sources live at the repository root. Start with `setupdb.h` for the public API and ownership rules, `setupdb.c` for registry operations, and `setup-xml.h` for libxml compatibility. Platform detection lives in `arch.c` and checksum handling in `md5.c`.

`register.c` implements the `setupdb` command-line tool. Its command handlers show how callers work with products, components, options, and files.

## Build and Run

Use Autoconf, Automake, a C compiler, GNU Make, and the libxml2 development files. The repository’s CodeQL workflow selects libxml2 explicitly. From the repository root, run:

```sh
./autogen.sh
./configure --with-libxml2
make
```

The default target creates `<arch>/libsetupdb.a`. Build it before configuring the sibling `loki_setup` checkout. To build the CLI, run `make setupdb`. Use `./setupdb probe sysinfo` to inspect platform detection. The first argument is a placeholder for this command, so it works without a registered product.

## Coding Style

Follow the indentation of the surrounding C function and keep Makefile recipes tab-indented. Match public registry names such as `loki_create_component` and handle types such as `product_t`. Document ownership in `setupdb.h` when changing what a caller must free. Make build changes in `configure.ac` or `Makefile.in`, then regenerate locally.

## Testing

Run `make check` to verify the detected architecture and the checksum of a known input. For a persistence change, use a disposable registered product. Run `./setupdb PRODUCT create Extras 1.0 Docs docs`, then run `./setupdb PRODUCT printtags Extras` in a new process and verify that it prints `docs`. Substitute the disposable product’s name for `PRODUCT`. Reopening the registry checks that the change reached disk.

## Commits and Pull Requests

Use a short subject naming the behavior or platform affected, as in `Fix 64-bit glibc detection.` In a pull request, include the platform, configure options, and verification commands. For registry changes, describe the effect on existing product records and downstream callers.
