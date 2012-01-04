include(../tests.pri)
exists($${TARGET}.qrc):RESOURCES += $${TARGET}.qrc

load(features)
contains(DEFINES, ENABLE_WEBGL=1) {
    QT += opengl
}
