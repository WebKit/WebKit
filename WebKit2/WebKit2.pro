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
QT += network

!CONFIG(release, debug|release) {
    OBJECTS_DIR = obj/debug
} else { # Release
    OBJECTS_DIR = obj/release
    DEFINES += NDEBUG
}

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
    Platform \
    Platform/CoreIPC \
    Shared \
    Shared/CoreIPCSupport \
    Shared/qt \
    UIProcess \
    UIProcess/API/C \
    UIProcess/API/cpp \
    UIProcess/API/cpp/qt \
    UIProcess/API/qt \
    UIProcess/Launcher \
    UIProcess/Plugins \
    UIProcess/qt \
    WebProcess \
    WebProcess/InjectedBundle \
    WebProcess/InjectedBundle/DOM \
    WebProcess/InjectedBundle/API/c \
    WebProcess/Plugins \
    WebProcess/Plugins/Netscape \
    WebProcess/WebCoreSupport \
    WebProcess/WebPage \
    $$INCLUDEPATH

INCLUDEPATH += \
    $$OUTPUT_DIR/include \
    $$OUTPUT_DIR/WebCore/generated


PREFIX_HEADER = $$PWD/../WebKit2/WebKit2Prefix.h
QMAKE_CXXFLAGS += "-include $$PREFIX_HEADER"

DEFINES += BUILDING_QT__

HEADERS += \
    Platform/CoreIPC/ArgumentDecoder.h \
    Platform/CoreIPC/ArgumentEncoder.h \
    Platform/CoreIPC/Arguments.h \
    Platform/CoreIPC/Attachment.h \
    Platform/CoreIPC/Connection.h \
    Platform/CoreIPC/CoreIPCMessageKinds.h \
    Platform/CoreIPC/MessageID.h \
    Platform/Module.h \
    Platform/PlatformProcessIdentifier.h \
    Platform/RunLoop.h \
    Platform/SharedMemory.h \
    Platform/WorkItem.h \
    Platform/WorkQueue.h \
    Shared/CoreIPCSupport/DrawingAreaMessageKinds.h \
    Shared/CoreIPCSupport/DrawingAreaProxyMessageKinds.h \
    Shared/CoreIPCSupport/WebPageMessageKinds.h \
    Shared/CoreIPCSupport/WebPageProxyMessageKinds.h \
    Shared/CoreIPCSupport/WebProcessMessageKinds.h \
    Shared/DrawingAreaBase.h \
    Shared/ImmutableArray.h \
    Shared/ImmutableDictionary.h \
    Shared/MutableArray.h \
    Shared/NotImplemented.h \
    Shared/qt/WebEventFactoryQt.h \
    Shared/VisitedLinkTable.h \
    Shared/WebEventConversion.h \
    Shared/WebEvent.h \
    Shared/WebNavigationDataStore.h \
    Shared/WebPreferencesStore.h \
    Shared/WebURLRequest.h \
    Shared/WebURLResponse.h \
    UIProcess/API/cpp/WKRetainPtr.h \
    UIProcess/API/cpp/qt/WKStringQt.h \
    UIProcess/API/cpp/qt/WKURLQt.h \
    UIProcess/API/C/WebKit2.h \
    UIProcess/API/C/WKAPICast.h \
    UIProcess/API/C/WKBase.h \
    UIProcess/API/C/WKContext.h \
    UIProcess/API/C/WKContextPrivate.h \
    UIProcess/API/C/WKFrame.h \
    UIProcess/API/C/WKFramePolicyListener.h \
    UIProcess/API/C/WKNavigationData.h \
    UIProcess/API/C/WKPage.h \
    UIProcess/API/C/WKPageNamespace.h \
    UIProcess/API/C/WKPagePrivate.h \
    UIProcess/API/C/WKPreferences.h \
    UIProcess/API/C/WKString.h \
    UIProcess/API/C/WKType.h \
    UIProcess/API/C/WKURL.h \
    UIProcess/API/C/WKURLRequest.h \
    UIProcess/API/C/WKURLResponse.h \
    UIProcess/API/qt/qgraphicswkview.h \
    UIProcess/API/qt/qwkpage.h \
    UIProcess/API/qt/qwkpage_p.h \
    UIProcess/ChunkedUpdateDrawingAreaProxy.h \
    UIProcess/DrawingAreaProxy.h \
    UIProcess/GenericCallback.h \
    UIProcess/Launcher/ProcessLauncher.h \
    UIProcess/Plugins/PluginInfoStore.h \
    UIProcess/PageClient.h \
    UIProcess/ProcessModel.h \
    UIProcess/API/qt/ClientImpl.h \
    UIProcess/ResponsivenessTimer.h \
    UIProcess/VisitedLinkProvider.h \
    UIProcess/WebContext.h \
    UIProcess/WebContextInjectedBundleClient.h \
    UIProcess/WebFormClient.h \
    UIProcess/WebFormSubmissionListenerProxy.h \
    UIProcess/WebFrameListenerProxy.h \
    UIProcess/WebFramePolicyListenerProxy.h \
    UIProcess/WebFrameProxy.h \
    UIProcess/WebHistoryClient.h \
    UIProcess/WebLoaderClient.h \
    UIProcess/WebNavigationData.h \
    UIProcess/WebPageNamespace.h \
    UIProcess/WebPageProxy.h \
    UIProcess/WebPolicyClient.h \
    UIProcess/WebPreferences.h \
    UIProcess/WebProcessManager.h \
    UIProcess/WebProcessProxy.h \
    UIProcess/WebUIClient.h \
    WebProcess/InjectedBundle/API/c/WKBundleBase.h \
    WebProcess/InjectedBundle/API/c/WKBundlePage.h \
    WebProcess/InjectedBundle/DOM/InjectedBundleNodeHandle.h \
    WebProcess/InjectedBundle/InjectedBundle.h \
    WebProcess/InjectedBundle/InjectedBundlePageFormClient.h \
    WebProcess/InjectedBundle/InjectedBundlePageUIClient.h \
    WebProcess/InjectedBundle/InjectedBundleScriptWorld.h \
    WebProcess/Plugins/JSNPObject.h \
    WebProcess/Plugins/JSNPMethod.h \
    WebProcess/Plugins/NPJSObject.h \
    WebProcess/Plugins/NPRuntimeObjectMap.h \
    WebProcess/Plugins/NPRuntimeUtilities.h \
    WebProcess/Plugins/Plugin.h \
    WebProcess/Plugins/PluginController.h \
    WebProcess/Plugins/PluginView.h \
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
    WebProcess/WebCoreSupport/WebPopupMenu.h \
    WebProcess/WebCoreSupport/WebSearchPopupMenu.h \
    WebProcess/WebPage/ChunkedUpdateDrawingArea.h \
    WebProcess/WebPage/DrawingArea.h \
    WebProcess/WebPage/WebFrame.h \
    WebProcess/WebPage/WebPage.h \
    WebProcess/WebProcess.h \

SOURCES += \
    Platform/CoreIPC/ArgumentDecoder.cpp \
    Platform/CoreIPC/ArgumentEncoder.cpp \
    Platform/CoreIPC/Attachment.cpp \
    Platform/CoreIPC/Connection.cpp \
    Platform/CoreIPC/qt/ConnectionQt.cpp \
    Platform/Module.cpp \
    Platform/RunLoop.cpp \
    Platform/WorkQueue.cpp \
    Platform/qt/ModuleQt.cpp \
    Platform/qt/RunLoopQt.cpp \
    Platform/qt/SharedMemoryQt.cpp \
    Platform/qt/WorkQueueQt.cpp \
    Shared/DrawingAreaBase.cpp \
    Shared/ImmutableArray.cpp \
    Shared/ImmutableDictionary.cpp \
    Shared/MutableArray.cpp \
    Shared/VisitedLinkTable.cpp \
    Shared/WebEventConversion.cpp \
    Shared/WebPreferencesStore.cpp \
    Shared/WebURLRequest.cpp \
    Shared/WebURLResponse.cpp \
    Shared/qt/UpdateChunk.cpp \
    Shared/qt/WebEventFactoryQt.cpp \
    Shared/qt/WebCoreArgumentCodersQt.cpp \
    Shared/qt/WebURLRequestQt.cpp \
    Shared/qt/WebURLResponseQt.cpp \
    UIProcess/API/C/WKContext.cpp \
    UIProcess/API/C/WKFrame.cpp \
    UIProcess/API/C/WKFramePolicyListener.cpp \
    UIProcess/API/C/WKNavigationData.cpp \
    UIProcess/API/C/WKPage.cpp \
    UIProcess/API/C/WKPageNamespace.cpp \
    UIProcess/API/C/WKPreferences.cpp \
    UIProcess/API/C/WKString.cpp \
    UIProcess/API/C/WKType.cpp \
    UIProcess/API/C/WKURL.cpp \
    UIProcess/API/C/WKURLRequest.cpp \
    UIProcess/API/C/WKURLResponse.cpp \
    UIProcess/API/qt/ClientImpl.cpp \
    UIProcess/API/qt/qgraphicswkview.cpp \
    UIProcess/API/qt/qwkpage.cpp \
    UIProcess/API/cpp/qt/WKStringQt.cpp \
    UIProcess/API/cpp/qt/WKURLQt.cpp \
    UIProcess/ChunkedUpdateDrawingAreaProxy.cpp \
    UIProcess/DrawingAreaProxy.cpp \
    UIProcess/Plugins/PluginInfoStore.cpp \
    UIProcess/Plugins/qt/PluginInfoStoreQt.cpp \
    UIProcess/Launcher/ProcessLauncher.cpp \
    UIProcess/Launcher/qt/ProcessLauncherQt.cpp \
    UIProcess/ResponsivenessTimer.cpp \
    UIProcess/VisitedLinkProvider.cpp \
    UIProcess/WebBackForwardList.cpp \
    UIProcess/WebBackForwardListItem.cpp \
    UIProcess/WebContext.cpp \
    UIProcess/WebContextInjectedBundleClient.cpp \
    UIProcess/WebFormClient.cpp \
    UIProcess/WebFormSubmissionListenerProxy.cpp \
    UIProcess/WebFrameListenerProxy.cpp \
    UIProcess/WebFramePolicyListenerProxy.cpp \
    UIProcess/WebFrameProxy.cpp \
    UIProcess/WebHistoryClient.cpp \
    UIProcess/WebLoaderClient.cpp \
    UIProcess/WebNavigationData.cpp \
    UIProcess/WebPageNamespace.cpp \
    UIProcess/WebPageProxy.cpp \
    UIProcess/WebPolicyClient.cpp \
    UIProcess/WebPreferences.cpp \
    UIProcess/WebProcessManager.cpp \
    UIProcess/WebProcessProxy.cpp \
    UIProcess/WebUIClient.cpp \
    WebProcess/InjectedBundle/DOM/InjectedBundleNodeHandle.cpp \
    WebProcess/InjectedBundle/InjectedBundle.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageEditorClient.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageFormClient.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageUIClient.cpp \
    WebProcess/InjectedBundle/InjectedBundlePageLoaderClient.cpp \
    WebProcess/InjectedBundle/InjectedBundleScriptWorld.cpp \
    WebProcess/InjectedBundle/qt/InjectedBundleQt.cpp \
    WebProcess/Plugins/JSNPObject.cpp \
    WebProcess/Plugins/JSNPMethod.cpp \
    WebProcess/Plugins/NPJSObject.cpp \
    WebProcess/Plugins/NPRuntimeObjectMap.cpp \
    WebProcess/Plugins/NPRuntimeUtilities.cpp \
    WebProcess/Plugins/Plugin.cpp \
    WebProcess/Plugins/PluginView.cpp \
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
    WebProcess/WebCoreSupport/WebBackForwardControllerClient.cpp \
    WebProcess/WebCoreSupport/WebPopupMenu.cpp \
    WebProcess/WebCoreSupport/WebSearchPopupMenu.cpp \
    WebProcess/WebCoreSupport/qt/WebErrorsQt.cpp \
    WebProcess/WebPage/ChunkedUpdateDrawingArea.cpp \
    WebProcess/WebPage/DrawingArea.cpp \
    WebProcess/WebPage/WebFrame.cpp \
    WebProcess/WebPage/WebPage.cpp \
    WebProcess/WebPage/WebBackForwardListProxy.cpp \
    WebProcess/WebPage/qt/ChunkedUpdateDrawingAreaQt.cpp \
    WebProcess/WebPage/qt/WebPageQt.cpp \
    WebProcess/WebProcess.cpp \
    UIProcess/qt/ChunkedUpdateDrawingAreaProxyQt.cpp \
    UIProcess/qt/WebContextQt.cpp \
    WebProcess/qt/WebProcessMainQt.cpp \
