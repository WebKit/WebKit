list(APPEND WTF_SOURCES
    gobject/GOwnPtr.cpp
    gobject/GRefPtr.cpp
    gobject/GlibUtilities.cpp

    gtk/MainThreadGtk.cpp
)

list(APPEND WTF_LIBRARIES
    pthread
    ${GLIB_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

list(APPEND WTF_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)
