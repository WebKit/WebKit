TEMPLATE = lib
TARGET = dummy

CONFIG -= debug_and_release

WEBCORE_GENERATED_HEADERS_FOR_WEBKIT2 += \
    $$OUTPUT_DIR/WebCore/generated/HTMLNames.h \
    $$OUTPUT_DIR/WebCore/generated/JSCSSStyleDeclaration.h \
    $$OUTPUT_DIR/WebCore/generated/JSDOMWindow.h \
    $$OUTPUT_DIR/WebCore/generated/JSElement.h \
    $$OUTPUT_DIR/WebCore/generated/JSHTMLElement.h \
    $$OUTPUT_DIR/WebCore/generated/JSNode.h \
    $$OUTPUT_DIR/WebCore/generated/JSRange.h \


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

SBOX_CHECK = $$(_SBOX_DIR)
!isEmpty(SBOX_CHECK) {
    PYTHON = python2.6
} else {
    PYTHON = python
}


DIRS = \
    $$OUTPUT_DIR/include/JavaScriptCore \
    $$OUTPUT_DIR/include/WebCore \
    $$OUTPUT_DIR/include/WebKit2 \
    $$OUTPUT_DIR/WebKit2/generated

for(DIR, DIRS) {
    !exists($$DIR): system($$QMAKE_MKDIR $$DIR)
}

QMAKE_EXTRA_TARGETS += createdirs

SRC_ROOT_DIR = $$replace(PWD, /WebKit2, /)

defineTest(addExtraCompiler) {
    eval($${1}.CONFIG = target_predeps no_link)
    eval($${1}.variable_out =)
    eval($${1}.dependency_type = TYPE_C)

    wkScript = $$eval($${1}.wkScript)
    eval($${1}.depends += $$wkScript)

    export($${1}.CONFIG)
    export($${1}.variable_out)
    export($${1}.dependency_type)
    export($${1}.depends)

    QMAKE_EXTRA_COMPILERS += $$1
    generated_files.depends += compiler_$${1}_make_all
    export(QMAKE_EXTRA_COMPILERS)
    export(generated_files.depends)
    return(true)
}

defineReplace(message_header_generator_output) {
  FILENAME=$$basename(1)
  return($$OUTPUT_DIR/WebKit2/generated/$$replace(FILENAME, ".messages.in","Messages.h"))
}

defineReplace(message_receiver_generator_output) {
  FILENAME=$$basename(1)
  return($$OUTPUT_DIR/WebKit2/generated/$$replace(FILENAME, ".messages.in","MessageReceiver.cpp"))
}

VPATH = \
    PluginProcess \
    WebProcess/Plugins \
    WebProcess/WebPage \
    WebProcess \
    UIProcess \
    UIProcess/Downloads \
    UIProcess/Plugins

MESSAGE_RECEIVERS = \
    DownloadProxy.messages.in \
    PluginControllerProxy.messages.in \
    PluginProcess.messages.in \
    PluginProcessProxy.messages.in \
    PluginProxy.messages.in \
    WebContext.messages.in \
    WebInspectorProxy.messages.in \
    WebPage/WebInspector.messages.in \
    WebPage/WebPage.messages.in \
    WebPageProxy.messages.in \
    WebProcess.messages.in \
    WebProcessConnection.messages.in \
    WebProcessProxy.messages.in

SCRIPTS = \
    $$PWD/Scripts/generate-message-receiver.py \
    $$PWD/Scripts/generate-messages-header.py \
    $$PWD/Scripts/webkit2/__init__.py \
    $$PWD/Scripts/webkit2/messages.py

message_header_generator.commands = $${PYTHON} $${SRC_ROOT_DIR}WebKit2/Scripts/generate-messages-header.py ${QMAKE_FILE_IN} > ${QMAKE_FILE_OUT}
message_header_generator.input = MESSAGE_RECEIVERS
message_header_generator.depends = $$SCRIPTS
message_header_generator.output_function = message_header_generator_output
addExtraCompiler(message_header_generator)

message_receiver_generator.commands = $${PYTHON} $${SRC_ROOT_DIR}WebKit2/Scripts/generate-message-receiver.py  ${QMAKE_FILE_IN} > ${QMAKE_FILE_OUT}
message_receiver_generator.input = MESSAGE_RECEIVERS
message_receiver_generator.depends = $$SCRIPTS
message_receiver_generator.output_function = message_receiver_generator_output
addExtraCompiler(message_receiver_generator)

fwheader_generator.commands = perl $${SRC_ROOT_DIR}/WebKitTools/Scripts/generate-forwarding-headers.pl $${SRC_ROOT_DIR}/WebKit2 $${OUTPUT_DIR}/include qt
fwheader_generator.depends  = $${SRC_ROOT_DIR}/WebKitTools/Scripts/generate-forwarding-headers.pl
generated_files.depends     += fwheader_generator
QMAKE_EXTRA_TARGETS         += fwheader_generator

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

QMAKE_EXTRA_TARGETS += generated_files
