QT       += core gui
TARGET = $$qtLibraryTarget(platformplugin)
TEMPLATE = lib
CONFIG += plugin

DESTDIR = $$[QT_INSTALL_PLUGINS]/webkit

SOURCES += \
    WebPlugin.cpp

HEADERS += \
    WebPlugin.h \
    qwebkitplatformplugin.h
