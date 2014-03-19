AC_PREREQ([2.65])
AC_INIT([j4status-plugins],[0.1],[],[])

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([.aux])

AM_INIT_AUTOMAKE([1.11 subdir-objects dist-xz no-dist-gzip tar-ustar foreign])

AM_SILENT_RULES([yes])

AC_ARG_VAR([XSLTPROC], [The xsltproc executable])

AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_SED
AC_DISABLE_STATIC
LT_INIT
PKG_PROG_PKG_CONFIG
AC_PATH_PROG([XSLTPROC], [xsltproc])
if test -z "$XSLTPROC"; then
    AC_MSG_ERROR([xsltproc not found])
fi

GETTEXT_PACKAGE=$PACKAGE_NAME
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Gettext Package])
AC_SUBST(GETTEXT_PACKAGE)
dnl Not yet useful
dnl IT_PROG_INTLTOOL([0.40.0])

AC_PROG_INSTALL

#
# Directories
#

# package-specific dirs
pkglibdir='${libdir}/'${PACKAGE}
pkgincludedir='${includedir}/'${PACKAGE}
pkgdatadir='${datadir}/'${PACKAGE}
AC_SUBST([pkglibdir])
AC_SUBST([pkgincludedir])
AC_SUBST([pkgdatadir])

# pkg-config dirs
PKG_INSTALLDIR


AM_CFLAGS="-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
AM_CPPFLAGS=""
AM_LDFLAGS="-no-undefined -Wl,--no-undefined"
AM_XSLTPROCFLAGS="--nonet --stringparam man.output.quietly 1 --stringparam funcsynopsis.style ansi"
AM_DOCBOOK_CONDITIONS="installation"


if test -d .git; then
    PACKAGE_VERSION="${PACKAGE_VERSION} - `git describe --tags --always` (`git describe --tags --always --all | sed -e 's:heads/::'`)"
    AC_DEFINE_UNQUOTED([PACKAGE_VERSION], ["$PACKAGE_VERSION"], [Full version with git information])
fi

J4SP_INIT([pluginsdir])

#
# Plugins
#
J4STATUS_PLUGINS_PLUGIN_I3FOCUS
J4STATUS_PLUGINS_PLUGIN_MPRIS


#
# Tests
#
dnl AC_ARG_ENABLE(tests, AS_HELP_STRING([--enable-tests], [Build tests]), [], enable_tests=yes)
dnl AM_CONDITIONAL(ENABLE_TESTS, [test x$enable_tests = xyes])


# Debug flags
AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [Enable debug]), [], enable_debug=no)
if test x$enable_debug = xyes; then
    AC_DEFINE([DEBUG], [1], [Activate debug mode])
fi

AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(AM_LDFLAGS)
AC_SUBST(AM_XSLTPROCFLAGS)
AC_SUBST(AM_DOCBOOK_CONDITIONS)

AM_SUBST_NOTMAKE(LIBS)
AC_SUBST(shrext_cmds)

AC_CONFIG_FILES([
    Makefile
])
AC_OUTPUT

AC_MSG_RESULT([
    ${PACKAGE_NAME} ${PACKAGE_VERSION}
])
J4SP_SUMMARY
AC_MSG_RESULT([
dnl     Tests: $enable_tests
    Debug mode: $enable_debug
])
