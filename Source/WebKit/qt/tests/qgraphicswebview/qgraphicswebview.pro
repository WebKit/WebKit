include(../tests.pri)
exists($${TARGET}.qrc):RESOURCES += $${TARGET}.qrc

contains(DEFINES, ENABLE_WEBGL=1) {
    QT += opengl
}
