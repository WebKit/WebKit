include(../tests.pri)
SOURCES += $${TARGET}.cpp
CONFIG += qtwebkit-private
DEFINES += IMPORT_DIR=\"\\\"$${ROOT_BUILD_DIR}$${QMAKE_DIR_SEP}imports\\\"\"
