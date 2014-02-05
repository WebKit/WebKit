AC_MSG_CHECKING([whether to build WebKit1])
AC_ARG_ENABLE(webkit1, 
    AC_HELP_STRING([--enable-webkit1], [build WebKit1 [default=yes]]), 
    [],
    [enable_webkit1="yes"])
AC_MSG_RESULT([$enable_webkit1])

# If you change the default here, please also make sure the assumptions made
# in Tools/Scripts/webkitdirs.pm:buildAutotoolsProject still make sense.
AC_MSG_CHECKING([whether to build WebKit2])
AC_ARG_ENABLE(webkit2,
    AC_HELP_STRING([--enable-webkit2], [build WebKit2 [default=yes]]),
    [],
    [enable_webkit2="yes"])
AC_MSG_RESULT([$enable_webkit2])

AC_MSG_CHECKING([whether to do a debug build])
AC_ARG_ENABLE(debug, 
    AC_HELP_STRING([--enable-debug], [turn on debugging [default=no]]),
    [],[enable_debug="no"])
AC_MSG_RESULT([$enable_debug])

AC_MSG_CHECKING([whether to enable developer mode])
AC_ARG_ENABLE(developer-mode,
    AC_HELP_STRING([--enable-developer-mode], [development-oriented build (no symbols filter and builds testing harness) [default=no (yes for debug builds)]]),
    [enable_developer_mode=$enableval],
    [
        if test "$enable_debug" = "yes"; then
            enable_developer_mode="yes"
        else
            enable_developer_mode="no"
        fi
    ])
AC_MSG_RESULT([$enable_developer_mode])

AC_MSG_CHECKING([whether to enable optimized builds])
AC_ARG_ENABLE(optimizations, 
    AC_HELP_STRING([--enable-optimizations], [turn on build-time optimizations [default=yes]]),
    [enable_optimizations=$enableval],
    [
        if test "$enable_debug" = "yes"; then
            enable_optimizations="no";
        else
            enable_optimizations="yes";
        fi
    ])
AC_MSG_RESULT([$enable_optimizations])

AC_MSG_CHECKING([the GTK+ version to use])
AC_ARG_WITH([gtk], 
    [AS_HELP_STRING([--with-gtk=2.0|3.0], [the GTK+ version to use (default: 3.0)])],
    [
        case "$with_gtk" in
            2.0|3.0) ;;
            *) AC_MSG_ERROR([invalid GTK+ version specified]) ;;
        esac
    ],
    [with_gtk=3.0])
AC_MSG_RESULT([$with_gtk])

default_build_targets="x11=yes wayland=auto win32=no quartz=no directfb=no"
build_targets=""

AC_ARG_ENABLE([x11-target], [AC_HELP_STRING([--enable-x11-target], [enable building for the X11 target [default=yes]])],
    [build_targets="$build_targets x11=$enable_x11_target"], [])
AC_ARG_ENABLE([wayland-target], [AC_HELP_STRING([--enable-wayland-target], [enable building for the Wayland target [default=auto]])],
    [build_targets="$build_targets wayland=$enable_wayland_target"], [])
AC_ARG_ENABLE([win32-target], [AC_HELP_STRING([--enable-win32-target], [enable building for the Win32 target [default=no]])],
    [build_targets="$build_targets win32=$enable_win32_target"], [])
AC_ARG_ENABLE([quartz-target], [AC_HELP_STRING([--enable-quartz-target], [enable building for the Quartz target [default=no]])],
    [build_targets="$build_targets quartz=$enable_quartz_target"], [])
AC_ARG_ENABLE([directfb-target], [AC_HELP_STRING([--enable-directfb-target], [enable building for the DirectFB target [default=no]])],
    [build_targets="$build_targets directfb=$enable_directfb_target"], [])

if test "$build_targets" = ""; then
    build_targets="$default_build_targets"
fi

AM_WEBKIT_DETERMINE_BUILD_TARGET_STATUS([x11], [enable_x11_target], [build_targets])
AM_WEBKIT_DETERMINE_BUILD_TARGET_STATUS([wayland], [enable_wayland_target], [build_targets])
AM_WEBKIT_DETERMINE_BUILD_TARGET_STATUS([win32], [enable_win32_target], [build_targets])
AM_WEBKIT_DETERMINE_BUILD_TARGET_STATUS([quartz], [enable_quartz_target], [build_targets])
AM_WEBKIT_DETERMINE_BUILD_TARGET_STATUS([directfb], [enable_directfb_target], [build_targets])

AC_MSG_CHECKING([whether to build for the Win32 target])
AC_MSG_RESULT([$enable_wayland_target])

AC_MSG_CHECKING([whether to enable spellcheck support])
AC_ARG_ENABLE([spellcheck],
    [AS_HELP_STRING([--enable-spellcheck],[enable support for spellcheck])],
    [],
    [enable_spellcheck="yes"])
AC_MSG_RESULT([$enable_spellcheck])

AC_MSG_CHECKING([whether to enable credential storage])
AC_ARG_ENABLE([credential_storage],
    [AS_HELP_STRING([--enable-credential-storage],[enable support for credential storage using libsecret [default=yes]])],
    [],
    [enable_credential_storage="yes"])
AC_MSG_RESULT([$enable_credential_storage])

AC_ARG_ENABLE(glx, 
    AC_HELP_STRING([--enable-glx], [enable support for GLX [default=auto]]),
    [],
    [enable_glx="auto"])

AC_ARG_ENABLE(egl, 
    AC_HELP_STRING([--enable-egl], [enable support for EGL [default=auto]]),
    [],
    [enable_egl="auto"])
AC_ARG_ENABLE(gles2, AC_HELP_STRING([--enable-gles2], [enable support for OpenGL ES 2 [default=auto]]), [], [enable_gles2="auto"])

AC_MSG_CHECKING([whether to enable Gamepad support])
AC_ARG_ENABLE(gamepad, 
    AC_HELP_STRING([--enable-gamepad], [enable Gamepad support [default=no]]), 
    [],
    [enable_gamepad="no"])
AC_MSG_RESULT([$enable_gamepad])

AC_MSG_CHECKING([whether to enable HTML5 video support])
AC_ARG_ENABLE(video, 
    AC_HELP_STRING([--enable-video], [enable HTML5 video support [default=yes]]),
    [],
    [enable_video="yes"])
AC_MSG_RESULT([$enable_video])

AC_MSG_CHECKING([whether to enable geolocation support])
AC_ARG_ENABLE(geolocation, 
    AC_HELP_STRING([--enable-geolocation], [enable support for geolocation [default=yes]]),
    [],
    [enable_geolocation="yes"])
AC_MSG_RESULT([$enable_geolocation])

AC_MSG_CHECKING([whether to enable support for SVG fonts])
AC_ARG_ENABLE(svg_fonts, 
    AC_HELP_STRING([--enable-svg-fonts], [enable support for SVG fonts (experimental) [default=yes]]),
    [],
    [enable_svg_fonts="yes"])
AC_MSG_RESULT([$enable_svg_fonts])

AC_MSG_CHECKING([whether to enable Web Audio support])
AC_ARG_ENABLE(web_audio, 
    AC_HELP_STRING([--enable-web-audio], [enable support for Web Audio [default=yes]]),
    [],
    [enable_web_audio="yes"])
AC_MSG_RESULT([$enable_web_audio])

AC_MSG_CHECKING([whether to enable Battery Status API support])
AC_ARG_ENABLE(battery_status,
    AC_HELP_STRING([--enable-battery-status], [enable support for Battery Status API [default=no]]),
    [],
    [enable_battery_status="no"])
AC_MSG_RESULT([$enable_battery_status])

AC_MSG_CHECKING([whether to enable code coverage support])
AC_ARG_ENABLE(coverage,
    AC_HELP_STRING([--enable-coverage], [enable code coverage support [default=no]]),
    [],
    [enable_coverage="no"])
AC_MSG_RESULT([$enable_coverage])

AC_MSG_CHECKING([whether to enable optimized memory allocator])
AC_ARG_ENABLE(fast_malloc, 
    AC_HELP_STRING([--enable-fast-malloc], [enable optimized memory allocator default=yes, default=no for debug builds]),
    [],
    [if test "$enable_debug" = "yes"; then
         enable_fast_malloc="no";
     else
         enable_fast_malloc="yes";
     fi])
AC_MSG_RESULT([$enable_fast_malloc])

AC_MSG_CHECKING([whether to enable debug symbols])
AC_ARG_ENABLE(debug_symbols,
    AC_HELP_STRING([--enable-debug-symbols=yes|no|min|full], [enable debug symbols default=no, default=yes for debug builds]),
    [
        case "$enable_debug_symbols" in
            yes) enable_debug_symbols="full" ;;
            no|min|full) ;;
            *) AC_MSG_ERROR([Invalid debug symbols option: must be yes, no, min or full.]) ;;
        esac
    ],
    [
         if test "$enable_debug" = "yes"; then
             enable_debug_symbols="yes";
         else
             enable_debug_symbols="no";
         fi
    ])
AC_MSG_RESULT([$enable_debug_symbols])

AC_MSG_CHECKING([whether to enable WebGL support])
AC_ARG_ENABLE(webgl, AC_HELP_STRING([--enable-webgl], [enable support for WebGL [default=auto]]),
    [],
    [enable_webgl="auto"])
AC_MSG_RESULT([$enable_webgl])

AC_MSG_CHECKING([whether to enable accelerated compositing support])
AC_ARG_ENABLE(accelerated_compositing,
    AC_HELP_STRING([--enable-accelerated-compositing], [enable support for accelerated compositing [default=auto]]),
    [],
    [enable_accelerated_compositing="auto"])
AC_MSG_RESULT([$enable_accelerated_compositing])

AC_MSG_CHECKING([whether to enable JIT compilation])
AC_ARG_ENABLE(jit, AS_HELP_STRING([--enable-jit], [Enable JIT compilation (default: auto)]), [], [enable_jit=auto])
AC_MSG_RESULT([$enable_jit])

AC_MSG_CHECKING([whether to enable FTL JIT])
AC_ARG_ENABLE(ftl-jit, AS_HELP_STRING([--enable-ftl-jit], [Enable FTL JIT (experimental) (default: no)]), [], [enable_ftl_jit=no])
AC_MSG_RESULT([$enable_ftl_jit])

AC_MSG_CHECKING([whether to enable opcode stats])
AC_ARG_ENABLE([opcode-stats], 
    AS_HELP_STRING([--enable-opcode-stats], [Enable Opcode statistics (default: disabled)]),
    [],
    [enable_opcode_stats=no])
AC_MSG_RESULT([$enable_opcode_stats])

AC_MSG_CHECKING([whether to enable GObject introspection support])
AC_ARG_ENABLE([introspection], 
    AS_HELP_STRING([--enable-introspection],[Enable GObject introspection (default: disabled)]),
    [],
    [enable_introspection=no])
AC_MSG_RESULT([$enable_introspection])


