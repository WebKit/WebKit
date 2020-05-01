if (NOT TARGET WebKit::WebCore)
    if (NOT INTERNAL_BUILD)
        message(FATAL_ERROR "WebKit::WebCore target not found")
    endif ()

    # This should be moved to an if block if the Apple Mac/iOS build moves completely to CMake
    # Just assuming Windows for the moment
    add_library(WebKit::WebCore STATIC IMPORTED)
    set_target_properties(WebKit::WebCore PROPERTIES
        IMPORTED_LOCATION ${WEBKIT_LIBRARIES_LINK_DIR}/WebCore${DEBUG_SUFFIX}.lib
        # Should add Apple libraries here when https://bugs.webkit.org/show_bug.cgi?id=205085 lands
        INTERFACE_LINK_LIBRARIES "WebKit::WTF;WebKit::JavaScriptCore"
    )
    set(WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR "${CMAKE_BINARY_DIR}/../include/private")
    target_include_directories(WebKit::WebCore INTERFACE
        ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    )
endif ()
