# WPEPlatform Launcher - Minimal WPE WebKit browser using WPEPlatform API only.
# Follows the same pattern as MiniBrowser/wpe/CMakeLists.txt

set(WPEPlatformLauncher_SOURCES
    main.cpp
)

set(WPEPlatformLauncher_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${DERIVED_SOURCES_WPE_API_DIR}
    ${FORWARDING_HEADERS_WPE_DIR}
    ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}
    ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
)

set(WPEPlatformLauncher_PRIVATE_LIBRARIES
    ${JavaScriptCore_LIBRARY_NAME}
    WebKit::WebKit
)

if (ENABLE_WPE_PLATFORM)
    list(APPEND WPEPlatformLauncher_PRIVATE_INCLUDE_DIRECTORIES
        ${WPEPlatform_DERIVED_SOURCES_DIR}
        ${WEBKIT_DIR}/WPEPlatform
    )
endif ()

WEBKIT_EXECUTABLE_DECLARE(WPEPlatformLauncher)
WEBKIT_EXECUTABLE(WPEPlatformLauncher)

install(TARGETS WPEPlatformLauncher DESTINATION "${LIBEXEC_INSTALL_DIR}")
