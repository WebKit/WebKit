set(MINIBROWSER_DIR "${TOOLS_DIR}/MiniBrowser/mac")

#FIXME: This should not need WEBCORE_EXPORT defined.  This means we are including WebCore headers, and we should not.
add_definitions("-include MiniBrowser_Prefix.pch -DWEBCORE_EXPORT=")

set(MiniBrowser_SOURCES
    ${MINIBROWSER_DIR}/AppDelegate.m
    ${MINIBROWSER_DIR}/BrowserWindowController.m
    ${MINIBROWSER_DIR}/ExtensionManagerWindowController.m
    ${MINIBROWSER_DIR}/SettingsController.m
    ${MINIBROWSER_DIR}/WK1BrowserWindowController.m
    ${MINIBROWSER_DIR}/WK2BrowserWindowController.m
    ${MINIBROWSER_DIR}/main.m
    ${TOOLS_DIR}/MiniBrowser/MBToolbarItem.m
    ${TOOLS_DIR}/MiniBrowser/MiniBrowserWebProcessPlugin.m
)

set(MiniBrowser_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${DERIVED_SOURCES_DIR}
    ${DERIVED_SOURCES_DIR}/ForwardingHeaders
    ${FORWARDING_HEADERS_DIR}
    ${MINIBROWSER_DIR}
)

set(MiniBrowser_LIBRARIES
    WebKit2
)

include_directories(${MiniBrowser_INCLUDE_DIRECTORIES})
add_executable(MiniBrowser ${MiniBrowser_SOURCES})
target_link_libraries(MiniBrowser ${MiniBrowser_LIBRARIES})
