set(WTF_OUTPUT_NAME WTFGTK)

list(APPEND WTF_PUBLIC_HEADERS
    glib/ChassisType.h
    glib/GLibUtilities.h
    glib/GMutexLocker.h
    glib/GRefPtr.h
    glib/GSocketMonitor.h
    glib/GTypedefs.h
    glib/GUniquePtr.h
    glib/RunLoopSourcePriority.h
    glib/SocketConnection.h
    glib/WTFGType.h
)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    list(APPEND WTF_PUBLIC_HEADERS
        linux/ProcessMemoryFootprint.h
        linux/CurrentProcessMemoryStatus.h
    )
endif ()

list(APPEND WTF_SOURCES
    generic/MainThreadGeneric.cpp
    generic/WorkQueueGeneric.cpp

    glib/ChassisType.cpp
    glib/FileSystemGlib.cpp
    glib/GLibUtilities.cpp
    glib/GRefPtr.cpp
    glib/GSocketMonitor.cpp
    glib/RunLoopGLib.cpp
    glib/SocketConnection.cpp
    glib/URLGLib.cpp

    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/CPUTimeUnix.cpp
    unix/LanguageUnix.cpp
    unix/UniStdExtrasUnix.cpp
)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    list(APPEND WTF_SOURCES
        linux/CurrentProcessMemoryStatus.cpp
        linux/MemoryFootprintLinux.cpp

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

list(APPEND WTF_LIBRARIES
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    Threads::Threads
    ZLIB::ZLIB
)

if (Systemd_FOUND)
    list(APPEND WTF_LIBRARIES Systemd::Systemd)
endif ()

list(APPEND WTF_SYSTEM_INCLUDE_DIRECTORIES
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)
