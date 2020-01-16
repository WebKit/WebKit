set(WebKit_OUTPUT_NAME SceWebKit)
set(WebKit_WebProcess_OUTPUT_NAME WebKitWebProcess)
set(WebKit_NetworkProcess_OUTPUT_NAME WebKitNetworkProcess)
set(WebKit_PluginProcess_OUTPUT_NAME WebKitPluginProcess)

add_definitions(-DBUILDING_WEBKIT)

set(WebKit_USE_PREFIX_HEADER ON)

list(APPEND WebProcess_SOURCES
    WebProcess/EntryPoint/unix/WebProcessMain.cpp
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/unix/NetworkProcessMain.cpp
)

list(APPEND WebKit_SOURCES
    NetworkProcess/Classifier/WebResourceLoadStatisticsStore.cpp
    NetworkProcess/Classifier/WebResourceLoadStatisticsTelemetry.cpp

    NetworkProcess/Cookies/curl/WebCookieManagerCurl.cpp

    NetworkProcess/WebStorage/StorageManager.cpp

    NetworkProcess/cache/NetworkCacheDataCurl.cpp
    NetworkProcess/cache/NetworkCacheIOChannelCurl.cpp

    NetworkProcess/curl/NetworkDataTaskCurl.cpp
    NetworkProcess/curl/NetworkProcessCurl.cpp
    NetworkProcess/curl/NetworkSessionCurl.cpp
    NetworkProcess/curl/RemoteNetworkingContextCurl.cpp

    Platform/IPC/unix/AttachmentUnix.cpp
    Platform/IPC/unix/ConnectionUnix.cpp

    Platform/classifier/ResourceLoadStatisticsClassifier.cpp

    Platform/unix/LoggingUnix.cpp
    Platform/unix/ModuleUnix.cpp
    Platform/unix/SharedMemoryUnix.cpp

    Shared/API/c/cairo/WKImageCairo.cpp

    Shared/API/c/curl/WKCertificateInfoCurl.cpp

    Shared/CoordinatedGraphics/CoordinatedGraphicsScene.cpp
    Shared/CoordinatedGraphics/SimpleViewportController.cpp

    Shared/CoordinatedGraphics/threadedcompositor/CompositingRunLoop.cpp
    Shared/CoordinatedGraphics/threadedcompositor/ThreadedCompositor.cpp
    Shared/CoordinatedGraphics/threadedcompositor/ThreadedDisplayRefreshMonitor.cpp

    Shared/Plugins/Netscape/NetscapePluginModuleNone.cpp

    Shared/cairo/ShareableBitmapCairo.cpp

    Shared/curl/WebCoreArgumentCodersCurl.cpp

    Shared/libwpe/NativeWebKeyboardEventLibWPE.cpp
    Shared/libwpe/NativeWebMouseEventLibWPE.cpp
    Shared/libwpe/NativeWebTouchEventLibWPE.cpp
    Shared/libwpe/NativeWebWheelEventLibWPE.cpp
    Shared/libwpe/WebEventFactory.cpp

    Shared/unix/AuxiliaryProcessMain.cpp

    UIProcess/BackingStore.cpp
    UIProcess/DefaultUndoController.cpp
    UIProcess/LegacySessionStateCodingNone.cpp
    UIProcess/WebGrammarDetail.cpp
    UIProcess/WebMemoryPressureHandler.cpp
    UIProcess/WebViewportAttributes.cpp

    UIProcess/API/C/WKGrammarDetail.cpp
    UIProcess/API/C/WKViewportAttributes.cpp

    UIProcess/API/C/curl/WKProtectionSpaceCurl.cpp
    UIProcess/API/C/curl/WKWebsiteDataStoreRefCurl.cpp

    UIProcess/Automation/cairo/WebAutomationSessionCairo.cpp

    UIProcess/CoordinatedGraphics/DrawingAreaProxyCoordinatedGraphics.cpp

    UIProcess/Launcher/playstation/ProcessLauncherPlayStation.cpp

    UIProcess/WebsiteData/curl/WebsiteDataStoreCurl.cpp

    UIProcess/WebsiteData/playstation/WebsiteDataStorePlayStation.cpp

    UIProcess/cairo/BackingStoreCairo.cpp

    UIProcess/libwpe/WebPasteboardProxyLibWPE.cpp

    UIProcess/playstation/WebPageProxyPlayStation.cpp
    UIProcess/playstation/WebProcessPoolPlayStation.cpp

    WebProcess/InjectedBundle/playstation/InjectedBundlePlayStation.cpp

    WebProcess/WebCoreSupport/curl/WebFrameNetworkingContext.cpp

    WebProcess/WebPage/AcceleratedSurface.cpp

    WebProcess/WebPage/CoordinatedGraphics/CompositingCoordinator.cpp
    WebProcess/WebPage/CoordinatedGraphics/DrawingAreaCoordinatedGraphics.cpp
    WebProcess/WebPage/CoordinatedGraphics/LayerTreeHost.cpp

    WebProcess/WebPage/libwpe/AcceleratedSurfaceLibWPE.cpp

    WebProcess/WebPage/playstation/WebPagePlayStation.cpp

    WebProcess/playstation/WebProcessPlayStation.cpp
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/NetworkProcess/curl"
    "${WEBKIT_DIR}/NetworkProcess/unix"
    "${WEBKIT_DIR}/Platform/IPC/unix"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT_DIR}/Shared/libwpe"
    "${WEBKIT_DIR}/Shared/unix"
    "${WEBKIT_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT_DIR}/UIProcess/API/C/curl"
    "${WEBKIT_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/curl"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebPage/libwpe"
    "${WEBKIT_DIR}/WebProcess/unix"
)

# Temporarily list out shared headers here
set(WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/API/c/WKArray.h
    Shared/API/c/WKBase.h
    Shared/API/c/WKCertificateInfo.h
    Shared/API/c/WKConnectionRef.h
    Shared/API/c/WKContextMenuItem.h
    Shared/API/c/WKContextMenuItemTypes.h
    Shared/API/c/WKData.h
    Shared/API/c/WKDeclarationSpecifiers.h
    Shared/API/c/WKDeprecated.h
    Shared/API/c/WKDiagnosticLoggingResultType.h
    Shared/API/c/WKDictionary.h
    Shared/API/c/WKErrorRef.h
    Shared/API/c/WKEvent.h
    Shared/API/c/WKFindOptions.h
    Shared/API/c/WKGeometry.h
    Shared/API/c/WKImage.h
    Shared/API/c/WKMutableArray.h
    Shared/API/c/WKMutableDictionary.h
    Shared/API/c/WKNumber.h
    Shared/API/c/WKPageLoadTypes.h
    Shared/API/c/WKPageLoadTypesPrivate.h
    Shared/API/c/WKPageVisibilityTypes.h
    Shared/API/c/WKPluginInformation.h
    Shared/API/c/WKSecurityOriginRef.h
    Shared/API/c/WKSerializedScriptValue.h
    Shared/API/c/WKString.h
    Shared/API/c/WKStringPrivate.h
    Shared/API/c/WKType.h
    Shared/API/c/WKURL.h
    Shared/API/c/WKURLRequest.h
    Shared/API/c/WKURLResponse.h
    Shared/API/c/WKUserContentInjectedFrames.h
    Shared/API/c/WKUserScriptInjectionTime.h

    UIProcess/API/C/WKAuthenticationChallenge.h
    UIProcess/API/C/WKAuthenticationDecisionListener.h
    UIProcess/API/C/WKBackForwardListItemRef.h
    UIProcess/API/C/WKBackForwardListRef.h
    UIProcess/API/C/WKContext.h
    UIProcess/API/C/WKContextConfigurationRef.h
    UIProcess/API/C/WKContextConnectionClient.h
    UIProcess/API/C/WKContextDownloadClient.h
    UIProcess/API/C/WKContextHistoryClient.h
    UIProcess/API/C/WKContextInjectedBundleClient.h
    UIProcess/API/C/WKContextPrivate.h
    UIProcess/API/C/WKCookieManager.h
    UIProcess/API/C/WKCredential.h
    UIProcess/API/C/WKCredentialTypes.h
    UIProcess/API/C/WKDownload.h
    UIProcess/API/C/WKFormSubmissionListener.h
    UIProcess/API/C/WKFrame.h
    UIProcess/API/C/WKFrameHandleRef.h
    UIProcess/API/C/WKFrameInfoRef.h
    UIProcess/API/C/WKFramePolicyListener.h
    UIProcess/API/C/WKGeolocationManager.h
    UIProcess/API/C/WKGeolocationPermissionRequest.h
    UIProcess/API/C/WKGeolocationPosition.h
    UIProcess/API/C/WKHTTPCookieStoreRef.h
    UIProcess/API/C/WKHitTestResult.h
    UIProcess/API/C/WKIconDatabase.h
    UIProcess/API/C/WKInspector.h
    UIProcess/API/C/WKLayoutMode.h
    UIProcess/API/C/WKMessageListener.h
    UIProcess/API/C/WKMockDisplay.h
    UIProcess/API/C/WKMockMediaDevice.h
    UIProcess/API/C/WKNativeEvent.h
    UIProcess/API/C/WKNavigationActionRef.h
    UIProcess/API/C/WKNavigationDataRef.h
    UIProcess/API/C/WKNavigationRef.h
    UIProcess/API/C/WKNavigationResponseRef.h
    UIProcess/API/C/WKNotification.h
    UIProcess/API/C/WKNotificationManager.h
    UIProcess/API/C/WKNotificationPermissionRequest.h
    UIProcess/API/C/WKNotificationProvider.h
    UIProcess/API/C/WKOpenPanelParametersRef.h
    UIProcess/API/C/WKOpenPanelResultListener.h
    UIProcess/API/C/WKPage.h
    UIProcess/API/C/WKPageConfigurationRef.h
    UIProcess/API/C/WKPageContextMenuClient.h
    UIProcess/API/C/WKPageDiagnosticLoggingClient.h
    UIProcess/API/C/WKPageFindClient.h
    UIProcess/API/C/WKPageFindMatchesClient.h
    UIProcess/API/C/WKPageFormClient.h
    UIProcess/API/C/WKPageGroup.h
    UIProcess/API/C/WKPageInjectedBundleClient.h
    UIProcess/API/C/WKPageLoaderClient.h
    UIProcess/API/C/WKPageNavigationClient.h
    UIProcess/API/C/WKPagePolicyClient.h
    UIProcess/API/C/WKPagePrivate.h
    UIProcess/API/C/WKPageRenderingProgressEvents.h
    UIProcess/API/C/WKPageStateClient.h
    UIProcess/API/C/WKPageUIClient.h
    UIProcess/API/C/WKPluginLoadPolicy.h
    UIProcess/API/C/WKPreferencesRef.h
    UIProcess/API/C/WKPreferencesRefPrivate.h
    UIProcess/API/C/WKProcessTerminationReason.h
    UIProcess/API/C/WKProtectionSpace.h
    UIProcess/API/C/WKProtectionSpaceTypes.h
    UIProcess/API/C/WKResourceCacheManager.h
    UIProcess/API/C/WKSessionStateRef.h
    UIProcess/API/C/WKTestingSupport.h
    UIProcess/API/C/WKTextChecker.h
    UIProcess/API/C/WKUserContentControllerRef.h
    UIProcess/API/C/WKUserContentExtensionStoreRef.h
    UIProcess/API/C/WKUserMediaPermissionCheck.h
    UIProcess/API/C/WKUserMediaPermissionRequest.h
    UIProcess/API/C/WKUserScriptRef.h
    UIProcess/API/C/WKViewportAttributes.h
    UIProcess/API/C/WKWebsiteDataStoreConfigurationRef.h
    UIProcess/API/C/WKWebsiteDataStoreRef.h
    UIProcess/API/C/WKWebsitePolicies.h
    UIProcess/API/C/WKWindowFeaturesRef.h
    UIProcess/API/C/WebKit2_C.h

    UIProcess/API/cpp/WKRetainPtr.h

    WebProcess/InjectedBundle/API/c/WKBundle.h
    WebProcess/InjectedBundle/API/c/WKBundleAPICast.h
    WebProcess/InjectedBundle/API/c/WKBundleBackForwardList.h
    WebProcess/InjectedBundle/API/c/WKBundleBackForwardListItem.h
    WebProcess/InjectedBundle/API/c/WKBundleDOMWindowExtension.h
    WebProcess/InjectedBundle/API/c/WKBundleFileHandleRef.h
    WebProcess/InjectedBundle/API/c/WKBundleFrame.h
    WebProcess/InjectedBundle/API/c/WKBundleFramePrivate.h
    WebProcess/InjectedBundle/API/c/WKBundleHitTestResult.h
    WebProcess/InjectedBundle/API/c/WKBundleInitialize.h
    WebProcess/InjectedBundle/API/c/WKBundleInspector.h
    WebProcess/InjectedBundle/API/c/WKBundleNavigationAction.h
    WebProcess/InjectedBundle/API/c/WKBundleNavigationActionPrivate.h
    WebProcess/InjectedBundle/API/c/WKBundleNodeHandle.h
    WebProcess/InjectedBundle/API/c/WKBundleNodeHandlePrivate.h
    WebProcess/InjectedBundle/API/c/WKBundlePage.h
    WebProcess/InjectedBundle/API/c/WKBundlePageBanner.h
    WebProcess/InjectedBundle/API/c/WKBundlePageContextMenuClient.h
    WebProcess/InjectedBundle/API/c/WKBundlePageEditorClient.h
    WebProcess/InjectedBundle/API/c/WKBundlePageFormClient.h
    WebProcess/InjectedBundle/API/c/WKBundlePageFullScreenClient.h
    WebProcess/InjectedBundle/API/c/WKBundlePageGroup.h
    WebProcess/InjectedBundle/API/c/WKBundlePageLoaderClient.h
    WebProcess/InjectedBundle/API/c/WKBundlePageOverlay.h
    WebProcess/InjectedBundle/API/c/WKBundlePagePolicyClient.h
    WebProcess/InjectedBundle/API/c/WKBundlePagePrivate.h
    WebProcess/InjectedBundle/API/c/WKBundlePageResourceLoadClient.h
    WebProcess/InjectedBundle/API/c/WKBundlePageUIClient.h
    WebProcess/InjectedBundle/API/c/WKBundlePrivate.h
    WebProcess/InjectedBundle/API/c/WKBundleRangeHandle.h
    WebProcess/InjectedBundle/API/c/WKBundleScriptWorld.h
)

# PlayStation specific
list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS

)

WEBKIT_MAKE_FORWARDING_HEADERS(WebKit
    TARGET_NAME WebKitFrameworkHeaders
    DESTINATION ${WebKit_FRAMEWORK_HEADERS_DIR}/WebKit
    FILES ${WebKit_PUBLIC_FRAMEWORK_HEADERS}
    FLATTENED
)
