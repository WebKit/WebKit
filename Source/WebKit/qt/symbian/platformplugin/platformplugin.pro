TEMPLATE = lib
CONFIG += plugin \
          mobility

MOBILITY = multimedia

TARGET = $$qtLibraryTarget(platformplugin)
TARGETPATH = QtWebKit
QT       += core gui \
            network \
            xml

## load mobilityconfig if mobility is available
load(mobilityconfig, true)

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../../../..

SOURCES += \
    WebPlugin.cpp

HEADERS += \
    WebPlugin.h \
    qwebkitplatformplugin.h

DESTDIR = $$OUTPUT_DIR/plugins/$$TARGETPATH

symbian: {
# EPOCALLOWDLLDATA have to set true because Qt macros has initialised global data
    TARGET.EPOCALLOWDLLDATA=1
    TARGET.CAPABILITY = All -Tcb
    TARGET.UID3 = 0x2002E674
    TARGET.VID = VID_DEFAULT
    TARGET = $$TARGET$${QT_LIBINFIX}

    LIBS += -lcone -leikcore -lavkon
}
target.path += $$[QT_INSTALL_PLUGINS]/$$TARGETPATH
INSTALLS += target
