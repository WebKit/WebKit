if (NOT TARGET WebKit::WTF)
    if (NOT INTERNAL_BUILD)
        message(FATAL_ERROR "WebKit::WTF target not found")
    endif ()

    # This should be moved to an if block if the Apple Mac/iOS build moves completely to CMake
    # Just assuming Windows for the moment
    add_library(WebKit::WTF SHARED IMPORTED)
    set_target_properties(WebKit::WTF PROPERTIES
        IMPORTED_LOCATION ${WEBKIT_LIBRARIES_RUNTIME_DIR}/WTF${DEBUG_SUFFIX}.dll
        IMPORTED_IMPLIB ${WEBKIT_LIBRARIES_LINK_DIR}/WTF${DEBUG_SUFFIX}.lib
        # Should add Apple::CoreFoundation here when https://bugs.webkit.org/show_bug.cgi?id=205085 lands
        INTERFACE_LINK_LIBRARIES "ICU::data;ICU::i18n;ICU::uc"
    )
    set(WTF_FRAMEWORK_HEADERS_DIR "${CMAKE_BINARY_DIR}/../include/private/WTF")
    target_include_directories(WebKit::WTF INTERFACE
        ${WTF_FRAMEWORK_HEADERS_DIR}
    )
endif ()
