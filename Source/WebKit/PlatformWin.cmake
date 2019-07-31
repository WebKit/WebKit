set(WebKit_OUTPUT_NAME WebKit2)
set(WebKit_WebProcess_OUTPUT_NAME WebKitWebProcess)
set(WebKit_NetworkProcess_OUTPUT_NAME WebKitNetworkProcess)
set(WebKit_PluginProcess_OUTPUT_NAME WebKitPluginProcess)

add_definitions(-DBUILDING_WEBKIT)

list(APPEND WebKit_SOURCES
    NetworkProcess/Classifier/WebResourceLoadStatisticsStore.cpp
    NetworkProcess/Classifier/WebResourceLoadStatisticsTelemetry.cpp

    NetworkProcess/WebStorage/StorageManager.cpp

    NetworkProcess/win/NetworkProcessMainWin.cpp

    Platform/IPC/win/AttachmentWin.cpp
    Platform/IPC/win/ConnectionWin.cpp

    Platform/classifier/ResourceLoadStatisticsClassifier.cpp

    Platform/win/LoggingWin.cpp
    Platform/win/ModuleWin.cpp
    Platform/win/SharedMemoryWin.cpp

    Shared/API/c/curl/WKCertificateInfoCurl.cpp

    Shared/Plugins/Netscape/NetscapePluginModuleNone.cpp

    Shared/win/AuxiliaryProcessMainWin.cpp
    Shared/win/NativeWebKeyboardEventWin.cpp
    Shared/win/NativeWebMouseEventWin.cpp
    Shared/win/NativeWebTouchEventWin.cpp
    Shared/win/NativeWebWheelEventWin.cpp
    Shared/win/WebEventFactory.cpp

    UIProcess/BackingStore.cpp
    UIProcess/DefaultUndoController.cpp
    UIProcess/LegacySessionStateCodingNone.cpp
    UIProcess/WebGrammarDetail.cpp
    UIProcess/WebViewportAttributes.cpp

    UIProcess/API/C/WKViewportAttributes.cpp

    UIProcess/API/C/curl/WKProtectionSpaceCurl.cpp
    UIProcess/API/C/curl/WKWebsiteDataStoreRefCurl.cpp

    UIProcess/API/C/win/WKView.cpp

    UIProcess/API/win/APIWebsiteDataStoreWin.cpp

    UIProcess/CoordinatedGraphics/DrawingAreaProxyCoordinatedGraphics.cpp

    UIProcess/Launcher/win/ProcessLauncherWin.cpp

    UIProcess/WebsiteData/curl/WebsiteDataStoreCurl.cpp

    UIProcess/WebsiteData/win/WebsiteDataStoreWin.cpp

    UIProcess/win/PageClientImpl.cpp
    UIProcess/win/TextCheckerWin.cpp
    UIProcess/win/WebContextMenuProxyWin.cpp
    UIProcess/win/WebInspectorProxyWin.cpp
    UIProcess/win/WebPageProxyWin.cpp
    UIProcess/win/WebPopupMenuProxyWin.cpp
    UIProcess/win/WebPreferencesWin.cpp
    UIProcess/win/WebProcessPoolWin.cpp
    UIProcess/win/WebView.cpp

    WebProcess/InjectedBundle/win/InjectedBundleWin.cpp

    WebProcess/MediaCache/WebMediaKeyStorageManager.cpp

    WebProcess/Plugins/Netscape/NetscapePluginNone.cpp
    WebProcess/Plugins/Netscape/win/PluginProxyWin.cpp

    WebProcess/WebCoreSupport/win/WebContextMenuClientWin.cpp
    WebProcess/WebCoreSupport/win/WebPopupMenuWin.cpp

    WebProcess/WebPage/AcceleratedSurface.cpp

    WebProcess/WebPage/CoordinatedGraphics/CompositingCoordinator.cpp
    WebProcess/WebPage/CoordinatedGraphics/DrawingAreaCoordinatedGraphics.cpp
    WebProcess/WebPage/CoordinatedGraphics/LayerTreeHost.cpp

    WebProcess/WebPage/win/WebInspectorUIWin.cpp
    WebProcess/WebPage/win/WebPageWin.cpp

    WebProcess/win/WebProcessMainWin.cpp
    WebProcess/win/WebProcessWin.cpp
)

# DerivedSources/JavaScriptCore/inspector/InspectorBackendCommands.js is
# expected in DerivedSources/WebInspectorUI/UserInterface/Protocol/.
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    DEPENDS ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorBackendCommands.js
    COMMAND cp ${JavaScriptCore_DERIVED_SOURCES_DIR}/inspector/InspectorBackendCommands.js ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/NetworkProcess/win"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/PluginProcess/win"
    "${WEBKIT_DIR}/Shared/API/c/win"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT_DIR}/Shared/Plugins/win"
    "${WEBKIT_DIR}/Shared/unix"
    "${WEBKIT_DIR}/Shared/win"
    "${WEBKIT_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT_DIR}/UIProcess/API/C/curl"
    "${WEBKIT_DIR}/UIProcess/API/C/win"
    "${WEBKIT_DIR}/UIProcess/API/cpp/win"
    "${WEBKIT_DIR}/UIProcess/API/win"
    "${WEBKIT_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT_DIR}/UIProcess/Plugins/win"
    "${WEBKIT_DIR}/UIProcess/win"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/win"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/win/DOM"
    "${WEBKIT_DIR}/WebProcess/win"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/win"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebPage/win"
    "${WEBKIT_DIR}/win"
)

list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
)

set(WebKitCommonIncludeDirectories ${WebKit_INCLUDE_DIRECTORIES})
set(WebKitCommonSystemIncludeDirectories ${WebKit_SYSTEM_INCLUDE_DIRECTORIES})

list(APPEND WebProcess_SOURCES
    WebProcess/EntryPoint/win/WebProcessMain.cpp
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/win/NetworkProcessMain.cpp
)

if (${ENABLE_PLUGIN_PROCESS})
    list(APPEND PluginProcess_SOURCES
    )
endif ()

if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DUSE_CAIRO=1 -DUSE_CURL=1)

    list(APPEND WebKit_SOURCES
        NetworkProcess/Cookies/curl/WebCookieManagerCurl.cpp

        NetworkProcess/cache/NetworkCacheDataCurl.cpp
        NetworkProcess/cache/NetworkCacheIOChannelCurl.cpp

        NetworkProcess/curl/NetworkDataTaskCurl.cpp
        NetworkProcess/curl/NetworkProcessCurl.cpp
        NetworkProcess/curl/NetworkSessionCurl.cpp
        NetworkProcess/curl/RemoteNetworkingContextCurl.cpp

        Shared/API/c/cairo/WKImageCairo.cpp

        Shared/cairo/ShareableBitmapCairo.cpp

        Shared/curl/WebCoreArgumentCodersCurl.cpp

        UIProcess/Automation/cairo/WebAutomationSessionCairo.cpp

        UIProcess/cairo/BackingStoreCairo.cpp

        WebProcess/WebCoreSupport/curl/WebFrameNetworkingContext.cpp
    )

    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBKIT_DIR}/NetworkProcess/curl"
        "${WEBKIT_DIR}/WebProcess/WebCoreSupport/curl"
    )

    list(APPEND WebKit_LIBRARIES
        PRIVATE
            ${OPENSSL_LIBRARIES}
            mfuuid.lib
            strmiids.lib
    )
endif ()

if (ENABLE_REMOTE_INSPECTOR)
    list(APPEND WebKit_SOURCES
        UIProcess/socket/RemoteInspectorClient.cpp
        UIProcess/socket/RemoteInspectorProtocolHandler.cpp

        UIProcess/win/RemoteWebInspectorProxyWin.cpp
    )

    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBKIT_DIR}/UIProcess/socket"
    )
endif ()

set(SharedWebKitLibraries
    ${WebKit_LIBRARIES}
)

WEBKIT_WRAP_SOURCELIST(${WebKit_SOURCES})

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

# Windows specific
list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
    Shared/API/c/win/WKBaseWin.h

    UIProcess/API/C/win/WKView.h
)

set(WebKit_FORWARDING_HEADERS_DIRECTORIES
    Shared/API/c

    Shared/API/c/cairo
    Shared/API/c/cf
    Shared/API/c/win

    UIProcess/API/C
    UIProcess/API/cpp

    UIProcess/API/C/win

    WebProcess/InjectedBundle/API/c
)

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND WebKit_PUBLIC_FRAMEWORK_HEADERS
        Shared/API/c/cairo/WKImageCairo.h

        Shared/API/c/curl/WKCertificateInfoCurl.h

        UIProcess/API/C/curl/WKProtectionSpaceCurl.h
        UIProcess/API/C/curl/WKWebsiteDataStoreRefCurl.h
    )
endif ()

WEBKIT_MAKE_FORWARDING_HEADERS(WebKit
    TARGET_NAME WebKitFrameworkHeaders
    DESTINATION ${WebKit_FRAMEWORK_HEADERS_DIR}/WebKit
    FILES ${WebKit_PUBLIC_FRAMEWORK_HEADERS}
    FLATTENED
)
