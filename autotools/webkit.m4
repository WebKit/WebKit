dnl macros to check for JavaScriptCore and WebKit/Gtk+ dependencies
dnl
dnl The rationale is so that we can share these macros between 
dnl WebKit and JavaScriptCore builds.

# global states
m4_define([initialized], [no])

AC_DEFUN([INIT_C_CXX_FLAGS],
[dnl
# If CXXFLAGS and CFLAGS are unset, default to empty.
# This is to tell automake not to include '-g' if CXXFLAGS is not set
# For more info - http://www.gnu.org/software/automake/manual/autoconf.html#C_002b_002b-Compiler
if test -z "$CXXFLAGS"; then
   CXXFLAGS=""
fi
if test -z "$CFLAGS"; then
   CFLAGS=""
fi
])

AC_DEFUN_ONCE([WEBKIT_INIT],
[dnl
dnl check if we have the required packages to have successful checks
dnl
# Make sure CXXFLAGS and CFLAGS are set before expanding AC_PROG_CXX to avoid
# building with '-g -O2' on Release builds.
AC_REQUIRE([INIT_C_CXX_FLAGS])

# check for -fvisibility=hidden compiler support (GCC >= 4)
saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -fvisibility=hidden -fvisibility-inlines-hidden"
AC_MSG_CHECKING([if ${CXX} supports -fvisibility=hidden -fvisibility-inlines-hidden])
AC_COMPILE_IFELSE([char foo;],
      [ AC_MSG_RESULT([yes])
        SYMBOL_VISIBILITY="-fvisibility=hidden" SYMBOL_VISIBILITY_INLINES="-fvisibility-inlines-hidden" ],
        AC_MSG_RESULT([no]))
CFLAGS="$saved_CFLAGS"
AC_SUBST(SYMBOL_VISIBILITY)
AC_SUBST(SYMBOL_VISIBILITY_INLINES)

# check for pkg-config
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
if test "$PKG_CONFIG" = "no"; then
   AC_MSG_ERROR([Cannot find pkg-config, make sure it is installed in your PATH])
fi

AC_PATH_PROG(PERL, perl)
if test -z "$PERL"; then
   AC_MSG_ERROR([You need 'perl' to compile WebKit])
fi

AC_PATH_PROG(BISON, bison)
if test -z "$BISON"; then
   AC_MSG_ERROR([You need the 'bison' parser generator to compile WebKit])
fi

AC_PATH_PROG(MV, mv)
if test -z "$MV"; then
   AC_MSG_ERROR([You need 'mv' to compile WebKit])
fi

AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AC_PROG_CXX])
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_SYS_LARGEFILE

# Check whether a C++ was found (AC_PROG_CXX sets $CXX to "g++" even when it
# doesn't exist)
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],[],[AC_MSG_ERROR([No C++ compiler found])])
AC_LANG_POP([C++])

# C/C++ Language Features
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE

# C/C++ Headers
AC_REQUIRE([AC_HEADER_STDC])
AC_HEADER_STDBOOL

m4_define([initialized], [yes])
])

AC_DEFUN_ONCE([WEBKIT_CHECK_DEPENDENCIES],
[dnl
dnl check for module dependencies
for module in $1
do
    case $module in
        glib) _WEBKIT_CHECK_GLIB ;;
        unicode) _WEBKIT_CHECK_UNICODE ;;
        *) AC_MSG_ERROR([I don't support that module. Sorry..]) ;;

    esac
done
])

AC_DEFUN_ONCE([_WEBKIT_CHECK_GLIB],
[dnl
dnl check for glib
# Version requirements
GLIB_REQUIRED_VERSION=2.21.3
GOBJECT_REQUIRED_VERSION=2.0
GTHREAD_REQUIRED_VERSION=2.0

PKG_CHECK_MODULES([GLIB],
                  [glib-2.0 >= $GLIB_REQUIRED_VERSION
                  gobject-2.0 >= $GOBJECT_REQUIRED_VERSION
                  gthread-2.0 >= $GTHREAD_REQUIRED_VERSION])
AC_SUBST([GLIB_CFLAGS])
AC_SUBST([GLIB_LIBS])

# GTK+ port only
# Check for glib-genmarshal and glib-mkenums
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])
if test -z "$GLIB_GENMARSHAL" || test -z "$GLIB_MKENUMS"; then
   AC_MSG_ERROR([You need the GLib dev tools in your path])
fi
])

AC_DEFUN_ONCE([_WEBKIT_CHECK_UNICODE],
[dnl
dnl determine the Unicode backend
AC_MSG_CHECKING([which Unicode backend to use])
AC_ARG_WITH(unicode_backend,
            AC_HELP_STRING([--with-unicode-backend=@<:@icu/glib@:>@],
                           [Select Unicode backend (WARNING: the glib-based backend is slow, and incomplete) [default=icu]]),
            [],[with_unicode_backend="icu"])

case "$with_unicode_backend" in
     icu|glib) ;;
     *) AC_MSG_ERROR([Invalid Unicode backend: must be icu or glib.]) ;;
esac

AC_MSG_RESULT([$with_unicode_backend])

# https://bugs.webkit.org/show_bug.cgi?id=15914
# Splitting ICU removal patch into smaller portions. We compile a hybrid version
# with the WTF Unicode backend being based on GLib while text codecs and TextBreakIterator
# keep the ICU dependency. That's why we temporarily add icu headers and libs for glib config case as well.
if test "$with_unicode_backend" = "icu" -o "$with_unicode_backend" = "glib"; then
	if test "$os_darwin" = "yes"; then
		UNICODE_CFLAGS="-I\$(srcdir)/JavaScriptCore/icu -I\$(srcdir)/WebCore/icu"
		UNICODE_LIBS="-licucore"
	elif test "$os_win32" = "yes"; then
		UNICODE_CFLAGS=""
		UNICODE_LIBS="-licuin -licuuc"
	else
		AC_PATH_PROG(icu_config, icu-config, no)
		if test "$icu_config" = "no"; then
			AC_MSG_ERROR([Cannot find icu-config. The ICU library is needed.])
		fi

		# We don't use --cflags as this gives us a lot of things that we don't
		# necessarily want, like debugging and optimization flags
		# See man (1) icu-config for more info.
		UNICODE_CFLAGS=`$icu_config --cppflags`
		UNICODE_LIBS=`$icu_config --ldflags-libsonly`
	fi
fi

if test "$with_unicode_backend" = "glib"; then
	# https://bugs.webkit.org/show_bug.cgi?id=15914
	# Splitting ICU removal patch into smaller portions, that's why we
	# temporarily retrieve flags & libs info for glib into UNICODEGLIB
	# instead of UNICODE variable, then concatenate.
	# Patch 3/4 of the above issue will rename the variable back to UNICODE.
	PKG_CHECK_MODULES([UNICODEGLIB], [glib-2.0 pango >= 1.21.0])
	UNICODE_CFLAGS="$UNICODE_CFLAGS $UNICODEGLIB_CFLAGS"
	UNICODE_LIBS="$UNICODE_LIBS $UNICODEGLIB_LIBS"
fi

AC_SUBST([UNICODE_CFLAGS])
AC_SUBST([UNICODE_LIBS])
])
