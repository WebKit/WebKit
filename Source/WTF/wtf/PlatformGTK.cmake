set(WTF_OUTPUT_NAME WTFGTK)

list(APPEND WTF_SOURCES
    generic/MainThreadGeneric.cpp
    generic/WorkQueueGeneric.cpp
    glib/GLibUtilities.cpp
    glib/GRefPtr.cpp
    glib/RunLoopGLib.cpp

    linux/CurrentProcessMemoryStatus.cpp
    linux/MemoryFootprintLinux.cpp
    linux/MemoryPressureHandlerLinux.cpp

    unix/CPUTimeUnix.cpp
    unix/LanguageUnix.cpp

    UniStdExtras.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp
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
