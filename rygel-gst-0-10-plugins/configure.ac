AC_PREREQ([2.64])
AC_INIT([rygel-gst-0-10-plugins],
	[0.20.0],
	[http://bugzilla.gnome.org/enter_bug.cgi?product=Rygel],
	[rygel-gst-0-10-plugins],
	[http://live.gnome.org/Rygel])
AC_CONFIG_AUX_DIR([build-aux])

AC_CONFIG_SRCDIR([src/media-export/rygel-media-export-plugin.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_SEARCH_LIBS([strerror],[cposix])
AC_PROG_CC
AC_HEADER_STDC
AC_FUNC_MMAP

AC_SUBST(VERSION)

dnl Initialize automake
AM_INIT_AUTOMAKE([1.11.1 tar-ustar no-dist-gzip dist-xz -Wno-portability])
AM_MAINTAINER_MODE([enable])

dnl Initialize libtool
dnl Disable generation of static libraries
LT_PREREQ([2.2.6])
LT_INIT([dlopen disable-static])

dnl Required versions of library packages
LIBRYGEL_SERVER_REQUIRED=0.17.4
GUPNP_REQUIRED=0.19.0
GUPNP_AV_REQUIRED=0.11.2
GUPNP_DLNA_REQUIRED=0.9.5
GSTREAMER_REQUIRED=0.10.36
GSTPBU_REQUIRED=0.10.35
GIO_REQUIRED=2.26
GEE_REQUIRED=0.8.0
UUID_REQUIRED=1.41.3

dnl Additional requirements for media-export plugin
GSTREAMER_TAG_REQUIRED=0.10.28
GSTREAMER_APP_REQUIRED=0.10.28
LIBSQLITE3_REQUIRED=3.5


RYGEL_BASE_MODULES="gupnp-1.0 >= $GUPNP_REQUIRED gee-0.8 >= $GEE_REQUIRED"
RYGEL_COMMON_MODULES="$RYGEL_BASE_MODULES gupnp-av-1.0 >= $GUPNP_AV_REQUIRED"
PKG_CHECK_MODULES([DEPS], [$RYGEL_COMMON_MODULES rygel-server-2.2 >= $LIBRYGEL_SERVER_REQUIRED gio-2.0 >= $GIO_REQUIRED gstreamer-0.10 >= $GSTREAMER_REQUIRED gstreamer-pbutils-0.10 >= $GSTPBU_REQUIRED gupnp-dlna-2.0 >= $GUPNP_DLNA_REQUIRED gupnp-dlna-gst-legacy-2.0 >= $GUPNP_DLNA_REQUIRED gstreamer-tag-0.10 >= $GSTREAMER_TAG_REQUIRED gstreamer-app-0.10 >= $GSTREAMER_TAG_REQUIRED sqlite3 >= $LIBSQLITE3_REQUIRED uuid >= $UUID_REQUIRED])

AC_CHECK_HEADER([unistr.h],
                AC_CHECK_LIB([unistring],
                             [u8_strcoll],
                             [have_unistring=yes],[have_unistring=no]))
if test "x$have_unistring" = "xyes"; then
    AC_DEFINE([HAVE_UNISTRING],[1],[Use libunistring for collation])
    COLLATION_CFLAGS=
    COLLATION_LIBS=-lunistring
    AC_SUBST([COLLATION_CFLAGS])
    AC_SUBST([COLLATION_LIBS])
fi


dnl Strict compiler
AC_ARG_ENABLE(strict-cc,
	AS_HELP_STRING([--enable-strict-cc],[enable strict C compiler]),,
        enable_strict_cc=no)
if test "x$enable_strict_cc" = "xyes"; then
        CFLAGS="$CFLAGS -Wall -Werror"
fi


dnl Gettext
GETTEXT_PACKAGE=rygel-gst-0-10-plugins
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], 
  [Define the gettext package to be used])

AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.40.0])

dnl Shave the output
AM_SILENT_RULES([yes])

PKG_PROG_PKG_CONFIG

RYGEL_PLUGIN_LINKER_FLAGS="`$PKG_CONFIG --variable=plugin_ldflags rygel-core-2.2`"
AC_SUBST(RYGEL_PLUGIN_LINKER_FLAGS)

RYGEL_VERSION="`$PKG_CONFIG --modversion rygel-core-2.2`"
AC_SUBST(RYGEL_VERSION)

if test "x$RYGEL_PLUGIN_LINKER_FLAGS" = 'x'
then
	AC_MSG_ERROR([Could not get Rygel plugin linker flags])
fi

RYGEL_PLUGIN_DIR="`$PKG_CONFIG --variable=plugindir rygel-core-2.2`"
AC_SUBST(RYGEL_PLUGIN_DIR)

if test "x$RYGEL_PLUGIN_DIR" = 'x'
then
	AC_MSG_ERROR([Could not get Rygel plugin installation directory])
fi

AC_CONFIG_FILES([
Makefile
data/Makefile
src/Makefile
src/media-export/Makefile
src/media-export/media-export-gst-0-10.plugin
tests/Makefile
po/Makefile.in
])
AC_OUTPUT

echo "

                    ${PACKAGE} ${VERSION}
                    ============


        Prefix:                 ${prefix}
        Source code location:   ${srcdir}
        Compiler:               ${CC}
        CFLAGS:                 ${CFLAGS}
"
