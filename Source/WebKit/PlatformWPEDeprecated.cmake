file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_DOM_DIR})

list(APPEND WebKit_UNIFIED_SOURCE_LIST_FILES
    "SourcesWPEDeprecated.txt"
)

list(APPEND WPE_API_HEADER_TEMPLATES
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFaviconDatabase.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitJavascriptResult.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitMimeInfo.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitPlugin.h.in
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${FORWARDING_HEADERS_WPE_DOM_DIR}"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/DOM"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM"
)

list(APPEND WPE_WEB_PROCESS_EXTENSION_API_INSTALLED_HEADERS
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM/webkitdom.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM/WebKitDOMDefines.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM/WebKitDOMDocument.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM/WebKitDOMElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM/WebKitDOMNode.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM/WebKitDOMObject.h
)

set(WPE_DOM_SOURCES_FOR_INTROSPECTION
    WebProcess/InjectedBundle/API/glib/DOM/WebKitDOMNode.cpp
)

list(APPEND WPE_FAKE_API_HEADERS
    ${FORWARDING_HEADERS_WPE_DOM_DIR}/wpe
    ${DERIVED_SOURCES_WPE_API_DIR}/webkit-web-extension.h
)

add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WPE_DOM_DIR}/wpe
    DEPENDS ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM
    COMMAND ln -n -s -f ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM ${FORWARDING_HEADERS_WPE_DOM_DIR}/wpe
    VERBATIM
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WPE_API_DIR}/webkit-web-extension.h
    DEPENDS ${DERIVED_SOURCES_WPE_API_DIR}/webkit-web-process-extension.h
    COMMAND ${CMAKE_COMMAND} -E copy ${DERIVED_SOURCES_WPE_API_DIR}/webkit-web-process-extension.h ${DERIVED_SOURCES_WPE_API_DIR}/webkit-web-extension.h
    VERBATIM
)

install(FILES ${DERIVED_SOURCES_WPE_API_DIR}/webkit-web-extension.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wpe-webkit-${WPE_API_VERSION}/wpe
    COMPONENT "Development"
)
