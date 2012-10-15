include(../tests.pri)
SOURCES += $${TARGET}.cpp
QT += webkitwidgets-private
DEFINES += IMPORT_DIR=\"\\\"$${ROOT_BUILD_DIR}$${QMAKE_DIR_SEP}imports\\\"\"
