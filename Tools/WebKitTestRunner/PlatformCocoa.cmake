if (WEBKIT_SDK_IS_MACOS)

# TestRunnerShared/CMakeLists.txt defines TestRunnerShared_DIR; this file uses the old name.
set(WebKitTestRunner_SHARED_DIR ${TOOLS_DIR}/TestRunnerShared)

find_library(CARBON_LIBRARY Carbon)
find_library(FOUNDATION_LIBRARY Foundation)

find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(CORESERVICES_LIBRARY CoreServices)
add_definitions(-iframework ${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks)
add_definitions(-iframework ${CORESERVICES_LIBRARY}/Versions/Current/Frameworks)

link_directories(../../WebKitLibraries)
# JSC_{API,CLASS}_AVAILABLE defined by JavaScriptCore/WebKitAvailability.h since it gained the
# !JSC_FRAMEWORK_HEADER_POSTPROCESSING_ENABLED fallback -- these now just cause -Wmacro-redefined.

# Prefix header matching Xcode's GCC_PREFIX_HEADER. Must use _COMPILE_OPTIONS list
# (not add_compile_options) -- targets are created before this file is included.
set(_wtr_mac_compile_options
    -include "${WebKitTestRunner_DIR}/WebKitTestRunnerPrefix.h"
    # CTFontManagerRegisterFontsForURLs deprecated in 10.15; NSAnyEventMask in 10.12.
    -Wno-deprecated  # "treating 'c-header' input as 'c++-header'" -- -include of a .h
    -Wno-deprecated-declarations

    -Wno-objc-method-access  # FIXME: https://bugs.webkit.org/show_bug.cgi?id=312034
)
list(APPEND WebKitTestRunner_COMPILE_OPTIONS ${_wtr_mac_compile_options})
list(APPEND TestRunnerInjectedBundle_COMPILE_OPTIONS ${_wtr_mac_compile_options})

# TestControllerMac.mm expects a .bundle next to the WTR binary.
# BUNDLE TRUE on a MODULE library emits a macOS CFBundle layout.
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

if (USE_APPLE_INTERNAL_SDK)
    set(WebKitTestRunner_CODE_SIGN_ENTITLEMENTS
        "${WebKitTestRunner_DIR}/Configurations/WebKitTestRunner-internal.entitlements"
    )
else ()
    set(WebKitTestRunner_CODE_SIGN_ENTITLEMENTS
        "${WebKitTestRunner_DIR}/Configurations/WebKitTestRunner.entitlements"
    )
endif ()

# ActivateFontsCocoa.mm registers fonts from the bundle's Resources/.
# AHEM is the critical one -- layout tests assume it for pixel-perfect metric consistency.
file(GLOB _wtr_fonts
    "${WebKitTestRunner_DIR}/fonts/*.ttf"
    "${WebKitTestRunner_DIR}/fonts/*.TTF"
    "${WebKitTestRunner_DIR}/fonts/*.otf"
    "${WebKitTestRunner_DIR}/FakeHelvetica-ArmenianCharacter.ttf"
    "${WebKitTestRunner_DIR}/FontWithFeatures.ttf"
    "${WebKitTestRunner_DIR}/FontWithFeatures.otf"
)
set_property(TARGET TestRunnerInjectedBundle APPEND PROPERTY RESOURCE "${_wtr_fonts}")
target_sources(TestRunnerInjectedBundle PRIVATE ${_wtr_fonts})

# WebKit headers first (same ordering as MiniBrowser). WTR also uses WebKit private headers
# directly -- add UIProcess source paths matching Xcode's HEADER_SEARCH_PATHS.
list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${WebKit_FRAMEWORK_HEADERS_DIR}
    ${WebKitLegacy_FRAMEWORK_HEADERS_DIR}
    ${WEBKIT_DIR}/UIProcess/API/C/mac
    ${WEBKIT_DIR}/UIProcess/API/Cocoa
    ${WEBKIT_DIR}/UIProcess/Cocoa
    ${WEBKIT_DIR}/Shared
)

list(APPEND WebKitTestRunner_LIBRARIES
    ${CARBON_LIBRARY}
)

set(_wtr_mac_include_dirs
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/WebKitLibraries
    ${ICU_INCLUDE_DIRS}
    ${WEBCORE_DIR}/testing/cocoa
    ${WEBKITLEGACY_DIR}
    ${WebKitTestRunner_DIR}/cf
    ${WebKitTestRunner_DIR}/cg
    ${WebKitTestRunner_DIR}/cocoa
    ${WebKitTestRunner_DIR}/mac
    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa
    ${WebKitTestRunner_DIR}/InjectedBundle/mac
    ${WebKitTestRunner_SHARED_DIR}/EventSerialization/mac
    ${WebKitTestRunner_SHARED_DIR}/cocoa
    ${WebKitTestRunner_SHARED_DIR}/mac
    ${WebKitTestRunner_SHARED_DIR}/spi
)
# WebKitTestRunner_INCLUDE_DIRECTORIES only applies to the WebKitTestRunner executable target;
# TestRunnerInjectedBundle is a separate target with its own include list.
list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES ${_wtr_mac_include_dirs})
list(APPEND TestRunnerInjectedBundle_INCLUDE_DIRECTORIES ${_wtr_mac_include_dirs})

# TestRunnerInjectedBundle links WebCoreTestSupport (static) which references
# WTF symbols. The bundle is loaded into a process that already has WTF, so
# use -undefined dynamic_lookup to resolve them at runtime.
list(APPEND TestRunnerInjectedBundle_PRIVATE_LIBRARIES "-Wl,-undefined,dynamic_lookup")

list(APPEND TestRunnerInjectedBundle_SOURCES
    ${WebKitTestRunner_DIR}/cocoa/CrashReporterInfo.mm

    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa/AccessibilityCommonCocoa.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa/AccessibilityTextMarkerRangeCocoa.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa/ActivateFontsCocoa.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/cocoa/InjectedBundlePageCocoa.mm

    ${WebKitTestRunner_DIR}/InjectedBundle/mac/AccessibilityControllerMac.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/mac/AccessibilityNotificationHandler.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/mac/AccessibilityTextMarkerMac.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/mac/AccessibilityUIElementClientMac.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/mac/AccessibilityUIElementMac.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/mac/InjectedBundleMac.mm
    ${WebKitTestRunner_DIR}/InjectedBundle/mac/TestRunnerMac.mm

    ${WebKitTestRunner_SHARED_DIR}/EventSerialization/mac/EventSerializerMac.mm
    ${WebKitTestRunner_SHARED_DIR}/EventSerialization/mac/SharedEventStreamsMac.mm
)

# WTF is an OBJECT library -- linking it directly creates a second copy of WTF static state
# alongside JavaScriptCore.framework's, causing heap crashes. WTF symbols come transitively
# via JavaScriptCore which re-exports WTF.
list(APPEND TestRunnerInjectedBundle_LIBRARIES
    ${FOUNDATION_LIBRARY}
    JavaScriptCore
    WebCoreTestSupport
    WebKit
)
set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-framework Cocoa")

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

    ${WebKitTestRunner_DIR}/mac/EventSenderProxy.mm
    ${WebKitTestRunner_DIR}/mac/PlatformWebViewMac.mm
    ${WebKitTestRunner_DIR}/mac/TestControllerMac.mm
    ${WebKitTestRunner_DIR}/mac/UIScriptControllerMac.mm
    ${WebKitTestRunner_DIR}/mac/WebKitTestRunnerDraggingInfo.mm
    ${WebKitTestRunner_DIR}/mac/WebKitTestRunnerEvent.mm
    ${WebKitTestRunner_DIR}/mac/WebKitTestRunnerPasteboard.mm
    ${WebKitTestRunner_DIR}/mac/WebKitTestRunnerWindow.mm
    ${WebKitTestRunner_DIR}/mac/main.mm

    ${WebKitTestRunner_SHARED_DIR}/mac/NSPasteboardAdditions.mm

    ${WebKitTestRunner_SHARED_DIR}/cocoa/ClassMethodSwizzler.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/InstanceMethodSwizzler.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/LayoutTestSpellChecker.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/ModifierKeys.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/PlatformViewHelpers.mm
    ${WebKitTestRunner_SHARED_DIR}/cocoa/PoseAsClass.mm

    ${WebKitTestRunner_SHARED_DIR}/EventSerialization/mac/EventSerializerMac.mm
    ${WebKitTestRunner_SHARED_DIR}/EventSerialization/mac/SharedEventStreamsMac.mm
)

link_directories(../../WebKitLibraries)

elseif (WEBKIT_SDK_IS_IOS_FAMILY)

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

endif ()
