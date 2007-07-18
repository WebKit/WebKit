TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle qt
SOURCES = dftables.c
TARGET = dftables
DESTDIR = tmp

INCLUDEPATH += $$PWD/../wtf

gdk-port {
  DEFINES += BUILDING_GDK__ BUILDING_CAIRO__
} else {
  DEFINES += BUILDING_QT__
}

