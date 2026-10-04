add_compile_options("$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-std=c++2b>" "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-D__STDC_WANT_LIB_EXT1__>")
list(APPEND WebKit_COMPILE_OPTIONS
    "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-std=c++2b>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-D__STDC_WANT_LIB_EXT1__>"
)

find_library(NETWORK_LIBRARY Network)
find_library(SECURITY_LIBRARY Security)
find_library(UNIFORMTYPEIDENTIFIERS_LIBRARY UniformTypeIdentifiers)
find_library(AVFOUNDATION_LIBRARY AVFoundation)
find_library(CORESERVICES_LIBRARY CoreServices)
find_library(DEVICEIDENTITY_LIBRARY DeviceIdentity HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
if (NOT DEVICEIDENTITY_LIBRARY)
    set(DEVICEIDENTITY_LIBRARY "" CACHE FILEPATH "" FORCE)
endif ()

find_library(CFNETWORK_LIBRARY CFNetwork)
find_library(COREAUDIO_LIBRARY CoreAudio)
find_library(COREFOUNDATION_LIBRARY CoreFoundation)
find_library(COREGRAPHICS_LIBRARY CoreGraphics)
find_library(CORETEXT_LIBRARY CoreText)
find_library(FOUNDATION_LIBRARY Foundation)
find_library(GRAPHICSSERVICES_LIBRARY GraphicsServices HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(IMAGEIO_LIBRARY ImageIO)
find_library(IOKIT_LIBRARY IOKit)
find_library(IOSURFACE_LIBRARY IOSurface)
find_library(METAL_LIBRARY Metal)
find_library(MOBILECORESERVICES_LIBRARY MobileCoreServices)
find_library(SPRINGBOARDSERVICES_LIBRARY SpringBoardServices HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(UIKITSERVICES_LIBRARY UIKitServices HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(UIKIT_LIBRARY UIKit)
find_library(APPSTOREDAEMON_LIBRARY AppStoreDaemon HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(BACKBOARDSERVICES_LIBRARY BackBoardServices HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(CONTACTS_LIBRARY Contacts)
find_library(CORETELEPHONY_LIBRARY CoreTelephony)
find_library(FRONTBOARDSERVICES_LIBRARY FrontBoardServices HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(GAMECONTROLLERUI_LIBRARY GameControllerUI HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(INSTALLCOORDINATION_LIBRARY InstallCoordination HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(MOBILEKEYBAG_LIBRARY MobileKeyBag HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(NETWORKEXTENSION_LIBRARY NetworkExtension)
find_library(PDFKIT_LIBRARY PDFKit)
find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(CARBON_LIBRARY Carbon)
find_library(SECURITYINTERFACE_LIBRARY SecurityInterface)
find_library(QUARTZ_LIBRARY Quartz)
find_library(AVFAUDIO_LIBRARY AVFAudio HINTS ${AVFOUNDATION_LIBRARY}/Versions/*/Frameworks)

add_compile_options(
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-DHAVE_CORE_PREDICTION=1>"
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-DWK_XPC_SERVICE_SUFFIX=\".Development\">"
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-DWEBKIT_BUNDLE_VERSION=\"${WEBKIT_MAC_VERSION}\">"
)
list(APPEND WebKit_COMPILE_OPTIONS
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-DWK_XPC_SERVICE_SUFFIX=\".Development\">"
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-DWEBKIT_BUNDLE_VERSION=\"${WEBKIT_MAC_VERSION}\">"
)

set(MACOSX_FRAMEWORK_IDENTIFIER com.apple.WebKit)

# Used to substitute placeholders in Info.plist, which CMake writes into the
# bundle. Xcode's $(PLATFORM_NAME) is the lowercase SDK name, and its
# $(IOS_DEPLOYMENT_TARGET) is only set for the embedded SDKs.
set(BUNDLE_VERSION "${MACOSX_FRAMEWORK_BUNDLE_VERSION}")
set(SHORT_VERSION_STRING "${MACOSX_FRAMEWORK_SHORT_VERSION_STRING}")
set(PRODUCT_NAME "WebKit")
set(PRODUCT_BUNDLE_IDENTIFIER "com.apple.WebKit")
set(PLATFORM_NAME "${WEBKIT_SDK_NAME}")
# Xcode only fills this in for iOS (hence the platform-specific variable).
if (WEBKIT_SDK_IS_IOS_FAMILY)
    set(IOS_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif ()
set_target_properties(WebKit PROPERTIES
    MACOSX_FRAMEWORK_INFO_PLIST ${WEBKIT_DIR}/Info.plist)

include(Headers.cmake)

# Read by process-entitlements.sh.
set(WebKit_ENTITLEMENTS_DEPENDS
    ${WEBKIT_DIR}/Resources/cocoa/NotificationAllowList/EmbeddedForwardedNotifications.def
    ${WEBKIT_DIR}/Resources/cocoa/NotificationAllowList/ForwardedNotifications.def
    ${WEBKIT_DIR}/Resources/cocoa/NotificationAllowList/MacForwardedNotifications.def
    ${WEBKIT_DIR}/Resources/cocoa/NotificationAllowList/NonForwardedNotifications.def
)

# The iOS process extensions build with fixed entitlements, from
# Shared/AuxiliaryProcessExtensions.
if (NOT USE_EXTENSIONKIT)
    webkit_generate_entitlements(WebProcess
        USING Scripts/process-entitlements.sh
        DEPENDS ${WebKit_ENTITLEMENTS_DEPENDS}
        BUNDLE_IDENTIFIER com.apple.WebKit.WebContent)
    webkit_generate_entitlements(NetworkProcess
        USING Scripts/process-entitlements.sh
        DEPENDS ${WebKit_ENTITLEMENTS_DEPENDS}
        BUNDLE_IDENTIFIER com.apple.WebKit.Networking)
    webkit_generate_entitlements(GPUProcess
        USING Scripts/process-entitlements.sh
        DEPENDS ${WebKit_ENTITLEMENTS_DEPENDS}
        BUNDLE_IDENTIFIER com.apple.WebKit.GPU)
endif ()

list(APPEND WebKit_UNIFIED_SOURCE_LIST_FILES
    "SourcesCocoa.txt"

    "Platform/SourcesCocoa.txt"
)
# FIXME: Test building on iOS and then enable on iOS.
if (NOT WEBKIT_SDK_IS_IOS_FAMILY)
    list(APPEND WebKit_UNIFIED_SOURCE_LIST_FILES
        "SourcesCMakeCocoa.txt"
    )
endif ()

list(APPEND WebKit_SOURCES
    GPUProcess/media/RemoteAudioDestinationManager.cpp

    NetworkProcess/Downloads/cocoa/WKDownloadProgress.mm

    NetworkProcess/cocoa/LaunchServicesDatabaseObserver.mm
    NetworkProcess/cocoa/WebSocketTaskCocoa.mm

    NetworkProcess/webrtc/NetworkRTCProvider.cpp
    NetworkProcess/webrtc/NetworkRTCTCPSocketCocoa.mm
    NetworkProcess/webrtc/NetworkRTCUDPSocketCocoa.mm
    NetworkProcess/webrtc/NetworkRTCUtilitiesCocoa.mm

    Platform/IPC/cocoa/SharedFileHandleCocoa.cpp

    Platform/cocoa/WKMaterialHostingSupport.swift

    Shared/API/Cocoa/WKMain.mm

    Shared/Cocoa/DefaultWebBrowserChecks.mm
    Shared/Cocoa/XPCEndpoint.mm
    Shared/Cocoa/XPCEndpointClient.mm

    Shared/Model/WKStageModeOrbitSimulator.swift

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
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/Logger+Extras.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/ObjectiveCBlockConversions.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WebKitSwiftOverlay.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WKContentWorld.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/_WKRectEdge+Extras.swift

    UIProcess/Cocoa/PreferenceObserver.mm
    UIProcess/Cocoa/WKShareSheet.mm
    UIProcess/Cocoa/WKStorageAccessAlert.mm
    UIProcess/Cocoa/WebInspectorPreferenceObserver.mm
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKDeferringGestureRecognizer.swift

    UIProcess/PDF/WKPDFPageNumberIndicator.mm
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/_WKTextExtraction.swift

    UIProcess/ios/fullscreen/FullscreenTouchSecheuristicParameters.cpp

    WebProcess/WebAuthentication/WebAuthenticatorCoordinator.cpp

    WebProcess/cocoa/AudioSessionRoutingArbitrator.cpp
    WebProcess/cocoa/LaunchServicesDatabaseManager.mm

    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/c/mac/WKBundlePageMac.mm
)

# FIXME: Add the remaining `${WEBKIT_DIR}/UIProcess/API/Swift/` files once CMake has proper framework directories.

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/GPUProcess/graphics/Model"
    "${WEBKIT_DIR}/GPUProcess/mac"
    "${WEBKIT_DIR}/GPUProcess/media/cocoa"
    "${WEBKIT_DIR}/GPUProcess/media/ios"
    "${WEBKIT_DIR}/ModelProcess/cocoa"
    "${WEBKIT_DIR}/NetworkProcess/Cookies/cocoa"
    "${WEBKIT_DIR}/NetworkProcess/Downloads/cocoa"
    "${WEBKIT_DIR}/NetworkProcess/EntryPoint/Cocoa/Daemon"
    "${WEBKIT_DIR}/NetworkProcess/PrivateClickMeasurement/cocoa"
    "${WEBKIT_DIR}/NetworkProcess/cocoa"
    "${WEBKIT_DIR}/NetworkProcess/ios"
    "${WEBKIT_DIR}/NetworkProcess/mac"
    "${WEBKIT_DIR}/Platform/IPC/cocoa"
    "${WEBKIT_DIR}/Platform/IPC/darwin"
    "${WEBKIT_DIR}/Platform/IPC/mac"
    "${WEBKIT_DIR}/Platform/cg"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/Platform/classifier/cocoa"
    "${WEBKIT_DIR}/Platform/cocoa"
    "${WEBKIT_DIR}/Platform/ios"
    "${WEBKIT_DIR}/Platform/mac"
    "${WEBKIT_DIR}/Platform/spi/Cocoa"
    "${WEBKIT_DIR}/Platform/spi/Cocoa/Modules/WritingToolsUI_Private_SPI"
    "${WEBKIT_DIR}/Platform/spi/Cocoa/Modules/WritingTools_SPI"
    "${WEBKIT_DIR}/Platform/spi/ios"
    "${WEBKIT_DIR}/Platform/spi/mac"
    "${WEBKIT_DIR}/Platform/spi/visionos"
    "${WEBKIT_DIR}/Platform/unix"
    "${WEBKIT_DIR}/Shared/API/Cocoa"
    "${WEBKIT_DIR}/Shared/API/c/cf"
    "${WEBKIT_DIR}/Shared/API/c/cg"
    "${WEBKIT_DIR}/Shared/API/c/mac"
    "${WEBKIT_DIR}/Shared/ApplePay/cocoa/"
    "${WEBKIT_DIR}/Shared/Authentication/cocoa"
    "${WEBKIT_DIR}/Shared/Cocoa"
    "${WEBKIT_DIR}/Shared/Daemon"
    "${WEBKIT_DIR}/Shared/EntryPointUtilities/Cocoa/Daemon"
    "${WEBKIT_DIR}/Shared/EntryPointUtilities/Cocoa/XPCService"
    "${WEBKIT_DIR}/Shared/Sandbox"
    "${WEBKIT_DIR}/Shared/Scrolling"
    "${WEBKIT_DIR}/Shared/cf"
    "${WEBKIT_DIR}/Shared/ios"
    "${WEBKIT_DIR}/Shared/mac"
    "${WEBKIT_DIR}/UIProcess/API/C/mac"
    "${WEBKIT_DIR}/UIProcess/API/Cocoa"
    "${WEBKIT_DIR}/UIProcess/API/ios"
    "${WEBKIT_DIR}/UIProcess/API/mac"
    "${WEBKIT_DIR}/UIProcess/Authentication/cocoa"
    "${WEBKIT_DIR}/UIProcess/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Cocoa/GroupActivities"
    "${WEBKIT_DIR}/UIProcess/Cocoa/SOAuthorization"
    "${WEBKIT_DIR}/UIProcess/Cocoa/Separated"
    "${WEBKIT_DIR}/UIProcess/Cocoa/TextExtraction"
    "${WEBKIT_DIR}/UIProcess/Extensions/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Inspector/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Inspector/ios"
    "${WEBKIT_DIR}/UIProcess/Inspector/mac"
    "${WEBKIT_DIR}/UIProcess/Launcher/cocoa"
    "${WEBKIT_DIR}/UIProcess/Launcher/mac"
    "${WEBKIT_DIR}/UIProcess/Media"
    "${WEBKIT_DIR}/UIProcess/Media/cocoa"
    "${WEBKIT_DIR}/UIProcess/Notifications/cocoa"
    "${WEBKIT_DIR}/UIProcess/PDF"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/cocoa"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/ios"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/mac"
    "${WEBKIT_DIR}/UIProcess/WebAuthentication/Cocoa"
    "${WEBKIT_DIR}/UIProcess/WebAuthentication/Virtual"
    "${WEBKIT_DIR}/UIProcess/WebAuthentication/fido"
    "${WEBKIT_DIR}/UIProcess/WebsiteData/Cocoa"
    "${WEBKIT_DIR}/UIProcess/XR/ios"
    "${WEBKIT_DIR}/UIProcess/XR/xros"
    "${WEBKIT_DIR}/UIProcess/ios"
    "${WEBKIT_DIR}/UIProcess/ios/forms"
    "${WEBKIT_DIR}/UIProcess/ios/fullscreen"
    "${WEBKIT_DIR}/UIProcess/mac"
    "${WEBKIT_DIR}/UIProcess/mac/AppKitGestures"
    "${WEBKIT_DIR}/WebKitSwift/GroupActivities"
    "${WEBKIT_DIR}/WebKitSwift/IdentityDocumentServices"
    "${WEBKIT_DIR}/WebKitSwift/MarketplaceKit"
    "${WEBKIT_DIR}/WebKitSwift/Preview"
    "${WEBKIT_DIR}/WebKitSwift/TextAnimation"
    "${WEBKIT_DIR}/WebKitSwift/WritingTools"
    "${WEBKIT_DIR}/WebProcess/API/Cocoa"
    "${WEBKIT_DIR}/WebProcess/DigitalCredentials"
    "${WEBKIT_DIR}/WebProcess/Extensions/Cocoa"
    "${WEBKIT_DIR}/WebProcess/GPU/graphics/cocoa"
    "${WEBKIT_DIR}/WebProcess/GPU/media/cocoa"
    "${WEBKIT_DIR}/WebProcess/GPU/media/ios"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/Cocoa"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/mac"
    "${WEBKIT_DIR}/WebProcess/Inspector/mac"
    "${WEBKIT_DIR}/WebProcess/MediaSession"
    "${WEBKIT_DIR}/WebProcess/Model/mac"
    "${WEBKIT_DIR}/WebProcess/Plugins/PDF"
    "${WEBKIT_DIR}/WebProcess/Plugins/PDF/UnifiedPDF"
    "${WEBKIT_DIR}/WebProcess/WebAuthentication"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/cocoa"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/ios"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/mac"
    "${WEBKIT_DIR}/WebProcess/WebPage/Cocoa"
    "${WEBKIT_DIR}/WebProcess/WebPage/RemoteLayerTree"
    "${WEBKIT_DIR}/WebProcess/WebPage/ios"
    "${WEBKIT_DIR}/WebProcess/WebPage/mac"
    "${WEBKIT_DIR}/WebProcess/cocoa"
    "${WEBKIT_DIR}/WebProcess/cocoa/IdentityDocumentServices"
    "${WEBKIT_DIR}/WebProcess/mac"
    "${WEBKIT_DIR}/webpushd"
    "${WEBKIT_DIR}/webpushd/webpushtool"
    "${CMAKE_BINARY_DIR}/libwebrtc/PrivateHeaders"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source"
)

# Xcode appends .Development to the executable inside each service bundle for
# macOS and the simulators, through WK_XPC_SERVICE_SUFFIX in DebugRelease.xcconfig
# and EXECUTABLE_SUFFIX in BaseXPCService.xcconfig. Bundle names never carry it.
if (WEBKIT_SDK_IS_MACOS OR WEBKIT_SDK_IS_SIMULATOR)
    set(WK_XPC_SERVICE_SUFFIX .Development)
else ()
    set(WK_XPC_SERVICE_SUFFIX "")
endif ()

set(WebProcess_OUTPUT_NAME com.apple.WebKit.WebContent${WK_XPC_SERVICE_SUFFIX})
set(NetworkProcess_OUTPUT_NAME com.apple.WebKit.Networking${WK_XPC_SERVICE_SUFFIX})
set(GPUProcess_OUTPUT_NAME com.apple.WebKit.GPU${WK_XPC_SERVICE_SUFFIX})

# Entry point shared by all three auxiliary processes on both SDKs.
set(WebProcess_SOURCES Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp)
set(NetworkProcess_SOURCES Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp)
set(GPUProcess_SOURCES Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp)

set(WebKit_SWIFT_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/Platform/spi/Cocoa"
    "${WEBKIT_DIR}/Platform/spi/Cocoa/Modules"
    "${WEBKIT_DIR}/Platform/spi/ios"
)

set(WebKit_FORWARDING_HEADERS_FILES
    Platform/cocoa/WKCrashReporter.h

    Shared/API/c/WKDiagnosticLoggingResultType.h

    UIProcess/API/C/WKPageDiagnosticLoggingClient.h
    UIProcess/API/C/WKPageNavigationClient.h
    UIProcess/API/C/WKPageRenderingProgressEvents.h
)

list(APPEND WebKit_MESSAGES_IN_FILES
    GPUProcess/media/RemoteImageDecoderAVFProxy

    GPUProcess/media/ios/RemoteMediaSessionHelperProxy

    GPUProcess/webrtc/UserMediaCaptureManagerProxy

    ModelProcess/cocoa/ModelProcessModelPlayerProxy

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


list(APPEND WebKit_MESSAGES_IN_FILES LogStream)

file(GLOB _webkit_cocoa_serialization_files RELATIVE "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/Platform/cocoa/*.serialization.in"
    "${WEBKIT_DIR}/Shared/ApplePay/*.serialization.in"
    "${WEBKIT_DIR}/Shared/Cocoa/*.serialization.in"
    "${WEBKIT_DIR}/Shared/RemoteLayerTree/*.serialization.in"
    "${WEBKIT_DIR}/Shared/cf/*.serialization.in"
    "${WEBKIT_DIR}/Shared/mac/*.serialization.in"
    "${WEBKIT_DIR}/WebProcess/WebPage/RemoteLayerTree/*.serialization.in"
)
list(APPEND WebKit_SERIALIZATION_IN_FILES ${_webkit_cocoa_serialization_files})
unset(_webkit_cocoa_serialization_files)

list(APPEND WebKit_SERIALIZATION_IN_FILES
    Shared/AdditionalFonts.serialization.in
    Shared/AlternativeTextClient.serialization.in
    Shared/AppPrivacyReportTestingData.serialization.in
    Shared/PushMessageForTesting.serialization.in
    Shared/TextAnimationTypes.serialization.in
    Shared/ViewWindowCoordinates.serialization.in
)


list(APPEND WebCore_SERIALIZATION_IN_FILES
    PlaybackSessionModel.serialization.in
)


file(GLOB _webkit_additional_cocoa_sources RELATIVE "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/Shared/Cocoa/CoreIPC*.mm"
    "${WEBKIT_DIR}/Shared/cf/CoreIPC*.mm"
)
list(APPEND WebKit_SOURCES ${_webkit_additional_cocoa_sources})
unset(_webkit_additional_cocoa_sources)
list(APPEND WebKit_SOURCES
    NetworkProcess/cocoa/DeviceManagementSoftLink.mm
    NetworkProcess/cocoa/NetworkSoftLink.mm
    NetworkProcess/cocoa/SecuritySoftLink.mm

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

    UIProcess/EndowmentStateTracker.mm

    UIProcess/Cocoa/AboutSchemeHandlerCocoa.mm
    UIProcess/Cocoa/AuxiliaryProcessProxyCocoa.mm
    UIProcess/Cocoa/CSPExtensionUtilities.mm
    UIProcess/Cocoa/_WKWarningView.mm

    UIProcess/Downloads/DownloadProxyCocoa.mm

    UIProcess/Extensions/WebExtensionCommand.cpp
    UIProcess/Extensions/WebExtensionMenuItem.cpp

    UIProcess/Launcher/cocoa/ExtensionProcess.mm

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
    webpushd/WebPushDaemonMain.mm
    webpushd/_WKMockUserNotificationCenter.mm

    webpushd/webpushtool/WebPushToolConnection.mm
    webpushd/webpushtool/WebPushToolMain.mm
)

if (WEBKIT_SDK_IS_MACOS)
list(APPEND WebKit_SOURCES
    NetworkProcess/mac/NetworkConnectionToWebProcessMac.mm

    ${WEBKIT_DIR}/Platform/cocoa/FloatRectCG.swift
    ${WEBKIT_DIR}/Platform/cocoa/IntRectCG.swift
    ${WEBKIT_DIR}/UIProcess/WebPageProxy.swift
    ${WEBKIT_DIR}/UIProcess/mac/_WKCaptionStyleMenuControllerAVKitMac.mm
    ${WEBKIT_DIR}/UIProcess/mac/_WKCaptionStyleMenuControllerMac.mm
    ${WEBKIT_DIR}/UIProcess/mac/AppKitGestures/WKAppKitGestureController.swift
    ${WEBKIT_DIR}/UIProcess/mac/AppKitGestures/WKDOMDoubleClickGestureRecognizer.swift
    ${WEBKIT_DIR}/UIProcess/mac/AppKitGestures/WKDirectionalScrollLockTracker.swift
    ${WEBKIT_DIR}/UIProcess/mac/AppKitGestures/WKFastScrollTracker.swift
    ${WEBKIT_DIR}/UIProcess/mac/AppKitGestures/WKMouseTrackingGestureRecognizer.swift
    ${WEBKIT_DIR}/UIProcess/mac/AppKitGestures/WKPressGestureRecognizer.swift
    ${WEBKIT_DIR}/UIProcess/mac/SpatialShim.swift
    ${WEBKIT_DIR}/UIProcess/mac/WKTextSelectionController.swift
    ${WEBKIT_DIR}/UIProcess/PDF/WKAlternatePDFHUDView+Testing.swift
    ${WEBKIT_DIR}/UIProcess/PDF/WKAlternatePDFHUDView.swift
    ${WEBKIT_DIR}/UIProcess/PDF/WKDefaultPDFHUDView.swift

    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WKContextMenuElementInfoAdapter.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WKUserContentController.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WKWebView+RefreshControl.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WKWebViewConfiguration+Extras.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WKWebpagePreferences+Extras.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WKWebsiteDataStore+SwiftOverlay.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/URLSchemeHandler.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+BackForwardList.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+Configuration.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+DialogPresenting.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+FormInfo.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+FrameInfo.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+ImmersiveEnvironment.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+Navigation.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+NavigationDeciding.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+NavigationPreferences.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+SPI.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+Transferable.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/TextExtraction/WKWebView+TextExtraction.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKNavigationDelegateAdapter.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKScrollGeometryAdapter.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKUIDelegateAdapter.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKURLSchemeHandlerAdapter.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WebPageWebView.swift
)
elseif (WEBKIT_SDK_IS_IOS_FAMILY)
list(APPEND WebKit_SOURCES
    Shared/ios/WebAutocorrectionData.mm

    UIProcess/RemoteLayerTree/ios/RemoteLayerTreeViews.mm

    UIProcess/ios/WebDeviceOrientationUpdateProviderProxy.mm
    UIProcess/ios/_WKCaptionStyleMenuControllerAVKit.mm
    UIProcess/ios/_WKCaptionStyleMenuControllerIOS.mm

    ${WEBKIT_DIR}/Shared/EntryPointUtilities/Cocoa/ExtensionEventHandler.mm

    ${WEBKIT_DIR}/ModelProcess/cocoa/WKUSDStageConverter.swift
    ${WEBKIT_DIR}/Platform/spi/visionos/WKSurroundingsEffect.swift
    ${WEBKIT_DIR}/Platform/spi/visionos/WKSurroundingsEffectView.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WKWebViewConfiguration+Extras.swift
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/WKWebpagePreferences+Extras.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/URLSchemeHandler.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+BackForwardList.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+Configuration.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+DialogPresenting.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+FormInfo.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+FrameInfo.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+ImmersiveEnvironment.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+Navigation.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+NavigationDeciding.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+NavigationPreferences.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+SPI.swift
    ${WEBKIT_DIR}/UIProcess/API/Swift/WebPage+Transferable.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/CALayer+CoreRE.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageView.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageView+Analysis.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageView+Generation.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageView+Rendering.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageView+Surface.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageViewConstants.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKNavigationDelegateAdapter.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKUIDelegateAdapter.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WebPageWebView.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKScrollGeometryAdapter.swift
    ${WEBKIT_DIR}/UIProcess/Cocoa/WKURLSchemeHandlerAdapter.swift
    ${WEBKIT_DIR}/UIProcess/WKMouseDeviceObserver.swift
)
endif ()

find_library(CRYPTOTOKENKIT_LIBRARY CryptoTokenKit)
find_library(USERNOTIFICATIONS_LIBRARY UserNotifications)
find_library(WRITINGTOOLS_LIBRARY WritingTools HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(APPLEPUSHSERVICE_LIBRARY ApplePushService HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
list(APPEND WebKit_PRIVATE_LIBRARIES
    Accessibility
    ${CORESERVICES_LIBRARY}
    ${CRYPTOTOKENKIT_LIBRARY}
    ${USERNOTIFICATIONS_LIBRARY}
    ${WRITINGTOOLS_LIBRARY}
    ${APPLEPUSHSERVICE_LIBRARY}
    ${NETWORK_LIBRARY}
    ${UNIFORMTYPEIDENTIFIERS_LIBRARY}
    ${DEVICEIDENTITY_LIBRARY}
)

if (WEBKIT_SDK_IS_MACOS)
    list(APPEND WebKit_PRIVATE_LIBRARIES
        ${APPLICATIONSERVICES_LIBRARY}
        ${SECURITYINTERFACE_LIBRARY}
        $<$<BOOL:${AVFAUDIO_LIBRARY}>:${AVFAUDIO_LIBRARY}>
    )
    target_link_options(WebKit PRIVATE "LINKER:-weak_framework,PowerLog")
    if (USE_APPLE_INTERNAL_SDK)
        target_link_options(WebKit PRIVATE
            "LINKER:-weak_framework,CoreML"
            "LINKER:-weak_framework,NaturalLanguage"
        )
    endif ()
elseif (WEBKIT_SDK_IS_IOS_FAMILY)
    target_link_options(WebKit PRIVATE "LINKER:-delay_framework,CoreTelephony")
    list(APPEND WebKit_PRIVATE_LIBRARIES
        -lnetworkextension
        -lsqlite3
        ${CFNETWORK_LIBRARY}
        ${CONTACTS_LIBRARY}
        ${COREAUDIO_LIBRARY}
        ${COREFOUNDATION_LIBRARY}
        ${COREGRAPHICS_LIBRARY}
        ${CORETEXT_LIBRARY}
        ${FOUNDATION_LIBRARY}
        ${IMAGEIO_LIBRARY}
        ${IOKIT_LIBRARY}
        ${IOSURFACE_LIBRARY}
        ${METAL_LIBRARY}
        ${NETWORKEXTENSION_LIBRARY}
        ${PDFKIT_LIBRARY}
        ${UIKIT_LIBRARY}
        $<$<BOOL:${APPSTOREDAEMON_LIBRARY}>:${APPSTOREDAEMON_LIBRARY}>
        $<$<BOOL:${BACKBOARDSERVICES_LIBRARY}>:${BACKBOARDSERVICES_LIBRARY}>
        $<$<BOOL:${CORETELEPHONY_LIBRARY}>:${CORETELEPHONY_LIBRARY}>
        $<$<BOOL:${FRONTBOARDSERVICES_LIBRARY}>:${FRONTBOARDSERVICES_LIBRARY}>
        $<$<BOOL:${GAMECONTROLLERUI_LIBRARY}>:${GAMECONTROLLERUI_LIBRARY}>
        $<$<BOOL:${GRAPHICSSERVICES_LIBRARY}>:${GRAPHICSSERVICES_LIBRARY}>
        $<$<BOOL:${INSTALLCOORDINATION_LIBRARY}>:${INSTALLCOORDINATION_LIBRARY}>
        $<$<BOOL:${MOBILECORESERVICES_LIBRARY}>:${MOBILECORESERVICES_LIBRARY}>
        $<$<BOOL:${MOBILEKEYBAG_LIBRARY}>:${MOBILEKEYBAG_LIBRARY}>
        $<$<BOOL:${SPRINGBOARDSERVICES_LIBRARY}>:${SPRINGBOARDSERVICES_LIBRARY}>
        $<$<BOOL:${UIKITSERVICES_LIBRARY}>:${UIKITSERVICES_LIBRARY}>
    )
endif ()

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

target_link_options(WebKit PRIVATE -framework AuthKit)


target_link_options(WebKit PRIVATE
    "LINKER:-unexported_symbol,__ZTISt9bad_alloc"
    "LINKER:-unexported_symbol,__ZTISt9exception"
    "LINKER:-unexported_symbol,__ZTSSt9bad_alloc"
    "LINKER:-unexported_symbol,__ZTSSt9exception"
    "LINKER:-unexported_symbol,__ZdlPvS_"
    "LINKER:-unexported_symbol,__ZnwmPv"
    "LINKER:-unexported_symbol,__Znwm"
    "LINKER:-unexported_symbol,__ZTVNSt3__117bad_function_callE"
    "LINKER:-unexported_symbol,__ZTCNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE0_NS_13basic_istreamIcS2_EE"
    "LINKER:-unexported_symbol,__ZTCNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE0_NS_14basic_iostreamIcS2_EE"
    "LINKER:-unexported_symbol,__ZTCNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE16_NS_13basic_ostreamIcS2_EE"
    "LINKER:-unexported_symbol,__ZTTNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE"
    "LINKER:-unexported_symbol,__ZTVNSt3__115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEEE"
    "LINKER:-unexported_symbol,__ZTVNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE"
    "LINKER:-unexported_symbol,__ZTCNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE8_NS_13basic_ostreamIcS2_EE"
    "LINKER:-unexported_symbol,__ZTAXtlN7WebCore3CSS5RangeELdfff0000000000000ELd7ff0000000000000EEE"
    "LINKER:-unexported_symbol,_$s*3Cxx*"
)

# FIXME: Hide WebKit's Objective-C++ symbols too, as Xcode does. TestIPC needs
# to be refactored to import WebKit's IPC serialization code.
target_compile_options(WebKit PRIVATE "$<$<COMPILE_LANGUAGE:OBJC,OBJCXX>:-fvisibility=default>")

# Like Xcode, which dead-strips WebKit in every configuration. The precompiled headers' objects
# otherwise keep references to inline functions that call hidden WebCore and WTF symbols.
target_link_options(WebKit PRIVATE "$<$<CONFIG:Debug>:LINKER:-dead_strip>")

set(WebKit_OUTPUT_NAME WebKit)
if (WebKit_INSTALL_NAME_DIR)
    set_target_properties(WebKit PROPERTIES
        INSTALL_NAME_DIR "${WebKit_INSTALL_NAME_DIR}"
    )
endif ()
if (WEBKIT_SDK_IS_IOS_FAMILY)
    set_target_properties(WebKit PROPERTIES
        VERSION "${WEBKIT_MAC_VERSION}"
        SOVERSION 1
    )
endif ()

set(WebKit_GENERATED_SERIALIZERS_SUFFIX mm)


list(APPEND WebKit_DERIVED_SOURCES
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedWebKitSecureCoding.h
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedWebKitSecureCoding.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
)

# Generated JSWebExtension*.mm IDL bindings need -fobjc-arc; route to WebKitARC.
list(APPEND WebKit_ARC_SOURCES ${WebKit_DERIVED_SOURCES_DIR}/JSWebExtensionAPIUnified.mm)

# libWebKitSwift

set(_wks_dir "${WEBKIT_DIR}/WebKitSwift")

set(WebKitSwift_LIBRARY_TYPE SHARED)
WEBKIT_LIBRARY_DECLARE(WebKitSwift)

set(WebKitSwift_SOURCES
    ${_wks_dir}/WebKitSwift.swift
    ${_wks_dir}/AVKit/WKSExperienceController.swift
    ${_wks_dir}/CredentialUpdaterShim.swift
    ${_wks_dir}/GroupActivities/WKGroupSession.swift
    ${_wks_dir}/IdentityDocumentServices/ISO18013MobileDocumentRequest+Extras.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentController.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentMobileDocumentRequest.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentMobileDocumentRequest+Extras.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentRawRequest.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentRequest.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentResponse.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentRawRequestValidator.swift
    ${_wks_dir}/LinearMediaKit/LinearMediaPlayer.swift
    ${_wks_dir}/LinearMediaKit/LinearMediaTypes.swift
    ${_wks_dir}/MarketplaceKit/WKMarketplaceKit.swift
    ${_wks_dir}/Preview/WKPreviewWindowController.swift
    ${_wks_dir}/RealityKit/WKRKEntity.swift
    ${_wks_dir}/StageMode/WKStageMode.swift
    ${_wks_dir}/TextAnimation/WKTextAnimationManagerIOS.swift
    ${_wks_dir}/WritingTools/IntelligenceTextEffectChunk.swift
    ${_wks_dir}/WritingTools/IntelligenceTextEffectViewManager.swift
    ${_wks_dir}/WritingTools/PlatformIntelligenceTextEffectView.swift
    ${_wks_dir}/WritingTools/WKIntelligenceReplacementTextEffectCoordinator.swift
    ${_wks_dir}/WritingTools/WKIntelligenceSmartReplyTextEffectCoordinator.swift
    ${_wks_dir}/IdentityDocumentServices/WKIdentityDocumentPresentmentError.mm
    ${WEBKIT_DIR}/GPUProcess/graphics/Model/ModelBridge.swift
    ${WEBKIT_DIR}/GPUProcess/graphics/Model/ModelParameters.swift
    ${WEBKIT_DIR}/GPUProcess/graphics/Model/ModelRenderer.swift
    ${WEBKIT_DIR}/GPUProcess/graphics/Model/ModelUtils.swift
    ${WEBKIT_DIR}/GPUProcess/graphics/Model/USDModel.swift
    ${WEBKIT_DIR}/GPUProcess/graphics/Model/USDModel+Deformation.swift
)

set_target_properties(WebKitSwift PROPERTIES
    OUTPUT_NAME WebKitSwift
    PREFIX "lib"
    SUFFIX ".dylib"
    Swift_MODULE_NAME WebKitSwift
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    # INSTALL_NAME_DIR of Configurations/WebKitSwift.xcconfig.
    MACOSX_RPATH OFF
    BUILD_WITH_INSTALL_NAME_DIR ON
    INSTALL_NAME_DIR "/System/Library/Frameworks/WebKit.framework/${WEBKIT_FRAMEWORK_VERSION_PATH}Frameworks"
)

# WebKitAdditions ships these Swift sources with a .swift.in extension so a build
# without the internal SDK leaves them out. DerivedSources.make copies them into
# the derived sources directory for the Xcode build; do the same here, or the
# declarations they implement compile but have no implementation at runtime.
if (USE_APPLE_INTERNAL_SDK)
    foreach (_additions_swift_source
        AppKitGesturesExtras
        TestWebKitAPILibraryAdditions
        UIWindowScene+Extras
        WKSExperienceController+Transitions
        WKWebView+SystemTextExtraction)
        add_custom_command(
            OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/${_additions_swift_source}.swift
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${WebKitAdditions_HEADERS_DIR}/${_additions_swift_source}.swift.in
                ${WebKit_DERIVED_SOURCES_DIR}/${_additions_swift_source}.swift
            DEPENDS
                ${WebKitAdditions_HEADERS_DIR}/${_additions_swift_source}.swift.in
                WebKitAdditions_CopyHeaders
            COMMENT "Copying ${_additions_swift_source}.swift"
            VERBATIM
        )
    endforeach ()

    list(APPEND WebKit_SOURCES
        ${WebKit_DERIVED_SOURCES_DIR}/AppKitGesturesExtras.swift
        ${WebKit_DERIVED_SOURCES_DIR}/TestWebKitAPILibraryAdditions.swift
        ${WebKit_DERIVED_SOURCES_DIR}/UIWindowScene+Extras.swift
        ${WebKit_DERIVED_SOURCES_DIR}/WKWebView+SystemTextExtraction.swift
    )
    target_sources(WebKitSwift PRIVATE
        ${WebKit_DERIVED_SOURCES_DIR}/WKSExperienceController+Transitions.swift
    )
endif ()

target_include_directories(WebKitSwift PRIVATE
    ${_wks_dir}
    ${_wks_dir}/AVKit
    ${_wks_dir}/GroupActivities
    ${_wks_dir}/IdentityDocumentServices
    ${_wks_dir}/LinearMediaKit
    ${_wks_dir}/MarketplaceKit
    ${_wks_dir}/Preview
    ${_wks_dir}/RealityKit
    ${_wks_dir}/StageMode
    ${_wks_dir}/TextAnimation
    ${_wks_dir}/WritingTools
    ${WEBKIT_DIR}
    ${WEBKIT_DIR}/Shared/Model
    ${WEBKIT_DIR}/GPUProcess/graphics/Model
    ${CMAKE_BINARY_DIR}
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
    # The Objective-C++ sources include Source/WebKit/config.h, which includes
    # <pal/ExportMacros.h>.
    ${PAL_FRAMEWORK_HEADERS_DIR}
)

webkit_target_add_swift_options(WebKitSwift
    -parse-as-library
    "-library-level other"
    "@${CMAKE_CURRENT_BINARY_DIR}/WebKit.platform-swift-args.resp"
    -I${WEBKIT_DIR}/Platform/spi/Cocoa
    -I${WEBKIT_DIR}/Platform/spi/Cocoa/Modules
    -I${WEBKIT_DIR}/Platform/spi/ios
    "-Xcc -DHAVE_CONFIG_H=1"
    "-Xcc -I${CMAKE_BINARY_DIR}"
    "-Xcc -I${WTF_FRAMEWORK_HEADERS_DIR}"
    "-Xcc -I${bmalloc_FRAMEWORK_HEADERS_DIR}"
)

# WebKit's custom Swift @available macros, as a response file.
set(_swift_tba_resp "${CMAKE_CURRENT_BINARY_DIR}/swift-tba-availability-macros.resp")
if (WEBKIT_SDK_IS_MACOS)
    set(_swift_tba_env
        IPHONEOS_DEPLOYMENT_TARGET=9999
        MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        WK_PLATFORM_NAME=macosx
    )
else ()
    if (WEBKIT_SDK_IS_SIMULATOR)
        set(_swift_tba_platform "iphonesimulator")
    else ()
        set(_swift_tba_platform "iphoneos")
    endif ()
    set(_swift_tba_env
        WK_PLATFORM_NAME=${_swift_tba_platform}
        IPHONEOS_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        # LLVM_TARGET_TRIPLE_OS_VERSION is only used by iOS code (to
        # disambiguate between iOS and Catalyst).
        LLVM_TARGET_TRIPLE_OS_VERSION=ios${CMAKE_OSX_DEPLOYMENT_TARGET}
        MACOSX_DEPLOYMENT_TARGET=9999
    )
    unset(_swift_tba_platform)
endif ()
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env
        ${_swift_tba_env}
        XROS_DEPLOYMENT_TARGET=9999
        BUILT_PRODUCTS_DIR=${CMAKE_BINARY_DIR}
        SDKROOT=${CMAKE_OSX_SYSROOT}
        SCRIPT_OUTPUT_FILE_0=${_swift_tba_resp}
        WK_LIBRARY_HEADERS_FOLDER_PATH=/usr/local/include
        WK_WEBKITADDITIONS_HEADERS_FOLDER_PATH=${CMAKE_OSX_SYSROOT}/usr/local/include/WebKitAdditions
        bash ${WEBKIT_DIR}/Scripts/generate-swift-availability-macros
    RESULT_VARIABLE _swift_tba_resp_result
    OUTPUT_VARIABLE _swift_tba_resp_stdout
    ERROR_VARIABLE _swift_tba_resp_stderr)
if (NOT _swift_tba_resp_result EQUAL 0 OR NOT EXISTS "${_swift_tba_resp}")
    message(FATAL_ERROR "generate-swift-availability-macros failed (exit ${_swift_tba_resp_result}).\nstdout:\n${_swift_tba_resp_stdout}\nstderr:\n${_swift_tba_resp_stderr}")
endif ()
unset(_swift_tba_env)
unset(_swift_tba_resp_stdout)
unset(_swift_tba_resp_stderr)
unset(_swift_tba_resp_result)

if (WEBKIT_SDK_IS_IOS_FAMILY)
    webkit_target_add_swift_options(WebKitSwift
        "@${_swift_tba_resp}"
    )
endif ()

target_compile_options(WebKitSwift PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-std=c++2b>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-DHAVE_CONFIG_H=1>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-DBUILDING_WITH_CMAKE=1>"
)

target_compile_options(WebKitSwift PRIVATE
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CMAKE_BINARY_DIR}>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-I${WebKit_FRAMEWORK_HEADERS_DIR}>"
    "$<$<COMPILE_LANGUAGE:Swift>:-F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}>"
    ${WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG}
)

find_library(WRITINGTOOLSUI_LIBRARY WritingToolsUI HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
target_link_libraries(WebKitSwift PRIVATE
    WebKit
    $<$<BOOL:${WRITINGTOOLSUI_LIBRARY}>:${WRITINGTOOLSUI_LIBRARY}>
)
add_dependencies(WebKitSwift WebKit)

# WebKit.framework's own signature does not cover this file: the
# Frameworks/libWebKitSwift.dylib inside the bundle is a symlink to it.
WEBKIT_LIBRARY(WebKitSwift)

unset(_wks_dir)

# Defines the auxiliary process targets, along with the framework content that
# only exists to support them.
function(WEBKIT_DEFINE_AUXILIARY_PROCESSES)
    set(_wka_entitlements_dir "")
    # FIXME: Use the WebKitAdditions path provided by the interface library.
    if (WEBKIT_ADDITIONS_INCLUDE_PATH AND EXISTS "${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/Entitlements")
        set(_wka_entitlements_dir "${WEBKIT_ADDITIONS_INCLUDE_PATH}/WebKitAdditions/Entitlements")
    endif ()

    function(WEBKIT_RESOLVE_ENTITLEMENTS _result _filename)
        if (_wka_entitlements_dir AND EXISTS "${_wka_entitlements_dir}/${_filename}")
            set(${_result} "${_wka_entitlements_dir}/${_filename}" PARENT_SCOPE)
        else ()
            set(${_result} "${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/${_filename}" PARENT_SCOPE)
        endif ()
    endfunction()

    set(_get_task_allow "${CMAKE_CURRENT_BINARY_DIR}/XPCService-get-task-allow.entitlements")
    WEBKIT_WRITE_SIMULATOR_SIGNING_ENTITLEMENTS(${_get_task_allow})

    if (USE_EXTENSIONKIT)
        WEBKIT_DEFINE_PROCESS_EXTENSIONS()
    else ()
        WEBKIT_DEFINE_XPC_SERVICES()
    endif ()

    if (WEBKIT_SDK_IS_MACOS)
        WEBKIT_DEFINE_MACOS_RESOURCES()
    else ()
        WEBKIT_DEFINE_IOS_RESOURCES()
    endif ()
    WEBKIT_DEFINE_DAEMONS()
endfunction()

function(WEBKIT_DEFINE_XPC_SERVICES)
    if (WEBKIT_SDK_IS_MACOS)
        set(_info_plist_variant OSX)
        set(WebKit_XPC_SERVICE_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/XPCServices)
        # Relative symlink (matches Xcode layout; absolute breaks if build dir is moved).
        file(MAKE_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework")
        file(CREATE_LINK "Versions/Current/XPCServices"
                         "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices" SYMBOLIC)
        # _WebKit runloop type is obsolete (macOS < 11.0); modern libxpc requires NSRunLoop
        # or the XPC event handler never fires and WebContent hangs.
        set(RUNLOOP_TYPE NSRunLoop)
    else ()
        set(_info_plist_variant iOS)
        # Built beside the framework and symlinked into it afterwards, so each
        # service is signed before the framework seals over the symlinks.
        set(WebKit_XPC_SERVICE_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
    endif ()
    # Also read by WEBKIT_DEFINE_MACOS_RESOURCES.
    set(WebKit_XPC_SERVICE_DIR ${WebKit_XPC_SERVICE_DIR} PARENT_SCOPE)

    set(_default_sim_entitlements "${WEBKIT_DIR}/Resources/ios/XPCService-embedded-simulator.entitlements")

    function(WEBKIT_XPC_SERVICE _target)
        cmake_parse_arguments(_svc "NO_RESTRICTED_ENTITLEMENTS"
            "BUNDLE_IDENTIFIER;ENTRY_POINT;EXECUTABLE_NAME" "" ${ARGN})
        set(_bundle_dir ${WebKit_XPC_SERVICE_DIR}/${_svc_BUNDLE_IDENTIFIER}.xpc)
        if (WEBKIT_SDK_IS_MACOS)
            set(_contents_dir ${_bundle_dir}/Contents)
            set(_exe_dir ${_contents_dir}/MacOS)
            file(MAKE_DIRECTORY ${_contents_dir}/Resources)
        else ()
            set(_contents_dir ${_bundle_dir})
            set(_exe_dir ${_bundle_dir})
        endif ()
        file(MAKE_DIRECTORY ${_exe_dir})

        set(BUNDLE_VERSION ${MACOSX_FRAMEWORK_BUNDLE_VERSION})
        set(SHORT_VERSION_STRING ${MACOSX_FRAMEWORK_SHORT_VERSION_STRING})
        set(PRODUCT_BUNDLE_IDENTIFIER ${_svc_BUNDLE_IDENTIFIER})
        set(EXECUTABLE_NAME ${_svc_EXECUTABLE_NAME})
        set(PRODUCT_NAME ${_svc_BUNDLE_IDENTIFIER})
        # Processed outside the bundle and copied in only when it changes: the
        # service has to relink, and so re-sign, whenever its Info.plist does.
        set(_info_plist ${CMAKE_CURRENT_BINARY_DIR}/${_svc_BUNDLE_IDENTIFIER}-Info.plist)
        configure_file(${_svc_ENTRY_POINT}/Info-${_info_plist_variant}.plist ${_info_plist})
        if (WEBKIT_SDK_IS_MACOS)
            execute_process(COMMAND plutil -insert CFBundleSupportedPlatforms -json "[\"${WEBKIT_PLATFORM_NAME}\"]" ${_info_plist})
            execute_process(COMMAND plutil -insert DTPlatformName -string "${WEBKIT_SDK_NAME}" ${_info_plist})
            execute_process(COMMAND plutil -insert LSMinimumSystemVersion -string "${CMAKE_OSX_DEPLOYMENT_TARGET}" ${_info_plist})
        else ()
            # No XPC service target sets TARGETED_DEVICE_FAMILY.
            WEBKIT_GET_DEVICE_FAMILY(_device_family)
            WEBKIT_ADD_EMBEDDED_BUNDLE_PLIST_KEYS(${_info_plist} ${_device_family})
        endif ()
        if (USE_RESTRICTED_ENTITLEMENTS AND NOT _svc_NO_RESTRICTED_ENTITLEMENTS)
            # Matches Scripts/update-info-plist-for-runningboard.sh.
            if (USE_APPLE_INTERNAL_SDK AND WEBKIT_SDK_IS_MACOS)
                execute_process(COMMAND plutil -insert LSDoNotSetTaskPolicyAutomatically -bool YES ${_info_plist})
                execute_process(COMMAND plutil -insert XPCService._AdditionalProperties -json "{\"RunningBoard\":{\"Managed\":true,\"Reported\":true}}" ${_info_plist})
            endif ()
        else ()
            # As in Xcode, services without restricted entitlements are signed ad hoc.
            set_target_properties(${_target} PROPERTIES CODE_SIGN_IDENTITY "-")
        endif ()
        file(COPY_FILE ${_info_plist} ${_contents_dir}/Info.plist ONLY_IF_DIFFERENT)
        set_property(TARGET ${_target} APPEND PROPERTY LINK_DEPENDS ${_contents_dir}/Info.plist)

        if (NOT WEBKIT_SDK_IS_MACOS)
            target_link_options(${_target} PRIVATE
                "LINKER:-rpath,@executable_path/.."
                "LINKER:-dyld_env,DYLD_FRAMEWORK_PATH=@executable_path/.."
                "LINKER:-dyld_env,DYLD_LIBRARY_PATH=@executable_path/.."
            )
            target_link_libraries(${_target} PRIVATE
                "-framework Foundation"
                "-framework CoreFoundation"
            )

            if (WEBKIT_SDK_IS_SIMULATOR)
                WEBKIT_EMBED_ENTITLEMENTS(${_target} ${_default_sim_entitlements})
                # Overrides the generated entitlements.
                set_property(TARGET ${_target} PROPERTY
                    CODE_SIGN_ENTITLEMENTS "${_get_task_allow}")
            endif ()
            set_property(TARGET ${_target} PROPERTY CODE_SIGN_FLAGS
                --timestamp=none --generate-entitlement-der)
        endif ()

        if (WEBKIT_SDK_IS_MACOS)
            # This WebKit is loaded from the build directory rather than from its
            # install name, and launchd starts a service with no environment pointing
            # at it, so bake in the way back to the frameworks beside the framework
            # the service lives in. Xcode does the same through
            # WK_PATH_FROM_SERVICE_EXECUTABLE_TO_FRAMEWORKS.
            file(RELATIVE_PATH _path_to_frameworks
                "${_exe_dir}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
            target_link_options(${_target} PRIVATE
                "LINKER:-dyld_env,DYLD_FRAMEWORK_PATH=@executable_path/${_path_to_frameworks}"
                "LINKER:-dyld_env,DYLD_LIBRARY_PATH=@executable_path/${_path_to_frameworks}"
            )
        endif ()

        # Link into the bundle, and sign the wrapper rather than the Mach-O.
        # MACOSX_BUNDLE defaults on for the embedded SDKs, which would nest an
        # .app inside the .xpc; the wrapper here is the hand-built one.
        set_target_properties(${_target} PROPERTIES
            MACOSX_BUNDLE FALSE
            RUNTIME_OUTPUT_DIRECTORY "${_exe_dir}"
            CODE_SIGN_BUNDLE "${_bundle_dir}")

        # XPC services are part of WebKit.framework, so they must finish
        # signing before the framework bundle signs.
        add_dependencies(WebKit_CodeSign ${_target}_CodeSign)
    endfunction()

    WEBKIT_XPC_SERVICE(WebProcess
        BUNDLE_IDENTIFIER com.apple.WebKit.WebContent
        ENTRY_POINT ${WEBKIT_DIR}/WebProcess/EntryPoint/Cocoa/XPCService/WebContentService
        EXECUTABLE_NAME ${WebProcess_OUTPUT_NAME})

    WEBKIT_XPC_SERVICE(NetworkProcess
        BUNDLE_IDENTIFIER com.apple.WebKit.Networking
        ENTRY_POINT ${WEBKIT_DIR}/NetworkProcess/EntryPoint/Cocoa/XPCService/NetworkService
        EXECUTABLE_NAME ${NetworkProcess_OUTPUT_NAME})

    if (ENABLE_GPU_PROCESS)
        WEBKIT_XPC_SERVICE(GPUProcess
            BUNDLE_IDENTIFIER com.apple.WebKit.GPU
            ENTRY_POINT ${WEBKIT_DIR}/GPUProcess/EntryPoint/Cocoa/XPCService/GPUService
            EXECUTABLE_NAME ${GPUProcess_OUTPUT_NAME})
    endif ()

    # Without these XPC bundles, process swaps fail with "Invalid connection identifier".
    function(WEBKIT_WEBCONTENT_VARIANT _variant)
        set(_target WebProcess${_variant})
        set(_exec_name com.apple.WebKit.WebContent.${_variant}${WK_XPC_SERVICE_SUFFIX})
        WEBKIT_EXECUTABLE_DECLARE(${_target})
        set(${_target}_SOURCES ${WebProcess_SOURCES})
        set(${_target}_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}
            $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
        set(${_target}_LIBRARIES WebKit)
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME ${_exec_name})
        # Generate first: WEBKIT_XPC_SERVICE overrides CODE_SIGN_ENTITLEMENTS
        # under the simulator, and whichever sets the property last wins.
        WEBKIT_GENERATE_ENTITLEMENTS(${_target}
            USING Scripts/process-entitlements.sh
            DEPENDS ${WebKit_ENTITLEMENTS_DEPENDS}
            BUNDLE_IDENTIFIER com.apple.WebKit.WebContent.${_variant}
            VARIANT ${_variant}
            ${ARGN})
        WEBKIT_XPC_SERVICE(${_target}
            BUNDLE_IDENTIFIER com.apple.WebKit.WebContent.${_variant}
            ENTRY_POINT ${WEBKIT_DIR}/WebProcess/EntryPoint/Cocoa/XPCService/WebContentService
            EXECUTABLE_NAME ${_exec_name}
            ${ARGN})
        WEBKIT_EXECUTABLE(${_target})
        WEBKIT_REUSE_PREFIX_HEADER(${_target} WebKit WebKitPrefix.h PREFIX_LANGUAGES CXX)
        target_compile_options(${_target} PRIVATE -Wno-unused-parameter)
    endfunction()
    WEBKIT_WEBCONTENT_VARIANT(EnhancedSecurity)
    WEBKIT_WEBCONTENT_VARIANT(CaptivePortal)
    if (WEBKIT_SDK_IS_MACOS)
        # Local builds use this bundle (see logic in ProcessLaunchrCocoa.mm).
        WEBKIT_WEBCONTENT_VARIANT(Development NO_RESTRICTED_ENTITLEMENTS)
    endif ()
endfunction()

function(WEBKIT_DEFINE_DAEMONS)
    # ENTITLEMENTS signs with that file instead of process-entitlements.sh output.
    function(WEBKIT_DAEMON _target _source)
        cmake_parse_arguments(_arg "" "ENTITLEMENTS" "" ${ARGN})
        WEBKIT_EXECUTABLE_DECLARE(${_target})
        set(${_target}_SOURCES ${_source})
        set(${_target}_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR}
            $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
        set(${_target}_LIBRARIES WebKit)

        set_target_properties(${_target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
        target_compile_options(${_target} PRIVATE
            -F${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

        if (WEBKIT_SDK_IS_SIMULATOR)
            set_property(TARGET ${_target} PROPERTY
                CODE_SIGN_ENTITLEMENTS "${_get_task_allow}")
        elseif (_arg_ENTITLEMENTS)
            set_property(TARGET ${_target} PROPERTY
                CODE_SIGN_ENTITLEMENTS "${_arg_ENTITLEMENTS}")
        else ()
            WEBKIT_GENERATE_ENTITLEMENTS(${_target}
                USING Scripts/process-entitlements.sh
                DEPENDS ${WebKit_ENTITLEMENTS_DEPENDS})
        endif ()

        WEBKIT_EXECUTABLE(${_target})
    endfunction()

    WEBKIT_DAEMON(webpushd
        ${WEBKIT_DIR}/webpushd/webpushd.cpp)
    if (WEBKIT_SDK_IS_MACOS AND DEVELOPER_MODE)
        target_link_options(webpushd PRIVATE
            "LINKER:-rpath,@executable_path/."
            "LINKER:-dyld_env,DYLD_FRAMEWORK_PATH=@executable_path/."
            "LINKER:-dyld_env,DYLD_LIBRARY_PATH=@executable_path/."
        )
    endif ()

    # adattributiond doesn't have any custom entitlements on macOS.
    if (WEBKIT_SDK_IS_MACOS AND DEVELOPER_MODE)
        set(_adattributiond_entitlements ENTITLEMENTS "${_get_task_allow}")
    endif ()
    WEBKIT_DAEMON(adattributiond
        ${WEBKIT_DIR}/Shared/EntryPointUtilities/Cocoa/Daemon/adattributiond.cpp
        ${_adattributiond_entitlements})

    # Xcode injects get-task-allow into webpushtool's CODE_SIGN_ENTITLEMENTS.
    set(_webpushtool_entitlements ${CMAKE_CURRENT_BINARY_DIR}/webpushtool.entitlements)
    add_custom_command(OUTPUT ${_webpushtool_entitlements}
        COMMAND ${CMAKE_COMMAND} -E copy ${WEBKIT_DIR}/Resources/webpushtool.entitlements ${_webpushtool_entitlements}
        COMMAND /usr/libexec/PlistBuddy -c "Add :com.apple.security.get-task-allow bool YES" ${_webpushtool_entitlements}
        DEPENDS ${WEBKIT_DIR}/Resources/webpushtool.entitlements
        VERBATIM)
    add_custom_target(webpushtoolEntitlements DEPENDS ${_webpushtool_entitlements})
    WEBKIT_DAEMON(webpushtool
        ${WEBKIT_DIR}/webpushd/webpushtool/webpushtool.cpp
        ENTITLEMENTS ${_webpushtool_entitlements})
    add_dependencies(webpushtool webpushtoolEntitlements)
endfunction()

# Platform-specific configuration, selected by the target SDK.
# FIXME: Continue merging forked iOS/Mac code here.
if (WEBKIT_SDK_IS_IOS_FAMILY)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/WebKitLegacy.h
    "#if defined(__has_include) && __has_include(<WebKitLegacy/WebKit.h>)\n"
    "#import <WebKitLegacy/WebKit.h>\n"
    "#endif\n"
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/WebKitLegacy.h PROPERTIES
    MACOSX_PACKAGE_LOCATION Headers
    GENERATED TRUE
)

set(WebKit_USE_PREFIX_HEADER ON)

# WebKit's Swift compile loads `framework module WebKit_Private` via -fmodule-map-file
# and resolves header paths relative to the modulemap's framework root (a parent of
# Modules/). The build-tree layout is `WebKit/PrivateHeaders/WebKit/X.h` to support
# `<WebKit/X.h>` includes, which doesn't match the framework's expected
# `WebKit.framework/PrivateHeaders/X.h` (flat). Stage a real WebKit.framework
# directory before the compile so the umbrella header lookup succeeds.
# WebKit_CopyHeaders / WebKit_CopyPrivateHeaders are defined later in
# CMakeLists.txt; defer the add_dependencies until those targets exist.
add_custom_target(WebKit_StageFrameworkHeaders
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/Source/cmake/SymlinkHeaders.cmake
        ${WebKit_FRAMEWORK_HEADERS_DIR}/WebKit
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Headers
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/Source/cmake/SymlinkHeaders.cmake
        ${WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebKit
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/PrivateHeaders
    COMMENT "Staging WebKit.framework Headers/, PrivateHeaders/"
)
add_dependencies(WebKit WebKit_StageFrameworkHeaders)
cmake_language(DEFER CALL add_dependencies WebKit_StageFrameworkHeaders WebKit_CopyHeaders WebKit_CopyPrivateHeaders)

set(_migrated_excluded_for_ios
    WebDynamicScrollBarsView.h
    WebIconDatabase.h
    WebJavaScriptTextInputPanel.h
    WebNSEventExtras.h
    WebNSPasteboardExtras.h
    WebNSWindowExtras.h
    WebPanelAuthenticationHandler.h
    WebStringTruncator.h
)

# Mirror Source/WebKitLegacy/scripts/xcfilelist-copy.py: pair each line of
# MigratedHeaders-input.xcfilelist with the same-line entry of
# MigratedHeaders-output.xcfilelist, resolve the destination's Xcode build
# variables, and apply the allowed `WebKit.h` -> `WebKitLegacy.h` rename.
# The output xcfilelist drives Headers/ vs PrivateHeaders/, so blindly
# putting every migrated header into PrivateHeaders/ (the prior cmake
# behavior) over-staged the full Mac WebKitLegacy umbrella into
# WebKit.framework/PrivateHeaders/, which the WebKit_Private modulemap
# auto-discovered as a `WebKit_Private.WebKitLegacy` submodule with its
# own `WebFrame` -- conflicting with the real WebKitLegacy module.
set(_migrate_pairs_file "${CMAKE_BINARY_DIR}/WebKit_MigrateHeaders.pairs")
set(_migrate_pairs_content "")
file(STRINGS "${WEBKIT_DIR}/MigratedHeaders-input.xcfilelist" _migrate_in_lines)
file(STRINGS "${WEBKIT_DIR}/MigratedHeaders-output.xcfilelist" _migrate_out_lines)
# Drop comment / blank lines from both, preserving order, so the indices line up.
set(_migrate_in "")
foreach (_line IN LISTS _migrate_in_lines)
    if (_line AND NOT _line MATCHES "^#")
        list(APPEND _migrate_in "${_line}")
    endif ()
endforeach ()
set(_migrate_out "")
foreach (_line IN LISTS _migrate_out_lines)
    if (_line AND NOT _line MATCHES "^#")
        list(APPEND _migrate_out "${_line}")
    endif ()
endforeach ()
list(LENGTH _migrate_in _migrate_in_count)
list(LENGTH _migrate_out _migrate_out_count)
if (NOT _migrate_in_count EQUAL _migrate_out_count)
    message(FATAL_ERROR "MigratedHeaders-input.xcfilelist (${_migrate_in_count} entries) "
            "and MigratedHeaders-output.xcfilelist (${_migrate_out_count} entries) "
            "are out of sync; xcfilelist-copy.py pairs them by line.")
endif ()
math(EXPR _migrate_last "${_migrate_in_count} - 1")
foreach (_i RANGE ${_migrate_last})
    list(GET _migrate_in ${_i} _src)
    list(GET _migrate_out ${_i} _out)
    string(REPLACE "$(WEBCORE_PRIVATE_HEADERS_DIR)" "${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCore" _src "${_src}")
    string(REPLACE "$(WEBKITLEGACY_PRIVATE_HEADERS_DIR)" "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}/WebKitLegacy" _src "${_src}")
    get_filename_component(_in_basename "${_src}" NAME)
    get_filename_component(_out_basename "${_out}" NAME)
    if (_in_basename IN_LIST _migrated_excluded_for_ios)
        continue ()
    endif ()
    # Resolve the output xcfilelist's $(...) variables to a real path under
    # WebKit.framework/.
    if (_out MATCHES "\\$\\(PUBLIC_HEADERS_FOLDER_PATH\\)")
        set(_dst "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Headers/${_out_basename}")
    elseif (_out MATCHES "\\$\\(PRIVATE_HEADERS_FOLDER_PATH\\)")
        set(_dst "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/PrivateHeaders/${_out_basename}")
    elseif (_out MATCHES "\\$\\(WK_MAC_PUBLIC_IOS_PRIVATE_HEADERS_DIR\\)")
        # iOS resolves WK_MAC_PUBLIC_IOS_PRIVATE_HEADERS_DIR to the private
        # headers folder (Mac resolves it to public). See WK_MAC_PUBLIC_IOS_*
        # in Source/WebKit/Configurations/WebKit.xcconfig.
        set(_dst "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/PrivateHeaders/${_out_basename}")
    else ()
        message(FATAL_ERROR "Unrecognized destination in MigratedHeaders-output.xcfilelist: ${_out}")
    endif ()
    string(APPEND _migrate_pairs_content "${_src}|${_dst}\n")
endforeach ()
file(WRITE "${_migrate_pairs_file}" "${_migrate_pairs_content}")

add_custom_target(WebKit_MigrateHeaders
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/Source/cmake/MigrateHeaders.cmake "${_migrate_pairs_file}"
    COMMENT "Migrating WebCore/WebKitLegacy headers into WebKit.framework/{Headers,PrivateHeaders}/"
)
add_dependencies(WebKit_MigrateHeaders WebKit_StageFrameworkHeaders WebCore_CopyPrivateHeaders WebKitLegacy_CopyHeaders)
add_dependencies(WebKit WebKit_MigrateHeaders)
unset(_migrate_pairs_file)
unset(_migrate_pairs_content)
unset(_migrate_in_lines)
unset(_migrate_out_lines)
unset(_migrate_in)
unset(_migrate_out)
unset(_migrate_in_count)
unset(_migrate_out_count)
unset(_migrate_last)
unset(_migrated_excluded_for_ios)

# Stripped-down WebKit_Internal modulemap for Swift, mirroring
# PlatformMac.cmake:72-102. The full upstream modulemap at
# Source/WebKit/Modules/Internal/module.modulemap exposes 66 submodules
# whose headers transitively #import (textually, via "X.h") types declared
# in WebKit_Private (e.g. _WKTapHandlingResult from WKWebViewIOS.h,
# WKWebExtensionWindowType from WebExtensionWindow.h, WKWebExtensionController
# from WebExtensionController.h). When clang compiles WebKit_Internal as a
# PCM under -explicit-module-build, its strict cross-module-import-visibility
# check (enabled by -enable-upcoming-feature MemberImportVisibility) fires
# on those references because WebKit_Private isn't loaded into the PCM
# compile context. Xcode dodges this by passing -fmodule-file=WebKit_Private
# to the WebKit_Internal PCM compile (verified by inspecting Xcode's built
# WebKit_Internal-ATDO9JP6XFZSB8E4OQUTTMYJJ.pcm), but cmake's libSwiftScan
# dep graph doesn't add WebKit_Private as a transitive dep of WebKit_Internal
# because the cross-module #imports are textual, not modular. Stripping the
# WebKit_Internal modulemap to only the submodules iOS Swift sources need
# avoids compiling the offending headers altogether. Bug 312083.
set(WebKit_CMAKE_MODULEMAP_DIR "${CMAKE_BINARY_DIR}/WebKit/SwiftModules/Internal")
file(MAKE_DIRECTORY "${WebKit_CMAKE_MODULEMAP_DIR}")
file(WRITE "${WebKit_CMAKE_MODULEMAP_DIR}/module.modulemap"
"module WebKit_Internal [system] {
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

    module WKWebView {
        requires objc
        header \"${WebKit_HEADERS_DIR}/WKWebView.h\"
        export *
    }

    module WKUSDStageConverter {
        requires objc
        header \"${WEBKIT_DIR}/ModelProcess/cocoa/WKUSDStageConverter.h\"
        export *
    }

    module WKSurroundingsEffect {
        requires objc
        header \"${WEBKIT_DIR}/Platform/spi/visionos/WKSurroundingsEffect.h\"
        export *
    }

    module WKStageModeOrbitSimulator {
        requires objc
        header \"${WEBKIT_DIR}/Shared/Model/WKStageModeOrbitSimulator.h\"
        export *
    }

    module WKDeferringGestureRecognizer {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/Cocoa/WKDeferringGestureRecognizer.h\"
        export *
    }

    module WKSeparatedImageView {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/Cocoa/Separated/WKSeparatedImageView.h\"
        export *
    }

    module WKMouseDeviceObserver {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/ios/WKMouseDeviceObserver.h\"
        export *
    }

    module WKTextEffectManager {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/Cocoa/WKTextEffectManager.h\"
        export *
    }

    module WKBackForwardListItemInternal {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/API/Cocoa/WKBackForwardListItemInternal.h\"
        export *
    }

    module WKScrollGeometry {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/API/Cocoa/WKScrollGeometry.h\"
        export *
    }

    module WKWebViewInternal {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/API/Cocoa/WKWebViewInternal.h\"
        export *
    }

    module WKWebViewIOS {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/API/ios/WKWebViewIOS.h\"
        export *
    }

    module WKWebViewConfigurationInternal {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/API/Cocoa/WKWebViewConfigurationInternal.h\"
        export *
    }

    module UIWindowScene_Extras {
        requires objc
        header \"${WEBKIT_DIR}/UIProcess/Cocoa/UIWindowScene+Extras.h\"
        export *
    }

    module JavaScriptEvaluationResult {
        requires cplusplus20
        header \"${WEBKIT_DIR}/Shared/JavaScriptEvaluationResult.h\"
        export *
    }

    module JavaScriptEvaluationResultCxxInteropSupport {
        requires cplusplus23
        header \"${WEBKIT_DIR}/Shared/JavaScriptEvaluationResultCxxInteropSupport.h\"
        export *
    }

    module RunJavaScriptParameters {
        requires cplusplus23
        header \"${WEBKIT_DIR}/Shared/RunJavaScriptParameters.h\"
        export *
    }

    module RunJavaScriptResult {
        requires cplusplus20
        header \"${WEBKIT_DIR}/Shared/RunJavaScriptResult.h\"
        export *
    }

}
")
set(WebKit_SWIFT_INTEROP_MODULE_PATH "${WebKit_CMAKE_MODULEMAP_DIR}")


target_compile_options(WebKit PRIVATE ${WEBKIT_PRIVATE_FRAMEWORKS_COMPILE_FLAG})
target_compile_options(WebKit PRIVATE "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CMAKE_BINARY_DIR}>")

set_target_properties(WebKit PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    OBJC_VISIBILITY_PRESET hidden
    OBJCXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

webkit_target_add_swift_options(WebKit
    # Match Xcode iOS WebKit Swift compile flags from
    # WebKit.framework/Modules/WebKit.swiftmodule/*.swiftinterface.
    -no-verify-emitted-module-interface
    # -Xcc -D/-f flags shared with PAL/WebGPU come from
    # _WEBKIT_COMPUTE_SWIFT_SHARED_CLANG_FLAGS in WebKitMacros.cmake (which also
    # omits WK_SUPPORTS_SWIFT_OBJCXX_INTEROP on iOS — see bug 312083). Only
    # -I/-isystem/-fmodule-map-file (not in the module-cache hash)
    # remain per-target here.
    "-Xcc -DHAVE_CONFIG_H=1"
    "-Xcc -I${CMAKE_BINARY_DIR}"
    "-Xfrontend -disable-cross-import-overlays"
    # Auto-import the WebKit framework's clang module (matched by -module-name
    # WebKit) so iOS Swift sources see public WebKit Obj-C API (WKWebView,
    # WKError, WKFrameInfo, WKURLSchemeHandler, ...) without needing an
    # explicit `import WebKit` line. The previous full 66-submodule
    # WebKit_Internal modulemap re-exported these types transitively via
    # textual #imports; the stripped modulemap above doesn't, so the
    # underlying-module-import is now required to keep WebPage.swift and
    # friends compiling. Bug 312083.
    -import-underlying-module
    # Use WebKit_Private modulemap as an external client; do not pin
    # -fmodule-name=WebKit (that contradicts the loaded modulemap and feeds
    # clang module-loader cycles in the Swift dep scan).
    "-Xcc -fmodule-map-file=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Modules/module.private.modulemap"
    "@${_swift_tba_resp}"
    -I${WEBKIT_DIR}/Platform/spi/Cocoa
    -I${WEBKIT_DIR}/Platform/spi/Cocoa/Modules
    -I${WEBKIT_DIR}/Platform/spi/ios
    "-Xcc -I${WTF_FRAMEWORK_HEADERS_DIR}"
    "-Xcc -I${bmalloc_FRAMEWORK_HEADERS_DIR}"
    "-Xcc -I${PAL_FRAMEWORK_HEADERS_DIR}"
    "-Xcc -isystem${CMAKE_OSX_SYSROOT}/usr/local/include"
    "-Xcc -fmodule-map-file=${CMAKE_OSX_SYSROOT}/usr/local/include/unicode_private.modulemap"
)

webkit_target_add_swift_options(WebKit
    -enable-library-evolution
    "-emit-module-interface-path ${CMAKE_BINARY_DIR}/Source/WebKit/WebKit.swiftinterface"
    "-emit-private-module-interface-path ${CMAKE_BINARY_DIR}/Source/WebKit/WebKit.private.swiftinterface"
)


if (WEBKIT_ADDITIONS_SWIFT_SOURCES)
    # WebViewRepresentable+Extras.swift belongs to the _WebKit_SwiftUI overlay
    # module (per the pbxproj); compiling it in main WebKit clashes with
    # WKMaterialHostingSupport.swift's @_weakLinked SwiftUI import.
    set(WebKit_SwiftUI_ADDITIONS_SOURCES "")
    foreach (_f IN LISTS WEBKIT_ADDITIONS_SWIFT_SOURCES)
        cmake_path(GET _f FILENAME _fn)
        if (_fn STREQUAL "WebViewRepresentable+Extras.swift")
            list(APPEND WebKit_SwiftUI_ADDITIONS_SOURCES "${_f}")
        else ()
            list(APPEND WebKit_SOURCES "${_f}")
        endif ()
    endforeach ()
    unset(_fn)
endif ()

set(_log_defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR} ENABLE_STREAMING_IPC_IN_LOG_FORWARDING")
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
        "${_log_defines}"
    WORKING_DIRECTORY ${WebKit_DERIVED_SOURCES_DIR}
    VERBATIM
)

file(GLOB _webkit_ios_serialization_files RELATIVE "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/Shared/ios/*.serialization.in"
)
list(APPEND WebKit_SERIALIZATION_IN_FILES ${_webkit_ios_serialization_files})
unset(_webkit_ios_serialization_files)

list(APPEND WebKit_SERIALIZATION_IN_FILES
    Shared/KeyEventInterpretationContext.serialization.in
    Shared/UserInterfaceIdiom.serialization.in
)

list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS ${WebKit_PUBLIC_FRAMEWORK_HEADERS})
set(WebKit_PUBLIC_FRAMEWORK_HEADERS "")

list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/API/Cocoa/WKDataDetectorTypes.h
    Shared/API/Cocoa/WKFoundation.h
    Shared/API/Cocoa/WebKit.apinotes
    Shared/API/Cocoa/WebKit.h

    UIProcess/API/Cocoa/NSAttributedString.h
    UIProcess/API/Cocoa/WKBackForwardList.h
    UIProcess/API/Cocoa/WKBackForwardListItem.h
    UIProcess/API/Cocoa/WKContentRuleList.h
    UIProcess/API/Cocoa/WKContentRuleListStore.h
    UIProcess/API/Cocoa/WKContentWorld.h
    UIProcess/API/Cocoa/WKContentWorldConfiguration.h
    UIProcess/API/Cocoa/WKContextMenuElementInfo.h
    UIProcess/API/Cocoa/WKDownload.h
    UIProcess/API/Cocoa/WKDownloadDelegate.h
    UIProcess/API/Cocoa/WKError.h
    UIProcess/API/Cocoa/WKFindConfiguration.h
    UIProcess/API/Cocoa/WKFindResult.h
    UIProcess/API/Cocoa/WKFrameInfo.h
    UIProcess/API/Cocoa/WKHTTPCookieStore.h
    UIProcess/API/Cocoa/WKNavigation.h
    UIProcess/API/Cocoa/WKNavigationAction.h
    UIProcess/API/Cocoa/WKNavigationDelegate.h
    UIProcess/API/Cocoa/WKNavigationResponse.h
    UIProcess/API/Cocoa/WKOpenPanelParameters.h
    UIProcess/API/Cocoa/WKPDFConfiguration.h
    UIProcess/API/Cocoa/WKPreferences.h
    UIProcess/API/Cocoa/WKPreviewActionItem.h
    UIProcess/API/Cocoa/WKPreviewActionItemIdentifiers.h
    UIProcess/API/Cocoa/WKPreviewElementInfo.h
    UIProcess/API/Cocoa/WKProcessPool.h
    UIProcess/API/Cocoa/WKScriptMessage.h
    UIProcess/API/Cocoa/WKScriptMessageHandler.h
    UIProcess/API/Cocoa/WKScriptMessageHandlerWithReply.h
    UIProcess/API/Cocoa/WKSecurityOrigin.h
    UIProcess/API/Cocoa/WKSnapshotConfiguration.h
    UIProcess/API/Cocoa/WKUIDelegate.h
    UIProcess/API/Cocoa/WKURLSchemeHandler.h
    UIProcess/API/Cocoa/WKURLSchemeTask.h
    UIProcess/API/Cocoa/WKUserContentController.h
    UIProcess/API/Cocoa/WKUserScript.h
    UIProcess/API/Cocoa/WKWebView.h
    UIProcess/API/Cocoa/WKWebViewConfiguration.h
    UIProcess/API/Cocoa/WKWebpagePreferences.h
    UIProcess/API/Cocoa/WKWebsiteDataRecord.h
    UIProcess/API/Cocoa/WKWebsiteDataStore.h
    UIProcess/API/Cocoa/WKWindowFeatures.h

    ${CMAKE_CURRENT_BINARY_DIR}/WebKitLegacy.h
)

list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS
    Platform/spi/ios/UIKitSPI.h

    Shared/API/Cocoa/RemoteObjectInvocation.h
    Shared/API/Cocoa/RemoteObjectRegistry.h
    Shared/API/Cocoa/WKBrowsingContextHandle.h
    Shared/API/Cocoa/WKDragDestinationAction.h
    Shared/API/Cocoa/WKMain.h
    Shared/API/Cocoa/WKRemoteObject.h
    Shared/API/Cocoa/WKRemoteObjectCoder.h
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

    Shared/mac/SecItemRequestData.h
    Shared/mac/SecItemResponseData.h

    UIProcess/API/C/mac/WKContextPrivateMac.h
    UIProcess/API/C/mac/WKInspectorPrivateMac.h
    UIProcess/API/C/mac/WKNotificationPrivateMac.h
    UIProcess/API/C/mac/WKPagePrivateMac.h
    UIProcess/API/C/mac/WKProtectionSpaceNS.h
    UIProcess/API/C/mac/WKWebsiteDataStoreRefPrivateMac.h

    UIProcess/API/Cocoa/NSAttributedStringPrivate.h
    UIProcess/API/Cocoa/PageLoadStateObserver.h
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
    UIProcess/API/Cocoa/WKContentRuleListPrivate.h
    UIProcess/API/Cocoa/WKContentRuleListStorePrivate.h
    UIProcess/API/Cocoa/WKContentWorldPrivate.h
    UIProcess/API/Cocoa/WKContextMenuElementInfoPrivate.h
    UIProcess/API/Cocoa/WKErrorPrivate.h
    UIProcess/API/Cocoa/WKFrameInfoPrivate.h
    UIProcess/API/Cocoa/WKHTTPCookieStorePrivate.h
    UIProcess/API/Cocoa/WKHistoryDelegatePrivate.h
    UIProcess/API/Cocoa/WKMenuItemIdentifiersPrivate.h
    UIProcess/API/Cocoa/WKNSURLAuthenticationChallenge.h
    UIProcess/API/Cocoa/WKNavigationActionPrivate.h
    UIProcess/API/Cocoa/WKNavigationData.h
    UIProcess/API/Cocoa/WKNavigationDelegatePrivate.h
    UIProcess/API/Cocoa/WKNavigationPrivate.h
    UIProcess/API/Cocoa/WKNavigationResponsePrivate.h
    UIProcess/API/Cocoa/WKOpenPanelParametersPrivate.h
    UIProcess/API/Cocoa/WKPreferencesPrivate.h
    UIProcess/API/Cocoa/WKProcessPoolPrivate.h
    UIProcess/API/Cocoa/WKSecurityOriginPrivate.h
    UIProcess/API/Cocoa/WKUIDelegatePrivate.h
    UIProcess/API/Cocoa/WKURLSchemeTaskPrivate.h
    UIProcess/API/Cocoa/WKUserContentControllerPrivate.h
    UIProcess/API/Cocoa/WKUserScriptPrivate.h
    UIProcess/API/Cocoa/WKWebArchive.h
    UIProcess/API/Cocoa/WKWebViewConfigurationPrivate.h
    UIProcess/API/Cocoa/WKWebViewPrivate.h
    UIProcess/API/Cocoa/WKWebViewPrivateForTesting.h
    UIProcess/API/Cocoa/WKWebpagePreferencesPrivate.h
    UIProcess/API/Cocoa/WKWebsiteDataRecordPrivate.h
    UIProcess/API/Cocoa/WKWebsiteDataStorePrivate.h
    UIProcess/API/Cocoa/WKWindowFeaturesPrivate.h
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
    UIProcess/API/Cocoa/_WKTranslationDelegate.h
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

    UIProcess/Cocoa/WKContactPicker.h
    UIProcess/Cocoa/WKShareSheet.h
    UIProcess/Cocoa/_WKCaptionStyleMenuController.h

    UIProcess/Extensions/Cocoa/_WKWebExtensionDeclarativeNetRequestRule.h
    UIProcess/Extensions/Cocoa/_WKWebExtensionDeclarativeNetRequestTranslator.h

    WebKitSwift/Preview/WKPreviewWindowController.h

    WebProcess/Extensions/Cocoa/_WKWebExtensionWebNavigationURLFilter.h
    WebProcess/Extensions/Cocoa/_WKWebExtensionWebRequestFilter.h

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

# Headers referenced by iOS_Private.modulemap but not picked up elsewhere.
# Xcode's Headers build phase installs all of these as Private on every Apple
# platform (no per-target exclusions), so the iOS WebKit.framework needs them
# staged for the framework module's `header "X.h"` declarations to resolve.
list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS
    Platform/cocoa/WKCrashReporter.h

    Platform/unix/EnvironmentUtilities.h

    Shared/WebPushDaemonConstants.h

    Shared/API/c/WKActionMenuItemTypes.h
    Shared/API/c/WKActionMenuTypes.h
    Shared/API/c/WKImmediateActionTypes.h
    Shared/API/c/WKRenderLayer.h
    Shared/API/c/WKRenderObject.h
    Shared/API/c/WKUserContentURLPattern.h

    UIProcess/_WKWebViewPrintFormatter.h

    UIProcess/API/C/WKContextMenuListener.h
    UIProcess/API/C/WKKeyValueStorageManager.h

    UIProcess/API/C/cg/WKIconDatabaseCG.h

    UIProcess/API/C/mac/WKFrameMac.h

    UIProcess/API/mac/WKWebViewPrivateForTestingMac.h

    UIProcess/Cocoa/SOAuthorization/SOAuthorizationNSURLExtras.h

    WebProcess/API/Cocoa/WKWebProcess.h

    WebProcess/InjectedBundle/API/c/mac/WKBundleMac.h
    WebProcess/InjectedBundle/API/c/mac/WKBundlePageBannerMac.h
    WebProcess/InjectedBundle/API/c/mac/WKBundlePageMac.h
)

file(GLOB _webkit_api_headers RELATIVE "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/GPUProcess/graphics/Model/*.h"
    "${WEBKIT_DIR}/Shared/API/Cocoa/*.h"
    "${WEBKIT_DIR}/UIProcess/API/Cocoa/*.h"
    "${WEBKIT_DIR}/UIProcess/API/ios/*.h"
    "${WEBKIT_DIR}/UIProcess/DigitalCredentials/*.h"
    "${WEBKIT_DIR}/UIProcess/ios/fullscreen/*.h"
    "${WEBKIT_DIR}/WebKitSwift/IdentityDocumentServices/*.h"
)
# UIProcess/API/Cocoa/WebKitLegacy.h is not in pbxproj; Xcode stages
# WebKit.framework/Headers/WebKitLegacy.h via the migrate phase by renaming
# WebKitLegacy/PrivateHeaders/WebKit.h. Letting the glob pick it up here would
# stage the full Mac-style WebKitLegacy umbrella into WebKit.framework/PrivateHeaders/,
# auto-discovering a `WebKit_Private.WebKitLegacy` submodule that conflicts
# with the WebKitLegacy framework module's `WebFrame` definition.
list(FILTER _webkit_api_headers EXCLUDE REGEX "/WebKitLegacy\\.h$")
list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS ${_webkit_api_headers})
unset(_webkit_api_headers)

list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    UIProcess/API/Cocoa/WKDOMNodeSnapshot.h
    UIProcess/API/Cocoa/WKFormInfo.h
    UIProcess/API/Cocoa/WKImmersiveEnvironment.h
    UIProcess/API/Cocoa/WKImmersiveEnvironmentDelegate.h
    UIProcess/API/Cocoa/WKJSHandle.h
    UIProcess/API/Cocoa/WKWebExtension.h
    UIProcess/API/Cocoa/WKWebExtensionAction.h
    UIProcess/API/Cocoa/WKWebExtensionCommand.h
    UIProcess/API/Cocoa/WKWebExtensionContext.h
    UIProcess/API/Cocoa/WKWebExtensionController.h
    UIProcess/API/Cocoa/WKWebExtensionControllerConfiguration.h
    UIProcess/API/Cocoa/WKWebExtensionControllerDelegate.h
    UIProcess/API/Cocoa/WKWebExtensionDataRecord.h
    UIProcess/API/Cocoa/WKWebExtensionDataType.h
    UIProcess/API/Cocoa/WKWebExtensionMatchPattern.h
    UIProcess/API/Cocoa/WKWebExtensionMessagePort.h
    UIProcess/API/Cocoa/WKWebExtensionPermission.h
    UIProcess/API/Cocoa/WKWebExtensionTab.h
    UIProcess/API/Cocoa/WKWebExtensionTabConfiguration.h
    UIProcess/API/Cocoa/WKWebExtensionWindow.h
    UIProcess/API/Cocoa/WKWebExtensionWindowConfiguration.h
)

set(_internal_headers ${WebKit_PUBLIC_FRAMEWORK_HEADERS})
list(FILTER _internal_headers INCLUDE REGEX "Internal\\.h$")
list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS ${_internal_headers})
list(FILTER WebKit_PUBLIC_FRAMEWORK_HEADERS EXCLUDE REGEX "Internal\\.h$")
unset(_internal_headers)

file(GLOB _webkit_ios_impl_headers RELATIVE "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/UIProcess/Cocoa/*.h"
    "${WEBKIT_DIR}/UIProcess/ios/*.h"
    "${WEBKIT_DIR}/UIProcess/ios/forms/*.h"
)
list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS ${_webkit_ios_impl_headers})
unset(_webkit_ios_impl_headers)

list(REMOVE_DUPLICATES WebKit_PUBLIC_FRAMEWORK_HEADERS)
list(REMOVE_DUPLICATES WebKit_PRIVATE_FRAMEWORK_HEADERS)


list(APPEND WebKit_PRIVATE_FRAMEWORK_HEADERS
    ${CMAKE_SOURCE_DIR}/Source/WebKitLegacy/ios/WebCoreSupport/WebSelectionRect.h
    ${CMAKE_SOURCE_DIR}/Source/WebKitLegacy/mac/Misc/WebNSURLExtras.h
    ${CMAKE_SOURCE_DIR}/Source/WebKitLegacy/mac/WebView/WebFeature.h
)

# FIXME: Re-export all WebKitLegacy headers. https://bugs.webkit.org/show_bug.cgi?id=312083

target_link_options(WebKit PRIVATE
    "LINKER:-weak_framework,BrowserEngineKit"
    "LINKER:-weak_framework,CoreML"
    "LINKER:-weak_framework,CorePrediction"
    "LINKER:-weak_framework,NaturalLanguage"
)

if (WEBKIT_SDK_IS_XROS)
    target_link_options(WebKit PRIVATE "LINKER:-weak_framework,MRUIKit")
endif ()

if (CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
    target_link_options(WebKit PRIVATE "SHELL:-L${CMAKE_OSX_SYSROOT}/usr/local/lib/dyld" "LINKER:-hidden-lsandbox-static")
else ()
    target_link_options(WebKit PRIVATE -lsandbox)
endif ()

add_dependencies(WebKit WebKitLegacy)
target_link_options(WebKit PRIVATE
    "LINKER:-reexport_library,$<TARGET_LINKER_FILE:WebKitLegacy>"
)

set(_wk_framework_dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework)

set(_wk_assets_staging "${CMAKE_CURRENT_BINARY_DIR}/WebKit-Assets")
set(_wk_xcassets
    ${WEBKIT_DIR}/Resources/SafeBrowsing.xcassets
    ${WEBKIT_DIR}/HTTPSBrowsingWarning.xcassets
    ${WEBKIT_DIR}/Resources/ios/iOS.xcassets
)
if (CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
    set(_actool_platform "iphonesimulator")
else ()
    set(_actool_platform "iphoneos")
endif ()
WEBKIT_XCRUN(_actool -f actool)
add_custom_command(
    OUTPUT ${_wk_assets_staging}/Assets.car
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_wk_assets_staging}
    COMMAND ${_actool} --compile ${_wk_assets_staging}
        --platform ${_actool_platform} --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
        ${_wk_xcassets}
    DEPENDS ${_wk_xcassets}
    COMMENT "Compiling WebKit asset catalogs"
    VERBATIM)
add_custom_target(WebKit_Assets DEPENDS ${_wk_assets_staging}/Assets.car)
add_dependencies(WebKit WebKit_Assets)

function(WEBKIT_EMBED_EXTENSION _host_target _ext_name _host_bundle_id)
    set(options CHANGE_EXTENSION_POINT ADD_ATS)
    cmake_parse_arguments(ARG "${options}" "" "" ${ARGN})

    set(_dst_plist "$<TARGET_FILE_DIR:${_host_target}>/Extensions/${_ext_name}.appex/Info.plist")

    add_custom_command(TARGET ${_host_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${_host_target}>/Extensions"
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            "$<TARGET_FILE_DIR:${_host_target}>/Extensions/${_ext_name}.appex"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${_ext_name}.appex"
            "$<TARGET_FILE_DIR:${_host_target}>/Extensions/${_ext_name}.appex"
        COMMAND /usr/libexec/PlistBuddy -c
            "Set :CFBundleIdentifier ${_host_bundle_id}.${_ext_name}"
            "${_dst_plist}"
        COMMENT "Embedding ${_ext_name} in ${_host_target}")

    if (ARG_CHANGE_EXTENSION_POINT)
        add_custom_command(TARGET ${_host_target} POST_BUILD
            COMMAND /usr/libexec/PlistBuddy -c
                "Set :EXAppExtensionAttributes:EXExtensionPointIdentifier com.apple.web-browser-engine.content"
                "${_dst_plist}")
    endif ()

    if (ARG_ADD_ATS)
        add_custom_command(TARGET ${_host_target} POST_BUILD
            COMMAND /usr/libexec/PlistBuddy -c
                "Add :NSAppTransportSecurity dict"
                "${_dst_plist}"
            COMMAND /usr/libexec/PlistBuddy -c
                "Add :NSAppTransportSecurity:NSAllowsArbitraryLoads bool true"
                "${_dst_plist}")
    endif ()

    add_custom_command(TARGET ${_host_target} POST_BUILD
        COMMAND codesign --force --sign -
            "$<TARGET_FILE_DIR:${_host_target}>/Extensions/${_ext_name}.appex")
endfunction()

function(WEBKIT_DEFINE_IOS_RESOURCES)
    # The XPC service symlinks must be created in the SAME POST_BUILD chain as
    # the codesign below; a separate add_custom_command(POST_BUILD ...)
    # registered afterward modifies the framework after the seal and breaks
    # code-sign verification at sim runtime. Inject inline.
    if (NOT USE_EXTENSIONKIT)
        set(_xpc_service_bundles
            com.apple.WebKit.Networking
            com.apple.WebKit.WebContent
            com.apple.WebKit.WebContent.CaptivePortal
            com.apple.WebKit.WebContent.EnhancedSecurity
        )
        if (ENABLE_GPU_PROCESS)
            list(APPEND _xpc_service_bundles com.apple.WebKit.GPU)
        endif ()
        set(_xpc_service_symlinks
            COMMAND ${CMAKE_COMMAND} -E make_directory
                ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices)
        foreach (_bundle_id IN LISTS _xpc_service_bundles)
            list(APPEND _xpc_service_symlinks
                COMMAND ${CMAKE_COMMAND} -E create_symlink ../../${_bundle_id}.xpc
                    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices/${_bundle_id}.xpc)
        endforeach ()
    endif ()

    add_custom_command(TARGET WebKit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/en.lproj
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${WEBKIT_DIR}/en.lproj/InfoPlist.strings
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/en.lproj/InfoPlist.strings
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${WEBKIT_DIR}/Resources/ResourceLoadStatistics/corePrediction_model
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/corePrediction_model
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${WEBKIT_DIR}/Resources/TextExtractionFilter.mlmodel
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/TextExtractionFilter.mlmodel
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${WEBKIT_DIR}/WebKitSwift/RealityKit/studio_lighting_objectmode_v3.reibl
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/studio_lighting_objectmode_v3.reibl
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${WEBKIT_DIR}/WebKitSwift/RealityKit/studio_lighting_objectmode_v3_diffmap.ktx
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/studio_lighting_objectmode_v3_diffmap.ktx
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${WEBKIT_DIR}/WebKitSwift/RealityKit/studio_lighting_objectmode_v3_specmap.ktx
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/studio_lighting_objectmode_v3_specmap.ktx
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${_wk_assets_staging}/Assets.car
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Assets.car
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Resources
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions
        COMMAND ${CMAKE_COMMAND} -E rm -rf
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Frameworks
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Frameworks
        COMMAND ${CMAKE_COMMAND} -E create_symlink ../../libWebKitSwift.dylib
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Frameworks/libWebKitSwift.dylib
        ${_xpc_service_symlinks}
        COMMAND codesign --force --sign - ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework
        COMMENT "Installing WebKit.framework resources and codesigning")

    # Note: the XPC service and Frameworks/libWebKitSwift.dylib symlinks MUST be
    # created in the same POST_BUILD chain as codesign above. A separate
    # add_custom_command modifies the framework after the seal, which breaks
    # codesign verification at sim runtime.

    set(_sb_profiles_dir "${WEBKIT_DIR}/Resources/SandboxProfiles/ios")
    set(_sb_output_dir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework")

    set(_sb_include_flags
        -I ${WEBKIT_DIR}
        -I ${WTF_FRAMEWORK_HEADERS_DIR}
        -I ${bmalloc_FRAMEWORK_HEADERS_DIR}
    )
    if (WEBKIT_ADDITIONS_INCLUDE_PATH)
        list(APPEND _sb_include_flags -I ${WEBKIT_ADDITIONS_INCLUDE_PATH})
    endif ()
    if (WEBKIT_ADDITIONS_COMPILE_PATH)
        list(APPEND _sb_include_flags -I ${WEBKIT_ADDITIONS_COMPILE_PATH})
    endif ()
    if (EXISTS "${CMAKE_BINARY_DIR}/generated-stubs")
        list(APPEND _sb_include_flags -I ${CMAKE_BINARY_DIR}/generated-stubs)
    endif ()

    set(WebKit_SB_FILES "")
    foreach (_sb_profile
        com.apple.WebKit.WebContent.Development
        com.apple.WebKit.Networking.Development
        com.apple.WebKit.GPU.Development)
        add_custom_command(
            OUTPUT ${_sb_output_dir}/${_sb_profile}.sb
            COMMAND grep -o "^[^;]*" ${_sb_profiles_dir}/${_sb_profile}.sb.in |
                    ${CMAKE_C_COMPILER} -isysroot ${CMAKE_OSX_SYSROOT} -E -P -w -include wtf/Platform.h ${_sb_include_flags} - >
                    ${_sb_output_dir}/${_sb_profile}.sb
            DEPENDS ${_sb_profiles_dir}/${_sb_profile}.sb.in
            COMMENT "Compiling sandbox profile ${_sb_profile}.sb"
            VERBATIM)
        list(APPEND WebKit_SB_FILES ${_sb_output_dir}/${_sb_profile}.sb)
    endforeach ()
    add_custom_target(WebKitIOSSandboxProfiles ALL DEPENDS ${WebKit_SB_FILES})
    add_dependencies(WebKit WebKitIOSSandboxProfiles)
endfunction()

function(WEBKIT_DEFINE_PROCESS_EXTENSIONS)
    function(WEBKIT_IOS_EXTENSION _name _bundle_id _info_plist _swift_source _entitlements)
        set(_appex_dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${_name}.appex)
        set(_executable_name ${_bundle_id})
        file(MAKE_DIRECTORY ${_appex_dir})

        set(BUNDLE_VERSION ${MACOSX_FRAMEWORK_BUNDLE_VERSION})
        set(EXECUTABLE_NAME ${_executable_name})
        set(PRODUCT_BUNDLE_IDENTIFIER ${_bundle_id})
        set(PRODUCT_BUNDLE_NAME ${_name})
        configure_file(${_info_plist} ${_appex_dir}/Info.plist)

        # Add platform keys required by runningboardd/ExtensionKit validation.
        # TARGETED_DEVICE_FAMILY from Configurations/BaseExtension.xcconfig, which
        # the extension targets set for every SDK.
        WEBKIT_GET_DEVICE_FAMILY(_device_family 1 2 7)
        WEBKIT_ADD_EMBEDDED_BUNDLE_PLIST_KEYS(${_appex_dir}/Info.plist ${_device_family})

        WEBKIT_EXECUTABLE_DECLARE(${_name})
        set(${_name}_SOURCES ${_swift_source})
        add_dependencies(${_name} WebKit)

        set_target_properties(${_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${_appex_dir}"
            OUTPUT_NAME "${_executable_name}"
            Swift_MODULE_NAME "${_name}"
            # MACOSX_BUNDLE defaults on for the embedded SDKs, which would nest
            # an .app inside the .appex.
            MACOSX_BUNDLE FALSE
            # Sign the appex bundle. 
            CODE_SIGN_BUNDLE "${_appex_dir}"
        )
        set_property(TARGET ${_name} PROPERTY CODE_SIGN_FLAGS
            --timestamp=none --generate-entitlement-der)

        webkit_target_add_swift_options(${_name}
            "-import-objc-header ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/CMakeExtensionBridge.h"
            -parse-as-library
            -application-extension
        )

        target_link_libraries(${_name} PRIVATE
            "-F${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
            "-framework WebKit"
            "-framework BrowserEngineKit"
            "-framework ExtensionFoundation"
            "-framework Foundation"
        )

        target_link_options(${_name} PRIVATE
            "LINKER:-rpath,@loader_path/../../Frameworks"
            "LINKER:-rpath,@loader_path/../.."
            "LINKER:-e,_NSExtensionMain"
        )

        # Simulator: embed only. Device: embed and pass to codesign.
        WEBKIT_EMBED_ENTITLEMENTS(${_name} ${_entitlements})
        if (NOT WEBKIT_SDK_IS_SIMULATOR)
            set_property(TARGET ${_name} PROPERTY
                CODE_SIGN_ENTITLEMENTS "${_entitlements}")
        endif ()

        WEBKIT_EXECUTABLE(${_name})
    endfunction()

    WEBKIT_RESOLVE_ENTITLEMENTS(_webcontent_ext_ents "WebContentProcessExtension.entitlements")
    WEBKIT_IOS_EXTENSION(WebContentExtension
        "com.apple.WebKit.WebContent"
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/WebContentExtension-Info.plist
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/WebContentProcessExtension.swift
        ${_webcontent_ext_ents})

    WEBKIT_IOS_EXTENSION(WebContentEnhancedSecurityExtension
        "com.apple.WebKit.WebContent.EnhancedSecurity"
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/WebContentExtension-EnhancedSecurity-Info.plist
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/WebContentProcessExtension.swift
        ${_webcontent_ext_ents})

    WEBKIT_IOS_EXTENSION(WebContentCaptivePortalExtension
        "com.apple.WebKit.WebContent.CaptivePortal"
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/WebContentExtension-CaptivePortal-Info.plist
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/WebContentProcessExtension.swift
        ${_webcontent_ext_ents})

    WEBKIT_RESOLVE_ENTITLEMENTS(_networking_ext_ents "NetworkingProcessExtension.entitlements")
    WEBKIT_IOS_EXTENSION(NetworkingExtension
        "com.apple.WebKit.Networking"
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/NetworkingExtension-Info.plist
        ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/NetworkingProcessExtension.swift
        ${_networking_ext_ents})

    if (ENABLE_GPU_PROCESS)
        WEBKIT_RESOLVE_ENTITLEMENTS(_gpu_ext_ents "GPUProcessExtension.entitlements")
        WEBKIT_IOS_EXTENSION(GPUExtension
            "com.apple.WebKit.GPU"
            ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/GPUExtension-Info.plist
            ${WEBKIT_DIR}/Shared/AuxiliaryProcessExtensions/GPUProcessExtension.swift
            ${_gpu_ext_ents})
    endif ()
endfunction()

else ()

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${ICU_INCLUDE_DIRS}"
    "${WEBKITLEGACY_DIR}"
    "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}"
)

set(WebProcess_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})
set(NetworkProcess_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})

# WebBackForwardList.swift and friends need the full C++ WebKit_Internal module
# (WebPageProxy, SessionState, WebBackForwardListSwiftUtilities, ...) so use the
# source-tree map directly. The earlier ObjC-only stripped map is insufficient
# once ENABLE_BACK_FORWARD_LIST_SWIFT pulls in C++ interop.
set(WebKit_SWIFT_INTEROP_MODULE_PATH "${WEBKIT_DIR}/Modules/Internal")

# The full WebKit_Internal C++ module pulls in WebPageProxy.h and friends, which
# quote-include across the entire WebKit/WebCore/JSC private header set. Mirror
# the C++ target's include directories to swiftc's Clang importer so those
# resolve. cmakeconfig.h is force-included because the headers assume the
# project's prefix header has already defined ENABLE()/HAVE() values.
#
# The last two entries complete the search path for the WebCore_Private umbrella:
# its PrivateHeaders/ quote-include generated WebCore headers
# (WebCore_DERIVED_SOURCES_DIR) and style/computed source inlines. This is the
# live Mac importer list; the CMakeLists.txt copy is inert on Apple.
set(WebKit_SWIFT_CLANG_INCLUDE_DIRS
    ${CMAKE_BINARY_DIR}
    ${WebKit_FRAMEWORK_HEADERS_DIR}
    ${WebKit_DERIVED_SOURCES_DIR}
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
    ${PAL_FRAMEWORK_HEADERS_DIR}
    ${ICU_INCLUDE_DIRS}
    ${WebKit_PRIVATE_INCLUDE_DIRECTORIES}
    ${WebCore_DERIVED_SOURCES_DIR}
    ${WEBCORE_DIR}/style/computed
)

# -Xcc -D/-f flags shared with PAL/WebGPU come from
# _WEBKIT_COMPUTE_SWIFT_SHARED_CLANG_FLAGS so all three targets land in the
# same SwiftModuleCache hash dir. Only -I (not hashed) remains per-target.
foreach (_dir IN LISTS WebKit_SWIFT_CLANG_INCLUDE_DIRS)
    target_compile_options(WebKit PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${_dir}>")
endforeach ()
foreach (_dir IN LISTS WebKit_SWIFT_INCLUDE_DIRECTORIES)
    target_compile_options(WebKit PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:-I${_dir}>")
endforeach ()

webkit_target_add_swift_options(WebKit
    "-library-level api"
    "-enable-experimental-feature RequiresObjC=Foundation"
)

# Turn on library evolution and emit the swift interface files.
target_compile_options(WebKit PRIVATE
        "$<$<COMPILE_LANGUAGE:Swift>:-enable-library-evolution>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-emit-module-interface-path ${CMAKE_BINARY_DIR}/Source/WebKit/WebKit.swiftinterface>"
        "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-emit-private-module-interface-path ${CMAKE_BINARY_DIR}/Source/WebKit/WebKit.private.swiftinterface>"
)

webkit_target_add_swift_options(WebKit "@${_swift_tba_resp}")

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
        ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}
    WORKING_DIRECTORY ${WebKit_DERIVED_SOURCES_DIR}
    VERBATIM
)

# Headers which import WebKitAdditions fragments have to be run through the
# replacement script rather than being copied verbatim. The processed copy takes
# the source header's place in the list, so that the header maps resolve
# <WebKit/Foo.h> to the copy with the additions spliced in rather than to the
# source tree. WebKit's own sources are pointed back at the unprocessed headers
# below.
set(_webkitadditions_source_headers)
set(_header_lists WebKit_PUBLIC_FRAMEWORK_HEADERS WebKit_PRIVATE_FRAMEWORK_HEADERS)
set(_header_dirs WebKit_HEADERS_DIR WebKit_PRIVATE_HEADERS_DIR)
foreach (_header_list _header_dir IN ZIP_LISTS _header_lists _header_dirs)
    set(_updated_headers)
    foreach (_header IN LISTS ${_header_list})
        # Entries are relative to WEBKIT_DIR unless they are already absolute.
        set(_src ${WEBKIT_DIR})
        cmake_path(APPEND _src ${_header})
        file(READ ${_src} _contents)
        # Only run headers through the replacement script if they actually contain
        # a WKA import.
        if (NOT _contents MATCHES "#import <WebKitAdditions/.*\.h>")
            list(APPEND _updated_headers ${_header})
            continue ()
        endif ()

        cmake_path(GET _src FILENAME _name)
        set(_dst ${${_header_dir}}/${_name})
        add_custom_command(
            OUTPUT ${_dst}
            COMMAND
                env ${WEBKITADDITIONS_DEFINITIONS_FOR_HEADER_REPLACEMENT}
                    ${WEBKIT_DIR}/mac/replace-webkit-additions-includes.py
                    ${WebKitAdditions_FRAMEWORK_HEADERS_DIR} ${CMAKE_OSX_SYSROOT}
                    ${_src} ${_dst}
            MAIN_DEPENDENCY ${_src}
            DEPENDS ${WEBKITADDITIONS_HEADERS_DEPENDENCIES}
            VERBATIM
        )
        list(APPEND _updated_headers ${_dst})
        list(APPEND WebKit_WEBKITADDITIONS_HEADERS ${_dst})
        list(APPEND _webkitadditions_source_headers ${_src})
    endforeach ()
    set(${_header_list} ${_updated_headers})
endforeach ()

if (WebKit_WEBKITADDITIONS_HEADERS)
    # Everything reading these headers through a header map has to wait for the
    # replacement to run, including targets that only link against WebKit.
    add_custom_target(WebKit_ReplaceWebKitAdditionsIncludes ALL
        DEPENDS ${WebKit_WEBKITADDITIONS_HEADERS})
    list(APPEND WebKit_DEPENDENCIES WebKit_ReplaceWebKitAdditionsIncludes)
    list(APPEND WebKit_INTERFACE_DEPENDENCIES WebKit_ReplaceWebKitAdditionsIncludes)

    if (USE_HEADER_MAPS)
        # WebKit's own sources are the exception: they have to keep seeing the
        # unprocessed headers, the way the Xcode build's project header map points
        # them at the source tree. A spliced-in fragment declares its API inside one
        # of WebKit's own categories, while the WebKitAdditions .mm that implements
        # it declares a category of its own and imports the same fragment into that,
        # so a translation unit which sees both ends up with duplicate declarations
        # and with properties whose implementation is in the wrong category.
        #
        # The targets built from this directory pick up the header maps through the
        # directory's include directories, and `include_directories(BEFORE)`
        # prepends, so the last caller wins. Defer the call to the end of the
        # directory so this override is searched ahead of WebKit-framework-headers.
        WEBKIT_WRITE_HEADER_MAP(WebKit
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/WebKit-webkitadditions-source-headers.hmap
            FILES ${_webkitadditions_source_headers}
            QUOTED BRACKETED
        )
        cmake_language(DEFER DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} CALL include_directories BEFORE
            ${CMAKE_CURRENT_BINARY_DIR}/WebKit-webkitadditions-source-headers.hmap)
    endif ()
endif ()

# LINKER:-u forces a symbol reference so -dead_strip_dylibs won't prune the weak framework.
target_link_options(WebKit PRIVATE
    -lsandbox
    -F${CMAKE_BINARY_DIR}
    "LINKER:-weak_framework,WebInspectorUI"
    "LINKER:-u,_WebInspectorUIFrameworkLoad"
    "LINKER:-weak_framework,CoreML"
    "LINKER:-weak_framework,CorePrediction"
    "LINKER:-weak_framework,NaturalLanguage"
    # for bincompat, cf. rdar://117360317
    "LINKER:-reexport-lobjc"
)
add_dependencies(WebKit WebInspectorUIFramework)

# The Automation protocol description and its injected-script atoms ship in
# WebKit.framework/PrivateHeaders, where clients reach them through
# WEBKIT2_PRIVATE_HEADERS_DIR. Safari's WebDriver framework reads both when
# generating WDProtocol.h and the WD*ScriptSource.h sources.
WEBKIT_SYMLINK_FILES(WebKit_CopyAutomationProtocol
    DESTINATION ${WebKit_PRIVATE_HEADERS_DIR}
    FILES ${WEBKIT_DIR}/UIProcess/Automation/Automation.json
    FLATTENED
)
WEBKIT_SYMLINK_FILES(WebKit_CopyAutomationAtoms
    DESTINATION ${WebKit_PRIVATE_HEADERS_DIR}/atoms
    FILES
        ${WEBKIT_DIR}/UIProcess/Automation/atoms/ElementAttribute.js
        ${WEBKIT_DIR}/UIProcess/Automation/atoms/ElementDisplayed.js
        ${WEBKIT_DIR}/UIProcess/Automation/atoms/EnterFullscreen.js
        ${WEBKIT_DIR}/UIProcess/Automation/atoms/FindNodes.js
        ${WEBKIT_DIR}/UIProcess/Automation/atoms/FormElementClear.js
        ${WEBKIT_DIR}/UIProcess/Automation/atoms/FormSubmit.js
    FLATTENED
)
list(APPEND WebKit_DEPENDENCIES WebKit_CopyAutomationProtocol WebKit_CopyAutomationAtoms)

# XPC Services

function(WEBKIT_DEFINE_MACOS_RESOURCES)
    set(WebKit_RESOURCES_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/Resources)
    set(_sb_extra_includes "-isysroot" "${CMAKE_OSX_SYSROOT}")
    file(GLOB _sb_additions "${CMAKE_SOURCE_DIR}/WebKitLibraries/SDKs/macosx*-additions.sdk/usr/local/include")
    list(SORT _sb_additions)
    list(REVERSE _sb_additions)
    foreach (_d IN LISTS _sb_additions)
        if (EXISTS "${_d}/AvailabilityProhibitedInternal.h")
            list(APPEND _sb_extra_includes "-isystem" "${_d}")
            break ()
        endif ()
    endforeach ()
    if (EXISTS "${CMAKE_BINARY_DIR}/generated-stubs/AppleFeatures/AppleFeatures.h")
        list(APPEND _sb_extra_includes "-isystem" "${CMAKE_BINARY_DIR}/generated-stubs")
    endif ()
    # Pass -fsanitize so sandbox preprocessor sees __has_feature(address_sanitizer).
    if (ENABLE_SANITIZERS)
        foreach (_san IN LISTS ENABLE_SANITIZERS)
            list(APPEND _sb_extra_includes "-fsanitize=${_san}")
        endforeach ()
    endif ()

    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.sb COMMAND
        grep -o "^[^;]*" ${WEBKIT_DIR}/WebProcess/com.apple.WebProcess.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.sb
        DEPENDS ${WEBKIT_DIR}/WebProcess/com.apple.WebProcess.sb.in
        VERBATIM)
    list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.sb)

    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.x86.sb COMMAND
        grep -o "^[^;]*" ${WEBKIT_DIR}/WebProcess/com.apple.WebProcess.x86.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.x86.sb
        DEPENDS ${WEBKIT_DIR}/WebProcess/com.apple.WebProcess.x86.sb.in
        VERBATIM)
    list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.x86.sb)

    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebKit.NetworkProcess.sb COMMAND
        grep -o "^[^;]*" ${WEBKIT_DIR}/NetworkProcess/mac/com.apple.WebKit.NetworkProcess.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebKit.NetworkProcess.sb
        DEPENDS ${WEBKIT_DIR}/NetworkProcess/mac/com.apple.WebKit.NetworkProcess.sb.in
        VERBATIM)
    list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebKit.NetworkProcess.sb)

    if (ENABLE_GPU_PROCESS)
        add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebKit.GPUProcess.sb COMMAND
            grep -o "^[^;]*" ${WEBKIT_DIR}/GPUProcess/mac/com.apple.WebKit.GPUProcess.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebKit.GPUProcess.sb
            DEPENDS ${WEBKIT_DIR}/GPUProcess/mac/com.apple.WebKit.GPUProcess.sb.in
            VERBATIM)
        list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebKit.GPUProcess.sb)
    endif ()

    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebKit.webpushd.mac.sb COMMAND
        grep -o "^[^;]*" ${WEBKIT_DIR}/webpushd/mac/com.apple.WebKit.webpushd.mac.sb.in | clang -E -P -w -include wtf/Platform.h -I ${WTF_FRAMEWORK_HEADERS_DIR} -I ${bmalloc_FRAMEWORK_HEADERS_DIR} -I ${WEBKIT_DIR} ${_sb_extra_includes} - > ${WebKit_RESOURCES_DIR}/com.apple.WebKit.webpushd.mac.sb
        DEPENDS ${WEBKIT_DIR}/webpushd/mac/com.apple.WebKit.webpushd.mac.sb.in
        VERBATIM)
    list(APPEND WebKit_SB_FILES ${WebKit_RESOURCES_DIR}/com.apple.WebKit.webpushd.mac.sb)

    add_custom_target(WebKitSandboxProfiles ALL DEPENDS ${WebKit_SB_FILES})
    add_dependencies(WebKit WebKitSandboxProfiles)

    add_custom_command(OUTPUT ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib COMMAND
        ibtool --compile ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib ${WEBKIT_DIR}/Resources/WebContentProcess.xib
        VERBATIM)
    add_custom_target(WebContentProcessNib ALL DEPENDS ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib)
    # Must be in place before WebProcess links and seals the .xpc.
    add_dependencies(WebProcess WebContentProcessNib)

    set(_wk_xcassets
        ${WEBKIT_DIR}/Resources/SafeBrowsing.xcassets
        ${WEBKIT_DIR}/HTTPSBrowsingWarning.xcassets
    )
    list(TRANSFORM _wk_xcassets APPEND "/*" OUTPUT_VARIABLE _wk_xcassets_globs)
    # FIXME: GLOB isn't suitable for incremental builds (rdar://188725492)
    file(GLOB_RECURSE _wk_xcassets_contents LIST_DIRECTORIES true ${_wk_xcassets_globs})
    WEBKIT_XCRUN(_actool -f actool)
    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/Assets.car
        COMMAND ${_actool} --compile ${WebKit_RESOURCES_DIR} --output-format human-readable-text
            --platform macosx --target-device mac --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
            ${_wk_xcassets}
        DEPENDS ${_wk_xcassets} ${_wk_xcassets_contents}
        COMMENT "Compiling WebKit asset catalogs"
        VERBATIM)
    add_custom_target(WebKit_Assets DEPENDS ${WebKit_RESOURCES_DIR}/Assets.car)
    add_dependencies(WebKit WebKit_Assets)

    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/TextExtractionFilter.mlmodel COMMAND
        ${CMAKE_COMMAND} -E copy_if_different ${WEBKIT_DIR}/Resources/TextExtractionFilter.mlmodel ${WebKit_RESOURCES_DIR}/TextExtractionFilter.mlmodel
        VERBATIM)
    add_custom_target(WebKitTextExtractionFilterModel ALL DEPENDS ${WebKit_RESOURCES_DIR}/TextExtractionFilter.mlmodel)
    add_dependencies(WebKit WebKitTextExtractionFilterModel)

    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/corePrediction_model COMMAND
        ${CMAKE_COMMAND} -E copy_if_different ${WEBKIT_DIR}/Resources/ResourceLoadStatistics/corePrediction_model ${WebKit_RESOURCES_DIR}/corePrediction_model
        DEPENDS ${WEBKIT_DIR}/Resources/ResourceLoadStatistics/corePrediction_model
        VERBATIM)
    add_custom_target(WebKitCorePredictionModel ALL DEPENDS ${WebKit_RESOURCES_DIR}/corePrediction_model)
    add_dependencies(WebKit WebKitCorePredictionModel)
endfunction()

target_link_options(WebKit PRIVATE
    "SHELL:-Xlinker -weak_library -Xlinker ${CMAKE_OSX_SYSROOT}/usr/lib/libAccessibility.tbd"
    "SHELL:-Xlinker -weak_library -Xlinker ${CMAKE_OSX_SYSROOT}/usr/lib/libnetworkextension.tbd"
    "SHELL:-Xlinker -weak_library -Xlinker ${CMAKE_OSX_SYSROOT}/usr/lib/libbsm.tbd"
)

endif ()

# WebKit.framework/Modules: the module maps, the Swift module and the SwiftUI
# cross-import declaration.
set(_webkit_framework_dir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework")
set(_webkit_modules_dir "${_webkit_framework_dir}/${WEBKIT_FRAMEWORK_VERSION_PATH}Modules")
file(MAKE_DIRECTORY "${_webkit_framework_dir}")
if (WEBKIT_FRAMEWORK_VERSION_PATH)
    file(CREATE_LINK "Versions/Current/Modules" "${_webkit_framework_dir}/Modules" SYMBOLIC)
endif ()

if (WEBKIT_SDK_IS_MACOS)
    set(_modulemap_platform OSX)
else ()
    set(_modulemap_platform iOS)
endif ()

# The public module map is copied verbatim. The private one is preprocessed and
# has the addendum below spliced in.
# FIXME: Xcode doesn't use an addendum like this; figure out why and fix.
set(_modulemap_input "${WEBKIT_DIR}/Modules/${_modulemap_platform}.modulemap")
set(_private_modulemap_input "${WEBKIT_DIR}/Modules/${_modulemap_platform}_Private.modulemap")
set(_private_modulemap_intermediates_dir "${CMAKE_BINARY_DIR}/WebKit/Modules")
set(_private_modulemap_preprocessed "${_private_modulemap_intermediates_dir}/module.private.modulemap.preprocessed")
set(_private_modulemap_addendum "${_private_modulemap_intermediates_dir}/module.private.addendum.modulemap")
if (WEBKIT_SDK_IS_IOS_FAMILY)
    file(WRITE "${_private_modulemap_addendum}"
"  explicit module WKWebViewPrivate {
    header \"WKWebViewPrivate.h\"
    export *
  }
")
else ()
    file(WRITE "${_private_modulemap_addendum}" "")
endif ()

set(_private_modulemap_inject_script "${_private_modulemap_intermediates_dir}/inject-addendum.cmake")
file(WRITE "${_private_modulemap_inject_script}"
"file(READ \"\${INPUT}\" _content)
file(READ \"\${ADDENDUM}\" _addendum)
# Strip trailing whitespace, then replace the final `}` (closing
# `framework module WebKit_Private`) with addendum + `}`.
string(REGEX REPLACE \"[ \\t\\r\\n]+$\" \"\" _content \"\${_content}\")
string(REGEX REPLACE \"}$\" \"\${_addendum}}\\n\" _content \"\${_content}\")
file(WRITE \"\${OUTPUT}\" \"\${_content}\")
")

add_custom_command(
    OUTPUT "${_webkit_modules_dir}/module.modulemap"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_webkit_modules_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_modulemap_input}" "${_webkit_modules_dir}/module.modulemap"
    MAIN_DEPENDENCY "${_modulemap_input}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${_webkit_modules_dir}/module.private.modulemap"
    DEPENDS "${_private_modulemap_input}" "${_private_modulemap_addendum}" "${_private_modulemap_inject_script}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_webkit_modules_dir}"
    COMMAND ${CMAKE_C_COMPILER} -E -P -w
        -target ${WEBKIT_SDK_TARGET_TRIPLE}
        -isysroot ${CMAKE_OSX_SYSROOT}
        -x c "${_private_modulemap_input}"
        -o "${_private_modulemap_preprocessed}"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${_private_modulemap_preprocessed}
        -DADDENDUM=${_private_modulemap_addendum}
        -DOUTPUT=${_webkit_modules_dir}/module.private.modulemap
        -P ${_private_modulemap_inject_script}
    COMMENT "Preprocessing ${_modulemap_platform}_Private.modulemap"
    VERBATIM
)
add_custom_target(WebKit_CopyModules DEPENDS
    "${_webkit_modules_dir}/module.modulemap"
    "${_webkit_modules_dir}/module.private.modulemap"
)
# WebKit's own Swift compile loads the private module map from the framework.
list(APPEND WebKit_DEPENDENCIES WebKit_CopyModules)

# Stage WebKit's Swift module via a tracked add_custom_command so ninja replays
# the copies whenever the staged files go missing. POST_BUILD on the dylib link
# rule only fires on relink, leaving incremental builds with an empty Modules/.
set(_webkit_swift_output "${CMAKE_BINARY_DIR}/Source/WebKit")
set(_webkit_swiftmodule_dir "${_webkit_modules_dir}/WebKit.swiftmodule")
set(_webkit_staged_swiftmodule_artifacts "")
set(_webkit_stage_swiftmodule_commands "")
foreach (_ext IN ITEMS swiftmodule swiftdoc abi.json swiftinterface private.swiftinterface swiftsourceinfo)
    if (_ext STREQUAL "swiftsourceinfo")
        set(_staged "${_webkit_swiftmodule_dir}/Project/${WEBKIT_SWIFT_MODULE_TRIPLE}.${_ext}")
    else ()
        set(_staged "${_webkit_swiftmodule_dir}/${WEBKIT_SWIFT_MODULE_TRIPLE}.${_ext}")
    endif ()
    list(APPEND _webkit_staged_swiftmodule_artifacts "${_staged}")
    list(APPEND _webkit_stage_swiftmodule_commands
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_webkit_swift_output}/WebKit.${_ext}" "${_staged}"
    )
endforeach ()
add_custom_command(
    OUTPUT ${_webkit_staged_swiftmodule_artifacts}
    DEPENDS "${_webkit_swift_output}/WebKit.swiftmodule"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_webkit_swiftmodule_dir}/Project"
    ${_webkit_stage_swiftmodule_commands}
    COMMENT "Staging WebKit.swiftmodule into WebKit.framework/${WEBKIT_FRAMEWORK_VERSION_PATH}Modules/"
    VERBATIM
)
# The staging command is ordered after WebKit, so this target must not be added
# to WebKit_DEPENDENCIES; ALL is what gets it built.
add_custom_target(WebKit_StageSwiftModule ALL DEPENDS ${_webkit_staged_swiftmodule_artifacts})
# Ordering only; a dependency on the WebKit target would track the framework
# binary, which code signing rewrites later.
add_dependencies(WebKit_StageSwiftModule WebKit)

add_custom_command(
    OUTPUT "${_webkit_modules_dir}/WebKit.swiftcrossimport/SwiftUI.swiftoverlay"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_webkit_modules_dir}/WebKit.swiftcrossimport"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WEBKIT_DIR}/Modules/SwiftUI.swiftoverlay"
        "${_webkit_modules_dir}/WebKit.swiftcrossimport/SwiftUI.swiftoverlay"
    MAIN_DEPENDENCY "${WEBKIT_DIR}/Modules/SwiftUI.swiftoverlay"
    VERBATIM
)
add_custom_target(WebKit_SwiftCrossImport ALL DEPENDS
    "${_webkit_modules_dir}/WebKit.swiftcrossimport/SwiftUI.swiftoverlay")
add_dependencies(WebKit WebKit_SwiftCrossImport)

add_subdirectory(${WEBKIT_DIR}/_WebKit_SwiftUI)
