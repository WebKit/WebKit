# WebKit2 - Qt4 build info

CONFIG += building-libs
CONFIG += depend_includepath

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
    WebProcess/InjectedBundle/API/c \
    WebProcess/Plugins \
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
    ../WebKit2/Platform/CoreIPC/ArgumentDecoder.h \
    ../WebKit2/Platform/CoreIPC/ArgumentEncoder.h \
    ../WebKit2/Platform/CoreIPC/Arguments.h \
    ../WebKit2/Platform/CoreIPC/Attachment.h \
    ../WebKit2/Platform/CoreIPC/Connection.h \
    ../WebKit2/Platform/CoreIPC/CoreIPCMessageKinds.h \
    ../WebKit2/Platform/CoreIPC/MessageID.h \
    ../WebKit2/Platform/PlatformProcessIdentifier.h \
    ../WebKit2/Platform/RunLoop.h \
    ../WebKit2/Platform/WorkItem.h \
    ../WebKit2/Platform/WorkQueue.h \
    ../WebKit2/Shared/CoreIPCSupport/DrawingAreaMessageKinds.h \
    ../WebKit2/Shared/CoreIPCSupport/DrawingAreaProxyMessageKinds.h \
    ../WebKit2/Shared/CoreIPCSupport/WebPageMessageKinds.h \
    ../WebKit2/Shared/CoreIPCSupport/WebPageProxyMessageKinds.h \
    ../WebKit2/Shared/CoreIPCSupport/WebProcessMessageKinds.h \
    ../WebKit2/Shared/NotImplemented.h \
    ../WebKit2/Shared/qt/WebEventFactoryQt.h \
    ../WebKit2/Shared/WebEventConversion.h \
    ../WebKit2/Shared/WebEvent.h \
    ../WebKit2/Shared/WebNavigationDataStore.h \
    ../WebKit2/Shared/WebPreferencesStore.h \
    ../WebKit2/UIProcess/API/cpp/WKRetainPtr.h \
    ../WebKit2/UIProcess/API/cpp/qt/WKStringQt.h \
    ../WebKit2/UIProcess/API/cpp/qt/WKURLQt.h \
    ../WebKit2/UIProcess/API/C/WebKit2.h \
    ../WebKit2/UIProcess/API/C/WKAPICast.h \
    ../WebKit2/UIProcess/API/C/WKBase.h \
    ../WebKit2/UIProcess/API/C/WKContext.h \
    ../WebKit2/UIProcess/API/C/WKContextPrivate.h \
    ../WebKit2/UIProcess/API/C/WKFrame.h \
    ../WebKit2/UIProcess/API/C/WKFramePolicyListener.h \
    ../WebKit2/UIProcess/API/C/WKNavigationData.h \
    ../WebKit2/UIProcess/API/C/WKPage.h \
    ../WebKit2/UIProcess/API/C/WKPageNamespace.h \
    ../WebKit2/UIProcess/API/C/WKPagePrivate.h \
    ../WebKit2/UIProcess/API/C/WKPreferences.h \
    ../WebKit2/UIProcess/API/C/WKString.h \
    ../WebKit2/UIProcess/API/C/WKURL.h \
    ../WebKit2/UIProcess/API/qt/qgraphicswkview.h \
    ../WebKit2/UIProcess/API/qt/qwkpage.h \
    ../WebKit2/UIProcess/API/qt/qwkpage_p.h \
    ../WebKit2/UIProcess/ChunkedUpdateDrawingAreaProxy.h \
    ../WebKit2/UIProcess/DrawingAreaProxy.h \
    ../WebKit2/UIProcess/GenericCallback.h \
    ../WebKit2/UIProcess/Launcher/ProcessLauncher.h \
    ../WebKit2/UIProcess/Plugins/PluginInfoStore.h \
    ../WebKit2/UIProcess/PageClient.h \
    ../WebKit2/UIProcess/ProcessModel.h \
    ../WebKit2/UIProcess/API/qt/ClientImpl.h \
    ../WebKit2/UIProcess/ResponsivenessTimer.h \
    ../WebKit2/UIProcess/WebContext.h \
    ../WebKit2/UIProcess/WebContextInjectedBundleClient.h \
    ../WebKit2/UIProcess/WebFramePolicyListenerProxy.h \
    ../WebKit2/UIProcess/WebFrameProxy.h \
    ../WebKit2/UIProcess/WebHistoryClient.h \
    ../WebKit2/UIProcess/WebLoaderClient.h \
    ../WebKit2/UIProcess/WebNavigationData.h \
    ../WebKit2/UIProcess/WebPageNamespace.h \
    ../WebKit2/UIProcess/WebPageProxy.h \
    ../WebKit2/UIProcess/WebPolicyClient.h \
    ../WebKit2/UIProcess/WebPreferences.h \
    ../WebKit2/UIProcess/WebProcessManager.h \
    ../WebKit2/UIProcess/WebProcessProxy.h \
    ../WebKit2/UIProcess/WebUIClient.h \
    ../WebKit2/WebProcess/InjectedBundle/API/c/WKBundleBase.h \
    ../WebKit2/WebProcess/InjectedBundle/API/c/WKBundlePage.h \
    ../WebKit2/WebProcess/InjectedBundle/InjectedBundle.h \
    ../WebKit2/WebProcess/InjectedBundle/InjectedBundlePageUIClient.h \
    ../WebKit2/WebProcess/Plugins/JSNPObject.h \
    ../WebKit2/WebProcess/Plugins/JSNPMethod.h \
    ../WebKit2/WebProcess/Plugins/NPJSObject.h \
    ../WebKit2/WebProcess/Plugins/NPRuntimeObjectMap.h \
    ../WebKit2/WebProcess/Plugins/NPRuntimeUtilities.h \
    ../WebKit2/WebProcess/Plugins/Plugin.h \
    ../WebKit2/WebProcess/Plugins/PluginController.h \
    ../WebKit2/WebProcess/Plugins/PluginView.h \
    ../WebKit2/WebProcess/WebCoreSupport/WebChromeClient.h \
    ../WebKit2/WebProcess/WebCoreSupport/WebContextMenuClient.h \
    ../WebKit2/WebProcess/WebCoreSupport/WebDragClient.h \
    ../WebKit2/WebProcess/WebCoreSupport/WebEditorClient.h \
    ../WebKit2/WebProcess/WebCoreSupport/WebErrors.h \
    ../WebKit2/WebProcess/WebCoreSupport/WebFrameLoaderClient.h \
    ../WebKit2/WebProcess/WebCoreSupport/WebInspectorClient.h \
    ../WebKit2/WebProcess/WebCoreSupport/WebPopupMenu.h \
    ../WebKit2/WebProcess/WebCoreSupport/WebSearchPopupMenu.h \
    ../WebKit2/WebProcess/WebPage/ChunkedUpdateDrawingArea.h \
    ../WebKit2/WebProcess/WebPage/DrawingArea.h \
    ../WebKit2/WebProcess/WebPage/WebFrame.h \
    ../WebKit2/WebProcess/WebPage/WebPage.h \
    ../WebKit2/WebProcess/WebProcess.h \

SOURCES += \
    ../WebKit2/Platform/CoreIPC/ArgumentDecoder.cpp \
    ../WebKit2/Platform/CoreIPC/ArgumentEncoder.cpp \
    ../WebKit2/Platform/CoreIPC/Attachment.cpp \
    ../WebKit2/Platform/CoreIPC/Connection.cpp \
    ../WebKit2/Platform/CoreIPC/qt/ConnectionQt.cpp \
    ../WebKit2/Platform/RunLoop.cpp \
    ../WebKit2/Platform/WorkQueue.cpp \
    ../WebKit2/Platform/qt/RunLoopQt.cpp \
    ../WebKit2/Platform/qt/WorkQueueQt.cpp \
    ../WebKit2/Shared/ImmutableArray.cpp \
    ../WebKit2/Shared/WebEventConversion.cpp \
    ../WebKit2/Shared/WebPreferencesStore.cpp \
    ../WebKit2/Shared/qt/UpdateChunk.cpp \
    ../WebKit2/Shared/qt/WebEventFactoryQt.cpp \
    ../WebKit2/UIProcess/API/C/WKContext.cpp \
    ../WebKit2/UIProcess/API/C/WKFrame.cpp \
    ../WebKit2/UIProcess/API/C/WKFramePolicyListener.cpp \
    ../WebKit2/UIProcess/API/C/WKNavigationData.cpp \
    ../WebKit2/UIProcess/API/C/WKPage.cpp \
    ../WebKit2/UIProcess/API/C/WKPageNamespace.cpp \
    ../WebKit2/UIProcess/API/C/WKPreferences.cpp \
    ../WebKit2/UIProcess/API/C/WKString.cpp \
    ../WebKit2/UIProcess/API/C/WKURL.cpp \
    ../WebKit2/UIProcess/API/qt/ClientImpl.cpp \
    ../WebKit2/UIProcess/API/qt/qgraphicswkview.cpp \
    ../WebKit2/UIProcess/API/qt/qwkpage.cpp \
    ../WebKit2/UIProcess/API/cpp/qt/WKStringQt.cpp \
    ../WebKit2/UIProcess/API/cpp/qt/WKURLQt.cpp \
    ../WebKit2/UIProcess/ChunkedUpdateDrawingAreaProxy.cpp \
    ../WebKit2/UIProcess/DrawingAreaProxy.cpp \
    ../WebKit2/UIProcess/Plugins/PluginInfoStore.cpp \
    ../WebKit2/UIProcess/Plugins/qt/PluginInfoStoreQt.cpp \
    ../WebKit2/UIProcess/Launcher/ProcessLauncher.cpp \
    ../WebKit2/UIProcess/Launcher/qt/ProcessLauncherQt.cpp \
    ../WebKit2/UIProcess/ResponsivenessTimer.cpp \
    ../WebKit2/UIProcess/WebBackForwardList.cpp \
    ../WebKit2/UIProcess/WebBackForwardListItem.cpp \
    ../WebKit2/UIProcess/WebContext.cpp \
    ../WebKit2/UIProcess/WebContextInjectedBundleClient.cpp \
    ../WebKit2/UIProcess/WebFramePolicyListenerProxy.cpp \
    ../WebKit2/UIProcess/WebFrameProxy.cpp \
    ../WebKit2/UIProcess/WebHistoryClient.cpp \
    ../WebKit2/UIProcess/WebLoaderClient.cpp \
    ../WebKit2/UIProcess/WebNavigationData.cpp \
    ../WebKit2/UIProcess/WebPageNamespace.cpp \
    ../WebKit2/UIProcess/WebPageProxy.cpp \
    ../WebKit2/UIProcess/WebPolicyClient.cpp \
    ../WebKit2/UIProcess/WebPreferences.cpp \
    ../WebKit2/UIProcess/WebProcessManager.cpp \
    ../WebKit2/UIProcess/WebProcessProxy.cpp \
    ../WebKit2/UIProcess/WebUIClient.cpp \
    ../WebKit2/WebProcess/InjectedBundle/InjectedBundle.cpp \
    ../WebKit2/WebProcess/InjectedBundle/InjectedBundlePageEditorClient.cpp \
    ../WebKit2/WebProcess/InjectedBundle/InjectedBundlePageUIClient.cpp \
    ../WebKit2/WebProcess/InjectedBundle/InjectedBundlePageLoaderClient.cpp \
    ../WebKit2/WebProcess/InjectedBundle/qt/InjectedBundleQt.cpp \
    ../WebKit2/WebProcess/Plugins/JSNPObject.cpp \
    ../WebKit2/WebProcess/Plugins/JSNPMethod.cpp \
    ../WebKit2/WebProcess/Plugins/NPJSObject.cpp \
    ../WebKit2/WebProcess/Plugins/NPRuntimeObjectMap.cpp \
    ../WebKit2/WebProcess/Plugins/NPRuntimeUtilities.cpp \
    ../WebKit2/WebProcess/Plugins/Plugin.cpp \
    ../WebKit2/WebProcess/Plugins/PluginView.cpp \
    ../WebKit2/WebProcess/WebCoreSupport/WebChromeClient.cpp \
    ../WebKit2/WebProcess/WebCoreSupport/WebContextMenuClient.cpp \
    ../WebKit2/WebProcess/WebCoreSupport/WebDragClient.cpp \
    ../WebKit2/WebProcess/WebCoreSupport/WebEditorClient.cpp \
    ../WebKit2/WebProcess/WebCoreSupport/WebFrameLoaderClient.cpp \
    ../WebKit2/WebProcess/WebCoreSupport/WebInspectorClient.cpp \
    ../WebKit2/WebProcess/WebCoreSupport/WebBackForwardControllerClient.cpp \
    ../WebKit2/WebProcess/WebCoreSupport/WebPopupMenu.cpp \
    ../WebKit2/WebProcess/WebCoreSupport/WebSearchPopupMenu.cpp \
    ../WebKit2/WebProcess/WebCoreSupport/qt/WebErrorsQt.cpp \
    ../WebKit2/WebProcess/WebPage/ChunkedUpdateDrawingArea.cpp \
    ../WebKit2/WebProcess/WebPage/DrawingArea.cpp \
    ../WebKit2/WebProcess/WebPage/WebFrame.cpp \
    ../WebKit2/WebProcess/WebPage/WebPage.cpp \
    ../WebKit2/WebProcess/WebPage/WebBackForwardListProxy.cpp \
    ../WebKit2/WebProcess/WebPage/qt/ChunkedUpdateDrawingAreaQt.cpp \
    ../WebKit2/WebProcess/WebPage/qt/WebPageQt.cpp \
    ../WebKit2/WebProcess/WebProcess.cpp \
    ../WebKit2/UIProcess/qt/ChunkedUpdateDrawingAreaProxyQt.cpp \
    ../WebKit2/UIProcess/qt/WebContextQt.cpp \
    ../WebKit2/WebProcess/qt/WebProcessMainQt.cpp \
