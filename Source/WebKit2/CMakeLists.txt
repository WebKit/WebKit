set(WebKit2_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}/llint"
    "${WEBKIT2_DIR}"
    "${WEBKIT2_DIR}/DatabaseProcess"
    "${WEBKIT2_DIR}/DatabaseProcess/IndexedDB"
    "${WEBKIT2_DIR}/NetworkProcess"
    "${WEBKIT2_DIR}/NetworkProcess/CustomProtocols"
    "${WEBKIT2_DIR}/NetworkProcess/Downloads"
    "${WEBKIT2_DIR}/NetworkProcess/FileAPI"
    "${WEBKIT2_DIR}/NetworkProcess/cache"
    "${WEBKIT2_DIR}/Platform"
    "${WEBKIT2_DIR}/Platform/IPC"
    "${WEBKIT2_DIR}/PluginProcess"
    "${WEBKIT2_DIR}/Shared"
    "${WEBKIT2_DIR}/Shared/API"
    "${WEBKIT2_DIR}/Shared/API/c"
    "${WEBKIT2_DIR}/Shared/Authentication"
    "${WEBKIT2_DIR}/Shared/CoreIPCSupport"
    "${WEBKIT2_DIR}/Shared/Databases"
    "${WEBKIT2_DIR}/Shared/Databases/IndexedDB"
    "${WEBKIT2_DIR}/Shared/FileAPI"
    "${WEBKIT2_DIR}/Shared/Plugins"
    "${WEBKIT2_DIR}/Shared/Plugins/Netscape"
    "${WEBKIT2_DIR}/Shared/Plugins/Netscape/x11"
    "${WEBKIT2_DIR}/Shared/WebsiteData"
    "${WEBKIT2_DIR}/UIProcess"
    "${WEBKIT2_DIR}/UIProcess/API"
    "${WEBKIT2_DIR}/UIProcess/API/C"
    "${WEBKIT2_DIR}/UIProcess/API/cpp"
    "${WEBKIT2_DIR}/UIProcess/Authentication"
    "${WEBKIT2_DIR}/UIProcess/Automation"
    "${WEBKIT2_DIR}/UIProcess/Databases"
    "${WEBKIT2_DIR}/UIProcess/Downloads"
    "${WEBKIT2_DIR}/UIProcess/InspectorServer"
    "${WEBKIT2_DIR}/UIProcess/Launcher"
    "${WEBKIT2_DIR}/UIProcess/Network"
    "${WEBKIT2_DIR}/UIProcess/Network/CustomProtocols"
    "${WEBKIT2_DIR}/UIProcess/Notifications"
    "${WEBKIT2_DIR}/UIProcess/Plugins"
    "${WEBKIT2_DIR}/UIProcess/Storage"
    "${WEBKIT2_DIR}/UIProcess/UserContent"
    "${WEBKIT2_DIR}/UIProcess/WebsiteData"
    "${WEBKIT2_DIR}/WebProcess"
    "${WEBKIT2_DIR}/WebProcess/ApplicationCache"
    "${WEBKIT2_DIR}/WebProcess/Automation"
    "${WEBKIT2_DIR}/WebProcess/Battery"
    "${WEBKIT2_DIR}/WebProcess/Cookies"
    "${WEBKIT2_DIR}/WebProcess/Databases"
    "${WEBKIT2_DIR}/WebProcess/Databases/IndexedDB"
    "${WEBKIT2_DIR}/WebProcess/FileAPI"
    "${WEBKIT2_DIR}/WebProcess/FullScreen"
    "${WEBKIT2_DIR}/WebProcess/Geolocation"
    "${WEBKIT2_DIR}/WebProcess/IconDatabase"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c"
    "${WEBKIT2_DIR}/WebProcess/InjectedBundle/DOM"
    "${WEBKIT2_DIR}/WebProcess/Launching"
    "${WEBKIT2_DIR}/WebProcess/MediaCache"
    "${WEBKIT2_DIR}/WebProcess/MediaStream"
    "${WEBKIT2_DIR}/WebProcess/Network"
    "${WEBKIT2_DIR}/WebProcess/Notifications"
    "${WEBKIT2_DIR}/WebProcess/OriginData"
    "${WEBKIT2_DIR}/WebProcess/Plugins"
    "${WEBKIT2_DIR}/WebProcess/Plugins/Netscape"
    "${WEBKIT2_DIR}/WebProcess/ResourceCache"
    "${WEBKIT2_DIR}/WebProcess/Storage"
    "${WEBKIT2_DIR}/WebProcess/UserContent"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport"
    "${WEBKIT2_DIR}/WebProcess/WebPage"
    "${WEBCORE_DIR}"
    "${WEBCORE_DIR}/Modules/battery"
    "${WEBCORE_DIR}/Modules/indexeddb"
    "${WEBCORE_DIR}/Modules/indexeddb/client"
    "${WEBCORE_DIR}/Modules/indexeddb/server"
    "${WEBCORE_DIR}/Modules/indexeddb/shared"
    "${WEBCORE_DIR}/Modules/mediastream"
    "${WEBCORE_DIR}/Modules/networkinfo"
    "${WEBCORE_DIR}/Modules/notifications"
    "${WEBCORE_DIR}/Modules/streams"
    "${WEBCORE_DIR}/Modules/vibration"
    "${WEBCORE_DIR}/Modules/webdatabase"
    "${WEBCORE_DIR}/accessibility"
    "${WEBCORE_DIR}/bindings/js"
    "${WEBCORE_DIR}/bindings"
    "${WEBCORE_DIR}/bridge"
    "${WEBCORE_DIR}/bridge/jsc"
    "${WEBCORE_DIR}/contentextensions"
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
    "${WEBCORE_DIR}/page/csp"
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
    "${WEBCORE_DIR}/platform/spi/cf"
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
    "${CMAKE_SOURCE_DIR}/WebKitLibraries"
)

set(PROTOCOL_GENERATOR_SCRIPTS_DIR "${JAVASCRIPTCORE_DIR}/inspector/scripts")

set(WebKit2_SYSTEM_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
)

if (ENABLE_GRAPHICS_CONTEXT_3D)
    list(APPEND WebKit2_INCLUDE_DIRECTORIES
        "${THIRDPARTY_DIR}/ANGLE"
        "${THIRDPARTY_DIR}/ANGLE/include/KHR"
    )
endif ()

set(NetworkProcess_COMMON_SOURCES
    NetworkProcess/NetworkConnectionToWebProcess.cpp
    NetworkProcess/NetworkLoad.cpp
    NetworkProcess/NetworkLoadParameters.cpp
    NetworkProcess/NetworkProcess.cpp
    NetworkProcess/NetworkProcessPlatformStrategies.cpp
    NetworkProcess/NetworkResourceLoader.cpp

    NetworkProcess/FileAPI/NetworkBlobRegistry.cpp

    NetworkProcess/cache/NetworkCache.cpp
    NetworkProcess/cache/NetworkCacheBlobStorage.cpp
    NetworkProcess/cache/NetworkCacheCoders.cpp
    NetworkProcess/cache/NetworkCacheData.cpp
    NetworkProcess/cache/NetworkCacheDecoder.cpp
    NetworkProcess/cache/NetworkCacheEncoder.cpp
    NetworkProcess/cache/NetworkCacheEntry.cpp
    NetworkProcess/cache/NetworkCacheFileSystem.cpp
    NetworkProcess/cache/NetworkCacheKey.cpp
    NetworkProcess/cache/NetworkCacheSpeculativeLoad.cpp
    NetworkProcess/cache/NetworkCacheSpeculativeLoadManager.cpp
    NetworkProcess/cache/NetworkCacheSubresourcesEntry.cpp
    NetworkProcess/cache/NetworkCacheStatistics.cpp
    NetworkProcess/cache/NetworkCacheStorage.cpp
)

set(WebKit2_SOURCES
    DatabaseProcess/DatabaseProcess.cpp
    DatabaseProcess/DatabaseToWebProcessConnection.cpp

    DatabaseProcess/IndexedDB/WebIDBConnectionToClient.cpp

    NetworkProcess/NetworkProcessCreationParameters.cpp
    NetworkProcess/NetworkResourceLoadParameters.cpp

    NetworkProcess/Downloads/Download.cpp
    NetworkProcess/Downloads/DownloadAuthenticationClient.cpp
    NetworkProcess/Downloads/DownloadManager.cpp

    Platform/Logging.cpp
    Platform/Module.cpp

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

    Shared/ActivityAssertion.cpp
    Shared/AsyncRequest.cpp
    Shared/BlobDataFileReferenceWithSandboxExtension.cpp
    Shared/CacheModel.cpp
    Shared/ChildProcess.cpp
    Shared/ContextMenuContextData.cpp
    Shared/EditorState.cpp
    Shared/FontInfo.cpp
    Shared/LayerTreeContext.cpp
    Shared/NavigationActionData.cpp
    Shared/OriginAndDatabases.cpp
    Shared/PlatformPopupMenuData.cpp
    Shared/PrintInfo.cpp
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
    Shared/WebCompiledContentExtension.cpp
    Shared/WebCompiledContentExtensionData.cpp
    Shared/WebConnection.cpp
    Shared/WebConnectionClient.cpp
    Shared/WebContextMenuItem.cpp
    Shared/WebContextMenuItemData.cpp
    Shared/WebCoreArgumentCoders.cpp
    Shared/WebCrossThreadCopier.cpp
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
    Shared/WebOpenPanelParameters.cpp
    Shared/WebPageCreationParameters.cpp
    Shared/WebPageGroupData.cpp
    Shared/WebPlatformTouchPoint.cpp
    Shared/WebPopupItem.cpp
    Shared/WebPreferencesKeys.cpp
    Shared/WebPreferencesStore.cpp
    Shared/WebProcessCreationParameters.cpp
    Shared/WebRenderLayer.cpp
    Shared/WebRenderObject.cpp
    Shared/WebTouchEvent.cpp
    Shared/WebUserContentControllerDataTypes.cpp
    Shared/WebWheelEvent.cpp

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

    Shared/Databases/DatabaseProcessCreationParameters.cpp

    Shared/Databases/IndexedDB/IDBUtilities.cpp

    Shared/Plugins/NPIdentifierData.cpp
    Shared/Plugins/NPObjectMessageReceiver.cpp
    Shared/Plugins/NPObjectProxy.cpp
    Shared/Plugins/NPRemoteObjectMap.cpp
    Shared/Plugins/NPVariantData.cpp
    Shared/Plugins/PluginProcessCreationParameters.cpp

    Shared/Plugins/Netscape/NetscapePluginModule.cpp
    Shared/Plugins/Netscape/NetscapePluginModuleNone.cpp
    Shared/Plugins/Netscape/PluginInformation.cpp

    Shared/WebsiteData/WebsiteData.cpp

    UIProcess/ChildProcessProxy.cpp
    UIProcess/DrawingAreaProxy.cpp
    UIProcess/FrameLoadState.cpp
    UIProcess/GeolocationPermissionRequestManagerProxy.cpp
    UIProcess/GeolocationPermissionRequestProxy.cpp
    UIProcess/PageLoadState.cpp
    UIProcess/ProcessAssertion.cpp
    UIProcess/ProcessThrottler.cpp
    UIProcess/ResponsivenessTimer.cpp
    UIProcess/StatisticsRequest.cpp
    UIProcess/TextCheckerCompletion.cpp
    UIProcess/UserMediaPermissionCheckProxy.cpp
    UIProcess/UserMediaPermissionRequestManagerProxy.cpp
    UIProcess/UserMediaPermissionRequestProxy.cpp
    UIProcess/VisitedLinkStore.cpp
    UIProcess/WebBackForwardList.cpp
    UIProcess/WebBatteryManagerProxy.cpp
    UIProcess/WebBatteryProvider.cpp
    UIProcess/WebColorPicker.cpp
    UIProcess/WebConnectionToWebProcess.cpp
    UIProcess/WebContextClient.cpp
    UIProcess/WebContextConnectionClient.cpp
    UIProcess/WebContextInjectedBundleClient.cpp
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
    UIProcess/WebIconDatabase.cpp
    UIProcess/WebIconDatabaseClient.cpp
    UIProcess/WebInspectorProxy.cpp
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
    UIProcess/WebTextChecker.cpp
    UIProcess/WebTextCheckerClient.cpp
    UIProcess/WebVibrationProvider.cpp
    UIProcess/WebVibrationProxy.cpp
    UIProcess/WebViewportAttributes.cpp

    UIProcess/API/APIFrameInfo.cpp
    UIProcess/API/APIHitTestResult.cpp
    UIProcess/API/APINavigation.cpp
    UIProcess/API/APINavigationData.cpp
    UIProcess/API/APIPageConfiguration.cpp
    UIProcess/API/APIProcessPoolConfiguration.cpp
    UIProcess/API/APISession.cpp
    UIProcess/API/APISessionState.cpp
    UIProcess/API/APIUserContentExtension.cpp
    UIProcess/API/APIUserContentExtensionStore.cpp
    UIProcess/API/APIUserContentWorld.cpp
    UIProcess/API/APIUserScript.cpp
    UIProcess/API/APIUserStyleSheet.cpp
    UIProcess/API/APIWebsiteDataStore.cpp
    UIProcess/API/APIWindowFeatures.cpp

    UIProcess/API/C/WKApplicationCacheManager.cpp
    UIProcess/API/C/WKAuthenticationChallenge.cpp
    UIProcess/API/C/WKAuthenticationDecisionListener.cpp
    UIProcess/API/C/WKBackForwardListItemRef.cpp
    UIProcess/API/C/WKBackForwardListRef.cpp
    UIProcess/API/C/WKBatteryManager.cpp
    UIProcess/API/C/WKBatteryStatus.cpp
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
    UIProcess/API/C/WKGrammarDetail.cpp
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
    UIProcess/API/C/WKOpenPanelParameters.cpp
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
    UIProcess/API/C/WKVibration.cpp
    UIProcess/API/C/WKViewportAttributes.cpp
    UIProcess/API/C/WKWindowFeaturesRef.cpp

    UIProcess/Authentication/AuthenticationChallengeProxy.cpp
    UIProcess/Authentication/AuthenticationDecisionListener.cpp
    UIProcess/Authentication/WebCredential.cpp
    UIProcess/Authentication/WebProtectionSpace.cpp

    UIProcess/Automation/WebAutomationSession.cpp

    UIProcess/Databases/DatabaseProcessProxy.cpp

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

    UIProcess/Storage/LocalStorageDatabase.cpp
    UIProcess/Storage/LocalStorageDatabaseTracker.cpp

    UIProcess/UserContent/WebScriptMessageHandler.cpp
    UIProcess/UserContent/WebUserContentControllerProxy.cpp

    UIProcess/WebsiteData/WebsiteDataRecord.cpp
    UIProcess/WebsiteData/WebsiteDataStore.cpp

    WebProcess/WebConnectionToUIProcess.cpp
    WebProcess/WebProcess.cpp

    WebProcess/Automation/WebAutomationSessionProxy.cpp

    WebProcess/Battery/WebBatteryManager.cpp

    WebProcess/Cookies/WebCookieManager.cpp

    WebProcess/Databases/WebDatabaseProvider.cpp
    WebProcess/Databases/WebToDatabaseProcessConnection.cpp

    WebProcess/Databases/IndexedDB/WebIDBConnectionToServer.cpp

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

    WebProcess/InjectedBundle/DOM/InjectedBundleCSSStyleDeclarationHandle.cpp
    WebProcess/InjectedBundle/DOM/InjectedBundleFileHandle.cpp
    WebProcess/InjectedBundle/DOM/InjectedBundleNodeHandle.cpp
    WebProcess/InjectedBundle/DOM/InjectedBundleRangeHandle.cpp

    WebProcess/MediaStream/UserMediaPermissionRequestManager.cpp

    WebProcess/Network/NetworkProcessConnection.cpp
    WebProcess/Network/WebLoaderStrategy.cpp
    WebProcess/Network/WebResourceLoader.cpp

    WebProcess/Notifications/NotificationPermissionRequestManager.cpp
    WebProcess/Notifications/WebNotificationManager.cpp

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

    WebProcess/Storage/StorageAreaImpl.cpp
    WebProcess/Storage/StorageAreaMap.cpp
    WebProcess/Storage/StorageNamespaceImpl.cpp
    WebProcess/Storage/WebStorageNamespaceProvider.cpp

    WebProcess/UserContent/WebUserContentController.cpp

    WebProcess/WebCoreSupport/SessionStateConversion.cpp
    WebProcess/WebCoreSupport/WebBatteryClient.cpp
    WebProcess/WebCoreSupport/WebChromeClient.cpp
    WebProcess/WebCoreSupport/WebColorChooser.cpp
    WebProcess/WebCoreSupport/WebContextMenuClient.cpp
    WebProcess/WebCoreSupport/WebDeviceProximityClient.cpp
    WebProcess/WebCoreSupport/WebDiagnosticLoggingClient.cpp
    WebProcess/WebCoreSupport/WebDragClient.cpp
    WebProcess/WebCoreSupport/WebEditorClient.cpp
    WebProcess/WebCoreSupport/WebFrameLoaderClient.cpp
    WebProcess/WebCoreSupport/WebGeolocationClient.cpp
    WebProcess/WebCoreSupport/WebInspectorClient.cpp
    WebProcess/WebCoreSupport/WebNotificationClient.cpp
    WebProcess/WebCoreSupport/WebPlatformStrategies.cpp
    WebProcess/WebCoreSupport/WebPlugInClient.cpp
    WebProcess/WebCoreSupport/WebPopupMenu.cpp
    WebProcess/WebCoreSupport/WebProgressTrackerClient.cpp
    WebProcess/WebCoreSupport/WebSearchPopupMenu.cpp
    WebProcess/WebCoreSupport/WebUserMediaClient.cpp
    WebProcess/WebCoreSupport/WebVibrationClient.cpp

    WebProcess/WebPage/DrawingArea.cpp
    WebProcess/WebPage/EventDispatcher.cpp
    WebProcess/WebPage/FindController.cpp
    WebProcess/WebPage/LayerTreeHost.cpp
    WebProcess/WebPage/PageBanner.cpp
    WebProcess/WebPage/VisitedLinkTableController.cpp
    WebProcess/WebPage/WebBackForwardListProxy.cpp
    WebProcess/WebPage/WebContextMenu.cpp
    WebProcess/WebPage/WebDocumentLoader.cpp
    WebProcess/WebPage/WebFrame.cpp
    WebProcess/WebPage/WebInspector.cpp
    WebProcess/WebPage/WebInspectorFrontendAPIDispatcher.cpp
    WebProcess/WebPage/WebInspectorUI.cpp
    WebProcess/WebPage/WebOpenPanelResultListener.cpp
    WebProcess/WebPage/WebPage.cpp
    WebProcess/WebPage/WebPageGroupProxy.cpp
    WebProcess/WebPage/WebPageOverlay.cpp
    WebProcess/WebPage/WebUndoStep.cpp



    ${NetworkProcess_COMMON_SOURCES}
)

set(WebKit2_MESSAGES_IN_FILES
    DatabaseProcess/DatabaseProcess.messages.in
    DatabaseProcess/DatabaseToWebProcessConnection.messages.in

    DatabaseProcess/IndexedDB/WebIDBConnectionToClient.messages.in

    NetworkProcess/CustomProtocols/CustomProtocolManager.messages.in

    NetworkProcess/NetworkConnectionToWebProcess.messages.in
    NetworkProcess/NetworkProcess.messages.in
    NetworkProcess/NetworkResourceLoader.messages.in

    PluginProcess/PluginControllerProxy.messages.in
    PluginProcess/PluginProcess.messages.in
    PluginProcess/WebProcessConnection.messages.in

    Shared/ChildProcess.messages.in
    Shared/WebConnection.messages.in

    Shared/Authentication/AuthenticationManager.messages.in

    Shared/Plugins/NPObjectMessageReceiver.messages.in

    UIProcess/DrawingAreaProxy.messages.in
    UIProcess/VisitedLinkStore.messages.in
    UIProcess/WebBatteryManagerProxy.messages.in
    UIProcess/WebCookieManagerProxy.messages.in
    UIProcess/WebFullScreenManagerProxy.messages.in
    UIProcess/WebGeolocationManagerProxy.messages.in
    UIProcess/WebIconDatabase.messages.in
    UIProcess/WebInspectorProxy.messages.in
    UIProcess/WebPageProxy.messages.in
    UIProcess/WebPasteboardProxy.messages.in
    UIProcess/WebProcessPool.messages.in
    UIProcess/WebProcessProxy.messages.in
    UIProcess/WebResourceLoadStatisticsStore.messages.in
    UIProcess/WebVibrationProxy.messages.in

    UIProcess/Automation/WebAutomationSession.messages.in

    UIProcess/Databases/DatabaseProcessProxy.messages.in

    UIProcess/Downloads/DownloadProxy.messages.in

    UIProcess/Network/NetworkProcessProxy.messages.in

    UIProcess/Network/CustomProtocols/CustomProtocolManagerProxy.messages.in

    UIProcess/Plugins/PluginProcessProxy.messages.in

    UIProcess/Storage/StorageManager.messages.in

    UIProcess/UserContent/WebUserContentControllerProxy.messages.in

    WebProcess/WebProcess.messages.in

    WebProcess/Automation/WebAutomationSessionProxy.messages.in

    WebProcess/Battery/WebBatteryManager.messages.in

    WebProcess/Cookies/WebCookieManager.messages.in

    WebProcess/Databases/IndexedDB/WebIDBConnectionToServer.messages.in

    WebProcess/FullScreen/WebFullScreenManager.messages.in

    WebProcess/Geolocation/WebGeolocationManager.messages.in

    WebProcess/IconDatabase/WebIconDatabaseProxy.messages.in

    WebProcess/Network/NetworkProcessConnection.messages.in
    WebProcess/Network/WebResourceLoader.messages.in

    WebProcess/Notifications/WebNotificationManager.messages.in

    WebProcess/Plugins/PluginProcessConnection.messages.in
    WebProcess/Plugins/PluginProcessConnectionManager.messages.in
    WebProcess/Plugins/PluginProxy.messages.in

    WebProcess/Storage/StorageAreaMap.messages.in

    WebProcess/UserContent/WebUserContentController.messages.in

    WebProcess/WebPage/DrawingArea.messages.in
    WebProcess/WebPage/EventDispatcher.messages.in
    WebProcess/WebPage/VisitedLinkTableController.messages.in
    WebProcess/WebPage/WebInspector.messages.in
    WebProcess/WebPage/WebInspectorUI.messages.in
    WebProcess/WebPage/WebPage.messages.in
)

set(WebProcess_LIBRARIES
    WebKit2
)

set(NetworkProcess_LIBRARIES
    WebKit2
)

set(DatabaseProcess_LIBRARIES
  WebKit2
)

set(WebKit2_LIBRARIES
    JavaScriptCore
    WebCore
)

set(PluginProcess_LIBRARIES
    WebKit2
)

if (WIN32 AND INTERNAL_BUILD)
    set(JavaScriptCore_SCRIPTS_DIR "${CMAKE_BINARY_DIR}/../include/private/JavaScriptCore/Scripts")
else ()
    set(JavaScriptCore_SCRIPTS_DIR "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/Scripts")
endif ()

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

GENERATE_WEBKIT2_MESSAGE_SOURCES(WebKit2_DERIVED_SOURCES "${WebKit2_MESSAGES_IN_FILES}")

set(WebKit2_AUTOMATION_PROTOCOL_GENERATOR_SCRIPTS
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/generate-inspector-protocol-bindings.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/cpp_generator.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/cpp_generator_templates.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_backend_dispatcher_header.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_backend_dispatcher_implementation.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_protocol_types_header.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_protocol_types_implementation.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generator.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generator_templates.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/models.py
)

set(WebKit2_AUTOMATION_PROTOCOL_GENERATOR_INPUTS
    ${WEBKIT2_DIR}/UIProcess/Automation/Automation.json
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2_DIR}/AutomationBackendDispatchers.h ${DERIVED_SOURCES_WEBKIT2_DIR}/AutomationBackendDispatchers.cpp ${DERIVED_SOURCES_WEBKIT2_DIR}/AutomationProtocolObjects.h ${DERIVED_SOURCES_WEBKIT2_DIR}/AutomationProtocolObjects.cpp
    MAIN_DEPENDENCY ${WebKit2_AUTOMATION_PROTOCOL_GENERATOR_INPUTS}
    DEPENDS ${WebKit2_AUTOMATION_PROTOCOL_GENERATOR_SCRIPTS}
    COMMAND ${PYTHON_EXECUTABLE} ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/generate-inspector-protocol-bindings.py --outputDir "${DERIVED_SOURCES_WEBKIT2_DIR}" --framework WebKit --backend ${WebKit2_AUTOMATION_PROTOCOL_GENERATOR_INPUTS}
    VERBATIM)

list(APPEND WebKit2_HEADERS
    ${DERIVED_SOURCES_WEBKIT2_DIR}/AutomationBackendDispatchers.h
    ${DERIVED_SOURCES_WEBKIT2_DIR}/AutomationProtocolObjects.h
)

list(APPEND WebKit2_SOURCES
    ${DERIVED_SOURCES_WEBKIT2_DIR}/AutomationBackendDispatchers.cpp
    ${DERIVED_SOURCES_WEBKIT2_DIR}/AutomationProtocolObjects.cpp
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2_DIR}/WebAutomationSessionProxyScriptSource.h ${DERIVED_SOURCES_WEBKIT2_DIR}/WebAutomationSessionProxy.min.js
    MAIN_DEPENDENCY ${WEBKIT2_DIR}/WebProcess/Automation/WebAutomationSessionProxy.js
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py
    COMMAND ${CMAKE_COMMAND} -E echo "//# sourceURL=__WebAutomationInjectedScript__" > ${DERIVED_SOURCES_WEBKIT2_DIR}/WebAutomationSessionProxy.min.js
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py < ${WEBKIT2_DIR}/WebProcess/Automation/WebAutomationSessionProxy.js >> ${DERIVED_SOURCES_WEBKIT2_DIR}/WebAutomationSessionProxy.min.js
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl WebAutomationSessionProxyScriptSource ${DERIVED_SOURCES_WEBKIT2_DIR}/WebAutomationSessionProxy.min.js ${DERIVED_SOURCES_WEBKIT2_DIR}/WebAutomationSessionProxyScriptSource.h
    VERBATIM)

list(APPEND WebKit2_HEADERS
    ${DERIVED_SOURCES_WEBKIT2_DIR}/WebAutomationSessionProxyScriptSource.h
)

list(APPEND WebKit2_SOURCES ${WebKit2_DERIVED_SOURCES})

WEBKIT_FRAMEWORK(WebKit2)
add_dependencies(WebKit2 WebCore ${WEBKIT2_EXTRA_DEPENDENCIES})
add_webkit2_prefix_header(WebKit2)

# Suppress unused parameter warnings for sources in WebKit2.
ADD_TARGET_PROPERTIES(WebKit2 COMPILE_FLAGS "-Wno-unused-parameter")

if (WebKit2_VERSION_SCRIPT)
    ADD_TARGET_PROPERTIES(WebKit2 LINK_FLAGS "${WebKit2_VERSION_SCRIPT}")
endif ()

add_executable(WebProcess ${WebProcess_SOURCES})
add_webkit2_prefix_header(WebProcess)
target_link_libraries(WebProcess ${WebProcess_LIBRARIES})
set_target_properties(WebProcess PROPERTIES FOLDER "WebKit")

if (WebKit2_WebProcess_OUTPUT_NAME)
    set_target_properties(WebProcess PROPERTIES OUTPUT_NAME ${WebKit2_WebProcess_OUTPUT_NAME})
endif ()

add_executable(NetworkProcess ${NetworkProcess_SOURCES})
add_webkit2_prefix_header(NetworkProcess)
target_link_libraries(NetworkProcess ${NetworkProcess_LIBRARIES})
set_target_properties(NetworkProcess PROPERTIES FOLDER "WebKit")

if (WebKit2_NetworkProcess_OUTPUT_NAME)
    set_target_properties(NetworkProcess PROPERTIES OUTPUT_NAME ${WebKit2_NetworkProcess_OUTPUT_NAME})
endif ()

if (ENABLE_PLUGIN_PROCESS AND NOT APPLE)
    add_definitions(-DENABLE_PLUGIN_PROCESS=1)
    add_executable(PluginProcess ${PluginProcess_SOURCES})
    add_webkit2_prefix_header(PluginProcess)
    target_link_libraries(PluginProcess ${PluginProcess_LIBRARIES})
    set_target_properties(PluginProcess PROPERTIES FOLDER "WebKit")
    install(TARGETS PluginProcess DESTINATION "${LIBEXEC_INSTALL_DIR}")

    if (WebKit2_PluginProcess_OUTPUT_NAME)
      set_target_properties(PluginProcess PROPERTIES OUTPUT_NAME ${WebKit2_PluginProcess_OUTPUT_NAME})
    endif ()
endif ()

if (ENABLE_DATABASE_PROCESS AND NOT APPLE)
    add_definitions(-DENABLE_DATABASE_PROCESS=1)
    add_executable(DatabaseProcess ${DatabaseProcess_SOURCES})
    add_webkit2_prefix_header(DatabaseProcess)
    target_link_libraries(DatabaseProcess ${DatabaseProcess_LIBRARIES})
    set_target_properties(DatabaseProcess PROPERTIES FOLDER "WebKit")
    install(TARGETS DatabaseProcess DESTINATION "${LIBEXEC_INSTALL_DIR}")

    if (WebKit2_DatabaseProcess_OUTPUT_NAME)
        set_target_properties(DatabaseProcess PROPERTIES OUTPUT_NAME ${WebKit2_DatabaseProcess_OUTPUT_NAME})
    endif ()
endif ()

if (APPLE)
    set(_web_xpc_dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/XPCServices/com.apple.WebKit.WebContent.Development.xpc/Contents)
    make_directory(${_web_xpc_dir}/MacOS)
    make_directory(${_web_xpc_dir}/Resources)
    make_directory(${_web_xpc_dir}/_CodeSignature)

    add_custom_target(WebContentProcessNib COMMAND
        ibtool --compile ${_web_xpc_dir}/Resources/WebContentProcess.nib ${WEBKIT2_DIR}/Resources/WebContentProcess.xib
        VERBATIM)

    # FIXME: This shouldn't be two operations.
    file(COPY ${WEBKIT2_DIR}/WebProcess/EntryPoint/mac/XPCService/WebContentService/Info-OSX.plist DESTINATION ${_web_xpc_dir})
    file(RENAME ${_web_xpc_dir}/Info-OSX.plist ${_web_xpc_dir}/Info.plist)

    set_target_properties(WebProcess PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_web_xpc_dir}/MacOS")


    set(_net_xpc_dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/XPCServices/com.apple.WebKit.Networking.Development.xpc/Contents)
    make_directory(${_net_xpc_dir}/MacOS)
    make_directory(${_net_xpc_dir}/Resources)
    make_directory(${_net_xpc_dir}/_CodeSignature)

    # FIXME: This shouldn't be two operations.
    file(COPY ${WEBKIT2_DIR}/NetworkProcess/EntryPoint/mac/XPCService/NetworkService/Info-OSX.plist DESTINATION ${_net_xpc_dir})
    file(RENAME ${_net_xpc_dir}/Info-OSX.plist ${_net_xpc_dir}/Info.plist)

    set_target_properties(NetworkProcess PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_net_xpc_dir}/MacOS")
else ()
    POPULATE_LIBRARY_VERSION(WEBKIT2)
    set_target_properties(WebKit2 PROPERTIES VERSION ${WEBKIT2_VERSION} SOVERSION ${WEBKIT2_VERSION_MAJOR})

    install(TARGETS WebKit2 WebProcess NetworkProcess
        LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
        RUNTIME DESTINATION "${LIBEXEC_INSTALL_DIR}"
    )
endif ()
