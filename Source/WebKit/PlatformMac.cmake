# -ObjC++ only for .mm unified sources. Applying it to .cpp files causes ambiguity
# errors (WebCore::Pattern vs QuickDraw::Pattern, WebCore::IOSurface vs IOSurface ObjC class)
# when `using namespace WebCore;` pulls WebCore names into the global scope alongside SDK ObjC types.
add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-std=c++2b>" "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-D__STDC_WANT_LIB_EXT1__>")
find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(CARBON_LIBRARY Carbon)
find_library(CORESERVICES_LIBRARY CoreServices)
find_library(NETWORK_LIBRARY Network)
find_library(SECURITY_LIBRARY Security)
find_library(SECURITYINTERFACE_LIBRARY SecurityInterface)
find_library(QUARTZ_LIBRARY Quartz)
find_library(UNIFORMTYPEIDENTIFIERS_LIBRARY UniformTypeIdentifiers)
find_library(AVFOUNDATION_LIBRARY AVFoundation)
find_library(AVFAUDIO_LIBRARY AVFAudio HINTS ${AVFOUNDATION_LIBRARY}/Versions/*/Frameworks)
find_library(DEVICEIDENTITY_LIBRARY DeviceIdentity HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
add_compile_options(
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${QUARTZ_LIBRARY}/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CARBON_LIBRARY}/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks>"
)
add_definitions(-DWK_XPC_SERVICE_SUFFIX=".Development")

# Match Xcode's BaseTarget.xcconfig WEBKIT_BUNDLE_VERSION. XPC child processes compare this
# at launch; empty string crashes (crashDueWebKitFrameworkVersionMismatch).
add_definitions(-DWEBKIT_BUNDLE_VERSION="${WEBKIT_MAC_VERSION}")

set(MACOSX_FRAMEWORK_IDENTIFIER com.apple.WebKit)

add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CORESERVICES_LIBRARY}/Versions/Current/Frameworks>")

include(Headers.cmake)

list(APPEND WebKit_PRIVATE_LIBRARIES
    Accessibility
    ${APPLICATIONSERVICES_LIBRARY}
    ${CORESERVICES_LIBRARY}
    ${DEVICEIDENTITY_LIBRARY}
    ${NETWORK_LIBRARY}
    ${SECURITYINTERFACE_LIBRARY}
    ${UNIFORMTYPEIDENTIFIERS_LIBRARY}
)

if (NOT AVFAUDIO_LIBRARY-NOTFOUND)
    list(APPEND WebKit_LIBRARIES ${AVFAUDIO_LIBRARY})
endif ()

list(APPEND WebKit_UNIFIED_SOURCE_LIST_FILES
    "SourcesCocoa.txt"

    "Platform/SourcesCocoa.txt"
)

list(APPEND WebKit_SOURCES
    GPUProcess/media/RemoteAudioDestinationManager.cpp

    NetworkProcess/cocoa/LaunchServicesDatabaseObserver.mm
    NetworkProcess/cocoa/WebSocketTaskCocoa.mm

    NetworkProcess/mac/NetworkConnectionToWebProcessMac.mm

    NetworkProcess/webrtc/NetworkRTCProvider.cpp
    NetworkProcess/webrtc/NetworkRTCTCPSocketCocoa.mm
    NetworkProcess/webrtc/NetworkRTCUDPSocketCocoa.mm
    NetworkProcess/webrtc/NetworkRTCUtilitiesCocoa.mm

    NetworkProcess/Downloads/cocoa/WKDownloadProgress.mm

    Platform/IPC/cocoa/SharedFileHandleCocoa.cpp

    Shared/API/Cocoa/WKMain.mm

    Shared/Cocoa/DefaultWebBrowserChecks.mm
    Shared/Cocoa/XPCEndpoint.mm
    Shared/Cocoa/XPCEndpointClient.mm

    UIProcess/API/Cocoa/WKContentWorld.mm
    UIProcess/API/Cocoa/_WKAuthenticationExtensionsClientInputs.mm
    UIProcess/API/Cocoa/_WKAuthenticationExtensionsClientOutputs.mm
    UIProcess/API/Cocoa/_WKAuthenticatorAssertionResponse.mm
    UIProcess/API/Cocoa/_WKAuthenticatorAttestationResponse.mm
    UIProcess/API/Cocoa/_WKAuthenticatorResponse.mm
    UIProcess/API/Cocoa/_WKAuthenticatorSelectionCriteria.mm
    UIProcess/API/Cocoa/_WKPublicKeyCredentialCreationOptions.mm
    UIProcess/API/Cocoa/_WKPublicKeyCredentialDescriptor.mm
    UIProcess/API/Cocoa/_WKPublicKeyCredentialEntity.mm
    UIProcess/API/Cocoa/_WKPublicKeyCredentialParameters.mm
    UIProcess/API/Cocoa/_WKPublicKeyCredentialRelyingPartyEntity.mm
    UIProcess/API/Cocoa/_WKPublicKeyCredentialRequestOptions.mm
    UIProcess/API/Cocoa/_WKPublicKeyCredentialUserEntity.mm
    UIProcess/API/Cocoa/_WKResourceLoadStatisticsFirstParty.mm
    UIProcess/API/Cocoa/_WKResourceLoadStatisticsThirdParty.mm

    UIProcess/Cocoa/PreferenceObserver.mm
    UIProcess/Cocoa/WKShareSheet.mm
    UIProcess/Cocoa/WKStorageAccessAlert.mm
    UIProcess/Cocoa/WebInspectorPreferenceObserver.mm

    UIProcess/PDF/WKPDFHUDView.mm
    ${WEBKIT_DIR}/UIProcess/PDF/WKPDFHUDView.swift
    UIProcess/PDF/WKPDFPageNumberIndicator.mm

    ${WEBKIT_DIR}/Platform/cocoa/WKMaterialHostingSupport.swift

    ${WEBKIT_DIR}/UIProcess/API/Cocoa/_WKTextExtraction.swift

    WebProcess/InjectedBundle/API/c/mac/WKBundlePageMac.mm

    WebProcess/WebAuthentication/WebAuthenticatorCoordinator.cpp

    WebProcess/cocoa/AudioSessionRoutingArbitrator.cpp
    WebProcess/cocoa/LaunchServicesDatabaseManager.mm
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}/libwebrtc/PrivateHeaders"
    "${ICU_INCLUDE_DIRS}"
    "${WEBKIT_DIR}/GPUProcess/graphics/Model"
    "${WEBKIT_DIR}/GPUProcess/mac"
    "${WEBKIT_DIR}/GPUProcess/media/cocoa"
    "${WEBKIT_DIR}/NetworkProcess/cocoa"
    "${WEBKIT_DIR}/NetworkProcess/mac"
    "${WEBKIT_DIR}/NetworkProcess/PrivateClickMeasurement/cocoa"
    "${WEBKIT_DIR}/UIProcess/mac"
    "${WEBKIT_DIR}/UIProcess/API/C/mac"
    "${WEBKIT_DIR}/UIProcess/API/Cocoa"
    "${WEBKIT_DIR}/UIProcess/API/mac"
    "${WEBKIT_DIR}/UIProcess/Authentication/cocoa"
    "${WEBKIT_DIR}/UIProcess/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Cocoa/SOAuthorization"
    "${WEBKIT_DIR}/UIProcess/Cocoa/TextExtraction"
    "${WEBKIT_DIR}/UIProcess/Extensions/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Inspector/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Inspector/mac"
    "${WEBKIT_DIR}/UIProcess/Launcher/mac"
    "${WEBKIT_DIR}/UIProcess/Media/cocoa"
    "${WEBKIT_DIR}/UIProcess/Notifications/cocoa"
    "${WEBKIT_DIR}/UIProcess/PDF"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/cocoa"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/mac"
    "${WEBKIT_DIR}/UIProcess/ios"
    "${WEBKIT_DIR}/UIProcess/WebAuthentication/Cocoa"
    "${WEBKIT_DIR}/UIProcess/WebAuthentication/Virtual"
    "${WEBKIT_DIR}/UIProcess/WebsiteData/Cocoa"
    "${WEBKIT_DIR}/Platform/cg"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/Platform/classifier/cocoa"
    "${WEBKIT_DIR}/Platform/cocoa"
    "${WEBKIT_DIR}/Platform/ios"
    "${WEBKIT_DIR}/Platform/mac"
    "${WEBKIT_DIR}/Platform/unix"
    # WebKitSwift headers imported by Cocoa .mm files (WKMarketplaceKit.h, WKIntelligence*.h,
    # WKIdentityDocument*.h). These are the ObjC-side interface headers to the Swift modules --
    # they self-guard with feature checks, safe to include-path them.
    "${WEBKIT_DIR}/WebKitSwift/MarketplaceKit"
    "${WEBKIT_DIR}/WebKitSwift/WritingTools"
    "${WEBKIT_DIR}/Platform/spi/Cocoa"
    "${WEBKIT_DIR}/Platform/spi/mac"
    "${WEBKIT_DIR}/Platform/IPC/darwin"
    "${WEBKIT_DIR}/Platform/IPC/mac"
    "${WEBKIT_DIR}/Platform/IPC/cocoa"
    "${WEBKIT_DIR}/Platform/spi/ios"
    "${WEBKIT_DIR}/Shared/API/Cocoa"
    "${WEBKIT_DIR}/Shared/API/c/cf"
    "${WEBKIT_DIR}/Shared/API/c/cg"
    "${WEBKIT_DIR}/Shared/API/c/mac"
    "${WEBKIT_DIR}/Shared/ApplePay/cocoa/"
    "${WEBKIT_DIR}/Shared/Authentication/cocoa"
    "${WEBKIT_DIR}/Shared/cf"
    "${WEBKIT_DIR}/Shared/Cocoa"
    "${WEBKIT_DIR}/Shared/Daemon"
    "${WEBKIT_DIR}/Shared/EntryPointUtilities/Cocoa/Daemon"
    "${WEBKIT_DIR}/Shared/EntryPointUtilities/Cocoa/XPCService"
    "${WEBKIT_DIR}/Shared/mac"
    "${WEBKIT_DIR}/Shared/Scrolling"
    "${WEBKIT_DIR}/UIProcess/Cocoa/GroupActivities"
    "${WEBKIT_DIR}/UIProcess/Media"
    "${WEBKIT_DIR}/UIProcess/WebAuthentication/fido"
    "${WEBKIT_DIR}/WebProcess/DigitalCredentials"
    "${WEBKIT_DIR}/WebProcess/WebAuthentication"
    "${WEBKIT_DIR}/WebProcess/cocoa"
    "${WEBKIT_DIR}/WebProcess/cocoa/IdentityDocumentServices"
    "${WEBKIT_DIR}/WebProcess/Extensions/Cocoa"
    "${WEBKIT_DIR}/WebKitSwift/IdentityDocumentServices"
    "${WEBKIT_DIR}/WebProcess/mac"
    "${WEBKIT_DIR}/WebProcess/GPU/graphics/cocoa"
    "${WEBKIT_DIR}/WebProcess/Inspector/mac"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/Cocoa"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/mac"
    "${WEBKIT_DIR}/WebProcess/MediaSession"
    "${WEBKIT_DIR}/WebProcess/Model/mac"
    "${WEBKIT_DIR}/WebProcess/Plugins/PDF"
    "${WEBKIT_DIR}/WebProcess/Plugins/PDF/UnifiedPDF"
    "${WEBKIT_DIR}/WebProcess/WebPage/Cocoa"
    "${WEBKIT_DIR}/WebProcess/WebPage/RemoteLayerTree"
    "${WEBKIT_DIR}/WebProcess/WebPage/mac"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/cocoa"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/mac"
    "${WEBKIT_DIR}/webpushd"
    "${WEBKIT_DIR}/webpushd/webpushtool"
    # <webrtc/webkit_sdk/WebKit/CMBaseObjectSPI.h> -- Apple SPI headers in libwebrtc's overlay.
    # Referenced with the full webrtc/ prefix even when USE(LIBWEBRTC) is off.
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source"
    "${WEBKITLEGACY_DIR}"
    "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}"
)

set(XPCService_SOURCES
    Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp

    Shared/EntryPointUtilities/Cocoa/XPCService/XPCServiceEntryPoint.mm
    Shared/EntryPointUtilities/Cocoa/XPCService/XPCServiceMain.mm
)

set(WebProcess_SOURCES
    WebProcess/EntryPoint/Cocoa/XPCService/WebContentServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

set(NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/Cocoa/XPCService/NetworkServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

set(GPUProcess_SOURCES
    GPUProcess/EntryPoint/Cocoa/XPCService/GPUServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

# FIXME: These should not have Development in production builds.
set(WebProcess_OUTPUT_NAME com.apple.WebKit.WebContent.Development)
set(NetworkProcess_OUTPUT_NAME com.apple.WebKit.Networking.Development)
set(GPUProcess_OUTPUT_NAME com.apple.WebKit.GPU.Development)

set(WebProcess_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})
set(NetworkProcess_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})

# Generate a simplified module map for Swift interop.
# The source-tree module.modulemap includes many C++ submodules with deep header
# dependencies (WEBCORE_EXPORT, API::Object, etc.) that fail in CMake's explicit
# module build context. We generate a stripped-down map that only includes the
# submodules needed by the Swift files compiled in this CMake build.
#
# Public API headers (WKWebView.h, _WKTextExtraction*.h) use WK_API_AVAILABLE
# macros from WKFoundation.h. These resolve via -Xcc -I${WebKit_FRAMEWORK_HEADERS_DIR}
# which points to the copied framework headers where WKFoundation.h is colocated.
set(WebKit_CMAKE_MODULEMAP_DIR "${CMAKE_BINARY_DIR}/WebKit/SwiftModules")
file(MAKE_DIRECTORY "${WebKit_CMAKE_MODULEMAP_DIR}")
file(WRITE "${WebKit_CMAKE_MODULEMAP_DIR}/module.modulemap"
"module WebKit_Internal [system] {
    module WKPDFHUDView {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/PDF/WKPDFHUDView.h\"
        export *
    }

    module WKWebView {
        requires objc
        header \"${WebKit_FRAMEWORK_HEADERS_DIR}/WebKit/WKWebView.h\"
        export *
    }

    module WKMaterialHostingSupport {
        requires objc
        header \"${WEBKIT_DIR}/Platform/cocoa/WKMaterialHostingSupport.h\"
        export *
    }

    module _WKTextExtractionInternal {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/API/Cocoa/_WKTextExtractionInternal.h\"
        export *
    }
}
")
set(WebKit_SWIFT_INTEROP_MODULE_PATH "${WebKit_CMAKE_MODULEMAP_DIR}")

# SPI .swiftinterface modules (SwiftUI_SPI, AVKit_SPI, etc.) live under
# Platform/spi/. These paths mirror Xcode's SWIFT_INCLUDE_PATHS setting.
set(WebKit_SWIFT_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/Platform/spi/Cocoa"
    "${WEBKIT_DIR}/Platform/spi/Cocoa/Modules"
    "${WEBKIT_DIR}/Platform/spi/ios"
)

# HAVE_MATERIAL_HOSTING is a PlatformHave.h preprocessor flag (macOS 16+),
# not a CMake define. SWIFT_EXTRA_OPTIONS and SWIFT_INCLUDE_DIRECTORIES only
# affect the typecheck custom command. Mirror everything to target_compile_options
# so the actual Swift compilation sees the same flags.
target_compile_options(WebKit PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:-DHAVE_MATERIAL_HOSTING>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/Cocoa>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/Cocoa/Modules>"
    "$<$<COMPILE_LANGUAGE:Swift>:-I${WEBKIT_DIR}/Platform/spi/ios>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${WebKit_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${WTF_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${bmalloc_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${PAL_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${ICU_INCLUDE_DIRS}>"
)

set(WebKit_SWIFT_EXTRA_OPTIONS
    -DHAVE_MATERIAL_HOSTING
    -Xcc -I${WebKit_FRAMEWORK_HEADERS_DIR}
    -Xcc -I${WTF_FRAMEWORK_HEADERS_DIR}
    -Xcc -I${bmalloc_FRAMEWORK_HEADERS_DIR}
    -Xcc -I${PAL_FRAMEWORK_HEADERS_DIR}
    -Xcc -I${ICU_INCLUDE_DIRS}
)

# webpushd entry points are standalone daemon executables, not part of the
# framework. WKMain.mm references them, so provide stubs that return error.
file(WRITE "${CMAKE_BINARY_DIR}/WebKit/WebPushDaemonStubs.cpp"
"#include \"config.h\"\n#if ENABLE(WEB_PUSH_NOTIFICATIONS)\nnamespace WebKit {\nint WebPushDaemonMain(int, char**) { return 1; }\nint WebPushToolMain(int, char**) { return 1; }\n}\n#endif\n")
list(APPEND WebKit_SOURCES "${CMAKE_BINARY_DIR}/WebKit/WebPushDaemonStubs.cpp")

set(WebKit_FORWARDING_HEADERS_FILES
    Platform/cocoa/WKCrashReporter.h

    Shared/API/c/WKDiagnosticLoggingResultType.h

    UIProcess/API/C/WKPageDiagnosticLoggingClient.h
    UIProcess/API/C/WKPageNavigationClient.h
    UIProcess/API/C/WKPageRenderingProgressEvents.h
)

list(APPEND WebKit_MESSAGES_IN_FILES
    GPUProcess/media/RemoteImageDecoderAVFProxy

    ModelProcess/cocoa/ModelProcessModelPlayerProxy

    GPUProcess/media/ios/RemoteMediaSessionHelperProxy

    GPUProcess/webrtc/UserMediaCaptureManagerProxy

    NetworkProcess/CustomProtocols/LegacyCustomProtocolManager

    Shared/API/Cocoa/RemoteObjectRegistry

    Shared/ApplePay/WebPaymentCoordinatorProxy

    UIProcess/ViewGestureController

    UIProcess/Cocoa/PlaybackSessionManagerProxy
    UIProcess/Cocoa/VideoPresentationManagerProxy

    UIProcess/Inspector/WebInspectorUIExtensionControllerProxy

    UIProcess/Media/AudioSessionRoutingArbitratorProxy

    UIProcess/Network/CustomProtocols/LegacyCustomProtocolManagerProxy

    UIProcess/RemoteLayerTree/RemoteLayerTreeDrawingAreaProxy

    UIProcess/WebAuthentication/WebAuthenticatorCoordinatorProxy

    UIProcess/ios/SmartMagnificationController
    UIProcess/ios/WebDeviceOrientationUpdateProviderProxy

    UIProcess/mac/SecItemShimProxy

    WebProcess/ApplePay/WebPaymentCoordinator

    WebProcess/GPU/media/RemoteImageDecoderAVFManager

    WebProcess/GPU/media/ios/RemoteMediaSessionHelper

    WebProcess/Inspector/WebInspectorUIExtensionController

    WebProcess/WebCoreSupport/WebDeviceOrientationUpdateProvider

    WebProcess/WebPage/ViewGestureGeometryCollector
    WebProcess/WebPage/ViewUpdateDispatcher

    WebProcess/WebPage/Cocoa/TextCheckingControllerProxy

    WebProcess/WebPage/RemoteLayerTree/RemoteScrollingCoordinator

    WebProcess/cocoa/PlaybackSessionManager
    WebProcess/cocoa/RemoteCaptureSampleManager
    WebProcess/cocoa/UserMediaCaptureManager
    WebProcess/cocoa/VideoPresentationManager
)

# LogStream uses two-stage generation: LogMessages.in -> LogStream.messages.in -> LogStreamMessageReceiver.cpp.
# Stage 1 runs generate-derived-log-sources.py (Xcode's DerivedSources.make equivalent).

set(_log_messages_inputs
    ${WEBKIT_DIR}/Platform/LogMessages.in
    ${WEBCORE_DIR}/platform/LogMessages.in
)
set(_log_messages_generated
    ${WebKit_DERIVED_SOURCES_DIR}/LogStream.messages.in
    ${WebKit_DERIVED_SOURCES_DIR}/LogMessagesDeclarations.h
    ${WebKit_DERIVED_SOURCES_DIR}/LogMessagesImplementations.h
    ${WebKit_DERIVED_SOURCES_DIR}/WebKitLogClientDeclarations.h
    ${WebKit_DERIVED_SOURCES_DIR}/WebCoreLogClientDeclarations.h
)
add_custom_command(
    OUTPUT ${_log_messages_generated}
    DEPENDS
        ${WEBKIT_DIR}/Scripts/generate-derived-log-sources.py
        ${WEBCORE_DIR}/Scripts/generate-log-declarations.py
        ${_log_messages_inputs}
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${WEBCORE_DIR}/Scripts"
        ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-derived-log-sources.py
        ${_log_messages_inputs}
        ${_log_messages_generated}
        "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}"
    WORKING_DIRECTORY ${WebKit_DERIVED_SOURCES_DIR}
    VERBATIM
)

# Stage 2: GENERATE_MESSAGE_SOURCES handles LogStream via the derived .messages.in path
# fallback in CMakeLists.txt, so LogStream appears in the MessageNames enum.
list(APPEND WebKit_MESSAGES_IN_FILES LogStream)

# 49 serialization.in files in Shared/Cocoa + 10 in Shared/cf; the original list had 6. Each
# unregistered file leaves its type forward-declared-only -> 'incomplete type' errors in
# WebKitPlatformGeneratedSerializers.mm. Glob keeps this in sync as WebKit adds types.
file(GLOB _webkit_cocoa_serialization_files RELATIVE "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/Shared/Cocoa/*.serialization.in"
    "${WEBKIT_DIR}/Shared/cf/*.serialization.in"
    "${WEBKIT_DIR}/Shared/mac/*.serialization.in"
    "${WEBKIT_DIR}/Shared/RemoteLayerTree/*.serialization.in"
    "${WEBKIT_DIR}/Shared/ApplePay/*.serialization.in"
    "${WEBKIT_DIR}/WebProcess/WebPage/RemoteLayerTree/*.serialization.in"
    "${WEBKIT_DIR}/Platform/cocoa/*.serialization.in"
)
list(APPEND WebKit_SERIALIZATION_IN_FILES ${_webkit_cocoa_serialization_files})
unset(_webkit_cocoa_serialization_files)

# Cocoa-only types in Shared/ not in the cross-platform base list. Can't glob
# Shared/*.serialization.in -- it would duplicate base entries and double-register types.
list(APPEND WebKit_SERIALIZATION_IN_FILES
    Shared/AdditionalFonts.serialization.in
    Shared/AlternativeTextClient.serialization.in
    Shared/AppPrivacyReportTestingData.serialization.in
    Shared/PushMessageForTesting.serialization.in
    Shared/TextAnimationTypes.serialization.in
    Shared/ViewWindowCoordinates.serialization.in
)

# PlaybackSessionModel.serialization.in is Cocoa-only; the cross-platform CMakeLists.txt omits it.
list(APPEND WebCore_SERIALIZATION_IN_FILES
    PlaybackSessionModel.serialization.in
)

# CoreIPC* and other .mm files compiled by .pbxproj directly, not via SourcesCocoa.txt.
file(GLOB _webkit_missing_cocoa_sources RELATIVE "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/Shared/Cocoa/CoreIPC*.mm"
    "${WEBKIT_DIR}/Shared/cf/CoreIPC*.mm"
    # Shared/mac/CoreIPCDDSecureActionContext.mm already in SourcesCocoa.txt
)
list(APPEND WebKit_SOURCES ${_webkit_missing_cocoa_sources})
unset(_webkit_missing_cocoa_sources)
list(APPEND WebKit_SOURCES
    NetworkProcess/cocoa/NetworkSoftLink.mm

    Platform/cocoa/_WKWebViewTextInputNotifications.mm

    Shared/AdditionalFonts.mm

    Shared/Cocoa/AnnotatedMachSendRight.mm
    Shared/Cocoa/ArgumentCodersCocoa.mm
    Shared/Cocoa/BackgroundFetchStateCocoa.mm
    Shared/Cocoa/CoreTextHelpers.mm
    Shared/Cocoa/DataDetectionResult.mm
    Shared/Cocoa/LaunchLogHook.mm
    Shared/Cocoa/WKKeyedCoder.mm
    Shared/Cocoa/WKProcessExtension.mm
    Shared/Cocoa/WebKit2InitializeCocoa.mm
    Shared/Cocoa/WebPushMessageCocoa.mm

    UIProcess/Cocoa/AuxiliaryProcessProxyCocoa.mm
    UIProcess/Cocoa/CSPExtensionUtilities.mm
    UIProcess/Cocoa/_WKWarningView.mm

    UIProcess/Downloads/DownloadProxyCocoa.mm

    UIProcess/Extensions/WebExtensionCommand.cpp
    UIProcess/Extensions/WebExtensionMenuItem.cpp

    UIProcess/RemoteLayerTree/cocoa/RemoteScrollingTreeCocoa.mm

    UIProcess/WebAuthentication/AuthenticatorManager.cpp

    UIProcess/WebAuthentication/Cocoa/AuthenticationServicesSoftLink.mm
    UIProcess/WebAuthentication/Cocoa/HidConnection.mm
    UIProcess/WebAuthentication/Cocoa/HidService.mm
    UIProcess/WebAuthentication/Cocoa/WebAuthenticatorCoordinatorProxy.mm

    UIProcess/WebAuthentication/Virtual/VirtualAuthenticatorManager.cpp
    UIProcess/WebAuthentication/Virtual/VirtualAuthenticatorUtils.mm
    UIProcess/WebAuthentication/Virtual/VirtualHidConnection.cpp
    UIProcess/WebAuthentication/Virtual/VirtualLocalConnection.mm
    UIProcess/WebAuthentication/Virtual/VirtualService.mm

    UIProcess/WebAuthentication/fido/CtapAuthenticator.cpp
    UIProcess/WebAuthentication/fido/CtapCcidDriver.cpp
    UIProcess/WebAuthentication/fido/CtapHidDriver.cpp

    UIProcess/mac/_WKCaptionStyleMenuControllerAVKitMac.mm
    UIProcess/mac/_WKCaptionStyleMenuControllerMac.mm

    WebProcess/Inspector/ServiceWorkerDebuggableFrontendChannel.cpp
    WebProcess/Inspector/ServiceWorkerDebuggableProxy.cpp

    WebProcess/Network/WebMockContentFilterManager.cpp

    WebProcess/WebPage/Cocoa/PositionInformationForWebPage.mm

    WebProcess/cocoa/TextTrackRepresentationCocoa.mm

    webpushd/ApplePushServiceConnection.mm
    webpushd/MockPushServiceConnection.mm
    webpushd/PushClientConnection.mm
    webpushd/PushService.mm
    webpushd/PushServiceConnection.mm
    webpushd/WebClipCache.mm
    webpushd/WebPushDaemon.mm
    webpushd/_WKMockUserNotificationCenter.mm
)

# Generated JSWebExtension*.mm IDL bindings need -fobjc-arc per file.
# NB: file(GLOB) runs at configure time; on first clean build these don't exist yet.
file(GLOB _webkit_js_extension_sources "${WebKit_DERIVED_SOURCES_DIR}/JSWebExtension*.mm")
set_source_files_properties(${_webkit_js_extension_sources} PROPERTIES COMPILE_FLAGS "-fobjc-arc" GENERATED TRUE)
list(APPEND WebKit_SOURCES ${_webkit_js_extension_sources})
unset(_webkit_js_extension_sources)

find_library(CRYPTOTOKENKIT_LIBRARY CryptoTokenKit)
find_library(USERNOTIFICATIONS_LIBRARY UserNotifications)
find_library(WRITINGTOOLS_LIBRARY WritingTools HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(APPLEPUSHSERVICE_LIBRARY ApplePushService HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
list(APPEND WebKit_PRIVATE_LIBRARIES
    ${CRYPTOTOKENKIT_LIBRARY}
    ${USERNOTIFICATIONS_LIBRARY}
    ${WRITINGTOOLS_LIBRARY}
    ${APPLEPUSHSERVICE_LIBRARY}
    "-weak_framework PowerLog"
)
# FIXME: Replace with targeted -U flags or explicit stubs.
# https://bugs.webkit.org/show_bug.cgi?id=312067
list(APPEND WebKit_PRIVATE_LIBRARIES "-Wl,-undefined,dynamic_lookup")

list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/WebPushDaemonConstants.h

    Shared/API/Cocoa/RemoteObjectInvocation.h
    Shared/API/Cocoa/RemoteObjectRegistry.h
    Shared/API/Cocoa/WKBrowsingContextHandle.h
    Shared/API/Cocoa/WKDataDetectorTypes.h

    UIProcess/Cocoa/_WKCaptionStyleMenuController.h

    UIProcess/Extensions/Cocoa/_WKWebExtensionDeclarativeNetRequestRule.h
    UIProcess/Extensions/Cocoa/_WKWebExtensionDeclarativeNetRequestTranslator.h

    WebProcess/Extensions/Cocoa/_WKWebExtensionWebNavigationURLFilter.h
    WebProcess/Extensions/Cocoa/_WKWebExtensionWebRequestFilter.h
    Shared/API/Cocoa/WKDragDestinationAction.h
    Shared/API/Cocoa/WKFoundation.h
    Shared/API/Cocoa/WKMain.h
    Shared/API/Cocoa/WKRemoteObject.h
    Shared/API/Cocoa/WKRemoteObjectCoder.h
    Shared/API/Cocoa/WebKit.h
    Shared/API/Cocoa/WebKitPrivate.h
    Shared/API/Cocoa/_WKFrameHandle.h
    Shared/API/Cocoa/_WKHitTestResult.h
    Shared/API/Cocoa/_WKNSFileManagerExtras.h
    Shared/API/Cocoa/_WKNSWindowExtras.h
    Shared/API/Cocoa/_WKRemoteObjectInterface.h
    Shared/API/Cocoa/_WKRemoteObjectRegistry.h
    Shared/API/Cocoa/_WKRenderingProgressEvents.h
    Shared/API/Cocoa/_WKSameDocumentNavigationType.h

    Shared/API/c/cf/WKErrorCF.h
    Shared/API/c/cf/WKStringCF.h
    Shared/API/c/cf/WKURLCF.h

    Shared/API/c/cg/WKImageCG.h

    Shared/API/c/mac/WKBaseMac.h
    Shared/API/c/mac/WKCertificateInfoMac.h
    Shared/API/c/mac/WKObjCTypeWrapperRef.h
    Shared/API/c/mac/WKURLRequestNS.h
    Shared/API/c/mac/WKURLResponseNS.h
    Shared/API/c/mac/WKWebArchiveRef.h
    Shared/API/c/mac/WKWebArchiveResource.h

    UIProcess/API/C/mac/WKContextPrivateMac.h
    UIProcess/API/C/mac/WKInspectorPrivateMac.h
    UIProcess/API/C/mac/WKNotificationPrivateMac.h
    UIProcess/API/C/mac/WKPagePrivateMac.h
    UIProcess/API/C/mac/WKProtectionSpaceNS.h
    UIProcess/API/C/mac/WKWebsiteDataStoreRefPrivateMac.h

    UIProcess/API/Cocoa/NSAttributedString.h
    UIProcess/API/Cocoa/NSAttributedStringPrivate.h
    UIProcess/API/Cocoa/PageLoadStateObserver.h
    UIProcess/API/Cocoa/WKBackForwardList.h
    UIProcess/API/Cocoa/WKBackForwardListItem.h
    UIProcess/API/Cocoa/WKBackForwardListItemPrivate.h
    UIProcess/API/Cocoa/WKBackForwardListPrivate.h
    UIProcess/API/Cocoa/WKBrowsingContextController.h
    UIProcess/API/Cocoa/WKBrowsingContextControllerPrivate.h
    UIProcess/API/Cocoa/WKBrowsingContextGroup.h
    UIProcess/API/Cocoa/WKBrowsingContextGroupPrivate.h
    UIProcess/API/Cocoa/WKBrowsingContextHistoryDelegate.h
    UIProcess/API/Cocoa/WKBrowsingContextLoadDelegate.h
    UIProcess/API/Cocoa/WKBrowsingContextLoadDelegatePrivate.h
    UIProcess/API/Cocoa/WKBrowsingContextPolicyDelegate.h
    UIProcess/API/Cocoa/WKContentRuleList.h
    UIProcess/API/Cocoa/WKContentRuleListPrivate.h
    UIProcess/API/Cocoa/WKContentRuleListStore.h
    UIProcess/API/Cocoa/WKContentRuleListStorePrivate.h
    UIProcess/API/Cocoa/WKContentWorld.h
    UIProcess/API/Cocoa/WKContentWorldConfiguration.h
    UIProcess/API/Cocoa/WKContentWorldPrivate.h
    UIProcess/API/Cocoa/WKContextMenuElementInfo.h
    UIProcess/API/Cocoa/WKContextMenuElementInfoPrivate.h
    UIProcess/API/Cocoa/WKDownload.h
    UIProcess/API/Cocoa/WKDownloadDelegate.h
    UIProcess/API/Cocoa/WKError.h
    UIProcess/API/Cocoa/WKErrorPrivate.h
    UIProcess/API/Cocoa/WKFindConfiguration.h
    UIProcess/API/Cocoa/WKFindResult.h
    UIProcess/API/Cocoa/WKFrameInfo.h
    UIProcess/API/Cocoa/WKFrameInfoPrivate.h
    UIProcess/API/Cocoa/WKHTTPCookieStore.h
    UIProcess/API/Cocoa/WKHTTPCookieStorePrivate.h
    UIProcess/API/Cocoa/WKHistoryDelegatePrivate.h
    UIProcess/API/Cocoa/WKMenuItemIdentifiersPrivate.h
    UIProcess/API/Cocoa/WKNSURLAuthenticationChallenge.h
    UIProcess/API/Cocoa/WKNavigation.h
    UIProcess/API/Cocoa/WKNavigationAction.h
    UIProcess/API/Cocoa/WKNavigationActionPrivate.h
    UIProcess/API/Cocoa/WKNavigationData.h
    UIProcess/API/Cocoa/WKNavigationDelegate.h
    UIProcess/API/Cocoa/WKNavigationDelegatePrivate.h
    UIProcess/API/Cocoa/WKNavigationPrivate.h
    UIProcess/API/Cocoa/WKNavigationResponse.h
    UIProcess/API/Cocoa/WKNavigationResponsePrivate.h
    UIProcess/API/Cocoa/WKOpenPanelParameters.h
    UIProcess/API/Cocoa/WKOpenPanelParametersPrivate.h
    UIProcess/API/Cocoa/WKPDFConfiguration.h
    UIProcess/API/Cocoa/WKPreferences.h
    UIProcess/API/Cocoa/WKPreferencesPrivate.h
    UIProcess/API/Cocoa/WKPreviewActionItem.h
    UIProcess/API/Cocoa/WKPreviewActionItemIdentifiers.h
    UIProcess/API/Cocoa/WKPreviewElementInfo.h
    UIProcess/API/Cocoa/WKProcessPool.h
    UIProcess/API/Cocoa/WKProcessPoolPrivate.h
    UIProcess/API/Cocoa/WKScriptMessage.h
    UIProcess/API/Cocoa/WKScriptMessageHandler.h
    UIProcess/API/Cocoa/WKScriptMessageHandlerWithReply.h
    UIProcess/API/Cocoa/WKSecurityOrigin.h
    UIProcess/API/Cocoa/WKSecurityOriginPrivate.h
    UIProcess/API/Cocoa/WKSnapshotConfiguration.h
    UIProcess/API/Cocoa/WKUIDelegate.h
    UIProcess/API/Cocoa/WKUIDelegatePrivate.h
    UIProcess/API/Cocoa/WKURLSchemeHandler.h
    UIProcess/API/Cocoa/WKURLSchemeTask.h
    UIProcess/API/Cocoa/WKURLSchemeTaskPrivate.h
    UIProcess/API/Cocoa/WKUserContentController.h
    UIProcess/API/Cocoa/WKUserContentControllerPrivate.h
    UIProcess/API/Cocoa/WKUserScript.h
    UIProcess/API/Cocoa/WKUserScriptPrivate.h
    UIProcess/API/Cocoa/WKView.h
    UIProcess/API/Cocoa/WKViewPrivate.h
    UIProcess/API/Cocoa/WKWebArchive.h
    UIProcess/API/Cocoa/WKWebView.h
    UIProcess/API/Cocoa/WKWebViewConfiguration.h
    UIProcess/API/Cocoa/WKWebViewConfigurationPrivate.h
    UIProcess/API/Cocoa/WKWebViewPrivate.h
    UIProcess/API/Cocoa/WKWebViewPrivateForTesting.h
    UIProcess/API/Cocoa/WKWebpagePreferences.h
    UIProcess/API/Cocoa/WKWebpagePreferencesPrivate.h
    UIProcess/API/Cocoa/WKWebsiteDataRecord.h
    UIProcess/API/Cocoa/WKWebsiteDataRecordPrivate.h
    UIProcess/API/Cocoa/WKWebsiteDataStore.h
    UIProcess/API/Cocoa/WKWebsiteDataStorePrivate.h
    UIProcess/API/Cocoa/WKWindowFeatures.h
    UIProcess/API/Cocoa/WKWindowFeaturesPrivate.h
    UIProcess/API/Cocoa/WebKitLegacy.h
    UIProcess/API/Cocoa/_WKActivatedElementInfo.h
    UIProcess/API/Cocoa/_WKAppHighlight.h
    UIProcess/API/Cocoa/_WKAppHighlightDelegate.h
    UIProcess/API/Cocoa/_WKApplicationManifest.h
    UIProcess/API/Cocoa/_WKAttachment.h
    UIProcess/API/Cocoa/_WKAuthenticationExtensionsClientInputs.h
    UIProcess/API/Cocoa/_WKAuthenticationExtensionsClientOutputs.h
    UIProcess/API/Cocoa/_WKAuthenticatorAssertionResponse.h
    UIProcess/API/Cocoa/_WKAuthenticatorAttachment.h
    UIProcess/API/Cocoa/_WKAuthenticatorAttestationResponse.h
    UIProcess/API/Cocoa/_WKAuthenticatorResponse.h
    UIProcess/API/Cocoa/_WKAuthenticatorSelectionCriteria.h
    UIProcess/API/Cocoa/_WKAutomationDelegate.h
    UIProcess/API/Cocoa/_WKAutomationSession.h
    UIProcess/API/Cocoa/_WKAutomationSessionConfiguration.h
    UIProcess/API/Cocoa/_WKAutomationSessionDelegate.h
    UIProcess/API/Cocoa/_WKContentRuleListAction.h
    UIProcess/API/Cocoa/_WKContextMenuElementInfo.h
    UIProcess/API/Cocoa/_WKCustomHeaderFields.h
    UIProcess/API/Cocoa/_WKDiagnosticLoggingDelegate.h
    UIProcess/API/Cocoa/_WKDownload.h
    UIProcess/API/Cocoa/_WKDownloadDelegate.h
    UIProcess/API/Cocoa/_WKElementAction.h
    UIProcess/API/Cocoa/_WKErrorRecoveryAttempting.h
    UIProcess/API/Cocoa/_WKExperimentalFeature.h
    UIProcess/API/Cocoa/_WKFindDelegate.h
    UIProcess/API/Cocoa/_WKFindOptions.h
    UIProcess/API/Cocoa/_WKFocusedElementInfo.h
    UIProcess/API/Cocoa/_WKFormInputSession.h
    UIProcess/API/Cocoa/_WKFrameTreeNode.h
    UIProcess/API/Cocoa/_WKFullscreenDelegate.h
    UIProcess/API/Cocoa/_WKGeolocationCoreLocationProvider.h
    UIProcess/API/Cocoa/_WKGeolocationPosition.h
    UIProcess/API/Cocoa/_WKIconLoadingDelegate.h
    UIProcess/API/Cocoa/_WKInputDelegate.h
    UIProcess/API/Cocoa/_WKInspector.h
    UIProcess/API/Cocoa/_WKInspectorConfiguration.h
    UIProcess/API/Cocoa/_WKInspectorDebuggableInfo.h
    UIProcess/API/Cocoa/_WKInspectorDelegate.h
    UIProcess/API/Cocoa/_WKInspectorExtension.h
    UIProcess/API/Cocoa/_WKInspectorExtensionDelegate.h
    UIProcess/API/Cocoa/_WKInspectorExtensionHost.h
    UIProcess/API/Cocoa/_WKInspectorIBActions.h
    UIProcess/API/Cocoa/_WKInspectorPrivate.h
    UIProcess/API/Cocoa/_WKInspectorPrivateForTesting.h
    UIProcess/API/Cocoa/_WKInspectorWindow.h
    UIProcess/API/Cocoa/_WKInternalDebugFeature.h
    UIProcess/API/Cocoa/_WKLayoutMode.h
    UIProcess/API/Cocoa/_WKLinkIconParameters.h
    UIProcess/API/Cocoa/_WKOverlayScrollbarStyle.h
    UIProcess/API/Cocoa/_WKProcessPoolConfiguration.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialCreationOptions.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialDescriptor.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialEntity.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialParameters.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialRelyingPartyEntity.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialRequestOptions.h
    UIProcess/API/Cocoa/_WKPublicKeyCredentialUserEntity.h
    UIProcess/API/Cocoa/_WKRemoteWebInspectorViewController.h
    UIProcess/API/Cocoa/_WKRemoteWebInspectorViewControllerPrivate.h
    UIProcess/API/Cocoa/_WKResourceLoadDelegate.h
    UIProcess/API/Cocoa/_WKResourceLoadInfo.h
    UIProcess/API/Cocoa/_WKResourceLoadStatisticsFirstParty.h
    UIProcess/API/Cocoa/_WKResourceLoadStatisticsThirdParty.h
    UIProcess/API/Cocoa/_WKSessionState.h
    UIProcess/API/Cocoa/_WKSystemPreferences.h
    UIProcess/API/Cocoa/_WKTapHandlingResult.h
    UIProcess/API/Cocoa/_WKTextInputContext.h
    UIProcess/API/Cocoa/_WKTextManipulationConfiguration.h
    UIProcess/API/Cocoa/_WKTextManipulationDelegate.h
    UIProcess/API/Cocoa/_WKTextManipulationExclusionRule.h
    UIProcess/API/Cocoa/_WKTextManipulationItem.h
    UIProcess/API/Cocoa/_WKTextManipulationToken.h
    UIProcess/API/Cocoa/_WKThumbnailView.h
    UIProcess/API/Cocoa/_WKUserContentWorld.h
    UIProcess/API/Cocoa/_WKUserInitiatedAction.h
    UIProcess/API/Cocoa/_WKUserStyleSheet.h
    UIProcess/API/Cocoa/_WKUserVerificationRequirement.h
    UIProcess/API/Cocoa/_WKVisitedLinkStore.h
    UIProcess/API/Cocoa/_WKWebAuthenticationAssertionResponse.h
    UIProcess/API/Cocoa/_WKWebAuthenticationPanel.h
    UIProcess/API/Cocoa/_WKWebAuthenticationPanelForTesting.h
    UIProcess/API/Cocoa/_WKWebsiteDataSize.h
    UIProcess/API/Cocoa/_WKWebsiteDataStoreConfiguration.h
    UIProcess/API/Cocoa/_WKWebsiteDataStoreDelegate.h

    UIProcess/API/mac/WKWebViewPrivateForTestingMac.h

    UIProcess/Cocoa/WKShareSheet.h

    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessBundleParameters.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInCSSStyleDeclarationHandle.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInEditingDelegate.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInFormDelegatePrivate.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInFrame.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInFramePrivate.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInHitTestResult.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInLoadDelegate.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInNodeHandle.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInNodeHandlePrivate.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInPageGroup.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInRangeHandle.h
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInScriptWorld.h

    WebProcess/InjectedBundle/API/mac/WKDOMDocument.h
    WebProcess/InjectedBundle/API/mac/WKDOMElement.h
    WebProcess/InjectedBundle/API/mac/WKDOMInternals.h
    WebProcess/InjectedBundle/API/mac/WKDOMNode.h
    WebProcess/InjectedBundle/API/mac/WKDOMNodePrivate.h
    WebProcess/InjectedBundle/API/mac/WKDOMRange.h
    WebProcess/InjectedBundle/API/mac/WKDOMRangePrivate.h
    WebProcess/InjectedBundle/API/mac/WKDOMText.h
    WebProcess/InjectedBundle/API/mac/WKDOMTextIterator.h
    WebProcess/InjectedBundle/API/mac/WKWebProcessPlugIn.h
    WebProcess/InjectedBundle/API/mac/WKWebProcessPlugInBrowserContextController.h
    WebProcess/InjectedBundle/API/mac/WKWebProcessPlugInBrowserContextControllerPrivate.h
    WebProcess/InjectedBundle/API/mac/WKWebProcessPlugInPrivate.h
)
# WEBKIT_COPY_FILES symlinks on APPLE, so glob is safe (same inode avoids duplicate @interface).
file(GLOB _webkit_api_headers RELATIVE "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/UIProcess/API/Cocoa/*.h"
    "${WEBKIT_DIR}/Shared/API/Cocoa/*.h"
    "${WEBKIT_DIR}/Shared/mac/*.h"
    "${WEBKIT_DIR}/GPUProcess/graphics/Model/*.h"
    "${WEBKIT_DIR}/WebKitSwift/IdentityDocumentServices/*.h"
    "${WEBKIT_DIR}/UIProcess/DigitalCredentials/*.h"
    "${WEBKIT_DIR}/UIProcess/ios/fullscreen/*.h"
)
list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS ${_webkit_api_headers})
list(REMOVE_DUPLICATES WebKit_PUBLIC_FRAMEWORK_HEADERS)
unset(_webkit_api_headers)

set(WebKit_FORWARDING_HEADERS_DIRECTORIES
    Platform
    Shared

    NetworkProcess/Downloads

    Platform/IPC

    Shared/API
    Shared/Cocoa

    Shared/API/Cocoa
    Shared/API/c

    Shared/API/c/cf
    Shared/API/c/mac

    UIProcess/Cocoa

    UIProcess/API/C
    UIProcess/API/Cocoa
    UIProcess/API/cpp

    UIProcess/API/C/Cocoa
    UIProcess/API/C/mac

    WebProcess/InjectedBundle/API/Cocoa
    WebProcess/InjectedBundle/API/c
    WebProcess/InjectedBundle/API/mac
)

# FIXME: Forwarding headers should be complete copies of the header.
set(WebKitLegacyForwardingHeaders
    DOM.h
    DOMCore.h
    DOMElement.h
    DOMException.h
    DOMObject.h
    DOMPrivate.h
    WebApplicationCache.h
    WebCache.h
    WebCoreStatistics.h
    WebDOMOperations.h
    WebDOMOperationsPrivate.h
    WebDatabaseManagerPrivate.h
    WebDataSource.h
    WebDataSourcePrivate.h
    WebDefaultPolicyDelegate.h
    WebDeviceOrientation.h
    WebDeviceOrientationProviderMock.h
    WebDocument.h
    WebDocumentPrivate.h
    WebDynamicScrollBarsView.h
    WebEditingDelegate.h
    WebFrame.h
    WebFramePrivate.h
    WebFrameViewPrivate.h
    WebGeolocationPosition.h
    WebHTMLRepresentation.h
    WebHTMLView.h
    WebHTMLViewPrivate.h
    WebHistory.h
    WebHistoryItem.h
    WebHistoryItemPrivate.h
    WebHistoryPrivate.h
    WebIconDatabasePrivate.h
    WebInspector.h
    WebInspectorPrivate.h
    WebKitNSStringExtras.h
    WebNSURLExtras.h
    WebNavigationData.h
    WebNotification.h
    WebPluginDatabase.h
    WebPolicyDelegate.h
    WebPolicyDelegatePrivate.h
    WebPreferenceKeysPrivate.h
    WebPreferences.h
    WebPreferencesPrivate.h
    WebQuotaManager.h
    WebScriptWorld.h
    WebSecurityOriginPrivate.h
    WebStorageManagerPrivate.h
    WebTypesInternal.h
    WebUIDelegate.h
    WebUIDelegatePrivate.h
    WebView.h
    WebViewPrivate
    WebViewPrivate.h
)

set(ObjCForwardingHeaders
    DOMAbstractView.h
    DOMAttr.h
    DOMBeforeLoadEvent.h
    DOMBlob.h
    DOMCDATASection.h
    DOMCSSCharsetRule.h
    DOMCSSFontFaceRule.h
    DOMCSSImportRule.h
    DOMCSSKeyframeRule.h
    DOMCSSKeyframesRule.h
    DOMCSSMediaRule.h
    DOMCSSPageRule.h
    DOMCSSPrimitiveValue.h
    DOMCSSRule.h
    DOMCSSRuleList.h
    DOMCSSStyleDeclaration.h
    DOMCSSStyleRule.h
    DOMCSSStyleSheet.h
    DOMCSSSupportsRule.h
    DOMCSSUnknownRule.h
    DOMCSSValue.h
    DOMCSSValueList.h
    DOMCharacterData.h
    DOMComment.h
    DOMCounter.h
    DOMDOMImplementation.h
    DOMDOMNamedFlowCollection.h
    DOMDOMTokenList.h
    DOMDocument.h
    DOMDocumentFragment.h
    DOMDocumentType.h
    DOMElement.h
    DOMEntity.h
    DOMEntityReference.h
    DOMEvent.h
    DOMEventException.h
    DOMEventListener.h
    DOMEventTarget.h
    DOMFile.h
    DOMFileList.h
    DOMHTMLAnchorElement.h
    DOMHTMLAppletElement.h
    DOMHTMLAreaElement.h
    DOMHTMLBRElement.h
    DOMHTMLBaseElement.h
    DOMHTMLBaseFontElement.h
    DOMHTMLBodyElement.h
    DOMHTMLButtonElement.h
    DOMHTMLCanvasElement.h
    DOMHTMLCollection.h
    DOMHTMLDListElement.h
    DOMHTMLDirectoryElement.h
    DOMHTMLDivElement.h
    DOMHTMLDocument.h
    DOMHTMLElement.h
    DOMHTMLEmbedElement.h
    DOMHTMLFieldSetElement.h
    DOMHTMLFontElement.h
    DOMHTMLFormElement.h
    DOMHTMLFrameElement.h
    DOMHTMLFrameSetElement.h
    DOMHTMLHRElement.h
    DOMHTMLHeadElement.h
    DOMHTMLHeadingElement.h
    DOMHTMLHtmlElement.h
    DOMHTMLIFrameElement.h
    DOMHTMLImageElement.h
    DOMHTMLInputElement.h
    DOMHTMLInputElementPrivate.h
    DOMHTMLLIElement.h
    DOMHTMLLabelElement.h
    DOMHTMLLegendElement.h
    DOMHTMLLinkElement.h
    DOMHTMLMapElement.h
    DOMHTMLMarqueeElement.h
    DOMHTMLMediaElement.h
    DOMHTMLMenuElement.h
    DOMHTMLMetaElement.h
    DOMHTMLModElement.h
    DOMHTMLOListElement.h
    DOMHTMLObjectElement.h
    DOMHTMLOptGroupElement.h
    DOMHTMLOptionElement.h
    DOMHTMLOptionsCollection.h
    DOMHTMLParagraphElement.h
    DOMHTMLParamElement.h
    DOMHTMLPreElement.h
    DOMHTMLQuoteElement.h
    DOMHTMLScriptElement.h
    DOMHTMLSelectElement.h
    DOMHTMLStyleElement.h
    DOMHTMLTableCaptionElement.h
    DOMHTMLTableCellElement.h
    DOMHTMLTableColElement.h
    DOMHTMLTableElement.h
    DOMHTMLTableRowElement.h
    DOMHTMLTableSectionElement.h
    DOMHTMLTextAreaElement.h
    DOMHTMLTitleElement.h
    DOMHTMLUListElement.h
    DOMHTMLVideoElement.h
    DOMImplementation.h
    DOMKeyboardEvent.h
    DOMMediaError.h
    DOMMediaList.h
    DOMMessageEvent.h
    DOMMessagePort.h
    DOMMouseEvent.h
    DOMMutationEvent.h
    DOMNamedNodeMap.h
    DOMNode.h
    DOMNodeFilter.h
    DOMNodeIterator.h
    DOMNodeList.h
    DOMOverflowEvent.h
    DOMProcessingInstruction.h
    DOMProgressEvent.h
    DOMRGBColor.h
    DOMRange.h
    DOMRangeException.h
    DOMRect.h
    DOMStyleSheet.h
    DOMStyleSheetList.h
    DOMText.h
    DOMTextEvent.h
    DOMTimeRanges.h
    DOMTreeWalker.h
    DOMUIEvent.h
    DOMValidityState.h
    DOMWebKitCSSFilterValue.h
    DOMWebKitCSSRegionRule.h
    DOMWebKitCSSTransformValue.h
    DOMWebKitNamedFlow.h
    DOMWheelEvent.h
    DOMXPathException.h
    DOMXPathExpression.h
    DOMXPathNSResolver.h
    DOMXPathResult.h
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION}")
target_link_options(WebKit PRIVATE -lsandbox -framework AuthKit)

# Match WebKit.xcconfig REEXPORTED_FRAMEWORK_NAMES / REEXPORTED_LIBRARY_NAMES so
# the CMake-built framework exports the same ABI as the Xcode build. Without the
# WebKitLegacy re-export, clients that link WebKit.framework expecting WK1
# symbols (e.g. _OBJC_CLASS_$_WebView, used by Xcode's view-debugger support
# dylib) fail at load when DYLD_FRAMEWORK_PATH points at this build.
#
# WebKitLegacy is intentionally absent from WebKit_PRIVATE_LIBRARIES above: ld64
# resolves a dylib's load-command type by its last reference on the link line,
# and a plain target_link_libraries entry would override this LC_REEXPORT_DYLIB
# with LC_LOAD_DYLIB. Link it solely via -reexport_library here; the explicit
# add_dependencies preserves build ordering.
add_dependencies(WebKit WebKitLegacy)
target_link_options(WebKit PRIVATE
    "-Wl,-reexport_library,$<TARGET_LINKER_FILE:WebKitLegacy>"
    "-Wl,-reexport-lobjc"
)

set(WebKit_OUTPUT_NAME WebKit)

# XPC Services

function(WEBKIT_DEFINE_XPC_SERVICES)
    # _WebKit runloop type is obsolete (macOS < 11.0); modern libxpc requires NSRunLoop
    # or the XPC event handler never fires and WebContent hangs.
    set(RUNLOOP_TYPE NSRunLoop)
    set(WebKit_XPC_SERVICE_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/XPCServices)
    # Relative symlink (matches Xcode's framework layout). Absolute symlinks work for launchd
    # but break if the build dir is moved/copied. Parent dir must exist for CREATE_LINK.
    make_directory("${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework")
    file(CREATE_LINK "Versions/Current/XPCServices"
                     "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices" SYMBOLIC)

    function(WEBKIT_XPC_SERVICE _target _bundle_identifier _info_plist _executable_name)
        set(_service_dir ${WebKit_XPC_SERVICE_DIR}/${_bundle_identifier}.xpc/Contents)
        make_directory(${_service_dir}/MacOS)
        make_directory(${_service_dir}/_CodeSignature)
        make_directory(${_service_dir}/Resources)

        # FIXME: These version strings don't match Xcode's.
        set(BUNDLE_VERSION ${WEBKIT_VERSION})
        set(SHORT_VERSION_STRING ${WEBKIT_VERSION_MAJOR})
        set(BUNDLE_VERSION ${WEBKIT_VERSION})
        set(EXECUTABLE_NAME ${_executable_name})
        set(PRODUCT_BUNDLE_IDENTIFIER ${_bundle_identifier})
        set(PRODUCT_NAME ${_bundle_identifier})
        configure_file(${_info_plist} ${_service_dir}/Info.plist)

        set_target_properties(${_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_service_dir}/MacOS")
    endfunction()

    WEBKIT_XPC_SERVICE(WebProcess
        "com.apple.WebKit.WebContent"
        ${WEBKIT_DIR}/WebProcess/EntryPoint/Cocoa/XPCService/WebContentService/Info-OSX.plist
        ${WebProcess_OUTPUT_NAME})

    WEBKIT_XPC_SERVICE(NetworkProcess
        "com.apple.WebKit.Networking"
        ${WEBKIT_DIR}/NetworkProcess/EntryPoint/Cocoa/XPCService/NetworkService/Info-OSX.plist
        ${NetworkProcess_OUTPUT_NAME})

    if (ENABLE_GPU_PROCESS)
        WEBKIT_XPC_SERVICE(GPUProcess
            "com.apple.WebKit.GPU"
            ${WEBKIT_DIR}/GPUProcess/EntryPoint/Cocoa/XPCService/GPUService/Info-OSX.plist
            ${GPUProcess_OUTPUT_NAME})
    endif ()

    # EnhancedSecurity and CaptivePortal WebContent variants -- without these XPC bundles,
    # process swaps for enhanced security tracking or Lockdown Mode fail with
    # "Invalid connection identifier" and navigation hangs.
    function(WEBKIT_WEBCONTENT_VARIANT _variant)
        set(_target WebProcess${_variant})
        set(_exec_name com.apple.WebKit.WebContent.${_variant}.Development)
        add_executable(${_target} ${WebProcess_SOURCES})
        target_link_libraries(${_target} PRIVATE WebKit)
        target_include_directories(${_target} PRIVATE
            ${CMAKE_BINARY_DIR}
            $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
        target_compile_options(${_target} PRIVATE -Wno-unused-parameter)
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME ${_exec_name})
        WEBKIT_XPC_SERVICE(${_target}
            "com.apple.WebKit.WebContent.${_variant}"
            ${WEBKIT_DIR}/WebProcess/EntryPoint/Cocoa/XPCService/WebContentService/Info-OSX.plist
            ${_exec_name})
    endfunction()
    WEBKIT_WEBCONTENT_VARIANT(EnhancedSecurity)
    WEBKIT_WEBCONTENT_VARIANT(CaptivePortal)

    set(WebKit_RESOURCES_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/Resources)
    # Sandbox profile preprocessing uses raw clang -E, which doesn't get cmake's
    # add_compile_options. Build the same -isystem flags that OptionsMac.cmake sets.
    set(_sb_extra_includes "")
    file(GLOB _sb_additions "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/macosx*-additions.sdk/usr/local/include")
    list(SORT _sb_additions)
    list(REVERSE _sb_additions)
    foreach (_d IN LISTS _sb_additions)
        if (EXISTS "${_d}/AvailabilityProhibitedInternal.h")
            set(_sb_extra_includes "-isystem" "${_d}")
            break ()
        endif ()
    endforeach ()
    if (EXISTS "${CMAKE_BINARY_DIR}/generated-stubs/AppleFeatures/AppleFeatures.h")
        list(APPEND _sb_extra_includes "-isystem" "${CMAKE_BINARY_DIR}/generated-stubs")
    endif ()
    # Sandbox profiles gate ASan-required syscalls (SYS_sigaltstack, ...) on
    # #if ASAN_ENABLED, which wtf/Compiler.h derives from
    # __has_feature(address_sanitizer). Pass -fsanitize so the preprocessor sees
    # the same feature set the compiled code does -- mirrors $(SANITIZE_FLAGS) in
    # DerivedSources.make. Without this the WebContent sandbox blocks
    # sigaltstack() and ASan CHECK-fails inside __asan_handle_no_return.
    if (ENABLE_SANITIZERS)
        foreach (_san IN LISTS ENABLE_SANITIZERS)
            list(APPEND _sb_extra_includes "-fsanitize=${_san}")
        endforeach ()
    endif ()

    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.sb COMMAND
        grep -o "^[^;]*" ${WEBKIT_DIR}/WebProcess/com.apple.WebProcess.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.sb
        VERBATIM)
    list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.sb)

    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebKit.NetworkProcess.sb COMMAND
        grep -o "^[^;]*" ${WEBKIT_DIR}/NetworkProcess/mac/com.apple.WebKit.NetworkProcess.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebKit.NetworkProcess.sb
        VERBATIM)
    list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebKit.NetworkProcess.sb)

    if (ENABLE_GPU_PROCESS)
        add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebKit.GPUProcess.sb COMMAND
            grep -o "^[^;]*" ${WEBKIT_DIR}/GPUProcess/mac/com.apple.WebKit.GPUProcess.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebKit.GPUProcess.sb
            VERBATIM)
        list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebKit.GPUProcess.sb)
    endif ()
    if (ENABLE_WEB_PUSH_NOTIFICATIONS)
        add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebKit.webpushd.mac.sb COMMAND
            grep -o "^[^;]*" ${WEBKIT_DIR}/webpushd/mac/com.apple.WebKit.webpushd.mac.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebKit.webpushd.mac.sb
            VERBATIM)
        list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebKit.webpushd.mac.sb)
    endif ()
    add_custom_target(WebKitSandboxProfiles ALL DEPENDS ${WebKit_SB_FILES})
    add_dependencies(WebKit WebKitSandboxProfiles)

    add_custom_command(OUTPUT ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib COMMAND
        ibtool --compile ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib ${WEBKIT_DIR}/Resources/WebContentProcess.xib
        VERBATIM)
    add_custom_target(WebContentProcessNib ALL DEPENDS ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib)
    add_dependencies(WebKit WebContentProcessNib)
endfunction()

set(WebKit_GENERATED_SERIALIZERS_SUFFIX mm)

# SecureCoding codegen is Cocoa-only (includes ArgumentCodersCocoa.h).
list(APPEND WebKit_DERIVED_SOURCES
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedWebKitSecureCoding.h
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedWebKitSecureCoding.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
)
