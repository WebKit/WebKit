
if (APPLE)
    if (USE_APPLE_INTERNAL_SDK)
        set(ICU_INCLUDE_DIRS ${CMAKE_OSX_SYSROOT}/usr/local/include)
    else ()
        set(ICU_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/ICU/Headers)
    endif ()
    # Apple just has a single tbd/dylib for ICU.
    find_library(ICU_I18N_LIBRARY icucore)
    find_library(ICU_UC_LIBRARY icucore)
    find_library(ICU_DATA_LIBRARY icucore)

    add_library(ICU::data UNKNOWN IMPORTED)
    set_target_properties(ICU::data PROPERTIES
        IMPORTED_LOCATION "${ICU_DATA_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIRS}"
        IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    )

    add_library(ICU::i18n UNKNOWN IMPORTED)
    set_target_properties(ICU::i18n PROPERTIES
        IMPORTED_LOCATION "${ICU_I18N_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIRS}"
        IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    )

    add_library(ICU::uc UNKNOWN IMPORTED)
    set_target_properties(ICU::uc PROPERTIES
        IMPORTED_LOCATION "${ICU_UC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIRS}"
        IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    )

    set(ICU_LIBRARIES ${ICU_UC_LIBRARY})
    set(ICU_FOUND ON)
    message(STATUS "Found ICU: ${ICU_LIBRARIES}")
else ()
    # Defer to CMake's built-in FindICU by removing ourselves
    # from the module path for the duration of this call.
    set(_saved_module_path "${CMAKE_MODULE_PATH}")
    list(REMOVE_ITEM CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
    find_package(ICU ${ICU_FIND_VERSION}
        ${ICU_FIND_REQUIRED_arg}
        ${ICU_FIND_QUIETLY_arg}
        COMPONENTS ${ICU_FIND_COMPONENTS})
    set(CMAKE_MODULE_PATH "${_saved_module_path}")
endif ()
