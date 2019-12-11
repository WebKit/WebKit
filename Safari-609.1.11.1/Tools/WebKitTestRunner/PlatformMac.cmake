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
    ${DERIVED_SOURCES_DIR}
    ${DERIVED_SOURCES_DIR}/WebCore
    ${FORWARDING_HEADERS_DIR}
    ${FORWARDING_HEADERS_DIR}/JavaScriptCore
    ${FORWARDING_HEADERS_DIR}/WebCore
    ${ICU_INCLUDE_DIRS}
    ${WEBCORE_DIR}/testing/cocoa
    ${WEBKITLEGACY_DIR}
    ${WEBKIT_TESTRUNNER_DIR}/cf
    ${WEBKIT_TESTRUNNER_DIR}/cg
    ${WEBKIT_TESTRUNNER_DIR}/cocoa
    ${WEBKIT_TESTRUNNER_DIR}/mac
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/mac
    ${WEBKIT_TESTRUNNER_SHARED_DIR}/EventSerialization/mac
    ${WEBKIT_TESTRUNNER_SHARED_DIR}/cocoa
    ${WEBKIT_TESTRUNNER_SHARED_DIR}/mac
    ${WEBKIT_TESTRUNNER_SHARED_DIR}/spi
)

list(APPEND WebKitTestRunnerInjectedBundle_SOURCES
    ${WEBKIT_TESTRUNNER_DIR}/cocoa/CrashReporterInfo.mm

    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/cocoa/ActivateFontsCocoa.mm
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/cocoa/InjectedBundlePageCocoa.mm

    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/mac/AccessibilityControllerMac.mm
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/mac/AccessibilityNotificationHandler.mm
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/mac/AccessibilityTextMarkerRangeMac.mm
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/mac/InjectedBundleMac.mm
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/mac/AccessibilityCommonMac.mm
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/mac/AccessibilityTextMarkerMac.mm
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/mac/AccessibilityUIElementMac.mm
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/mac/TestRunnerMac.mm

    ${WEBKIT_TESTRUNNER_SHARED_DIR}/EventSerialization/mac/EventSerializerMac.mm
    ${WEBKIT_TESTRUNNER_SHARED_DIR}/EventSerialization/mac/SharedEventStreamsMac.mm
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
    ${WEBKIT_TESTRUNNER_DIR}/cg/TestInvocationCG.cpp

    ${WEBKIT_TESTRUNNER_DIR}/cocoa/CrashReporterInfo.mm
    ${WEBKIT_TESTRUNNER_DIR}/cocoa/TestControllerCocoa.mm
    ${WEBKIT_TESTRUNNER_DIR}/cocoa/TestRunnerWKWebView.mm
    ${WEBKIT_TESTRUNNER_DIR}/cocoa/TestWebsiteDataStoreDelegate.mm
    ${WEBKIT_TESTRUNNER_DIR}/cocoa/UIScriptControllerCocoa.mm

    ${WEBKIT_TESTRUNNER_DIR}/mac/EventSenderProxy.mm
    ${WEBKIT_TESTRUNNER_DIR}/mac/PlatformWebViewMac.mm
    ${WEBKIT_TESTRUNNER_DIR}/mac/PoseAsClass.mm
    ${WEBKIT_TESTRUNNER_DIR}/mac/TestControllerMac.mm
    ${WEBKIT_TESTRUNNER_DIR}/mac/UIScriptControllerMac.mm
    ${WEBKIT_TESTRUNNER_DIR}/mac/WebKitTestRunnerDraggingInfo.mm
    ${WEBKIT_TESTRUNNER_DIR}/mac/WebKitTestRunnerEvent.mm
    ${WEBKIT_TESTRUNNER_DIR}/mac/WebKitTestRunnerPasteboard.mm
    ${WEBKIT_TESTRUNNER_DIR}/mac/WebKitTestRunnerWindow.mm
    ${WEBKIT_TESTRUNNER_DIR}/mac/main.mm
    
    ${WEBKIT_TESTRUNNER_SHARED_DIR}/cocoa/ClassMethodSwizzler.mm

    ${WEBKIT_TESTRUNNER_SHARED_DIR}/EventSerialization/mac/EventSerializerMac.mm
    ${WEBKIT_TESTRUNNER_SHARED_DIR}/EventSerialization/mac/SharedEventStreamsMac.mm
)

link_directories(../../WebKitLibraries)
