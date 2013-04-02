# Configuration flags that are used throughout WebKitGTK+.
AC_DEFINE([WTF_USE_GLIB], [1], [ ])
AC_DEFINE([WTF_USE_FREETYPE], [1], [ ])
AC_DEFINE([WTF_USE_HARFBUZZ], [1], [ ])
AC_DEFINE([WTF_USE_SOUP], [1], [ ])
AC_DEFINE([WTF_USE_WEBP], [1], [ ])
AC_DEFINE([WTF_USE_ICU_UNICODE], [1], [ ])
AC_DEFINE([GST_API_VERSION_1], [1], [Using GStreamer 1.0])
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [The gettext catalog name])

if test "$enable_debug" = "yes"; then
    AC_DEFINE([GDK_PIXBUF_DISABLE_DEPRECATED], [1], [ ])
    AC_DEFINE([GDK_DISABLE_DEPRECATED], [1], [ ])
    AC_DEFINE([GTK_DISABLE_DEPRECATED], [1], [ ])
    AC_DEFINE([PANGO_DISABLE_DEPRECATED], [1], [ ])
else
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
fi

if test "$enable_webkit2" = "yes"; then
    AC_DEFINE([ENABLE_PLUGIN_PROCESS], [1], [ ])
    if test "$have_gtk_unix_printing" = "yes"; then
        AC_DEFINE([HAVE_GTK_UNIX_PRINTING], [1], [Define if GTK+ UNIX Printing is available])
    fi
fi

if test "$os_win32" = "yes"; then
    AC_DEFINE([XP_WIN], [1], [ ])
    AC_DEFINE([UNICODE], [1], [ ])
    AC_DEFINE([_UNICODE], [1], [ ])
else
    AC_DEFINE([XP_UNIX], [1], [ ])
fi

if test "$with_target" = "x11"; then
    AC_DEFINE([MOZ_X11], [1], [ ])
    AC_DEFINE([WTF_PLATFORM_X11], [1], [Define if target is X11])
fi

if test "$enable_fast_malloc" = "no"; then
    AC_DEFINE([WTF_SYSTEM_MALLOC], [1], [ ])
fi

if test "$enable_opcode_stats" = "yes"; then
    AC_DEFINE([ENABLE_OPCODE_STATS], [1], [Define to enable Opcode statistics])
fi

if test "$enable_introspection" = "yes"; then
    AC_DEFINE([ENABLE_INTROSPECTION], [1], [Define to enable GObject introspection support])
fi

if test "$enable_video" = "yes" || test "$enable_web_audio" = "yes"; then
    AC_DEFINE([WTF_USE_GSTREAMER], [1], [ ])
    if test "$enable_debug" = "yes"; then
        AC_DEFINE([GST_DISABLE_DEPRECATED], [1], [ ])
    fi

    if test "$enable_video" = "yes"; then
        AC_DEFINE([WTF_USE_NATIVE_FULLSCREEN_VIDEO], [1], [ ])
    fi
fi

if test "$enable_web_audio" = "yes"; then
    AC_DEFINE([WTF_USE_WEBAUDIO_GSTREAMER], [1], [1])
fi

if test "$enable_accelerated_compositing" = "yes"; then
    AC_DEFINE([WTF_USE_ACCELERATED_COMPOSITING], [1], [ ])

    if test "$with_acceleration_backend" = "none"; then
        AC_DEFINE([WTF_USE_TEXTURE_MAPPER], [1], [ ])
        AC_DEFINE([WTF_USE_TEXTURE_MAPPER_CAIRO], [1], [ ])
    fi

    if test "$with_acceleration_backend" = "opengl"; then
        AC_DEFINE([WTF_USE_TEXTURE_MAPPER], [1], [ ])
        AC_DEFINE([WTF_USE_TEXTURE_MAPPER_GL], [1], [ ])
    fi

    if test "$with_acceleration_backend" = "clutter"; then
        AC_DEFINE([WTF_USE_CLUTTER], [1], [ ])
    fi
fi

if test "$with_acceleration_backend" = "opengl"; then
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
    AC_DEFINE([WTF_ENABLE_CREDENTIAL_STORAGE], [1], [ ])
fi

