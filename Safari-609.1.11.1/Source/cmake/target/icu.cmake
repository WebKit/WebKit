# Apple ports provide their own ICU that can't be found by find_package(ICU).
# This file will create targets that would be created by find_package(ICU).
if (APPLE)
    set(ICU_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/ICU/Headers)

    # Apple just has a single dylib for ICU
    set(ICU_I18N_LIBRARY /usr/lib/libicucore.dylib)
    set(ICU_UC_LIBRARY /usr/lib/libicucore.dylib)
    set(ICU_DATA_LIBRARY /usr/lib/libicucore.dylib)

    set(ICU_LIBRARIES ${ICU_UC_LIBRARY})
elseif (WIN32 AND NOT WTF_PLATFORM_WIN_CAIRO)
    set(ICU_INCLUDE_DIRS ${WEBKIT_LIBRARIES_INCLUDE_DIR})

    set(ICU_I18N_LIBRARY ${WEBKIT_LIBRARIES_LINK_DIR}/libicuin${DEBUG_SUFFIX}.lib)
    set(ICU_UC_LIBRARY ${WEBKIT_LIBRARIES_LINK_DIR}/libicuuc${DEBUG_SUFFIX}.lib)

    # AppleWin does not provide a separate data library
    set(ICU_DATA_LIBRARY ${ICU_UC_LIBRARY})

    set(ICU_LIBRARIES ${ICU_I18N_LIBRARY} ${ICU_UC_LIBRARY})
else ()
    message(FATAL_ERROR "Could not find ICU targets. Use find_package(ICU REQUIRED data i1n8 uc)")
endif ()

# Emulate ICU:: targets
add_library(ICU::data UNKNOWN IMPORTED)
set_target_properties(ICU::data PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIRS}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    IMPORTED_LOCATION "${ICU_DATA_LIBRARY}"
)

add_library(ICU::i18n UNKNOWN IMPORTED)
set_target_properties(ICU::i18n PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIRS}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    IMPORTED_LOCATION "${ICU_I18N_LIBRARY}"
)

add_library(ICU::uc UNKNOWN IMPORTED)
set_target_properties(ICU::uc PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIRS}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    IMPORTED_LOCATION "${ICU_UC_LIBRARY}"
)
