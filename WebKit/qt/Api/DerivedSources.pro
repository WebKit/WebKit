TEMPLATE = lib
TARGET = dummy

WEBKIT_API_HEADERS = $$PWD/qwebframe.h \
                     $$PWD/qgraphicswebview.h \
                     $$PWD/qwebkitglobal.h \
                     $$PWD/qwebpage.h \
                     $$PWD/qwebview.h \
                     $$PWD/qwebsettings.h \
                     $$PWD/qwebhistoryinterface.h \
                     $$PWD/qwebdatabase.h \
                     $$PWD/qwebsecurityorigin.h \
                     $$PWD/qwebelement.h \
                     $$PWD/qwebpluginfactory.h \
                     $$PWD/qwebhistory.h \
                     $$PWD/qwebinspector.h \
                     $$PWD/qwebkitversion.h

WEBKIT_PRIVATE_HEADERS = $$PWD/qwebdatabase_p.h \
                         $$PWD/qwebframe_p.h \
                         $$PWD/qwebhistory_p.h \
                         $$PWD/qwebinspector_p.h \
                         $$PWD/qwebpage_p.h \
                         $$PWD/qwebplugindatabase_p.h \
                         $$PWD/qwebsecurityorigin_p.h


CONFIG -= debug_and_release

DESTDIR = ../../../include

qtheader_module.target = $${DESTDIR}/QtWebKit
qtheader_module.depends = $${_PRO_FILE_}
eval(qtheader_module.commands = echo \\\'\$${LITERAL_HASH}ifndef QT_QTWEBKIT_MODULE_H\\\' > $${qtheader_module.target};)
eval(qtheader_module.commands += echo \\\'\$${LITERAL_HASH}define QT_QTWEBKIT_MODULE_H\\\' >> $${qtheader_module.target};)
eval(qtheader_module.commands += echo \\\'\$${LITERAL_HASH}include <QtNetwork/QtNetwork>\\\' >> $${qtheader_module.target};)
WEBKIT_CLASS_HEADERS = ../include/QtWebKit

regex = ".*\sclass\sQWEBKIT_EXPORT\s(\w+)\s(.*)"

for(HEADER, WEBKIT_API_HEADERS) {
    qtheader_module.depends += $$HEADER
    eval(qtheader_module.commands += echo \\\'\$${LITERAL_HASH}include \\\"$$basename(HEADER)\\\"\\\' >> $${qtheader_module.target};)

    HEADER_NAME = $$basename(HEADER)
    HEADER_TARGET = $$replace(HEADER_NAME, [^a-zA-Z0-9_], -)
    HEADER_TARGET = "qtheader-$${HEADER_TARGET}"

    eval($${HEADER_TARGET}.target = $${DESTDIR}/$${HEADER_NAME})
    eval($${HEADER_TARGET}.depends = $$HEADER)
    eval($${HEADER_TARGET}.commands = echo \\\'\$${LITERAL_HASH}include \\\"$$HEADER\\\"\\\' > $$eval($${HEADER_TARGET}.target))

    QMAKE_EXTRA_TARGETS += $$HEADER_TARGET

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

        exp = $$replace(src, $$regex, "EXPORTED_CLASS = \1")
        eval($$exp)

        CLASS_TARGET = "qtheader_$${EXPORTED_CLASS}"

        eval($${CLASS_TARGET}.target = $${DESTDIR}/$${EXPORTED_CLASS})
        eval($${CLASS_TARGET}.depends = $$eval($${HEADER_TARGET}.target))
        eval($${CLASS_TARGET}.commands = echo \\\'\$${LITERAL_HASH}include \\\"$$basename(HEADER)\\\"\\\' > $$eval($${CLASS_TARGET}.target))

        QMAKE_EXTRA_TARGETS += $$CLASS_TARGET
        WEBKIT_CLASS_HEADERS += ../include/$${EXPORTED_CLASS}

        generated_files.depends += $$eval($${CLASS_TARGET}.target)
        qtheader_pri.depends += $$eval($${CLASS_TARGET}.target)

        # Qt's QRegExp does not support inline non-greedy matching,
        # so we'll have to work around it by updating the haystack
        src = $$replace(src, $$regex, "\2")
        src_words = $$join(src, $${LITERAL_WHITESPACE})
    }
}

eval(qtheader_module.commands += echo \\\'\$${LITERAL_HASH}endif // QT_QTWEBKIT_MODULE_H\\\' >> $${qtheader_module.target})
QMAKE_EXTRA_TARGETS += qtheader_module

qtheader_pri.target = $${DESTDIR}/headers.pri
qtheader_pri.depends = $${WEBKIT_API_HEADERS} $${WEBKIT_PRIVATE_HEADERS} $${_PRO_FILE_}
eval(qtheader_pri.commands = echo \\\'WEBKIT_API_HEADERS = $${WEBKIT_API_HEADERS}\\\' > $${qtheader_pri.target};)
eval(qtheader_pri.commands += echo \\\'WEBKIT_CLASS_HEADERS = $${WEBKIT_CLASS_HEADERS}\\\' >> $${qtheader_pri.target};)
eval(qtheader_pri.commands += echo \\\'WEBKIT_PRIVATE_HEADERS = $${WEBKIT_PRIVATE_HEADERS}\\\' >> $${qtheader_pri.target};)
QMAKE_EXTRA_TARGETS += qtheader_pri

generated_files.depends += $${qtheader_module.target} $${qtheader_pri.target}
QMAKE_EXTRA_TARGETS += generated_files



