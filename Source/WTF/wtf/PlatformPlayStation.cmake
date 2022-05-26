list(APPEND WTF_PUBLIC_HEADERS
    unix/UnixFileDescriptor.h
)

list(APPEND WTF_SOURCES
    generic/MainThreadGeneric.cpp
    generic/MemoryFootprintGeneric.cpp
    generic/RunLoopGeneric.cpp
    generic/WorkQueueGeneric.cpp

    playstation/FileSystemPlayStation.cpp
    playstation/LanguagePlayStation.cpp
    playstation/UniStdExtrasPlayStation.cpp

    posix/CPUTimePOSIX.cpp
    posix/FileSystemPOSIX.cpp
    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/LoggingUnix.cpp
    unix/MemoryPressureHandlerUnix.cpp
)

list(APPEND WTF_LIBRARIES
    ${KERNEL_LIBRARY}
    Threads::Threads
)

set(WTF_MODULES ICU)

if (USE_WPE_BACKEND_PLAYSTATION)
    list(APPEND WTF_SOURCES libwpe/FileSystemLibWPE.cpp)
    list(APPEND WTF_LIBRARIES WPE::PlayStation)
    list(APPEND WTF_MODULES WPE)
endif ()

PLAYSTATION_COPY_REQUIREMENTS(WTF_CopySharedLibs ${WTF_MODULES})
