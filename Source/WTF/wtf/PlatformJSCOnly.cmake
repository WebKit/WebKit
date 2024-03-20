list(APPEND WTF_SOURCES
    generic/WorkQueueGeneric.cpp
)

if (WIN32)
    list(APPEND WTF_SOURCES
        text/win/StringWin.cpp
        text/win/TextBreakIteratorInternalICUWin.cpp

        win/CPUTimeWin.cpp
        win/DbgHelperWin.cpp
        win/FileSystemWin.cpp
        win/LanguageWin.cpp
        win/LoggingWin.cpp
        win/MainThreadWin.cpp
        win/OSAllocatorWin.cpp
        win/PathWalker.cpp
        win/SignalsWin.cpp
        win/ThreadingWin.cpp
        win/Win32Handle.cpp
    )
    list(APPEND WTF_PUBLIC_HEADERS
        text/win/WCharStringExtras.h

        win/DbgHelperWin.h
        win/PathWalker.h
    )
    list(APPEND WTF_LIBRARIES
        DbgHelp
        shlwapi
        winmm
    )
else ()
    list(APPEND WTF_SOURCES
        generic/MainThreadGeneric.cpp

        posix/OSAllocatorPOSIX.cpp
        posix/ThreadingPOSIX.cpp

        text/unix/TextBreakIteratorInternalICUUnix.cpp

        unix/LanguageUnix.cpp
        unix/LoggingUnix.cpp
    )
    if (WTF_OS_FUCHSIA)
        list(APPEND WTF_SOURCES
            fuchsia/CPUTimeFuchsia.cpp
        )
    else ()
        list(APPEND WTF_SOURCES
            posix/CPUTimePOSIX.cpp
        )
    endif ()

    if (LOWERCASE_EVENT_LOOP_TYPE STREQUAL "glib")
        list(APPEND WTF_SOURCES
            glib/FileSystemGlib.cpp
        )
    else ()
        list(APPEND WTF_SOURCES
            posix/FileSystemPOSIX.cpp

            unix/UniStdExtrasUnix.cpp
        )
    endif ()

endif ()

if (WIN32)
    list(APPEND WTF_SOURCES
        win/MemoryFootprintWin.cpp
        win/MemoryPressureHandlerWin.cpp
    )
    list(APPEND WTF_PUBLIC_HEADERS
        win/Win32Handle.h
    )
elseif (APPLE)
    file(COPY mac/MachExceptions.defs DESTINATION ${WTF_DERIVED_SOURCES_DIR})
    add_custom_command(
        OUTPUT
            ${WTF_DERIVED_SOURCES_DIR}/MachExceptionsServer.h
            ${WTF_DERIVED_SOURCES_DIR}/mach_exc.h
            ${WTF_DERIVED_SOURCES_DIR}/mach_excServer.c
            ${WTF_DERIVED_SOURCES_DIR}/mach_excUser.c
        MAIN_DEPENDENCY mac/MachExceptions.defs
        WORKING_DIRECTORY ${WTF_DERIVED_SOURCES_DIR}
        COMMAND mig -sheader MachExceptionsServer.h MachExceptions.defs
        VERBATIM)
    list(APPEND WTF_SOURCES
        cocoa/MemoryFootprintCocoa.cpp

        generic/MemoryPressureHandlerGeneric.cpp

        ${WTF_DERIVED_SOURCES_DIR}/mach_excServer.c
        ${WTF_DERIVED_SOURCES_DIR}/mach_excUser.c
    )
    list(APPEND WTF_PUBLIC_HEADERS
        spi/darwin/AbortWithReasonSPI.h
        spi/darwin/ProcessMemoryFootprint.h
    )
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    list(APPEND WTF_SOURCES
        linux/CurrentProcessMemoryStatus.cpp
        linux/MemoryFootprintLinux.cpp
        linux/RealTimeThreads.cpp

        unix/MemoryPressureHandlerUnix.cpp
    )
    list(APPEND WTF_PUBLIC_HEADERS
        linux/ProcessMemoryFootprint.h
        linux/CurrentProcessMemoryStatus.h
    )
elseif (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    list(APPEND WTF_SOURCES
        generic/MemoryFootprintGeneric.cpp

        unix/MemoryPressureHandlerUnix.cpp
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
    list(APPEND WTF_PUBLIC_HEADERS
        glib/GRefPtr.h
        glib/GTypedefs.h
        glib/RunLoopSourcePriority.h
    )

    if (ENABLE_REMOTE_INSPECTOR)
        list(APPEND WTF_SOURCES
            glib/GSocketMonitor.cpp
            glib/SocketConnection.cpp
        )
        list(APPEND WTF_PUBLIC_HEADERS
            glib/GSocketMonitor.h
            glib/GUniquePtr.h
            glib/SocketConnection.h
        )
    endif ()

    list(APPEND WTF_SYSTEM_INCLUDE_DIRECTORIES
        ${GIO_UNIX_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
    )
    list(APPEND WTF_LIBRARIES
        ${GIO_UNIX_LIBRARIES}
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
    Threads::Threads
)
