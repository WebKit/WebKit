include(../../../WebKit.pri)

unix {
    QDOC = SRCDIR=$$PWD/../../.. OUTPUT_DIR=$$OUTPUT_DIR $$(QTDIR)/bin/qdoc3
} else {
    QDOC = $$(QTDIR)\bin\qdoc3.exe
}

unix {
docs.commands = $$QDOC $$PWD/qtwebkit.qdocconf
} else {
docs.commands = \"$$QDOC $$PWD/qtwebkit.qdocconf\"
}

QMAKE_EXTRA_TARGETS += docs
