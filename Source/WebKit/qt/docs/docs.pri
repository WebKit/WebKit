QDOC = $$QT.core.bins/qdoc

$$unixstyle {
} else {
    QDOC = $$replace(QDOC, "qdoc", "qdoc3.exe")
}

QDOC = SRCDIR=$$PWD/../../.. OUTPUT_DIR=$${ROOT_BUILD_DIR} $$QDOC

docs.commands = $$QDOC $$PWD/qtwebkit.qdocconf

QMAKE_EXTRA_TARGETS += docs
