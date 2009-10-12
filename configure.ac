AC_PREREQ(2.59)

m4_define([webkit_major_version], [1])
m4_define([webkit_minor_version], [1])
m4_define([webkit_micro_version], [15])

# This is the version we'll be using as part of our User-Agent string
# e.g., AppleWebKit/$(webkit_user_agent_version) ...
#
# Sourced from WebCore/Configurations/Version.xcconfig
m4_define([webkit_user_agent_major_version], [531])
m4_define([webkit_user_agent_minor_version], [2])

AC_INIT([WebKit],[webkit_major_version.webkit_minor_version.webkit_micro_version],[http://bugs.webkit.org/])

AC_CONFIG_MACRO_DIR([autotools])
AC_CONFIG_AUX_DIR([autotools])
AC_SUBST(ACLOCAL_AMFLAGS, "-I autotools")

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

AC_CONFIG_SRCDIR([WebCore/config.h])

dnl # Libtool library version, not to confuse with API version
dnl # see http://www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html#Libtool-versioning
LIBWEBKITGTK_VERSION=13:0:11
AC_SUBST([LIBWEBKITGTK_VERSION])

AM_INIT_AUTOMAKE([foreign subdir-objects tar-ustar])

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

# pthread (not needed on Windows)
if test "$os_win32" = "no"; then
AC_CHECK_HEADERS([pthread.h],
                 AC_DEFINE([HAVE_PTHREAD_H],[1],[Define if pthread exists]),
                 AC_MSG_ERROR([pthread support is required to build WebKit]))
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
AC_MSG_CHECKING(for libpng12)
if $PKG_CONFIG --exists libpng12 ; then
    AC_MSG_RESULT(yes)
    PNG_LIBS=`$PKG_CONFIG --libs libpng12`
else
  AC_MSG_RESULT(no)
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

# minimum base dependencies
LIBSOUP_REQUIRED_VERSION=2.27.91
CAIRO_REQUIRED_VERSION=1.2
FONTCONFIG_REQUIRED_VERSION=2.4
FREETYPE2_REQUIRED_VERSION=9.0
LIBXML_REQUIRED_VERSION=2.6

# minimum GTK+ base dependencies
PANGO_REQUIRED_VERSION=1.12
GTK_REQUIRED_VERSION=2.10

# optional modules
LIBXSLT_REQUIRED_VERSION=1.1.7
SQLITE_REQUIRED_VERSION=3.0
GSTREAMER_REQUIRED_VERSION=0.10
ENCHANT_REQUIRED_VERSION=0.22
GAIL_REQUIRED_VERSION=1.8

# Available modules
#
# glib - glib and includes gthread
# unicode - check and identify which unicode backend to use
#
# todo: webcore gtk
WEBKIT_CHECK_DEPENDENCIES([glib unicode])

GETTEXT_PACKAGE=$PACKAGE
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

PKG_CHECK_MODULES(ENCHANT, enchant >= $ENCHANT_REQUIRED_VERSION)
AC_SUBST(ENCHANT_CFLAGS)
AC_SUBST(ENCHANT_LIBS)

PKG_CHECK_MODULES(GAIL, gail >= $GAIL_REQUIRED_VERSION)
AC_SUBST(GAIL_CFLAGS)
AC_SUBST(GAIL_LIBS)

# check for target-specific dependencies
if test "$with_target" = "directfb"; then
   PKG_CHECK_MODULES(CAIRO, cairo-directfb >= $CAIRO_REQUIRED_VERSION)
   PKG_CHECK_MODULES(GTK, gtk+-directfb-2.0 >= $GTK_REQUIRED_VERSION)
   AC_DEFINE([WTF_PLATFORM_DIRECTFB],[1],[Define if target is DirectFB])
else
   PKG_CHECK_MODULES(CAIRO, cairo >= $CAIRO_REQUIRED_VERSION)
   PKG_CHECK_MODULES(GTK, gtk+-2.0 >= $GTK_REQUIRED_VERSION)

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

# check whether to enable dashboard support
AC_MSG_CHECKING([whether to enable Dashboard support])
AC_ARG_ENABLE(dashboard_support,
              AC_HELP_STRING([--enable-dashboard-support],
                             [enable Dashboard support [default=yes]]),
              [],[enable_dashboard_support="yes"])
AC_MSG_RESULT([$enable_dashboard_support])

# check whether to enable 3D transforms support
AC_MSG_CHECKING([whether to enable support for 3D Transforms])
AC_ARG_ENABLE(3D_transforms,
              AC_HELP_STRING([--enable-3D-transforms],
                             [enable support for 3D transforms [default=no]]),
              [],[enable_3D_transforms="no"])
AC_MSG_RESULT([$enable_3D_transforms])

# check whether to enable channel messaging support
AC_MSG_CHECKING([whether to enable HTML5 Channel Messaging support])
AC_ARG_ENABLE(channel_messaging,
              AC_HELP_STRING([--enable-channel-messaging],
                             [enable HTML5 channel messaging support [default=yes]]),
              [],[enable_channel_messaging="yes"])
AC_MSG_RESULT([$enable_channel_messaging])

# check whether to enable JavaScript debugger/profiler support
AC_MSG_CHECKING([whether to enable JavaScript debugger/profiler support])
AC_ARG_ENABLE(javascript_debugger,
              AC_HELP_STRING([--enable-javascript-debugger],
                             [enable JavaScript debugger/profiler support [default=yes]]),
              [],[enable_javascript_debugger="yes"])
AC_MSG_RESULT([$enable_javascript_debugger])

# check whether to enable HTML5 Offline Web Applications support
AC_MSG_CHECKING([whether to enable HTML5 offline web applications support])
AC_ARG_ENABLE(offline_web_applications,
              AC_HELP_STRING([--enable-offline-web-applications],
                             [enable HTML5 offline web applications support [default=yes]]),
              [],[enable_offline_web_applications="yes"])
AC_MSG_RESULT([$enable_offline_web_applications])

# check whether to enable HTML5 client-side session and persitent storage support
AC_MSG_CHECKING([whether to enable HTML5 client-side session and persistent storage support])
AC_ARG_ENABLE(dom_storage,
              AC_HELP_STRING([--enable-dom-storage],
                             [enable HTML5 client-side session and persistent storage support [default=yes]]),
              [],[enable_dom_storage="yes"])
AC_MSG_RESULT([$enable_dom_storage])

# check whether to build with database support
AC_MSG_CHECKING([whether to enable HTML5 client-side database storage support])
AC_ARG_ENABLE(database,
              AC_HELP_STRING([--enable-database],
                             [enable HTML5 client-side database storage support [default=yes]]),
              [],[enable_database="yes"])
AC_MSG_RESULT([$enable_database])

# check whether to build with server-sent events support
AC_MSG_CHECKING([whether to enable HTML5 server-sent events support])
AC_ARG_ENABLE(eventsource,
              AC_HELP_STRING([--enable-eventsource],
                             [enable HTML5 server-sent events support [default=yes]]),
              [],[enable_eventsource="yes"])
AC_MSG_RESULT([$enable_eventsource])

# check whether to build with icon database support
AC_MSG_CHECKING([whether to enable icon database support])
AC_ARG_ENABLE(icon_database,
              AC_HELP_STRING([--enable-icon-database],
                             [enable icon database [default=yes]]),
              [],[enable_icon_database="yes"])
AC_MSG_RESULT([$enable_icon_database])

# check whether to enable HTML5 datalist support
AC_MSG_CHECKING([whether to enable HTML5 datalist support])
AC_ARG_ENABLE(datalist,
              AC_HELP_STRING([--enable-datalist],
                             [enable HTML5 datalist support [default=yes]]),
              [],[enable_datalist="yes"])
AC_MSG_RESULT([$enable_datalist])

# check whether to enable HTML5 ruby support
AC_MSG_CHECKING([whether to enable HTML5 ruby support])
AC_ARG_ENABLE(ruby,
              AC_HELP_STRING([--enable-ruby],
                             [enable HTML5 ruby support [default=yes]]),
              [],[enable_ruby="yes"])
AC_MSG_RESULT([$enable_ruby])

# check whether to enable HTML5 audio/video support
AC_MSG_CHECKING([whether to enable HTML5 video support])
AC_ARG_ENABLE(video,
              AC_HELP_STRING([--enable-video],
                             [enable HTML5 video support [default=yes]]),
              [],[enable_video="yes"])
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

# check whether to enable geolocation support
AC_MSG_CHECKING([whether to enable geolocation support])
AC_ARG_ENABLE(geolocation,
              AC_HELP_STRING([--enable-geolocation],
                             [enable support for geolocation [default=no]]),
              [],[enable_geolocation="no"])
AC_MSG_RESULT([$enable_geolocation])

# check whether to enable SVG support
AC_MSG_CHECKING([whether to enable SVG support])
AC_ARG_ENABLE(svg,
              AC_HELP_STRING([--enable-svg],
                             [enable support for SVG [default=yes]]),
              [],[enable_svg="yes"])
AC_MSG_RESULT([$enable_svg])

# check whether to enable WML support
AC_MSG_CHECKING([whether to enable WML support])
AC_ARG_ENABLE(wml,
              AC_HELP_STRING([--enable-wml],
                             [enable support for WML [default=no]]),
              [],[enable_wml="no"])
AC_MSG_RESULT([$enable_wml])

# check whether to enable SharedWorkers support
AC_MSG_CHECKING([whether to enable SharedWorkers support])
AC_ARG_ENABLE(shared_workers,
              AC_HELP_STRING([--enable-shared-workers],
                             [enable support for SharedWorkers [default=no]]),
              [],[enable_shared_workers="no"])
AC_MSG_RESULT([$enable_shared_workers])

# check whether to enable Web Workers support
AC_MSG_CHECKING([whether to enable Web Workers support])
AC_ARG_ENABLE(workers,
              AC_HELP_STRING([--enable-workers],
                             [enable support for Web Workers [default=yes]]),
              [],[enable_workers="yes"])
AC_MSG_RESULT([$enable_workers])

# turn off svg features if --disable-svg is requested
if test "$enable_svg" = "no"; then
   enable_svg_animation=no
   enable_svg_fonts=no
   enable_svg_foreign_object=no
   enable_svg_as_image=no
   enable_svg_use=no
fi

# check whether to enable support for SVG animation
AC_MSG_CHECKING([whether to enable support for SVG animation])
AC_ARG_ENABLE(svg_animation,
              AC_HELP_STRING([--enable-svg-animation],
                             [enable support for SVG animation (experimental) [default=yes]]),
              [],[enable_svg_animation="yes"])
AC_MSG_RESULT([$enable_svg_animation])

# check whether to enable support for filters
AC_MSG_CHECKING([whether to enable support for filters])
AC_ARG_ENABLE(filters,
              AC_HELP_STRING([--enable-filters],
                             [enable support for filters (experimental) [default=no]]),
              [],[enable_filters="no"])
AC_MSG_RESULT([$enable_filters])

# check whether to enable support for SVG fonts
AC_MSG_CHECKING([whether to enable support for SVG fonts])
AC_ARG_ENABLE(svg_fonts,
              AC_HELP_STRING([--enable-svg-fonts],
                             [enable support for SVG fonts (experimental) [default=yes]]),
              [],[enable_svg_fonts="yes"])
AC_MSG_RESULT([$enable_svg_fonts])

# check whether to enable foreign objects support for SVG
AC_MSG_CHECKING([whether to enable support for SVG foreign objects])
AC_ARG_ENABLE(svg_foreign_object,
              AC_HELP_STRING([--enable-svg-foreign-object],
                             [enable support for SVG foreign objects (experimental) [default=yes]]),
              [],[enable_svg_foreign_object="yes"])
AC_MSG_RESULT([$enable_svg_foreign_object])

# check whether to enable SVG As Image support
AC_MSG_CHECKING([whether to enable SVG as Image support])
AC_ARG_ENABLE(svg_as_image,
              AC_HELP_STRING([--enable-svg-as-image],
                             [enable SVG as Image support (experimental) [default=yes]]),
              [],[enable_svg_as_image="yes"])
AC_MSG_RESULT([$enable_svg_as_image])

# check whether to enable SVG USE element support
AC_MSG_CHECKING([whether to enable support for SVG use element])
AC_ARG_ENABLE(svg_use,
              AC_HELP_STRING([--enable-svg-use],
                             [enable SVG use element support (experimental) [default=yes]]),
              [],[enable_svg_use="yes"])
AC_MSG_RESULT([$enable_svg_use])

# check for SVG features, enabling SVG if necessary
if test "$enable_svg_animation" = "yes" || \
   test "$enable_svg_fonts" = "yes" || \
   test "$enable_svg_foreign_object" = "yes" || \
   test "$enable_svg_as_image" = "yes" || \
   test "$enable_svg_use" = "yes"; then
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
              [],[enable_web_sockets="no"])
AC_MSG_RESULT([$enable_web_sockets])

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

AC_MSG_CHECKING([whether to enable JIT compilation])
AC_ARG_ENABLE([jit],
              AC_HELP_STRING([--enable-jit],
                             [Enable JIT compilation default=yes]),
              [],[enable_jit="yes"])
if test "$enable_jit" = "yes"; then
    case "$host_cpu" in
        i*86|x86_64)
            AC_DEFINE([ENABLE_JIT], [1], [Define to enable JIT])
            AC_DEFINE([ENABLE_YARR], [1], [Define to enable YARR])
            AC_DEFINE([ENABLE_YARR_JIT], [1], [Define to enable YARR JIT])
            AC_DEFINE([ENABLE_JIT_OPTIMIZE_CALL], [1], [Define to enable optimizing calls])
            AC_DEFINE([ENABLE_JIT_OPTIMIZE_PROPERTY_ACCESS], [1], [Define to enable optimized property access])
            AC_DEFINE([ENABLE_JIT_OPTIMIZE_ARITHMETIC], [1], [Define to enable optimized arithmetic])
            case "$host_cpu" in
                i*86)
                    AC_DEFINE([WTF_USE_JIT_STUB_ARGUMENT_VA_LIST], [1], [Use stub va_list])
                ;;
                x86_64)
                    AC_DEFINE([WTF_USE_JIT_STUB_ARGUMENT_REGISTER], [1], [Use stub register])
                    AC_DEFINE([WTF_USE_ALTERNATE_JSIMMEDIATE], [1], [Use alternate JSImmediate])
                ;;
            esac
        ;;
        *)
            enable_jit="no (CPU '$host_cpu' not supported)"
        ;;
    esac
fi
AC_MSG_RESULT([$enable_jit])

# GObject Introspection
AC_MSG_CHECKING([whether to enable GObject introspection support])
AC_ARG_ENABLE([introspection],
  [AS_HELP_STRING([--enable-introspection],[Enable GObject introspection (default: disabled)])],
  [],[enable_introspection=no])
AC_MSG_RESULT([$enable_introspection])

G_IR_SCANNER=
G_IR_COMPILER=
G_IR_GENERATE=
GIRDIR=
GIRTYPELIBDIR=

if test "$enable_introspection" = "yes"; then
  GOBJECT_INTROSPECTION_REQUIRED=0.6.2
  PKG_CHECK_MODULES([INTROSPECTION],[gobject-introspection-1.0 >= $GOBJECT_INTROSPECTION_REQUIRED])
  
  G_IR_SCANNER="$($PKG_CONFIG --variable=g_ir_scanner gobject-introspection-1.0)"
  G_IR_COMPILER="$($PKG_CONFIG --variable=g_ir_compiler gobject-introspection-1.0)"
  G_IR_GENERATE="$($PKG_CONFIG --variable=g_ir_generate gobject-introspection-1.0)"
  GIRDIR="$($PKG_CONFIG --variable=girdir gobject-introspection-1.0)"
  GIRTYPELIBDIR="$($PKG_CONFIG --variable=typelibdir gobject-introspection-1.0)"

  AC_DEFINE([ENABLE_INTROSPECTION],[1],[Define to enable GObject introspection support])
fi

AC_SUBST([G_IR_SCANNER])
AC_SUBST([G_IR_COMPILER])
AC_SUBST([G_IR_GENERATE])
AC_SUBST([GIRDIR])
AC_SUBST([GIRTYPELIBDIR])

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

# Add '-g' flag to gcc if it's debug build
if test "$enable_debug" = "yes"; then
   CXXFLAGS="$CXXFLAGS -g"
   CFLAGS="$CFLAGS -g"
else
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

# check if sqlite 3 is available
if test "$enable_icon_database" = "yes" || \
   test "$enable_database" = "yes" || \
   test "$enable_offline_web_applications" = "yes" || \
   test "$enable_dom_storage" = "yes"; then
   PKG_CHECK_MODULES([SQLITE3],
                     [sqlite3 >= $SQLITE_REQUIRED_VERSION],
                     [sqlite3_has_pkg_config=yes],
                     [sqlite3_has_pkg_config=no])
   if test "$sqlite3_has_pkg_config" = "no"; then
      AC_SEARCH_LIBS([sqlite3_open16], [sqlite3],
                     [SQLITE3_LIBS="$LIBS";SQLITE3_CFLAGS="-I $srcdir/WebKitLibraries/WebCoreSQLite3"],
                     [AC_MSG_ERROR([SQLite3 is required to enable Database support])])
   fi
   AC_SUBST([SQLITE3_CFLAGS])
   AC_SUBST([SQLITE3_LIBS])
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

# check if gstreamer is available
if test "$enable_video" = "yes"; then
   PKG_CHECK_MODULES([GSTREAMER],
                     [gstreamer-0.10 >= $GSTREAMER_REQUIRED_VERSION
                     gstreamer-base-0.10
                     gstreamer-pbutils-0.10
                     gstreamer-plugins-base-0.10
                     gstreamer-video-0.10])
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

# check for HTML features
if test "$enable_video" = "yes"; then
    html_flags=yes
fi

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

# WebKit feature conditionals
AM_CONDITIONAL([ENABLE_DEBUG],[test "$enable_debug" = "yes"])
AM_CONDITIONAL([ENABLE_DASHBOARD_SUPPORT],[test "$enable_dashboard_support" = "yes"])
AM_CONDITIONAL([ENABLE_3D_TRANSFORMS],[test "$enable_3D_transforms" = "yes"])
AM_CONDITIONAL([ENABLE_CHANNEL_MESSAGING],[test "$enable_channel_messaging" = "yes"])
AM_CONDITIONAL([ENABLE_JAVASCRIPT_DEBUGGER],[test "$enable_javascript_debugger" = "yes"])
AM_CONDITIONAL([ENABLE_OFFLINE_WEB_APPLICATIONS],[test "$enable_offline_web_applications" = "yes"])
AM_CONDITIONAL([ENABLE_DOM_STORAGE],[test "$enable_dom_storage" = "yes"])
AM_CONDITIONAL([ENABLE_DATABASE],[test "$enable_database" = "yes"])
AM_CONDITIONAL([ENABLE_DATALIST],[test "$enable_datalist" = "yes"])
AM_CONDITIONAL([ENABLE_EVENTSOURCE],[test "$enable_eventsource" = "yes"])
AM_CONDITIONAL([ENABLE_ICONDATABASE],[test "$enable_icon_database" = "yes"])
AM_CONDITIONAL([ENABLE_XPATH],[test "$enable_xpath" = "yes"])
AM_CONDITIONAL([ENABLE_XSLT],[test "$enable_xslt" = "yes"])
AM_CONDITIONAL([ENABLE_FILTERS],[test "$enable_filters" = "yes"])
AM_CONDITIONAL([ENABLE_GEOLOCATION], [test "$enable_geolocation" = "yes"])
AM_CONDITIONAL([ENABLE_RUBY],[test "$enable_ruby" = "yes"])
AM_CONDITIONAL([ENABLE_VIDEO],[test "$enable_video" = "yes"])
AM_CONDITIONAL([ENABLE_NOTIFICATIONS],[test "$enable_notifications" = "yes"])
AM_CONDITIONAL([ENABLE_ORIENTATION_EVENTS],[test "$enable_orientation_events" = "yes"])
AM_CONDITIONAL([ENABLE_SVG],[test "$enable_svg" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_ANIMATION],[test "$enable_svg_animation" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_FONTS],[test "$enable_svg_fonts" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_FOREIGN_OBJECT],[test "$enable_svg_foreign_object" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_AS_IMAGE],[test "$enable_svg_as_image" = "yes"])
AM_CONDITIONAL([ENABLE_SVG_USE],[test "$enable_svg_use" = "yes"])
AM_CONDITIONAL([ENABLE_COVERAGE],[test "$enable_coverage" = "yes"])
AM_CONDITIONAL([ENABLE_FAST_MALLOC],[test "$enable_fast_malloc" = "yes"])
AM_CONDITIONAL([ENABLE_WML],[test "$enable_wml" = "yes"])
AM_CONDITIONAL([ENABLE_WORKERS],[test "$enable_workers" = "yes"])
AM_CONDITIONAL([ENABLE_SHARED_WORKERS],[test "$enable_shared_workers" = "yes"])
AM_CONDITIONAL([SVG_FLAGS],[test "$svg_flags" = "yes"])
AM_CONDITIONAL([HTML_FLAGS],[test "$html_flags" = "yes"])
AM_CONDITIONAL([ENABLE_WEB_SOCKETS],[test "$enable_web_sockets" = "yes"])

# Gtk conditionals
AM_CONDITIONAL([ENABLE_INTROSPECTION],[test "$enable_introspection" = "yes"])


AC_CONFIG_FILES([
GNUmakefile
WebKit/gtk/webkit-1.0.pc:WebKit/gtk/webkit.pc.in
WebKit/gtk/webkit/webkitversion.h
WebKit/gtk/docs/GNUmakefile
WebKit/gtk/docs/version.xml
]
)

AC_OUTPUT

echo "
WebKit was configured with the following options:

Build configuration:
 Enable debugging (slow)                                  : $enable_debug
 Enable GCC build optimization                            : $enable_optimizations
 Code coverage support                                    : $enable_coverage
 Unicode backend                                          : $with_unicode_backend
 Font backend                                             : $with_font_backend
 Optimized memory allocator                               : $enable_fast_malloc
Features:
 3D Transforms                                            : $enable_3D_transforms
 JIT compilation                                          : $enable_jit
 Dashboard support                                        : $enable_dashboard_support
 Filters support                                          : $enable_filters
 Geolocation support                                      : $enable_geolocation
 JavaScript debugger/profiler support                     : $enable_javascript_debugger
 HTML5 offline web applications support                   : $enable_offline_web_applications
 HTML5 channel messaging support                          : $enable_channel_messaging
 HTML5 client-side session and persistent storage support : $enable_dom_storage
 HTML5 client-side database storage support               : $enable_database
 HTML5 ruby support                                       : $enable_ruby
 HTML5 server-sent events support                         : $enable_eventsource
 HTML5 video element support                              : $enable_video
 Icon database support                                    : $enable_icon_database
 SharedWorkers support                                    : $enable_shared_workers
 SVG support                                              : $enable_svg
 SVG animation support                                    : $enable_svg_animation
 SVG fonts support                                        : $enable_svg_fonts
 SVG foreign object support                               : $enable_svg_foreign_object
 SVG as image support                                     : $enable_svg_as_image
 SVG use element support                                  : $enable_svg_use
 WML support                                              : $enable_wml
 Web Sockets support                                      : $enable_web_sockets
 Web Workers support                                      : $enable_workers
 XPATH support                                            : $enable_xpath
 XSLT support                                             : $enable_xslt
GTK+ configuration:
 GDK target                                               : $with_target
 Hildon UI extensions                                     : $with_hildon
 Introspection support                                    : $enable_introspection
"
if test "$with_unicode_backend" = "glib"; then
   echo "     >> WARNING: the glib-based unicode backend is slow and incomplete <<"
   echo
   echo
fi
