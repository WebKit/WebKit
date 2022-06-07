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

PLAYSTATION_COPY_MODULES(WTF
    TARGETS
        ICU
)
