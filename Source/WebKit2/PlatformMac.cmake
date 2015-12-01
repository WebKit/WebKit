#FIXME: Add Mac-specific sources here.

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT2_DIR})

list(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/cocoa"
    "${WEBCORE_DIR}/platform/mac"
    "${WEBCORE_DIR}/platform/network/cf"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBKIT2_DIR}/UIProcess/mac"
    "${WEBKIT2_DIR}/UIProcess/API/C/mac"
    "${WEBKIT2_DIR}/UIProcess/Cocoa"
    "${WEBKIT2_DIR}/UIProcess/Scrolling"
    "${WEBKIT2_DIR}/Platform/mac"
    "${WEBKIT2_DIR}/Platform/IPC/mac"
    "${WEBKIT2_DIR}/Shared/API/Cocoa"
    "${WEBKIT2_DIR}/Shared/API/c/cf"
    "${WEBKIT2_DIR}/Shared/cf"
    "${WEBKIT2_DIR}/Shared/mac"
    "${WEBKIT2_DIR}/Shared/Plugins/mac"
    "${WEBKIT2_DIR}/WebProcess/Plugins/PDF"
    "${WEBKIT2_DIR}/WebProcess/Scrolling"
    "${WEBKIT2_DIR}/WebProcess/WebPage/mac"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/mac"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebCore"
)

set(WEBKIT2_EXTRA_DEPENDENCIES
     WebKit2-forwarding-headers
)
set(WebProcess_SOURCES
     WebProcess/mac/SecItemShimLibrary.mm
)

list(APPEND NetworkProcess_SOURCES
     ${NetworkProcess_COMMON_SOURCES}
)

add_definitions("-include WebKit2Prefix.h")

set(WebKit2_FORWARDING_HEADERS_FILES
    Shared/API/c/WKDiagnosticLoggingResultType.h

    UIProcess/API/C/WKPageDiagnosticLoggingClient.h
    UIProcess/API/C/WKPageNavigationClient.h
    UIProcess/API/C/WKPageRenderingProgressEvents.h
)

set(WebKit2_FORWARDING_HEADERS_DIRECTORIES
    Shared/API/c

    Shared/API/c/mac

    UIProcess/Cocoa

    UIProcess/API/C

    WebProcess/WebPage

    WebProcess/InjectedBundle/API/c
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebKit FILES ${WebKit2_FORWARDING_HEADERS_FILES} DIRECTORIES ${WebKit2_FORWARDING_HEADERS_DIRECTORIES})
