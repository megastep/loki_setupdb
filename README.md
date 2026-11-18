
# Loki SetupDB 1.6.5


## Introduction

This is a product registry database written by Loki Software, Inc.
It is used by the various product install/update/uninstall tools
released by Loki Software, Inc., and should be built before 
configuring any of the other tools.


## Building

Install a C compiler, GNU Make, Autoconf, Automake, pkg-config, and the libxml2
and zlib development files. From the repository root run:

```sh
./autogen.sh
./configure
make -j4 all setupdb
./setupdb probe sysinfo
make check
```

The default target builds `<arch>/libsetupdb.a` and records its libxml ABI in
`<arch>/xml-major`. Keep this checkout beside
`loki_setup` so its configure script can find the archive and public headers.
The optional `make convert md5sum` targets build the manifest conversion and
checksum tools. Libxml2 is the default. Use `--without-libxml2` for a legacy
libxml 1 build. Run `make dist` to package a source release in the parent
directory.

An absolute `SETUPDB_ROOT` selects a separate registry for disposable installs.
The installer smoke test uses this to keep its product records in a temporary
directory.

## Licensing

This update tool is available for use under the GNU General Public License.


## Documentation

This library is documented through the header setupdb.h


## Feedback

You can send feedback to setup@lokigames.com, or join the fun on the
Loki Setup newsgroup: <news://news.lokigames.com/loki.open-source.setup>


## Authors

Loki SetupDB was written by Stéphane Peter and Sam Lantinga at
Loki Software, Inc.

