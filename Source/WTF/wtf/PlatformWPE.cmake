list(APPEND WTF_SOURCES
    PlatformUserPreferredLanguagesUnix.cpp
    UniStdExtras.cpp

    generic/MainThreadGeneric.cpp
    generic/WorkQueueGeneric.cpp

    glib/GLibUtilities.cpp
    glib/GRefPtr.cpp
    glib/RunLoopGLib.cpp

    linux/CurrentProcessMemoryStatus.cpp
    linux/MemoryFootprintLinux.cpp
    linux/MemoryPressureHandlerLinux.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/CPUTimeUnix.cpp
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
