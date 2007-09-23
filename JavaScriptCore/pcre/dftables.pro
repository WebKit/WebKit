TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle qt
SOURCES = dftables.c
TARGET = dftables
DESTDIR = tmp

INCLUDEPATH += $$PWD/../wtf

gtk-port {
  DEFINES += BUILDING_GTK__ BUILDING_CAIRO__
} else {
  DEFINES += BUILDING_QT__
}

