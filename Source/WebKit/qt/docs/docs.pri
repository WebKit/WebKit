qtPrepareTool(QDOC, qdoc)

QDOC = SRCDIR=$$PWD/../../.. OUTPUT_DIR=$${ROOT_BUILD_DIR} $$QDOC

docs.commands = $$QDOC $$PWD/qtwebkit.qdocconf

QMAKE_EXTRA_TARGETS += docs
