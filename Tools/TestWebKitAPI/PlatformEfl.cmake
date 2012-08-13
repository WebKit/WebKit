ADD_CUSTOM_TARGET(forwarding-headersEflForTestWebKitAPI
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${TESTWEBKITAPI_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include efl
)
SET(ForwardingHeadersForTestWebKitAPI_NAME forwarding-headersEflForTestWebKitAPI)

ADD_CUSTOM_TARGET(forwarding-headersSoupForTestWebKitAPI
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${TESTWEBKITAPI_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include soup
)
SET(ForwardingNetworkHeadersForTestWebKitAPI_NAME forwarding-headersSoupForTestWebKitAPI)

INCLUDE_DIRECTORIES(${LIBSOUP_INCLUDE_DIRS}
    ${WEBKIT2_DIR}/UIProcess/API/C/soup
    ${WEBKIT2_DIR}/UIProcess/API/C/efl
    ${ECORE_INCLUDE_DIRS}
    ${EINA_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
)

SET(test_main_SOURCES
    ${TESTWEBKITAPI_DIR}/efl/main.cpp
)

SET(bundle_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/efl/InjectedBundleController.cpp
    ${TESTWEBKITAPI_DIR}/efl/PlatformUtilities.cpp
)

SET(webkit2_api_harness_SOURCES
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

SET(test_webkit2_api_BINARIES
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
    InjectedBundleBasic
    InjectedBundleInitializationUserDataCallbackWins
    LoadAlternateHTMLStringWithNonDirectoryURL
    LoadCanceledNoServerRedirectCallback
    NewFirstVisuallyNonEmptyLayout
    NewFirstVisuallyNonEmptyLayoutFails
    PageLoadBasic
    PageLoadDidChangeLocationWithinPageForFrame
    ParentFrame
    PreventEmptyUserAgent
    PrivateBrowsingPushStateNoHistoryCallback
    WKConnection
    WKString
    WKStringJSString
    WillSendSubmitEvent
)

SET(test_webkit2_api_fail_BINARIES
    CanHandleRequest
    DOMWindowExtensionBasic
    DownloadDecideDestinationCrash
    NewFirstVisuallyNonEmptyLayoutForImages
    NewFirstVisuallyNonEmptyLayoutFrames
    RestoreSessionStateContainingFormData
    ShouldGoToBackForwardListItem
    WKPageGetScaleFactorNotZero
)

# Tests disabled because of missing features on the test harness:
#
#   AboutBlankLoad
#   HitTestResultNodeHandle
#   MouseMoveAfterCrash
#   ResponsivenessTimerDoesntFireEarly
#   SpacebarScrolling
#   WKPreferences
#
# Flaky test, fails on Release but passes on Debug:
#
#   UserMessage
