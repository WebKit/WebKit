TEMPLATE = lib
TARGET = dummy

CONFIG -= debug_and_release

WEBCORE_HEADERS_FOR_WEBKIT2 = $$system(../WebKitTools/Scripts/enumerate-included-framework-headers WebCore)
JSC_HEADERS_FOR_WEBKIT2 = $$system(../WebKitTools/Scripts/enumerate-included-framework-headers JavaScriptCore)
WEBKIT2_API_HEADERS = $$system(../WebKitTools/Scripts/enumerate-included-framework-headers WebKit2)

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
