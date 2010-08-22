TEMPLATE = lib
TARGET = dummy

CONFIG -= debug_and_release

WEBCORE_HEADERS_FOR_WEBKIT2 += \
    bindings/js/DOMWrapperWorld.h \
    bindings/js/GCController.h \
    bindings/js/JSPluginElementFunctions.h \
    bindings/js/ScriptController.h \
    bridge/IdentifierRep.h \
    bridge/npruntime_internal.h \
    config.h \
    css/CSSComputedStyleDeclaration.h \
    dom/Event.h \
    dom/KeyboardEvent.h \
    dom/Node.h \
    dom/Range.h \
    dom/UserTypingGestureIndicator.h \
    editing/EditCommand.h \
    editing/EditorInsertAction.h \
    editing/TextAffinity.h \
    history/BackForwardControllerClient.h \
    history/BackForwardList.h \
    history/HistoryItem.h \
    html/HTMLInputElement.h \
    html/HTMLFormElement.h \
    html/HTMLFrameOwnerElement.h \
    html/HTMLPlugInElement.h \
    html/HTMLTextAreaElement.h \
    inspector/InspectorClient.h \
    loader/appcache/ApplicationCacheStorage.h \
    loader/Cache.h \
    loader/DocumentLoader.h \
    loader/FormState.h \
    loader/FrameLoaderClient.h \
    loader/FrameLoader.h \
    loader/FrameLoaderTypes.h \
    loader/NetscapePlugInStreamLoader.h \
    loader/PolicyChecker.h \
    loader/ProgressTracker.h \
    page/animation/AnimationController.h \
    page/ChromeClient.h \
    page/Chrome.h \
    page/ContextMenuClient.h \
    page/DragClient.h \
    page/EditorClient.h \
    page/EventHandler.h \
    page/FocusController.h \
    page/Frame.h \
    page/FrameLoadRequest.h \
    page/FrameView.h \
    page/MediaCanStartListener.h \
    page/PageGroup.h \
    page/Page.h \
    page/SecurityOrigin.h \
    page/Settings.h \
    page/WindowFeatures.h \
    page/ZoomMode.h \
    platform/Cursor.h \
    platform/FileChooser.h \
    platform/FileSystem.h \
    platform/graphics/FloatRect.h \
    platform/graphics/FontRenderingMode.h \
    platform/graphics/GraphicsContext.h \
    platform/graphics/GraphicsLayerClient.h \
    platform/graphics/GraphicsLayer.h \
    platform/graphics/IntPoint.h \
    platform/graphics/IntRect.h \
    platform/graphics/IntSize.h \
    platform/HostWindow.h \
    platform/KURL.h \
    platform/LinkHash.h \
    platform/LocalizationStrategy.h \
    platform/MIMETypeRegistry.h \
    platform/network/android/ResourceError.h \
    platform/network/android/ResourceRequest.h \
    platform/network/android/ResourceResponse.h \
    platform/network/HTTPHeaderMap.h \
    platform/PlatformKeyboardEvent.h \
    platform/PlatformMouseEvent.h \
    platform/PlatformStrategies.h \
    platform/PlatformTouchPoint.h \
    platform/PlatformTouchEvent.h \
    platform/PlatformWheelEvent.h \
    platform/PopupMenu.h \
    platform/PopupMenuClient.h \
    platform/SchemeRegistry.h \
    platform/ScrollView.h \
    platform/SearchPopupMenu.h \
    platform/SharedBuffer.h \
    platform/text/PlatformString.h \
    platform/Widget.h \
    platform/win/BitmapInfo.h \
    platform/WindowsKeyboardCodes.h \
    platform/win/WebCoreInstanceHandle.h \
    platform/win/WindowMessageBroadcaster.h \
    platform/win/WindowMessageListener.h \
    plugins/npfunctions.h \
    plugins/PluginData.h \
    plugins/PluginStrategy.h \
    plugins/PluginViewBase.h \
    rendering/RenderLayer.h \
    rendering/RenderTreeAsText.h \

WEBCORE_GENERATED_HEADERS_FOR_WEBKIT2 += \
    $$OUTPUT_DIR/WebCore/generated/HTMLNames.h \
    $$OUTPUT_DIR/WebCore/generated/JSCSSStyleDeclaration.h \
    $$OUTPUT_DIR/WebCore/generated/JSDOMWindow.h \
    $$OUTPUT_DIR/WebCore/generated/JSElement.h \
    $$OUTPUT_DIR/WebCore/generated/JSHTMLElement.h \

JSC_HEADERS_FOR_WEBKIT2 += \
    API/APICast.h \
    API/JavaScript.h \
    API/JSBase.h \
    runtime/Error.h \
    runtime/FunctionPrototype.h \
    runtime/InternalFunction.h \
    runtime/JSGlobalObject.h \
    runtime/JSLock.h \
    runtime/JSObject.h \
    runtime/JSObjectWithGlobalObject.h \
    runtime/ObjectPrototype.h \
    runtime/Protect.h \
    parser/SourceCode.h \
    wtf/Platform.h \
    wtf/text/StringHash.h \

WEBKIT2_API_HEADERS += \
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
    UIProcess/API/C/WKURL.h \
    UIProcess/API/C/WebKit2.h \
    UIProcess/API/cpp/WKRetainPtr.h \
    UIProcess/API/qt/qgraphicswkview.h \
    UIProcess/API/qt/qwkpage.h \
    WebProcess/InjectedBundle/API/c/WKBundleBase.h \
    WebProcess/InjectedBundle/API/c/WKBundlePage.h \


QUOTE = ""
DOUBLE_ESCAPED_QUOTE = ""
ESCAPE = ""
win32-msvc*|symbian {
    ESCAPE = "^"
} else:win32-g++*:isEmpty(QMAKE_SH) {
    # MinGW's make will run makefile commands using sh, even if make
    #  was run from the Windows shell, if it finds sh in the path.
    ESCAPE = "^"
} else {
    QUOTE = "\'"
    DOUBLE_ESCAPED_QUOTE = "\\\'"
}

DIRS = \
    $$OUTPUT_DIR/include/JavaScriptCore \
    $$OUTPUT_DIR/include/WebCore \
    $$OUTPUT_DIR/include/WebKit2

for(DIR, DIRS) {
    !exists($$DIR): system($$QMAKE_MKDIR $$DIR)
}

QMAKE_EXTRA_TARGETS += createdirs

SRC_ROOT_DIR = $$replace(PWD, /WebKit2, /)

for(HEADER, WEBCORE_HEADERS_FOR_WEBKIT2) {
    DESTDIR_BASE = "WebCore"

    HEADER_NAME = $$basename(HEADER)
    HEADER_PATH = $$SRC_ROOT_DIR/$$DESTDIR_BASE/$$HEADER
    HEADER_TARGET = $$replace(HEADER_PATH, [^a-zA-Z0-9_], -)
    HEADER_TARGET = "qtheader-$${HEADER_TARGET}"
    DESTDIR = $$OUTPUT_DIR/include/$$DESTDIR_BASE

    #FIXME: This should be organized out into a function
    eval($${HEADER_TARGET}.target = $$DESTDIR/$$HEADER_NAME)
    eval($${HEADER_TARGET}.depends = $$HEADER_PATH)
    eval($${HEADER_TARGET}.commands = echo $${DOUBLE_ESCAPED_QUOTE}\$${LITERAL_HASH}include \\\"$$HEADER_PATH\\\"$${DOUBLE_ESCAPED_QUOTE} > $$eval($${HEADER_TARGET}.target))

    QMAKE_EXTRA_TARGETS += $$HEADER_TARGET
    generated_files.depends += $$eval($${HEADER_TARGET}.target)
}

for(HEADER, WEBCORE_GENERATED_HEADERS_FOR_WEBKIT2) {
    HEADER_NAME = $$basename(HEADER)
    HEADER_PATH = $$HEADER
    HEADER_TARGET = $$replace(HEADER_PATH, [^a-zA-Z0-9_], -)
    HEADER_TARGET = "qtheader-$${HEADER_TARGET}"
    DESTDIR = $$OUTPUT_DIR/include/"WebCore"

    eval($${HEADER_TARGET}.target = $$DESTDIR/$$HEADER_NAME)
    eval($${HEADER_TARGET}.depends = $$HEADER_PATH)
    eval($${HEADER_TARGET}.commands = echo $${DOUBLE_ESCAPED_QUOTE}\$${LITERAL_HASH}include \\\"$$HEADER_PATH\\\"$${DOUBLE_ESCAPED_QUOTE} > $$eval($${HEADER_TARGET}.target))

    QMAKE_EXTRA_TARGETS += $$HEADER_TARGET
    generated_files.depends += $$eval($${HEADER_TARGET}.target)
}

for(HEADER, JSC_HEADERS_FOR_WEBKIT2) {
    DESTDIR_BASE = "JavaScriptCore"

    HEADER_NAME = $$basename(HEADER)
    HEADER_PATH = $$SRC_ROOT_DIR/$$DESTDIR_BASE/$$HEADER
    HEADER_TARGET = $$replace(HEADER_PATH, [^a-zA-Z0-9_], -)
    HEADER_TARGET = "qtheader-$${HEADER_TARGET}"
    DESTDIR = $$OUTPUT_DIR/include/$$DESTDIR_BASE

    eval($${HEADER_TARGET}.target = $$DESTDIR/$$HEADER_NAME)
    eval($${HEADER_TARGET}.depends = $$HEADER_PATH)
    eval($${HEADER_TARGET}.commands = echo $${DOUBLE_ESCAPED_QUOTE}\$${LITERAL_HASH}include \\\"$$HEADER_PATH\\\"$${DOUBLE_ESCAPED_QUOTE} > $$eval($${HEADER_TARGET}.target))

    QMAKE_EXTRA_TARGETS += $$HEADER_TARGET
    generated_files.depends += $$eval($${HEADER_TARGET}.target)
}

for(HEADER, WEBKIT2_API_HEADERS) {
    DESTDIR_BASE = "WebKit2"

    HEADER_NAME = $$basename(HEADER)
    HEADER_PATH = $$PWD/$$HEADER
    HEADER_TARGET = $$replace(HEADER_PATH, [^a-zA-Z0-9_], -)
    HEADER_TARGET = "qtheader-$${HEADER_TARGET}"
    DESTDIR = $$OUTPUT_DIR/include/$$DESTDIR_BASE

    eval($${HEADER_TARGET}.target = $$DESTDIR/$$HEADER_NAME)
    eval($${HEADER_TARGET}.depends = $$HEADER_PATH)
    eval($${HEADER_TARGET}.commands = echo $${DOUBLE_ESCAPED_QUOTE}\$${LITERAL_HASH}include \\\"$$HEADER_PATH\\\"$${DOUBLE_ESCAPED_QUOTE} > $$eval($${HEADER_TARGET}.target))

    QMAKE_EXTRA_TARGETS += $$HEADER_TARGET
    generated_files.depends += $$eval($${HEADER_TARGET}.target)
}

QMAKE_EXTRA_TARGETS += generated_files
