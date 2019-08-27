add_definitions("-ObjC++ -std=c++17")
find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(CARBON_LIBRARY Carbon)
find_library(QUARTZ_LIBRARY Quartz)
find_library(AVFOUNDATION_LIBRARY AVFoundation)
find_library(AVFAUDIO_LIBRARY AVFAudio HINTS ${AVFOUNDATION_LIBRARY}/Versions/*/Frameworks)
add_definitions(-iframework ${QUARTZ_LIBRARY}/Frameworks)
add_definitions(-iframework ${CARBON_LIBRARY}/Frameworks)
add_definitions(-iframework ${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks)
add_definitions(-DWK_XPC_SERVICE_SUFFIX=".Development")

list(APPEND WebKit_LIBRARIES
    WebKitLegacy
    ${APPLICATIONSERVICES_LIBRARY}
)

if (NOT AVFAUDIO_LIBRARY-NOTFOUND)
    list(APPEND WebKit_LIBRARIES ${AVFAUDIO_LIBRARY})
endif ()

list(APPEND WebKit_SOURCES
    NetworkProcess/Classifier/WebResourceLoadStatisticsStore.cpp
    NetworkProcess/Classifier/WebResourceLoadStatisticsTelemetry.cpp

    NetworkProcess/Cookies/mac/WebCookieManagerMac.mm

    NetworkProcess/CustomProtocols/LegacyCustomProtocolManager.cpp

    NetworkProcess/CustomProtocols/Cocoa/LegacyCustomProtocolManagerCocoa.mm

    NetworkProcess/Downloads/PendingDownload.cpp

    NetworkProcess/Downloads/cocoa/DownloadCocoa.mm

    NetworkProcess/WebStorage/StorageManager.cpp

    NetworkProcess/cache/NetworkCacheDataCocoa.mm
    NetworkProcess/cache/NetworkCacheIOChannelCocoa.mm

    NetworkProcess/cocoa/NetworkDataTaskCocoa.mm
    NetworkProcess/cocoa/NetworkProcessCocoa.mm
    NetworkProcess/cocoa/NetworkSessionCocoa.mm

    NetworkProcess/mac/NetworkProcessMac.mm
    NetworkProcess/mac/RemoteNetworkingContext.mm

    Platform/IPC/cocoa/ConnectionCocoa.mm
    Platform/IPC/cocoa/MachMessage.cpp

    Platform/cf/ModuleCF.cpp

    Platform/cg/CGUtilities.cpp

    Platform/cocoa/WKCrashReporter.mm

    Platform/classifier/ResourceLoadStatisticsClassifier.cpp

    Platform/classifier/cocoa/ResourceLoadStatisticsClassifierCocoa.cpp

    Platform/foundation/LoggingFoundation.mm

    Platform/cocoa/SharedMemoryCocoa.cpp

    Platform/mac/LayerHostingContext.mm
    Platform/mac/MachUtilities.cpp
    Platform/mac/MenuUtilities.mm
    Platform/mac/StringUtilities.mm

    Platform/unix/EnvironmentUtilities.cpp

    PluginProcess/mac/PluginControllerProxyMac.mm
    PluginProcess/mac/PluginProcessMac.mm
    PluginProcess/mac/PluginProcessShim.mm

    Shared/APIWebArchive.mm
    Shared/APIWebArchiveResource.mm

    Shared/API/Cocoa/RemoteObjectInvocation.mm
    Shared/API/Cocoa/RemoteObjectRegistry.mm
    Shared/API/Cocoa/WKBrowsingContextHandle.mm
    Shared/API/Cocoa/WKRemoteObject.mm
    Shared/API/Cocoa/WKRemoteObjectCoder.mm
    Shared/API/Cocoa/WebKit.m
    Shared/API/Cocoa/_WKFrameHandle.mm
    Shared/API/Cocoa/_WKHitTestResult.mm
    Shared/API/Cocoa/_WKNSFileManagerExtras.mm
    Shared/API/Cocoa/_WKRemoteObjectInterface.mm
    Shared/API/Cocoa/_WKRemoteObjectRegistry.mm

    Shared/API/c/cf/WKErrorCF.cpp
    Shared/API/c/cf/WKStringCF.mm
    Shared/API/c/cf/WKURLCF.mm

    Shared/API/c/cg/WKImageCG.cpp

    Shared/API/c/mac/WKCertificateInfoMac.mm
    Shared/API/c/mac/WKObjCTypeWrapperRef.mm
    Shared/API/c/mac/WKURLRequestNS.mm
    Shared/API/c/mac/WKURLResponseNS.mm
    Shared/API/c/mac/WKWebArchive.cpp
    Shared/API/c/mac/WKWebArchiveResource.cpp

    Shared/Cocoa/APIDataCocoa.mm
    Shared/Cocoa/APIObject.mm
    Shared/Cocoa/AuxiliaryProcessCocoa.mm
    Shared/Cocoa/CompletionHandlerCallChecker.mm
    Shared/Cocoa/DataDetectionResult.mm
    Shared/Cocoa/LoadParametersCocoa.mm
    Shared/Cocoa/WKNSArray.mm
    Shared/Cocoa/WKNSData.mm
    Shared/Cocoa/WKNSDictionary.mm
    Shared/Cocoa/WKNSError.mm
    Shared/Cocoa/WKNSNumber.mm
    Shared/Cocoa/WKNSString.mm
    Shared/Cocoa/WKNSURL.mm
    Shared/Cocoa/WKNSURLExtras.mm
    Shared/Cocoa/WKNSURLRequest.mm
    Shared/Cocoa/WKObject.mm
    Shared/Cocoa/WebErrorsCocoa.mm
    Shared/Cocoa/WebKit2InitializeCocoa.mm

    Shared/Plugins/Netscape/mac/NetscapePluginModuleMac.mm
    Shared/Plugins/Netscape/mac/PluginInformationMac.mm

    Shared/Plugins/mac/PluginSandboxProfile.mm

    Shared/RemoteLayerTree/RemoteLayerBackingStore.mm
    Shared/RemoteLayerTree/RemoteLayerBackingStoreCollection.mm
    Shared/RemoteLayerTree/RemoteLayerTreePropertyApplier.mm
    Shared/RemoteLayerTree/RemoteLayerTreeTransaction.mm
    Shared/RemoteLayerTree/RemoteScrollingCoordinatorTransaction.cpp

    Shared/cf/ArgumentCodersCF.cpp
    Shared/cf/CookieStorageUtilsCF.mm

    Shared/cg/ShareableBitmapCG.cpp

    Shared/mac/AttributedString.mm
    Shared/mac/AuxiliaryProcessMac.mm
    Shared/mac/CodeSigning.mm
    Shared/mac/ColorSpaceData.mm
    Shared/mac/HangDetectionDisablerMac.mm
    Shared/mac/NativeWebGestureEventMac.mm
    Shared/mac/NativeWebKeyboardEventMac.mm
    Shared/mac/NativeWebMouseEventMac.mm
    Shared/mac/NativeWebWheelEventMac.mm
    Shared/mac/ObjCObjectGraph.mm
    Shared/mac/PDFKitImports.mm
    Shared/mac/PasteboardTypes.mm
    Shared/mac/PrintInfoMac.mm
    Shared/mac/SecItemRequestData.cpp
    Shared/mac/SecItemResponseData.cpp
    Shared/mac/SecItemShim.cpp
    Shared/mac/WebCoreArgumentCodersMac.mm
    Shared/mac/WebEventFactory.mm
    Shared/mac/WebGestureEvent.cpp
    Shared/mac/WebHitTestResultData.mm
    Shared/mac/WebMemorySampler.mac.mm

    UIProcess/HighPerformanceGraphicsUsageSampler.cpp
    UIProcess/PerActivityStateCPUUsageSampler.cpp
    UIProcess/ViewGestureController.cpp
    UIProcess/ViewSnapshotStore.cpp

    UIProcess/Automation/WebAutomationSession.cpp

    UIProcess/Automation/cocoa/WebAutomationSessionCocoa.mm

    UIProcess/Automation/mac/WebAutomationSessionMac.mm

    UIProcess/API/APIUserScript.cpp
    UIProcess/API/APIUserStyleSheet.cpp
    UIProcess/API/APIWebsiteDataRecord.cpp

    UIProcess/API/C/mac/WKContextPrivateMac.mm
    UIProcess/API/C/mac/WKPagePrivateMac.mm

    UIProcess/API/Cocoa/APIContentRuleListStoreCocoa.mm
    UIProcess/API/Cocoa/APISerializedScriptValueCocoa.mm
    UIProcess/API/Cocoa/APIWebsiteDataStoreCocoa.mm
    UIProcess/API/Cocoa/LegacyBundleForClass.mm
    UIProcess/API/Cocoa/WKBackForwardList.mm
    UIProcess/API/Cocoa/WKBackForwardListItem.mm
    UIProcess/API/Cocoa/WKBrowsingContextController.mm
    UIProcess/API/Cocoa/WKBrowsingContextGroup.mm
    UIProcess/API/Cocoa/WKConnection.mm
    UIProcess/API/Cocoa/WKContentRuleList.mm
    UIProcess/API/Cocoa/WKContentRuleListStore.mm
    UIProcess/API/Cocoa/WKError.mm
    UIProcess/API/Cocoa/WKFrameInfo.mm
    UIProcess/API/Cocoa/WKHTTPCookieStore.mm
    UIProcess/API/Cocoa/WKMenuItemIdentifiers.mm
    UIProcess/API/Cocoa/WKNSURLAuthenticationChallenge.mm
    UIProcess/API/Cocoa/WKNavigation.mm
    UIProcess/API/Cocoa/WKNavigationAction.mm
    UIProcess/API/Cocoa/WKNavigationData.mm
    UIProcess/API/Cocoa/WKNavigationResponse.mm
    UIProcess/API/Cocoa/WKOpenPanelParameters.mm
    UIProcess/API/Cocoa/WKPreferences.mm
    UIProcess/API/Cocoa/WKPreviewActionItem.mm
    UIProcess/API/Cocoa/WKPreviewActionItemIdentifiers.mm
    UIProcess/API/Cocoa/WKPreviewElementInfo.mm
    UIProcess/API/Cocoa/WKProcessGroup.mm
    UIProcess/API/Cocoa/WKProcessPool.mm
    UIProcess/API/Cocoa/WKScriptMessage.mm
    UIProcess/API/Cocoa/WKSecurityOrigin.mm
    UIProcess/API/Cocoa/WKTypeRefWrapper.mm
    UIProcess/API/Cocoa/WKURLSchemeTask.mm
    UIProcess/API/Cocoa/WKUserContentController.mm
    UIProcess/API/Cocoa/WKUserScript.mm
    UIProcess/API/Cocoa/WKWebView.mm
    UIProcess/API/Cocoa/WKWebViewConfiguration.mm
    UIProcess/API/Cocoa/WKWebsiteDataRecord.mm
    UIProcess/API/Cocoa/WKWebsiteDataStore.mm
    UIProcess/API/Cocoa/WKWindowFeatures.mm
    UIProcess/API/Cocoa/_WKActivatedElementInfo.mm
    UIProcess/API/Cocoa/_WKApplicationManifest.mm
    UIProcess/API/Cocoa/_WKAttachment.mm
    UIProcess/API/Cocoa/_WKAutomationSession.mm
    UIProcess/API/Cocoa/_WKAutomationSessionConfiguration.mm
    UIProcess/API/Cocoa/_WKContextMenuElementInfo.mm
    UIProcess/API/Cocoa/_WKDownload.mm
    UIProcess/API/Cocoa/_WKElementAction.mm
    UIProcess/API/Cocoa/_WKErrorRecoveryAttempting.mm
    UIProcess/API/Cocoa/_WKExperimentalFeature.mm
    UIProcess/API/Cocoa/_WKLinkIconParameters.mm
    UIProcess/API/Cocoa/_WKProcessPoolConfiguration.mm
    UIProcess/API/Cocoa/_WKRemoteWebInspectorViewController.mm
    UIProcess/API/Cocoa/_WKSessionState.mm
    UIProcess/API/Cocoa/_WKThumbnailView.mm
    UIProcess/API/Cocoa/_WKUserContentExtensionStore.mm
    UIProcess/API/Cocoa/_WKUserContentFilter.mm
    UIProcess/API/Cocoa/_WKUserContentWorld.mm
    UIProcess/API/Cocoa/_WKUserInitiatedAction.mm
    UIProcess/API/Cocoa/_WKUserStyleSheet.mm
    UIProcess/API/Cocoa/_WKVisitedLinkStore.mm
    UIProcess/API/Cocoa/_WKWebsiteDataSize.mm
    UIProcess/API/Cocoa/_WKWebsiteDataStore.mm
    UIProcess/API/Cocoa/_WKWebsitePolicies.mm

    UIProcess/API/mac/WKView.mm

    UIProcess/Cocoa/AutomationClient.mm
    UIProcess/Cocoa/AutomationSessionClient.mm
    UIProcess/Cocoa/DiagnosticLoggingClient.mm
    UIProcess/Cocoa/DownloadClient.mm
    UIProcess/Cocoa/FindClient.mm
    UIProcess/Cocoa/FullscreenClient.mm
    UIProcess/Cocoa/IconLoadingDelegate.mm
    UIProcess/Cocoa/LegacyCustomProtocolManagerClient.mm
    UIProcess/Cocoa/NavigationState.mm
    UIProcess/Cocoa/PageClientImplCocoa.mm
    UIProcess/Cocoa/SessionStateCoding.mm
    UIProcess/Cocoa/UIDelegate.mm
    UIProcess/Cocoa/UIRemoteObjectRegistry.cpp
    UIProcess/Cocoa/VersionChecks.mm
    UIProcess/Cocoa/WKFullKeyboardAccessWatcher.mm
    UIProcess/Cocoa/WKReloadFrameErrorRecoveryAttempter.mm
    UIProcess/Cocoa/WKWebViewContentProviderRegistry.mm
    UIProcess/Cocoa/WebPageProxyCocoa.mm
    UIProcess/Cocoa/WebPasteboardProxyCocoa.mm
    UIProcess/Cocoa/WebProcessPoolCocoa.mm
    UIProcess/Cocoa/WebProcessProxyCocoa.mm
    UIProcess/Cocoa/WebURLSchemeHandlerCocoa.mm
    UIProcess/Cocoa/WebViewImpl.mm

    UIProcess/Launcher/mac/ProcessLauncherMac.mm

    UIProcess/Network/CustomProtocols/LegacyCustomProtocolManagerProxy.cpp

    UIProcess/Plugins/mac/PluginInfoStoreMac.mm
    UIProcess/Plugins/mac/PluginProcessManagerMac.mm
    UIProcess/Plugins/mac/PluginProcessProxyMac.mm

    UIProcess/RemoteLayerTree/RemoteLayerTreeScrollingPerformanceData.mm
    UIProcess/RemoteLayerTree/RemoteScrollingCoordinatorProxy.cpp
    UIProcess/RemoteLayerTree/RemoteScrollingTree.cpp
    UIProcess/RemoteLayerTree/RemoteLayerTreeDrawingAreaProxy.mm
    UIProcess/RemoteLayerTree/RemoteLayerTreeHost.mm

    UIProcess/WebsiteData/Cocoa/WebsiteDataStoreCocoa.mm

    UIProcess/mac/CorrectionPanel.mm
    UIProcess/mac/LegacySessionStateCoding.cpp
    UIProcess/mac/PageClientImplMac.mm
    UIProcess/mac/RemoteWebInspectorProxyMac.mm
    UIProcess/mac/SecItemShimProxy.cpp
    UIProcess/mac/ServicesController.mm
    UIProcess/mac/TextCheckerMac.mm
    UIProcess/mac/TiledCoreAnimationDrawingAreaProxy.mm
    UIProcess/mac/ViewGestureControllerMac.mm
    UIProcess/mac/ViewSnapshotStoreMac.mm
    UIProcess/mac/WKFullScreenWindowController.mm
    UIProcess/mac/WKImmediateActionController.mm
    UIProcess/mac/WKInspectorViewController.mm
    UIProcess/mac/WKInspectorWKWebView.mm
    UIProcess/mac/WKPrintingView.mm
    UIProcess/mac/WKSharingServicePickerDelegate.mm
    UIProcess/mac/WKTextFinderClient.mm
    UIProcess/mac/WKTextInputWindowController.mm
    UIProcess/mac/WKViewLayoutStrategy.mm
    UIProcess/mac/WebColorPickerMac.mm
    UIProcess/mac/WebContextMenuProxyMac.mm
    UIProcess/mac/WebCookieManagerProxyMac.mm
    UIProcess/mac/WebInspectorProxyMac.mm
    UIProcess/mac/WebPageProxyMac.mm
    UIProcess/mac/WebPopupMenuProxyMac.mm
    UIProcess/mac/WebPreferencesMac.mm
    UIProcess/mac/WebProcessProxyMac.mm
    UIProcess/mac/WindowServerConnection.mm

    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessBundleParameters.mm
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInFrame.mm
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInHitTestResult.mm
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInNodeHandle.mm
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInPageGroup.mm
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInRangeHandle.mm
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInScriptWorld.mm

    WebProcess/InjectedBundle/API/mac/WKDOMDocument.mm
    WebProcess/InjectedBundle/API/mac/WKDOMElement.mm
    WebProcess/InjectedBundle/API/mac/WKDOMInternals.mm
    WebProcess/InjectedBundle/API/mac/WKDOMNode.mm
    WebProcess/InjectedBundle/API/mac/WKDOMRange.mm
    WebProcess/InjectedBundle/API/mac/WKDOMText.mm
    WebProcess/InjectedBundle/API/mac/WKDOMTextIterator.mm
    WebProcess/InjectedBundle/API/mac/WKWebProcessPlugIn.mm
    WebProcess/InjectedBundle/API/mac/WKWebProcessPlugInBrowserContextController.mm

    WebProcess/InjectedBundle/mac/InjectedBundleMac.mm

    WebProcess/MediaCache/WebMediaKeyStorageManager.cpp

    WebProcess/Plugins/Netscape/mac/NetscapePluginMac.mm
    WebProcess/Plugins/Netscape/mac/PluginProxyMac.mm

    WebProcess/Plugins/PDF/PDFPlugin.mm
    WebProcess/Plugins/PDF/PDFPluginAnnotation.mm
    WebProcess/Plugins/PDF/PDFPluginChoiceAnnotation.mm
    WebProcess/Plugins/PDF/PDFPluginPasswordField.mm
    WebProcess/Plugins/PDF/PDFPluginTextAnnotation.mm

    WebProcess/WebPage/RemoteLayerTree/GraphicsLayerCARemote.cpp
    WebProcess/WebPage/RemoteLayerTree/PlatformCAAnimationRemote.mm
    WebProcess/WebPage/RemoteLayerTree/PlatformCALayerRemote.cpp
    WebProcess/WebPage/RemoteLayerTree/PlatformCALayerRemoteCustom.mm
    WebProcess/WebPage/RemoteLayerTree/PlatformCALayerRemoteTiledBacking.cpp
    WebProcess/WebPage/RemoteLayerTree/RemoteLayerTreeContext.mm
    WebProcess/WebPage/RemoteLayerTree/RemoteLayerTreeDisplayRefreshMonitor.mm
    WebProcess/WebPage/RemoteLayerTree/RemoteLayerTreeDrawingArea.mm
    WebProcess/WebPage/RemoteLayerTree/RemoteScrollingCoordinator.mm

    WebProcess/WebCoreSupport/WebPasteboardOverrides.cpp
    WebProcess/WebCoreSupport/WebValidationMessageClient.cpp

    WebProcess/WebCoreSupport/mac/WebAlternativeTextClient.cpp
    WebProcess/WebCoreSupport/mac/WebContextMenuClientMac.mm
    WebProcess/WebCoreSupport/mac/WebDragClientMac.mm
    WebProcess/WebCoreSupport/mac/WebEditorClientMac.mm
    WebProcess/WebCoreSupport/mac/WebFrameNetworkingContext.mm
    WebProcess/WebCoreSupport/mac/WebPopupMenuMac.mm

    WebProcess/WebPage/ViewGestureGeometryCollector.cpp

    WebProcess/WebPage/Cocoa/WebPageCocoa.mm
    WebProcess/WebPage/Cocoa/WebRemoteObjectRegistry.cpp

    WebProcess/WebPage/mac/PageBannerMac.mm
    WebProcess/WebPage/mac/TiledCoreAnimationDrawingArea.mm
    WebProcess/WebPage/mac/WKAccessibilityWebPageObjectBase.mm
    WebProcess/WebPage/mac/WKAccessibilityWebPageObjectMac.mm
    WebProcess/WebPage/mac/WebInspectorUIMac.mm
    WebProcess/WebPage/mac/WebPageMac.mm

    WebProcess/cocoa/WebProcessCocoa.mm

    WebProcess/mac/SecItemShimLibrary.mm
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/NetworkProcess/cocoa"
    "${WEBKIT_DIR}/NetworkProcess/mac"
    "${WEBKIT_DIR}/PluginProcess/mac"
    "${WEBKIT_DIR}/UIProcess/mac"
    "${WEBKIT_DIR}/UIProcess/API/C/mac"
    "${WEBKIT_DIR}/UIProcess/API/Cocoa"
    "${WEBKIT_DIR}/UIProcess/API/mac"
    "${WEBKIT_DIR}/UIProcess/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Launcher/mac"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/ios"
    "${WEBKIT_DIR}/UIProcess/ios"
    "${WEBKIT_DIR}/Platform/cg"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/Platform/classifier/cocoa"
    "${WEBKIT_DIR}/Platform/cocoa"
    "${WEBKIT_DIR}/Platform/mac"
    "${WEBKIT_DIR}/Platform/unix"
    "${WEBKIT_DIR}/Platform/spi/Cocoa"
    "${WEBKIT_DIR}/Platform/spi/mac"
    "${WEBKIT_DIR}/Platform/IPC/mac"
    "${WEBKIT_DIR}/Platform/spi/Cocoa"
    "${WEBKIT_DIR}/Shared/API/Cocoa"
    "${WEBKIT_DIR}/Shared/API/c/cf"
    "${WEBKIT_DIR}/Shared/API/c/cg"
    "${WEBKIT_DIR}/Shared/API/c/mac"
    "${WEBKIT_DIR}/Shared/cf"
    "${WEBKIT_DIR}/Shared/Cocoa"
    "${WEBKIT_DIR}/Shared/EntryPointUtilities/mac/XPCService"
    "${WEBKIT_DIR}/Shared/mac"
    "${WEBKIT_DIR}/Shared/Plugins/mac"
    "${WEBKIT_DIR}/Shared/Scrolling"
    "${WEBKIT_DIR}/WebProcess/cocoa"
    "${WEBKIT_DIR}/WebProcess/mac"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/Cocoa"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/mac"
    "${WEBKIT_DIR}/WebProcess/Plugins/PDF"
    "${WEBKIT_DIR}/WebProcess/Plugins/Netscape/mac"
    "${WEBKIT_DIR}/WebProcess/WebPage/Cocoa"
    "${WEBKIT_DIR}/WebProcess/WebPage/RemoteLayerTree"
    "${WEBKIT_DIR}/WebProcess/WebPage/mac"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/mac"
    "${FORWARDING_HEADERS_DIR}/WebCore"
)

set(XPCService_SOURCES
    Shared/EntryPointUtilities/Cocoa/XPCService/XPCServiceEntryPoint.mm
    Shared/EntryPointUtilities/Cocoa/XPCService/XPCServiceMain.mm
)

set(WebProcess_SOURCES
    WebProcess/EntryPoint/Cocoa/XPCService/WebContentServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

set(PluginProcess_SOURCES
    PluginProcess/EntryPoint/Cocoa/XPCService/PluginServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/Cocoa/XPCService/NetworkServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

# FIXME: These should not have Development in production builds.
set(WebKit_WebProcess_OUTPUT_NAME com.apple.WebKit.WebContent.Development)
set(WebKit_NetworkProcess_OUTPUT_NAME com.apple.WebKit.Networking.Development)

add_definitions("-include WebKit2Prefix.h")

set(WebKit_FORWARDING_HEADERS_FILES
    Platform/cocoa/WKCrashReporter.h

    Shared/API/c/WKDiagnosticLoggingResultType.h

    UIProcess/API/C/WKPageDiagnosticLoggingClient.h
    UIProcess/API/C/WKPageNavigationClient.h
    UIProcess/API/C/WKPageRenderingProgressEvents.h
)

list(APPEND WebKit_MESSAGES_IN_FILES
    NetworkProcess/CustomProtocols/LegacyCustomProtocolManager.messages.in

    Shared/API/Cocoa/RemoteObjectRegistry.messages.in

    UIProcess/ViewGestureController.messages.in

    UIProcess/Cocoa/VideoFullscreenManagerProxy.messages.in

    UIProcess/Network/CustomProtocols/LegacyCustomProtocolManagerProxy.messages.in

    UIProcess/RemoteLayerTree/RemoteLayerTreeDrawingAreaProxy.messages.in

    UIProcess/mac/SecItemShimProxy.messages.in

    WebProcess/WebPage/RemoteLayerTree/RemoteScrollingCoordinator.messages.in
    WebProcess/WebPage/ViewGestureGeometryCollector.messages.in
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

    UIProcess
    UIProcess/Cocoa

    UIProcess/API/C

    UIProcess/API/C/Cocoa
    UIProcess/API/C/mac
    UIProcess/API/cpp

    WebProcess/WebPage

    WebProcess/InjectedBundle/API/Cocoa
    WebProcess/InjectedBundle/API/c
    WebProcess/InjectedBundle/API/mac
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebKit FILES ${WebKit_FORWARDING_HEADERS_FILES} DIRECTORIES ${WebKit_FORWARDING_HEADERS_DIRECTORIES})

# This is needed right now to import ObjC headers instead of including them.
# FIXME: Forwarding headers should be copies of actual headers.
file(GLOB ObjCHeaders UIProcess/API/Cocoa/*.h)
foreach (_file ${ObjCHeaders})
    get_filename_component(_name ${_file} NAME)
    if (NOT EXISTS ${FORWARDING_HEADERS_DIR}/WebKit/${_name})
        file(WRITE ${FORWARDING_HEADERS_DIR}/WebKit/${_name} "#import <WebKit/UIProcess/API/Cocoa/${_name}>")
    endif ()
endforeach ()

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
foreach (_file ${WebKitLegacyForwardingHeaders})
    file(WRITE ${FORWARDING_HEADERS_DIR}/WebKit/${_file} "#import <WebKitLegacy/${_file}>")
endforeach ()

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
foreach (_file ${ObjCForwardingHeaders})
    file(WRITE ${FORWARDING_HEADERS_DIR}/WebKit/${_file} "#import <WebKitLegacy/${_file}>")
endforeach ()

list(APPEND WebKit_AUTOMATION_PROTOCOL_GENERATOR_EXTRA_FLAGS
    --platform=macOS
)

# FIXME: These should not be necessary.
file(WRITE ${FORWARDING_HEADERS_DIR}/WebKit/WKImageCG.h "#import <WebKit/Shared/API/c/cg/WKImageCG.h>")

set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION}")

set(WebKit_OUTPUT_NAME WebKit)

# XPC Services

function(WEBKIT_DEFINE_XPC_SERVICES)
    set(WebKit_XPC_SERVICE_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/XPCServices)
    WEBKIT_CREATE_SYMLINK(WebProcess ${WebKit_XPC_SERVICE_DIR} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices)

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
        ${WebKit_WebProcess_OUTPUT_NAME})

    WEBKIT_XPC_SERVICE(NetworkProcess
        "com.apple.WebKit.Networking"
        ${WEBKIT_DIR}/NetworkProcess/EntryPoint/Cocoa/XPCService/NetworkService/Info-OSX.plist
        ${WebKit_NetworkProcess_OUTPUT_NAME})

    add_custom_target(WebContentProcessNib COMMAND
        ibtool --compile ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib ${WEBKIT_DIR}/Resources/WebContentProcess.xib
        VERBATIM)
    add_dependencies(WebKit WebContentProcessNib)
endfunction()
