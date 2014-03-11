# Configuration flags that are used throughout WebKitGTK+.
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [The gettext catalog name])

if test "$enable_debug" = "no"; then
    AC_DEFINE([NDEBUG], [1], [Define to disable debugging])
fi

if test "$os_win32" = "no"; then
AC_CHECK_HEADERS([pthread.h],
    AC_DEFINE([HAVE_PTHREAD_H],[1],[Define if pthread exists]),
    AC_MSG_ERROR([pthread support is required to build WebKit]))
AC_CHECK_LIB(pthread, pthread_rwlock_init,
    AC_DEFINE([HAVE_PTHREAD_RWLOCK],[1],[Define if pthread rwlock is present]),
    AC_MSG_WARN([pthread rwlock support is not available]))
fi

if test "$GTK_API_VERSION" = "2.0"; then
    AC_DEFINE([GTK_API_VERSION_2], [1], [ ])
else
    AC_DEFINE([GDK_VERSION_MIN_REQUIRED], [GDK_VERSION_3_6], [Minimum GTK/GDK version required])
fi

if test "$enable_webkit2" = "yes"; then
    AC_DEFINE([ENABLE_PLUGIN_PROCESS], [1], [ ])
    if test "$have_gtk_unix_printing" = "yes"; then
        AC_DEFINE([HAVE_GTK_UNIX_PRINTING], [1], [Define if GTK+ UNIX Printing is available])
    fi
fi

if test "$found_geoclue2" = "yes"; then
    AC_DEFINE([WTF_USE_GEOCLUE2], [1], [ ])
fi

if test "$os_win32" = "yes"; then
    AC_DEFINE([XP_WIN], [1], [ ])
    AC_DEFINE([UNICODE], [1], [ ])
    AC_DEFINE([_UNICODE], [1], [ ])
elif test "$enable_x11_target" = "yes" || test "$enable_wayland_target" != "yes"; then
    AC_DEFINE([XP_UNIX], [1], [ ])
fi

if test "$enable_x11_target" = "yes"; then
    AC_DEFINE([MOZ_X11], [1], [ ])
    AC_DEFINE([WTF_PLATFORM_X11], [1], [Define if target is X11])
fi

if test "$enable_wayland_target" = "yes"; then
    AC_DEFINE([WTF_PLATFORM_WAYLAND], [1], [Define if target is Wayland])
fi

if test "$enable_fast_malloc" = "no"; then
    AC_DEFINE([WTF_SYSTEM_MALLOC], [1], [ ])
fi

if test "$enable_opcode_stats" = "yes"; then
    AC_DEFINE([ENABLE_OPCODE_STATS], [1], [Define to enable Opcode statistics])
fi

if test "$enable_video" = "yes" || test "$enable_web_audio" = "yes"; then
    AC_DEFINE([WTF_USE_GSTREAMER], [1], [ ])
fi

if test "$enable_web_audio" = "yes"; then
    AC_DEFINE([WTF_USE_WEBAUDIO_GSTREAMER], [1], [1])
fi

if test "$enable_accelerated_compositing" = "yes"; then
    AC_DEFINE([WTF_USE_ACCELERATED_COMPOSITING], [1], [ ])
    AC_DEFINE([WTF_USE_TEXTURE_MAPPER], [1], [ ])
    AC_DEFINE([WTF_USE_TEXTURE_MAPPER_GL], [1], [ ])
fi

if test "$found_opengl" = "yes"; then
    AC_DEFINE([WTF_USE_OPENGL], [1], [ ])
fi

if test "$enable_glx" = "yes"; then
    AC_DEFINE([WTF_USE_GLX], [1], [ ])
fi

if test "$enable_egl" = "yes"; then
    AC_DEFINE([WTF_USE_EGL], [1], [ ])
fi

if test "$enable_gles2" = "yes"; then
    AC_DEFINE([WTF_USE_OPENGL_ES_2], [1], [ ])
fi

if test "$enable_spellcheck" = "yes"; then
    AC_DEFINE([ENABLE_SPELLCHECK], [1], [ ])
fi

if test "$enable_credential_storage" = "yes"; then
    AC_DEFINE([ENABLE_CREDENTIAL_STORAGE], [1], [ ])
fi

if test "$enable_jit" = "yes"; then
    AC_DEFINE([ENABLE_JIT], [1], [ ])
elif test "$enable_jit" = "no"; then
    AC_DEFINE([ENABLE_JIT], [0], [ ])
fi

if test "$enable_ftl_jit" = "yes"; then
    AC_DEFINE([ENABLE_FTL_JIT], [1], [ ])
elif test "$enable_ftl_jit" = "no"; then
    AC_DEFINE([ENABLE_FTL_JIT], [0], [ ])
fi

if test "$have_llvm" = "yes"; then
    AC_DEFINE([HAVE_LLVM], [1], [ ])
fi
