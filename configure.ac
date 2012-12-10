AC_PREREQ(2.60)

m4_define([webkit_major_version], [1])
m4_define([webkit_minor_version], [11])
m4_define([webkit_micro_version], [2])

# This is the version we'll be using as part of our User-Agent string
# e.g., AppleWebKit/$(webkit_user_agent_version) ...
#
# Sourced from Source/WebCore/Configurations/Version.xcconfig
m4_define([webkit_user_agent_major_version], [537])
m4_define([webkit_user_agent_minor_version], [20])

AC_INIT([WebKitGTK],[webkit_major_version.webkit_minor_version.webkit_micro_version],[http://bugs.webkit.org/])

AC_CONFIG_MACRO_DIR([Source/autotools])
AC_CONFIG_AUX_DIR([Source/autotools])
AC_SUBST(ACLOCAL_AMFLAGS, "-I Source/autotools")

AC_CONFIG_HEADERS([autotoolsconfig.h])
AC_CANONICAL_HOST

WEBKIT_MAJOR_VERSION=webkit_major_version
WEBKIT_MINOR_VERSION=webkit_minor_version
WEBKIT_MICRO_VERSION=webkit_micro_version
WEBKIT_USER_AGENT_MAJOR_VERSION=webkit_user_agent_major_version
WEBKIT_USER_AGENT_MINOR_VERSION=webkit_user_agent_minor_version
AC_SUBST(WEBKIT_MAJOR_VERSION)
AC_SUBST(WEBKIT_MINOR_VERSION)
AC_SUBST(WEBKIT_MICRO_VERSION)
AC_SUBST(WEBKIT_USER_AGENT_MAJOR_VERSION)
AC_SUBST(WEBKIT_USER_AGENT_MINOR_VERSION)

AC_CONFIG_SRCDIR([Source/WebCore/config.h])

dnl # Libtool library version, not to confuse with API version
dnl # see http://www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html
LIBWEBKITGTK_VERSION=17:1:17
AC_SUBST([LIBWEBKITGTK_VERSION])

LIBJAVASCRIPTCOREGTK_VERSION=13:7:13
AC_SUBST([LIBJAVASCRIPTCOREGTK_VERSION])

LIBWEBKIT2GTK_VERSION=19:0:19
AC_SUBST([LIBWEBKIT2GTK_VERSION])

AM_INIT_AUTOMAKE([foreign subdir-objects dist-xz no-dist-gzip tar-ustar])

# Use AM_SILENT_RULES if present
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# host checking - inspired by the GTK+ configure.in
# TODO: move these to webkit.m4?
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

case "$host_os" in
     gnu* | linux* | k*bsd*-gnu)
       os_gnu=yes
       ;;
     *)
       os_gnu=no
       ;;
esac

# If CFLAGS and CXXFLAGS are unset, default to empty.
# This is to tell automake not to include '-g' if C{XX,}FLAGS is not set
# For more info - http://www.gnu.org/software/automake/manual/autoconf.html#C_002b_002b-Compiler
if test -z "$CXXFLAGS"; then
   CXXFLAGS=""
fi
if test -z "$CFLAGS"; then
   CFLAGS=""
fi

PKG_PROG_PKG_CONFIG

AC_PATH_PROG(PERL, perl)
if test -z "$PERL"; then
   AC_MSG_ERROR([You need 'perl' to compile WebKit])
fi

AC_PATH_PROG(PYTHON, python)
if test -z "$PYTHON"; then
   AC_MSG_ERROR([You need 'python' to compile WebKit])
fi

AC_PATH_PROG(RUBY, ruby)
if test -z "$RUBY"; then
   AC_MSG_ERROR([You need 'ruby' to compile WebKit])
fi

AC_PATH_PROG(BISON, bison)
if test -z "$BISON"; then
   AC_MSG_ERROR([You need the 'bison' parser generator to compile WebKit])
fi

AC_PATH_PROG(MV, mv)
if test -z "$MV"; then
   AC_MSG_ERROR([You need 'mv' to compile WebKit])
fi

AC_PROG_CC
AC_PROG_CXX
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
AC_HEADER_STDC
AC_HEADER_STDBOOL

# Linker
AC_DISABLE_STATIC
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
# use dolt to speedup the build
DOLT

AC_PATH_PROG(FLEX, flex)
if test -z "$FLEX"; then
   AC_MSG_ERROR([You need the 'flex' lexer generator to compile WebKit])
else
   FLEX_VERSION=`$FLEX --version | sed 's,.*\ \([0-9]*\.[0-9]*\.[0-9]*\)$,\1,'`
   AX_COMPARE_VERSION([2.5.33],[gt],[$FLEX_VERSION],
      AC_MSG_WARN([You need at least version 2.5.33 of the 'flex' lexer generator to compile WebKit correctly]))
fi

AC_PATH_PROG(GPERF, gperf)
if test -z "$GPERF"; then
   AC_MSG_ERROR([You need the 'gperf' hash function generator to compile WebKit])
fi

# check for -fvisibility=hidden compiler support (GCC >= 4)
saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -fvisibility=hidden -fvisibility-inlines-hidden"
AC_MSG_CHECKING([if ${CXX} supports -fvisibility=hidden -fvisibility-inlines-hidden])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([char foo;])],
      [ AC_MSG_RESULT([yes])
        SYMBOL_VISIBILITY="-fvisibility=hidden" SYMBOL_VISIBILITY_INLINES="-fvisibility-inlines-hidden" ],
        AC_MSG_RESULT([no]))
CFLAGS="$saved_CFLAGS"
AC_SUBST(SYMBOL_VISIBILITY)
AC_SUBST(SYMBOL_VISIBILITY_INLINES)

# Disable C++0x compat warnings for GCC >= 4.6.0 until we build
# cleanly with that.
AC_LANG_PUSH(C++)
TMPCXXFLAGS=$CXXFLAGS
CXXFLAGS="-Wall -Werror"
AC_MSG_CHECKING([if we have to disable C++0x compat warnings for GCC >= 4.6.0])
AC_TRY_COMPILE([
namespace std {
    class nullptr_t { };
}
extern std::nullptr_t nullptr;
], [return 0;],
disable_cxx0x_compat=no,
disable_cxx0x_compat=yes)
AC_MSG_RESULT($disable_cxx0x_compat)
if test "$disable_cxx0x_compat" = yes; then
  CXXFLAGS="$TMPCXXFLAGS -Wno-c++0x-compat"
else
  CXXFLAGS="$TMPCXXFLAGS"
fi
AC_LANG_POP(C++)

# pthread (not needed on Windows)
if test "$os_win32" = "no"; then
AC_CHECK_HEADERS([pthread.h],
                 AC_DEFINE([HAVE_PTHREAD_H],[1],[Define if pthread exists]),
                 AC_MSG_ERROR([pthread support is required to build WebKit]))
AC_CHECK_LIB(pthread, pthread_rwlock_init,
                 AC_DEFINE([HAVE_PTHREAD_RWLOCK],[1],[Define if pthread rwlock is present]),
                 AC_MSG_WARN([pthread rwlock support is not available]))
fi

# check for libjpeg the way Gtk does it.
AC_CHECK_LIB(jpeg, jpeg_destroy_decompress,
                   jpeg_ok=yes, jpeg_ok=no
                   AC_MSG_ERROR([JPEG library (libjpeg) not found]))
if test "$jpeg_ok" = yes; then
   AC_MSG_CHECKING([for jpeglib])
   AC_TRY_CPP(
[#include <stdio.h>
#undef PACKAGE
#undef VERSION
#undef HAVE_STDLIB_H
#include <jpeglib.h>],
         jpeg_ok=yes,
         jpeg_ok=no)
   AC_MSG_RESULT($jpeg_ok)
   if test "$jpeg_ok" = yes; then
      JPEG_LIBS="-ljpeg"
      # should we check for progressive JPEG like GTK+ as well?
   else
      AC_MSG_ERROR([JPEG library (libjpeg) not found])
   fi
fi
AC_SUBST([JPEG_LIBS])

# Check for libpng the way Gtk+ does it
for l in libpng libpng14 libpng12; do
  AC_MSG_CHECKING(for $l)
  if $PKG_CONFIG --exists $l ; then
    AC_MSG_RESULT(yes)
    PNG_LIBS=`$PKG_CONFIG --libs $l`
    png_ok=yes
    break
  else
    AC_MSG_RESULT(no)
    png_ok=no
  fi
done
if test "$png_ok" != yes; then
  AC_CHECK_LIB(png, png_read_info,
    [AC_CHECK_HEADER(png.h,
      png_ok=yes,
      png_ok=no)],
    AC_MSG_ERROR([PNG library (libpng) not found]), -lz -lm)
  if test "$png_ok" = yes; then
    AC_MSG_CHECKING([for png_structp in png.h])
    AC_TRY_COMPILE([#include <png.h>],
      [png_structp pp; png_infop info; png_colorp cmap; png_create_read_struct;],
      png_ok=yes,
      png_ok=no)
    AC_MSG_RESULT($png_ok)
    if test "$png_ok" = yes; then
      PNG_LIBS='-lpng -lz'
    else
      AC_MSG_ERROR([PNG library (libpng) not found])
    fi
  else
    AC_MSG_ERROR([PNG library (libpng) not found])
  fi
fi
AC_SUBST([PNG_LIBS])


if test "$os_win32" = "yes"; then
  WINMM_LIBS=-lwinmm
  SHLWAPI_LIBS=-lshlwapi
  OLE32_LIBS=-lole32
fi
AC_SUBST([WINMM_LIBS])
AC_SUBST([SHLWAPI_LIBS])
AC_SUBST([OLE32_LIBS])


# determine the GTK+ version to use
AC_MSG_CHECKING([the GTK+ version to use])
AC_ARG_WITH([gtk],
        [AS_HELP_STRING([--with-gtk=2.0|3.0], [the GTK+ version to use (default: 3.0)])],
        [case "$with_gtk" in
        2.0|3.0) ;;
        *) AC_MSG_ERROR([invalid GTK+ version specified]) ;;
        esac],
        [with_gtk=3.0])
AC_MSG_RESULT([$with_gtk])

GTK2_REQUIRED_VERSION=2.10
GAIL2_REQUIRED_VERSION=1.8
GTK3_REQUIRED_VERSION=3.4.0
GAIL3_REQUIRED_VERSION=3.0

case "$with_gtk" in
     2.0) GTK_REQUIRED_VERSION=$GTK2_REQUIRED_VERSION
          GTK_API_VERSION=2.0
          WEBKITGTK_API_MAJOR_VERSION=1
          WEBKITGTK_API_MINOR_VERSION=0
          WEBKITGTK_API_VERSION=1.0
          WEBKITGTK_PC_NAME=webkit
          GAIL_PC_NAME=gail
          GAIL_REQUIRED_VERSION=$GAIL2_REQUIRED_VERSION
          ;;
     3.0) GTK_REQUIRED_VERSION=$GTK3_REQUIRED_VERSION
          GTK_API_VERSION=3.0
          WEBKITGTK_API_MAJOR_VERSION=3
          WEBKITGTK_API_MINOR_VERSION=0
          WEBKITGTK_API_VERSION=3.0
          WEBKITGTK_PC_NAME=webkitgtk
          GAIL_PC_NAME=gail-3.0
          GAIL_REQUIRED_VERSION=$GAIL3_REQUIRED_VERSION
          ;;
esac

AC_SUBST([WEBKITGTK_API_MAJOR_VERSION])
AC_SUBST([WEBKITGTK_API_MINOR_VERSION])
AC_SUBST([WEBKITGTK_API_VERSION])
AC_SUBST([WEBKITGTK_PC_NAME])
AC_SUBST([GTK_API_VERSION])
AM_CONDITIONAL([GTK_API_VERSION_2],[test "$GTK_API_VERSION" = "2.0"])

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

AC_MSG_CHECKING([the GStreamer version to use])
AC_ARG_WITH([gstreamer],
        [AS_HELP_STRING([--with-gstreamer=0.10|1.0], [the GStreamer version to use (default: 0.10)])],
        [case "$with_gstreamer" in
        0.10|1.0) ;;
        *) AC_MSG_ERROR([invalid GStreamer version specified]) ;;
        esac],
        [with_gstreamer=0.10])
AC_MSG_RESULT([$with_gstreamer])

GSTREAMER_0_10_REQUIRED_VERSION=0.10
GSTREAMER_0_10_PLUGINS_BASE_REQUIRED_VERSION=0.10.30
GSTREAMER_1_0_REQUIRED_VERSION=0.11.90
GSTREAMER_1_0_PLUGINS_BASE_REQUIRED_VERSION=0.11.90

case "$with_gstreamer" in
     0.10) GSTREAMER_REQUIRED_VERSION=$GSTREAMER_0_10_REQUIRED_VERSION
           GSTREAMER_PLUGINS_BASE_REQUIRED_VERSION=$GSTREAMER_0_10_PLUGINS_BASE_REQUIRED_VERSION
           GST_API_VERSION=0.10
           ;;
     1.0) GSTREAMER_REQUIRED_VERSION=$GSTREAMER_1_0_REQUIRED_VERSION
           GSTREAMER_PLUGINS_BASE_REQUIRED_VERSION=$GSTREAMER_1_0_PLUGINS_BASE_REQUIRED_VERSION
           GST_API_VERSION=1.0
           ;;
esac

AC_SUBST([GST_API_VERSION])
if test "$GST_API_VERSION" = "1.0"; then
AC_DEFINE([GST_USE_UNSTABLE_API], [1], [Using unstable GStreamer API])
AC_DEFINE([GST_API_VERSION_1],[1], [Using GStreamer 1.0])
fi

# minimum base dependencies
CAIRO_REQUIRED_VERSION=1.10
FONTCONFIG_REQUIRED_VERSION=2.4
FREETYPE2_REQUIRED_VERSION=9.0
GLIB_REQUIRED_VERSION=2.32.0
LIBSOUP_REQUIRED_VERSION=2.39.2
LIBXML_REQUIRED_VERSION=2.6
PANGO_REQUIRED_VERSION=1.21.0

# optional modules
LIBXSLT_REQUIRED_VERSION=1.1.7
SQLITE_REQUIRED_VERSION=3.0
ENCHANT_REQUIRED_VERSION=0.22
CLUTTER_REQUIRED_VERSION=1.8.2
CLUTTER_GTK_REQUIRED_VERSION=1.0.2
ATSPI2_REQUIRED_VERSION=2.2.1

# Check for glib and required utilities
AM_PATH_GLIB_2_0($GLIB_REQUIRED_VERSION, :, :, gmodule gobject gthread gio)
if test -z "$GLIB_GENMARSHAL" || test -z "$GLIB_MKENUMS"; then
   AC_MSG_ERROR([You need the GLib dev tools in your path])
fi

# GResources
GLIB_COMPILE_RESOURCES=`$PKG_CONFIG --variable glib_compile_resources gio-2.0`
AC_SUBST(GLIB_COMPILE_RESOURCES)

GLIB_GSETTINGS

# Check and identify which unicode backend to use
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

if test "$with_unicode_backend" = "icu"; then
    # TODO: use pkg-config (after CFLAGS in their .pc files are cleaned up)
    case "$host" in
        *-*-darwin*)
            UNICODE_CFLAGS="-I$srcdir/Source/JavaScriptCore/icu -I$srcdir/Source/WebCore/icu"
            UNICODE_LIBS="-licucore"
            ;;
        *-*-mingw*)
            UNICODE_CFLAGS=""
            UNICODE_LIBS="-licui18n -licuuc"
            ;;
        *)
            AC_PATH_PROG(icu_config, icu-config, no)
            if test "$icu_config" = "no"; then
                AC_MSG_ERROR([Cannot find icu-config. The ICU library is needed.])
            fi

            # We don't use --cflags as this gives us a lot of things that we don't
            # necessarily want, like debugging and optimization flags
            # See man (1) icu-config for more info.
            UNICODE_CFLAGS=`$icu_config --cppflags`
            UNICODE_LIBS=`$icu_config --ldflags-libsonly`
            ;;
    esac
fi

if test "$with_unicode_backend" = "glib"; then
    PKG_CHECK_MODULES([UNICODE], [glib-2.0 pango >= $PANGO_REQUIRED_VERSION])
fi

AC_SUBST([UNICODE_CFLAGS])
AC_SUBST([UNICODE_LIBS])

PKG_CHECK_MODULES([ZLIB], [zlib])
AC_SUBST([ZLIB_CFLAGS])
AC_SUBST([ZLIB_LIBS])

GETTEXT_PACKAGE=$PACKAGE-$GTK_API_VERSION
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
                   [The gettext catalog name])

PKG_CHECK_MODULES(LIBXML, libxml-2.0 >= $LIBXML_REQUIRED_VERSION)
AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(LIBXML_LIBS)

PKG_CHECK_MODULES(PANGO, 
                  [pango >= $PANGO_REQUIRED_VERSION
                   pangoft2])
AC_SUBST(PANGO_CFLAGS)
AC_SUBST(PANGO_LIBS)

AC_MSG_CHECKING([whether to enable spellcheck support])
AC_ARG_ENABLE([spellcheck],
  [AS_HELP_STRING([--enable-spellcheck],[enable support for spellcheck])],
  [],[enable_spellcheck="yes"])
AC_MSG_RESULT([$enable_spellcheck])

if test "$enable_spellcheck" = "yes"; then
PKG_CHECK_MODULES(ENCHANT, enchant >= $ENCHANT_REQUIRED_VERSION, [], [enable_spellcheck="no"])
AC_SUBST(ENCHANT_CFLAGS)
AC_SUBST(ENCHANT_LIBS)
fi

# farstream-0.1 depends on gstreamer 0.10 currently. Disable
# media_stream if gstreamer 0.11 build support is enabled.
if test "$GST_API_VERSION" = "1.0"; then
   enable_media_stream=false
fi
if test "$enable_media_stream" = "yes"; then
   PKG_CHECK_MODULES([FARSTREAM], [farstream-0.1],
                     [have_farstream=yes], [enable_media_stream=false])
   AC_SUBST([FARSTREAM_CFLAGS])
   AC_SUBST([FARSTREAM_LIBS])
fi

PKG_CHECK_MODULES(GAIL, $GAIL_PC_NAME >= $GAIL_REQUIRED_VERSION)
AC_SUBST(GAIL_CFLAGS)
AC_SUBST(GAIL_LIBS)

# check for target-specific dependencies
if test "$with_target" = "directfb"; then
   PKG_CHECK_MODULES(CAIRO, cairo-directfb >= $CAIRO_REQUIRED_VERSION)
   PKG_CHECK_MODULES(GTK, gtk+-directfb-2.0 >= $GTK_REQUIRED_VERSION)
   AC_DEFINE([WTF_PLATFORM_DIRECTFB],[1],[Define if target is DirectFB])
else
   PKG_CHECK_MODULES(CAIRO, cairo >= $CAIRO_REQUIRED_VERSION)
   PKG_CHECK_MODULES(GTK, gtk+-$GTK_API_VERSION >= $GTK_REQUIRED_VERSION)

   if test "$with_target" = "x11" && test "$os_win32" = "no"; then
      # check for XT
      PKG_CHECK_MODULES([XT],
                        [xt],
                        [xt_has_pkg_config=yes],
                        [xt_has_pkg_config=no])
      # some old versions of Xt do not provide xt.pc, so try to link against Xt
      # and if it's installed fall back to just adding -lXt
      if test "$xt_has_pkg_config" = "no"; then
         # using AC_CHECK_LIB instead of AC_SEARCH_LIB is fine in this case as
         # we don't care about the XtOpenDisplay symbol but only about the
         # existence of libXt
         AC_CHECK_LIB([Xt], [XtOpenDisplay],
                      [XT_CFLAGS=""; XT_LIBS="-lXt"],
                      [AC_MSG_ERROR([X Toolkit Intrinsics library (libXt) not found])])
      fi
      AC_SUBST([XT_CFLAGS])
      AC_SUBST([XT_LIBS])
      AC_DEFINE([WTF_PLATFORM_X11],[1],[Define if target is X11])
   fi
fi
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)
AC_SUBST(CAIRO_CFLAGS)
AC_SUBST(CAIRO_LIBS)

# check whether to build with debugging enabled
AC_MSG_CHECKING([whether to do a debug build])
AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug],
                             [turn on debugging [default=no]]),
              [],[enable_debug="no"])
AC_MSG_RESULT([$enable_debug])

# check whether to enable optimized builds
AC_MSG_CHECKING([whether to enable optimized builds])
AC_ARG_ENABLE(optimizations,
              AC_HELP_STRING([--enable-optimizations],
                             [turn on optimize builds (GCC only)
                             [default=yes]]),
              [enable_optimizations=$enableval],
              [if test "$enable_debug" = "yes"; then enable_optimizations="no"; else enable_optimizations="yes"; fi])
AC_MSG_RESULT([$enable_optimizations])

# check whether to enable unstable features
AC_MSG_CHECKING([whether to enable unstable features])
AC_ARG_ENABLE(unstable_features,
              AC_HELP_STRING([--enable-unstable-features],
                             [enable unstable features [default=no]]),
              [],[enable_unstable_features="no"])
AC_MSG_RESULT([$enable_unstable_features])

found_opengl=no
AC_ARG_ENABLE(glx,
              AC_HELP_STRING([--enable-glx], [enable support for GLX [default=auto]]),
              [], [enable_glx="auto"])
AC_ARG_ENABLE(egl,
              AC_HELP_STRING([--enable-egl], [enable support for EGL [default=auto]]),
              [], [enable_egl="auto"])
AC_ARG_ENABLE(gles2,
              AC_HELP_STRING([--enable-gles2], [enable support for OpenGL ES 2 [default=auto]]),
              [], [enable_gles2="auto"])

have_glx=no
AC_CHECK_HEADERS([GL/glx.h], [have_glx="yes"], [have_glx="no"])
AC_MSG_CHECKING([whether to enable GLX support])
if test "$enable_glx" != "no"; then
   if test "$have_glx" = "no"; then
      if test "$enable_glx" = "yes"; then
         AC_MSG_ERROR([--enable-glx specified, but not available])
      else
         enable_glx=no
      fi
   elif test "$enable_gles2" != "yes"; then
      enable_glx=yes
   fi
fi
AC_MSG_RESULT([$enable_glx])

have_egl=no
AC_CHECK_HEADERS([EGL/egl.h], [have_egl="yes"], [have_egl="no"])
AC_MSG_CHECKING([whether to enable EGL support])
if test "$enable_egl" != "no"; then
   if test "$have_egl" = "no"; then
      if test "$enable_egl" = "yes"; then
         AC_MSG_ERROR([--enable-egl specified, but not available])
      else
         enable_egl=no
      fi
   else
      enable_egl=yes
   fi
fi
AC_MSG_RESULT([$enable_egl])

have_gles2=no
AC_CHECK_HEADERS([GLES2/gl2.h], [have_gles2="yes"], [have_gles2="no"])
AC_MSG_CHECKING([whether to use OpenGL ES 2 support])
if test "$enable_glx" = "yes"; then
   if test "$enable_gles2" = "yes"; then
      AC_MSG_ERROR([Cannot enable OpenGL ES 2 support with GLX])
   else
      enable_gles2=no
   fi
fi
if test "$enable_egl" = "no"; then
   if test "$enable_gles2" = "yes"; then
      AC_MSG_ERROR([Cannot enable OpenGL ES 2 support without EGL])
   else
      enable_gles2=no
   fi
fi
if test "$enable_gles2" != "no"; then
   if test "$have_gles2" = "no"; then
      if test "$enable_gles2" = "yes"; then
         AC_MSG_ERROR([--enable-gles2 specified, but not available])
      else
         enable_gles2=no
      fi
   else
      enable_gles2=yes
      found_opengl=yes
   fi
fi
AC_MSG_RESULT([$enable_gles2])

if test "$enable_gles2" != "yes"; then
    AC_CHECK_HEADERS([GL/gl.h], [found_opengl="yes"], [])
fi

AC_MSG_CHECKING([which GPU acceleration backend to use])
AC_ARG_WITH(acceleration_backend,
            AC_HELP_STRING([--with-acceleration-backend=@<:@opengl/clutter/none@:>@],
                           [Select accelerated backend (Clutter currently unsupported, OpenGL autodetected) [default=autodetect]]), [],
                           [
                                if test "$found_opengl" = "yes"; then
                                   with_acceleration_backend="opengl";
                                else
                                   with_acceleration_backend="none";
                                fi
                           ])
AC_MSG_RESULT([$with_acceleration_backend])

if test "$with_acceleration_backend" = "opengl"; then
    PKG_CHECK_MODULES([XCOMPOSITE], [xcomposite]);
    PKG_CHECK_MODULES([XDAMAGE], [xdamage]);
    AC_SUBST(XCOMPOSITE_CFLAGS)
    AC_SUBST(XCOMPOSITE_LIBS)
    AC_SUBST(XDAMAGE_CFLAGS)
    AC_SUBST(XDAMAGE_LIBS)
fi

# OpenGL is turned on by default (along with WebGL and accelerated compositing), but if
# Clutter is chosen as the accelerated drawing backend, we want to disable it. COGL does 
# not play well with OpenGL.
AC_MSG_CHECKING([whether to enable WebGL support])
AC_ARG_ENABLE(webgl,
              AC_HELP_STRING([--enable-webgl], [enable support for WebGL [default=check]]), [],
              [
                if test "$with_acceleration_backend" = "opengl"; then
                    enable_webgl="yes";
                else
                    enable_webgl="no";
                fi
             ])
AC_MSG_RESULT([$enable_webgl])
if test "$enable_webgl" = "yes" && test "$with_acceleration_backend" != "opengl"; then
    AC_MSG_ERROR([OpenGL must be active (and Clutter disabled) to use WebGL.])
fi;

AC_MSG_CHECKING([whether to enable accelerated compositing support])
AC_ARG_ENABLE(accelerated_compositing,
            AC_HELP_STRING([--enable-accelerated-compositing], [enable support for accelerated compositing [default=check]]),
            [
                if test "$enableval" = "yes" && test "$with_acceleration_backend" = "none"; then
                    AC_MSG_ERROR([OpenGL or Clutter must be active to use accelerated compositing.])
                fi
                enable_accelerated_compositing=$enableval;
            ],
            [
                if test "$with_acceleration_backend" != "none"; then
                    enable_accelerated_compositing="yes";
                else
                    enable_accelerated_compositing="no";
                fi
            ])
AC_MSG_RESULT([$enable_accelerated_compositing])

# check whether to enable Gamepad support
AC_MSG_CHECKING([whether to enable Gamepad support])
AC_ARG_ENABLE(gamepad,
              AC_HELP_STRING([--enable-gamepad],
                             [enable Gamepad support [default=no]]),
              [],[enable_gamepad=$enable_unstable_features])
AC_MSG_RESULT([$enable_gamepad])

if test "$enable_gamepad" = "yes" && test "$os_linux" = no; then
   AC_MSG_WARN([Gamepad support is only available on Linux. Disabling Gamepad support.])
   enable_gamepad=no;
fi

# check whether to enable the indexed database API
AC_MSG_CHECKING([whether to enable the indexed database API])
AC_ARG_ENABLE(indexed_database,
              AC_HELP_STRING([--enable-indexed-database],
                             [enable the indexed database API (incomplete) [default=no]]),
              [],[enable_indexed_database="no"])
AC_MSG_RESULT([$enable_indexed_database])

# check whether to enable HTML5 audio/video support
AC_MSG_CHECKING([whether to enable HTML5 video support])
AC_ARG_ENABLE(video,
              AC_HELP_STRING([--enable-video],
                             [enable HTML5 video support [default=yes]]),
              [],[enable_video="yes"])
AC_MSG_RESULT([$enable_video])

# check whether to enable media stream support
AC_MSG_CHECKING([whether to enable media stream support])
AC_ARG_ENABLE(media_stream,
              AC_HELP_STRING([--enable-media-stream],
                             [enable media stream support (incomplete) [default=no]]),
              [],[enable_media_stream=$enable_unstable_features])
AC_MSG_RESULT([$enable_media_stream])

# check whether to enable XHR timeout support
AC_MSG_CHECKING([whether to enable XHR timeout support])
AC_ARG_ENABLE(xhr_timeout,
              AC_HELP_STRING([--enable-xhr-timeout],
                             [enable support for XHR timeout [default=yes]]),
              [],[enable_xhr_timeout="yes"])
AC_MSG_RESULT([$enable_xhr_timeout])

# check whether to enable XSLT support
AC_MSG_CHECKING([whether to enable XSLT support])
AC_ARG_ENABLE(xslt,
              AC_HELP_STRING([--enable-xslt],
                             [enable support for XSLT [default=yes]]),
              [],[enable_xslt="yes"])
AC_MSG_RESULT([$enable_xslt])

# check whether to enable geolocation support
AC_MSG_CHECKING([whether to enable geolocation support])
AC_ARG_ENABLE(geolocation,
              AC_HELP_STRING([--enable-geolocation],
                             [enable support for geolocation [default=yes]]),
              [],[enable_geolocation="yes"])
AC_MSG_RESULT([$enable_geolocation])

# check whether to enable SVG support
AC_MSG_CHECKING([whether to enable SVG support])
AC_ARG_ENABLE(svg,
              AC_HELP_STRING([--enable-svg],
                             [enable support for SVG [default=yes]]),
              [],[enable_svg="yes"])
AC_MSG_RESULT([$enable_svg])

# turn off svg features if --disable-svg is requested
if test "$enable_svg" = "no"; then
   enable_svg_fonts=no
fi

# check whether to enable support for filters
AC_MSG_CHECKING([whether to enable support for filters])
AC_ARG_ENABLE(filters,
              AC_HELP_STRING([--enable-filters],
                             [enable support for filters (experimental) [default=yes]]),
              [],[enable_filters="yes"])
AC_MSG_RESULT([$enable_filters])

# check whether to enable support for SVG fonts
AC_MSG_CHECKING([whether to enable support for SVG fonts])
AC_ARG_ENABLE(svg_fonts,
              AC_HELP_STRING([--enable-svg-fonts],
                             [enable support for SVG fonts (experimental) [default=yes]]),
              [],[enable_svg_fonts="yes"])
AC_MSG_RESULT([$enable_svg_fonts])

# check for SVG features, enabling SVG if necessary
if test "$enable_svg_fonts" = "yes" && test "$enable_svg" = "no"; then
   AC_MSG_WARN([SVG feature(s) requested but SVG is disabled.. Enabling SVG support])
   enable_svg=yes
fi

# check whether to enable Web Socket support
AC_MSG_CHECKING([whether to enable Web Sockets support])
AC_ARG_ENABLE(web_sockets,
              AC_HELP_STRING([--enable-web-sockets],
                             [enable support for Web Sockets [default=yes]]),
              [],[enable_web_sockets="yes"])
AC_MSG_RESULT([$enable_web_sockets])

# check whether to enable Web Audio support
AC_MSG_CHECKING([whether to enable Web Audio support])
AC_ARG_ENABLE(web_audio,
              AC_HELP_STRING([--enable-web-audio],
                             [enable support for Web Audio [default=no]]),
              [],[enable_web_audio="no"])
AC_MSG_RESULT([$enable_web_audio])

# check whether to enable code coverage
AC_MSG_CHECKING([whether to enable code coverage support])
AC_ARG_ENABLE(coverage,
              AC_HELP_STRING([--enable-coverage],
                             [enable code coverage support [default=no]]),
              [],[enable_coverage="no"])
AC_MSG_RESULT([$enable_coverage])

# check whether to enable CSS3 background support
AC_MSG_CHECKING([whether to enable CSS3 background support])
AC_ARG_ENABLE(css3_background,
              AC_HELP_STRING([--enable-css3-background],
                             [enable CSS3 background support [default=yes]]),
              [],[enable_css3_background="yes"])
AC_MSG_RESULT([$enable_css3_background])

# check whether to enable CSS conditional rules support
AC_MSG_CHECKING([whether to enable CSS3 conditional rules support])
AC_ARG_ENABLE(css3_conditional_rules,
              AC_HELP_STRING([--enable-css3-conditional-rules],
                             [enable CSS3 conditional rules support [default=no]]),
              [],[enable_css3_conditional_rules="no"])
AC_MSG_RESULT([$enable_css3_conditional_rules])

# check whether to enable FastMalloc
AC_MSG_CHECKING([whether to enable optimized memory allocator])
AC_ARG_ENABLE(fast_malloc,
              AC_HELP_STRING([--enable-fast-malloc],
                             [enable optimized memory allocator default=yes, default=no for debug builds]),
              [],[if test "$enable_debug" = "yes"; then enable_fast_malloc="no"; else enable_fast_malloc="yes"; fi])
AC_MSG_RESULT([$enable_fast_malloc])

# check whether to enable debug symbols
AC_MSG_CHECKING([whether to enable debug symbols])
AC_ARG_ENABLE(debug_symbols,
              AC_HELP_STRING([--enable-debug-symbols=yes|no|min|full],
                             [enable debug symbols default=no, default=yes for debug builds]),
              [],[if test "$enable_debug" = "yes"; then enable_debug_symbols="yes"; else enable_debug_symbols="no"; fi])

case "$enable_debug_symbols" in
  yes) enable_debug_symbols="full" ;;
  no|min|full) ;;
  *) AC_MSG_ERROR([Invalid debug symbols option: must be yes, no, min or full.]) ;;
esac
AC_MSG_RESULT([$enable_debug_symbols])

# check whether to enable debug features
AC_MSG_CHECKING([whether to enable debug features])
AC_ARG_ENABLE(debug_features,
              AC_HELP_STRING([--enable-debug-features],
                             [enable debug features default=no, default=yes for debug builds]),
              [],[if test "$enable_debug" = "yes"; then enable_debug_features="yes"; else enable_debug_features="no"; fi])
AC_MSG_RESULT([$enable_debug_features])

AC_MSG_CHECKING([whether to enable JIT compilation])
AC_ARG_ENABLE(jit,
              AS_HELP_STRING([--enable-jit],
                             [Enable JIT compilation (default: autodetect)]))
case "$enable_jit" in
  yes) JSC_CPPFLAGS="-DENABLE_JIT=1" ;;
  no) JSC_CPPFLAGS="-DENABLE_JIT=0" ;;
  *) enable_jit="autodetect" ;;
esac
AC_SUBST(JSC_CPPFLAGS)
AC_MSG_RESULT([$enable_jit])

# Opcode stats
AC_MSG_CHECKING([whether to enable opcode stats])
AC_ARG_ENABLE([opcode-stats],
  [AS_HELP_STRING([--enable-opcode-stats], [Enable Opcode statistics (default: disabled)])],
  [], [enable_opcode_stats=no])
AC_MSG_RESULT([$enable_opcode_stats])

if test "$enable_opcode_stats" = "yes"; then
  if test "$enable_jit" = "yes"; then
    AC_MSG_ERROR([JIT must be disabled for Opcode stats to work.])
  fi
  AC_DEFINE([ENABLE_OPCODE_STATS], [1], [Define to enable Opcode statistics])
fi

# GObject Introspection
AC_MSG_CHECKING([whether to enable GObject introspection support])
AC_ARG_ENABLE([introspection],
  [AS_HELP_STRING([--enable-introspection],[Enable GObject introspection (default: disabled)])],
  [],[enable_introspection=no])
AC_MSG_RESULT([$enable_introspection])

# Enable CSS Filters and Shaders if unstable_features and accelerated_compositing are turned on.
enable_css_filters=no;
enable_css_shaders=no;
AC_MSG_CHECKING([whether to enable CSS Filters and Shaders])
if test "$enable_unstable_features" = "yes" && test "$enable_accelerated_compositing" = "yes" && test "$with_acceleration_backend" = "opengl"; then
    enable_css_filters=yes;
    enable_css_shaders=yes;
fi
AC_MSG_RESULT([$enable_css_filters])

# CSS Device Adaptation
AC_MSG_CHECKING([whether to enable CSS Device Adaptation])
AC_ARG_ENABLE(css_device_adaptation,
              AC_HELP_STRING([--enable-css-device-adaptation],
                             [enable support for CSS Device Adaptation [default=no]]),
              [],[enable_css_device_adaptation="no"])
AC_MSG_RESULT([$enable_css_device_adaptation])

G_IR_SCANNER=
G_IR_COMPILER=
G_IR_GENERATE=
GIRDIR=
GIRTYPELIBDIR=

if test "$enable_introspection" = "yes"; then
  GOBJECT_INTROSPECTION_REQUIRED=0.9.5
  PKG_CHECK_MODULES([INTROSPECTION],[gobject-introspection-1.0 >= $GOBJECT_INTROSPECTION_REQUIRED])
  
  G_IR_SCANNER="$($PKG_CONFIG --variable=g_ir_scanner gobject-introspection-1.0)"
  G_IR_COMPILER="$($PKG_CONFIG --variable=g_ir_compiler gobject-introspection-1.0)"
  G_IR_GENERATE="$($PKG_CONFIG --variable=g_ir_generate gobject-introspection-1.0)"
  AC_DEFINE([ENABLE_INTROSPECTION],[1],[Define to enable GObject introspection support])
fi

AC_SUBST([G_IR_SCANNER])
AC_SUBST([G_IR_COMPILER])
AC_SUBST([G_IR_GENERATE])

# determine the font backend
AC_MSG_CHECKING([the font backend to use])
AC_ARG_WITH(font_backend,
            AC_HELP_STRING([--with-font-backend=@<:@freetype/pango@:>@],
                           [Select font backend [default=freetype]]),
            [],[with_font_backend="freetype"])

case "$with_font_backend" in
     freetype|pango) ;;
     *) AC_MSG_ERROR([Invalid font backend: must be freetype or pango.]) ;;
esac
AC_MSG_RESULT([$with_font_backend])

if test "$host_cpu" = "sh4"; then
   CXXFLAGS="$CXXFLAGS -mieee -w"
   CFLAGS="$CFLAGS -mieee -w"
fi

# Add '-g' flag to gcc to build with debug symbols
if test "$enable_debug_symbols" = "min"; then
   CXXFLAGS="$CXXFLAGS -g1"
   CFLAGS="$CFLAGS -g1"
elif test "$enable_debug_symbols" != "no"; then
   CXXFLAGS="$CXXFLAGS -g"
   CFLAGS="$CFLAGS -g"
fi

if test "$enable_debug_features" = "no"; then
   AC_DEFINE([NDEBUG], [1], [Define to disable debugging features])
fi

# Add the appropriate 'O' level for optimized builds
if test "$enable_optimizations" = "yes"; then
   CXXFLAGS="$CXXFLAGS -O2"
   CFLAGS="$CFLAGS -O2"
else
   CXXFLAGS="$CXXFLAGS -O0"
   CFLAGS="$CFLAGS -O0"
fi

PKG_CHECK_MODULES([LIBSOUP],
                  [libsoup-2.4 >= $LIBSOUP_REQUIRED_VERSION])
AC_SUBST([LIBSOUP_CFLAGS])
AC_SUBST([LIBSOUP_LIBS])

PKG_CHECK_MODULES([LIBSECRET], [libsecret-1])
AC_SUBST([LIBSECRET_CFLAGS])
AC_SUBST([LIBSECRET_LIBS])

# check if FreeType/FontConfig are available
if test "$with_font_backend" = "freetype"; then
   if test "$with_target" = "directfb"; then
   PKG_CHECK_MODULES([FREETYPE],
                     [fontconfig >= $FONTCONFIG_REQUIRED_VERSION
                     freetype2 >= $FREETYPE2_REQUIRED_VERSION
                     harfbuzz])
   else
   PKG_CHECK_MODULES([FREETYPE],
                     [cairo-ft
                     fontconfig >= $FONTCONFIG_REQUIRED_VERSION
                     freetype2 >= $FREETYPE2_REQUIRED_VERSION
                     harfbuzz])
   fi
   AC_SUBST([FREETYPE_CFLAGS])
   AC_SUBST([FREETYPE_LIBS])
fi

# check if SQLite3 is available. Error out only if one of the
# features hard-depending on it is enabled while SQLite3 is
# unavailable.
PKG_CHECK_MODULES([SQLITE3],
                  [sqlite3 >= $SQLITE_REQUIRED_VERSION],
                  [sqlite3_has_pkg_config=yes],
                  [sqlite3_has_pkg_config=no])
if test "$sqlite3_has_pkg_config" = "no"; then
   AC_SEARCH_LIBS([sqlite3_open16], [sqlite3],
                  [sqlite3_found=yes;SQLITE3_LIBS="$LIBS";SQLITE3_CFLAGS="-I $srcdir/WebKitLibraries/WebCoreSQLite3"],
                  [sqlite3_found=no])
fi
AC_SUBST([SQLITE3_CFLAGS])
AC_SUBST([SQLITE3_LIBS])

if (test "$sqlite3_found" = "no"); then
   AC_MSG_ERROR([SQLite3 is required for the Database related features])
fi

# check if libxslt is available
if test "$enable_xslt" = "yes"; then
   PKG_CHECK_MODULES([LIBXSLT],[libxslt >= $LIBXSLT_REQUIRED_VERSION])
   AC_SUBST([LIBXSLT_CFLAGS])
   AC_SUBST([LIBXSLT_LIBS])
fi

# check if geoclue is available
if test "$enable_geolocation" = "yes"; then
    PKG_CHECK_MODULES([GEOCLUE], [geoclue])
    AC_SUBST([GEOCLUE_CFLAGS])
    AC_SUBST([GEOCLUE_LIBS])
fi

# check for XRender under Linux/Unix. Some linkers require explicit
# linkage (like GNU Gold), so we cannot rely on GTK+ pulling XRender
if test "$os_win32" = "no"; then
   PKG_CHECK_MODULES([XRENDER], [xrender])
   AC_SUBST([XRENDER_CFLAGS])
   AC_SUBST([XRENDER_LIBS])
fi

# check if gstreamer is available
if test "$enable_video" = "yes" || test "$enable_web_audio" = "yes"; then
    gstreamer_modules="gstreamer-$GST_API_VERSION >= $GSTREAMER_REQUIRED_VERSION
                       gstreamer-app-$GST_API_VERSION
                       gstreamer-audio-$GST_API_VERSION
                       gstreamer-fft-$GST_API_VERSION
                       gstreamer-base-$GST_API_VERSION
                       gstreamer-pbutils-$GST_API_VERSION
                       gstreamer-plugins-base-$GST_API_VERSION >= $GSTREAMER_PLUGINS_BASE_REQUIRED_VERSION
                       gstreamer-video-$GST_API_VERSION";
   if test "$GST_API_VERSION" != "1.0"; then
       gstreamer_modules="$gstreamer_modules gstreamer-interfaces-$GST_API_VERSION";
   fi
   PKG_CHECK_MODULES([GSTREAMER], [$gstreamer_modules], [have_gstreamer=yes])
   AC_SUBST([GSTREAMER_CFLAGS])
   AC_SUBST([GSTREAMER_LIBS])
fi

acceleration_backend_description=$with_acceleration_backend
if test "$with_acceleration_backend" = "clutter"; then
   PKG_CHECK_MODULES(CLUTTER, clutter-1.0 >= $CLUTTER_REQUIRED_VERSION)
   PKG_CHECK_MODULES([CLUTTER_GTK],
                     [clutter-gtk-1.0 >= $CLUTTER_GTK_REQUIRED_VERSION])

   AC_SUBST(CLUTTER_CFLAGS)
   AC_SUBST(CLUTTER_LIBS)
   AC_SUBST(CLUTTER_GTK_CFLAGS)
   AC_SUBST(CLUTTER_GTK_LIBS)
fi

if test "$with_acceleration_backend" = "opengl"; then
   if test "$enable_gles2" = "yes"; then
    acceleration_backend_description+= "(gles2"
    OPENGL_LIBS="-lGLESv2"
   else
    acceleration_backend_description+="(gl"
    OPENGL_LIBS="-lGL"
   fi
   if test "$enable_egl" = "yes"; then
    acceleration_backend_description+=", egl"
    OPENGL_LIBS+=" -lEGL"
   fi
   if test "$enable_glx" = "yes"; then
    acceleration_backend_description+=", glx"
   fi
   OPENGL_LIBS+=" -ldl"
   acceleration_backend_description+=")"
fi
AC_SUBST([OPENGL_LIBS])

if test "$enable_gamepad" = "yes"; then
   PKG_CHECK_MODULES([GAMEPAD], [gio-unix-2.0 gudev-1.0])

   AC_SUBST(GAMEPAD_CFLAGS)
   AC_SUBST(GAMEPAD_LIBS)
fi

# check for code coverage support
if test "$enable_coverage" = "yes"; then
   COVERAGE_CFLAGS="-MD"
   COVERAGE_LDFLAGS="-ftest-coverage -fprofile-arcs"
   AC_SUBST([COVERAGE_CFLAGS])
   AC_SUBST([COVERAGE_LDFLAGS])
fi

# WebKit2
# If you change the default here, please also make sure the assumptions made
# in Tools/Scripts/webkitdirs.pm:buildAutotoolsProject still make sense.
AC_MSG_CHECKING([whether to build Webkit2])
AC_ARG_ENABLE(webkit2,
    AC_HELP_STRING([--enable-webkit2], [build webkit2 [default=yes]]),
    [], [enable_webkit2="yes"])
AC_MSG_RESULT([$enable_webkit2])
if test "$enable_webkit2" = "yes"; then
   if test "$GTK_API_VERSION" = "2.0"; then
      AC_MSG_ERROR([WebKit2 requires GTK+ 3.x, use --with-gtk=3.0])
   fi

   # Check Unix printing
   PKG_CHECK_MODULES(GTK_UNIX_PRINTING, gtk+-unix-print-3.0,
                     [have_gtk_unix_printing=yes],
                     [have_gtk_unix_printing=no])
   AC_SUBST(GTK_UNIX_PRINTING_CFLAGS)
   AC_SUBST(GTK_UNIX_PRINTING_LIBS)
   if test "$have_gtk_unix_printing" = "yes"; then
       AC_DEFINE([HAVE_GTK_UNIX_PRINTING], [1], [Define if GTK+ UNIX Printing is available])
   fi

   # On some Linux/Unix platforms, shm_* may only be available if linking
   # against librt
   if test "$os_win32" = "no"; then
       AC_SEARCH_LIBS([shm_open], [rt], [SHM_LIBS="-lrt"])
       AC_SUBST(SHM_LIBS)
   fi
fi

# Plugin Process
AC_MSG_CHECKING([whether to build plugin process for WebKit2])
AC_ARG_ENABLE(plugin_process,
            AC_HELP_STRING([--enable-plugin-process], [build plugin process for WebKit2 [default=yes]]),
                           [], [enable_plugin_process="yes"])
AC_MSG_RESULT([$enable_plugin_process])

# Build the plugin process only when building Webkit2.
if test "$enable_webkit2" = "no"; then
    enable_plugin_process=no
fi

# Make sure we have GTK+ 2.x to build the plugin process.
if test "$enable_plugin_process" = "yes"; then
   PKG_CHECK_MODULES(GTK2, gtk+-2.0 >= $GTK2_REQUIRED_VERSION gail >= $GAIL2_REQUIRED_VERSION)
fi
AC_SUBST(GTK2_CFLAGS)
AC_SUBST(GTK2_LIBS)

GTK_DOC_CHECK([1.10])

# We need atspi2 for Webkit2 unit tests
if test "$enable_webkit2" = "yes"; then
   PKG_CHECK_MODULES([ATSPI2],
                     [atspi-2 >= $ATSPI2_REQUIRED_VERSION],
                     [have_atspi2=yes], [have_atspi2=no])

   AC_SUBST([ATSPI2_CFLAGS])
   AC_SUBST([ATSPI2_LIBS])
fi

# OS conditionals
AM_CONDITIONAL([OS_WIN32],[test "$os_win32" = "yes"])
AM_CONDITIONAL([OS_UNIX],[test "$os_win32" = "no"])
AM_CONDITIONAL([OS_LINUX],[test "$os_linux" = "yes"])
AM_CONDITIONAL([OS_GNU],[test "$os_gnu" = "yes"])
AM_CONDITIONAL([OS_FREEBSD],[test "$os_freebsd" = "yes"])

# target conditionals
AM_CONDITIONAL([TARGET_X11], [test "$with_target" = "x11"])
AM_CONDITIONAL([TARGET_WIN32], [test "$with_target" = "win32"])
AM_CONDITIONAL([TARGET_QUARTZ], [test "$with_target" = "quartz"])
AM_CONDITIONAL([TARGET_DIRECTFB], [test "$with_target" = "directfb"])

# Unicode backend conditionals
AM_CONDITIONAL([USE_ICU_UNICODE], [test "$with_unicode_backend" = "icu"])
AM_CONDITIONAL([USE_GLIB_UNICODE], [test "$with_unicode_backend" = "glib"])

# Font backend conditionals
AM_CONDITIONAL([USE_FREETYPE], [test "$with_font_backend" = "freetype"])
AM_CONDITIONAL([USE_PANGO], [test "$with_font_backend" = "pango"])

# GStreamer feature conditional
AM_CONDITIONAL([USE_GSTREAMER], [test "$have_gstreamer" = "yes"])
AM_CONDITIONAL([USE_WEBAUDIO_GSTREAMER], [test "$enable_web_audio" = "yes"])

# ATSPI2 conditional
AM_CONDITIONAL([HAVE_ATSPI2], [test "$have_atspi2" = "yes"])

# Accelerated compositing conditional
AM_CONDITIONAL([USE_ACCELERATED_COMPOSITING], [test "$enable_accelerated_compositing" = "yes"])
AM_CONDITIONAL([USE_TEXTURE_MAPPER_CAIRO], [test "$enable_accelerated_compositing" = "yes" && test "$with_acceleration_backend" = "none"])
AM_CONDITIONAL([USE_TEXTURE_MAPPER_GL], [test "$enable_accelerated_compositing" = "yes" && test "$with_acceleration_backend" = "opengl"])
AM_CONDITIONAL([USE_CLUTTER], [test "$with_acceleration_backend" = "clutter"])

# These are the same now, but they will soon be separate.
AM_CONDITIONAL([USE_OPENGL], [test "$with_acceleration_backend" = "opengl"])
AM_CONDITIONAL([USE_EGL], [test "$enable_egl" = "yes"])
AM_CONDITIONAL([USE_GLES2], [test "$enable_gles2" = "yes"])
AM_CONDITIONAL([USE_GLX], [test "$enable_glx" = "yes"])

# IndexedDB implies LevelDB, but other features may one day require LevelDB.
AM_CONDITIONAL([ENABLE_INDEXED_DATABASE],[test "$enable_indexed_database" = "yes"])
AM_CONDITIONAL([USE_LEVELDB],[test "$enable_indexed_database" = "yes"])

# Farstream feature conditional
AM_CONDITIONAL([USE_FARSTREAM], [test "$have_farstream" = "yes"])

# WebKit feature conditionals
AM_CONDITIONAL([ENABLE_DEBUG],[test "$enable_debug_features" = "yes"])
AM_CONDITIONAL([ENABLE_UNSTABLE_FEATURES],[test "$enable_unstable_features" = "yes"])
AM_CONDITIONAL([ENABLE_WEBGL],[test "$enable_webgl" = "yes"])
AM_CONDITIONAL([ENABLE_GAMEPAD],[test "$enable_gamepad" = "yes"])
AM_CONDITIONAL([ENABLE_XHR_TIMEOUT],[test "$enable_xhr_timeout" = "yes"])
AM_CONDITIONAL([ENABLE_XSLT],[test "$enable_xslt" = "yes"])
AM_CONDITIONAL([ENABLE_FILTERS],[test "$enable_filters" = "yes"])
AM_CONDITIONAL([ENABLE_GEOLOCATION], [test "$enable_geolocation" = "yes"])
AM_CONDITIONAL([ENABLE_VIDEO],[test "$enable_video" = "yes"])
AM_CONDITIONAL([ENABLE_MEDIA_STREAM],[test "$enable_media_stream" = "yes"])
AM_CONDITIONAL([ENABLE_SVG],[test "$enable_svg" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_FONTS],[test "$enable_svg_fonts" = "yes"])
AM_CONDITIONAL([ENABLE_COVERAGE],[test "$enable_coverage" = "yes"])
AM_CONDITIONAL([ENABLE_CSS3_BACKGROUND],[test "$enable_css3_background" = "yes"])
AM_CONDITIONAL([ENABLE_CSS3_CONDITIONAL_RULES],[test "$enable_css3_conditional_rules" = "yes"])
AM_CONDITIONAL([ENABLE_FAST_MALLOC],[test "$enable_fast_malloc" = "yes"])
AM_CONDITIONAL([ENABLE_WEB_AUDIO],[test "$enable_web_audio" = "yes"])
AM_CONDITIONAL([ENABLE_OPCODE_STATS],[test "$enable_opcode_stats" = "yes"])
AM_CONDITIONAL([ENABLE_WEBKIT2],[test "$enable_webkit2" = "yes"])
AM_CONDITIONAL([ENABLE_PLUGIN_PROCESS],[test "$enable_plugin_process" = "yes"])
AM_CONDITIONAL([ENABLE_SPELLCHECK],[test "$enable_spellcheck" = "yes"])
AM_CONDITIONAL([ENABLE_CSS_DEVICE_ADAPTATION],[test "$enable_css_device_adaptation" = "yes"])
AM_CONDITIONAL([ENABLE_CSS_FILTERS],[test "$enable_css_filters" = "yes"])
AM_CONDITIONAL([ENABLE_CSS_SHADERS],[test "$enable_css_shaders" = "yes"])

# Gtk conditionals
AM_CONDITIONAL([ENABLE_INTROSPECTION],[test "$enable_introspection" = "yes"])

AC_CONFIG_FILES([
GNUmakefile
])
 

AC_CONFIG_FILES([
Source/WebKit/gtk/webkit/webkitversion.h
Source/WebKit/gtk/po/POTFILES
DerivedSources/WebCore/UserAgentGtk.h:Source/WebCore/platform/gtk/UserAgentGtk.h.in
])

AC_CONFIG_FILES([
Source/WebKit/gtk/${WEBKITGTK_PC_NAME}-${WEBKITGTK_API_VERSION}.pc:Source/WebKit/gtk/webkit.pc.in
Source/WebKit/gtk/JSCore-${WEBKITGTK_API_VERSION}.gir:Source/WebKit/gtk/JSCore.gir.in
Source/JavaScriptCore/javascriptcoregtk-${WEBKITGTK_API_VERSION}.pc:Source/JavaScriptCore/javascriptcoregtk.pc.in
]
,[WEBKITGTK_API_VERSION=$WEBKITGTK_API_VERSION,WEBKITGTK_PC_NAME=$WEBKITGTK_PC_NAME]
)


if test "$enable_webkit2" = "yes"; then
    AC_CONFIG_FILES([
    DerivedSources/WebKit2/webkit2gtk/webkit2/WebKitVersion.h:Source/WebKit2/UIProcess/API/gtk/WebKitVersion.h.in
    ])

    AC_CONFIG_FILES([
    Source/WebKit2/webkit2gtk-${WEBKITGTK_API_VERSION}.pc:Source/WebKit2/webkit2gtk.pc.in
    ]
    ,[WEBKITGTK_API_VERSION=$WEBKITGTK_API_VERSION,WEBKITGTK_PC_NAME=$WEBKITGTK_PC_NAME]
    )
fi

AC_OUTPUT

echo "
WebKit was configured with the following options:

Build configuration:
 Enable debugging (slow)                                  : $enable_debug
 Compile with debug symbols (slow)                        : $enable_debug_symbols
 Enable debug features (slow)                             : $enable_debug_features
 Enable GCC build optimization                            : $enable_optimizations
 Code coverage support                                    : $enable_coverage
 Unicode backend                                          : $with_unicode_backend
 Font backend                                             : $with_font_backend
 Optimized memory allocator                               : $enable_fast_malloc
 Accelerated rendering backend                            : $acceleration_backend_description

Features:
=======
 Accelerated Compositing                                  : $enable_accelerated_compositing
 CSS3 Conditional Rules support                           : $enable_css3_conditional_rules
 CSS3 background support                                  : $enable_css3_background
 Filters support                                          : $enable_filters
 Gamepad support                                          : $enable_gamepad
 Geolocation support                                      : $enable_geolocation
 HTML5 video element support                              : $enable_video
 JIT compilation                                          : $enable_jit
 Media stream support                                     : $enable_media_stream
 Opcode stats                                             : $enable_opcode_stats
 SVG fonts support                                        : $enable_svg_fonts
 SVG support                                              : $enable_svg
 Spellcheck support                                       : $enable_spellcheck
 Web Audio support                                        : $enable_web_audio
 WebGL                                                    : $enable_webgl
 XHR timeout support                                      : $enable_xhr_timeout
 XSLT support                                             : $enable_xslt

WebKit2 support:
 WebKit2 support                                          : $enable_webkit2
 WebKit2 plugin process                                   : $enable_plugin_process

GTK+ configuration:
 GTK+ version                                             : $with_gtk
 GDK target                                               : $with_target
 GStreamer version                                        : $with_gstreamer
 Introspection support                                    : $enable_introspection
 Generate documentation                                   : $enable_gtk_doc
"
if test "$with_unicode_backend" = "glib"; then
   echo "     >> WARNING: the glib-based unicode backend is slow and incomplete <<"
   echo
   echo
fi
