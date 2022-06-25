if (NOT TARGET WebKit::JavaScriptCore)
    if (NOT INTERNAL_BUILD)
        message(FATAL_ERROR "WebKit::JavaScriptCore target not found")
    endif ()

    # This should be moved to an if block if the Apple Mac/iOS build moves completely to CMake
    # Just assuming Windows for the moment
    add_library(WebKit::JavaScriptCore SHARED IMPORTED)
    set_target_properties(WebKit::JavaScriptCore PROPERTIES
        IMPORTED_LOCATION ${WEBKIT_LIBRARIES_RUNTIME_DIR}/JavaScriptCore${DEBUG_SUFFIX}.dll
        IMPORTED_IMPLIB ${WEBKIT_LIBRARIES_LINK_DIR}/JavaScriptCore${DEBUG_SUFFIX}.lib
        # Should add Apple::CoreFoundation here when https://bugs.webkit.org/show_bug.cgi?id=205085 lands
        INTERFACE_LINK_LIBRARIES "WebKit::WTF;ICU::data;ICU::i18n;ICU::uc"
    )
    set(JavaScriptCore_FRAMEWORK_HEADERS_DIR "${CMAKE_BINARY_DIR}/../include/private/JavaScriptCore")
    set(JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR ${JavaScriptCore_FRAMEWORK_HEADERS_DIR})

    target_include_directories(WebKit::JavaScriptCore INTERFACE
        ${JavaScriptCore_FRAMEWORK_HEADERS_DIR}
        ${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    )
endif ()
