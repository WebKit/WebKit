add_definitions(-DBPLATFORM_MAC=1)

list(APPEND bmalloc_SOURCES
    bmalloc/IsoHeap.cpp
    bmalloc/ProcessCheck.mm
)

list(APPEND bmalloc_PUBLIC_HEADERS
    bmalloc/darwin/MemoryStatusSPI.h
)
