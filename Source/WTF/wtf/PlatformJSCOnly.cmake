list(APPEND WTF_SOURCES
    generic/WorkQueueGeneric.cpp
)

if (WIN32)
    list(APPEND WTF_SOURCES
        text/win/TextBreakIteratorInternalICUWin.cpp

        win/CPUTimeWin.cpp
        win/LanguageWin.cpp
        win/MainThreadWin.cpp
    )
    list(APPEND WTF_PUBLIC_HEADERS
        text/win/WCharStringExtras.h
    )
else ()
    list(APPEND WTF_SOURCES
        UniStdExtras.cpp

        generic/MainThreadGeneric.cpp

        text/unix/TextBreakIteratorInternalICUUnix.cpp

        unix/LanguageUnix.cpp
    )
    if (WTF_OS_FUCHSIA)
        list(APPEND WTF_SOURCES
            fuchsia/CPUTimeFuchsia.cpp
        )
    else ()
        list(APPEND WTF_SOURCES
            unix/CPUTimeUnix.cpp
        )
    endif ()
endif ()

if (WIN32)
    list(APPEND WTF_SOURCES
        win/MemoryFootprintWin.cpp
    )
elseif (APPLE)
    file(COPY mac/MachExceptions.defs DESTINATION ${DERIVED_SOURCES_WTF_DIR})
    add_custom_command(
        OUTPUT
            ${DERIVED_SOURCES_WTF_DIR}/MachExceptionsServer.h
            ${DERIVED_SOURCES_WTF_DIR}/mach_exc.h
            ${DERIVED_SOURCES_WTF_DIR}/mach_excServer.c
            ${DERIVED_SOURCES_WTF_DIR}/mach_excUser.c
        MAIN_DEPENDENCY mac/MachExceptions.defs
        WORKING_DIRECTORY ${DERIVED_SOURCES_WTF_DIR}
        COMMAND mig -sheader MachExceptionsServer.h MachExceptions.defs
        VERBATIM)
    list(APPEND WTF_SOURCES
        cocoa/MemoryFootprintCocoa.cpp
        ${DERIVED_SOURCES_WTF_DIR}/mach_excServer.c
        ${DERIVED_SOURCES_WTF_DIR}/mach_excUser.c
    )
    list(APPEND WTF_INCLUDE_DIRECTORIES
        ${DERIVED_SOURCES_WTF_DIR}
    )
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    list(APPEND WTF_SOURCES
        linux/CurrentProcessMemoryStatus.cpp
        linux/MemoryFootprintLinux.cpp
        linux/MemoryPressureHandlerLinux.cpp
    )
    list(APPEND WTF_PUBLIC_HEADERS
        linux/CurrentProcessMemoryStatus.h
    )
else ()
    list(APPEND WTF_SOURCES
        generic/MemoryFootprintGeneric.cpp
        generic/MemoryPressureHandlerGeneric.cpp
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
