# WebKit2 - Qt4 build info

CONFIG += building-libs
CONFIG += depend_includepath

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../..

CONFIG(standalone_package) {
    isEmpty(WEBKIT2_GENERATED_SOURCES_DIR):WEBKIT2_GENERATED_SOURCES_DIR = $$PWD/generated
    isEmpty(WC_GENERATED_SOURCES_DIR):WC_GENERATED_SOURCES_DIR = $$PWD/../WebCore/generated
} else {
    isEmpty(WEBKIT2_GENERATED_SOURCES_DIR):WEBKIT2_GENERATED_SOURCES_DIR = generated
    isEmpty(WC_GENERATED_SOURCES_DIR):WC_GENERATED_SOURCES_DIR = ../WebCore/generated
}

include($$PWD/../WebKit.pri)
include($$PWD/../common.pri)
include($$PWD/../WebCore/features.pri)
include(WebKit2.pri)

TEMPLATE = lib
CONFIG += staticlib
TARGET = $$WEBKIT2_TARGET
DESTDIR = $$WEBKIT2_DESTDIR

!CONFIG(release, debug|release) {
    OBJECTS_DIR = obj/debug
} else { # Release
    OBJECTS_DIR = obj/release
    DEFINES += NDEBUG
}

# Build both debug and release configurations
mac: CONFIG += build_all

WEBKIT2_INCLUDEPATH = \
    $$PWD/.. \

WEBKIT2_INCLUDEPATH = \
    $$PWD/../JavaScriptCore \
    $$PWD/../JavaScriptCore/assembler \
    $$PWD/../JavaScriptCore/bytecode \
    $$PWD/../JavaScriptCore/bytecompiler \
    $$PWD/../JavaScriptCore/debugger \
    $$PWD/../JavaScriptCore/interpreter \
    $$PWD/../JavaScriptCore/jit \
    $$PWD/../JavaScriptCore/parser \
    $$PWD/../JavaScriptCore/profiler \
    $$PWD/../JavaScriptCore/runtime \
    $$PWD/../JavaScriptCore/wtf \
    $$PWD/../JavaScriptCore/wtf/symbian \
    $$PWD/../JavaScriptCore/wtf/unicode \
    $$PWD/../JavaScriptCore/yarr \
    $$PWD/../JavaScriptCore/API \
    $$PWD/../JavaScriptCore/ForwardingHeaders \
    $$WEBKIT2_INCLUDEPATH

WEBKIT2_INCLUDEPATH = \
    $$PWD/../WebCore \
    $$PWD/../WebCore/accessibility \
    $$PWD/../WebCore/bindings \
    $$PWD/../WebCore/bindings/generic \
    $$PWD/../WebCore/bindings/js \
    $$PWD/../WebCore/bridge \
    $$PWD/../WebCore/bridge/c \
    $$PWD/../WebCore/bridge/jsc \
    $$PWD/../WebCore/css \
    $$PWD/../WebCore/dom \
    $$PWD/../WebCore/dom/default \
    $$PWD/../WebCore/editing \
    $$PWD/../WebCore/history \
    $$PWD/../WebCore/html \
    $$PWD/../WebCore/html/canvas \
    $$PWD/../WebCore/inspector \
    $$PWD/../WebCore/loader \
    $$PWD/../WebCore/loader/appcache \
    $$PWD/../WebCore/loader/archive \
    $$PWD/../WebCore/loader/cache \
    $$PWD/../WebCore/loader/icon \
    $$PWD/../WebCore/mathml \
    $$PWD/../WebCore/notifications \
    $$PWD/../WebCore/page \
    $$PWD/../WebCore/page/animation \
    $$PWD/../WebCore/platform \
    $$PWD/../WebCore/platform/animation \
    $$PWD/../WebCore/platform/graphics \
    $$PWD/../WebCore/platform/graphics/filters \
    $$PWD/../WebCore/platform/graphics/transforms \
    $$PWD/../WebCore/platform/image-decoders \
    $$PWD/../WebCore/platform/mock \
    $$PWD/../WebCore/platform/network \
    $$PWD/../WebCore/platform/sql \
    $$PWD/../WebCore/platform/text \
    $$PWD/../WebCore/platform/text/transcoder \
    $$PWD/../WebCore/plugins \
    $$PWD/../WebCore/rendering \
    $$PWD/../WebCore/rendering/style \
    $$PWD/../WebCore/storage \
    $$PWD/../WebCore/svg \
    $$PWD/../WebCore/svg/animation \
    $$PWD/../WebCore/svg/graphics \
    $$PWD/../WebCore/svg/graphics/filters \
    $$PWD/../WebCore/svg/properties \
    $$PWD/../WebCore/websockets \
    $$PWD/../WebCore/wml \
    $$PWD/../WebCore/workers \
    $$PWD/../WebCore/xml \
    $$WEBKIT2_INCLUDEPATH

WEBKIT2_INCLUDEPATH = \
    $$PWD/../WebCore/bridge/qt \
    $$PWD/../WebCore/page/qt \
    $$PWD/../WebCore/platform/graphics/qt \
    $$PWD/../WebCore/platform/network/qt \
    $$PWD/../WebCore/platform/qt \
    $$PWD/../WebKit/qt/Api \
    $$PWD/../WebKit/qt/WebCoreSupport \
    $$WEBKIT2_INCLUDEPATH

WEBKIT2_INCLUDEPATH = \
    $$PWD \
    Platform \
    Platform/CoreIPC \
    Platform/qt \
    Shared \
    Shared/API/c \
    Shared/CoreIPCSupport \
    Shared/Plugins \
    Shared/Plugins/Netscape \
    Shared/qt \
    UIProcess \
    UIProcess/API/C \
    UIProcess/API/cpp \
    UIProcess/API/cpp/qt \
    UIProcess/API/qt \
    UIProcess/Authentication \
    UIProcess/Downloads \
    UIProcess/Launcher \
    UIProcess/Plugins \
    UIProcess/qt \
    WebProcess \
    WebProcess/Authentication \
    WebProcess/Downloads \
    WebProcess/Downloads/qt \
    WebProcess/Geolocation \
    WebProcess/InjectedBundle \
    WebProcess/InjectedBundle/DOM \
    WebProcess/InjectedBundle/API/c \
    WebProcess/Plugins \
    WebProcess/Plugins/Netscape \
    WebProcess/WebCoreSupport \
    WebProcess/WebCoreSupport/qt \
    WebProcess/WebPage \
    WebProcess/qt \
    $$WEBKIT2_INCLUDEPATH

WEBKIT2_INCLUDEPATH += \
    $$OUTPUT_DIR/include \
    $$WC_GENERATED_SOURCES_DIR \
    $$WEBKIT2_GENERATED_SOURCES_DIR \
    $$WEBKIT2_INCLUDEPATH

# On Symbian PREPEND_INCLUDEPATH is the best way to make sure that WebKit headers 
# are included before platform headers.

symbian {
    PREPEND_INCLUDEPATH = $$WEBKIT2_INCLUDEPATH $$PREPEND_INCLUDEPATH
} else {
    INCLUDEPATH = $$WEBKIT2_INCLUDEPATH $$INCLUDEPATH
}

WEBKIT2_GENERATED_HEADERS = \
    $$WEBKIT2_GENERATED_SOURCES_DIR/AuthenticationManagerMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/DownloadProxyMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/PluginControllerProxyMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/PluginProcessMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/PluginProcessProxyMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/PluginProxyMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebContextMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebDatabaseManagerMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebDatabaseManagerProxyMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebGeolocationManagerMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebGeolocationManagerProxyMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebInspectorMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebInspectorProxyMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebPageMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebPageProxyMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebProcessConnectionMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebProcessMessages.h \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebProcessProxyMessages.h

WEBKIT2_GENERATED_SOURCES = \
    $$WEBKIT2_GENERATED_SOURCES_DIR/AuthenticationManagerMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/DownloadProxyMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/PluginControllerProxyMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/PluginProcessMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/PluginProcessProxyMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/PluginProxyMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebContextMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebDatabaseManagerMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebDatabaseManagerProxyMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebGeolocationManagerMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebGeolocationManagerProxyMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebInspectorMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebInspectorProxyMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebPageMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebPageProxyMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebProcessConnectionMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebProcessMessageReceiver.cpp \
    $$WEBKIT2_GENERATED_SOURCES_DIR/WebProcessProxyMessageReceiver.cpp

HEADERS += \
    Platform/CoreIPC/ArgumentDecoder.h \
    Platform/CoreIPC/ArgumentEncoder.h \
    Platform/CoreIPC/Arguments.h \
    Platform/CoreIPC/Attachment.h \
    Platform/CoreIPC/BinarySemaphore.h \
    Platform/CoreIPC/Connection.h \
    Platform/CoreIPC/CoreIPCMessageKinds.h \
    Platform/CoreIPC/DataReference.h \
    Platform/CoreIPC/HandleMessage.h \
    Platform/CoreIPC/MessageID.h \
    Platform/CoreIPC/MessageSender.h \
    Platform/Logging.h \
    Platform/Module.h \
    Platform/PlatformProcessIdentifier.h \
    Platform/RunLoop.h \
    Platform/SharedMemory.h \
    Platform/WorkItem.h \
    Platform/WorkQueue.h \
    Shared/API/c/WKBase.h \
    Shared/API/c/WKCertificateInfo.h \
    Shared/API/c/WKContextMenuItem.h \
    Shared/API/c/WKContextMenuItemTypes.h \
    Shared/API/c/WKGeometry.h \
    Shared/API/c/WKGraphicsContext.h \
    Shared/API/c/WKImage.h \
    Shared/API/c/WKNumber.h \
    Shared/API/c/WKPageLoadTypes.h \
    Shared/API/c/WKSecurityOrigin.h \
    Shared/API/c/WKSerializedScriptValue.h \
    Shared/API/c/WKSharedAPICast.h \
    Shared/API/c/WKString.h \
    Shared/API/c/WKStringPrivate.h \
    Shared/API/c/WKType.h \
    Shared/API/c/WKURL.h \
    Shared/API/c/WKURLRequest.h \
    Shared/API/c/WKURLResponse.h \
    Shared/API/c/WKUserContentURLPattern.h \
    Shared/ShareableBitmap.h \
    Shared/CacheModel.h \
    Shared/ChildProcess.h \
    Shared/CoreIPCSupport/DrawingAreaMessageKinds.h \
    Shared/CoreIPCSupport/DrawingAreaProxyMessageKinds.h \
    Shared/ImageOptions.h \
    Shared/ImmutableArray.h \
    Shared/ImmutableDictionary.h \
    Shared/MutableArray.h \
    Shared/MutableDictionary.h \
    Shared/NativeWebKeyboardEvent.h \
    Shared/NotImplemented.h \
    Shared/OriginAndDatabases.h \
    Shared/PlatformPopupMenuData.h \
    Shared/PrintInfo.h \
    Shared/SameDocumentNavigationType.h \
    Shared/SessionState.h \
    Shared/StringPairVector.h \
    Shared/UserMessageCoders.h \
    Shared/VisitedLinkTable.h \
    Shared/WebBackForwardListItem.h \
    Shared/WebCertificateInfo.h \
    Shared/WebContextMenuItem.h \
    Shared/WebContextMenuItemData.h \
    Shared/WebError.h \
    Shared/WebEvent.h \
    Shared/WebEventConversion.h \
    Shared/WebFindOptions.h \
    Shared/WebGeolocationPosition.h \
    Shared/WebGraphicsContext.h \
    Shared/WebImage.h \
    Shared/WebNavigationDataStore.h \
    Shared/WebNumber.h \
    Shared/WebOpenPanelParameters.h \
    Shared/WebPageCreationParameters.h \
    Shared/WebPageGroupData.h \
    Shared/WebPopupItem.h \
    Shared/WebPreferencesStore.h \
    Shared/WebProcessCreationParameters.h \
    Shared/WebURLRequest.h \
    Shared/WebURLResponse.h \
    Shared/WebUserContentURLPattern.h \
    Shared/Plugins/Netscape/NetscapePluginModule.h \
    Shared/qt/PlatformCertificateInfo.h \
    Shared/qt/UpdateChunk.h \
    Shared/qt/WebEventFactoryQt.h \
    UIProcess/API/C/WKAPICast.h \
    UIProcess/API/C/WKAuthenticationChallenge.h \
    UIProcess/API/C/WKAuthenticationDecisionListener.h \
    UIProcess/API/C/WKBackForwardList.h \
    UIProcess/API/C/WKBackForwardListItem.h \
    UIProcess/API/C/WKContext.h \
    UIProcess/API/C/WKContextPrivate.h \
    UIProcess/API/C/WKCredential.h \
    UIProcess/API/C/WKCredentialTypes.h \
    UIProcess/API/C/WKDatabaseManager.h \
    UIProcess/API/C/WKDownload.h \
    UIProcess/API/C/WKFrame.h \
    UIProcess/API/C/WKFramePolicyListener.h \
    UIProcess/API/C/WKGeolocationManager.h \
    UIProcess/API/C/WKGeolocationPermissionRequest.h \
    UIProcess/API/C/WKGeolocationPosition.h \
    UIProcess/API/C/WKInspector.h \
    UIProcess/API/C/WKOpenPanelParameters.h \
    UIProcess/API/C/WKOpenPanelResultListener.h \
    UIProcess/API/C/WKNavigationData.h \
    UIProcess/API/C/WKPage.h \
    UIProcess/API/C/WKPageGroup.h \
    UIProcess/API/C/WKPagePrivate.h \
    UIProcess/API/C/WKPreferences.h \
    UIProcess/API/C/WKPreferencesPrivate.h \
    UIProcess/API/C/WKProtectionSpace.h \
    UIProcess/API/C/WKProtectionSpaceTypes.h \
    UIProcess/API/C/WebKit2.h \
    UIProcess/API/C/qt/WKNativeEvent.h \
    UIProcess/API/cpp/WKRetainPtr.h \
    UIProcess/API/cpp/qt/WKStringQt.h \
    UIProcess/API/cpp/qt/WKURLQt.h \
    UIProcess/API/qt/ClientImpl.h \
    UIProcess/API/qt/qgraphicswkview.h \
    UIProcess/API/qt/qwkcontext.h \
    UIProcess/API/qt/qwkcontext_p.h \
    UIProcess/API/qt/qwkhistory.h \
    UIProcess/API/qt/qwkhistory_p.h \
    UIProcess/API/qt/qwkpage.h \
    UIProcess/API/qt/qwkpage_p.h \
    UIProcess/API/qt/qwkpreferences.h \
    UIProcess/Authentication/AuthenticationChallengeProxy.h \
    UIProcess/Authentication/AuthenticationDecisionListener.h \
    UIProcess/Authentication/WebCredential.h \
    UIProcess/Authentication/WebProtectionSpace.h \
    UIProcess/ChunkedUpdateDrawingAreaProxy.h \
    UIProcess/Downloads/DownloadProxy.h \
    UIProcess/DrawingAreaProxy.h \
    UIProcess/FindIndicator.h \
    UIProcess/GenericCallback.h \
    UIProcess/GeolocationPermissionRequestManagerProxy.h \
    UIProcess/GeolocationPermissionRequestProxy.h \
    UIProcess/Launcher/ProcessLauncher.h \
    UIProcess/Launcher/ThreadLauncher.h \
    UIProcess/PageClient.h \
    UIProcess/Plugins/PluginInfoStore.h \
    UIProcess/ProcessModel.h \
    UIProcess/ResponsivenessTimer.h \
    UIProcess/TextChecker.h \
    UIProcess/TiledDrawingAreaProxy.h \
    UIProcess/VisitedLinkProvider.h \
    UIProcess/WebContext.h \
    UIProcess/WebContextInjectedBundleClient.h \
    UIProcess/WebContextMenuProxy.h \
    UIProcess/WebContextUserMessageCoders.h \
    UIProcess/WebDatabaseManagerProxy.h \
    UIProcess/WebDatabaseManagerProxyClient.h \
    UIProcess/WebDownloadClient.h \
    UIProcess/WebEditCommandProxy.h \
    UIProcess/WebFindClient.h \
    UIProcess/WebFormClient.h \
    UIProcess/WebFormSubmissionListenerProxy.h \
    UIProcess/WebFrameListenerProxy.h \
    UIProcess/WebFramePolicyListenerProxy.h \
    UIProcess/WebFrameProxy.h \
    UIProcess/WebGeolocationManagerProxy.h \
    UIProcess/WebGeolocationProvider.h \
    UIProcess/WebHistoryClient.h \
    UIProcess/WebInspectorProxy.h \
    UIProcess/WebLoaderClient.h \
    UIProcess/WebNavigationData.h \
    UIProcess/WebPageContextMenuClient.h \
    UIProcess/WebOpenPanelResultListenerProxy.h \
    UIProcess/WebPageGroup.h \
    UIProcess/WebPageProxy.h \
    UIProcess/WebPolicyClient.h \
    UIProcess/WebPreferences.h \
    UIProcess/WebProcessManager.h \
    UIProcess/WebProcessProxy.h \
    UIProcess/WebResourceLoadClient.h \
    UIProcess/WebUIClient.h \
    UIProcess/qt/WebContextMenuProxyQt.h \
    UIProcess/qt/WebPopupMenuProxyQt.h \
    WebProcess/Authentication/AuthenticationManager.h \
    WebProcess/Downloads/Download.h \
    WebProcess/Downloads/DownloadManager.h \
    WebProcess/Geolocation/GeolocationPermissionRequestManager.h \
    WebProcess/Geolocation/WebGeolocationManager.h \
    WebProcess/InjectedBundle/API/c/WKBundleBackForwardList.h \
    WebProcess/InjectedBundle/API/c/WKBundleBackForwardListItem.h \
    WebProcess/InjectedBundle/API/c/WKBundleHitTestResult.h \
    WebProcess/InjectedBundle/API/c/WKBundleNavigationAction.h \
    WebProcess/InjectedBundle/API/c/WKBundleNodeHandle.h \
    WebProcess/InjectedBundle/API/c/WKBundleNodeHandlePrivate.h \
    WebProcess/InjectedBundle/API/c/WKBundlePage.h \
    WebProcess/InjectedBundle/API/c/WKBundlePageGroup.h \
    WebProcess/InjectedBundle/API/c/WKBundlePageOverlay.h \
    WebProcess/InjectedBundle/DOM/InjectedBundleNodeHandle.h \
    WebProcess/InjectedBundle/DOM/InjectedBundleRangeHandle.h \
    WebProcess/InjectedBundle/InjectedBundle.h \
    WebProcess/InjectedBundle/InjectedBundleClient.h \
    WebProcess/InjectedBundle/InjectedBundleHitTestResult.h \
    WebProcess/InjectedBundle/InjectedBundleNavigationAction.h \
    WebProcess/InjectedBundle/InjectedBundlePageContextMenuClient.h \
    WebProcess/InjectedBundle/InjectedBundlePageFormClient.h \
    WebProcess/InjectedBundle/InjectedBundlePagePolicyClient.h \
    WebProcess/InjectedBundle/InjectedBundlePageUIClient.h \
    WebProcess/InjectedBundle/InjectedBundleScriptWorld.h \
    WebProcess/InjectedBundle/InjectedBundleUserMessageCoders.h \
    WebProcess/Plugins/Netscape/JSNPMethod.h \
    WebProcess/Plugins/Netscape/JSNPObject.h \
    WebProcess/Plugins/Netscape/NPJSObject.h \
    WebProcess/Plugins/Netscape/NPRuntimeObjectMap.h \
    WebProcess/Plugins/Netscape/NPRuntimeUtilities.h \
    WebProcess/Plugins/Netscape/NetscapeBrowserFuncs.cpp \
    WebProcess/Plugins/Netscape/NetscapePlugin.h \
    WebProcess/Plugins/Netscape/NetscapePluginStream.h \
    WebProcess/Plugins/Plugin.h \
    WebProcess/Plugins/PluginController.h \
    WebProcess/Plugins/PluginView.h \
    WebProcess/WebCoreSupport/WebChromeClient.h \
    WebProcess/WebCoreSupport/WebContextMenuClient.h \
    WebProcess/WebCoreSupport/WebDatabaseManager.h \
    WebProcess/WebCoreSupport/WebDragClient.h \
    WebProcess/WebCoreSupport/WebEditorClient.h \
    WebProcess/WebCoreSupport/WebErrors.h \
    WebProcess/WebCoreSupport/WebFrameLoaderClient.h \
    WebProcess/WebCoreSupport/WebGeolocationClient.h \
    WebProcess/WebCoreSupport/WebInspectorClient.h \
    WebProcess/WebCoreSupport/WebInspectorFrontendClient.h \
    WebProcess/WebCoreSupport/WebPlatformStrategies.h \
    WebProcess/WebCoreSupport/WebPopupMenu.h \
    WebProcess/WebCoreSupport/WebSearchPopupMenu.h \
    WebProcess/WebCoreSupport/qt/WebFrameNetworkingContext.h \
    WebProcess/WebPage/ChunkedUpdateDrawingArea.h \
    WebProcess/WebPage/DrawingArea.h \
    WebProcess/WebPage/FindController.h \
    WebProcess/WebPage/PageOverlay.h \
    WebProcess/WebPage/WebContextMenu.h \
    WebProcess/WebPage/WebEditCommand.h \
    WebProcess/WebPage/WebFrame.h \
    WebProcess/WebPage/WebInspector.h \
    WebProcess/WebPage/WebOpenPanelResultListener.h \
    WebProcess/WebPage/WebPage.h \
    WebProcess/WebPage/WebPageGroupProxy.h \
    WebProcess/WebProcess.h \
    $$WEBKIT2_GENERATED_HEADERS

SOURCES += \
    Platform/CoreIPC/ArgumentDecoder.cpp \
    Platform/CoreIPC/ArgumentEncoder.cpp \
    Platform/CoreIPC/Attachment.cpp \
    Platform/CoreIPC/BinarySemaphore.cpp \
    Platform/CoreIPC/Connection.cpp \
    Platform/CoreIPC/DataReference.cpp \
    Platform/CoreIPC/qt/AttachmentQt.cpp \
    Platform/CoreIPC/qt/ConnectionQt.cpp \
    Platform/Logging.cpp \
    Platform/Module.cpp \
    Platform/RunLoop.cpp \
    Platform/WorkQueue.cpp \
    Platform/qt/ModuleQt.cpp \
    Platform/qt/RunLoopQt.cpp \
    Platform/qt/SharedMemoryQt.cpp \
    Platform/qt/WorkQueueQt.cpp \
    Shared/API/c/WKArray.cpp \
    Shared/API/c/WKCertificateInfo.cpp \
    Shared/API/c/WKContextMenuItem.cpp \
    Shared/API/c/WKGraphicsContext.cpp \
    Shared/API/c/WKImage.cpp \
    Shared/API/c/WKNumber.cpp \
    Shared/API/c/WKSecurityOrigin.cpp \
    Shared/API/c/WKSerializedScriptValue.cpp \
    Shared/API/c/WKString.cpp \
    Shared/API/c/WKType.cpp \
    Shared/API/c/WKURL.cpp \
    Shared/API/c/WKURLRequest.cpp \
    Shared/API/c/WKURLResponse.cpp \
    Shared/API/c/WKUserContentURLPattern.cpp \
    Shared/Plugins/Netscape/NetscapePluginModule.cpp \
    Shared/Plugins/Netscape/x11/NetscapePluginModuleX11.cpp \
    Shared/ShareableBitmap.cpp \
    Shared/ChildProcess.cpp \
    Shared/ImmutableArray.cpp \
    Shared/ImmutableDictionary.cpp \
    Shared/MutableArray.cpp \
    Shared/MutableDictionary.cpp \
    Shared/OriginAndDatabases.cpp \
    Shared/PlatformPopupMenuData.cpp \
    Shared/PrintInfo.cpp \
    Shared/SessionState.cpp \
    Shared/VisitedLinkTable.cpp \
    Shared/WebBackForwardListItem.cpp \
    Shared/WebContextMenuItem.cpp \
    Shared/WebContextMenuItemData.cpp \
    Shared/WebError.cpp \
    Shared/WebEvent.cpp \
    Shared/WebEventConversion.cpp \
    Shared/WebGeolocationPosition.cpp \
    Shared/WebGraphicsContext.cpp \
    Shared/WebKeyboardEvent.cpp \
    Shared/WebImage.cpp \
    Shared/WebMouseEvent.cpp \
    Shared/WebOpenPanelParameters.cpp \
    Shared/WebPageCreationParameters.cpp \
    Shared/WebPageGroupData.cpp \
    Shared/WebPlatformTouchPoint.cpp \
    Shared/WebPopupItem.cpp \
    Shared/WebPreferencesStore.cpp \
    Shared/WebProcessCreationParameters.cpp \
    Shared/WebTouchEvent.cpp \
    Shared/WebURLRequest.cpp \
    Shared/WebURLResponse.cpp \
    Shared/WebWheelEvent.cpp \
    Shared/qt/ShareableBitmapQt.cpp \
    Shared/qt/NativeWebKeyboardEventQt.cpp \
    Shared/qt/UpdateChunk.cpp \
    Shared/qt/WebCoreArgumentCodersQt.cpp \
    Shared/qt/WebEventFactoryQt.cpp \
    Shared/qt/WebURLRequestQt.cpp \
    Shared/qt/WebURLResponseQt.cpp \
    UIProcess/API/C/WKAuthenticationChallenge.cpp \
    UIProcess/API/C/WKAuthenticationDecisionListener.cpp \
    UIProcess/API/C/WKBackForwardList.cpp \
    UIProcess/API/C/WKBackForwardListItem.cpp \
    UIProcess/API/C/WKContext.cpp \
    UIProcess/API/C/WKCredential.cpp \
    UIProcess/API/C/WKDatabaseManager.cpp \
    UIProcess/API/C/WKDownload.cpp \
    UIProcess/API/C/WKFrame.cpp \
    UIProcess/API/C/WKFramePolicyListener.cpp \
    UIProcess/API/C/WKGeolocationManager.cpp \
    UIProcess/API/C/WKGeolocationPermissionRequest.cpp \
    UIProcess/API/C/WKGeolocationPosition.cpp \
    UIProcess/API/C/WKInspector.cpp \
    UIProcess/API/C/WKOpenPanelParameters.cpp \
    UIProcess/API/C/WKOpenPanelResultListener.cpp \
    UIProcess/API/C/WKNavigationData.cpp \
    UIProcess/API/C/WKPage.cpp \
    UIProcess/API/C/WKPageGroup.cpp \
    UIProcess/API/C/WKPreferences.cpp \
    UIProcess/API/C/WKProtectionSpace.cpp \
    UIProcess/API/cpp/qt/WKStringQt.cpp \
    UIProcess/API/cpp/qt/WKURLQt.cpp \
    UIProcess/API/qt/ClientImpl.cpp \
    UIProcess/API/qt/qgraphicswkview.cpp \
    UIProcess/API/qt/qwkcontext.cpp \
    UIProcess/API/qt/qwkhistory.cpp \
    UIProcess/API/qt/qwkpage.cpp \
    UIProcess/API/qt/qwkpreferences.cpp \
    UIProcess/Authentication/AuthenticationChallengeProxy.cpp \
    UIProcess/Authentication/AuthenticationDecisionListener.cpp \
    UIProcess/Authentication/WebCredential.cpp \
    UIProcess/Authentication/WebProtectionSpace.cpp \
    UIProcess/ChunkedUpdateDrawingAreaProxy.cpp \
    UIProcess/Downloads/DownloadProxy.cpp \
    UIProcess/DrawingAreaProxy.cpp \
    UIProcess/FindIndicator.cpp \
    UIProcess/GeolocationPermissionRequestManagerProxy.cpp \
    UIProcess/GeolocationPermissionRequestProxy.cpp \
    UIProcess/Launcher/ProcessLauncher.cpp \
    UIProcess/Launcher/ThreadLauncher.cpp \
    UIProcess/Launcher/qt/ProcessLauncherQt.cpp \
    UIProcess/Launcher/qt/ThreadLauncherQt.cpp \
    UIProcess/Plugins/PluginInfoStore.cpp \
    UIProcess/Plugins/qt/PluginInfoStoreQt.cpp \
    UIProcess/ResponsivenessTimer.cpp \
    UIProcess/TiledDrawingAreaProxy.cpp \
    UIProcess/VisitedLinkProvider.cpp \
    UIProcess/WebBackForwardList.cpp \
    UIProcess/WebContext.cpp \
    UIProcess/WebContextInjectedBundleClient.cpp \
    UIProcess/WebContextMenuProxy.cpp \
    UIProcess/WebDatabaseManagerProxy.cpp \
    UIProcess/WebDatabaseManagerProxyClient.cpp \
    UIProcess/WebDownloadClient.cpp \
    UIProcess/WebEditCommandProxy.cpp \
    UIProcess/WebFindClient.cpp \
    UIProcess/WebFormClient.cpp \
    UIProcess/WebFormSubmissionListenerProxy.cpp \
    UIProcess/WebFrameListenerProxy.cpp \
    UIProcess/WebFramePolicyListenerProxy.cpp \
    UIProcess/WebFrameProxy.cpp \
    UIProcess/WebGeolocationManagerProxy.cpp \
    UIProcess/WebGeolocationProvider.cpp \
    UIProcess/WebHistoryClient.cpp \
    UIProcess/WebInspectorProxy.cpp \
    UIProcess/WebLoaderClient.cpp \
    UIProcess/WebNavigationData.cpp \
    UIProcess/WebPageContextMenuClient.cpp \
    UIProcess/WebOpenPanelResultListenerProxy.cpp \
    UIProcess/WebPageGroup.cpp \
    UIProcess/WebPageProxy.cpp \
    UIProcess/WebPolicyClient.cpp \
    UIProcess/WebPreferences.cpp \
    UIProcess/WebProcessManager.cpp \
    UIProcess/WebProcessProxy.cpp \
    UIProcess/WebResourceLoadClient.cpp \
    UIProcess/WebUIClient.cpp \
    UIProcess/qt/ChunkedUpdateDrawingAreaProxyQt.cpp \
    UIProcess/qt/TiledDrawingAreaProxyQt.cpp \
    UIProcess/qt/TiledDrawingAreaTileQt.cpp \
    UIProcess/qt/TextCheckerQt.cpp \
    UIProcess/qt/WebContextMenuProxyQt.cpp \
    UIProcess/qt/WebContextQt.cpp \
    UIProcess/qt/WebInspectorProxyQt.cpp \
    UIProcess/qt/WebPageProxyQt.cpp \
    UIProcess/qt/WebPopupMenuProxyQt.cpp \
    UIProcess/qt/WebPreferencesQt.cpp \
    WebProcess/Authentication/AuthenticationManager.cpp \
    WebProcess/Downloads/Download.cpp \
    WebProcess/Downloads/DownloadManager.cpp \
    WebProcess/Geolocation/GeolocationPermissionRequestManager.cpp \
    WebProcess/Geolocation/WebGeolocationManager.cpp \
    WebProcess/Downloads/qt/DownloadQt.cpp \
    WebProcess/InjectedBundle/API/c/WKBundle.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleBackForwardList.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleBackForwardListItem.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleFrame.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleHitTestResult.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleInspector.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleNavigationAction.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleNodeHandle.cpp \
    WebProcess/InjectedBundle/API/c/WKBundlePage.cpp \
    WebProcess/InjectedBundle/API/c/WKBundlePageGroup.cpp \
    WebProcess/InjectedBundle/API/c/WKBundlePageOverlay.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleScriptWorld.cpp \
    WebProcess/InjectedBundle/DOM/InjectedBundleNodeHandle.cpp \
    WebProcess/InjectedBundle/DOM/InjectedBundleRangeHandle.cpp \
    WebProcess/InjectedBundle/InjectedBundle.cpp \
    WebProcess/InjectedBundle/InjectedBundleBackForwardList.cpp \
    WebProcess/InjectedBundle/InjectedBundleBackForwardListItem.cpp \
    WebProcess/InjectedBundle/InjectedBundleClient.cpp \
    WebProcess/InjectedBundle/InjectedBundleHitTestResult.cpp \    
    WebProcess/InjectedBundle/InjectedBundleNavigationAction.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageContextMenuClient.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageEditorClient.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageFormClient.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageLoaderClient.cpp \
    WebProcess/InjectedBundle/InjectedBundlePagePolicyClient.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageResourceLoadClient.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageUIClient.cpp \
    WebProcess/InjectedBundle/InjectedBundleScriptWorld.cpp \
    WebProcess/InjectedBundle/qt/InjectedBundleQt.cpp \
    WebProcess/Plugins/Netscape/JSNPMethod.cpp \
    WebProcess/Plugins/Netscape/JSNPObject.cpp \
    WebProcess/Plugins/Netscape/NPJSObject.cpp \
    WebProcess/Plugins/Netscape/NPRuntimeObjectMap.cpp \
    WebProcess/Plugins/Netscape/NPRuntimeUtilities.cpp \
    WebProcess/Plugins/Netscape/NetscapeBrowserFuncs.cpp \
    WebProcess/Plugins/Netscape/NetscapePlugin.cpp \
    WebProcess/Plugins/Netscape/NetscapePluginStream.cpp \
    WebProcess/Plugins/Netscape/qt/NetscapePluginQt.cpp \
    WebProcess/Plugins/Plugin.cpp \
    WebProcess/Plugins/PluginView.cpp \
    WebProcess/WebCoreSupport/WebChromeClient.cpp \
    WebProcess/WebCoreSupport/WebContextMenuClient.cpp \
    WebProcess/WebCoreSupport/WebDatabaseManager.cpp \
    WebProcess/WebCoreSupport/WebDragClient.cpp \
    WebProcess/WebCoreSupport/WebEditorClient.cpp \
    WebProcess/WebCoreSupport/WebFrameLoaderClient.cpp \
    WebProcess/WebCoreSupport/WebGeolocationClient.cpp \
    WebProcess/WebCoreSupport/WebInspectorClient.cpp \
    WebProcess/WebCoreSupport/WebInspectorFrontendClient.cpp \
    WebProcess/WebCoreSupport/WebPlatformStrategies.cpp \
    WebProcess/WebCoreSupport/WebPopupMenu.cpp \
    WebProcess/WebCoreSupport/WebSearchPopupMenu.cpp \
    WebProcess/WebCoreSupport/qt/WebContextMenuClientQt.cpp \
    WebProcess/WebCoreSupport/qt/WebErrorsQt.cpp \
    WebProcess/WebCoreSupport/qt/WebFrameNetworkingContext.cpp \
    WebProcess/WebCoreSupport/qt/WebPopupMenuQt.cpp \
    WebProcess/WebPage/ChunkedUpdateDrawingArea.cpp \
    WebProcess/WebPage/DecoderAdapter.cpp \
    WebProcess/WebPage/DrawingArea.cpp \
    WebProcess/WebPage/EncoderAdapter.cpp \
    WebProcess/WebPage/FindController.cpp \
    WebProcess/WebPage/PageOverlay.cpp \
    WebProcess/WebPage/TiledDrawingArea.cpp \
    WebProcess/WebPage/WebBackForwardListProxy.cpp \
    WebProcess/WebPage/WebContextMenu.cpp \
    WebProcess/WebPage/WebEditCommand.cpp \
    WebProcess/WebPage/WebFrame.cpp \
    WebProcess/WebPage/WebInspector.cpp \
    WebProcess/WebPage/WebOpenPanelResultListener.cpp \
    WebProcess/WebPage/WebPage.cpp \
    WebProcess/WebPage/WebPageGroupProxy.cpp \
    WebProcess/WebPage/qt/WebInspectorQt.cpp \
    WebProcess/WebPage/qt/ChunkedUpdateDrawingAreaQt.cpp \
    WebProcess/WebPage/qt/TiledDrawingAreaQt.cpp \
    WebProcess/WebPage/qt/WebPageQt.cpp \
    WebProcess/WebProcess.cpp \
    WebProcess/qt/WebProcessMainQt.cpp \
    WebProcess/qt/WebProcessQt.cpp \
    $$WEBKIT2_GENERATED_SOURCES

