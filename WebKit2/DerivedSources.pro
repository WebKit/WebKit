TEMPLATE = lib
TARGET = dummy

CONFIG -= debug_and_release

WEBCORE_GENERATED_HEADERS_FOR_WEBKIT2 += \
    $$OUTPUT_DIR/WebCore/generated/HTMLNames.h \
    $$OUTPUT_DIR/WebCore/generated/JSCSSStyleDeclaration.h \
    $$OUTPUT_DIR/WebCore/generated/JSDOMWindow.h \
    $$OUTPUT_DIR/WebCore/generated/JSElement.h \
    $$OUTPUT_DIR/WebCore/generated/JSHTMLElement.h \


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

fwheader_generator.commands = perl $${SRC_ROOT_DIR}/WebKit2/generate-forwarding-headers.pl $${OUTPUT_DIR}/include qt
fwheader_generator.depends  = $${SRC_ROOT_DIR}/WebKit2/generate-forwarding-headers.pl
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
