list(APPEND WTF_PUBLIC_HEADERS
    glib/ChassisType.h
    glib/GMutexLocker.h
    glib/GRefPtr.h
    glib/GSocketMonitor.h
    glib/GTypedefs.h
    glib/GUniquePtr.h
    glib/RunLoopSourcePriority.h
    glib/Sandbox.h
    glib/SocketConnection.h
    glib/WTFGType.h

    linux/ProcessMemoryFootprint.h
    linux/CurrentProcessMemoryStatus.h
    linux/RealTimeThreads.h

    unix/UnixFileDescriptor.h
)

list(APPEND WTF_SOURCES
    generic/MainThreadGeneric.cpp
    generic/MemoryFootprintGeneric.cpp
    generic/WorkQueueGeneric.cpp

    glib/ChassisType.cpp
    glib/FileSystemGlib.cpp
    glib/GRefPtr.cpp
    glib/GSocketMonitor.cpp
    glib/RunLoopGLib.cpp
    glib/Sandbox.cpp
    glib/SocketConnection.cpp
    glib/URLGLib.cpp

    linux/CurrentProcessMemoryStatus.cpp
    linux/RealTimeThreads.cpp

    posix/CPUTimePOSIX.cpp
    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/LanguageUnix.cpp
    unix/LoggingUnix.cpp
    unix/MemoryPressureHandlerUnix.cpp
    unix/UniStdExtrasUnix.cpp
)

list(APPEND WTF_LIBRARIES
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    Threads::Threads
    ZLIB::ZLIB
)

if (Journald_FOUND)
    list(APPEND WTF_LIBRARIES Journald::Journald)
endif ()

list(APPEND WTF_SYSTEM_INCLUDE_DIRECTORIES
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)
