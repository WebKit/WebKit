find_library(CARBON_LIBRARY Carbon)
find_library(FOUNDATION_LIBRARY Foundation)

find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(CORESERVICES_LIBRARY CoreServices)
add_definitions(-iframework ${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks)
add_definitions(-iframework ${CORESERVICES_LIBRARY}/Versions/Current/Frameworks)

link_directories(../../WebKitLibraries)
add_definitions(-DJSC_API_AVAILABLE\\\(...\\\)=)
add_definitions(-DJSC_CLASS_AVAILABLE\\\(...\\\)=)

list(APPEND WebKitTestRunner_LIBRARIES
    ${CARBON_LIBRARY}
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/WebKitLibraries
    ${ICU_INCLUDE_DIRS}
    ${WEBCORE_DIR}/testing/cocoa
    ${WEBKITLEGACY_DIR}
    ${WebKitTestRunner_DIR}/cf
    ${WebKitTestRunner_DIR}/cg
    ${WebKitTestRunner_DIR}/cocoa
    ${WebKitTestRunner_DIR}/mac
    ${WebKitTestRunner_DIR}/InjectedBundle/mac
    ${WebKitTestRunner_SHARED_DIR}/EventSerialization/mac
    ${WebKitTestRunner_SHARED_DIR}/cocoa
    ${WebKitTestRunner_SHARED_DIR}/mac
    ${WebKitTestRunner_SHARED_DIR}/spi
)

list(APPEND WebKitTestRunnerInjectedBundle_SOURCES
    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa/AccessibilityCommonCocoa.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa/ActivateFontsCocoa.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa/InjectedBundlePageCocoa.mm

    ${WebKitTestRunner_DIR}/InjectedBundle/mac/AccessibilityControllerMac.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/mac/AccessibilityNotificationHandler.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/mac/AccessibilityTextMarkerRangeMac.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/mac/InjectedBundleMac.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/mac/AccessibilityTextMarkerMac.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/mac/AccessibilityUIElementMac.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/mac/TestRunnerMac.mm

    ${WebKitTestRunner_SHARED_DIR}/EventSerialization/mac/EventSerializerMac.mm
    ${WebKitTestRunner_SHARED_DIR}/EventSerialization/mac/SharedEventStreamsMac.mm
)

list(APPEND WebKitTestRunnerInjectedBundle_LIBRARIES
    ${FOUNDATION_LIBRARY}
    JavaScriptCore
    WTF
    WebCoreTestSupport
    WebKit
)
set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-framework Cocoa")

list(APPEND WebKitTestRunner_SOURCES
    ${WebKitTestRunner_DIR}/cocoa/TestControllerCocoa.mm
    ${WebKitTestRunner_DIR}/cocoa/TestRunnerWKWebView.mm
    ${WebKitTestRunner_DIR}/cocoa/TestWebsiteDataStoreDelegate.mm
    ${WebKitTestRunner_DIR}/cocoa/UIScriptControllerCocoa.mm

    ${WebKitTestRunner_DIR}/mac/EventSenderProxy.mm
    ${WebKitTestRunner_DIR}/mac/PlatformWebViewMac.mm
    ${WebKitTestRunner_DIR}/mac/TestControllerMac.mm
    ${WebKitTestRunner_DIR}/mac/UIScriptControllerMac.mm
    ${WebKitTestRunner_DIR}/mac/WebKitTestRunnerDraggingInfo.mm
    ${WebKitTestRunner_DIR}/mac/WebKitTestRunnerEvent.mm
    ${WebKitTestRunner_DIR}/mac/WebKitTestRunnerPasteboard.mm
    ${WebKitTestRunner_DIR}/mac/WebKitTestRunnerWindow.mm
    ${WebKitTestRunner_DIR}/mac/main.mm

    ${WebKitTestRunner_SHARED_DIR}/cocoa/ClassMethodSwizzler.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/PlatformViewHelpers.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/PoseAsClass.mm

    ${WebKitTestRunner_SHARED_DIR}/EventSerialization/mac/EventSerializerMac.mm
    ${WebKitTestRunner_SHARED_DIR}/EventSerialization/mac/SharedEventStreamsMac.mm
)

link_directories(../../WebKitLibraries)
