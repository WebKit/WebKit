add_compile_options("$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-std=c++2b>" "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-D__STDC_WANT_LIB_EXT1__>")
list(APPEND WebKit_COMPILE_OPTIONS
    "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-std=c++2b>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-D__STDC_WANT_LIB_EXT1__>"
)

find_library(NETWORK_LIBRARY Network)
find_library(SECURITY_LIBRARY Security)
find_library(UNIFORMTYPEIDENTIFIERS_LIBRARY UniformTypeIdentifiers)
find_library(AVFOUNDATION_LIBRARY AVFoundation)
find_library(DEVICEIDENTITY_LIBRARY DeviceIdentity HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
if (NOT DEVICEIDENTITY_LIBRARY)
    set(DEVICEIDENTITY_LIBRARY "" CACHE FILEPATH "" FORCE)
endif ()

add_compile_options(
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-DWK_XPC_SERVICE_SUFFIX=\".Development\">"
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-DWEBKIT_BUNDLE_VERSION=\"${WEBKIT_MAC_VERSION}\">"
)
list(APPEND WebKit_COMPILE_OPTIONS
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-DWK_XPC_SERVICE_SUFFIX=\".Development\">"
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-DWEBKIT_BUNDLE_VERSION=\"${WEBKIT_MAC_VERSION}\">"
)

set(MACOSX_FRAMEWORK_IDENTIFIER com.apple.WebKit)

include(Headers.cmake)

list(APPEND WebKit_UNIFIED_SOURCE_LIST_FILES
    "SourcesCocoa.txt"

    "Platform/SourcesCocoa.txt"
)
# FIXME: Test building on iOS and then enable on iOS.
if (NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
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

    UIProcess/PDF/WKPDFPageNumberIndicator.mm
    ${WEBKIT_DIR}/UIProcess/API/Cocoa/_WKTextExtraction.swift

    WebProcess/WebAuthentication/WebAuthenticatorCoordinator.cpp

    WebProcess/cocoa/AudioSessionRoutingArbitrator.cpp
    WebProcess/cocoa/LaunchServicesDatabaseManager.mm
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}/libwebrtc/PrivateHeaders"
    "${WEBKIT_DIR}/GPUProcess/graphics/Model"
    "${WEBKIT_DIR}/GPUProcess/media/cocoa"
    "${WEBKIT_DIR}/NetworkProcess/cocoa"
    "${WEBKIT_DIR}/NetworkProcess/PrivateClickMeasurement/cocoa"
    "${WEBKIT_DIR}/UIProcess/ios"
    "${WEBKIT_DIR}/UIProcess/API/C/mac"
    "${WEBKIT_DIR}/UIProcess/API/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Authentication/cocoa"
    "${WEBKIT_DIR}/UIProcess/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Cocoa/Separated"
    "${WEBKIT_DIR}/UIProcess/Cocoa/SOAuthorization"
    "${WEBKIT_DIR}/UIProcess/Cocoa/TextExtraction"
    "${WEBKIT_DIR}/UIProcess/Extensions/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Inspector/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Launcher/mac"
    "${WEBKIT_DIR}/UIProcess/Media/cocoa"
    "${WEBKIT_DIR}/UIProcess/Notifications/cocoa"
    "${WEBKIT_DIR}/UIProcess/PDF"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/cocoa"
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
    "${WEBKIT_DIR}/WebKitSwift/MarketplaceKit"
    "${WEBKIT_DIR}/WebKitSwift/WritingTools"
    "${WEBKIT_DIR}/Platform/spi/Cocoa"
    "${WEBKIT_DIR}/Platform/spi/ios"
    "${WEBKIT_DIR}/Platform/spi/mac"
    "${WEBKIT_DIR}/Platform/IPC/darwin"
    "${WEBKIT_DIR}/Platform/IPC/mac"
    "${WEBKIT_DIR}/Platform/IPC/cocoa"
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
    "${WEBKIT_DIR}/WebProcess/Plugins/PDF"
    "${WEBKIT_DIR}/WebProcess/Plugins/PDF/UnifiedPDF"
    "${WEBKIT_DIR}/WebProcess/WebPage/Cocoa"
    "${WEBKIT_DIR}/WebProcess/WebPage/RemoteLayerTree"
    "${WEBKIT_DIR}/WebProcess/WebPage/mac"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/cocoa"
    "${WEBKIT_DIR}/webpushd"
    "${WEBKIT_DIR}/webpushd/webpushtool"
    "${CMAKE_SOURCE_DIR}/Source/ThirdParty/libwebrtc/Source"
)

# FIXME: These should not have Development in production builds.
set(WebProcess_OUTPUT_NAME com.apple.WebKit.WebContent.Development)
set(NetworkProcess_OUTPUT_NAME com.apple.WebKit.Networking.Development)
set(GPUProcess_OUTPUT_NAME com.apple.WebKit.GPU.Development)

set(WebKit_SWIFT_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/Platform/spi/Cocoa"
    "${WEBKIT_DIR}/Platform/spi/Cocoa/Modules"
    "${WEBKIT_DIR}/Platform/spi/ios"
)

file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/WebKit/WebPushDaemonStubs.cpp" CONTENT
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
    webpushd/_WKMockUserNotificationCenter.mm
)

find_library(CRYPTOTOKENKIT_LIBRARY CryptoTokenKit)
find_library(USERNOTIFICATIONS_LIBRARY UserNotifications)
find_library(WRITINGTOOLS_LIBRARY WritingTools HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
find_library(APPLEPUSHSERVICE_LIBRARY ApplePushService HINTS ${CMAKE_OSX_SYSROOT}/System/Library/PrivateFrameworks)
list(APPEND WebKit_PRIVATE_LIBRARIES
    ${CRYPTOTOKENKIT_LIBRARY}
    ${USERNOTIFICATIONS_LIBRARY}
    ${WRITINGTOOLS_LIBRARY}
    ${APPLEPUSHSERVICE_LIBRARY}
)

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

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION}")
target_link_options(WebKit PRIVATE -framework AuthKit)


target_link_options(WebKit PRIVATE
    -Wl,-unexported_symbol,__ZTISt9bad_alloc
    -Wl,-unexported_symbol,__ZTISt9exception
    -Wl,-unexported_symbol,__ZTSSt9bad_alloc
    -Wl,-unexported_symbol,__ZTSSt9exception
    -Wl,-unexported_symbol,__ZdlPvS_
    -Wl,-unexported_symbol,__ZnwmPv
    -Wl,-unexported_symbol,__Znwm
    -Wl,-unexported_symbol,__ZTVNSt3__117bad_function_callE
    -Wl,-unexported_symbol,__ZTCNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE0_NS_13basic_istreamIcS2_EE
    -Wl,-unexported_symbol,__ZTCNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE0_NS_14basic_iostreamIcS2_EE
    -Wl,-unexported_symbol,__ZTCNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE16_NS_13basic_ostreamIcS2_EE
    -Wl,-unexported_symbol,__ZTTNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE
    -Wl,-unexported_symbol,__ZTVNSt3__115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEEE
    -Wl,-unexported_symbol,__ZTVNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE
    -Wl,-unexported_symbol,__ZTCNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE8_NS_13basic_ostreamIcS2_EE
    -Wl,-unexported_symbol,__ZTAXtlN7WebCore3CSS5RangeELdfff0000000000000ELd7ff0000000000000EEE
    "-Wl,-unexported_symbol,_$s*3Cxx*"
)

set(WebKit_OUTPUT_NAME WebKit)
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set_target_properties(WebKit PROPERTIES
        INSTALL_NAME_DIR "${WebKit_INSTALL_NAME_DIR}"
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
