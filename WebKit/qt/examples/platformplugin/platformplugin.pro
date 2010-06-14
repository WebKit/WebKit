QT       += core gui
TARGET = $$qtLibraryTarget(platformplugin)
TEMPLATE = lib
CONFIG += plugin

DESTDIR = $$[QT_INSTALL_PLUGINS]/webkit

SOURCES += \
    WebPlugin.cpp \
    WebNotificationPresenter.cpp

HEADERS += \
    WebPlugin.h \
    qwebkitplatformplugin.h \
    WebNotificationPresenter.h

!contains(DEFINES, ENABLE_NOTIFICATIONS=.): DEFINES += ENABLE_NOTIFICATIONS=1
