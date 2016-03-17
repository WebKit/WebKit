add_definitions("-ObjC++ -std=c++11")
link_directories(../../WebKitLibraries)
find_library(CARBON_LIBRARY Carbon)
find_library(QUARTZ_LIBRARY Quartz)
add_definitions(-iframework ${QUARTZ_LIBRARY}/Frameworks)
add_definitions(-iframework ${CARBON_LIBRARY}/Frameworks)
add_definitions(-DWK_XPC_SERVICE_SUFFIX=".Development")

list(APPEND WebKit2_LIBRARIES
    WebKit
)

list(APPEND WebKit2_SOURCES
    DatabaseProcess/mac/DatabaseProcessMac.mm

    NetworkProcess/CustomProtocols/Cocoa/CustomProtocolManagerCocoa.mm

    NetworkProcess/Downloads/mac/DownloadMac.mm

    NetworkProcess/cache/NetworkCacheDataCocoa.mm
    NetworkProcess/cache/NetworkCacheIOChannelCocoa.mm

    NetworkProcess/cocoa/NetworkProcessCocoa.mm
    NetworkProcess/cocoa/NetworkSessionCocoa.mm

    NetworkProcess/mac/NetworkLoadMac.mm
    NetworkProcess/mac/NetworkProcessMac.mm
    NetworkProcess/mac/RemoteNetworkingContext.mm

    Platform/IPC/MessageRecorder.cpp

    Platform/IPC/mac/ConnectionMac.mm

    Platform/cf/ModuleCF.cpp

    Platform/cg/CGUtilities.cpp

    Platform/foundation/LoggingFoundation.mm

    Platform/mac/LayerHostingContext.mm
    Platform/mac/MachUtilities.cpp
    Platform/mac/MenuUtilities.mm
    Platform/mac/SharedMemoryMac.cpp
    Platform/mac/StringUtilities.mm

    Platform/unix/EnvironmentUtilities.cpp

    PluginProcess/mac/PluginControllerProxyMac.mm
    PluginProcess/mac/PluginProcessMac.mm
    PluginProcess/mac/PluginProcessShim.mm

    Shared/APIWebArchive.mm
    Shared/APIWebArchiveResource.mm

    Shared/Authentication/cocoa/AuthenticationManagerCocoa.mm

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

    Shared/Authentication/mac/AuthenticationManager.mac.mm

    Shared/Cocoa/APIDataCocoa.mm
    Shared/Cocoa/APIObject.mm
    Shared/Cocoa/CompletionHandlerCallChecker.mm
    Shared/Cocoa/DataDetectionResult.mm
    Shared/Cocoa/WKNSArray.mm
    Shared/Cocoa/WKNSData.mm
    Shared/Cocoa/WKNSDictionary.mm
    Shared/Cocoa/WKNSError.mm
    Shared/Cocoa/WKNSString.mm
    Shared/Cocoa/WKNSURL.mm
    Shared/Cocoa/WKNSURLExtras.mm
    Shared/Cocoa/WKNSURLRequest.mm
    Shared/Cocoa/WKObject.mm

    Shared/Plugins/Netscape/mac/NetscapePluginModuleMac.mm
    Shared/Plugins/Netscape/mac/PluginInformationMac.mm

    Shared/Plugins/mac/PluginSandboxProfile.mm

    Shared/Scrolling/RemoteScrollingCoordinatorTransaction.cpp

    Shared/cf/ArgumentCodersCF.cpp

    Shared/cg/ShareableBitmapCG.cpp

    Shared/mac/ArgumentCodersMac.mm
    Shared/mac/AttributedString.mm
    Shared/mac/ChildProcessMac.mm
    Shared/mac/ColorSpaceData.mm
    Shared/mac/CookieStorageShim.mm
    Shared/mac/CookieStorageShimLibrary.cpp
    Shared/mac/HangDetectionDisablerMac.mm
    Shared/mac/NativeWebGestureEventMac.mm
    Shared/mac/NativeWebKeyboardEventMac.mm
    Shared/mac/NativeWebMouseEventMac.mm
    Shared/mac/NativeWebWheelEventMac.mm
    Shared/mac/ObjCObjectGraph.mm
    Shared/mac/PDFKitImports.mm
    Shared/mac/PasteboardTypes.mm
    Shared/mac/PrintInfoMac.mm
    Shared/mac/RemoteLayerBackingStore.mm
    Shared/mac/RemoteLayerBackingStoreCollection.mm
    Shared/mac/RemoteLayerTreePropertyApplier.mm
    Shared/mac/RemoteLayerTreeTransaction.mm
    Shared/mac/SandboxExtensionMac.mm
    Shared/mac/SandboxInitialiationParametersMac.mm
    Shared/mac/SandboxUtilities.mm
    Shared/mac/SecItemRequestData.cpp
    Shared/mac/SecItemResponseData.cpp
    Shared/mac/SecItemShim.cpp
    Shared/mac/WebCoreArgumentCodersMac.mm
    Shared/mac/WebEventFactory.mm
    Shared/mac/WebGestureEvent.cpp
    Shared/mac/WebHitTestResultData.mm
    Shared/mac/WebMemorySampler.mac.mm

    UIProcess/ViewGestureController.cpp
    UIProcess/WebResourceLoadStatisticsStore.cpp

    UIProcess/Automation/WebAutomationSession.cpp

    UIProcess/API/APIUserScript.cpp
    UIProcess/API/APIUserStyleSheet.cpp
    UIProcess/API/APIWebsiteDataRecord.cpp

    UIProcess/API/Cocoa/APISerializedScriptValueCocoa.mm
    UIProcess/API/Cocoa/APIUserContentExtensionStoreCocoa.mm
    UIProcess/API/Cocoa/APIWebsiteDataStoreCocoa.mm
    UIProcess/API/Cocoa/LegacyBundleForClass.mm
    UIProcess/API/Cocoa/WKBackForwardList.mm
    UIProcess/API/Cocoa/WKBackForwardListItem.mm
    UIProcess/API/Cocoa/WKBrowsingContextController.mm
    UIProcess/API/Cocoa/WKBrowsingContextGroup.mm
    UIProcess/API/Cocoa/WKConnection.mm
    UIProcess/API/Cocoa/WKElementInfo.mm
    UIProcess/API/Cocoa/WKError.mm
    UIProcess/API/Cocoa/WKFrameInfo.mm
    UIProcess/API/Cocoa/WKMenuItemIdentifiers.mm
    UIProcess/API/Cocoa/WKNSURLAuthenticationChallenge.mm
    UIProcess/API/Cocoa/WKNavigation.mm
    UIProcess/API/Cocoa/WKNavigationAction.mm
    UIProcess/API/Cocoa/WKNavigationData.mm
    UIProcess/API/Cocoa/WKNavigationResponse.mm
    UIProcess/API/Cocoa/WKPreferences.mm
    UIProcess/API/Cocoa/WKPreviewActionItem.mm
    UIProcess/API/Cocoa/WKPreviewActionItemIdentifiers.mm
    UIProcess/API/Cocoa/WKPreviewElementInfo.mm
    UIProcess/API/Cocoa/WKProcessGroup.mm
    UIProcess/API/Cocoa/WKProcessPool.mm
    UIProcess/API/Cocoa/WKScriptMessage.mm
    UIProcess/API/Cocoa/WKSecurityOrigin.mm
    UIProcess/API/Cocoa/WKTypeRefWrapper.mm
    UIProcess/API/Cocoa/WKUserContentController.mm
    UIProcess/API/Cocoa/WKUserScript.mm
    UIProcess/API/Cocoa/WKWebView.mm
    UIProcess/API/Cocoa/WKWebViewConfiguration.mm
    UIProcess/API/Cocoa/WKWebsiteDataRecord.mm
    UIProcess/API/Cocoa/WKWebsiteDataStore.mm
    UIProcess/API/Cocoa/WKWindowFeatures.mm
    UIProcess/API/Cocoa/_WKActivatedElementInfo.mm
    UIProcess/API/Cocoa/_WKAutomationSession.mm
    UIProcess/API/Cocoa/_WKContextMenuElementInfo.mm
    UIProcess/API/Cocoa/_WKDownload.mm
    UIProcess/API/Cocoa/_WKElementAction.mm
    UIProcess/API/Cocoa/_WKErrorRecoveryAttempting.mm
    UIProcess/API/Cocoa/_WKProcessPoolConfiguration.mm
    UIProcess/API/Cocoa/_WKSessionState.mm
    UIProcess/API/Cocoa/_WKThumbnailView.mm
    UIProcess/API/Cocoa/_WKUserContentExtensionStore.mm
    UIProcess/API/Cocoa/_WKUserContentFilter.mm
    UIProcess/API/Cocoa/_WKUserStyleSheet.mm
    UIProcess/API/Cocoa/_WKVisitedLinkProvider.mm
    UIProcess/API/Cocoa/_WKVisitedLinkStore.mm
    UIProcess/API/Cocoa/_WKWebsiteDataStore.mm

    UIProcess/API/mac/WKView.mm

    UIProcess/Cocoa/AutomationClient.mm
    UIProcess/Cocoa/AutomationSessionClient.mm
    UIProcess/Cocoa/DiagnosticLoggingClient.mm
    UIProcess/Cocoa/DownloadClient.mm
    UIProcess/Cocoa/FindClient.mm
    UIProcess/Cocoa/NavigationState.mm
    UIProcess/Cocoa/RemoteLayerTreeScrollingPerformanceData.mm
    UIProcess/Cocoa/SessionStateCoding.mm
    UIProcess/Cocoa/UIDelegate.mm
    UIProcess/Cocoa/VersionChecks.mm
    UIProcess/Cocoa/WKReloadFrameErrorRecoveryAttempter.mm
    UIProcess/Cocoa/WKWebViewContentProviderRegistry.mm
    UIProcess/Cocoa/WebPageProxyCocoa.mm
    UIProcess/Cocoa/WebPasteboardProxyCocoa.mm
    UIProcess/Cocoa/WebProcessPoolCocoa.mm
    UIProcess/Cocoa/WebProcessProxyCocoa.mm
    UIProcess/Cocoa/WebViewImpl.mm

    UIProcess/Launcher/mac/ProcessLauncherMac.mm

    UIProcess/Network/CustomProtocols/mac/CustomProtocolManagerProxyMac.mm

    UIProcess/Network/mac/NetworkProcessProxyMac.mm

    UIProcess/Plugins/mac/PluginInfoStoreMac.mm
    UIProcess/Plugins/mac/PluginProcessManagerMac.mm
    UIProcess/Plugins/mac/PluginProcessProxyMac.mm

    UIProcess/Scrolling/RemoteScrollingCoordinatorProxy.cpp
    UIProcess/Scrolling/RemoteScrollingTree.cpp

    UIProcess/Storage/StorageManager.cpp

    UIProcess/WebsiteData/Cocoa/WebsiteDataStoreCocoa.mm

    UIProcess/mac/CorrectionPanel.mm
    UIProcess/mac/LegacySessionStateCoding.cpp
    UIProcess/mac/PageClientImpl.mm
    UIProcess/mac/RemoteLayerTreeDrawingAreaProxy.mm
    UIProcess/mac/RemoteLayerTreeHost.mm
    UIProcess/mac/SecItemShimProxy.cpp
    UIProcess/mac/ServicesController.mm
    UIProcess/mac/TextCheckerMac.mm
    UIProcess/mac/TiledCoreAnimationDrawingAreaProxy.mm
    UIProcess/mac/ViewGestureControllerMac.mm
    UIProcess/mac/ViewSnapshotStore.mm
    UIProcess/mac/WKFullKeyboardAccessWatcher.mm
    UIProcess/mac/WKFullScreenWindowController.mm
    UIProcess/mac/WKImmediateActionController.mm
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

    WebProcess/Cookies/mac/WebCookieManagerMac.mm

    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessBundleParameters.mm
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInFrame.mm
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInHitTestResult.mm
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInNodeHandle.mm
    WebProcess/InjectedBundle/API/Cocoa/WKWebProcessPlugInPageGroup.mm
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

    WebProcess/Plugins/PDF/DeprecatedPDFPlugin.mm
    WebProcess/Plugins/PDF/PDFPlugin.mm
    WebProcess/Plugins/PDF/PDFPluginAnnotation.mm
    WebProcess/Plugins/PDF/PDFPluginChoiceAnnotation.mm
    WebProcess/Plugins/PDF/PDFPluginPasswordField.mm
    WebProcess/Plugins/PDF/PDFPluginTextAnnotation.mm

    WebProcess/Scrolling/RemoteScrollingCoordinator.mm

    WebProcess/WebCoreSupport/WebPasteboardOverrides.cpp

    WebProcess/WebCoreSupport/mac/WebAlternativeTextClient.cpp
    WebProcess/WebCoreSupport/mac/WebContextMenuClientMac.mm
    WebProcess/WebCoreSupport/mac/WebDragClientMac.mm
    WebProcess/WebCoreSupport/mac/WebEditorClientMac.mm
    WebProcess/WebCoreSupport/mac/WebErrorsMac.mm
    WebProcess/WebCoreSupport/mac/WebFrameNetworkingContext.mm
    WebProcess/WebCoreSupport/mac/WebPopupMenuMac.mm
    WebProcess/WebCoreSupport/mac/WebSystemInterface.mm

    WebProcess/WebPage/ViewGestureGeometryCollector.cpp

    WebProcess/WebPage/Cocoa/RemoteLayerTreeDisplayRefreshMonitor.mm

    WebProcess/WebPage/mac/GraphicsLayerCARemote.cpp
    WebProcess/WebPage/mac/PageBannerMac.mm
    WebProcess/WebPage/mac/PlatformCAAnimationRemote.mm
    WebProcess/WebPage/mac/PlatformCALayerRemote.cpp
    WebProcess/WebPage/mac/PlatformCALayerRemoteCustom.mm
    WebProcess/WebPage/mac/PlatformCALayerRemoteTiledBacking.cpp
    WebProcess/WebPage/mac/RemoteLayerTreeContext.mm
    WebProcess/WebPage/mac/RemoteLayerTreeDrawingArea.mm
    WebProcess/WebPage/mac/TiledCoreAnimationDrawingArea.mm
    WebProcess/WebPage/mac/WKAccessibilityWebPageObjectBase.mm
    WebProcess/WebPage/mac/WKAccessibilityWebPageObjectMac.mm
    WebProcess/WebPage/mac/WebInspectorUIMac.mm
    WebProcess/WebPage/mac/WebPageMac.mm

    WebProcess/cocoa/WebProcessCocoa.mm

    WebProcess/mac/SecItemShimLibrary.mm
)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT2_DIR})

list(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/icu"
    "${WEBCORE_DIR}/editing/cocoa"
    "${WEBCORE_DIR}/editing/mac"
    "${WEBCORE_DIR}/platform/cf"
    "${WEBCORE_DIR}/platform/cocoa"
    "${WEBCORE_DIR}/platform/graphics/cocoa"
    "${WEBCORE_DIR}/platform/mac"
    "${WEBCORE_DIR}/platform/network/cf"
    "${WEBCORE_DIR}/platform/network/cocoa"
    "${WEBCORE_DIR}/platform/spi/cocoa"
    "${WEBCORE_DIR}/platform/spi/mac"
    "${WEBCORE_DIR}/platform/graphics/ca"
    "${WEBCORE_DIR}/platform/graphics/cg"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBKIT2_DIR}/NetworkProcess/cocoa"
    "${WEBKIT2_DIR}/NetworkProcess/mac"
    "${WEBKIT2_DIR}/PluginProcess/mac"
    "${WEBKIT2_DIR}/UIProcess/mac"
    "${WEBKIT2_DIR}/UIProcess/API/C/mac"
    "${WEBKIT2_DIR}/UIProcess/API/Cocoa"
    "${WEBKIT2_DIR}/UIProcess/API/mac"
    "${WEBKIT2_DIR}/UIProcess/Cocoa"
    "${WEBKIT2_DIR}/UIProcess/Launcher/mac"
    "${WEBKIT2_DIR}/UIProcess/Scrolling"
    "${WEBKIT2_DIR}/Platform/cg"
    "${WEBKIT2_DIR}/Platform/mac"
    "${WEBKIT2_DIR}/Platform/unix"
    "${WEBKIT2_DIR}/Platform/spi/Cocoa"
    "${WEBKIT2_DIR}/Platform/spi/mac"
    "${WEBKIT2_DIR}/Platform/IPC/mac"
    "${WEBKIT2_DIR}/Platform/spi/Cocoa"
    "${WEBKIT2_DIR}/Shared/API/Cocoa"
    "${WEBKIT2_DIR}/Shared/API/c/cf"
    "${WEBKIT2_DIR}/Shared/API/c/cg"
    "${WEBKIT2_DIR}/Shared/API/c/mac"
    "${WEBKIT2_DIR}/Shared/cf"
    "${WEBKIT2_DIR}/Shared/Cocoa"
    "${WEBKIT2_DIR}/Shared/EntryPointUtilities/mac/XPCService"
    "${WEBKIT2_DIR}/Shared/mac"
    "${WEBKIT2_DIR}/Shared/Plugins/mac"
    "${WEBKIT2_DIR}/Shared/Scrolling"
    "${WEBKIT2_DIR}/WebProcess/cocoa"
    "${WEBKIT2_DIR}/WebProcess/mac"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/Cocoa"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/mac"
    "${WEBKIT2_DIR}/WebProcess/Plugins/PDF"
    "${WEBKIT2_DIR}/WebProcess/Plugins/Netscape/mac"
    "${WEBKIT2_DIR}/WebProcess/Scrolling"
    "${WEBKIT2_DIR}/WebProcess/WebPage/Cocoa"
    "${WEBKIT2_DIR}/WebProcess/WebPage/mac"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/mac"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
)

# This is needed because of a naming conflict with DiagnosticLoggingClient.h.
# FIXME: Rename one of the DiagnosticLoggingClient headers.
list(REMOVE_ITEM WebKit2_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/page"
)
list(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/page"
)

set(WEBKIT2_EXTRA_DEPENDENCIES
     WebKit2-forwarding-headers
)

set(XPCService_SOURCES
    Shared/EntryPointUtilities/mac/XPCService/XPCServiceEntryPoint.mm
    Shared/EntryPointUtilities/mac/XPCService/XPCServiceMain.mm
)

set(WebProcess_SOURCES
    WebProcess/EntryPoint/mac/XPCService/WebContentServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

set(PluginProcess_SOURCES
    PluginProcess/EntryPoint/mac/XPCService/PluginServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/mac/XPCService/NetworkServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

list(APPEND DatabaseProcess_SOURCES
    DatabaseProcess/EntryPoint/mac/XPCService/DatabaseServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

add_definitions("-include WebKit2Prefix.h")

set(WebKit2_FORWARDING_HEADERS_FILES
    Shared/API/c/WKDiagnosticLoggingResultType.h

    UIProcess/API/C/WKPageDiagnosticLoggingClient.h
    UIProcess/API/C/WKPageNavigationClient.h
    UIProcess/API/C/WKPageRenderingProgressEvents.h
)

list(APPEND WebKit2_MESSAGES_IN_FILES
    Shared/API/Cocoa/RemoteObjectRegistry.messages.in

    Shared/mac/SecItemShim.messages.in

    UIProcess/Cocoa/WebVideoFullscreenManagerProxy.messages.in

    UIProcess/mac/RemoteLayerTreeDrawingAreaProxy.messages.in
    UIProcess/mac/SecItemShimProxy.messages.in
    UIProcess/mac/ViewGestureController.messages.in

    WebProcess/Scrolling/RemoteScrollingCoordinator.messages.in
    WebProcess/WebPage/ViewGestureGeometryCollector.messages.in
)

set(WebKit2_FORWARDING_HEADERS_DIRECTORIES
    Platform
    Shared

    Shared/API
    Shared/Cocoa

    Shared/API/Cocoa
    Shared/API/c

    Shared/API/c/cf
    Shared/API/c/mac

    UIProcess/Cocoa

    UIProcess/API/C
    UIProcess/API/cpp

    WebProcess/WebPage

    WebProcess/InjectedBundle/API/Cocoa
    WebProcess/InjectedBundle/API/c
    WebProcess/InjectedBundle/API/mac
)

# This is needed right now to import ObjC headers instead of including them.
# FIXME: Forwarding headers should be copies of actual headers.
file(GLOB ObjCHeaders UIProcess/API/Cocoa/*.h)
foreach (_file ${ObjCHeaders})
    get_filename_component(_name ${_file} NAME)
    if (NOT EXISTS ${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebKit/${_name})
        file(WRITE ${DERIVED_SOURCES_DIR}/ForwardingHeaders/WebKit/${_name} "#import <WebKit2/UIProcess/API/Cocoa/${_name}>")
    endif ()
endforeach ()

set(WebKit2_OUTPUT_NAME WebKit)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2_DIR}/MessageRecorderProbes.h
    MAIN_DEPENDENCY Platform/IPC/MessageRecorderProbes.d
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBKIT2_DIR}
    COMMAND dtrace -h -s ${WEBKIT2_DIR}/Platform/IPC/MessageRecorderProbes.d
    VERBATIM)
list(APPEND WebKit2_SOURCES
    ${DERIVED_SOURCES_WEBKIT2_DIR}/MessageRecorderProbes.h
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebKit FILES ${WebKit2_FORWARDING_HEADERS_FILES} DIRECTORIES ${WebKit2_FORWARDING_HEADERS_DIRECTORIES})
