if (NOT USE_SYSTEM_MALLOC)
    add_subdirectory(MallocBench)
else ()
    message(WARNING "MallocBench is not available on this platform")
endif ()

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
