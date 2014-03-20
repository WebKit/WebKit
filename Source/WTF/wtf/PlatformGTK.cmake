list(APPEND WTF_SOURCES
    gobject/GMainLoopSource.cpp
    gobject/GRefPtr.cpp
    gobject/GlibUtilities.cpp

    gtk/MainThreadGtk.cpp
    gtk/RunLoopGtk.cpp
)

list(APPEND WTF_LIBRARIES
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    pthread
    ${ZLIB_LIBRARIES}
)

list(APPEND WTF_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)
