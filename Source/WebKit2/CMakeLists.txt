set(WebKit2_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}/llint"
    "${WEBKIT2_DIR}"
    "${WEBKIT2_DIR}/NetworkProcess"
    "${WEBKIT2_DIR}/NetworkProcess/FileAPI"
    "${WEBKIT2_DIR}/Platform"
    "${WEBKIT2_DIR}/Platform/IPC"
    "${WEBKIT2_DIR}/PluginProcess"
    "${WEBKIT2_DIR}/Shared"
    "${WEBKIT2_DIR}/Shared/API"
    "${WEBKIT2_DIR}/Shared/API/c"
    "${WEBKIT2_DIR}/Shared/Authentication"
    "${WEBKIT2_DIR}/Shared/CoreIPCSupport"
    "${WEBKIT2_DIR}/Shared/Databases/IndexedDB"
    "${WEBKIT2_DIR}/Shared/Downloads"
    "${WEBKIT2_DIR}/Shared/FileAPI"
    "${WEBKIT2_DIR}/Shared/Network"
    "${WEBKIT2_DIR}/Shared/Network/CustomProtocols"
    "${WEBKIT2_DIR}/Shared/Plugins"
    "${WEBKIT2_DIR}/Shared/Plugins/Netscape"
    "${WEBKIT2_DIR}/Shared/Plugins/Netscape/x11"
    "${WEBKIT2_DIR}/UIProcess"
    "${WEBKIT2_DIR}/UIProcess/API"
    "${WEBKIT2_DIR}/UIProcess/API/C"
    "${WEBKIT2_DIR}/UIProcess/API/cpp"
    "${WEBKIT2_DIR}/UIProcess/Authentication"
    "${WEBKIT2_DIR}/UIProcess/Downloads"
    "${WEBKIT2_DIR}/UIProcess/InspectorServer"
    "${WEBKIT2_DIR}/UIProcess/Launcher"
    "${WEBKIT2_DIR}/UIProcess/Network"
    "${WEBKIT2_DIR}/UIProcess/Network/CustomProtocols"
    "${WEBKIT2_DIR}/UIProcess/Notifications"
    "${WEBKIT2_DIR}/UIProcess/Plugins"
    "${WEBKIT2_DIR}/UIProcess/Storage"
    "${WEBKIT2_DIR}/WebProcess"
    "${WEBKIT2_DIR}/WebProcess/ApplicationCache"
    "${WEBKIT2_DIR}/WebProcess/Battery"
    "${WEBKIT2_DIR}/WebProcess/Cookies"
    "${WEBKIT2_DIR}/WebProcess/Databases"
    "${WEBKIT2_DIR}/WebProcess/Databases/IndexedDB"
    "${WEBKIT2_DIR}/WebProcess/FileAPI"
    "${WEBKIT2_DIR}/WebProcess/FullScreen"
    "${WEBKIT2_DIR}/WebProcess/Geolocation"
    "${WEBKIT2_DIR}/WebProcess/NetworkInfo"
    "${WEBKIT2_DIR}/WebProcess/IconDatabase"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/DOM"
    "${WEBKIT2_DIR}/WebProcess/Launching"
    "${WEBKIT2_DIR}/WebProcess/MediaCache"
    "${WEBKIT2_DIR}/WebProcess/Network"
    "${WEBKIT2_DIR}/WebProcess/Notifications"
    "${WEBKIT2_DIR}/WebProcess/OriginData"
    "${WEBKIT2_DIR}/WebProcess/Plugins"
    "${WEBKIT2_DIR}/WebProcess/Plugins/Netscape"
    "${WEBKIT2_DIR}/WebProcess/ResourceCache"
    "${WEBKIT2_DIR}/WebProcess/Storage"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport"
    "${WEBKIT2_DIR}/WebProcess/WebPage"
    "${WEBCORE_DIR}"
    "${WEBCORE_DIR}/Modules/battery"
    "${WEBCORE_DIR}/Modules/mediastream"
    "${WEBCORE_DIR}/Modules/networkinfo"
    "${WEBCORE_DIR}/Modules/notifications"
    "${WEBCORE_DIR}/Modules/vibration"
    "${WEBCORE_DIR}/Modules/webdatabase"
    "${WEBCORE_DIR}/accessibility"
    "${WEBCORE_DIR}/bindings/js"
    "${WEBCORE_DIR}/bindings"
    "${WEBCORE_DIR}/bridge"
    "${WEBCORE_DIR}/bridge/jsc"
    "${WEBCORE_DIR}/css"
    "${WEBCORE_DIR}/dom"
    "${WEBCORE_DIR}/dom/default"
    "${WEBCORE_DIR}/editing"
    "${WEBCORE_DIR}/fileapi"
    "${WEBCORE_DIR}/history"
    "${WEBCORE_DIR}/html"
    "${WEBCORE_DIR}/html/shadow"
    "${WEBCORE_DIR}/html/track"
    "${WEBCORE_DIR}/inspector"
    "${WEBCORE_DIR}/loader"
    "${WEBCORE_DIR}/loader/archive"
    "${WEBCORE_DIR}/loader/icon"
    "${WEBCORE_DIR}/loader/cache"
    "${WEBCORE_DIR}/page"
    "${WEBCORE_DIR}/page/animation"
    "${WEBCORE_DIR}/page/scrolling"
    "${WEBCORE_DIR}/platform"
    "${WEBCORE_DIR}/platform/animation"
    "${WEBCORE_DIR}/platform/audio"
    "${WEBCORE_DIR}/platform/graphics"
    "${WEBCORE_DIR}/platform/graphics/filters"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz/ng"
    "${WEBCORE_DIR}/platform/graphics/surfaces"
    "${WEBCORE_DIR}/platform/graphics/texmap"
    "${WEBCORE_DIR}/platform/graphics/transforms"
    "${WEBCORE_DIR}/platform/mediastream"
    "${WEBCORE_DIR}/platform/network"
    "${WEBCORE_DIR}/platform/sql"
    "${WEBCORE_DIR}/platform/text"
    "${WEBCORE_DIR}/plugins"
    "${WEBCORE_DIR}/rendering"
    "${WEBCORE_DIR}/rendering/line"
    "${WEBCORE_DIR}/rendering/shapes"
    "${WEBCORE_DIR}/rendering/style"
    "${WEBCORE_DIR}/storage"
    "${WEBCORE_DIR}/style"
    "${WEBCORE_DIR}/svg"
    "${WEBCORE_DIR}/svg/graphics"
    "${WEBCORE_DIR}/svg/properties"
    "${JAVASCRIPTCORE_DIR}"
    "${JAVASCRIPTCORE_DIR}/ForwardingHeaders"
    "${JAVASCRIPTCORE_DIR}/API"
    "${JAVASCRIPTCORE_DIR}/assembler"
    "${JAVASCRIPTCORE_DIR}/bytecode"
    "${JAVASCRIPTCORE_DIR}/bytecompiler"
    "${JAVASCRIPTCORE_DIR}/collector/handles"
    "${JAVASCRIPTCORE_DIR}/dfg"
    "${JAVASCRIPTCORE_DIR}/disassembler"
    "${JAVASCRIPTCORE_DIR}/heap"
    "${JAVASCRIPTCORE_DIR}/interpreter"
    "${JAVASCRIPTCORE_DIR}/jit"
    "${JAVASCRIPTCORE_DIR}/parser"
    "${JAVASCRIPTCORE_DIR}/profiler"
    "${JAVASCRIPTCORE_DIR}/runtime"
    "${WTF_DIR}"
    "${DERIVED_SOURCES_DIR}"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}"
    "${DERIVED_SOURCES_WEBCORE_DIR}"
    "${DERIVED_SOURCES_WEBKIT2_DIR}"
    "${DERIVED_SOURCES_WEBKIT2_DIR}/include"
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}/Source"
)

if (WTF_USE_3D_GRAPHICS)
    list(APPEND WebKit2_INCLUDE_DIRECTORIES
        "${THIRDPARTY_DIR}/ANGLE/include/KHR"
        "${THIRDPARTY_DIR}/ANGLE/include/GLSLANG"
    )
endif ()

set(WebKit2_SOURCES
    NetworkProcess/AsynchronousNetworkLoaderClient.cpp
    NetworkProcess/HostRecord.cpp
    NetworkProcess/NetworkConnectionToWebProcess.cpp
    NetworkProcess/NetworkProcess.cpp
    NetworkProcess/NetworkProcessPlatformStrategies.cpp
    NetworkProcess/NetworkResourceLoadScheduler.cpp
    NetworkProcess/NetworkResourceLoader.cpp
    NetworkProcess/SynchronousNetworkLoaderClient.cpp

    NetworkProcess/FileAPI/NetworkBlobRegistry.cpp

    Platform/Logging.cpp
    Platform/Module.cpp
    Platform/WorkQueue.cpp

    Platform/IPC/ArgumentCoders.cpp
    Platform/IPC/ArgumentDecoder.cpp
    Platform/IPC/ArgumentEncoder.cpp
    Platform/IPC/Attachment.cpp
    Platform/IPC/Connection.cpp
    Platform/IPC/DataReference.cpp
    Platform/IPC/MessageDecoder.cpp
    Platform/IPC/MessageEncoder.cpp
    Platform/IPC/MessageReceiverMap.cpp
    Platform/IPC/MessageSender.cpp
    Platform/IPC/StringReference.cpp

    PluginProcess/PluginControllerProxy.cpp
    PluginProcess/PluginCreationParameters.cpp
    PluginProcess/PluginProcess.cpp
    PluginProcess/WebProcessConnection.cpp

    Shared/APIArray.cpp
    Shared/APIData.cpp
    Shared/APIError.cpp
    Shared/APIFrameHandle.cpp
    Shared/APIGeometry.cpp
    Shared/APIPageHandle.cpp
    Shared/APIObject.cpp
    Shared/APIURLRequest.cpp
    Shared/APIURLResponse.cpp
    Shared/ActivityAssertion.cpp
    Shared/CacheModel.cpp
    Shared/ChildProcess.cpp
    Shared/ChildProcessProxy.cpp
    Shared/ConnectionStack.cpp
    Shared/ContextMenuContextData.cpp
    Shared/DictionaryPopupInfo.cpp
    Shared/EditorState.cpp
    Shared/FontInfo.cpp
    Shared/ImmutableDictionary.cpp
    Shared/MutableDictionary.cpp
    Shared/NavigationActionData.cpp
    Shared/OriginAndDatabases.cpp
    Shared/PlatformPopupMenuData.cpp
    Shared/PrintInfo.cpp
    Shared/SecurityOriginData.cpp
    Shared/SessionState.cpp
    Shared/SessionTracker.cpp
    Shared/ShareableBitmap.cpp
    Shared/ShareableResource.cpp
    Shared/StatisticsData.cpp
    Shared/UpdateInfo.cpp
    Shared/UserData.cpp
    Shared/VisitedLinkTable.cpp
    Shared/WebBackForwardListItem.cpp
    Shared/WebBatteryStatus.cpp
    Shared/WebConnection.cpp
    Shared/WebConnectionClient.cpp
    Shared/WebContextMenuItem.cpp
    Shared/WebContextMenuItemData.cpp
    Shared/WebCoreArgumentCoders.cpp
    Shared/WebEvent.cpp
    Shared/WebEventConversion.cpp
    Shared/WebGeolocationPosition.cpp
    Shared/WebHitTestResult.cpp
    Shared/WebImage.cpp
    Shared/WebKeyboardEvent.cpp
    Shared/WebKit2Initialize.cpp
    Shared/WebMemorySampler.cpp
    Shared/WebMouseEvent.cpp
    Shared/WebNetworkInfo.cpp
    Shared/WebOpenPanelParameters.cpp
    Shared/WebPageCreationParameters.cpp
    Shared/WebPageGroupData.cpp
    Shared/WebPlatformTouchPoint.cpp
    Shared/WebPopupItem.cpp
    Shared/WebPreferencesStore.cpp
    Shared/WebProcessCreationParameters.cpp
    Shared/WebRenderLayer.cpp
    Shared/WebRenderObject.cpp
    Shared/WebResourceBuffer.cpp
    Shared/WebTouchEvent.cpp
    Shared/WebWheelEvent.cpp

    Shared/API/c/WKArray.cpp
    Shared/API/c/WKCertificateInfo.cpp
    Shared/API/c/WKConnectionRef.cpp
    Shared/API/c/WKContextMenuItem.cpp
    Shared/API/c/WKData.cpp
    Shared/API/c/WKDeprecatedFunctions.cpp
    Shared/API/c/WKDictionary.cpp
    Shared/API/c/WKError.cpp
    Shared/API/c/WKGeometry.cpp
    Shared/API/c/WKImage.cpp
    Shared/API/c/WKMutableArray.cpp
    Shared/API/c/WKMutableDictionary.cpp
    Shared/API/c/WKNumber.cpp
    Shared/API/c/WKPluginInformation.cpp
    Shared/API/c/WKRenderLayer.cpp
    Shared/API/c/WKRenderObject.cpp
    Shared/API/c/WKSecurityOrigin.cpp
    Shared/API/c/WKSerializedScriptValue.cpp
    Shared/API/c/WKString.cpp
    Shared/API/c/WKType.cpp
    Shared/API/c/WKURL.cpp
    Shared/API/c/WKURLRequest.cpp
    Shared/API/c/WKURLResponse.cpp
    Shared/API/c/WKUserContentURLPattern.cpp

    Shared/Authentication/AuthenticationManager.cpp

    Shared/Downloads/Download.cpp
    Shared/Downloads/DownloadAuthenticationClient.cpp
    Shared/Downloads/DownloadManager.cpp

    Shared/FileAPI/BlobRegistrationData.cpp

    Shared/Network/NetworkProcessCreationParameters.cpp
    Shared/Network/NetworkResourceLoadParameters.cpp

    Shared/Plugins/NPIdentifierData.cpp
    Shared/Plugins/NPObjectMessageReceiver.cpp
    Shared/Plugins/NPObjectProxy.cpp
    Shared/Plugins/NPRemoteObjectMap.cpp
    Shared/Plugins/NPVariantData.cpp
    Shared/Plugins/PluginModuleInfo.cpp
    Shared/Plugins/PluginProcessCreationParameters.cpp

    Shared/Plugins/Netscape/NetscapePluginModule.cpp
    Shared/Plugins/Netscape/NetscapePluginModuleNone.cpp
    Shared/Plugins/Netscape/PluginInformation.cpp

    UIProcess/APINavigationData.cpp
    UIProcess/APISession.cpp
    UIProcess/BackingStore.cpp
    UIProcess/DrawingAreaProxy.cpp
    UIProcess/FindIndicator.cpp
    UIProcess/FrameLoadState.cpp
    UIProcess/GeolocationPermissionRequestManagerProxy.cpp
    UIProcess/GeolocationPermissionRequestProxy.cpp
    UIProcess/PageLoadState.cpp
    UIProcess/ResponsivenessTimer.cpp
    UIProcess/StatisticsRequest.cpp
    UIProcess/TextCheckerCompletion.cpp
    UIProcess/VisitedLinkProvider.cpp
    UIProcess/WebApplicationCacheManagerProxy.cpp
    UIProcess/WebBackForwardList.cpp
    UIProcess/WebBatteryManagerProxy.cpp
    UIProcess/WebBatteryProvider.cpp
    UIProcess/WebColorPicker.cpp
    UIProcess/WebConnectionToWebProcess.cpp
    UIProcess/WebContext.cpp
    UIProcess/WebContextClient.cpp
    UIProcess/WebContextConnectionClient.cpp
    UIProcess/WebContextInjectedBundleClient.cpp
    UIProcess/WebContextMenuProxy.cpp
    UIProcess/WebCookieManagerProxy.cpp
    UIProcess/WebCookieManagerProxyClient.cpp
    UIProcess/WebDatabaseManagerProxy.cpp
    UIProcess/WebDatabaseManagerProxyClient.cpp
    UIProcess/WebEditCommandProxy.cpp
    UIProcess/WebFindClient.cpp
    UIProcess/WebFormClient.cpp
    UIProcess/WebFormSubmissionListenerProxy.cpp
    UIProcess/WebFrameListenerProxy.cpp
    UIProcess/WebFramePolicyListenerProxy.cpp
    UIProcess/WebFrameProxy.cpp
    UIProcess/WebFullScreenManagerProxy.cpp
    UIProcess/WebGeolocationManagerProxy.cpp
    UIProcess/WebGeolocationProvider.cpp
    UIProcess/WebGrammarDetail.cpp
    UIProcess/WebIconDatabase.cpp
    UIProcess/WebIconDatabaseClient.cpp
    UIProcess/WebInspectorProxy.cpp
    UIProcess/WebKeyValueStorageManager.cpp
    UIProcess/WebMediaCacheManagerProxy.cpp
    UIProcess/WebNetworkInfoManagerProxy.cpp
    UIProcess/WebNetworkInfoProvider.cpp
    UIProcess/WebOpenPanelResultListenerProxy.cpp
    UIProcess/WebOriginDataManagerProxy.cpp
    UIProcess/WebOriginDataManagerProxyChangeClient.cpp
    UIProcess/WebPageContextMenuClient.cpp
    UIProcess/WebPageGroup.cpp
    UIProcess/WebPageProxy.cpp
    UIProcess/WebPreferences.cpp
    UIProcess/WebProcessProxy.cpp
    UIProcess/WebResourceCacheManagerProxy.cpp
    UIProcess/WebTextChecker.cpp
    UIProcess/WebTextCheckerClient.cpp
    UIProcess/WebVibrationProvider.cpp
    UIProcess/WebVibrationProxy.cpp
    UIProcess/WebViewportAttributes.cpp

    UIProcess/API/C/WKApplicationCacheManager.cpp
    UIProcess/API/C/WKAuthenticationChallenge.cpp
    UIProcess/API/C/WKAuthenticationDecisionListener.cpp
    UIProcess/API/C/WKBackForwardListRef.cpp
    UIProcess/API/C/WKBackForwardListItemRef.cpp
    UIProcess/API/C/WKBatteryManager.cpp
    UIProcess/API/C/WKBatteryStatus.cpp
    UIProcess/API/C/WKContext.cpp
    UIProcess/API/C/WKCookieManager.cpp
    UIProcess/API/C/WKCredential.cpp
    UIProcess/API/C/WKDatabaseManager.cpp
    UIProcess/API/C/WKDownload.cpp
    UIProcess/API/C/WKFormSubmissionListener.cpp
    UIProcess/API/C/WKFrame.cpp
    UIProcess/API/C/WKFramePolicyListener.cpp
    UIProcess/API/C/WKGeolocationManager.cpp
    UIProcess/API/C/WKGeolocationPermissionRequest.cpp
    UIProcess/API/C/WKGeolocationPosition.cpp
    UIProcess/API/C/WKGrammarDetail.cpp
    UIProcess/API/C/WKHitTestResult.cpp
    UIProcess/API/C/WKIconDatabase.cpp
    UIProcess/API/C/WKInspector.cpp
    UIProcess/API/C/WKKeyValueStorageManager.cpp
    UIProcess/API/C/WKMediaCacheManager.cpp
    UIProcess/API/C/WKNavigationDataRef.cpp
    UIProcess/API/C/WKNetworkInfo.cpp
    UIProcess/API/C/WKNetworkInfoManager.cpp
    UIProcess/API/C/WKNotification.cpp
    UIProcess/API/C/WKNotificationManager.cpp
    UIProcess/API/C/WKNotificationPermissionRequest.cpp
    UIProcess/API/C/WKOpenPanelParameters.cpp
    UIProcess/API/C/WKOpenPanelResultListener.cpp
    UIProcess/API/C/WKOriginDataManager.cpp
    UIProcess/API/C/WKPage.cpp
    UIProcess/API/C/WKPageGroup.cpp
    UIProcess/API/C/WKPluginSiteDataManager.cpp
    UIProcess/API/C/WKPreferences.cpp
    UIProcess/API/C/WKProtectionSpace.cpp
    UIProcess/API/C/WKResourceCacheManager.cpp
    UIProcess/API/C/WKTextChecker.cpp
    UIProcess/API/C/WKVibration.cpp
    UIProcess/API/C/WKViewportAttributes.cpp

    UIProcess/Authentication/AuthenticationChallengeProxy.cpp
    UIProcess/Authentication/AuthenticationDecisionListener.cpp
    UIProcess/Authentication/WebCredential.cpp
    UIProcess/Authentication/WebProtectionSpace.cpp

    UIProcess/Downloads/DownloadProxy.cpp
    UIProcess/Downloads/DownloadProxyMap.cpp

    UIProcess/InspectorServer/HTTPRequest.cpp
    UIProcess/InspectorServer/WebInspectorServer.cpp
    UIProcess/InspectorServer/WebSocketServer.cpp
    UIProcess/InspectorServer/WebSocketServerConnection.cpp

    UIProcess/Launcher/ProcessLauncher.cpp

    UIProcess/Network/NetworkProcessProxy.cpp

    UIProcess/Notifications/NotificationPermissionRequest.cpp
    UIProcess/Notifications/NotificationPermissionRequestManagerProxy.cpp
    UIProcess/Notifications/WebNotification.cpp
    UIProcess/Notifications/WebNotificationManagerProxy.cpp
    UIProcess/Notifications/WebNotificationProvider.cpp

    UIProcess/Plugins/PlugInAutoStartProvider.cpp
    UIProcess/Plugins/PluginInfoStore.cpp
    UIProcess/Plugins/PluginProcessManager.cpp
    UIProcess/Plugins/PluginProcessProxy.cpp
    UIProcess/Plugins/WebPluginSiteDataManager.cpp

    UIProcess/Storage/LocalStorageDatabase.cpp
    UIProcess/Storage/LocalStorageDatabaseTracker.cpp

    WebProcess/WebConnectionToUIProcess.cpp
    WebProcess/WebProcess.cpp

    WebProcess/ApplicationCache/WebApplicationCacheManager.cpp

    WebProcess/Battery/WebBatteryManager.cpp

    WebProcess/Cookies/WebCookieManager.cpp

    WebProcess/Databases/IndexedDB/WebIDBFactoryBackend.cpp

    WebProcess/FileAPI/BlobRegistryProxy.cpp

    WebProcess/FullScreen/WebFullScreenManager.cpp

    WebProcess/Geolocation/GeolocationPermissionRequestManager.cpp
    WebProcess/Geolocation/WebGeolocationManager.cpp

    WebProcess/IconDatabase/WebIconDatabaseProxy.cpp

    WebProcess/InjectedBundle/InjectedBundle.cpp
    WebProcess/InjectedBundle/InjectedBundleBackForwardList.cpp
    WebProcess/InjectedBundle/InjectedBundleBackForwardListItem.cpp
    WebProcess/InjectedBundle/InjectedBundleClient.cpp
    WebProcess/InjectedBundle/InjectedBundleDOMWindowExtension.cpp
    WebProcess/InjectedBundle/InjectedBundleHitTestResult.cpp
    WebProcess/InjectedBundle/InjectedBundleNavigationAction.cpp
    WebProcess/InjectedBundle/InjectedBundlePageContextMenuClient.cpp
    WebProcess/InjectedBundle/InjectedBundlePageDiagnosticLoggingClient.cpp
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

    WebProcess/InjectedBundle/DOM/InjectedBundleNodeHandle.cpp
    WebProcess/InjectedBundle/DOM/InjectedBundleRangeHandle.cpp

    WebProcess/MediaCache/WebMediaCacheManager.cpp

    WebProcess/Network/NetworkProcessConnection.cpp
    WebProcess/Network/WebResourceLoadScheduler.cpp
    WebProcess/Network/WebResourceLoader.cpp

    WebProcess/NetworkInfo/WebNetworkInfoManager.cpp

    WebProcess/Notifications/NotificationPermissionRequestManager.cpp
    WebProcess/Notifications/WebNotificationManager.cpp

    WebProcess/OriginData/WebOriginDataManager.cpp

    WebProcess/Plugins/Plugin.cpp
    WebProcess/Plugins/PluginProcessConnection.cpp
    WebProcess/Plugins/PluginProcessConnectionManager.cpp
    WebProcess/Plugins/PluginProxy.cpp
    WebProcess/Plugins/PluginView.cpp

    WebProcess/Plugins/Netscape/JSNPMethod.cpp
    WebProcess/Plugins/Netscape/JSNPObject.cpp
    WebProcess/Plugins/Netscape/NPJSObject.cpp
    WebProcess/Plugins/Netscape/NPRuntimeObjectMap.cpp
    WebProcess/Plugins/Netscape/NPRuntimeUtilities.cpp
    WebProcess/Plugins/Netscape/NetscapeBrowserFuncs.cpp
    WebProcess/Plugins/Netscape/NetscapePlugin.cpp
    WebProcess/Plugins/Netscape/NetscapePluginNone.cpp
    WebProcess/Plugins/Netscape/NetscapePluginStream.cpp

    WebProcess/ResourceCache/WebResourceCacheManager.cpp

    WebProcess/Storage/StorageAreaImpl.cpp
    WebProcess/Storage/StorageAreaMap.cpp
    WebProcess/Storage/StorageNamespaceImpl.cpp

    WebProcess/WebCoreSupport/WebBatteryClient.cpp
    WebProcess/WebCoreSupport/WebChromeClient.cpp
    WebProcess/WebCoreSupport/WebColorChooser.cpp
    WebProcess/WebCoreSupport/WebContextMenuClient.cpp
    WebProcess/WebCoreSupport/WebDatabaseManager.cpp
    WebProcess/WebCoreSupport/WebDeviceProximityClient.cpp
    WebProcess/WebCoreSupport/WebDragClient.cpp
    WebProcess/WebCoreSupport/WebEditorClient.cpp
    WebProcess/WebCoreSupport/WebFrameLoaderClient.cpp
    WebProcess/WebCoreSupport/WebGeolocationClient.cpp
    WebProcess/WebCoreSupport/WebInspectorClient.cpp
    WebProcess/WebCoreSupport/WebInspectorFrontendClient.cpp
    WebProcess/WebCoreSupport/WebNetworkInfoClient.cpp
    WebProcess/WebCoreSupport/WebNotificationClient.cpp
    WebProcess/WebCoreSupport/WebPlatformStrategies.cpp
    WebProcess/WebCoreSupport/WebPlugInClient.cpp
    WebProcess/WebCoreSupport/WebPopupMenu.cpp
    WebProcess/WebCoreSupport/WebProgressTrackerClient.cpp
    WebProcess/WebCoreSupport/WebSearchPopupMenu.cpp
    WebProcess/WebCoreSupport/WebVibrationClient.cpp

    WebProcess/WebPage/DecoderAdapter.cpp
    WebProcess/WebPage/DrawingArea.cpp
    WebProcess/WebPage/EncoderAdapter.cpp
    WebProcess/WebPage/EventDispatcher.cpp
    WebProcess/WebPage/FindController.cpp
    WebProcess/WebPage/LayerTreeHost.cpp
    WebProcess/WebPage/PageBanner.cpp
    WebProcess/WebPage/PageOverlay.cpp
    WebProcess/WebPage/TapHighlightController.cpp
    WebProcess/WebPage/WebBackForwardListProxy.cpp
    WebProcess/WebPage/WebContextMenu.cpp
    WebProcess/WebPage/WebDocumentLoader.cpp
    WebProcess/WebPage/WebFrame.cpp
    WebProcess/WebPage/WebInspector.cpp
    WebProcess/WebPage/WebOpenPanelResultListener.cpp
    WebProcess/WebPage/WebPage.cpp
    WebProcess/WebPage/WebPageGroupProxy.cpp
    WebProcess/WebPage/VisitedLinkTableController.cpp
    WebProcess/WebPage/WebUndoStep.cpp
)

set(WebKit2_MESSAGES_IN_FILES
    NetworkProcess/NetworkConnectionToWebProcess.messages.in
    NetworkProcess/NetworkProcess.messages.in
    NetworkProcess/NetworkResourceLoader.messages.in

    PluginProcess/PluginControllerProxy.messages.in
    PluginProcess/PluginProcess.messages.in
    PluginProcess/WebProcessConnection.messages.in

    Shared/WebConnection.messages.in

    Shared/Authentication/AuthenticationManager.messages.in

    Shared/Plugins/NPObjectMessageReceiver.messages.in

    Shared/Network/CustomProtocols/CustomProtocolManager.messages.in

    UIProcess/DrawingAreaProxy.messages.in
    UIProcess/VisitedLinkProvider.messages.in
    UIProcess/WebApplicationCacheManagerProxy.messages.in
    UIProcess/WebBatteryManagerProxy.messages.in
    UIProcess/WebContext.messages.in
    UIProcess/WebCookieManagerProxy.messages.in
    UIProcess/WebDatabaseManagerProxy.messages.in
    UIProcess/WebFullScreenManagerProxy.messages.in
    UIProcess/WebGeolocationManagerProxy.messages.in
    UIProcess/WebIconDatabase.messages.in
    UIProcess/WebInspectorProxy.messages.in
    UIProcess/WebMediaCacheManagerProxy.messages.in
    UIProcess/WebNetworkInfoManagerProxy.messages.in
    UIProcess/WebOriginDataManagerProxy.messages.in
    UIProcess/WebPageProxy.messages.in
    UIProcess/WebProcessProxy.messages.in
    UIProcess/WebResourceCacheManagerProxy.messages.in
    UIProcess/WebVibrationProxy.messages.in

    UIProcess/Downloads/DownloadProxy.messages.in

    UIProcess/Network/NetworkProcessProxy.messages.in

    UIProcess/Network/CustomProtocols/CustomProtocolManagerProxy.messages.in

    UIProcess/Plugins/PluginProcessProxy.messages.in

    UIProcess/Storage/StorageManager.messages.in

    WebProcess/WebProcess.messages.in

    WebProcess/ApplicationCache/WebApplicationCacheManager.messages.in

    WebProcess/Battery/WebBatteryManager.messages.in

    WebProcess/Cookies/WebCookieManager.messages.in

    WebProcess/FullScreen/WebFullScreenManager.messages.in

    WebProcess/Geolocation/WebGeolocationManager.messages.in

    WebProcess/IconDatabase/WebIconDatabaseProxy.messages.in

    WebProcess/MediaCache/WebMediaCacheManager.messages.in

    WebProcess/Network/NetworkProcessConnection.messages.in
    WebProcess/Network/WebResourceLoader.messages.in

    WebProcess/NetworkInfo/WebNetworkInfoManager.messages.in

    WebProcess/Notifications/WebNotificationManager.messages.in

    WebProcess/OriginData/WebOriginDataManager.messages.in

    WebProcess/Plugins/PluginProcessConnection.messages.in
    WebProcess/Plugins/PluginProcessConnectionManager.messages.in
    WebProcess/Plugins/PluginProxy.messages.in

    WebProcess/ResourceCache/WebResourceCacheManager.messages.in

    WebProcess/Storage/StorageAreaMap.messages.in

    WebProcess/WebCoreSupport/WebDatabaseManager.messages.in

    WebProcess/WebPage/DrawingArea.messages.in
    WebProcess/WebPage/EventDispatcher.messages.in
    WebProcess/WebPage/VisitedLinkTableController.messages.in
    WebProcess/WebPage/WebInspector.messages.in
    WebProcess/WebPage/WebPage.messages.in
    WebProcess/WebPage/WebPageGroupProxy.messages.in
)

set(WebProcess_LIBRARIES
    JavaScriptCore
    WebCore
    WebKit2
)

set(NetworkProcess_LIBRARIES
    WebKit2
)

set(WebKit2_LIBRARIES
    JavaScriptCore
    WTF
    WebCore
)

# librt is needed for shm_open on Linux.
find_library(LIBRT_LIBRARIES NAMES rt)
mark_as_advanced(LIBRT_LIBRARIES)
if (LIBRT_LIBRARIES)
    list(APPEND WebKit2_LIBRARIES ${LIBRT_LIBRARIES})
endif ()

macro(ADD_WEBKIT2_PREFIX_HEADER _target)
    if (WebKit2_USE_PREFIX_HEADER)
        get_target_property(OLD_COMPILE_FLAGS ${_target} COMPILE_FLAGS)
        if (${OLD_COMPILE_FLAGS} STREQUAL "OLD_COMPILE_FLAGS-NOTFOUND")
            set(OLD_COMPILE_FLAGS "")
        endif ()
        set_target_properties(${_target} PROPERTIES COMPILE_FLAGS "${OLD_COMPILE_FLAGS} -include ${WEBKIT2_DIR}/WebKit2Prefix.h")
    endif ()
endmacro()

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

GENERATE_WEBKIT2_MESSAGE_SOURCES(WebKit2_SOURCES "${WebKit2_MESSAGES_IN_FILES}")

add_definitions(-DBUILDING_WEBKIT)
include_directories(${WebKit2_INCLUDE_DIRECTORIES})
add_library(WebKit2 ${WebKit2_LIBRARY_TYPE} ${WebKit2_SOURCES})

add_dependencies(WebKit2 WebCore)
add_dependencies(WebKit2 ${WEBKIT2_EXTRA_DEPENDENCIES})
add_webkit2_prefix_header(WebKit2)
target_link_libraries(WebKit2 ${WebKit2_LIBRARIES})
set_target_properties(WebKit2 PROPERTIES FOLDER "WebKit")
set_target_properties(WebKit2 PROPERTIES LINK_INTERFACE_LIBRARIES "")

if (WebKit2_OUTPUT_NAME)
    set_target_properties(WebKit2 PROPERTIES OUTPUT_NAME ${WebKit2_OUTPUT_NAME})
endif ()

add_executable(WebProcess ${WebProcess_SOURCES})
add_webkit2_prefix_header(WebProcess)
target_link_libraries(WebProcess ${WebProcess_LIBRARIES})
set_target_properties(WebProcess PROPERTIES FOLDER "WebKit")

if (WebKit2_WebProcess_OUTPUT_NAME)
    set_target_properties(WebProcess PROPERTIES OUTPUT_NAME ${WebKit2_WebProcess_OUTPUT_NAME})
endif ()

if (ENABLE_NETWORK_PROCESS)
    add_executable(NetworkProcess ${NetworkProcess_SOURCES})
    add_webkit2_prefix_header(NetworkProcess)
    target_link_libraries(NetworkProcess ${NetworkProcess_LIBRARIES})
    set_target_properties(NetworkProcess PROPERTIES FOLDER "WebKit")
    install(TARGETS NetworkProcess DESTINATION "${LIBEXEC_INSTALL_DIR}")

    if (WebKit2_NetworkProcess_OUTPUT_NAME)
        set_target_properties(NetworkProcess PROPERTIES OUTPUT_NAME ${WebKit2_NetworkProcess_OUTPUT_NAME})
    endif ()
endif ()

POPULATE_LIBRARY_VERSION(WEBKIT2)
set_target_properties(WebKit2 PROPERTIES VERSION ${WEBKIT2_VERSION} SOVERSION ${WEBKIT2_VERSION_MAJOR})

install(TARGETS WebKit2 DESTINATION "${LIB_INSTALL_DIR}")
install(TARGETS WebProcess DESTINATION "${LIBEXEC_INSTALL_DIR}")
