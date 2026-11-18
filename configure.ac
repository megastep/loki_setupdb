AC_PREREQ([2.69])
AC_INIT([Loki SetupDB], [1.6.5], [], [setupdb])
AC_CONFIG_SRCDIR([setupdb.c])
AC_CONFIG_HEADERS([config.h])

SETUPDB_VERSION_MAJOR=`echo "$PACKAGE_VERSION" | cut -d. -f1`
SETUPDB_VERSION_MINOR=`echo "$PACKAGE_VERSION" | cut -d. -f2`
SETUPDB_VERSION_RELEASE=`echo "$PACKAGE_VERSION" | cut -d. -f3`

AC_CANONICAL_HOST

AC_PROG_CC
CFLAGS="$CFLAGS -fsigned-char -Wall"
REENTRANT="-D_REENTRANT"

AC_CHECK_TOOL([AR], [ar])

AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(strings.h)
AC_CHECK_HEADERS(sys/utsname.h)
AC_CHECK_HEADERS(sys/sysmacros.h)
AC_CHECK_HEADERS(sys/mkdev.h)
AC_CHECK_HEADERS(sys/isa_defs.h)
AC_CHECK_HEADERS(glob.h)
AC_CHECK_HEADERS(elf.h)
AC_CHECK_HEADERS(getopt.h)
AC_CHECK_HEADERS(libintl.h)

GETOPT_C=""

AC_CHECK_FUNCS(setenv)
AC_CHECK_FUNCS(unsetenv)
AC_PATH_PROG(BRANDELF, brandelf, true)

STATIC=""
LIBSUFFIX=""


dnl Set the architecture flag
case "$host" in
*i?86*OpenUNIX* | *i?86*sco*)
	CFLAGS="$CFLAGS -Dsco"
	EXTRA_LIBS="$EXTRA_LIBS -L/udk/usr/lib -lsocket -lnsl -lresolv -lgen"
	ARCH=x86 ;;
hppa*hpux*)
	CFLAGS="$CFLAGS -I/opt/zlib/include -Dhpux -Dhppa"
	EXTRA_LIBS="$EXTRA_LIBS -ldld"
	STATIC="-Wl,-E"
	ARCH=hppa ;;
ia64*hpux*)
	LIBSUFFIX=/hpux32
	CFLAGS="$CFLAGS -I/usr/local/include -Dhpux -Dia64"
	EXTRA_LIBS="$EXTRA_LIBS -ldld"
	LIBS="-L/usr/local/lib$LIBSUFFIX $LIBS"
	STATIC="-Wl,-E"
	ARCH=ia64 ;;
*sgi*)
        LIBSUFFIX=32
	EXTRA_LIBS="$EXTRA_LIBS -L/usr/freeware/lib$LIBSUFFIX"
	ARCH=mips ;;
i?86*solaris*)
	EXTRA_LIBS="$EXTRA_LIBS -lsocket -lnsl -lresolv -ldl"
	ARCH=x86 ;;
*solaris*)
	EXTRA_LIBS="$EXTRA_LIBS -lsocket -lnsl -lresolv -ldl"
	ARCH=`uname -p` ;;
*-apple-darwin*)
	CFLAGS="$CFLAGS -Ddarwin"
	REENTRANT=""
	case "$host_cpu" in
            powerpc*) ARCH=ppc ;;
            i?86) ARCH=x86 ;;
            arm64) ARCH=aarch64 ;;
            *) ARCH=$host_cpu ;;
        esac ;;
*aix*)
	ARCH=`uname -p`
        if test "$ARCH" = powerpc; then
                ARCH=ppc
        fi
	;;
    aarch64* | arm64*)
        ARCH=aarch64 ;;
    *ppc64* | *powerpc64*)
        ARCH=ppc64 ;;
    *ppc* | *powerpc*)
        ARCH=ppc ;;
*i?86*)
        ARCH=x86 ;;
*x86_64*)
	LIBSUFFIX=64
	ARCH=x86_64 ;;
amd64*)
	ARCH=x86_64 ;;
*)
        ARCH=$host_cpu
	if test "$ARCH" = powerpc; then
		ARCH=ppc
	fi
	;;
esac


dnl Add architecture-specific compile flags
case "$host" in
    alpha*-*-linux*)
        if test x$GCC = xyes; then
            CFLAGS="$CFLAGS -mcpu=ev4 -Wa,-mall"
        fi
        ;;
esac

CFLAGS="$CFLAGS $REENTRANT"

AC_CHECK_FUNCS(getopt, , GETOPT_C="getopt.c")
AC_CHECK_FUNCS(getopt_long, , GETOPT_C="getopt.c")
AC_CHECK_LIB(gnugetopt, getopt_long, LIBS="$LIBS -lgnugetopt"; GETOPT_C="")

AC_SUBST(ARCH)
AC_SUBST(STATIC)
AC_SUBST(SETUPDB_VERSION_MAJOR)
AC_SUBST(SETUPDB_VERSION_MINOR)
AC_SUBST(SETUPDB_VERSION_RELEASE)
AC_SUBST(GETOPT_C)

AC_ARG_WITH([libxml2], AS_HELP_STRING([--without-libxml2], [use legacy libxml 1 instead]), [], [with_libxml2=yes])
AC_PATH_TOOL([PKG_CONFIG], [pkg-config])
if test "$with_libxml2" != no && test -n "$PKG_CONFIG" && "$PKG_CONFIG" --exists libxml-2.0; then
    XML_CFLAGS=`$PKG_CONFIG --cflags libxml-2.0`
    XML_LIBS=`$PKG_CONFIG --libs libxml-2.0`
else
    if test "$with_libxml2" = no; then
        AC_PATH_PROG([XML_CONFIG], [xml-config])
    else
        AC_PATH_PROG([XML2_CONFIG], [xml2-config])
        XML_CONFIG=$XML2_CONFIG
    fi
    AS_IF([test -z "$XML_CONFIG"], [AC_MSG_ERROR([install the libxml development files and pkg-config])])
    XML_CFLAGS=`$XML_CONFIG --cflags`
    XML_LIBS=`$XML_CONFIG --libs`
fi
CPPFLAGS="$CPPFLAGS $XML_CFLAGS"
LIBS="$XML_LIBS $LIBS -lz $EXTRA_LIBS"
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <libxml/parser.h>]], [[xmlParseFile("conftest.xml");]])], [],
    [AC_MSG_ERROR([cannot compile and link with the selected libxml library])])

AC_COMPUTE_INT([SETUPDB_XML_MAJOR], [LIBXML_VERSION / 10000],
    [[#include <libxml/parser.h>]], [AC_MSG_ERROR([cannot determine SetupDB's libxml ABI])])
AC_SUBST([SETUPDB_XML_MAJOR])

AC_ARG_WITH(vendor,
[  --with-vendor           with specified vendor prefix  [default=loki]],
	,   with_vendor=loki)
if test x$with_vendor != xloki; then
        CFLAGS="$CFLAGS -DLOKI_PREFIX=\\\"$with_vendor\\\""
fi

AC_ARG_WITH(image,
[  --with-image            specifiy the image path  [default=../../spp/private/image]],
	,   with_image=../../spp/private/image)
IMAGE="$with_image"


AC_SUBST(IMAGE)
AC_SUBST(BRANDELF)
AS_IF([test "$GCC" = yes], [DEPFLAGS="-MMD -MP"])
AC_SUBST([DEPFLAGS])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
