set(WebKitTestRunner_SHARED_DIR ${TOOLS_DIR}/TestRunnerShared)

find_library(FOUNDATION_LIBRARY Foundation)
find_library(UIKIT_LIBRARY UIKit)

set(_wktr_ios_compile_options
    -include "${WebKitTestRunner_DIR}/WebKitTestRunnerPrefix.h"
    -Wno-deprecated
    -Wno-deprecated-declarations
    -Wno-objc-method-access
)
list(APPEND WebKitTestRunner_COMPILE_OPTIONS ${_wktr_ios_compile_options})
list(APPEND TestRunnerInjectedBundle_COMPILE_OPTIONS ${_wktr_ios_compile_options})

set(EXECUTABLE_NAME WebKitTestRunnerInjectedBundle)
set(PRODUCT_BUNDLE_IDENTIFIER com.apple.WebKitTestRunner.InjectedBundle)
configure_file("${WebKitTestRunner_DIR}/InjectedBundle-Info.plist"
               "${CMAKE_CURRENT_BINARY_DIR}/InjectedBundle-Info.plist")
set_target_properties(TestRunnerInjectedBundle PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION bundle
    OUTPUT_NAME WebKitTestRunnerInjectedBundle
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/InjectedBundle-Info.plist"
)

# Fonts for layout tests.
file(GLOB _wktr_fonts
    "${WebKitTestRunner_DIR}/fonts/*.ttf"
    "${WebKitTestRunner_DIR}/fonts/*.TTF"
    "${WebKitTestRunner_DIR}/fonts/*.otf"
    "${WebKitTestRunner_DIR}/FakeHelvetica-ArmenianCharacter.ttf"
    "${WebKitTestRunner_DIR}/FontWithFeatures.ttf"
    "${WebKitTestRunner_DIR}/FontWithFeatures.otf"
)
set_property(TARGET TestRunnerInjectedBundle APPEND PROPERTY RESOURCE "${_wktr_fonts}")
target_sources(TestRunnerInjectedBundle PRIVATE ${_wktr_fonts})

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${WebKit_FRAMEWORK_HEADERS_DIR}
    ${WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
    ${WEBKIT_DIR}/UIProcess/API/C/mac
    ${WEBKIT_DIR}/UIProcess/API/Cocoa
    ${WEBKIT_DIR}/UIProcess/Cocoa
    ${WEBKIT_DIR}/Shared
)

list(APPEND WebKitTestRunner_LIBRARIES
    JavaScriptCore
)

set(_wktr_ios_include_dirs
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/WebKitLibraries
    ${WEBCORE_DIR}/testing/cocoa
    ${PAL_FRAMEWORK_HEADERS_DIR}
    ${WEBKIT_DIR}/Platform/spi/ios
    ${WEBKIT_DIR}/Platform/spi/watchos
    ${WEBKIT_DIR}/UIProcess/ios
    ${WebKitTestRunner_DIR}/cf
    ${WebKitTestRunner_DIR}/cg
    ${WebKitTestRunner_DIR}/cocoa
    ${WebKitTestRunner_DIR}/ios
    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa
    ${WebKitTestRunner_DIR}/InjectedBundle/ios
    ${WebKitTestRunner_SHARED_DIR}/cocoa
    ${WebKitTestRunner_SHARED_DIR}/spi
)
list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES ${_wktr_ios_include_dirs})
list(APPEND TestRunnerInjectedBundle_INCLUDE_DIRECTORIES ${_wktr_ios_include_dirs})

list(APPEND TestRunnerInjectedBundle_PRIVATE_LIBRARIES "-Wl,-undefined,dynamic_lookup" "-Wl,-not_for_dyld_shared_cache")

list(APPEND TestRunnerInjectedBundle_SOURCES
    ${WebKitTestRunner_DIR}/cocoa/CrashReporterInfo.mm

    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa/AccessibilityCommonCocoa.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa/AccessibilityTextMarkerRangeCocoa.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa/ActivateFontsCocoa.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa/InjectedBundlePageCocoa.mm

    ${WebKitTestRunner_DIR}/InjectedBundle/ios/AccessibilityControllerIOS.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/ios/AccessibilityTextMarkerIOS.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/ios/AccessibilityUIElementIOS.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/ios/EventSenderProxyIOS.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/ios/InjectedBundleIOS.mm
)

list(APPEND TestRunnerInjectedBundle_LIBRARIES
    ${FOUNDATION_LIBRARY}
    JavaScriptCore
    WebCoreTestSupport
    WebKit
)

list(APPEND WebKitTestRunner_SOURCES
    ${WebKitTestRunner_DIR}/cocoa/CrashReporterInfo.mm
    ${WebKitTestRunner_DIR}/cocoa/EventSenderProxyCocoa.mm
    ${WebKitTestRunner_DIR}/cocoa/TestControllerCocoa.mm
    ${WebKitTestRunner_DIR}/cocoa/TestInvocationCocoa.mm
    ${WebKitTestRunner_DIR}/cocoa/TestRunnerWKWebView.mm
    ${WebKitTestRunner_DIR}/cocoa/TestWebsiteDataStoreDelegate.mm
    ${WebKitTestRunner_DIR}/cocoa/UIScriptControllerCocoa.mm
    ${WebKitTestRunner_DIR}/cocoa/WebNotificationProviderCocoa.mm
    ${WebKitTestRunner_DIR}/cocoa/WKTextExtractionTestingHelpers.mm

    ${WebKitTestRunner_DIR}/InjectedBundle/ios/EventSenderProxyIOS.mm

    ${WebKitTestRunner_DIR}/ios/GeneratedTouchesDebugWindow.mm
    ${WebKitTestRunner_DIR}/ios/HIDEventGenerator.mm
    ${WebKitTestRunner_DIR}/ios/PlatformWebViewIOS.mm
    ${WebKitTestRunner_DIR}/ios/TestControllerIOS.mm
    ${WebKitTestRunner_DIR}/ios/UIPasteboardConsistencyEnforcer.mm
    ${WebKitTestRunner_DIR}/ios/UIScriptControllerIOS.mm
    ${WebKitTestRunner_DIR}/ios/mainIOS.mm

    ${WebKitTestRunner_SHARED_DIR}/cocoa/ClassMethodSwizzler.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/InstanceMethodSwizzler.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/LayoutTestSpellChecker.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/ModifierKeys.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/PlatformViewHelpers.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/PoseAsClass.mm
    ${WebKitTestRunner_SHARED_DIR}/IOSLayoutTestCommunication.cpp
)

target_link_libraries(WebKitTestRunner PRIVATE
    ${FOUNDATION_LIBRARY}
    ${UIKIT_LIBRARY}
)

set_target_properties(WebKitTestRunner PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.webkit.WebKitTestRunner"
    MACOSX_BUNDLE_BUNDLE_NAME "WebKitTestRunner"
)

set(_wktr_bundle_id "org.webkit.WebKitTestRunner")

add_dependencies(WebKitTestRunner WebContentExtension WebContentCaptivePortalExtension NetworkingExtension)
if (ENABLE_GPU_PROCESS)
    add_dependencies(WebKitTestRunner GPUExtension)
endif ()

WEBKIT_EMBED_EXTENSION(WebKitTestRunner WebContentExtension ${_wktr_bundle_id}
    CHANGE_EXTENSION_POINT ADD_ATS)
WEBKIT_EMBED_EXTENSION(WebKitTestRunner WebContentCaptivePortalExtension ${_wktr_bundle_id}
    CHANGE_EXTENSION_POINT ADD_ATS)
WEBKIT_EMBED_EXTENSION(WebKitTestRunner NetworkingExtension ${_wktr_bundle_id}
    ADD_ATS)
if (ENABLE_GPU_PROCESS)
    WEBKIT_EMBED_EXTENSION(WebKitTestRunner GPUExtension ${_wktr_bundle_id})
endif ()

set_target_properties(WebKitTestRunner PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(TestRunnerInjectedBundle PROPERTIES LINKER_LANGUAGE CXX)
