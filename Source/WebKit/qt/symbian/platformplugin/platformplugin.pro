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

contains(MOBILITY_CONFIG, multimedia) {
 
    CONFIG += mobility
    MOBILITY += multimedia
    DEFINES += WTF_USE_QT_MULTIMEDIA=1

    SOURCES += \
        HTML5VideoPlugin.cpp \
        HTML5VideoWidget.cpp \
        OverlayWidget.cpp \
        PlayerButton.cpp \
        PlayerLabel.cpp

    HEADERS += \
        HTML5VideoPlugin.h \
        HTML5VideoWidget.h \
        OverlayWidget.h \
        PlayerButton.h \
        PlayerLabel.h

    RESOURCES = platformplugin.qrc
}

SOURCES += \
    WebPlugin.cpp

HEADERS += \
    WebPlugin.h \
    qwebkitplatformplugin.h

DESTDIR = $$OUTPUT_DIR/plugins/$$TARGETPATH

symbian: {
    TARGET.EPOCALLOWDLLDATA=1
    CONFIG(production) {
        TARGET.CAPABILITY = All -Tcb
    } else {
        TARGET.CAPABILITY = All -Tcb -DRM -AllFiles
    }

    TARGET.UID3 = 0x2002E674
    TARGET = $$TARGET$${QT_LIBINFIX}

    LIBS += -lcone -leikcore -lavkon
}
target.path += $$[QT_INSTALL_PLUGINS]/$$TARGETPATH
INSTALLS += target
