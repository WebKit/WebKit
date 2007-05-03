QT -= gui

include(../../WebKit.pri)
INCLUDEPATH += .. ../kjs .
qt-port:INCLUDEPATH += bindings/qt

SOURCES += testqtbindings.cpp

