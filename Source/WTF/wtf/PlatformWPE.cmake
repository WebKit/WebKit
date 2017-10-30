list(APPEND WTF_SOURCES
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
    unix/LanguageUnix.cpp
)

list(APPEND WTF_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

list(APPEND WTF_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)
