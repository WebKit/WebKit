AC_INIT([WebKit],[0.1],[http://bugs.webkit.org/])

AC_PREREQ(2.59)

AC_CONFIG_HEADERS([aconfig.h])
AC_CONFIG_SRCDIR([WebCore/config.h])

# see http://www.gnu.org/software/libtool/manual.html#Versioning
LIBWEBKITGTK_VERSION=1:0:0
AC_SUBST([LIBWEBKITGTK_VERSION])

AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CANONICAL_HOST

# host checking - inspired by the GTK+ configure.in
# TODO: os_mac, os_bsd
AC_MSG_CHECKING([for native Win32])
case "$host" in
     *-*-mingw*)
       os_win32=yes
       ;;
     *)
       os_win32=no
       ;;
esac
AC_MSG_RESULT([$os_win32])

case "$host" in
     *-*-linux*)
       os_linux=yes
       ;;
     *-*-freebsd*)
       os_freebsd=yes
       ;;
     *-*-darwin*)
       os_darwin=yes
       ;;
esac

# If CXXFLAGS and CFLAGS are unset, default to -O2
# This is to tell automake not to include '-g' if CXXFLAGS is not set
# For more info - http://www.gnu.org/software/automake/manual/autoconf.html#C_002b_002b-Compiler
if test -z "$CXXFLAGS"; then
   CXXFLAGS="-O2"
fi
if test -z "$CFLAGS"; then
   CFLAGS="-O2"
fi

# programs
AC_DISABLE_STATIC
AM_PROG_LIBTOOL
AC_PROG_INSTALL
AM_PROG_CC_STDC
AC_PROG_CXX
AM_PROG_CC_C_O

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

AC_PATH_PROG(PERL, perl)
if test -z "$PERL"; then
   AC_MSG_ERROR([You need 'perl' to compile WebKit])
fi

AC_PATH_PROG(BISON, bison)
if test -z "$BISON"; then
   AC_MSG_ERROR([You need the 'bison' parser generator to compile WebKit])
fi

AC_PATH_PROG(FLEX, flex)
if test -z "$FLEX"; then
   AC_MSG_ERROR([You need the 'flex' lexer generator to compile WebKit])
fi

AC_PATH_PROG(GPERF, gperf)
if test -z "$GPERF"; then
   AC_MSG_ERROR([You need the 'gperf' hash function generator to compile WebKit])
fi

AC_PATH_PROG(MV, mv)
if test -z "$MV"; then
   AC_MSG_ERROR([You need 'mv' to compile WebKit])
fi

# GTK+ port only
# Check for glib-genmarshal and glib-mkenums
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

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
AC_HEADER_STDC
AC_HEADER_STDBOOL

# pthread (not needed on Windows)
if test "$os_win32" = "no"; then
AC_CHECK_HEADERS([pthread.h],
                 AC_DEFINE([HAVE_PTHREAD_H],[1],[Define if pthread exist]),
                 AC_MSG_ERROR([pthread support is required to build WebKit]))
fi

# libjpeg headers
AC_CHECK_HEADERS([jpeglib.h])

# check for pkg-config
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
if test "$PKG_CONFIG" = "no"; then
   AC_MSG_ERROR([Cannot find pkg-config, make sure it is installed in your PATH])
fi

# check for icu-config
if test "$os_darwin" = "yes"; then
ICU_CPPFLAGS="-I\$(srcdir)/JavaScriptCore/icu -I\$(srcdir)/WebCore/icu"
ICU_LIBS="-licucore"
else
AC_PATH_PROG(icu_config, icu-config, no)
if test "$icu_config" = "no"; then
   AC_MSG_ERROR([Cannot find icu-config. ICU library is needed.])
fi

# We don't use --cflags as this gives us a lot of things that we don't
# necessarily want, like debugging and optimization flags
# See man (1) icu-config for more info.
ICU_CPPFLAGS=`$icu_config --cppflags`
ICU_LIBS=`$icu_config --ldflags`
AC_SUBST([ICU_CPPFLAGS])
AC_SUBST([ICU_LIBS])
fi

# determine the GDK/GTK+ target
AC_MSG_CHECKING([the target windowing system])
AC_ARG_WITH(target,
            AC_HELP_STRING([--with-target=@<:@x11/win32/quartz/directfb@:>@],
                           [Select webkit target [default=x11]]),
            [],[with_target="x11"])

case "$with_target" in
     x11|win32|quartz|directfb) ;;
     *) AC_MSG_ERROR([Invalid target: must be x11, quartz, win32, or directfb.]) ;;
esac

AC_MSG_RESULT([$with_target])

AC_MSG_CHECKING([for Hildon UI extensions])
AC_ARG_WITH(hildon,
            AC_HELP_STRING([--with-hildon],
                           [Use Hildon UI extensions [default=no]]),
            [],[with_hildon="no"])
AC_MSG_RESULT([$with_hildon])

if test "$with_hildon" = "yes"; then
    HILDON_CPPFLAGS="-DMAEMO_CHANGES"
    PKG_CHECK_MODULES([HILDON], [hildon-1])
    AC_SUBST([HILDON_CPPFLAGS])
    AC_SUBST([HILDON_CFLAGS])
    AC_SUBST([HILDON_LIBS])
fi

# determine the http backend
AC_MSG_CHECKING([the HTTP backend to use])
AC_ARG_WITH(http_backend,
            AC_HELP_STRING([--with-http-backend=@<:@curl@:>@],
                           [Select HTTP backend [default=curl]]),
            [],[http_backend="curl"])

case "$http_backend" in
     curl) ;;
     *) AC_MSG_ERROR([Invalid HTTP backend: must be curl.]) ;;
esac

AC_MSG_RESULT([$http_backend])

# minimum base dependencies
CAIRO_REQUIRED_VERSION=1.4
FONTCONFIG_REQUIRED_VERSION=2.4
FREETYPE2_REQUIRED_VERSION=9.0
LIBXML_REQUIRED_VERSION=2.6

# minimum GTK+ base dependencies
GLIB_REQUIRED_VERSION=2.0
GOBJECT_REQUIRED_VERSION=2.0
GTHREAD_REQUIRED_VERSION=2.0
PANGO_REQUIRED_VERSION=1.0
GTK_REQUIRED_VERSION=2.8

# optional modules
LIBCURL_REQUIRED_VERSION=7.15
LIBXSLT_REQUIRED_VERSION=1.1.7
SQLITE_REQUIRED_VERSION=3.0
GSTREAMER_REQUIRED_VERSION=0.10
GNOME_VFS_REQUIRED_VERSION=2.0

PKG_CHECK_MODULES([GLOBALDEPS],
                  [glib-2.0 >= $GLIB_REQUIRED_VERSION
                  gobject-2.0 >= $GOBJECT_REQUIRED_VERSION
                  gthread-2.0 >= $GTHREAD_REQUIRED_VERSION])
AC_SUBST([GLOBALDEPS_CFLAGS])
AC_SUBST([GLOBALDEPS_LIBS])

PKG_CHECK_MODULES([WEBKITDEPS],
                  [gtk+-2.0 >= $GTK_REQUIRED_VERSION
                  pango >= $PANGO_REQUIRED_VERSION
                  cairo >= $CAIRO_REQUIRED_VERSION
                  cairo-ft,
                  fontconfig >= $FONTCONFIG_REQUIRED_VERSION
                  freetype2 >= $FREETYPE2_REQUIRED_VERSION
                  libxml-2.0 >= $LIBXML_REQUIRED_VERSION])
AC_SUBST([WEBKITDEPS_CFLAGS])
AC_SUBST([WEBKITDEPS_LIBS])

# check whether to build with debugging enabled
AC_MSG_CHECKING([whether to do a debug build])
AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug],
                             [turn on debugging [default=no]]),
              [],[enable_debug="no"])
AC_MSG_RESULT([$enable_debug])

# check whether to build with cross-document messaging support
AC_MSG_CHECKING([whether to enable HTML5 cross-document messaging support])
AC_ARG_ENABLE(cross_document_messaging,
              AC_HELP_STRING([--enable-cross-document-messaging],
                             [enable HTML5 cross-document messaging support [default=yes]]),
              [],[enable_cross_document_messaging="yes"])
AC_MSG_RESULT([$enable_cross_document_messaging])

# check whether to build with database support
AC_MSG_CHECKING([whether to enable HTML5 client-side storage support])
AC_ARG_ENABLE(database,
              AC_HELP_STRING([--enable-database],
                             [enable HTML5 client-side storage support [default=yes]]),
              [],[enable_database="yes"])
AC_MSG_RESULT([$enable_database])

# check whether to build with icon database support
AC_MSG_CHECKING([whether to enable icon database support])
AC_ARG_ENABLE(icon_database,
              AC_HELP_STRING([--enable-icon-database],
                             [enable icon database [default=no]]),
              [],[enable_icon_database="no"])
AC_MSG_RESULT([$enable_icon_database])

# check whether to enable HTML5 audio/video support
AC_MSG_CHECKING([whether to enable HTML5 video support])
AC_ARG_ENABLE(video,
              AC_HELP_STRING([--enable-video],
                             [enable HTML5 video support [default=no]]),
              [],[enable_video="no"])
AC_MSG_RESULT([$enable_video])

# check whether to enable XPath support
AC_MSG_CHECKING([whether to enable XPath support])
AC_ARG_ENABLE(xpath,
              AC_HELP_STRING([--enable-xpath],
                             [enable support for XPath [default=yes]]),
              [],[enable_xpath="yes"])
AC_MSG_RESULT([$enable_xpath])

# check whether to enable XSLT support
AC_MSG_CHECKING([whether to enable XSLT support])
AC_ARG_ENABLE(xslt,
              AC_HELP_STRING([--enable-xslt],
                             [enable support for XSLT [default=yes]]),
              [],[enable_xslt="yes"])
AC_MSG_RESULT([$enable_xslt])

# check whether to enable SVG experimental features
# Enable all SVG if it is
AC_MSG_CHECKING([whether to enable SVG experimental features])
AC_ARG_ENABLE(svg_experimental,
              AC_HELP_STRING([--enable-svg-experimental],
                             [enable support for SVG experimental features [default=no]]),
              [],[enable_svg_experimental="no"])
AC_MSG_RESULT([$enable_svg_experimental])

if test "$enable_svg_experimental" = "yes"; then
   enable_svg=yes
   enable_svg_animation=yes
#   enable_svg_filters=yes
   enable_svg_fonts=yes
   enable_svg_foreign_object=yes
   enable_svg_as_image=yes
   enable_svg_use_element=yes
fi

# check whether to enable SVG support
AC_MSG_CHECKING([whether to enable SVG support])
AC_ARG_ENABLE(svg,
              AC_HELP_STRING([--enable-svg],
                             [enable support for SVG [default=no]]),
              [],[enable_svg="no"])
AC_MSG_RESULT([$enable_svg])

# check whether to enable support for SVG animation
AC_MSG_CHECKING([whether to enable support for SVG animation])
AC_ARG_ENABLE(svg_animation,
              AC_HELP_STRING([--enable-svg-animation],
                             [enable support for SVG animation (experimental) [default=no]]),
              [],[enable_svg_animation="no"])
AC_MSG_RESULT([$enable_svg_animation])

# check whether to enable support for SVG filters
AC_MSG_CHECKING([whether to enable support for SVG filters])
AC_ARG_ENABLE(svg_filters,
              AC_HELP_STRING([--enable-svg-filters],
                             [enable support for SVG filters (experimental) [default=no]]),
              [],[enable_svg_filters="no"])
AC_MSG_RESULT([$enable_svg_filters])

# check whether to enable support for SVG fonts
AC_MSG_CHECKING([whether to enable support for SVG fonts])
AC_ARG_ENABLE(svg_fonts,
              AC_HELP_STRING([--enable-svg-fonts],
                             [enable support for SVG fonts (experimental) [default=no]]),
              [],[enable_svg_fonts="no"])
AC_MSG_RESULT([$enable_svg_fonts])

# check whether to enable foreign objects support for SVG
AC_MSG_CHECKING([whether to enable support for SVG foreign objects])
AC_ARG_ENABLE(svg_foreign_object,
              AC_HELP_STRING([--enable-svg-foreign-object],
                             [enable support for SVG foreign objects (experimental) [default=no]]),
              [],[enable_svg_foreign_object="no"])
AC_MSG_RESULT([$enable_svg_foreign_object])

# check whether to enable SVG As Image support
AC_MSG_CHECKING([whether to enable SVG as Image support])
AC_ARG_ENABLE(svg_as_image,
              AC_HELP_STRING([--enable-svg-as-image],
                             [enable SVG as Image support (experimental) [default=no]]),
              [],[enable_svg_as_image="no"])
AC_MSG_RESULT([$enable_svg_as_image])

# check whether to enable SVG USE element support
AC_MSG_CHECKING([whether to enable support for SVG use element])
AC_ARG_ENABLE(svg_use_element,
              AC_HELP_STRING([--enable-svg-use-element],
                             [enable SVG use element support (experimental) [default=no]]),
              [],[enable_svg_use_element="no"])
AC_MSG_RESULT([$enable_svg_use_element])

# check whether to enable code coverage
AC_MSG_CHECKING([whether to enable code coverage support])
AC_ARG_ENABLE(coverage,
              AC_HELP_STRING([--enable-coverage],
                             [enable code coverage support [default=no]]),
              [],[enable_coverage="no"])
AC_MSG_RESULT([$enable_coverage])

# check whether to enable FastMalloc
AC_MSG_CHECKING([whether to enable optimized memory allocator])
AC_ARG_ENABLE(fast_malloc,
              AC_HELP_STRING([--enable-fast-malloc],
                             [enable optimized memory allocator default=yes, default=no for debug builds]),
              [],[if test "$enable_debug" = "yes"; then enable_fast_malloc="no"; else enable_fast_malloc="yes"; fi])
AC_MSG_RESULT([$enable_fast_malloc])

# Add '-g' flag to gcc if it's debug build
if test "$enable_debug" = "yes"; then
   CXXFLAGS="$CXXFLAGS -g"
   CFLAGS="$CFLAGS -g"
fi

# check if curl is available
if test "$http_backend" = "curl"; then
   PKG_CHECK_MODULES([LIBCURL],
                     [libcurl >= $LIBCURL_REQUIRED_VERSION])
   AC_SUBST([LIBCURL_CFLAGS])
   AC_SUBST([LIBCURL_LIBS])
fi

# check if sqlite 3 is available
if test "$enable_icon_database" = "yes" || test "$enable_database" = "yes"; then
   PKG_CHECK_MODULES([SQLITE3], [sqlite3 >= $SQLITE_REQUIRED_VERSION])
   AC_SUBST([SQLITE3_CFLAGS])
   AC_SUBST([SQLITE3_LIBS])
fi

# check if libxslt is available
if test "$enable_xslt" = "yes"; then
   PKG_CHECK_MODULES([LIBXSLT],[libxslt >= $LIBXSLT_REQUIRED_VERSION])
   AC_SUBST([LIBXSLT_CFLAGS])
   AC_SUBST([LIBXSLT_LIBS])
fi

# check if gstreamer is available
if test "$enable_video" = "yes"; then
   PKG_CHECK_MODULES([GSTREAMER],
                     [gstreamer-0.10 >= $GSTREAMER_REQUIRED_VERSION
                     gstreamer-base-0.10,
                     gstreamer-plugins-base-0.10,
                     gnome-vfs-2.0 >= $GNOME_VFS_REQUIRED_VERSION])
   AC_SUBST([GSTREAMER_CFLAGS])
   AC_SUBST([GSTREAMER_LIBS])
fi

# check for code coverage support
if test "$enable_coverage" = "yes"; then
   COVERAGE_CFLAGS="-MD"
   COVERAGE_LDFLAGS="-ftest-coverage -fprofile-arcs"
   AC_SUBST([COVERAGE_CFLAGS])
   AC_SUBST([COVERAGE_LDFLAGS])
fi

# check for SVG features, enabling SVG if necessary
if test "$enable_svg_animation" = "yes" || \
   test "$enable_svg_filters" = "yes" || \
   test "$enable_svg_fonts" = "yes" || \
   test "$enable_svg_foreign_object" = "yes" || \
   test "$enable_svg_as_image" = "yes" || \
   test "$enable_svg_use_element" = "yes"; then
   svg_flags=yes
   if test "$enable_svg" = "no"; then
       AC_MSG_WARN([SVG feature(s) requested but SVG is disabled.. Enabling SVG support])
       enable_svg=yes
   fi
fi

# OS conditionals
AM_CONDITIONAL([OS_WIN32],[test "$os_win32" = "yes"])
AM_CONDITIONAL([OS_UNIX],[test "$os_win32" = "no"])
AM_CONDITIONAL([OS_LINUX],[test "$os_linux" = "yes"])
AM_CONDITIONAL([OS_FREEBSD],[test "$os_freebsd" = "yes"])

# target conditionals
AM_CONDITIONAL([TARGET_X11], [test "$with_target" = "x11"])
AM_CONDITIONAL([TARGET_WIN32], [test "$with_target" = "win32"])
AM_CONDITIONAL([TARGET_QUARTZ], [test "$with_target" = "quartz"])
AM_CONDITIONAL([TARGET_DIRECTFB], [test "$with_target" = "directfb"])

# HTTP backend conditionals
AM_CONDITIONAL([USE_CURL], [test "$http_backend" = "curl"])

# WebKit feature conditionals
AM_CONDITIONAL([ENABLE_DEBUG],[test "$enable_debug" = "yes"])
AM_CONDITIONAL([ENABLE_CROSS_DOCUMENT_MESSAGING],[test "$enable_cross_document_messaging" = "yes"])
AM_CONDITIONAL([ENABLE_DATABASE],[test "$enable_database" = "yes"])
AM_CONDITIONAL([ENABLE_ICONDATABASE],[test "$enable_icon_database" = "yes"])
AM_CONDITIONAL([ENABLE_XPATH],[test "$enable_xpath" = "yes"])
AM_CONDITIONAL([ENABLE_XSLT],[test "$enable_xslt" = "yes"])
AM_CONDITIONAL([ENABLE_VIDEO],[test "$enable_video" = "yes"])
AM_CONDITIONAL([ENABLE_SVG],[test "$enable_svg" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_ANIMATION],[test "$enable_svg_animation" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_FILTERS],[test "$enable_svg_filters" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_FONTS],[test "$enable_svg_fonts" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_FOREIGN_OBJECT],[test "$enable_svg_foreign_object" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_AS_IMAGE],[test "$enable_svg_as_image" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_USE],[test "$enable_svg_use_element" = "yes"])
AM_CONDITIONAL([ENABLE_COVERAGE],[test "$enable_coverage" = "yes"])
AM_CONDITIONAL([ENABLE_FAST_MALLOC],[test "$enable_fast_malloc" = "yes"])
AM_CONDITIONAL([SVG_FLAGS],[test "$svg_flags" = "yes"])

AC_CONFIG_FILES([
GNUmakefile
WebKit/gtk/webkit-1.0.pc:WebKit/gtk/webkit.pc.in
]
)

AC_OUTPUT

echo "
WebKit was configured with the following options:

Build configuration:
 Enable debugging (slow)           : $enable_debug
 Code coverage support             : $enable_coverage
 HTTP backend                      : $http_backend
 Optimized memory allocator        : $enable_fast_malloc
Features:
 HTML5 cross-document messaging    : $enable_cross_document_messaging
 HTML5 client-side storage support : $enable_database
 HTML5 video element support       : $enable_video
 Icon database support             : $enable_icon_database
 SVG support                       : $enable_svg
 SVG animation support             : $enable_svg_animation
 SVG filters support               : $enable_svg_filters
 SVG fonts support                 : $enable_svg_fonts
 SVG foreign object support        : $enable_svg_foreign_object
 SVG as image support              : $enable_svg_as_image
 SVG use element support           : $enable_svg_use_element
 XPATH support                     : $enable_xpath
 XSLT support                      : $enable_xslt
GTK+ configuration:
 GDK target                        : $with_target
 Hildon UI extensions              : $with_hildon
"
