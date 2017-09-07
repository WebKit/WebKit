list(APPEND WTF_SOURCES

    generic/MainThreadGeneric.cpp
    generic/WorkQueueGeneric.cpp
)

if (WIN32)
    list(APPEND WTF_SOURCES
        win/CPUTimeWin.cpp
        win/LanguageWin.cpp
        text/win/TextBreakIteratorInternalICUWin.cpp
    )
else ()
    list(APPEND WTF_SOURCES
        unix/CPUTimeUnix.cpp
        unix/LanguageUnix.cpp
        text/unix/TextBreakIteratorInternalICUUnix.cpp
    )
endif ()

if (WIN32)
    list(APPEND WTF_SOURCES
        win/MemoryFootprintWin.cpp
    )
elseif (APPLE)
    list(APPEND WTF_SOURCES
        cocoa/MemoryFootprintCocoa.cpp
    )
else ()
    list(APPEND WTF_SOURCES
        linux/MemoryFootprintLinux.cpp
    )
endif ()

if (LOWERCASE_EVENT_LOOP_TYPE STREQUAL "glib")
    list(APPEND WTF_SOURCES
        glib/GRefPtr.cpp
        glib/RunLoopGLib.cpp
    )
    list(APPEND WTF_SYSTEM_INCLUDE_DIRECTORIES
        ${GLIB_INCLUDE_DIRS}
    )
    list(APPEND WTF_LIBRARIES
        ${GLIB_GIO_LIBRARIES}
        ${GLIB_GOBJECT_LIBRARIES}
        ${GLIB_LIBRARIES}
    )
else ()
    list(APPEND WTF_SOURCES
        generic/RunLoopGeneric.cpp
    )
endif ()

list(APPEND WTF_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}
)

if (APPLE)
    list(APPEND WTF_INCLUDE_DIRECTORIES
        "${WTF_DIR}/icu"
    )
endif ()
