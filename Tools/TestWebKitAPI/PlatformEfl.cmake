set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI")
set(TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY_WTF "${TESTWEBKITAPI_RUNTIME_OUTPUT_DIRECTORY}/WTF")

add_custom_target(forwarding-headersEflForTestWebKitAPI
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${TESTWEBKITAPI_DIR} --output ${DERIVED_SOURCES_WEBKIT2_DIR}/include --platform efl --platform CoordinatedGraphics --platform soup
    DEPENDS forwarding-headersEflForWebKit2
)
set(ForwardingHeadersForTestWebKitAPI_NAME forwarding-headersEflForTestWebKitAPI)

include_directories(
    ${DERIVED_SOURCES_WEBKIT2_DIR}/include
    ${WTF_DIR}/wtf/efl
    ${WEBKIT2_DIR}/UIProcess/API/C/CoordinatedGraphics
    ${WEBKIT2_DIR}/UIProcess/API/C/soup
    ${WEBKIT2_DIR}/UIProcess/API/C/efl
    ${WEBKIT2_DIR}/UIProcess/API/efl
)

include_directories(SYSTEM
    ${ECORE_EVAS_INCLUDE_DIRS}
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

list(APPEND test_wtf_LIBRARIES
    WebKit2
)

list(APPEND test_webcore_LIBRARIES
    WebKit2
)

list(APPEND TestJavaScriptCore_LIBRARIES
    WebKit2
)

# The list below works like a test expectation. Tests in the
# test_{webkit2_api|webcore}_BINARIES list are added to the test runner and
# tried on the bots on every build. Tests in test_{webkit2_api|webcore}_BINARIES
# are compiled and suffixed with fail and skipped from the test runner.
#
# Make sure that the tests are passing on both Debug and
# Release builds before adding it to test_{webkit2_api|webcore}_BINARIES.

set(test_webcore_BINARIES
    CSSParser
    HTMLParserIdioms
    LayoutUnit
    URL
)

# In here we list the bundles that are used by our specific WK2 API Tests
list(APPEND bundle_harness_SOURCES
    ${TESTWEBKITAPI_DIR}/Tests/WebKit2/efl/WKViewClientWebProcessCallbacks_Bundle.cpp
)

set(test_webkit2_api_BINARIES
    AboutBlankLoad
    CloseThenTerminate
    CookieManager
    DOMWindowExtensionNoCache
    DidAssociateFormControls
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
    InjectedBundleFrameHitTest
    InjectedBundleInitializationUserDataCallbackWins
    LoadAlternateHTMLStringWithNonDirectoryURL
    LoadCanceledNoServerRedirectCallback
    NewFirstVisuallyNonEmptyLayout
    NewFirstVisuallyNonEmptyLayoutFails
    NewFirstVisuallyNonEmptyLayoutForImages
    PageLoadBasic
    PageLoadDidChangeLocationWithinPageForFrame
    ParentFrame
    PendingAPIRequestURL
    PreventEmptyUserAgent
    PrivateBrowsingPushStateNoHistoryCallback
    ResponsivenessTimerDoesntFireEarly
    ShouldGoToBackForwardListItem
    TerminateTwice
    TextFieldDidBeginAndEndEditing
    WKPreferences
    WKString
    WKStringJSString
    WKURL
    WillSendSubmitEvent

    CoordinatedGraphics/WKViewRestoreZoomAndScrollBackForward
    CoordinatedGraphics/WKViewUserViewportToContents

    efl/WKViewClientWebProcessCallbacks
    efl/WKViewScrollTo
)

set(test_webkit2_api_fail_BINARIES
    CanHandleRequest
    DOMWindowExtensionBasic
    DownloadDecideDestinationCrash
    Geolocation
    LoadPageOnCrash
    MouseMoveAfterCrash
    NewFirstVisuallyNonEmptyLayoutFrames
    ReloadPageAfterCrash
    ResizeReversePaginatedWebView
    ResizeWindowAfterCrash
    RestoreSessionStateContainingFormData
    ScrollPinningBehaviors
    UserMessage
    WKPageGetScaleFactorNotZero
    WillLoad
)

if (ENABLE_SECCOMP_FILTERS)
    list(APPEND test_webkit2_api_fail_BINARIES
        SeccompFilters
    )
endif ()

# Tests disabled because of missing features on the test harness:
#
#   SpacebarScrolling
#   CoordinatedGraphics/WKViewIsActiveSetIsActive
