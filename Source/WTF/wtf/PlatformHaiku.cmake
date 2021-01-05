LIST(APPEND WTF_SOURCES
    generic/WorkQueueGeneric.cpp
    generic/MainThreadGeneric.cpp

    unix/MemoryPressureHandlerUnix.cpp
    unix/CPUTimeUnix.cpp

    posix/FileSystemPOSIX.cpp
    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    haiku/RunLoopHaiku.cpp
    haiku/CurrentProcessMemoryStatus.cpp
    haiku/MemoryFootprintHaiku.cpp
    haiku/FileSystemHaiku.cpp

    unicode/icu/CollatorICU.cpp

    PlatformUserPreferredLanguagesHaiku.cpp

    text/haiku/TextBreakIteratorInternalICUHaiku.cpp

)

LIST(APPEND WTF_LIBRARIES
    ${ZLIB_LIBRARIES}
    be execinfo
)

list(APPEND WTF_INCLUDE_DIRECTORIES
	/system/develop/headers/private/system/arch/$ENV{BE_HOST_CPU}/
	/system/develop/headers/private/system
)

add_definitions(-D_BSD_SOURCE=1)
