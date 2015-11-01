if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    list(APPEND bmalloc_SOURCES
        bmalloc/Zone.cpp
    )
endif ()
