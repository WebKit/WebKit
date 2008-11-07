QT -= gui

include(../../WebKit.pri)
INCLUDEPATH += .. ../ .
qt-port:INCLUDEPATH += bindings/qt

SOURCES += testqtbindings.cpp

