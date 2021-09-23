include(GtkDoc)
include(WebKitDist)

list(APPEND DocumentationDependencies
    WebKit
    "${CMAKE_SOURCE_DIR}/Source/WebKit/UIProcess/API/wpe/docs/wpe-docs.sgml"
    "${CMAKE_SOURCE_DIR}/Source/WebKit/WebProcess/InjectedBundle/API/wpe/docs/wpe-webextensions-docs.sgml"
    "${CMAKE_SOURCE_DIR}/Source/WebKit/UIProcess/API/wpe/docs/wpe-${WPE_API_DOC_VERSION}-sections.txt"
    "${CMAKE_SOURCE_DIR}/Source/WebKit/WebProcess/InjectedBundle/API/wpe/docs/wpe-webextensions-${WPE_API_DOC_VERSION}-sections.txt"
)
