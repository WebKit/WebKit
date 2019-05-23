add_definitions(-DBPLATFORM_MAC=1)

list(APPEND bmalloc_SOURCES
    bmalloc/ProcessCheck.mm
)

list(APPEND bmalloc_PUBLIC_HEADERS
    bmalloc/darwin/BSoftLinking.h
    bmalloc/darwin/MemoryStatusSPI.h
)
