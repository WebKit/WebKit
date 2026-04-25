include(inspector/remote/Socket.cmake)

# This overrides the default x64 value of 1GB for the memory pool size
list(APPEND JavaScriptCore_PRIVATE_DEFINITIONS
    FIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB=64
)

list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES ${MEMORY_EXTRA_INCLUDE_DIR})

target_link_libraries(LLIntSettingsExtractor PRIVATE ${MEMORY_EXTRA_LIB})
target_link_libraries(LLIntOffsetsExtractor PRIVATE ${MEMORY_EXTRA_LIB})

if (DEVELOPER_MODE)
    add_subdirectory(testmem)
endif ()
