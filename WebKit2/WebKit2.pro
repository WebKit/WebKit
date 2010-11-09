# WebKit2 - Qt4 build info

CONFIG += building-libs
CONFIG += depend_includepath

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

INCLUDEPATH = \
    $$PWD/.. \
    $$PWD/../JavaScriptCore \
    $$PWD/../JavaScriptCore/assembler \
    $$PWD/../JavaScriptCore/bytecode \
    $$PWD/../JavaScriptCore/bytecompiler \
    $$PWD/../JavaScriptCore/debugger \
    $$PWD/../JavaScriptCore/interpreter \
    $$PWD/../JavaScriptCore/jit \
    $$PWD/../JavaScriptCore/parser \
    $$PWD/../JavaScriptCore/pcre \
    $$PWD/../JavaScriptCore/profiler \
    $$PWD/../JavaScriptCore/runtime \
    $$PWD/../JavaScriptCore/wtf \
    $$PWD/../JavaScriptCore/wtf/symbian \
    $$PWD/../JavaScriptCore/wtf/unicode \
    $$PWD/../JavaScriptCore/yarr \
    $$PWD/../JavaScriptCore/API \
    $$PWD/../JavaScriptCore/ForwardingHeaders \
    $$INCLUDEPATH

INCLUDEPATH = \
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
    $$INCLUDEPATH

INCLUDEPATH = \
    $$PWD/../WebCore/bridge/qt \
    $$PWD/../WebCore/page/qt \
    $$PWD/../WebCore/platform/graphics/qt \
    $$PWD/../WebCore/platform/network/qt \
    $$PWD/../WebCore/platform/qt \
    $$PWD/../WebKit/qt/Api \
    $$PWD/../WebKit/qt/WebCoreSupport \
    $$INCLUDEPATH

INCLUDEPATH = \
    $$PWD \
    Platform \
    Platform/CoreIPC \
    Platform/qt \
    Shared \
    Shared/API/c \
    Shared/CoreIPCSupport \
    Shared/qt \
    UIProcess \
    UIProcess/API/C \
    UIProcess/API/cpp \
    UIProcess/API/cpp/qt \
    UIProcess/API/qt \
    UIProcess/Downloads \
    UIProcess/Launcher \
    UIProcess/Plugins \
    UIProcess/qt \
    WebProcess \
    WebProcess/Downloads \
    WebProcess/Downloads/qt \
    WebProcess/InjectedBundle \
    WebProcess/InjectedBundle/DOM \
    WebProcess/InjectedBundle/API/c \
    WebProcess/Plugins \
    WebProcess/Plugins/Netscape \
    WebProcess/WebCoreSupport \
    WebProcess/WebCoreSupport/qt \
    WebProcess/WebPage \
    $$INCLUDEPATH

INCLUDEPATH += \
    $$OUTPUT_DIR/include \
    $$OUTPUT_DIR/WebCore/generated \
    $$OUTPUT_DIR/WebKit2/generated


PREFIX_HEADER = $$PWD/../WebKit2/WebKit2Prefix.h
QMAKE_CXXFLAGS += "-include $$PREFIX_HEADER"

DEFINES += BUILDING_QT__

WEBKIT2_GENERATED_HEADERS = \
    $$OUTPUT_DIR/WebKit2/generated/DownloadProxyMessages.h \
    $$OUTPUT_DIR/WebKit2/generated/PluginControllerProxyMessages.h \
    $$OUTPUT_DIR/WebKit2/generated/PluginProcessMessages.h \
    $$OUTPUT_DIR/WebKit2/generated/PluginProcessProxyMessages.h \
    $$OUTPUT_DIR/WebKit2/generated/PluginProxyMessages.h \
    $$OUTPUT_DIR/WebKit2/generated/WebContextMessages.h \
    $$OUTPUT_DIR/WebKit2/generated/WebInspectorMessages.h \
    $$OUTPUT_DIR/WebKit2/generated/WebPageMessages.h \
    $$OUTPUT_DIR/WebKit2/generated/WebPageProxyMessages.h \
    $$OUTPUT_DIR/WebKit2/generated/WebProcessConnectionMessages.h \
    $$OUTPUT_DIR/WebKit2/generated/WebProcessMessages.h \
    $$OUTPUT_DIR/WebKit2/generated/WebProcessProxyMessages.h

WEBKIT2_GENERATED_SOURCES = \
    $$OUTPUT_DIR/WebKit2/generated/DownloadProxyMessageReceiver.cpp \
    $$OUTPUT_DIR/WebKit2/generated/PluginControllerProxyMessageReceiver.cpp \
    $$OUTPUT_DIR/WebKit2/generated/PluginProcessMessageReceiver.cpp \
    $$OUTPUT_DIR/WebKit2/generated/PluginProcessProxyMessageReceiver.cpp \
    $$OUTPUT_DIR/WebKit2/generated/PluginProxyMessageReceiver.cpp \
    $$OUTPUT_DIR/WebKit2/generated/WebContextMessageReceiver.cpp \
    $$OUTPUT_DIR/WebKit2/generated/WebInspectorMessageReceiver.cpp \
    $$OUTPUT_DIR/WebKit2/generated/WebPageMessageReceiver.cpp \
    $$OUTPUT_DIR/WebKit2/generated/WebPageProxyMessageReceiver.cpp \
    $$OUTPUT_DIR/WebKit2/generated/WebProcessConnectionMessageReceiver.cpp \
    $$OUTPUT_DIR/WebKit2/generated/WebProcessMessageReceiver.cpp \
    $$OUTPUT_DIR/WebKit2/generated/WebProcessProxyMessageReceiver.cpp

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
    Platform/qt/MappedMemoryPool.h \
    Platform/RunLoop.h \
    Platform/SharedMemory.h \
    Platform/WorkItem.h \
    Platform/WorkQueue.h \
    Shared/API/c/WKBase.h \
    Shared/API/c/WKCertificateInfo.h \
    Shared/API/c/WKGeometry.h \
    Shared/API/c/WKNumber.h \
    Shared/API/c/WKSerializedScriptValue.h \
    Shared/API/c/WKSharedAPICast.h \
    Shared/API/c/WKString.h \
    Shared/API/c/WKStringPrivate.h \
    Shared/API/c/WKType.h \
    Shared/API/c/WKURL.h \
    Shared/API/c/WKURLRequest.h \
    Shared/API/c/WKURLResponse.h \
    Shared/API/c/WKUserContentURLPattern.h \
    Shared/CoreIPCSupport/DrawingAreaMessageKinds.h \
    Shared/CoreIPCSupport/DrawingAreaProxyMessageKinds.h \
    Shared/BackingStore.h \
    Shared/CacheModel.h \
    Shared/ChildProcess.h \
    Shared/DrawingAreaBase.h \
    Shared/ImmutableArray.h \
    Shared/ImmutableDictionary.h \
    Shared/MutableArray.h \
    Shared/MutableDictionary.h \
    Shared/NativeWebKeyboardEvent.h \
    Shared/NotImplemented.h \
    Shared/StringPairVector.h \
    Shared/qt/CrashHandler.h \
    Shared/qt/PlatformCertificateInfo.h \
    Shared/qt/UpdateChunk.h \
    Shared/qt/WebEventFactoryQt.h \
    Shared/PlatformPopupMenuData.h \
    Shared/UserMessageCoders.h \
    Shared/VisitedLinkTable.h \
    Shared/WebCertificateInfo.h \
    Shared/WebContextMenuItemData.h \
    Shared/WebEvent.h \
    Shared/WebError.h \
    Shared/WebEventConversion.h \
    Shared/WebNavigationDataStore.h \
    Shared/WebNumber.h \
    Shared/WebPageCreationParameters.h \
    Shared/WebPopupItem.h \
    Shared/WebProcessCreationParameters.h \
    Shared/WebPreferencesStore.h \
    Shared/WebURLRequest.h \
    Shared/WebURLResponse.h \
    Shared/WebUserContentURLPattern.h \
    UIProcess/API/C/WebKit2.h \
    UIProcess/API/C/WKAPICast.h \
    UIProcess/API/C/WKBackForwardList.h \
    UIProcess/API/C/WKBackForwardListItem.h \
    UIProcess/API/C/WKContext.h \
    UIProcess/API/C/WKContextPrivate.h \
    UIProcess/API/C/WKDownload.h \
    UIProcess/API/C/WKFrame.h \
    UIProcess/API/C/WKFramePolicyListener.h \
    UIProcess/API/C/WKInspector.h \
    UIProcess/API/C/WKNavigationData.h \
    UIProcess/API/C/WKPage.h \
    UIProcess/API/C/WKPageNamespace.h \
    UIProcess/API/C/WKPagePrivate.h \
    UIProcess/API/C/WKPreferences.h \
    UIProcess/API/C/WKPreferencesPrivate.h \
    UIProcess/API/C/qt/WKNativeEvent.h \
    UIProcess/API/cpp/qt/WKStringQt.h \
    UIProcess/API/cpp/qt/WKURLQt.h \
    UIProcess/API/cpp/WKRetainPtr.h \
    UIProcess/API/qt/qgraphicswkview.h \
    UIProcess/API/qt/qwkhistory.h \
    UIProcess/API/qt/qwkhistory_p.h \
    UIProcess/API/qt/qwkpage.h \
    UIProcess/API/qt/qwkpage_p.h \
    UIProcess/API/qt/qwkpreferences.h \
    UIProcess/ChunkedUpdateDrawingAreaProxy.h \
    UIProcess/DrawingAreaProxy.h \
    UIProcess/FindIndicator.h \
    UIProcess/GenericCallback.h \
    UIProcess/Downloads/DownloadProxy.h \
    UIProcess/Launcher/ProcessLauncher.h \
    UIProcess/Launcher/ThreadLauncher.h \
    UIProcess/Plugins/PluginInfoStore.h \
    UIProcess/PageClient.h \
    UIProcess/ProcessModel.h \
    UIProcess/API/qt/ClientImpl.h \
    UIProcess/ResponsivenessTimer.h \
    UIProcess/VisitedLinkProvider.h \
    UIProcess/WebContext.h \
    UIProcess/WebContextInjectedBundleClient.h \
    UIProcess/WebContextMenuProxy.h \
    UIProcess/WebContextUserMessageCoders.h \
    UIProcess/WebDownloadClient.h \
    UIProcess/WebEditCommandProxy.h \
    UIProcess/WebFindClient.h \
    UIProcess/WebFormClient.h \
    UIProcess/WebFormSubmissionListenerProxy.h \
    UIProcess/WebFrameListenerProxy.h \
    UIProcess/WebFramePolicyListenerProxy.h \
    UIProcess/WebFrameProxy.h \
    UIProcess/WebHistoryClient.h \
    UIProcess/WebInspectorProxy.h \
    UIProcess/WebLoaderClient.h \
    UIProcess/WebNavigationData.h \
    UIProcess/WebPageNamespace.h \
    UIProcess/WebPageProxy.h \
    UIProcess/WebPolicyClient.h \
    UIProcess/WebPreferences.h \
    UIProcess/WebProcessManager.h \
    UIProcess/WebProcessProxy.h \
    UIProcess/WebUIClient.h \
    UIProcess/qt/WebContextMenuProxyQt.h \
    UIProcess/qt/WebPopupMenuProxyQt.h \
    WebProcess/Downloads/Download.h \
    WebProcess/Downloads/DownloadManager.h \
    WebProcess/InjectedBundle/API/c/WKBundleBackForwardList.h \
    WebProcess/InjectedBundle/API/c/WKBundleBackForwardListItem.h \
    WebProcess/InjectedBundle/API/c/WKBundleHitTestResult.h \
    WebProcess/InjectedBundle/API/c/WKBundleNodeHandle.h \
    WebProcess/InjectedBundle/API/c/WKBundleNodeHandlePrivate.h \
    WebProcess/InjectedBundle/API/c/WKBundlePage.h \
    WebProcess/InjectedBundle/DOM/InjectedBundleNodeHandle.h \
    WebProcess/InjectedBundle/DOM/InjectedBundleRangeHandle.h \
    WebProcess/InjectedBundle/InjectedBundle.h \
    WebProcess/InjectedBundle/InjectedBundleHitTestResult.h \
    WebProcess/InjectedBundle/InjectedBundlePageFormClient.h \
    WebProcess/InjectedBundle/InjectedBundlePageUIClient.h \
    WebProcess/InjectedBundle/InjectedBundleScriptWorld.h \
    WebProcess/InjectedBundle/InjectedBundleUserMessageCoders.h \
    WebProcess/Plugins/Plugin.h \
    WebProcess/Plugins/PluginController.h \
    WebProcess/Plugins/PluginView.h \
    WebProcess/Plugins/Netscape/JSNPObject.h \
    WebProcess/Plugins/Netscape/JSNPMethod.h \
    WebProcess/Plugins/Netscape/NPJSObject.h \
    WebProcess/Plugins/Netscape/NPRuntimeObjectMap.h \
    WebProcess/Plugins/Netscape/NPRuntimeUtilities.h \
    WebProcess/Plugins/Netscape/NetscapeBrowserFuncs.cpp \
    WebProcess/Plugins/Netscape/NetscapePlugin.h \
    WebProcess/Plugins/Netscape/NetscapePluginModule.h \
    WebProcess/Plugins/Netscape/NetscapePluginStream.h \
    WebProcess/WebCoreSupport/WebChromeClient.h \
    WebProcess/WebCoreSupport/WebContextMenuClient.h \
    WebProcess/WebCoreSupport/WebDragClient.h \
    WebProcess/WebCoreSupport/WebEditorClient.h \
    WebProcess/WebCoreSupport/WebErrors.h \
    WebProcess/WebCoreSupport/WebFrameLoaderClient.h \
    WebProcess/WebCoreSupport/WebInspectorClient.h \
    WebProcess/WebCoreSupport/WebInspectorFrontendClient.h \
    WebProcess/WebCoreSupport/WebPopupMenu.h \
    WebProcess/WebCoreSupport/WebSearchPopupMenu.h \
    WebProcess/WebCoreSupport/WebPlatformStrategies.h \
    WebProcess/WebCoreSupport/qt/WebFrameNetworkingContext.h \
    WebProcess/WebPage/ChunkedUpdateDrawingArea.h \
    WebProcess/WebPage/DrawingArea.h \
    WebProcess/WebPage/FindController.h \
    WebProcess/WebPage/FindPageOverlay.h \
    WebProcess/WebPage/PageOverlay.h \
    WebProcess/WebPage/WebContextMenu.h \
    WebProcess/WebPage/WebEditCommand.h \
    WebProcess/WebPage/WebFrame.h \
    WebProcess/WebPage/WebInspector.h \
    WebProcess/WebPage/WebPage.h \
    WebProcess/WebProcess.h \
    $$WEBKIT2_GENERATED_HEADERS

SOURCES += \
    Platform/CoreIPC/ArgumentDecoder.cpp \
    Platform/CoreIPC/ArgumentEncoder.cpp \
    Platform/CoreIPC/Attachment.cpp \
    Platform/CoreIPC/BinarySemaphore.cpp \
    Platform/CoreIPC/Connection.cpp \
    Platform/CoreIPC/DataReference.cpp \
    Platform/CoreIPC/qt/ConnectionQt.cpp \
    Platform/Logging.cpp \
    Platform/Module.cpp \
    Platform/RunLoop.cpp \
    Platform/WorkQueue.cpp \
    Platform/qt/MappedMemoryPool.cpp \
    Platform/qt/ModuleQt.cpp \
    Platform/qt/RunLoopQt.cpp \
    Platform/qt/SharedMemoryQt.cpp \
    Platform/qt/WorkQueueQt.cpp \
    Shared/API/c/WKArray.cpp \
    Shared/API/c/WKCertificateInfo.cpp \
    Shared/API/c/WKNumber.cpp \
    Shared/API/c/WKSerializedScriptValue.cpp \
    Shared/API/c/WKString.cpp \
    Shared/API/c/WKType.cpp \
    Shared/API/c/WKURL.cpp \
    Shared/API/c/WKURLRequest.cpp \
    Shared/API/c/WKURLResponse.cpp \
    Shared/API/c/WKUserContentURLPattern.cpp \
    Shared/BackingStore.cpp \
    Shared/ChildProcess.cpp \
    Shared/ImmutableArray.cpp \
    Shared/ImmutableDictionary.cpp \
    Shared/MutableArray.cpp \
    Shared/MutableDictionary.cpp \
    Shared/qt/BackingStoreQt.cpp \
    Shared/qt/CrashHandler.cpp \
    Shared/qt/NativeWebKeyboardEventQt.cpp \
    Shared/qt/UpdateChunk.cpp \
    Shared/qt/WebCoreArgumentCodersQt.cpp \
    Shared/qt/WebEventFactoryQt.cpp \
    Shared/qt/WebURLRequestQt.cpp \
    Shared/qt/WebURLResponseQt.cpp \
    Shared/PlatformPopupMenuData.cpp \
    Shared/VisitedLinkTable.cpp \
    Shared/WebContextMenuItemData.cpp \
    Shared/WebError.cpp \
    Shared/WebEvent.cpp \
    Shared/WebEventConversion.cpp \
    Shared/WebKeyboardEvent.cpp \
    Shared/WebMouseEvent.cpp \
    Shared/WebPageCreationParameters.cpp \
    Shared/WebPlatformTouchPoint.cpp \
    Shared/WebPopupItem.cpp \
    Shared/WebPreferencesStore.cpp \
    Shared/WebProcessCreationParameters.cpp \
    Shared/WebTouchEvent.cpp \
    Shared/WebURLRequest.cpp \
    Shared/WebURLResponse.cpp \
    Shared/WebWheelEvent.cpp \
    UIProcess/API/C/WKBackForwardList.cpp \
    UIProcess/API/C/WKBackForwardListItem.cpp \
    UIProcess/API/C/WKContext.cpp \
    UIProcess/API/C/WKDownload.cpp \
    UIProcess/API/C/WKFrame.cpp \
    UIProcess/API/C/WKFramePolicyListener.cpp \
    UIProcess/API/C/WKInspector.cpp \
    UIProcess/API/C/WKNavigationData.cpp \
    UIProcess/API/C/WKPage.cpp \
    UIProcess/API/C/WKPageNamespace.cpp \
    UIProcess/API/C/WKPreferences.cpp \
    UIProcess/API/C/WKPreferencesPrivate.cpp \
    UIProcess/API/qt/ClientImpl.cpp \
    UIProcess/API/qt/qgraphicswkview.cpp \
    UIProcess/API/qt/qwkhistory.cpp \
    UIProcess/API/qt/qwkpage.cpp \
    UIProcess/API/qt/qwkpreferences.cpp \
    UIProcess/API/cpp/qt/WKStringQt.cpp \
    UIProcess/API/cpp/qt/WKURLQt.cpp \
    UIProcess/ChunkedUpdateDrawingAreaProxy.cpp \
    UIProcess/DrawingAreaProxy.cpp \
    UIProcess/FindIndicator.cpp \
    UIProcess/Plugins/PluginInfoStore.cpp \
    UIProcess/Plugins/qt/PluginInfoStoreQt.cpp \
    UIProcess/Downloads/DownloadProxy.cpp \
    UIProcess/Launcher/ProcessLauncher.cpp \
    UIProcess/Launcher/ThreadLauncher.cpp \
    UIProcess/Launcher/qt/ProcessLauncherQt.cpp \
    UIProcess/Launcher/qt/ThreadLauncherQt.cpp \
    UIProcess/ResponsivenessTimer.cpp \
    UIProcess/VisitedLinkProvider.cpp \
    UIProcess/WebBackForwardList.cpp \
    UIProcess/WebBackForwardListItem.cpp \
    UIProcess/WebContext.cpp \
    UIProcess/WebContextMenuProxy.cpp \
    UIProcess/WebContextInjectedBundleClient.cpp \
    UIProcess/WebDownloadClient.cpp \
    UIProcess/WebEditCommandProxy.cpp \
    UIProcess/WebFindClient.cpp \
    UIProcess/WebFormClient.cpp \
    UIProcess/WebFormSubmissionListenerProxy.cpp \
    UIProcess/WebFrameListenerProxy.cpp \
    UIProcess/WebFramePolicyListenerProxy.cpp \
    UIProcess/WebFrameProxy.cpp \
    UIProcess/WebHistoryClient.cpp \
    UIProcess/WebInspectorProxy.cpp \
    UIProcess/WebLoaderClient.cpp \
    UIProcess/WebNavigationData.cpp \
    UIProcess/WebPageNamespace.cpp \
    UIProcess/WebPageProxy.cpp \
    UIProcess/WebPolicyClient.cpp \
    UIProcess/WebPreferences.cpp \
    UIProcess/WebProcessManager.cpp \
    UIProcess/WebProcessProxy.cpp \
    UIProcess/WebUIClient.cpp \
    WebProcess/Downloads/Download.cpp \
    WebProcess/Downloads/DownloadManager.cpp \
    WebProcess/Downloads/qt/DownloadQt.cpp \
    WebProcess/InjectedBundle/API/c/WKBundle.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleBackForwardList.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleBackForwardListItem.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleFrame.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleHitTestResult.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleNodeHandle.cpp \
    WebProcess/InjectedBundle/API/c/WKBundlePage.cpp \
    WebProcess/InjectedBundle/API/c/WKBundleScriptWorld.cpp \
    WebProcess/InjectedBundle/DOM/InjectedBundleNodeHandle.cpp \
    WebProcess/InjectedBundle/DOM/InjectedBundleRangeHandle.cpp \
    WebProcess/InjectedBundle/InjectedBundle.cpp \
    WebProcess/InjectedBundle/InjectedBundleBackForwardList.cpp \
    WebProcess/InjectedBundle/InjectedBundleBackForwardListItem.cpp \
    WebProcess/InjectedBundle/InjectedBundleHitTestResult.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageEditorClient.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageFormClient.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageLoaderClient.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageUIClient.cpp \
    WebProcess/InjectedBundle/InjectedBundleScriptWorld.cpp \
    WebProcess/InjectedBundle/qt/InjectedBundleQt.cpp \
    WebProcess/Plugins/Plugin.cpp \
    WebProcess/Plugins/PluginView.cpp \
    WebProcess/Plugins/Netscape/JSNPObject.cpp \
    WebProcess/Plugins/Netscape/JSNPMethod.cpp \
    WebProcess/Plugins/Netscape/NPJSObject.cpp \
    WebProcess/Plugins/Netscape/NPRuntimeObjectMap.cpp \
    WebProcess/Plugins/Netscape/NPRuntimeUtilities.cpp \
    WebProcess/Plugins/Netscape/NetscapeBrowserFuncs.cpp \
    WebProcess/Plugins/Netscape/NetscapePlugin.cpp \
    WebProcess/Plugins/Netscape/NetscapePluginModule.cpp \
    WebProcess/Plugins/Netscape/NetscapePluginStream.cpp \
    WebProcess/Plugins/Netscape/qt/NetscapePluginQt.cpp \
    WebProcess/WebCoreSupport/WebChromeClient.cpp \
    WebProcess/WebCoreSupport/WebContextMenuClient.cpp \
    WebProcess/WebCoreSupport/WebDragClient.cpp \
    WebProcess/WebCoreSupport/WebEditorClient.cpp \
    WebProcess/WebCoreSupport/WebFrameLoaderClient.cpp \
    WebProcess/WebCoreSupport/WebInspectorClient.cpp \
    WebProcess/WebCoreSupport/WebInspectorFrontendClient.cpp \
    WebProcess/WebCoreSupport/WebPopupMenu.cpp \
    WebProcess/WebCoreSupport/WebSearchPopupMenu.cpp \
    WebProcess/WebCoreSupport/WebPlatformStrategies.cpp \
    WebProcess/WebCoreSupport/qt/WebErrorsQt.cpp \
    WebProcess/WebCoreSupport/qt/WebFrameNetworkingContext.cpp \
    WebProcess/WebCoreSupport/qt/WebPopupMenuQt.cpp \
    WebProcess/WebPage/ChunkedUpdateDrawingArea.cpp \
    WebProcess/WebPage/DrawingArea.cpp \
    WebProcess/WebPage/FindController.cpp \
    WebProcess/WebPage/FindPageOverlay.cpp \
    WebProcess/WebPage/PageOverlay.cpp \
    WebProcess/WebPage/WebContextMenu.cpp \
    WebProcess/WebPage/WebEditCommand.cpp \
    WebProcess/WebPage/WebFrame.cpp \
    WebProcess/WebPage/WebInspector.cpp \
    WebProcess/WebPage/WebPage.cpp \
    WebProcess/WebPage/WebBackForwardListProxy.cpp \
    WebProcess/WebPage/qt/ChunkedUpdateDrawingAreaQt.cpp \
    WebProcess/WebPage/qt/WebPageQt.cpp \
    WebProcess/WebProcess.cpp \
    UIProcess/qt/ChunkedUpdateDrawingAreaProxyQt.cpp \
    UIProcess/qt/WebContextQt.cpp \
    UIProcess/qt/WebContextMenuProxyQt.cpp \
    UIProcess/qt/WebPopupMenuProxyQt.cpp \
    WebProcess/qt/WebProcessMainQt.cpp \
    WebProcess/qt/WebProcessQt.cpp \
    $$WEBKIT2_GENERATED_SOURCES
