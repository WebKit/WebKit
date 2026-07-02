include(inspector/remote/Socket.cmake)

# This overrides the default x64 value of 1GB for the memory pool size
list(APPEND JavaScriptCore_PRIVATE_DEFINITIONS
    FIXED_EXECUTABLE_MEMORY_POOL_SIZE_IN_MB=64
)

list(APPEND JavaScriptCore_PRIVATE_INCLUDE_DIRECTORIES ${MEMORY_EXTRA_INCLUDE_DIR})

# PlayStation's libSceIcu_stub_weak.a does not export UCal write/mutation APIs
# (ucal_set, ucal_add, ucal_getMillis, ucal_getTimeZoneTransitionDate, udat_clone, etc.)
# that the Temporal ICU bridge requires. Provide stub implementations so the
# build links successfully; stubs set U_UNSUPPORTED_ERROR so callers degrade gracefully.
list(APPEND JavaScriptCore_SOURCES
    runtime/temporal/core/ICUCalendarStubs.cpp
)

target_link_libraries(LLIntSettingsExtractor PRIVATE ${MEMORY_EXTRA_LIB})
target_link_libraries(LLIntOffsetsExtractor PRIVATE ${MEMORY_EXTRA_LIB})

if (DEVELOPER_MODE)
    add_subdirectory(testmem)
endif ()
