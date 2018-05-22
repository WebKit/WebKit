include(WebKitCommon)

set_property(DIRECTORY . PROPERTY FOLDER "WebKit")

set(WebKit_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/NetworkProcess"
    "${WEBKIT_DIR}/NetworkProcess/Cookies"
    "${WEBKIT_DIR}/NetworkProcess/CustomProtocols"
    "${WEBKIT_DIR}/NetworkProcess/Downloads"
    "${WEBKIT_DIR}/NetworkProcess/FileAPI"
    "${WEBKIT_DIR}/NetworkProcess/cache"
    "${WEBKIT_DIR}/NetworkProcess/capture"
    "${WEBKIT_DIR}/NetworkProcess/webrtc"
    "${WEBKIT_DIR}/Platform"
    "${WEBKIT_DIR}/Platform/IPC"
    "${WEBKIT_DIR}/PluginProcess"
    "${WEBKIT_DIR}/Shared"
    "${WEBKIT_DIR}/Shared/API"
    "${WEBKIT_DIR}/Shared/API/c"
    "${WEBKIT_DIR}/Shared/Authentication"
    "${WEBKIT_DIR}/Shared/CoreIPCSupport"
    "${WEBKIT_DIR}/Shared/Databases"
    "${WEBKIT_DIR}/Shared/Databases/IndexedDB"
    "${WEBKIT_DIR}/Shared/FileAPI"
    "${WEBKIT_DIR}/Shared/Gamepad"
    "${WEBKIT_DIR}/Shared/Plugins"
    "${WEBKIT_DIR}/Shared/Plugins/Netscape"
    "${WEBKIT_DIR}/Shared/Plugins/Netscape/x11"
    "${WEBKIT_DIR}/Shared/RemoteLayerTree"
    "${WEBKIT_DIR}/Shared/Storage"
    "${WEBKIT_DIR}/Shared/WebsiteData"
    "${WEBKIT_DIR}/StorageProcess"
    "${WEBKIT_DIR}/StorageProcess/IndexedDB"
    "${WEBKIT_DIR}/StorageProcess/ServiceWorker"
    "${WEBKIT_DIR}/UIProcess"
    "${WEBKIT_DIR}/UIProcess/API"
    "${WEBKIT_DIR}/UIProcess/API/C"
    "${WEBKIT_DIR}/UIProcess/API/cpp"
    "${WEBKIT_DIR}/UIProcess/ApplePay"
    "${WEBKIT_DIR}/UIProcess/Authentication"
    "${WEBKIT_DIR}/UIProcess/Automation"
    "${WEBKIT_DIR}/UIProcess/Downloads"
    "${WEBKIT_DIR}/UIProcess/Gamepad"
    "${WEBKIT_DIR}/UIProcess/Launcher"
    "${WEBKIT_DIR}/UIProcess/Network"
    "${WEBKIT_DIR}/UIProcess/Network/CustomProtocols"
    "${WEBKIT_DIR}/UIProcess/Notifications"
    "${WEBKIT_DIR}/UIProcess/Plugins"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree"
    "${WEBKIT_DIR}/UIProcess/Storage"
    "${WEBKIT_DIR}/UIProcess/UserContent"
    "${WEBKIT_DIR}/UIProcess/WebStorage"
    "${WEBKIT_DIR}/UIProcess/WebsiteData"
    "${WEBKIT_DIR}/WebProcess"
    "${WEBKIT_DIR}/WebProcess/ApplePay"
    "${WEBKIT_DIR}/WebProcess/ApplicationCache"
    "${WEBKIT_DIR}/WebProcess/Automation"
    "${WEBKIT_DIR}/WebProcess/Cache"
    "${WEBKIT_DIR}/WebProcess/Databases"
    "${WEBKIT_DIR}/WebProcess/Databases/IndexedDB"
    "${WEBKIT_DIR}/WebProcess/FileAPI"
    "${WEBKIT_DIR}/WebProcess/FullScreen"
    "${WEBKIT_DIR}/WebProcess/Gamepad"
    "${WEBKIT_DIR}/WebProcess/Geolocation"
    "${WEBKIT_DIR}/WebProcess/IconDatabase"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/c"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/DOM"
    "${WEBKIT_DIR}/WebProcess/Launching"
    "${WEBKIT_DIR}/WebProcess/MediaCache"
    "${WEBKIT_DIR}/WebProcess/MediaStream"
    "${WEBKIT_DIR}/WebProcess/Network"
    "${WEBKIT_DIR}/WebProcess/Network/webrtc"
    "${WEBKIT_DIR}/WebProcess/Notifications"
    "${WEBKIT_DIR}/WebProcess/OriginData"
    "${WEBKIT_DIR}/WebProcess/Plugins"
    "${WEBKIT_DIR}/WebProcess/Plugins/Netscape"
    "${WEBKIT_DIR}/WebProcess/ResourceCache"
    "${WEBKIT_DIR}/WebProcess/Storage"
    "${WEBKIT_DIR}/WebProcess/UserContent"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport"
    "${WEBKIT_DIR}/WebProcess/WebPage"
    "${WEBKIT_DIR}/WebProcess/WebPage/RemoteLayerTree"
    "${WEBKIT_DIR}/WebProcess/WebStorage"
    "${DERIVED_SOURCES_WEBKIT_DIR}"
    "${DERIVED_SOURCES_WEBKIT_DIR}/include"
    "${CMAKE_SOURCE_DIR}/Source"
    "${CMAKE_SOURCE_DIR}/WebKitLibraries"
)

set(PROTOCOL_GENERATOR_SCRIPTS_DIR "${JAVASCRIPTCORE_DIR}/inspector/scripts")

set(WebKit_SYSTEM_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
)

set(WebKit_SOURCES
    NetworkProcess/NetworkActivityTracker.cpp
    NetworkProcess/NetworkCORSPreflightChecker.cpp
    NetworkProcess/NetworkConnectionToWebProcess.cpp
    NetworkProcess/NetworkContentRuleListManager.cpp
    NetworkProcess/NetworkDataTask.cpp
    NetworkProcess/NetworkDataTaskBlob.cpp
    NetworkProcess/NetworkLoad.cpp
    NetworkProcess/NetworkLoadChecker.cpp
    NetworkProcess/NetworkProcess.cpp
    NetworkProcess/NetworkProcessCreationParameters.cpp
    NetworkProcess/NetworkProcessPlatformStrategies.cpp
    NetworkProcess/NetworkResourceLoadParameters.cpp
    NetworkProcess/NetworkResourceLoader.cpp
    NetworkProcess/NetworkSession.cpp
    NetworkProcess/NetworkSocketStream.cpp
    NetworkProcess/PingLoad.cpp
    NetworkProcess/PreconnectTask.cpp

    NetworkProcess/Cookies/WebCookieManager.cpp

    NetworkProcess/Downloads/Download.cpp
    NetworkProcess/Downloads/DownloadManager.cpp
    NetworkProcess/Downloads/PendingDownload.cpp

    NetworkProcess/FileAPI/NetworkBlobRegistry.cpp

    NetworkProcess/cache/CacheStorageEngine.cpp
    NetworkProcess/cache/CacheStorageEngineCache.cpp
    NetworkProcess/cache/CacheStorageEngineCaches.cpp
    NetworkProcess/cache/CacheStorageEngineConnection.cpp
    NetworkProcess/cache/NetworkCache.cpp
    NetworkProcess/cache/NetworkCacheBlobStorage.cpp
    NetworkProcess/cache/NetworkCacheCoders.cpp
    NetworkProcess/cache/NetworkCacheData.cpp
    NetworkProcess/cache/NetworkCacheEntry.cpp
    NetworkProcess/cache/NetworkCacheFileSystem.cpp
    NetworkProcess/cache/NetworkCacheKey.cpp
    NetworkProcess/cache/NetworkCacheSpeculativeLoad.cpp
    NetworkProcess/cache/NetworkCacheSpeculativeLoadManager.cpp
    NetworkProcess/cache/NetworkCacheSubresourcesEntry.cpp
    NetworkProcess/cache/NetworkCacheStatistics.cpp
    NetworkProcess/cache/NetworkCacheStorage.cpp

    NetworkProcess/capture/NetworkCaptureEvent.cpp
    NetworkProcess/capture/NetworkCaptureManager.cpp
    NetworkProcess/capture/NetworkCaptureRecorder.cpp
    NetworkProcess/capture/NetworkCaptureReplayer.cpp
    NetworkProcess/capture/NetworkCaptureResource.cpp
    NetworkProcess/capture/NetworkDataTaskReplay.cpp

    NetworkProcess/webrtc/NetworkMDNSRegister.cpp

    Platform/Logging.cpp
    Platform/Module.cpp

    Platform/IPC/ArgumentCoders.cpp
    Platform/IPC/Attachment.cpp
    Platform/IPC/Connection.cpp
    Platform/IPC/DataReference.cpp
    Platform/IPC/Decoder.cpp
    Platform/IPC/Encoder.cpp
    Platform/IPC/MessageReceiverMap.cpp
    Platform/IPC/MessageSender.cpp
    Platform/IPC/StringReference.cpp

    PluginProcess/PluginControllerProxy.cpp
    PluginProcess/PluginCreationParameters.cpp
    PluginProcess/PluginProcess.cpp
    PluginProcess/WebProcessConnection.cpp

    Shared/ActivityAssertion.cpp
    Shared/AsyncRequest.cpp
    Shared/BlobDataFileReferenceWithSandboxExtension.cpp
    Shared/CacheModel.cpp
    Shared/ChildProcess.cpp
    Shared/ContextMenuContextData.cpp
    Shared/EditorState.cpp
    Shared/FontInfo.cpp
    Shared/FrameInfoData.cpp
    Shared/LayerTreeContext.cpp
    Shared/LoadParameters.cpp
    Shared/NavigationActionData.cpp
    Shared/PlatformPopupMenuData.cpp
    Shared/PrintInfo.cpp
    Shared/RTCNetwork.cpp
    Shared/RTCPacketOptions.cpp
    Shared/SessionState.cpp
    Shared/SessionTracker.cpp
    Shared/ShareableBitmap.cpp
    Shared/ShareableResource.cpp
    Shared/SharedStringHashStore.cpp
    Shared/SharedStringHashTableReadOnly.cpp
    Shared/SharedStringHashTable.cpp
    Shared/StatisticsData.cpp
    Shared/TouchBarMenuData.cpp
    Shared/TouchBarMenuItemData.cpp
    Shared/URLSchemeTaskParameters.cpp
    Shared/UpdateInfo.cpp
    Shared/UserData.cpp
    Shared/WebBackForwardListItem.cpp
    Shared/WebCompiledContentRuleList.cpp
    Shared/WebCompiledContentRuleListData.cpp
    Shared/WebConnection.cpp
    Shared/WebConnectionClient.cpp
    Shared/WebContextMenuItem.cpp
    Shared/WebContextMenuItemData.cpp
    Shared/WebCoreArgumentCoders.cpp
    Shared/WebErrors.cpp
    Shared/WebEvent.cpp
    Shared/WebEventConversion.cpp
    Shared/WebGeolocationPosition.cpp
    Shared/WebHitTestResultData.cpp
    Shared/WebImage.cpp
    Shared/WebKeyboardEvent.cpp
    Shared/WebKit2Initialize.cpp
    Shared/WebMediaSessionMetadata.cpp
    Shared/WebMemorySampler.cpp
    Shared/WebMouseEvent.cpp
    Shared/WebPageCreationParameters.cpp
    Shared/WebPageGroupData.cpp
    Shared/WebPlatformTouchPoint.cpp
    Shared/WebPopupItem.cpp
    Shared/WebPreferencesDefaultValues.cpp
    Shared/WebPreferencesStore.cpp
    Shared/WebProcessCreationParameters.cpp
    Shared/WebRenderLayer.cpp
    Shared/WebRenderObject.cpp
    Shared/WebTouchEvent.cpp
    Shared/WebUserContentControllerDataTypes.cpp
    Shared/WebWheelEvent.cpp
    Shared/WebsiteDataStoreParameters.cpp
    Shared/WebsitePoliciesData.cpp

    Shared/API/APIArray.cpp
    Shared/API/APIData.cpp
    Shared/API/APIDictionary.cpp
    Shared/API/APIError.cpp
    Shared/API/APIFrameHandle.cpp
    Shared/API/APIGeometry.cpp
    Shared/API/APIObject.cpp
    Shared/API/APIPageGroupHandle.cpp
    Shared/API/APIPageHandle.cpp
    Shared/API/APIURLRequest.cpp
    Shared/API/APIURLResponse.cpp

    Shared/API/c/WKArray.cpp
    Shared/API/c/WKCertificateInfo.cpp
    Shared/API/c/WKConnectionRef.cpp
    Shared/API/c/WKContextMenuItem.cpp
    Shared/API/c/WKData.cpp
    Shared/API/c/WKDeprecatedFunctions.cpp
    Shared/API/c/WKDictionary.cpp
    Shared/API/c/WKErrorRef.cpp
    Shared/API/c/WKGeometry.cpp
    Shared/API/c/WKImage.cpp
    Shared/API/c/WKMutableArray.cpp
    Shared/API/c/WKMutableDictionary.cpp
    Shared/API/c/WKNumber.cpp
    Shared/API/c/WKPluginInformation.cpp
    Shared/API/c/WKRenderLayer.cpp
    Shared/API/c/WKRenderObject.cpp
    Shared/API/c/WKSecurityOriginRef.cpp
    Shared/API/c/WKSerializedScriptValue.cpp
    Shared/API/c/WKString.cpp
    Shared/API/c/WKType.cpp
    Shared/API/c/WKURL.cpp
    Shared/API/c/WKURLRequest.cpp
    Shared/API/c/WKURLResponse.cpp
    Shared/API/c/WKUserContentURLPattern.cpp

    Shared/Authentication/AuthenticationManager.cpp

    Shared/Databases/IndexedDB/IDBUtilities.cpp
    Shared/Databases/IndexedDB/WebIDBResult.cpp

    Shared/Gamepad/GamepadData.cpp

    Shared/Plugins/NPIdentifierData.cpp
    Shared/Plugins/NPObjectMessageReceiver.cpp
    Shared/Plugins/NPObjectProxy.cpp
    Shared/Plugins/NPRemoteObjectMap.cpp
    Shared/Plugins/NPVariantData.cpp
    Shared/Plugins/PluginProcessCreationParameters.cpp

    Shared/Plugins/Netscape/NetscapePluginModule.cpp
    Shared/Plugins/Netscape/NetscapePluginModuleNone.cpp
    Shared/Plugins/Netscape/PluginInformation.cpp

    Shared/Storage/StorageProcessCreationParameters.cpp

    Shared/WebsiteData/WebsiteData.cpp

    StorageProcess/StorageProcess.cpp
    StorageProcess/StorageToWebProcessConnection.cpp

    StorageProcess/IndexedDB/WebIDBConnectionToClient.cpp

    StorageProcess/ServiceWorker/WebSWOriginStore.cpp
    StorageProcess/ServiceWorker/WebSWServerConnection.cpp
    StorageProcess/ServiceWorker/WebSWServerToContextConnection.cpp

    UIProcess/BackgroundProcessResponsivenessTimer.cpp
    UIProcess/ChildProcessProxy.cpp
    UIProcess/DrawingAreaProxy.cpp
    UIProcess/FrameLoadState.cpp
    UIProcess/GeolocationPermissionRequestManagerProxy.cpp
    UIProcess/GeolocationPermissionRequestProxy.cpp
    UIProcess/PageLoadState.cpp
    UIProcess/ProcessAssertion.cpp
    UIProcess/ProcessThrottler.cpp
    UIProcess/RemoteWebInspectorProxy.cpp
    UIProcess/ResourceLoadStatisticsPersistentStorage.cpp
    UIProcess/ResponsivenessTimer.cpp
    UIProcess/ServiceWorkerProcessProxy.cpp
    UIProcess/StatisticsRequest.cpp
    UIProcess/SuspendedPageProxy.cpp
    UIProcess/SystemPreviewController.cpp
    UIProcess/TextCheckerCompletion.cpp
    UIProcess/UIMessagePortChannelProvider.cpp
    UIProcess/UserMediaPermissionCheckProxy.cpp
    UIProcess/UserMediaPermissionRequestManagerProxy.cpp
    UIProcess/UserMediaPermissionRequestProxy.cpp
    UIProcess/UserMediaProcessManager.cpp
    UIProcess/VisitedLinkStore.cpp
    UIProcess/WebBackForwardList.cpp
    UIProcess/WebColorPicker.cpp
    UIProcess/WebConnectionToWebProcess.cpp
    UIProcess/WebContextClient.cpp
    UIProcess/WebContextConnectionClient.cpp
    UIProcess/WebContextInjectedBundleClient.cpp
    UIProcess/WebContextMenuListenerProxy.cpp
    UIProcess/WebContextMenuProxy.cpp
    UIProcess/WebCookieManagerProxy.cpp
    UIProcess/WebCookieManagerProxyClient.cpp
    UIProcess/WebEditCommandProxy.cpp
    UIProcess/WebFormClient.cpp
    UIProcess/WebFormSubmissionListenerProxy.cpp
    UIProcess/WebFrameListenerProxy.cpp
    UIProcess/WebFramePolicyListenerProxy.cpp
    UIProcess/WebFrameProxy.cpp
    UIProcess/WebFullScreenManagerProxy.cpp
    UIProcess/WebGeolocationManagerProxy.cpp
    UIProcess/WebGeolocationProvider.cpp
    UIProcess/WebGrammarDetail.cpp
    UIProcess/WebInspectorProxy.cpp
    UIProcess/WebInspectorUtilities.cpp
    UIProcess/WebNavigationState.cpp
    UIProcess/WebOpenPanelResultListenerProxy.cpp
    UIProcess/WebPageDiagnosticLoggingClient.cpp
    UIProcess/WebPageGroup.cpp
    UIProcess/WebPageInjectedBundleClient.cpp
    UIProcess/WebPageProxy.cpp
    UIProcess/WebPasteboardProxy.cpp
    UIProcess/WebPreferences.cpp
    UIProcess/WebProcessLifetimeObserver.cpp
    UIProcess/WebProcessLifetimeTracker.cpp
    UIProcess/WebProcessPool.cpp
    UIProcess/WebProcessProxy.cpp
    UIProcess/WebURLSchemeHandler.cpp
    UIProcess/WebURLSchemeTask.cpp
    UIProcess/WebViewportAttributes.cpp

    UIProcess/API/APIContentRuleList.cpp
    UIProcess/API/APIContentRuleListStore.cpp
    UIProcess/API/APIExperimentalFeature.cpp
    UIProcess/API/APIFrameInfo.cpp
    UIProcess/API/APIHTTPCookieStore.cpp
    UIProcess/API/APIHitTestResult.cpp
    UIProcess/API/APINavigation.cpp
    UIProcess/API/APINavigationData.cpp
    UIProcess/API/APIPageConfiguration.cpp
    UIProcess/API/APIProcessPoolConfiguration.cpp
    UIProcess/API/APIOpenPanelParameters.cpp
    UIProcess/API/APISessionState.cpp
    UIProcess/API/APIURLSchemeTask.cpp
    UIProcess/API/APIUserContentWorld.cpp
    UIProcess/API/APIUserScript.cpp
    UIProcess/API/APIUserStyleSheet.cpp
    UIProcess/API/APIWebsiteDataStore.cpp
    UIProcess/API/APIWebsitePolicies.cpp
    UIProcess/API/APIWindowFeatures.cpp

    UIProcess/API/C/WKApplicationCacheManager.cpp
    UIProcess/API/C/WKAuthenticationChallenge.cpp
    UIProcess/API/C/WKAuthenticationDecisionListener.cpp
    UIProcess/API/C/WKBackForwardListItemRef.cpp
    UIProcess/API/C/WKBackForwardListRef.cpp
    UIProcess/API/C/WKContext.cpp
    UIProcess/API/C/WKContextConfigurationRef.cpp
    UIProcess/API/C/WKCookieManager.cpp
    UIProcess/API/C/WKCredential.cpp
    UIProcess/API/C/WKDownload.cpp
    UIProcess/API/C/WKFormSubmissionListener.cpp
    UIProcess/API/C/WKFrame.cpp
    UIProcess/API/C/WKFrameHandleRef.cpp
    UIProcess/API/C/WKFrameInfoRef.cpp
    UIProcess/API/C/WKFramePolicyListener.cpp
    UIProcess/API/C/WKGeolocationManager.cpp
    UIProcess/API/C/WKGeolocationPermissionRequest.cpp
    UIProcess/API/C/WKGeolocationPosition.cpp
    UIProcess/API/C/WKHitTestResult.cpp
    UIProcess/API/C/WKIconDatabase.cpp
    UIProcess/API/C/WKInspector.cpp
    UIProcess/API/C/WKKeyValueStorageManager.cpp
    UIProcess/API/C/WKMediaSessionMetadata.cpp
    UIProcess/API/C/WKNavigationActionRef.cpp
    UIProcess/API/C/WKNavigationDataRef.cpp
    UIProcess/API/C/WKNavigationRef.cpp
    UIProcess/API/C/WKNavigationResponseRef.cpp
    UIProcess/API/C/WKNotification.cpp
    UIProcess/API/C/WKNotificationManager.cpp
    UIProcess/API/C/WKNotificationPermissionRequest.cpp
    UIProcess/API/C/WKOpenPanelParametersRef.cpp
    UIProcess/API/C/WKOpenPanelResultListener.cpp
    UIProcess/API/C/WKPage.cpp
    UIProcess/API/C/WKPageConfigurationRef.cpp
    UIProcess/API/C/WKPageGroup.cpp
    UIProcess/API/C/WKPreferences.cpp
    UIProcess/API/C/WKProtectionSpace.cpp
    UIProcess/API/C/WKResourceCacheManager.cpp
    UIProcess/API/C/WKSessionStateRef.cpp
    UIProcess/API/C/WKTextChecker.cpp
    UIProcess/API/C/WKUserContentControllerRef.cpp
    UIProcess/API/C/WKUserMediaPermissionCheck.cpp
    UIProcess/API/C/WKUserMediaPermissionRequest.cpp
    UIProcess/API/C/WKViewportAttributes.cpp
    UIProcess/API/C/WKWebsiteDataStoreRef.cpp
    UIProcess/API/C/WKWebsitePolicies.cpp
    UIProcess/API/C/WKWindowFeaturesRef.cpp

    UIProcess/Authentication/AuthenticationChallengeProxy.cpp
    UIProcess/Authentication/AuthenticationDecisionListener.cpp
    UIProcess/Authentication/WebCredential.cpp
    UIProcess/Authentication/WebProtectionSpace.cpp

    UIProcess/Automation/SimulatedInputDispatcher.cpp
    UIProcess/Automation/WebAutomationSession.cpp

    UIProcess/Downloads/DownloadProxy.cpp
    UIProcess/Downloads/DownloadProxyMap.cpp

    UIProcess/Gamepad/UIGamepad.cpp
    UIProcess/Gamepad/UIGamepadProvider.cpp

    UIProcess/Launcher/ProcessLauncher.cpp

    UIProcess/Network/NetworkProcessProxy.cpp

    UIProcess/Notifications/NotificationPermissionRequestManagerProxy.cpp
    UIProcess/Notifications/WebNotification.cpp
    UIProcess/Notifications/WebNotificationManagerProxy.cpp
    UIProcess/Notifications/WebNotificationProvider.cpp

    UIProcess/Plugins/PlugInAutoStartProvider.cpp
    UIProcess/Plugins/PluginInfoStore.cpp
    UIProcess/Plugins/PluginProcessManager.cpp
    UIProcess/Plugins/PluginProcessProxy.cpp

    UIProcess/Storage/StorageProcessProxy.cpp

    UIProcess/UserContent/WebScriptMessageHandler.cpp
    UIProcess/UserContent/WebUserContentControllerProxy.cpp

    UIProcess/WebStorage/LocalStorageDatabase.cpp
    UIProcess/WebStorage/LocalStorageDatabaseTracker.cpp

    UIProcess/WebsiteData/WebsiteDataRecord.cpp
    UIProcess/WebsiteData/WebsiteDataStore.cpp

    WebProcess/WebConnectionToUIProcess.cpp
    WebProcess/WebProcess.cpp

    WebProcess/Automation/WebAutomationSessionProxy.cpp

    WebProcess/Cache/WebCacheStorageConnection.cpp
    WebProcess/Cache/WebCacheStorageProvider.cpp

    WebProcess/Databases/WebDatabaseProvider.cpp

    WebProcess/Databases/IndexedDB/WebIDBConnectionToServer.cpp

    WebProcess/FileAPI/BlobRegistryProxy.cpp

    WebProcess/FullScreen/WebFullScreenManager.cpp

    WebProcess/Gamepad/WebGamepad.cpp
    WebProcess/Gamepad/WebGamepadProvider.cpp

    WebProcess/Geolocation/GeolocationPermissionRequestManager.cpp
    WebProcess/Geolocation/WebGeolocationManager.cpp

    WebProcess/InjectedBundle/InjectedBundle.cpp
    WebProcess/InjectedBundle/InjectedBundleBackForwardList.cpp
    WebProcess/InjectedBundle/InjectedBundleBackForwardListItem.cpp
    WebProcess/InjectedBundle/InjectedBundleClient.cpp
    WebProcess/InjectedBundle/InjectedBundleDOMWindowExtension.cpp
    WebProcess/InjectedBundle/InjectedBundleHitTestResult.cpp
    WebProcess/InjectedBundle/InjectedBundleNavigationAction.cpp
    WebProcess/InjectedBundle/InjectedBundlePageContextMenuClient.cpp
    WebProcess/InjectedBundle/InjectedBundlePageEditorClient.cpp
    WebProcess/InjectedBundle/InjectedBundlePageFormClient.cpp
    WebProcess/InjectedBundle/InjectedBundlePageFullScreenClient.cpp
    WebProcess/InjectedBundle/InjectedBundlePageLoaderClient.cpp
    WebProcess/InjectedBundle/InjectedBundlePagePolicyClient.cpp
    WebProcess/InjectedBundle/InjectedBundlePageResourceLoadClient.cpp
    WebProcess/InjectedBundle/InjectedBundlePageUIClient.cpp
    WebProcess/InjectedBundle/InjectedBundleScriptWorld.cpp

    WebProcess/InjectedBundle/API/c/WKBundle.cpp
    WebProcess/InjectedBundle/API/c/WKBundleBackForwardList.cpp
    WebProcess/InjectedBundle/API/c/WKBundleBackForwardListItem.cpp
    WebProcess/InjectedBundle/API/c/WKBundleDOMWindowExtension.cpp
    WebProcess/InjectedBundle/API/c/WKBundleFrame.cpp
    WebProcess/InjectedBundle/API/c/WKBundleHitTestResult.cpp
    WebProcess/InjectedBundle/API/c/WKBundleInspector.cpp
    WebProcess/InjectedBundle/API/c/WKBundleNavigationAction.cpp
    WebProcess/InjectedBundle/API/c/WKBundleNodeHandle.cpp
    WebProcess/InjectedBundle/API/c/WKBundlePage.cpp
    WebProcess/InjectedBundle/API/c/WKBundlePageGroup.cpp
    WebProcess/InjectedBundle/API/c/WKBundlePageOverlay.cpp
    WebProcess/InjectedBundle/API/c/WKBundleRangeHandle.cpp
    WebProcess/InjectedBundle/API/c/WKBundleScriptWorld.cpp

    WebProcess/InjectedBundle/DOM/InjectedBundleCSSStyleDeclarationHandle.cpp
    WebProcess/InjectedBundle/DOM/InjectedBundleFileHandle.cpp
    WebProcess/InjectedBundle/DOM/InjectedBundleNodeHandle.cpp
    WebProcess/InjectedBundle/DOM/InjectedBundleRangeHandle.cpp

    WebProcess/MediaStream/MediaDeviceSandboxExtensions.cpp
    WebProcess/MediaStream/UserMediaPermissionRequestManager.cpp

    WebProcess/Network/NetworkProcessConnection.cpp
    WebProcess/Network/WebLoaderStrategy.cpp
    WebProcess/Network/WebResourceLoader.cpp
    WebProcess/Network/WebSocketProvider.cpp
    WebProcess/Network/WebSocketStream.cpp

    WebProcess/Network/webrtc/LibWebRTCProvider.cpp
    WebProcess/Network/webrtc/LibWebRTCResolver.cpp
    WebProcess/Network/webrtc/LibWebRTCSocket.cpp
    WebProcess/Network/webrtc/LibWebRTCSocketFactory.cpp
    WebProcess/Network/webrtc/WebMDNSRegister.cpp
    WebProcess/Network/webrtc/WebRTCMonitor.cpp
    WebProcess/Network/webrtc/WebRTCResolver.cpp
    WebProcess/Network/webrtc/WebRTCSocket.cpp

    WebProcess/Notifications/NotificationPermissionRequestManager.cpp
    WebProcess/Notifications/WebNotificationManager.cpp

    WebProcess/Plugins/Plugin.cpp
    WebProcess/Plugins/PluginProcessConnection.cpp
    WebProcess/Plugins/PluginProcessConnectionManager.cpp
    WebProcess/Plugins/PluginProxy.cpp
    WebProcess/Plugins/PluginView.cpp
    WebProcess/Plugins/WebPluginInfoProvider.cpp

    WebProcess/Plugins/Netscape/JSNPMethod.cpp
    WebProcess/Plugins/Netscape/JSNPObject.cpp
    WebProcess/Plugins/Netscape/NPJSObject.cpp
    WebProcess/Plugins/Netscape/NPRuntimeObjectMap.cpp
    WebProcess/Plugins/Netscape/NPRuntimeUtilities.cpp
    WebProcess/Plugins/Netscape/NetscapeBrowserFuncs.cpp
    WebProcess/Plugins/Netscape/NetscapePlugin.cpp
    WebProcess/Plugins/Netscape/NetscapePluginNone.cpp
    WebProcess/Plugins/Netscape/NetscapePluginStream.cpp

    WebProcess/Storage/ServiceWorkerClientFetch.cpp
    WebProcess/Storage/WebSWClientConnection.cpp
    WebProcess/Storage/WebSWContextManagerConnection.cpp
    WebProcess/Storage/WebSWOriginTable.cpp
    WebProcess/Storage/WebServiceWorkerFetchTaskClient.cpp
    WebProcess/Storage/WebServiceWorkerProvider.cpp
    WebProcess/Storage/WebToStorageProcessConnection.cpp

    WebProcess/UserContent/WebUserContentController.cpp

    WebProcess/WebCoreSupport/SessionStateConversion.cpp
    WebProcess/WebCoreSupport/WebChromeClient.cpp
    WebProcess/WebCoreSupport/WebColorChooser.cpp
    WebProcess/WebCoreSupport/WebContextMenuClient.cpp
    WebProcess/WebCoreSupport/WebDiagnosticLoggingClient.cpp
    WebProcess/WebCoreSupport/WebDragClient.cpp
    WebProcess/WebCoreSupport/WebEditorClient.cpp
    WebProcess/WebCoreSupport/WebFrameLoaderClient.cpp
    WebProcess/WebCoreSupport/WebGeolocationClient.cpp
    WebProcess/WebCoreSupport/WebInspectorClient.cpp
    WebProcess/WebCoreSupport/WebMessagePortChannelProvider.cpp
    WebProcess/WebCoreSupport/WebNotificationClient.cpp
    WebProcess/WebCoreSupport/WebPerformanceLoggingClient.cpp
    WebProcess/WebCoreSupport/WebPlatformStrategies.cpp
    WebProcess/WebCoreSupport/WebPlugInClient.cpp
    WebProcess/WebCoreSupport/WebPopupMenu.cpp
    WebProcess/WebCoreSupport/WebProgressTrackerClient.cpp
    WebProcess/WebCoreSupport/WebSearchPopupMenu.cpp
    WebProcess/WebCoreSupport/WebUserMediaClient.cpp

    WebProcess/WebPage/DrawingArea.cpp
    WebProcess/WebPage/EventDispatcher.cpp
    WebProcess/WebPage/FindController.cpp
    WebProcess/WebPage/LayerTreeHost.cpp
    WebProcess/WebPage/PageBanner.cpp
    WebProcess/WebPage/RemoteWebInspectorUI.cpp
    WebProcess/WebPage/VisitedLinkTableController.cpp
    WebProcess/WebPage/WebBackForwardListProxy.cpp
    WebProcess/WebPage/WebContextMenu.cpp
    WebProcess/WebPage/WebDocumentLoader.cpp
    WebProcess/WebPage/WebFrame.cpp
    WebProcess/WebPage/WebInspector.cpp
    WebProcess/WebPage/WebInspectorInterruptDispatcher.cpp
    WebProcess/WebPage/WebInspectorFrontendAPIDispatcher.cpp
    WebProcess/WebPage/WebInspectorUI.cpp
    WebProcess/WebPage/WebOpenPanelResultListener.cpp
    WebProcess/WebPage/WebPage.cpp
    WebProcess/WebPage/WebPageGroupProxy.cpp
    WebProcess/WebPage/WebPageOverlay.cpp
    WebProcess/WebPage/WebURLSchemeHandlerProxy.cpp
    WebProcess/WebPage/WebURLSchemeTaskProxy.cpp
    WebProcess/WebPage/WebUndoStep.cpp

    WebProcess/WebStorage/StorageAreaImpl.cpp
    WebProcess/WebStorage/StorageAreaMap.cpp
    WebProcess/WebStorage/StorageNamespaceImpl.cpp
    WebProcess/WebStorage/WebStorageNamespaceProvider.cpp
)

set(WebKit_MESSAGES_IN_FILES
    NetworkProcess/Cookies/WebCookieManager.messages.in

    NetworkProcess/NetworkConnectionToWebProcess.messages.in
    NetworkProcess/NetworkContentRuleListManager.messages.in
    NetworkProcess/NetworkProcess.messages.in
    NetworkProcess/NetworkResourceLoader.messages.in
    NetworkProcess/NetworkSocketStream.messages.in

    NetworkProcess/cache/CacheStorageEngineConnection.messages.in

    NetworkProcess/webrtc/NetworkMDNSRegister.messages.in
    NetworkProcess/webrtc/NetworkRTCMonitor.messages.in
    NetworkProcess/webrtc/NetworkRTCProvider.messages.in
    NetworkProcess/webrtc/NetworkRTCSocket.messages.in

    PluginProcess/PluginControllerProxy.messages.in
    PluginProcess/PluginProcess.messages.in
    PluginProcess/WebProcessConnection.messages.in

    Shared/ChildProcess.messages.in
    Shared/WebConnection.messages.in

    Shared/Authentication/AuthenticationManager.messages.in

    Shared/Plugins/NPObjectMessageReceiver.messages.in

    StorageProcess/StorageProcess.messages.in
    StorageProcess/StorageToWebProcessConnection.messages.in

    StorageProcess/IndexedDB/WebIDBConnectionToClient.messages.in

    StorageProcess/ServiceWorker/WebSWServerConnection.messages.in
    StorageProcess/ServiceWorker/WebSWServerToContextConnection.messages.in

    UIProcess/DrawingAreaProxy.messages.in
    UIProcess/RemoteWebInspectorProxy.messages.in
    UIProcess/VisitedLinkStore.messages.in
    UIProcess/WebCookieManagerProxy.messages.in
    UIProcess/WebFullScreenManagerProxy.messages.in
    UIProcess/WebGeolocationManagerProxy.messages.in
    UIProcess/WebInspectorProxy.messages.in
    UIProcess/WebPageProxy.messages.in
    UIProcess/WebPasteboardProxy.messages.in
    UIProcess/WebProcessPool.messages.in
    UIProcess/WebProcessProxy.messages.in
    UIProcess/WebResourceLoadStatisticsStore.messages.in

    UIProcess/Automation/WebAutomationSession.messages.in

    UIProcess/Downloads/DownloadProxy.messages.in

    UIProcess/Network/NetworkProcessProxy.messages.in

    UIProcess/Plugins/PluginProcessProxy.messages.in

    UIProcess/Storage/StorageProcessProxy.messages.in

    UIProcess/UserContent/WebUserContentControllerProxy.messages.in

    UIProcess/WebStorage/StorageManager.messages.in

    WebProcess/WebProcess.messages.in

    WebProcess/Automation/WebAutomationSessionProxy.messages.in

    WebProcess/Cache/WebCacheStorageConnection.messages.in

    WebProcess/Databases/IndexedDB/WebIDBConnectionToServer.messages.in

    WebProcess/FullScreen/WebFullScreenManager.messages.in

    WebProcess/Geolocation/WebGeolocationManager.messages.in

    WebProcess/Network/webrtc/WebMDNSRegister.messages.in
    WebProcess/Network/webrtc/WebRTCMonitor.messages.in
    WebProcess/Network/webrtc/WebRTCResolver.messages.in
    WebProcess/Network/webrtc/WebRTCSocket.messages.in

    WebProcess/Network/NetworkProcessConnection.messages.in
    WebProcess/Network/WebResourceLoader.messages.in
    WebProcess/Network/WebSocketStream.messages.in

    WebProcess/Notifications/WebNotificationManager.messages.in

    WebProcess/Plugins/PluginProcessConnection.messages.in
    WebProcess/Plugins/PluginProcessConnectionManager.messages.in
    WebProcess/Plugins/PluginProxy.messages.in

    WebProcess/Storage/ServiceWorkerClientFetch.messages.in
    WebProcess/Storage/WebSWClientConnection.messages.in
    WebProcess/Storage/WebSWContextManagerConnection.messages.in

    WebProcess/UserContent/WebUserContentController.messages.in

    WebProcess/WebPage/DrawingArea.messages.in
    WebProcess/WebPage/EventDispatcher.messages.in
    WebProcess/WebPage/RemoteWebInspectorUI.messages.in
    WebProcess/WebPage/VisitedLinkTableController.messages.in
    WebProcess/WebPage/WebInspector.messages.in
    WebProcess/WebPage/WebInspectorInterruptDispatcher.messages.in
    WebProcess/WebPage/WebInspectorUI.messages.in
    WebProcess/WebPage/WebPage.messages.in

    WebProcess/WebStorage/StorageAreaMap.messages.in
)

if (${WebCore_LIBRARY_TYPE} STREQUAL STATIC)
    # Link with WebCore as PRIVATE not to propagate WebCore to all users of WebKit.
    set(WebKit_LIBRARIES
        PRIVATE WebCore
        PUBLIC WebCoreHeaderInterface
    )
else ()
    # All users of WebKit need to link WebCore if WebCore is a shared library.
    set(WebKit_LIBRARIES PUBLIC WebCore)
endif ()

set(WebProcess_LIBRARIES
    WebKit
)

set(NetworkProcess_LIBRARIES
    WebKit
)

set(StorageProcess_LIBRARIES
    WebKit
)

set(PluginProcess_LIBRARIES
    WebKit
)

if (ENABLE_GRAPHICS_CONTEXT_3D)
    # For platforms that want to use system-provided OpenGL (ES) / EGL headers,
    # these include directories, libraries or definitions need to be
    # added before the ANGLE directories.
    if (USE_LIBEPOXY)
        list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
            ${LIBEPOXY_INCLUDE_DIRS}
        )
        list(APPEND WebKit_LIBRARIES
            PRIVATE ${LIBEPOXY_LIBRARIES}
        )
        add_definitions(${LIBEPOXY_DEFINITIONS})
    else ()
        if (USE_OPENGL)
            list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
                ${OPENGL_INCLUDE_DIRS}
            )
            list(APPEND WebKit_LIBRARIES
                PRIVATE ${OPENGL_LIBRARIES}
            )
            add_definitions(${OPENGL_DEFINITIONS})
        elseif (USE_OPENGL_ES)
            list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
                ${OPENGLES2_INCLUDE_DIRS}
            )
            list(APPEND WebKit_LIBRARIES
                PRIVATE ${OPENGLES2_LIBRARIES}
            )
            add_definitions(${OPENGLES2_DEFINITIONS})
        endif ()

        if (USE_EGL)
            list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
                ${EGL_INCLUDE_DIRS}
            )
            list(APPEND WebKit_LIBRARIES
                PRIVATE ${EGL_LIBRARIES}
            )
            add_definitions(${EGL_DEFINITIONS})
        endif ()
    endif ()

    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${THIRDPARTY_DIR}/ANGLE"
        "${THIRDPARTY_DIR}/ANGLE/include/KHR"
    )
endif ()

if (WIN32 AND INTERNAL_BUILD)
    set(JavaScriptCore_SCRIPTS_DIR "${CMAKE_BINARY_DIR}/../include/private/JavaScriptCore/Scripts")
else ()
    set(JavaScriptCore_SCRIPTS_DIR "${FORWARDING_HEADERS_DIR}/JavaScriptCore/Scripts")
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    set(ATOMIC_TEST_SOURCE
    "
        #include <atomic>
        int main() { std::atomic<int64_t> i(0); i++; return 0; }
    "
    )
    check_cxx_source_compiles("${ATOMIC_TEST_SOURCE}" ATOMIC_INT64_IS_BUILTIN)
    if (NOT ATOMIC_INT64_IS_BUILTIN)
        set(CMAKE_REQUIRED_LIBRARIES atomic)
        check_cxx_source_compiles("${ATOMIC_TEST_SOURCE}" ATOMIC_INT64_REQUIRES_LIBATOMIC)
        if (ATOMIC_INT64_REQUIRES_LIBATOMIC)
            list(APPEND WebKit_LIBRARIES PRIVATE atomic)
        endif ()
        unset(CMAKE_REQUIRED_LIBRARIES)
    endif ()
endif ()

if (UNIX)
    check_function_exists(shm_open SHM_OPEN_EXISTS)
    if (NOT SHM_OPEN_EXISTS)
        set(CMAKE_REQUIRED_LIBRARIES rt)
        check_function_exists(shm_open SHM_OPEN_REQUIRES_LIBRT)
        if (SHM_OPEN_REQUIRES_LIBRT)
            list(APPEND WebKit_LIBRARIES PRIVATE rt)
        endif ()
        unset(CMAKE_REQUIRED_LIBRARIES)
    endif ()
endif ()

macro(ADD_WEBKIT_PREFIX_HEADER _target)
    if (WebKit_USE_PREFIX_HEADER)
        get_target_property(OLD_COMPILE_FLAGS ${_target} COMPILE_FLAGS)
        if (${OLD_COMPILE_FLAGS} STREQUAL "OLD_COMPILE_FLAGS-NOTFOUND")
            set(OLD_COMPILE_FLAGS "")
        endif ()
        set_target_properties(${_target} PROPERTIES COMPILE_FLAGS "${OLD_COMPILE_FLAGS} -include ${WEBKIT_DIR}/WebKit2Prefix.h")
    endif ()
endmacro()

set(WebKit_AUTOMATION_PROTOCOL_GENERATOR_EXTRA_FLAGS
)

WEBKIT_FRAMEWORK_DECLARE(WebKit)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

# Helper macro which wraps generate-message-receiver.py and generate-message-header.py scripts
#   _output_source is a list name which will contain generated sources.(eg. WebKit_SOURCES)
#   _input_files are messages.in files to generate.
macro(GENERATE_MESSAGE_SOURCES _output_source _input_files)
    foreach (_file ${_input_files})
        get_filename_component(_name ${_file} NAME_WE)
        add_custom_command(
            OUTPUT ${DERIVED_SOURCES_WEBKIT_DIR}/${_name}MessageReceiver.cpp ${DERIVED_SOURCES_WEBKIT_DIR}/${_name}Messages.h
            MAIN_DEPENDENCY ${_file}
            DEPENDS ${WEBKIT_DIR}/Scripts/webkit/__init__.py
                    ${WEBKIT_DIR}/Scripts/webkit/messages.py
                    ${WEBKIT_DIR}/Scripts/webkit/model.py
                    ${WEBKIT_DIR}/Scripts/webkit/parser.py
            COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-message-receiver.py ${_file} > ${DERIVED_SOURCES_WEBKIT_DIR}/${_name}MessageReceiver.cpp
            COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-messages-header.py ${_file} > ${DERIVED_SOURCES_WEBKIT_DIR}/${_name}Messages.h
            WORKING_DIRECTORY ${WEBKIT_DIR}
            VERBATIM)

        list(APPEND ${_output_source} ${DERIVED_SOURCES_WEBKIT_DIR}/${_name}MessageReceiver.cpp)
    endforeach ()
endmacro()
GENERATE_MESSAGE_SOURCES(WebKit_DERIVED_SOURCES "${WebKit_MESSAGES_IN_FILES}")

set(WebKit_AUTOMATION_PROTOCOL_GENERATOR_SCRIPTS
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/generate-inspector-protocol-bindings.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/cpp_generator.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/cpp_generator_templates.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_backend_dispatcher_header.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_backend_dispatcher_implementation.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_frontend_dispatcher_header.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_frontend_dispatcher_implementation.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_protocol_types_header.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_protocol_types_implementation.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generator.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generator_templates.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/models.py
)

set(WebKit_AUTOMATION_PROTOCOL_GENERATOR_INPUTS
    ${WEBKIT_DIR}/UIProcess/Automation/Automation.json
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT_DIR}/AutomationBackendDispatchers.h ${DERIVED_SOURCES_WEBKIT_DIR}/AutomationBackendDispatchers.cpp ${DERIVED_SOURCES_WEBKIT_DIR}/AutomationProtocolObjects.h ${DERIVED_SOURCES_WEBKIT_DIR}/AutomationProtocolObjects.cpp ${DERIVED_SOURCES_WEBKIT_DIR}/AutomationFrontendDispatchers.h ${DERIVED_SOURCES_WEBKIT_DIR}/AutomationFrontendDispatchers.cpp
    MAIN_DEPENDENCY ${WebKit_AUTOMATION_PROTOCOL_GENERATOR_INPUTS}
    DEPENDS ${WebKit_AUTOMATION_PROTOCOL_GENERATOR_SCRIPTS}
    COMMAND ${PYTHON_EXECUTABLE} ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/generate-inspector-protocol-bindings.py --outputDir "${DERIVED_SOURCES_WEBKIT_DIR}" --framework WebKit ${WebKit_AUTOMATION_PROTOCOL_GENERATOR_EXTRA_FLAGS} --backend ${WebKit_AUTOMATION_PROTOCOL_GENERATOR_INPUTS}
    VERBATIM)

list(APPEND WebKit_HEADERS
    ${DERIVED_SOURCES_WEBKIT_DIR}/AutomationBackendDispatchers.h
    ${DERIVED_SOURCES_WEBKIT_DIR}/AutomationFrontendDispatchers.h
    ${DERIVED_SOURCES_WEBKIT_DIR}/AutomationProtocolObjects.h
)

list(APPEND WebKit_SOURCES
    ${DERIVED_SOURCES_WEBKIT_DIR}/AutomationBackendDispatchers.cpp
    ${DERIVED_SOURCES_WEBKIT_DIR}/AutomationFrontendDispatchers.cpp
    ${DERIVED_SOURCES_WEBKIT_DIR}/AutomationProtocolObjects.cpp
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT_DIR}/WebAutomationSessionProxyScriptSource.h ${DERIVED_SOURCES_WEBKIT_DIR}/WebAutomationSessionProxy.min.js
    MAIN_DEPENDENCY ${WEBKIT_DIR}/WebProcess/Automation/WebAutomationSessionProxy.js
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py
    COMMAND ${CMAKE_COMMAND} -E echo "//# sourceURL=__InjectedScript_WebAutomationSessionProxy.js" > ${DERIVED_SOURCES_WEBKIT_DIR}/WebAutomationSessionProxy.min.js
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py < ${WEBKIT_DIR}/WebProcess/Automation/WebAutomationSessionProxy.js >> ${DERIVED_SOURCES_WEBKIT_DIR}/WebAutomationSessionProxy.min.js
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl WebAutomationSessionProxyScriptSource ${DERIVED_SOURCES_WEBKIT_DIR}/WebAutomationSessionProxy.min.js ${DERIVED_SOURCES_WEBKIT_DIR}/WebAutomationSessionProxyScriptSource.h
    VERBATIM)

list(APPEND WebKit_HEADERS
    ${DERIVED_SOURCES_WEBKIT_DIR}/WebAutomationSessionProxyScriptSource.h
)

set(WebKit_WEB_PREFERENCES_TEMPLATES
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPageUpdatePreferences.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesDefinitions.h.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesExperimentalFeatures.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesKeys.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesKeys.h.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesStoreDefaultsMap.cpp.erb
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT_DIR}/WebPageUpdatePreferences.cpp ${DERIVED_SOURCES_WEBKIT_DIR}/WebPreferencesDefinitions.h ${DERIVED_SOURCES_WEBKIT_DIR}/WebPreferencesExperimentalFeatures.cpp ${DERIVED_SOURCES_WEBKIT_DIR}/WebPreferencesKeys.cpp ${DERIVED_SOURCES_WEBKIT_DIR}/WebPreferencesKeys.h ${DERIVED_SOURCES_WEBKIT_DIR}/WebPreferencesStoreDefaultsMap.cpp
    MAIN_DEPENDENCY ${WEBKIT_DIR}/Shared/WebPreferences.yaml
    DEPENDS ${WebKit_WEB_PREFERENCES_TEMPLATES}
    COMMAND ${RUBY_EXECUTABLE} ${WEBKIT_DIR}/Scripts/GeneratePreferences.rb --input ${WEBKIT_DIR}/Shared/WebPreferences.yaml --outputDir "${DERIVED_SOURCES_WEBKIT_DIR}"
    VERBATIM)

list(APPEND WebKit_HEADERS
    ${DERIVED_SOURCES_WEBKIT_DIR}/WebPreferencesDefinitions.h
    ${DERIVED_SOURCES_WEBKIT_DIR}/WebPreferencesKeys.h
)

list(APPEND WebKit_SOURCES
    ${DERIVED_SOURCES_WEBKIT_DIR}/WebPageUpdatePreferences.cpp
    ${DERIVED_SOURCES_WEBKIT_DIR}/WebPreferencesExperimentalFeatures.cpp
    ${DERIVED_SOURCES_WEBKIT_DIR}/WebPreferencesKeys.cpp
    ${DERIVED_SOURCES_WEBKIT_DIR}/WebPreferencesStoreDefaultsMap.cpp
)


list(APPEND WebKit_SOURCES ${WebKit_DERIVED_SOURCES})

if ("${PORT}" STREQUAL "GTK" OR "${PORT}" STREQUAL "WPE")
    WEBKIT_COMPUTE_SOURCES(WebKit)
endif ()

WEBKIT_FRAMEWORK(WebKit)
add_dependencies(WebKit WebCore ${WEBKIT_EXTRA_DEPENDENCIES})
ADD_WEBKIT_PREFIX_HEADER(WebKit)

add_executable(WebProcess ${WebProcess_SOURCES})
ADD_WEBKIT_PREFIX_HEADER(WebProcess)
target_link_libraries(WebProcess ${WebProcess_LIBRARIES})

if (WebKit_WebProcess_OUTPUT_NAME)
    set_target_properties(WebProcess PROPERTIES OUTPUT_NAME ${WebKit_WebProcess_OUTPUT_NAME})
endif ()

add_executable(NetworkProcess ${NetworkProcess_SOURCES})
ADD_WEBKIT_PREFIX_HEADER(NetworkProcess)
target_link_libraries(NetworkProcess ${NetworkProcess_LIBRARIES})

if (WebKit_NetworkProcess_OUTPUT_NAME)
    set_target_properties(NetworkProcess PROPERTIES OUTPUT_NAME ${WebKit_NetworkProcess_OUTPUT_NAME})
endif ()

add_executable(StorageProcess ${StorageProcess_SOURCES})
ADD_WEBKIT_PREFIX_HEADER(StorageProcess)
target_link_libraries(StorageProcess ${StorageProcess_LIBRARIES})
install(TARGETS StorageProcess DESTINATION "${LIBEXEC_INSTALL_DIR}")

if (WebKit_StorageProcess_OUTPUT_NAME)
    set_target_properties(StorageProcess PROPERTIES OUTPUT_NAME ${WebKit_StorageProcess_OUTPUT_NAME})
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebKit -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebProcess -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(NetworkProcess -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(StorageProcess -Wno-unused-parameter)
endif ()

# FIXME: Mac should build a plugin process.
if (ENABLE_PLUGIN_PROCESS AND NOT "${PORT}" STREQUAL "Mac")
    add_definitions(-DENABLE_PLUGIN_PROCESS=1)
    add_executable(PluginProcess ${PluginProcess_SOURCES})
    ADD_WEBKIT_PREFIX_HEADER(PluginProcess)
    target_link_libraries(PluginProcess ${PluginProcess_LIBRARIES})
    install(TARGETS PluginProcess DESTINATION "${LIBEXEC_INSTALL_DIR}")

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(PluginProcess -Wno-unused-parameter)
    endif ()

    if (WebKit_PluginProcess_OUTPUT_NAME)
      set_target_properties(PluginProcess PROPERTIES OUTPUT_NAME ${WebKit_PluginProcess_OUTPUT_NAME})
    endif ()
endif ()

if ("${PORT}" STREQUAL "Mac")
    WEBKIT_DEFINE_XPC_SERVICES()
else ()
    WEBKIT_POPULATE_LIBRARY_VERSION(WEBKIT)

    set_target_properties(WebKit PROPERTIES VERSION ${WEBKIT_VERSION} SOVERSION ${WEBKIT_VERSION_MAJOR})

    install(TARGETS WebKit WebProcess NetworkProcess
        LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
        RUNTIME DESTINATION "${LIBEXEC_INSTALL_DIR}"
    )
endif ()
