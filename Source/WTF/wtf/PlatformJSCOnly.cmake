list(APPEND WTF_SOURCES
    generic/WorkQueueGeneric.cpp
)

if (WIN32)
    list(APPEND WTF_SOURCES
        text/win/StringWin.cpp
        text/win/TextBreakIteratorInternalICUWin.cpp

        win/CPUTimeWin.cpp
        win/DbgHelperWin.cpp
        win/FileHandleWin.cpp
        win/FileSystemWin.cpp
        win/LanguageWin.cpp
        win/LoggingWin.cpp
        win/MainThreadWin.cpp
        win/MappedFileDataWin.cpp
        win/OSAllocatorWin.cpp
        win/PathWalker.cpp
        win/SignalsWin.cpp
        win/ThreadingWin.cpp
        win/WTFCRTDebug.cpp
        win/Win32Handle.cpp
    )
    list(APPEND WTF_LIBRARIES
        DbgHelp
        shlwapi
        synchronization
        winmm
    )
    list(APPEND WTF_PUBLIC_HEADERS
        win/WTFCRTDebug.h
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
            glib/Sandbox.cpp
        )
    endif ()

    list(APPEND WTF_SOURCES
        posix/FileHandlePOSIX.cpp
        posix/FileSystemPOSIX.cpp
        posix/MappedFileDataPOSIX.cpp

        unix/UniStdExtrasUnix.cpp
    )
endif ()

if (WIN32)
    list(APPEND WTF_SOURCES
        win/MemoryFootprintWin.cpp
        win/MemoryPressureHandlerWin.cpp
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
        COMMAND mig -DMACH_EXC_SERVER_TASKIDTOKEN_STATE -sheader MachExceptionsServer.h MachExceptions.defs
        VERBATIM)
    list(APPEND WTF_SOURCES
        cocoa/MemoryFootprintCocoa.cpp

        generic/MemoryPressureHandlerGeneric.cpp

        ${WTF_DERIVED_SOURCES_DIR}/mach_excServer.c
        ${WTF_DERIVED_SOURCES_DIR}/mach_excUser.c
    )
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    list(APPEND WTF_SOURCES
        linux/CurrentProcessMemoryStatus.cpp
        linux/MemoryFootprintLinux.cpp
        linux/RealTimeThreads.cpp

        unix/MemoryPressureHandlerUnix.cpp
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
    list(APPEND WTF_PUBLIC_HEADERS
        glib/GRefPtr.h
        glib/GTypedefs.h
        glib/RunLoopSourcePriority.h
    )
    list(APPEND WTF_SOURCES
        glib/GRefPtr.cpp
        glib/RunLoopGLib.cpp
    )
    if (ENABLE_REMOTE_INSPECTOR)
        list(APPEND WTF_PUBLIC_HEADERS
            glib/GSocketMonitor.h
            glib/GSpanExtras.h
            glib/GUniquePtr.h
            glib/SocketConnection.h
        )
        list(APPEND WTF_SOURCES
            glib/GSocketMonitor.cpp
            glib/GSpanExtras.cpp
            glib/SocketConnection.cpp
        )
    endif ()
    if (ENABLE_JSC_GLIB_API)
        list(APPEND WTF_PUBLIC_HEADERS
            glib/GUniquePtr.h
            glib/GWeakPtr.h
            glib/WTFGType.h
        )
    endif ()
    list(APPEND WTF_LIBRARIES
        GLib::GioUnix
    )
else ()
    list(APPEND WTF_SOURCES
        generic/RunLoopGeneric.cpp
    )
endif ()

list(APPEND WTF_LIBRARIES
    Threads::Threads
)

if (USE_LIBBACKTRACE)
    list(APPEND WTF_LIBRARIES
        LIBBACKTRACE::LIBBACKTRACE
    )
endif ()
