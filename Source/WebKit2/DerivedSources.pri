# -------------------------------------------------------------------
# Derived sources for WebKit2
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

# This file is both a top level target, and included from Target.pri,
# so that the resulting generated sources can be added to SOURCES.
# We only set the template if we're a top level target, so that we
# don't override what Target.pri has already set.
sanitizedFile = $$toSanitizedPath($$_FILE_)
equals(sanitizedFile, $$toSanitizedPath($$_PRO_FILE_)):TEMPLATE = derived

load(features)

WEBCORE_GENERATED_SOURCES_DIR = ../WebCore/$${GENERATED_SOURCES_DESTDIR}

SOURCE_DIR = $${ROOT_WEBKIT_DIR}/Source

WEBCORE_GENERATED_HEADERS_FOR_WEBKIT2 += \
    $$WEBCORE_GENERATED_SOURCES_DIR/HTMLNames.h \
    $$WEBCORE_GENERATED_SOURCES_DIR/JSCSSStyleDeclaration.h \
    $$WEBCORE_GENERATED_SOURCES_DIR/JSDOMWindow.h \
    $$WEBCORE_GENERATED_SOURCES_DIR/JSElement.h \
    $$WEBCORE_GENERATED_SOURCES_DIR/JSHTMLElement.h \
    $$WEBCORE_GENERATED_SOURCES_DIR/JSNode.h \
    $$WEBCORE_GENERATED_SOURCES_DIR/JSRange.h \

defineReplace(message_header_generator_output) {
  FILENAME=$$basename(1)
  return($${GENERATED_SOURCES_DESTDIR}/$$replace(FILENAME, ".messages.in", "Messages.h"))
}

defineReplace(message_receiver_generator_output) {
  FILENAME=$$basename(1)
  return($${GENERATED_SOURCES_DESTDIR}/$$replace(FILENAME, ".messages.in", "MessageReceiver.cpp"))
}

VPATH = \
    PluginProcess \
    WebProcess/ApplicationCache \
    WebProcess/Authentication \
    WebProcess/Cookies \
    WebProcess/FullScreen \
    WebProcess/Geolocation \
    WebProcess/IconDatabase \
    WebProcess/KeyValueStorage \
    WebProcess/MediaCache \
    WebProcess/Notifications \
    WebProcess/Plugins \
    WebProcess/ResourceCache \
    WebProcess/WebCoreSupport \
    WebProcess/WebPage \
    WebProcess \
    UIProcess \
    UIProcess/Downloads \
    UIProcess/Notifications \
    UIProcess/Plugins \
    Shared/Plugins

MESSAGE_RECEIVERS = \
    AuthenticationManager.messages.in \
    DownloadProxy.messages.in \
    DrawingAreaProxy.messages.in \
    EventDispatcher.messages.in \
    LayerTreeHostProxy.messages.in \
    PluginControllerProxy.messages.in \
    PluginProcess.messages.in \
    PluginProcessConnection.messages.in \
    PluginProcessProxy.messages.in \
    PluginProxy.messages.in \
    WebApplicationCacheManager.messages.in \
    WebApplicationCacheManagerProxy.messages.in \
    WebContext.messages.in \
    WebCookieManager.messages.in \
    WebCookieManagerProxy.messages.in \
    WebDatabaseManager.messages.in \
    WebDatabaseManagerProxy.messages.in \
    WebGeolocationManager.messages.in \
    WebGeolocationManagerProxy.messages.in \
    WebIconDatabase.messages.in \
    WebIconDatabaseProxy.messages.in \
    WebInspectorProxy.messages.in \
    WebKeyValueStorageManager.messages.in \
    WebKeyValueStorageManagerProxy.messages.in \
    WebMediaCacheManager.messages.in \
    WebMediaCacheManagerProxy.messages.in \
    WebNotificationManagerProxy.messages.in \
    WebNotificationManager.messages.in \
    WebFullScreenManager.messages.in \
    WebFullScreenManagerProxy.messages.in \
    WebPage/DrawingArea.messages.in \
    WebPage/LayerTreeHost.messages.in \
    WebPage/WebInspector.messages.in \
    WebPage/WebPage.messages.in \
    WebPageProxy.messages.in \
    WebProcess.messages.in \
    WebProcessConnection.messages.in \
    WebProcessProxy.messages.in \
    WebResourceCacheManager.messages.in \
    WebResourceCacheManagerProxy.messages.in \
    NPObjectMessageReceiver.messages.in

SCRIPTS = \
    $$PWD/Scripts/generate-message-receiver.py \
    $$PWD/Scripts/generate-messages-header.py \
    $$PWD/Scripts/webkit2/__init__.py \
    $$PWD/Scripts/webkit2/messages.py \
    $$PWD/Scripts/webkit2/model.py \
    $$PWD/Scripts/webkit2/parser.py

message_header_generator.commands = $${PYTHON} $${SOURCE_DIR}/WebKit2/Scripts/generate-messages-header.py ${QMAKE_FILE_IN} > ${QMAKE_FILE_OUT}
message_header_generator.input = MESSAGE_RECEIVERS
message_header_generator.depends = $$SCRIPTS
message_header_generator.output_function = message_header_generator_output
message_header_generator.add_output_to_sources = false
GENERATORS += message_header_generator

message_receiver_generator.commands = $${PYTHON} $${SOURCE_DIR}/WebKit2/Scripts/generate-message-receiver.py  ${QMAKE_FILE_IN} > ${QMAKE_FILE_OUT}
message_receiver_generator.input = MESSAGE_RECEIVERS
message_receiver_generator.depends = $$SCRIPTS
message_receiver_generator.output_function = message_receiver_generator_output
GENERATORS += message_receiver_generator

fwheader_generator.commands = perl $${SOURCE_DIR}/WebKit2/Scripts/generate-forwarding-headers.pl $${SOURCE_DIR}/WebKit2 $${ROOT_BUILD_DIR}/Source/include qt
fwheader_generator.depends = $${SOURCE_DIR}/WebKit2/Scripts/generate-forwarding-headers.pl
generated_files.depends += fwheader_generator
GENERATORS += fwheader_generator

for(header, WEBCORE_GENERATED_HEADERS_FOR_WEBKIT2) {
    header_name = $$basename(header)
    header_path = $$header
    header_target = $$replace(header_path, [^a-zA-Z0-9_], -)
    header_target = "qtheader-$${header_target}"
    dest_dir = $${ROOT_BUILD_DIR}/Source/include/WebCore

    eval($${header_target}.target = $$dest_dir/$$header_name)
    eval($${header_target}.depends = $$header_path)
    eval($${header_target}.commands = $${QMAKE_MKDIR} $$dest_dir && echo $${DOUBLE_ESCAPED_QUOTE}\$${LITERAL_HASH}include \\\"$$header_path\\\"$${DOUBLE_ESCAPED_QUOTE} > $$eval($${header_target}.target))

    GENERATORS += $$header_target
}

