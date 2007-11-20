TEMPLATE = lib
TARGET = qtwebico
CONFIG += plugin
HEADERS += ICOHandler.h
SOURCES += ICOHandler.cpp

include(../../WebKit.pri)

contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols 
unix:contains(QT_CONFIG, reduce_relocations):CONFIG += bsymbolic_functions

target.path = $$[QT_INSTALL_PLUGINS]/imageformats
INSTALLS += target

