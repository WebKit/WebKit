include(PlatformCocoa.cmake)

find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(CARBON_LIBRARY Carbon)
find_library(CORESERVICES_LIBRARY CoreServices)
find_library(SECURITYINTERFACE_LIBRARY SecurityInterface)
find_library(QUARTZ_LIBRARY Quartz)
find_library(AVFAUDIO_LIBRARY AVFAudio HINTS ${AVFOUNDATION_LIBRARY}/Versions/*/Frameworks)
add_compile_options(
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${QUARTZ_LIBRARY}/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CARBON_LIBRARY}/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks>"
)
list(APPEND WebKit_COMPILE_OPTIONS
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${QUARTZ_LIBRARY}/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CARBON_LIBRARY}/Frameworks>"
    "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks>"
)

add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CORESERVICES_LIBRARY}/Versions/Current/Frameworks>")
list(APPEND WebKit_COMPILE_OPTIONS "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-iframework${CORESERVICES_LIBRARY}/Versions/Current/Frameworks>")

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

list(APPEND WebKit_PRIVATE_LIBRARIES "-weak_framework PowerLog")

list(APPEND WebKit_SOURCES
    NetworkProcess/mac/NetworkConnectionToWebProcessMac.mm

    UIProcess/PDF/WKPDFHUDView.mm
    ${WEBKIT_DIR}/Platform/cocoa/WKMaterialHostingSupport.swift
    ${WEBKIT_DIR}/UIProcess/PDF/WKPDFHUDView.swift

    WebProcess/InjectedBundle/API/c/mac/WKBundlePageMac.mm
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${ICU_INCLUDE_DIRS}"
    "${WEBKIT_DIR}/GPUProcess/mac"
    "${WEBKIT_DIR}/NetworkProcess/mac"
    "${WEBKIT_DIR}/UIProcess/mac"
    "${WEBKIT_DIR}/UIProcess/API/mac"
    "${WEBKIT_DIR}/UIProcess/Inspector/mac"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/mac"
    # WebKitSwift ObjC interface headers — self-guard with feature checks.
    "${WEBKIT_DIR}/Shared/mac"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/mac"
    "${WEBKIT_DIR}/WebProcess/Model/mac"
    "${WEBKITLEGACY_DIR}"
    "${WebKitLegacy_FRAMEWORK_HEADERS_DIR}"
)

set(WebProcess_SOURCES Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp)
set(NetworkProcess_SOURCES Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp)
set(GPUProcess_SOURCES Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp)

set(WebProcess_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})
set(NetworkProcess_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})

# WebBackForwardList.swift and friends need the full C++ WebKit_Internal module
# (WebPageProxy, SessionState, WebBackForwardListSwiftUtilities, ...) so use the
# source-tree map directly. The earlier ObjC-only stripped map is insufficient
# once ENABLE_BACK_FORWARD_LIST_SWIFT pulls in C++ interop.
set(WebKit_SWIFT_INTEROP_MODULE_PATH "${WEBKIT_DIR}/Modules/Internal")

# Mac Swift compilation uses explicit module builds so libSwiftScan pre-builds
# all PCMs (including WebKit_Internal C++ interop) with the project -Xcc flags,
# avoiding duplicated per-process module compilation. Same rationale as iOS.
set(WebKit_SWIFT_EXPLICIT_MODULE_BUILD TRUE)

# Xcode does not set SWIFT_TREAT_WARNINGS_AS_ERRORS; override CMake's -warnings-as-errors.
# Must go in WebKit_COMPILE_OPTIONS (applied after -warnings-as-errors in _WEBKIT_TARGET_SETUP).
list(APPEND WebKit_COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:Swift>:-no-warnings-as-errors>")

# The full WebKit_Internal C++ module pulls in WebPageProxy.h and friends, which
# quote-include across the entire WebKit/WebCore/JSC private header set. Mirror
# the C++ target's include directories to swiftc's Clang importer so those
# resolve. cmakeconfig.h is force-included because the headers assume the
# project's prefix header has already defined ENABLE()/HAVE() values.
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
    ${WebCore_Private_SWIFT_MODULEMAP_DIR}
    ${WebKit_PRIVATE_INCLUDE_DIRECTORIES}
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

list(APPEND WebKit_SOURCES
    UIProcess/mac/_WKCaptionStyleMenuControllerAVKitMac.mm
    UIProcess/mac/_WKCaptionStyleMenuControllerMac.mm
)

list(APPEND WebKit_PRIVATE_LIBRARIES
    "-weak_framework PowerLog"
)

list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/WebPushDaemonConstants.h

    Shared/API/Cocoa/RemoteObjectInvocation.h
    Shared/API/Cocoa/RemoteObjectRegistry.h
    Shared/API/Cocoa/WKBrowsingContextHandle.h
    Shared/API/Cocoa/WKDataDetectorTypes.h
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
    UIProcess/Cocoa/_WKCaptionStyleMenuController.h

    UIProcess/Extensions/Cocoa/_WKWebExtensionDeclarativeNetRequestRule.h
    UIProcess/Extensions/Cocoa/_WKWebExtensionDeclarativeNetRequestTranslator.h

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

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION}")
# -Wl,-u forces a symbol reference so -dead_strip_dylibs won't prune the weak framework.
target_link_options(WebKit PRIVATE
    -lsandbox
    -framework AuthKit
    -F${CMAKE_BINARY_DIR}
    -weak_framework WebInspectorUI
    -Wl,-u,_WebInspectorUIFrameworkLoad
    "SHELL:-weak_framework CoreML"
    "SHELL:-weak_framework NaturalLanguage"
    # for bincompat, cf. rdar://117360317
    -Wl,-reexport-lobjc
)
add_dependencies(WebKit WebInspectorUIFramework)

set(WebKit_OUTPUT_NAME WebKit)

# XPC Services

function(WEBKIT_DEFINE_XPC_SERVICES)
    # _WebKit runloop type is obsolete (macOS < 11.0); modern libxpc requires NSRunLoop
    # or the XPC event handler never fires and WebContent hangs.
    set(RUNLOOP_TYPE NSRunLoop)
    set(WebKit_XPC_SERVICE_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/XPCServices)
    # Relative symlink (matches Xcode layout; absolute breaks if build dir is moved).
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

    # Without these XPC bundles, process swaps fail with "Invalid connection identifier".
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
    # Pass -fsanitize so sandbox preprocessor sees __has_feature(address_sanitizer).
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

    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/TextExtractionFilter.mlmodel COMMAND
        ${CMAKE_COMMAND} -E copy_if_different ${WEBKIT_DIR}/Resources/TextExtractionFilter.mlmodel ${WebKit_RESOURCES_DIR}/TextExtractionFilter.mlmodel
        VERBATIM)
    add_custom_target(WebKitTextExtractionFilterModel ALL DEPENDS ${WebKit_RESOURCES_DIR}/TextExtractionFilter.mlmodel)
    add_dependencies(WebKit WebKitTextExtractionFilterModel)
endfunction()
