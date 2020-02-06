list(APPEND WTF_SOURCES
    generic/MainThreadGeneric.cpp
    generic/MemoryFootprintGeneric.cpp
    generic/MemoryPressureHandlerGeneric.cpp
    generic/RunLoopGeneric.cpp
    generic/WorkQueueGeneric.cpp

    playstation/LanguagePlayStation.cpp
    playstation/UniStdExtrasPlayStation.cpp

    posix/FileSystemPOSIX.cpp
    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/CPUTimeUnix.cpp
)

list(APPEND WTF_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}

    ${C_STD_LIBRARY}
    ${KERNEL_LIBRARY}
)

# bmalloc is compiled as an OBJECT library so it is statically linked
list(APPEND WTF_PRIVATE_DEFINITIONS STATICALLY_LINKED_WITH_bmalloc)
