add_definitions(-DBPLATFORM_MAC=1)

list(APPEND bmalloc_SOURCES
    bmalloc/IsoHeap.cpp
    bmalloc/ProcessCheck.mm
)
