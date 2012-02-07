AC_PREREQ(2.59)

m4_define([webkit_major_version], [1])
m4_define([webkit_minor_version], [7])
m4_define([webkit_micro_version], [5])

# This is the version we'll be using as part of our User-Agent string
# e.g., AppleWebKit/$(webkit_user_agent_version) ...
#
# Sourced from Source/WebCore/Configurations/Version.xcconfig
m4_define([webkit_user_agent_major_version], [535])
m4_define([webkit_user_agent_minor_version], [20])

AC_INIT([WebKit],[webkit_major_version.webkit_minor_version.webkit_micro_version],[http://bugs.webkit.org/])

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
LIBWEBKITGTK_VERSION=12:0:12
AC_SUBST([LIBWEBKITGTK_VERSION])

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

# initialize webkit options
WEBKIT_INIT
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
GTK3_REQUIRED_VERSION=3.0
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

AC_MSG_CHECKING([the GStreamer version to use])
AC_ARG_WITH([gstreamer],
        [AS_HELP_STRING([--with-gstreamer=0.10|0.11], [the GStreamer version to use (default: 0.10)])],
        [case "$with_gstreamer" in
        0.10|0.11) ;;
        *) AC_MSG_ERROR([invalid GStreamer version specified]) ;;
        esac],
        [with_gstreamer=0.10])
AC_MSG_RESULT([$with_gstreamer])

GSTREAMER_0_10_REQUIRED_VERSION=0.10
GSTREAMER_0_10_PLUGINS_BASE_REQUIRED_VERSION=0.10.30
GSTREAMER_0_11_REQUIRED_VERSION=0.11
GSTREAMER_0_11_PLUGINS_BASE_REQUIRED_VERSION=0.11.0.2

case "$with_gstreamer" in
     0.10) GSTREAMER_REQUIRED_VERSION=GSTREAMER_0_10_REQUIRED_VERSION
           GSTREAMER_PLUGINS_BASE_REQUIRED_VERSION=GSTREAMER_0_10_PLUGINS_BASE_REQUIRED_VERSION
           GST_API_VERSION=0.10
           ;;
     0.11) GSTREAMER_REQUIRED_VERSION=GSTREAMER_0_11_REQUIRED_VERSION
           GSTREAMER_PLUGINS_BASE_REQUIRED_VERSION=GSTREAMER_0_11_PLUGINS_BASE_REQUIRED_VERSION
           GST_API_VERSION=0.11
           ;;
esac

AC_SUBST([GST_API_VERSION])
if test "$GST_API_VERSION" = "0.11"; then
AC_DEFINE([GST_USE_UNSTABLE_API], [1], [Using unstable GStreamer API])
AC_DEFINE([GST_API_VERSION_1],[1], [Using GStreamer 0.11])
fi

# minimum base dependencies
LIBSOUP_REQUIRED_VERSION=2.37.2.1
CAIRO_REQUIRED_VERSION=1.10
FONTCONFIG_REQUIRED_VERSION=2.4
FREETYPE2_REQUIRED_VERSION=9.0
LIBXML_REQUIRED_VERSION=2.6

# minimum GTK+ base dependencies
PANGO_REQUIRED_VERSION=1.12

# optional modules
LIBXSLT_REQUIRED_VERSION=1.1.7
SQLITE_REQUIRED_VERSION=3.0
ENCHANT_REQUIRED_VERSION=0.22
LIBFFTW_REQUIRED_VERSION=3.2.2
CLUTTER_REQUIRED_VERSION=1.8.2
CLUTTER_GTK_REQUIRED_VERSION=1.0.2
ATSPI2_REQUIRED_VERSION=2.2.1

# Available modules
#
# glib - glib and includes gthread
# unicode - check and identify which unicode backend to use
#
# todo: webcore gtk
WEBKIT_CHECK_DEPENDENCIES([glib unicode])

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

# check whether to enable WebGL support
AC_MSG_CHECKING([whether to enable WebGL support])
AC_ARG_ENABLE(webgl,
              AC_HELP_STRING([--enable-webgl], [enable support for WebGL [default=yes]]),
              [], [if test "$with_target" = "x11"; then enable_webgl="yes"; else enable_webgl="no"; fi])
AC_MSG_RESULT([$enable_webgl])

# check whether to enable accelerated compositing support
AC_MSG_CHECKING([whether to enable accelerated compositing support])
AC_ARG_WITH(accelerated_compositing,
            AC_HELP_STRING([--with-accelerated-compositing=@<:@no/opengl/cairo/clutter@:>@],
                           [use accelerated compositing (experimental) [default=no]]),
            [], [with_accelerated_compositing="no"])
AC_MSG_RESULT([$with_accelerated_compositing])

if test "$with_accelerated_compositing" = "opengl" && test "$with_target" != "x11" ; then
    AC_MSG_ERROR([OpenGL accelerated compositing is only available on X11 currently.])
fi

if test "$enable_webgl" = "yes" ||  test "$with_accelerated_compositing" = "opengl" ; then
    AC_CHECK_HEADERS([GL/gl.h], [], AC_MSG_ERROR([OpenGL header not found]))
    AC_CHECK_HEADERS([GL/glx.h], [], AC_MSG_ERROR([GLX header not found]))
    OPENGL_LIBS="-lGL -ldl"
fi
AC_SUBST([OPENGL_LIBS])

if test "$with_accelerated_compositing" = "clutter"; then
   PKG_CHECK_MODULES(CLUTTER, clutter-1.0 >= $CLUTTER_REQUIRED_VERSION)
   PKG_CHECK_MODULES([CLUTTER_GTK], [clutter-gtk-1.0 >= $CLUTTER_GTK_REQUIRED_VERSION])

   AC_SUBST(CLUTTER_CFLAGS)
   AC_SUBST(CLUTTER_LIBS)
   AC_SUBST(CLUTTER_GTK_CFLAGS)
   AC_SUBST(CLUTTER_GTK_LIBS)
fi

# check whether to enable channel messaging support
AC_MSG_CHECKING([whether to enable HTML5 Channel Messaging support])
AC_ARG_ENABLE(channel_messaging,
              AC_HELP_STRING([--enable-channel-messaging],
                             [enable HTML5 channel messaging support [default=yes]]),
              [],[enable_channel_messaging="yes"])
AC_MSG_RESULT([$enable_channel_messaging])

# check whether to enable notifications
AC_MSG_CHECKING([whether to enable notifications])
AC_ARG_ENABLE(notifications,
              AC_HELP_STRING([--enable-notifications],
                             [enable notifications [default=no]]),
              [],[enable_notifications="no"])
AC_MSG_RESULT([$enable_notifications])

# check whether to enable the meter tag
AC_MSG_CHECKING([whether to enable HTML5 meter tag])
AC_ARG_ENABLE(meter_tag,
              AC_HELP_STRING([--enable-meter-tag],
                             [enable HTML5 meter [default=yes]]),
              [],[enable_meter_tag="yes"])
AC_MSG_RESULT([$enable_meter_tag])

# check whether to enable HTML5 Microdata support
AC_MSG_CHECKING([whether to enable HTML5 Microdata support])
AC_ARG_ENABLE(microdata,
              AC_HELP_STRING([--enable-microdata],
                             [enable HTML5 microdata support [default=no]]),
              [],[enable_microdata="no"])
AC_MSG_RESULT([$enable_microdata])

# check whether to enable page visibility API.
AC_MSG_CHECKING([whether to enable Page Visibility API support])
AC_ARG_ENABLE(page_visibility_api,
              AC_HELP_STRING([--enable-page-visibility-api],
                             [enable page visibility api[default=no]]),
              [],[enable_page_visibility_api="no"])
AC_MSG_RESULT([$enable_page_visibility_api])

# check whether to enable the progress tag
AC_MSG_CHECKING([whether to enable HTML5 progress tag])
AC_ARG_ENABLE(progress_tag,
              AC_HELP_STRING([--enable-progress-tag],
                             [enable HTML5 progress [default=yes]]),
              [],[enable_progress_tag="yes"])
AC_MSG_RESULT([$enable_progress_tag])

# check whether to enable JavaScript debugger/profiler support
AC_MSG_CHECKING([whether to enable JavaScript debugger/profiler support])
AC_ARG_ENABLE(javascript_debugger,
              AC_HELP_STRING([--enable-javascript-debugger],
                             [enable JavaScript debugger/profiler support [default=yes]]),
              [],[enable_javascript_debugger="yes"])
AC_MSG_RESULT([$enable_javascript_debugger])

# check whether to enable Gamepad support
AC_MSG_CHECKING([whether to enable Gamepad support])
AC_ARG_ENABLE(gamepad,
              AC_HELP_STRING([--enable-gamepad],
                             [enable Gamepad support [default=no]]),
              [],[enable_gamepad="no"])
AC_MSG_RESULT([$enable_gamepad])

# check whether to build with datagrid support
AC_MSG_CHECKING([whether to enable HTML5 datagrid support])
AC_ARG_ENABLE(datagrid,
              AC_HELP_STRING([--enable-datagrid],
                             [enable HTML5 datagrid support [default=no]]),
              [],[enable_datagrid="no"])
AC_MSG_RESULT([$enable_datagrid])

# check whether to build with data transfer items support
AC_MSG_CHECKING([whether to enable HTML5 data transfer items support])
AC_ARG_ENABLE(data_transfer_items,
              AC_HELP_STRING([--enable-data-transfer-items],
                             [enable HTML5 data transfer items support [default=no]]),
              [],[enable_data_transfer_items="no"])
AC_MSG_RESULT([$enable_data_transfer_items])

# check whether to enable DOM mutation observer support
AC_MSG_CHECKING([whether to enable DOM mutation observer support])
AC_ARG_ENABLE(mutation_observers,
              AC_HELP_STRING([--enable-mutation-observers],
                             [enable DOM mutation observer support [default=no]]),
              [],[enable_mutation_observers="no"])
AC_MSG_RESULT([$enable_mutation_observers])

# check whether to enable HTML5 client-side session and persitent storage support
AC_MSG_CHECKING([whether to enable HTML5 client-side session and persistent storage support])
AC_ARG_ENABLE(dom_storage,
              AC_HELP_STRING([--enable-dom-storage],
                             [enable HTML5 client-side session and persistent storage support [default=yes]]),
              [],[enable_dom_storage="yes"])
AC_MSG_RESULT([$enable_dom_storage])

# check whether to enable the indexed database API
AC_MSG_CHECKING([whether to enable the indexed database API])
AC_ARG_ENABLE(indexed_database,
              AC_HELP_STRING([--enable-indexed-database],
                             [enable the indexed database API [default=no]]),
              [],[enable_indexed_database="no"])
AC_MSG_RESULT([$enable_indexed_database])

# check whether to enable the color input
AC_MSG_CHECKING([whether to enable the color input])
AC_ARG_ENABLE(input_color,
              AC_HELP_STRING([--enable-input-color],
                             [enable the color input [default=no]]),
              [],[enable_input_color="no"])
AC_MSG_RESULT([$enable_input_color])

# check whether to enable the speech input API
AC_MSG_CHECKING([whether to enable the speech input API])
AC_ARG_ENABLE(input_speech,
              AC_HELP_STRING([--enable-input-speech],
                             [enable the speech input API [default=no]]),
              [],[enable_input_speech="no"])
AC_MSG_RESULT([$enable_input_speech])

# check whether to build with SQL database support
AC_MSG_CHECKING([whether to enable SQL client-side database storage support])
AC_ARG_ENABLE(sql_database,
              AC_HELP_STRING([--enable-sql-database],
                             [enable SQL client-side database storage support [default=yes]]),
              [],[enable_sql_database="yes"])
AC_MSG_RESULT([$enable_sql_database])

# check whether to build with icon database support
AC_MSG_CHECKING([whether to enable icon database support])
AC_ARG_ENABLE(icon_database,
              AC_HELP_STRING([--enable-icon-database],
                             [enable icon database [default=yes]]),
              [],[enable_icon_database="yes"])
AC_MSG_RESULT([$enable_icon_database])

# check whether to build with image resizer API support
AC_MSG_CHECKING([whether to enable image resizer API support])
AC_ARG_ENABLE(image_resizer,
              AC_HELP_STRING([--enable-image-resizer],
                             [enable image resizer [default=no]]),
              [],[enable_image_resizer="no"])
AC_MSG_RESULT([$enable_image_resizer])

# check whether to enable HTML5 datalist support
AC_MSG_CHECKING([whether to enable HTML5 datalist support])
AC_ARG_ENABLE(datalist,
              AC_HELP_STRING([--enable-datalist],
                             [enable HTML5 datalist support [default=yes]]),
              [],[enable_datalist="yes"])
AC_MSG_RESULT([$enable_datalist])

# check whether to enable HTML5 sandbox iframe support
AC_MSG_CHECKING([whether to enable HTML5 sandboxed iframe support])
AC_ARG_ENABLE(sandbox,
              AC_HELP_STRING([--enable-sandbox],
                             [enable HTML5 sandboxed iframe support [default=yes]]),
              [],[enable_sandbox="yes"])
AC_MSG_RESULT([$enable_sandbox])

# check whether to enable HTML5 audio/video support
AC_MSG_CHECKING([whether to enable HTML5 video support])
AC_ARG_ENABLE(video,
              AC_HELP_STRING([--enable-video],
                             [enable HTML5 video support [default=yes]]),
              [],[enable_video="yes"])
AC_MSG_RESULT([$enable_video])

# turn off video features if --disable-video is requested
if test "$enable_video" = "no"; then
   enable_video_track=no
fi

# check whether to enable HTML5 video track support
AC_MSG_CHECKING([whether to enable HTML5 video track support])
AC_ARG_ENABLE(video_track,
              AC_HELP_STRING([--enable-video-track],
                             [enable HTML5 video track support [default=no]]),
              [],[enable_video_track="no"])
AC_MSG_RESULT([$enable_video_track])

# check whether to enable media source support
AC_MSG_CHECKING([whether to enable media source support])
AC_ARG_ENABLE(media_source,
              AC_HELP_STRING([--enable-media-source],
                             [enable support for media source [default=no]]),
              [], [enable_media_source="no"])
AC_MSG_RESULT([$enable_media_source])

# check whether to enable media statistics support
AC_MSG_CHECKING([whether to enable media statistics support])
AC_ARG_ENABLE(media_statistics,
              AC_HELP_STRING([--enable-media-statistics],
                             [enable support for media statistics [default=no]]),
              [], [enable_media_statistics="no"])
AC_MSG_RESULT([$enable_media_statistics])

# check whether to enable Javascript Fullscreen API support
AC_MSG_CHECKING([whether to enable Fullscreen API support])
AC_ARG_ENABLE(fullscreen_api,
              AC_HELP_STRING([--enable-fullscreen-api],
                             [enable the Fullscreen API support [default=yes]]),
              [],[enable_fullscreen_api="yes"])
AC_MSG_RESULT([$enable_fullscreen_api])

# check whether to enable media stream support
AC_MSG_CHECKING([whether to enable media stream support])
AC_ARG_ENABLE(media_stream,
              AC_HELP_STRING([--enable-media-stream],
                             [enable media stream support (incomplete) [default=no]]),
              [],[enable_media_stream="no"])
AC_MSG_RESULT([$enable_media_stream])

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
                             [enable support for geolocation [default=no]]),
              [],[enable_geolocation="no"])
AC_MSG_RESULT([$enable_geolocation])

# check whether to enable MathML support
AC_MSG_CHECKING([whether to enable MathML support])
AC_ARG_ENABLE(mathml,
              AC_HELP_STRING([--enable-mathml],
                             [enable support for MathML [default=yes]]),
              [],[enable_mathml="yes"])
AC_MSG_RESULT([$enable_mathml])

# check whether to enable SVG support
AC_MSG_CHECKING([whether to enable SVG support])
AC_ARG_ENABLE(svg,
              AC_HELP_STRING([--enable-svg],
                             [enable support for SVG [default=yes]]),
              [],[enable_svg="yes"])
AC_MSG_RESULT([$enable_svg])

# check whether to enable Shadow DOM support
AC_MSG_CHECKING([whether to enable Shadow DOM support])
AC_ARG_ENABLE(shadow_dom,
              AC_HELP_STRING([--enable-shadow-dom],
                             [enable support for Shadow DOM [default=yes]]),
              [],[enable_shadow_dom="yes"])
AC_MSG_RESULT([$enable_shadow_dom])

# check whether to enable SharedWorkers support
AC_MSG_CHECKING([whether to enable SharedWorkers support])
AC_ARG_ENABLE(shared_workers,
              AC_HELP_STRING([--enable-shared-workers],
                             [enable support for SharedWorkers [default=yes]]),
              [],[enable_shared_workers="yes"])
AC_MSG_RESULT([$enable_shared_workers])

# check whether to enable Web Workers support
AC_MSG_CHECKING([whether to enable Web Workers support])
AC_ARG_ENABLE(workers,
              AC_HELP_STRING([--enable-workers],
                             [enable support for Web Workers [default=yes]]),
              [],[enable_workers="yes"])
AC_MSG_RESULT([$enable_workers])

# check whether to enable directory upload support
AC_MSG_CHECKING([whether to enable directory upload support])
AC_ARG_ENABLE(directory_upload,
              AC_HELP_STRING([--enable-directory-upload],
                             [enable support for directory upload [default=no]]),
              [], [enable_directory_upload="no"])
AC_MSG_RESULT([$enable_directory_upload])

# check whether to enable HTML5 FileSystem API support
AC_MSG_CHECKING([whether to enable HTML5 FileSystem API support])
AC_ARG_ENABLE(file_system,
              AC_HELP_STRING([--enable-file-system],
                             [enable support for HTML5 FileSystem API [default=no]]),
              [], [enable_file_system="no"])
AC_MSG_RESULT([$enable_file_system])

# check whether to enable HTML5 <style scoped> support
AC_MSG_CHECKING([whether to enable HTML5 <style scoped> support])
AC_ARG_ENABLE(style_scoped,
              AC_HELP_STRING([--enable-style-scoped],
                             [enable support for HTML5 <style scoped> [default=no]]),
              [], [enable_style_scoped="no"])
AC_MSG_RESULT([$enable_style_scoped])

# check whether to enable Quota API support
AC_MSG_CHECKING([whether to enable Quota API support])
AC_ARG_ENABLE(quota,
              AC_HELP_STRING([--enable-quota],
                             [enable support for Quota API [default=no]]),
              [], [enable_quota="no"])
AC_MSG_RESULT([$enable_quota])

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
if test "$enable_svg_fonts" = "yes"; then
   svg_flags=yes
   if test "$enable_svg" = "no"; then
       AC_MSG_WARN([SVG feature(s) requested but SVG is disabled.. Enabling SVG support])
       enable_svg=yes
   fi
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

# check whether to enable Web Timing support
AC_MSG_CHECKING([whether to enable Web Timing support])
AC_ARG_ENABLE(web_timing,
              AC_HELP_STRING([--enable-web-timing],
                             [enable support for Web Timing [default=no]]),
              [],[enable_web_timing="no"])
AC_MSG_RESULT([$enable_web_timing])

# check whether to enable Blob support
AC_MSG_CHECKING([whether to enable Blob support])
AC_ARG_ENABLE(blob,
              AC_HELP_STRING([--enable-blob],
                             [enable support for Blob [default=yes]]),
              [],[enable_blob="yes"])
AC_MSG_RESULT([$enable_blob])

# check whether to enable Fast Mobile Scrolling support
AC_MSG_CHECKING([whether to enable Fast Mobile Scrolling])
AC_ARG_ENABLE(fast_mobile_scrolling,
              AC_HELP_STRING([--enable-fast-mobile-scrolling],
                             [enable support for Fast Mobile Scrolling [default=no]]),
              [],[enable_fast_mobile_scrolling="no"])
AC_MSG_RESULT([$enable_fast_mobile_scrolling])

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

# check whether to enable debug symbols
AC_MSG_CHECKING([whether to enable debug symbols])
AC_ARG_ENABLE(debug_symbols,
              AC_HELP_STRING([--enable-debug-symbols],
                             [enable debug symbols default=no, default=yes for debug builds]),
              [],[if test "$enable_debug" = "yes"; then enable_debug_symbols="yes"; else enable_debug_symbols="no"; fi])
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

# Link prefetch
AC_MSG_CHECKING([whether to enable link prefetch support])
AC_ARG_ENABLE([link-prefetch],
  [AS_HELP_STRING([--enable-link-prefetch], [Enable Link prefetch support (default: disabled)])],
  [],[enable_link_prefetch=no])
AC_MSG_RESULT([$enable_link_prefetch])

if test "$enable_link_prefetch" = "yes"; then 
  AC_DEFINE([ENABLE_LINK_PREFETCH], [1], [Define to enable link prefetch support])
fi

# GObject Introspection
AC_MSG_CHECKING([whether to enable GObject introspection support])
AC_ARG_ENABLE([introspection],
  [AS_HELP_STRING([--enable-introspection],[Enable GObject introspection (default: disabled)])],
  [],[enable_introspection=no])
AC_MSG_RESULT([$enable_introspection])

# check whether to enable animation API
AC_MSG_CHECKING([whether to enable Animation API support])
AC_ARG_ENABLE(animation_api,
              AC_HELP_STRING([--enable-animation-api],
                             [enable support for Animation API (experimental) [default=no]]),
              [], [enable_animation_api="no"])
AC_MSG_RESULT([$enable_animation_api])

# check whether to enable requestAnimationFrame support
AC_MSG_CHECKING([whether to enable requestAnimationFrame support])
AC_ARG_ENABLE(request_animation_frame,
              AC_HELP_STRING([--enable-request-animation-frame],
                             [enable support for requestAnimationFrame (experimental) [default=no]]),
              [],[enable_request_animation_frame="no"])
AC_MSG_RESULT([$enable_request_animation_frame])

# check whether to enable touch icon loading 
AC_MSG_CHECKING([whether to enable touch icon loading])
AC_ARG_ENABLE(touch_icon_loading,
              AC_HELP_STRING([--enable-touch-icon-loading],
                             [enable support for loading touch icons [default=no]]),
              [], [enable_touch_icon_loading="no"])
AC_MSG_RESULT([$enable_touch_icon_loading])

# check whether to enable Register Protocol Handler support
AC_MSG_CHECKING([whether to enable Register Protocol Handler])
AC_ARG_ENABLE(register_protocol_handler,
              AC_HELP_STRING([--enable-register-protocol-handler],
                             [enable support for Register Protocol Handler (experimental) [default=no]]),
              [],[enable_register_protocol_handler="no"])
AC_MSG_RESULT([$enable_register_protocol_handler])

# check whether to enable DeviceOrientation support
AC_MSG_CHECKING([whether to enable DeviceOrientation])
AC_ARG_ENABLE(device_orientation,
              AC_HELP_STRING([--enable-device-orientation],
                             [enable support for DeviceOrientation (experimental and incomplete) [default=no]]),
              [],[enable_device_orientation="no"])
AC_MSG_RESULT([$enable_device_orientation])

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
if test "$enable_debug_symbols" = "yes"; then
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

# check if FreeType/FontConfig are available
if test "$with_font_backend" = "freetype"; then
   if test "$with_target" = "directfb"; then
   PKG_CHECK_MODULES([FREETYPE],
                     [fontconfig >= $FONTCONFIG_REQUIRED_VERSION
                     freetype2 >= $FREETYPE2_REQUIRED_VERSION])
   else
   PKG_CHECK_MODULES([FREETYPE],
                     [cairo-ft
                     fontconfig >= $FONTCONFIG_REQUIRED_VERSION
                     freetype2 >= $FREETYPE2_REQUIRED_VERSION])
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
   PKG_CHECK_MODULES([GSTREAMER],
                     [gstreamer-$GST_API_VERSION >= $GSTREAMER_REQUIRED_VERSION
                     gstreamer-app-$GST_API_VERSION
                     gstreamer-audio-$GST_API_VERSION
                     gstreamer-fft-$GST_API_VERSION
                     gstreamer-base-$GST_API_VERSION
                     gstreamer-interfaces-$GST_API_VERSION
                     gstreamer-pbutils-$GST_API_VERSION
                     gstreamer-plugins-base-$GST_API_VERSION >= $GSTREAMER_PLUGINS_BASE_REQUIRED_VERSION
                     gstreamer-video-$GST_API_VERSION],
                     [have_gstreamer=yes])

   AC_SUBST([GSTREAMER_CFLAGS])
   AC_SUBST([GSTREAMER_LIBS])
fi


if test "$with_accelerated_compositing" = "clutter"; then
   PKG_CHECK_MODULES(CLUTTER, clutter-1.0 >= $CLUTTER_REQUIRED_VERSION)
   PKG_CHECK_MODULES([CLUTTER_GTK],
                     [clutter-gtk-1.0 >= $CLUTTER_GTK_REQUIRED_VERSION])

   AC_SUBST(CLUTTER_CFLAGS)
   AC_SUBST(CLUTTER_LIBS)
   AC_SUBST(CLUTTER_GTK_CFLAGS)
   AC_SUBST(CLUTTER_GTK_LIBS)
fi

# check if atspi2 is available (only needed for Webkit2 unit tests)
if test "$enable_webkit2" = "yes"; then
   PKG_CHECK_MODULES([ATSPI2],
                     [atspi-2 >= $ATSPI2_REQUIRED_VERSION],
                     [have_atspi2=yes], [have_atspi2=no])

   AC_SUBST([ATSPI2_CFLAGS])
   AC_SUBST([ATSPI2_LIBS])
fi

# check for code coverage support
if test "$enable_coverage" = "yes"; then
   COVERAGE_CFLAGS="-MD"
   COVERAGE_LDFLAGS="-ftest-coverage -fprofile-arcs"
   AC_SUBST([COVERAGE_CFLAGS])
   AC_SUBST([COVERAGE_LDFLAGS])
fi

# check for HTML features
if test "$enable_video" = "yes"; then
    html_flags=yes
fi

# WebKit2
AC_MSG_CHECKING([whether to build Webkit2])
AC_ARG_ENABLE(webkit2,
    AC_HELP_STRING([--enable-webkit2], [build webkit2 [default=no]]),
    [], [enable_webkit2="no"])
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
AM_CONDITIONAL([USE_ACCELERATED_COMPOSITING], [test "$with_accelerated_compositing" = "cairo" || test "$with_accelerated_compositing" = "opengl" || test "$with_accelerated_compositing" = "clutter"])
AM_CONDITIONAL([USE_TEXTURE_MAPPER_CAIRO], [test "$with_accelerated_compositing" = "cairo"])
AM_CONDITIONAL([USE_TEXTURE_MAPPER_GL], [test "$with_accelerated_compositing" = "opengl"])
AM_CONDITIONAL([USE_CLUTTER], [test "$with_accelerated_compositing" = "clutter"])

# WebKit feature conditionals
AM_CONDITIONAL([ENABLE_DEBUG],[test "$enable_debug_features" = "yes"])
AM_CONDITIONAL([ENABLE_WEBGL],[test "$enable_webgl" = "yes"])
AM_CONDITIONAL([ENABLE_BLOB],[test "$enable_blob" = "yes"])
AM_CONDITIONAL([ENABLE_METER_TAG],[test "$enable_meter_tag" = "yes"])
AM_CONDITIONAL([ENABLE_MICRODATA],[test "$enable_microdata" = "yes"])
AM_CONDITIONAL([ENABLE_PAGE_VISIBILITY_API],[test "$enable_page_visibility_api" = "yes"])
AM_CONDITIONAL([ENABLE_PROGRESS_TAG],[test "$enable_progress_tag" = "yes"])
AM_CONDITIONAL([ENABLE_CHANNEL_MESSAGING],[test "$enable_channel_messaging" = "yes"])
AM_CONDITIONAL([ENABLE_JAVASCRIPT_DEBUGGER],[test "$enable_javascript_debugger" = "yes"])
AM_CONDITIONAL([ENABLE_GAMEPAD],[test "$enable_gamepad" = "yes"])
AM_CONDITIONAL([ENABLE_DIRECTORY_UPLOAD],[test "$enable_directory_upload" = "yes"])
AM_CONDITIONAL([ENABLE_DATAGRID],[test "$enable_datagrid" = "yes"])
AM_CONDITIONAL([ENABLE_DATA_TRANSFER_ITEMS],[test "$enable_data_transfer_items" = "yes"])
AM_CONDITIONAL([ENABLE_SQL_DATABASE],[test "$enable_sql_database" = "yes"])
AM_CONDITIONAL([ENABLE_DATALIST],[test "$enable_datalist" = "yes"])
AM_CONDITIONAL([ENABLE_DETAILS],[test "$enable_details" = "yes"])
AM_CONDITIONAL([ENABLE_FAST_MOBILE_SCROLLING],[test "$enable_fast_mobile_scrolling" = "yes"])
AM_CONDITIONAL([ENABLE_FILE_SYSTEM],[test "$enable_file_system" = "yes"])
AM_CONDITIONAL([ENABLE_STYLE_SCOPED],[test "$enable_style_scoped" = "yes"])
AM_CONDITIONAL([ENABLE_QUOTA],[test "$enable_quota" = "yes"])
AM_CONDITIONAL([ENABLE_ICONDATABASE],[test "$enable_icon_database" = "yes"])
AM_CONDITIONAL([ENABLE_IMAGE_RESIZER],[test "$enable_image_resizer" = "yes"])
AM_CONDITIONAL([ENABLE_INDEXED_DATABASE],[test "$enable_indexed_database" = "yes"])
AM_CONDITIONAL([ENABLE_INPUT_COLOR],[test "$enable_input_color" = "yes"])
AM_CONDITIONAL([ENABLE_INPUT_SPEECH],[test "$enable_input_speech" = "yes"])
AM_CONDITIONAL([ENABLE_XSLT],[test "$enable_xslt" = "yes"])
AM_CONDITIONAL([ENABLE_FILTERS],[test "$enable_filters" = "yes"])
AM_CONDITIONAL([ENABLE_GEOLOCATION], [test "$enable_geolocation" = "yes"])
AM_CONDITIONAL([ENABLE_MATHML], [test "$enable_mathml" = "yes"])
AM_CONDITIONAL([ENABLE_MHTML], [test "$enable_mhtml" = "yes"])
AM_CONDITIONAL([ENABLE_VIDEO],[test "$enable_video" = "yes"])
AM_CONDITIONAL([ENABLE_MEDIA_SOURCE],[test "$enable_media_source" = "yes"])
AM_CONDITIONAL([ENABLE_MEDIA_STATISTICS],[test "$enable_media_statistics" = "yes"])
AM_CONDITIONAL([ENABLE_VIDEO_TRACK],[test "$enable_video_track" = "yes"])
AM_CONDITIONAL([ENABLE_FULLSCREEN_API],[test "$enable_fullscreen_api" = "yes"])
AM_CONDITIONAL([ENABLE_MEDIA_STREAM],[test "$enable_media_stream" = "yes"])
AM_CONDITIONAL([ENABLE_NOTIFICATIONS],[test "$enable_notifications" = "yes"])
AM_CONDITIONAL([ENABLE_ORIENTATION_EVENTS],[test "$enable_orientation_events" = "yes"])
AM_CONDITIONAL([ENABLE_SVG],[test "$enable_svg" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_FONTS],[test "$enable_svg_fonts" = "yes"])
AM_CONDITIONAL([ENABLE_COVERAGE],[test "$enable_coverage" = "yes"])
AM_CONDITIONAL([ENABLE_FAST_MALLOC],[test "$enable_fast_malloc" = "yes"])
AM_CONDITIONAL([ENABLE_WORKERS],[test "$enable_workers" = "yes"])
AM_CONDITIONAL([ENABLE_SHADOW_DOM],[test "$enable_shadow_dom" = "yes"])
AM_CONDITIONAL([ENABLE_SHARED_WORKERS],[test "$enable_shared_workers" = "yes"])
AM_CONDITIONAL([SVG_FLAGS],[test "$svg_flags" = "yes"])
AM_CONDITIONAL([HTML_FLAGS],[test "$html_flags" = "yes"])
AM_CONDITIONAL([ENABLE_WEB_SOCKETS],[test "$enable_web_sockets" = "yes"])
AM_CONDITIONAL([ENABLE_WEB_AUDIO],[test "$enable_web_audio" = "yes"])
AM_CONDITIONAL([ENABLE_WEB_TIMING],[test "$enable_web_timing" = "yes"])
AM_CONDITIONAL([ENABLE_OPCODE_STATS],[test "$enable_opcode_stats" = "yes"])
AM_CONDITIONAL([ENABLE_WEBKIT2],[test "$enable_webkit2" = "yes"])
AM_CONDITIONAL([ENABLE_PLUGIN_PROCESS],[test "$enable_plugin_process" = "yes"])
AM_CONDITIONAL([ENABLE_SPELLCHECK],[test "$enable_spellcheck" = "yes"])
AM_CONDITIONAL([ENABLE_ANIMATION_API],[test "$enable_animation_api" = "yes"])
AM_CONDITIONAL([ENABLE_REQUEST_ANIMATION_FRAME],[test "$enable_request_animation_frame" = "yes"])
AM_CONDITIONAL([ENABLE_TOUCH_ICON_LOADING],[test "$enable_touch_icon_loading" = "yes"])
AM_CONDITIONAL([ENABLE_REGISTER_PROTOCOL_HANDLER],[test "$enable_register_protocol_handler" = "yes"])
AM_CONDITIONAL([ENABLE_DEVICE_ORIENTATION],[test "$enable_device_orientation" = "yes"])
AM_CONDITIONAL([ENABLE_MUTATION_OBSERVERS],[test "$enable_mutation_observers" = "yes"])

# Gtk conditionals
AM_CONDITIONAL([ENABLE_INTROSPECTION],[test "$enable_introspection" = "yes"])

AC_CONFIG_FILES([
GNUmakefile
])
 

AC_CONFIG_FILES([
Source/WebKit/gtk/webkit/webkitversion.h
])

AC_CONFIG_FILES([
Source/WebKit/gtk/${WEBKITGTK_PC_NAME}-${WEBKITGTK_API_VERSION}.pc:Source/WebKit/gtk/webkit.pc.in
Source/WebKit/gtk/JSCore-${WEBKITGTK_API_VERSION}.gir:Source/WebKit/gtk/JSCore.gir.in
Source/WebKit/gtk/org.webkitgtk-${WEBKITGTK_API_VERSION}.gschema.xml:Source/WebKit/gtk/org.webkitgtk.gschema.xml.in
Source/JavaScriptCore/javascriptcoregtk-${WEBKITGTK_API_VERSION}.pc:Source/JavaScriptCore/javascriptcoregtk.pc.in
]
,[WEBKITGTK_API_VERSION=$WEBKITGTK_API_VERSION,WEBKITGTK_PC_NAME=$WEBKITGTK_PC_NAME]
)


if test "$enable_webkit2" = "yes"; then
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
 Accelerated Compositing                                  : $with_accelerated_compositing

Features:
 WebGL                                                    : $enable_webgl
 Blob support                                             : $enable_blob
 DOM mutation observer support                            : $enable_mutation_observers
 DeviceOrientation support                                : $enable_device_orientation
 Directory upload                                         : $enable_directory_upload
 Fast Mobile Scrolling                                    : $enable_fast_mobile_scrolling
 JIT compilation                                          : $enable_jit
 Filters support                                          : $enable_filters
 Geolocation support                                      : $enable_geolocation
 JavaScript debugger/profiler support                     : $enable_javascript_debugger
 Gamepad support                                          : $enable_gamepad
 MathML support                                           : $enable_mathml
 Media source                                             : $enable_media_source
 Media statistics                                         : $enable_media_statistics
 HTML5 channel messaging support                          : $enable_channel_messaging
 HTML5 meter element support                              : $enable_meter_tag
 HTML5 microdata support                                  : $enable_microdata
 Page Visibility API support                              : $enable_page_visibility_api
 HTML5 progress element support                           : $enable_progress_tag
 HTML5 client-side session and persistent storage support : $enable_dom_storage
 SQL client-side database storage support                 : $enable_sql_database
 HTML5 datagrid support                                   : $enable_datagrid
 HTML5 data transfer items support                        : $enable_data_transfer_items
 HTML5 FileSystem API support                             : $enable_file_system
 Quota API support                                        : $enable_quota
 HTML5 sandboxed iframe support                           : $enable_sandbox
 HTML5 video element support                              : $enable_video
 HTML5 track element support                              : $enable_video_track
 Fullscreen API support                                   : $enable_fullscreen_api
 Media stream support                                     : $enable_media_stream
 Icon database support                                    : $enable_icon_database
 Image resizer support                                    : $enable_image_resizer
 Link prefetch support                                    : $enable_link_prefetch
 Opcode stats                                             : $enable_opcode_stats
 Shadow DOM support                                       : $enable_shadow_dom
 SharedWorkers support                                    : $enable_shared_workers
 Color input support                                      : $enable_input_color
 Speech input support                                     : $enable_input_speech
 SVG support                                              : $enable_svg
 SVG fonts support                                        : $enable_svg_fonts
 Web Audio support                                        : $enable_web_audio
 Web Sockets support                                      : $enable_web_sockets
 Web Timing support                                       : $enable_web_timing
 Web Workers support                                      : $enable_workers
 XSLT support                                             : $enable_xslt
 Spellcheck support                                       : $enable_spellcheck
 Animation API                                            : $enable_animation_api
 RequestAnimationFrame support                            : $enable_request_animation_frame
 Touch Icon Loading support                               : $enable_touch_icon_loading
 Register Protocol Handler support                        : $enable_register_protocol_handler
 WebKit2 support                                          : $enable_webkit2
 WebKit2 plugin process                                   : $enable_plugin_process

GTK+ configuration:
 GTK+ version                                             : $with_gtk
 GDK target                                               : $with_target
 Hildon UI extensions                                     : $with_hildon
 GStreamer version                                        : $with_gstreamer
 Introspection support                                    : $enable_introspection
 Generate documentation                                   : $enable_gtk_doc
"
if test "$with_unicode_backend" = "glib"; then
   echo "     >> WARNING: the glib-based unicode backend is slow and incomplete <<"
   echo
   echo
fi
