list(APPEND WTF_SOURCES
    generic/MainThreadGeneric.cpp
    generic/MemoryFootprintGeneric.cpp
    generic/WorkQueueGeneric.cpp

    glib/Application.cpp
    glib/ChassisType.cpp
    glib/FileSystemGlib.cpp
    glib/GRefPtr.cpp
    glib/GSocketMonitor.cpp
    glib/GSpanExtras.cpp
    glib/RunLoopGLib.cpp
    glib/Sandbox.cpp
    glib/SocketConnection.cpp
    glib/URLGLib.cpp

    linux/CurrentProcessMemoryStatus.cpp
    linux/RealTimeThreads.cpp

    posix/CPUTimePOSIX.cpp
    posix/FileSystemPOSIX.cpp
    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/LanguageUnix.cpp
    unix/LoggingUnix.cpp
    unix/MemoryPressureHandlerUnix.cpp
    unix/UniStdExtrasUnix.cpp
)

list(APPEND WTF_PUBLIC_HEADERS
    glib/Application.h
    glib/ChassisType.h
    glib/GMutexLocker.h
    glib/GRefPtr.h
    glib/GSocketMonitor.h
    glib/GSpanExtras.h
    glib/GThreadSafeWeakPtr.h
    glib/GTypedefs.h
    glib/GUniquePtr.h
    glib/GWeakPtr.h
    glib/RunLoopSourcePriority.h
    glib/Sandbox.h
    glib/SocketConnection.h
    glib/SysprofAnnotator.h
    glib/WTFGType.h

    linux/CurrentProcessMemoryStatus.h
    linux/ProcessMemoryFootprint.h
    linux/RealTimeThreads.h

    unix/UnixFileDescriptor.h
)

list(APPEND WTF_LIBRARIES
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    Threads::Threads
    ZLIB::ZLIB
)

if (ENABLE_JOURNALD_LOG)
    list(APPEND WTF_LIBRARIES Journald::Journald)
endif ()

list(APPEND WTF_SYSTEM_INCLUDE_DIRECTORIES
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

if (USE_LIBBACKTRACE)
    list(APPEND WTF_LIBRARIES
        LIBBACKTRACE::LIBBACKTRACE
    )
endif ()

if (USE_SYSPROF_CAPTURE)
    list(APPEND WTF_LIBRARIES
        SysProfCapture::SysProfCapture
    )
endif ()
