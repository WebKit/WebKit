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
        win/MainThreadWin.cpp
        win/OSAllocatorWin.cpp
        win/PathWalker.cpp
        win/ThreadingWin.cpp
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
        spi/darwin/ProcessMemoryFootprint.h
    )
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    list(APPEND WTF_SOURCES
        linux/CurrentProcessMemoryStatus.cpp
        linux/MemoryFootprintLinux.cpp

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

if (COMPILER_IS_GCC_OR_CLANG)
    # <filesystem> vs <experimental/filesystem>
    set(FILESYSTEM_TEST_SOURCE "
        #include <filesystem>
        int main() { std::filesystem::path p1(\"\"); std::filesystem::status(p1); }
    ")
    set(CMAKE_REQUIRED_FLAGS "--std=c++17")
    check_cxx_source_compiles("${FILESYSTEM_TEST_SOURCE}" STD_FILESYSTEM_IS_AVAILABLE)
    if (NOT STD_FILESYSTEM_IS_AVAILABLE)
        set(EXPERIMENTAL_FILESYSTEM_TEST_SOURCE "
            #include <experimental/filesystem>
            int main() {
                std::experimental::filesystem::path p1(\"//home\");
                std::experimental::filesystem::status(p1);
            }
        ")
        set(CMAKE_REQUIRED_LIBRARIES stdc++fs)
        check_cxx_source_compiles("${EXPERIMENTAL_FILESYSTEM_TEST_SOURCE}" STD_EXPERIMENTAL_FILESYSTEM_IS_AVAILABLE)
        unset(CMAKE_REQUIRED_LIBRARIES)
        if (STD_EXPERIMENTAL_FILESYSTEM_IS_AVAILABLE)
            list(APPEND WTF_LIBRARIES
                stdc++fs
            )
        endif ()
    endif ()
    unset(CMAKE_REQUIRED_FLAGS)
endif ()
