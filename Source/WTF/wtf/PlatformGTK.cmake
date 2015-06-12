set(WTF_LIBRARY_TYPE STATIC)
set(WTF_OUTPUT_NAME WTFGTK)

list(APPEND WTF_SOURCES
    glib/GLibUtilities.cpp
    glib/GMainLoopSource.cpp
    glib/GRefPtr.cpp
    glib/GThreadSafeMainLoopSource.cpp
    glib/MainThreadGLib.cpp
    glib/RunLoopGLib.cpp
    glib/WorkQueueGLib.cpp
)

list(APPEND WTF_LIBRARIES
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    pthread
    ${ZLIB_LIBRARIES}
)

list(APPEND WTF_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)
