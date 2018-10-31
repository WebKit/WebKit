if (${WTF_LIBRARY_TYPE} STREQUAL "STATIC")
    add_definitions(-DSTATICALLY_LINKED_WITH_WTF)
endif ()

# This overrides the default x64 value of 1GB for the memory pool size
add_definitions(-DFIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB=64)
