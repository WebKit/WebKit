#FIXME: Add Mac-specific sources here.

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT2_DIR})

list(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/mac"
    "${WEBCORE_DIR}/platform/network/cf"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBKIT2_DIR}/UIProcess/mac"
    "${WEBKIT2_DIR}/Platform/mac"
    "${WEBKIT2_DIR}/Platform/IPC/mac"
    "${WEBKIT2_DIR}/Shared/API/Cocoa"
    "${WEBKIT2_DIR}/Shared/cf"
    "${WEBKIT2_DIR}/Shared/mac"
    "${WEBKIT2_DIR}/Shared/Plugins/mac"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
)

set(WEBKIT2_EXTRA_DEPENDENCIES
     WebKit2-forwarding-headers
)
set(WebProcess_SOURCES
     WebProcess/mac/SecItemShimLibrary.mm
)
