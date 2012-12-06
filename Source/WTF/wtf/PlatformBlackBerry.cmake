list(APPEND WTF_SOURCES
    OSAllocatorPosix.cpp
    TCSystemAlloc.cpp
    ThreadIdentifierDataPthreads.cpp
    ThreadingPthreads.cpp
    blackberry/MainThreadBlackBerry.cpp
    unicode/icu/CollatorICU.cpp
)

list(INSERT WTF_INCLUDE_DIRECTORIES 0
    "${BLACKBERRY_THIRD_PARTY_DIR}/icu"
)
