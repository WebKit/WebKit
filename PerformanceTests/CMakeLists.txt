if (NOT USE_SYSTEM_MALLOC)
    add_subdirectory(MallocBench)
endif ()

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
