add_custom_target(forwarding-headersEflForTestWebKitAPI
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT2_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include efl
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${TESTWEBKITAPI_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include efl
)
set(ForwardingHeadersForTestWebKitAPI_NAME forwarding-headersEflForTestWebKitAPI)

add_custom_target(forwarding-headersSoupForTestWebKitAPI
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT2_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include soup
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${TESTWEBKITAPI_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include soup
)
set(ForwardingNetworkHeadersForTestWebKitAPI_NAME forwarding-headersSoupForTestWebKitAPI)

include_directories(
    ${WEBKIT2_DIR}/UIProcess/API/C/soup
    ${WEBKIT2_DIR}/UIProcess/API/C/efl
    ${WEBKIT2_DIR}/UIProcess/API/efl
    ${ECORE_INCLUDE_DIRS}
    ${EINA_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

set(test_main_SOURCES
    ${TESTWEBKITAPI_DIR}/efl/main.cpp
)

set(bundle_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/efl/InjectedBundleController.cpp
    ${TESTWEBKITAPI_DIR}/efl/PlatformUtilities.cpp
)

set(webkit2_api_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/efl/PlatformUtilities.cpp
    ${TESTWEBKITAPI_DIR}/efl/PlatformWebView.cpp
)

# The list below works like a test expectation. Tests in the
# test_webkit2_api_BINARIES list are added to the test runner and
# tried on the bots on every build. Tests in test_webkit2_api_fail_BINARIES
# are compiled and suffixed with fail and skipped from the test runner.
#
# Make sure that the tests are passing on both Debug and
# Release builds before adding it to test_webkit2_api_BINARIES.

set(test_webkit2_api_BINARIES
    AboutBlankLoad
    CookieManager
    DOMWindowExtensionNoCache
    DocumentStartUserScriptAlertCrash
    EvaluateJavaScript
    FailedLoad
    Find
    ForceRepaint
    FrameMIMETypeHTML
    FrameMIMETypePNG
    GetInjectedBundleInitializationUserDataCallback
    HitTestResultNodeHandle
    InjectedBundleBasic
    InjectedBundleInitializationUserDataCallbackWins
    LoadAlternateHTMLStringWithNonDirectoryURL
    LoadCanceledNoServerRedirectCallback
    MouseMoveAfterCrash
    NewFirstVisuallyNonEmptyLayout
    NewFirstVisuallyNonEmptyLayoutFails
    PageLoadBasic
    PageLoadDidChangeLocationWithinPageForFrame
    ParentFrame
    PreventEmptyUserAgent
    PrivateBrowsingPushStateNoHistoryCallback
    WKConnection
    WKPreferences
    WKString
    WKStringJSString
    WKURL
    WillSendSubmitEvent
)

set(test_webkit2_api_fail_BINARIES
    CanHandleRequest
    DOMWindowExtensionBasic
    DownloadDecideDestinationCrash
    NewFirstVisuallyNonEmptyLayoutForImages
    NewFirstVisuallyNonEmptyLayoutFrames
    RestoreSessionStateContainingFormData
    ShouldGoToBackForwardListItem
    WKPageGetScaleFactorNotZero
)

add_definitions(-DTHEME_DIR="${THEME_BINARY_DIR}")

# Tests disabled because of missing features on the test harness:
#
#   ResponsivenessTimerDoesntFireEarly
#   SpacebarScrolling
#
# Flaky test, fails on Release but passes on Debug:
#
#   UserMessage
