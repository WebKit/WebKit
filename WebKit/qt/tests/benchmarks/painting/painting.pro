isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../../../..
include(../../tests.pri)
exists($${TARGET}.qrc):RESOURCES += $${TARGET}.qrc
