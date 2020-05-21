list(APPEND WTF_SOURCES
    generic/MainThreadGeneric.cpp
    generic/MemoryFootprintGeneric.cpp
    generic/RunLoopGeneric.cpp
    generic/WorkQueueGeneric.cpp

    playstation/LanguagePlayStation.cpp
    playstation/UniStdExtrasPlayStation.cpp

    posix/FileSystemPOSIX.cpp
    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/CPUTimeUnix.cpp
    unix/MemoryPressureHandlerUnix.cpp
)

list(APPEND WTF_LIBRARIES
    ${KERNEL_LIBRARY}
    Threads::Threads
)

PLAYSTATION_COPY_SHARED_LIBRARIES(WTF_CopySharedLibs
    FILES
        ${ICU_LIBRARIES}
)

# bmalloc is compiled as an OBJECT library so it is statically linked
list(APPEND WTF_PRIVATE_DEFINITIONS STATICALLY_LINKED_WITH_bmalloc)
