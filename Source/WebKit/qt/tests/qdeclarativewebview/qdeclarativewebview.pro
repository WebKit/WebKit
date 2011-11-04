include(../tests.pri)
exists($${TARGET}.qrc):RESOURCES += $${TARGET}.qrc
INCLUDEPATH += \
    $$PWD/../../declarative

