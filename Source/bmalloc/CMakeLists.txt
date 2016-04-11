set_property(DIRECTORY . PROPERTY FOLDER "bmalloc")

set(bmalloc_INCLUDE_DIRECTORIES
    "${BMALLOC_DIR}"
)

set(bmalloc_SOURCES
    bmalloc/Allocator.cpp
    bmalloc/Cache.cpp
    bmalloc/Deallocator.cpp
    bmalloc/Environment.cpp
    bmalloc/FreeList.cpp
    bmalloc/Heap.cpp
    bmalloc/Logging.cpp
    bmalloc/ObjectType.cpp
    bmalloc/SegregatedFreeList.cpp
    bmalloc/StaticMutex.cpp
    bmalloc/VMHeap.cpp
    bmalloc/XLargeMap.cpp
    bmalloc/mbmalloc.cpp
)

set(bmalloc_LIBRARIES
    ${CMAKE_DL_LIBS}
)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_WRAP_SOURCELIST(${bmalloc_SOURCES})
include_directories(${bmalloc_INCLUDE_DIRECTORIES})
add_library(bmalloc STATIC ${bmalloc_SOURCES})
target_link_libraries(bmalloc ${bmalloc_LIBRARIES})
set_target_properties(bmalloc PROPERTIES COMPILE_DEFINITIONS "BUILDING_bmalloc")
