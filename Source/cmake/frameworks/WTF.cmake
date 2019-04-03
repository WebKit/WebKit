if (NOT TARGET WTFFramework)
    add_library(WTFFramework INTERFACE)

    if (WIN32 AND INTERNAL_BUILD)
        target_link_libraries(WTFFramework INTERFACE WTF${DEBUG_SUFFIX})
    else ()
        target_include_directories(WTFFramework INTERFACE
            ${WTF_INCLUDE_DIRECTORIES}
            ${WTF_FRAMEWORK_HEADERS_DIR}
        )
        target_include_directories(WTFFramework SYSTEM INTERFACE ${WTF_SYSTEM_INCLUDE_DIRECTORIES})
        target_link_libraries(WTFFramework INTERFACE WTF ${WTF_LIBRARIES})
        add_dependencies(WTFFramework WTFFrameworkHeaders)
    endif ()
endif ()
