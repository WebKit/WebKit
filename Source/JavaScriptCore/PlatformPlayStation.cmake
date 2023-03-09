include(inspector/remote/Socket.cmake)

# This overrides the default x64 value of 1GB for the memory pool size
list(APPEND JavaScriptCore_PRIVATE_DEFINITIONS
    FIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB=64
)

if (DEVELOPER_MODE)
    add_subdirectory(testmem)
endif ()
