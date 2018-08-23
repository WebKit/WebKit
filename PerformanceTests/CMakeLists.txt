# MallocBench requires malloc_trim for non-Apple platforms
if (NOT USE_SYSTEM_MALLOC AND (APPLE OR HAVE_MALLOC_TRIM))
    add_subdirectory(MallocBench)
endif ()

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
