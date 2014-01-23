# This is necessary because it is possible to build TestWebKitAPI with WebKit2
# disabled and this triggers the inclusion of the WebKit2 headers.
add_definitions(-DBUILDING_WEBKIT2__)

add_custom_target(forwarding-headersGTKForTestWebKitAPI
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT2_DIR} ${FORWARDING_HEADERS_DIR} gtk
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${TESTWEBKITAPI_DIR} ${FORWARDING_HEADERS_DIR}  gtk
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT2_DIR} ${FORWARDING_HEADERS_DIR} soup
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${TESTWEBKITAPI_DIR} ${FORWARDING_HEADERS_DIR}  soup
)
set(ForwardingHeadersForTestWebKitAPI_NAME forwarding-headersGTKForTestWebKitAPI)

include_directories(
    ${FORWARDING_HEADERS_DIR}
    ${WEBKIT2_DIR}/UIProcess/API/C/soup
    ${WEBKIT2_DIR}/UIProcess/API/C/gtk
    ${WEBKIT2_DIR}/UIProcess/API/gtk
    ${GDK3_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GTK3_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)
set(test_main_SOURCES
    ${TESTWEBKITAPI_DIR}/gtk/main.cpp
)

set(bundle_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/gtk/InjectedBundleControllerGtk.cpp
    ${TESTWEBKITAPI_DIR}/gtk/PlatformUtilitiesGtk.cpp
)

set(webkit2_api_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/gtk/PlatformUtilitiesGtk.cpp
    ${TESTWEBKITAPI_DIR}/gtk/PlatformWebViewGtk.cpp
    ${TESTWEBKITAPI_DIR}/gtk/PlatformWebViewGtk.cpp
)

list(APPEND test_wtf_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

list(APPEND test_webkit2_api_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

list(APPEND test_webcore_LIBRARIES
    WebCorePlatformGTK
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

list(APPEND TestWebKitAPI_LIBRARIES
    ${GDK3_LIBRARIES}
    ${GTK3_LIBRARIES}
)

# The list below works like a test expectation. Tests in the
# test_{webkit2_api|webcore}_BINARIES list are added to the test runner and
# tried on the bots on every build. Tests in test_{webkit2_api|webcore}_BINARIES
# are compiled and suffixed with fail and skipped from the test runner.
#
# Make sure that the tests are passing on both Debug and
# Release builds before adding it to test_{webkit2_api|webcore}_BINARIES.

set(test_webcore_BINARIES
    LayoutUnit
    URL
)

set(test_webkit2_api_BINARIES
    AboutBlankLoad
    CanHandleRequest
    CookieManager
    DocumentStartUserScriptAlertCrash
    DOMWindowExtensionBasic
    DOMWindowExtensionNoCache
    DownloadDecideDestinationCrash
    EvaluateJavaScript
    FailedLoad
    Find
    ForceRepaint
    FrameMIMETypeHTML
    FrameMIMETypePNG
    Geolocation
    GetInjectedBundleInitializationUserDataCallback
    HitTestResultNodeHandle
    InjectedBundleBasic
    InjectedBundleFrameHitTest
    InjectedBundleInitializationUserDataCallbackWins
    LoadAlternateHTMLStringWithNonDirectoryURL
    LoadCanceledNoServerRedirectCallback
    LoadPageOnCrash
    MouseMoveAfterCrash
    NewFirstVisuallyNonEmptyLayout
    NewFirstVisuallyNonEmptyLayoutFails
    NewFirstVisuallyNonEmptyLayoutForImages
    NewFirstVisuallyNonEmptyLayoutFrames
    PageLoadBasic
    PageLoadDidChangeLocationWithinPageForFrame
    ParentFrame
    PreventEmptyUserAgent
    PrivateBrowsingPushStateNoHistoryCallback
    ReloadPageAfterCrash
    ResizeWindowAfterCrash
    RestoreSessionStateContainingFormData
    ShouldGoToBackForwardListItem
    UserMessage
    WillSendSubmitEvent
    WKPageGetScaleFactorNotZero
    WKPreferences
    WKString
    WKStringJSString
    WKURL
)
