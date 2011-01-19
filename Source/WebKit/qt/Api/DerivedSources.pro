TEMPLATE = lib
TARGET = dummy

include(headers.pri)

CONFIG -= debug_and_release

DESTDIR = ../../../include/QtWebKit

QUOTE = ""
DOUBLE_ESCAPED_QUOTE = ""
ESCAPE = ""
win32-msvc* | wince* {
    ESCAPE = "^"
} else:contains(QMAKE_HOST.os, "Windows"):isEmpty(QMAKE_SH) {
    # MinGW's make will run makefile commands using sh, even if make
    #  was run from the Windows shell, if it finds sh in the path.
    ESCAPE = "^"
} else {
    QUOTE = "\'"
    DOUBLE_ESCAPED_QUOTE = "\\\'"
    ESCAPE = "\\"
}

qtheader_module.target = $${DESTDIR}/QtWebKit
qtheader_module.depends = $${_PRO_FILE_}
qtheader_module.commands = echo $${QUOTE}$${LITERAL_HASH}ifndef QT_QTWEBKIT_MODULE_H$${QUOTE} > $${qtheader_module.target} &&
qtheader_module.commands += echo $${QUOTE}$${LITERAL_HASH}define QT_QTWEBKIT_MODULE_H$${QUOTE} >> $${qtheader_module.target} &&
qtheader_module.commands += echo $${QUOTE}$${LITERAL_HASH}include $${ESCAPE}<QtNetwork/QtNetwork$${ESCAPE}>$${QUOTE} >> $${qtheader_module.target} &&
WEBKIT_CLASS_HEADERS = $${LITERAL_DOLLAR}$${LITERAL_DOLLAR}$${LITERAL_DOLLAR}$${LITERAL_DOLLAR}PWD/QtWebKit

regex = ".*\\sclass\\sQWEBKIT_EXPORT\\s(\\w+)\\s(.*)"

for(HEADER, WEBKIT_API_HEADERS) {
    # 1. Append to QtWebKit header that includes all other header files
    # Quotes need to be escaped once more when placed in eval()
    eval(qtheader_module.commands += echo $${DOUBLE_ESCAPED_QUOTE}\$${LITERAL_HASH}include \\\"$$basename(HEADER)\\\"$${DOUBLE_ESCAPED_QUOTE} >> $${qtheader_module.target} &&)

    HEADER_NAME = $$basename(HEADER)
    HEADER_TARGET = $$replace(HEADER_NAME, [^a-zA-Z0-9_], -)
    HEADER_TARGET = "qtheader-$${HEADER_TARGET}"

    # 2. Create forwarding header files for qwebframe.h, etc.
    # Normally they contain absolute paths, for package builds we make the path relative so that
    # the package sources are relocatable.

    PATH_TO_HEADER = $$HEADER
    CONFIG(standalone_package): PATH_TO_HEADER = ../../WebKit/qt/Api/$$basename(HEADER)

    eval($${HEADER_TARGET}.target = $${DESTDIR}/$${HEADER_NAME})
    eval($${HEADER_TARGET}.depends = $$HEADER)
    eval($${HEADER_TARGET}.commands = echo $${DOUBLE_ESCAPED_QUOTE}\$${LITERAL_HASH}include \\\"$$PATH_TO_HEADER\\\"$${DOUBLE_ESCAPED_QUOTE} > $$eval($${HEADER_TARGET}.target))

    QMAKE_EXTRA_TARGETS += $$HEADER_TARGET
    qtheader_module.depends += $$eval($${HEADER_TARGET}.target)

    # 3. Extract class names of exported classes from the headers and generate
    # the class name header files

    src_words = $$cat($$HEADER)
    # Really make sure we're dealing with words
    src_words = $$split(src_words, " ")

    src = $$join(src_words, $${LITERAL_WHITESPACE})
    for(ever) {
        # Looking up by line is faster, so we try that first
        res = $$find(src_words, "QWEBKIT_EXPORT")
        isEmpty(res):break()

        # Then do a slow lookup to ensure we're dealing with an exported class
        res = $$find(src, $$regex)
        isEmpty(res):break()

        exp = $$replace(src, $$regex, "EXPORTED_CLASS = \\1")
        eval($$exp)

        CLASS_TARGET = "qtheader_$${EXPORTED_CLASS}"

        eval($${CLASS_TARGET}.target = $${DESTDIR}/$${EXPORTED_CLASS})
        eval($${CLASS_TARGET}.depends = $$eval($${HEADER_TARGET}.target))
        eval($${CLASS_TARGET}.commands = echo $${DOUBLE_ESCAPED_QUOTE}\$${LITERAL_HASH}include \\\"$$basename(HEADER)\\\"$${DOUBLE_ESCAPED_QUOTE} > $$eval($${CLASS_TARGET}.target))

        QMAKE_EXTRA_TARGETS += $$CLASS_TARGET
        WEBKIT_CLASS_HEADERS += $${LITERAL_DOLLAR}$${LITERAL_DOLLAR}$${LITERAL_DOLLAR}$${LITERAL_DOLLAR}PWD/$${EXPORTED_CLASS}

        generated_files.depends += $$eval($${CLASS_TARGET}.target)
        qtheader_pri.depends += $$eval($${CLASS_TARGET}.target)

        # Qt's QRegExp does not support inline non-greedy matching,
        # so we'll have to work around it by updating the haystack
        src = $$replace(src, $$regex, "\\2")
        src_words = $$join(src, $${LITERAL_WHITESPACE})
    }
}

qtheader_module.commands += echo $${QUOTE}$${LITERAL_HASH}endif // QT_QTWEBKIT_MODULE_H$${QUOTE} >> $${qtheader_module.target}
QMAKE_EXTRA_TARGETS += qtheader_module

qtheader_pri.target = $${DESTDIR}/classheaders.pri
qtheader_pri.depends += $${_PRO_FILE_}
qtheader_pri.commands = echo $${QUOTE}WEBKIT_CLASS_HEADERS = $${WEBKIT_CLASS_HEADERS}$${QUOTE} > $${qtheader_pri.target}
QMAKE_EXTRA_TARGETS += qtheader_pri

generated_files.depends += $${qtheader_module.target} $${qtheader_pri.target}
QMAKE_EXTRA_TARGETS += generated_files



