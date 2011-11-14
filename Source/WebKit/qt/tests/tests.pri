TEMPLATE = app

VPATH += $$_PRO_FILE_PWD_
TARGET = tst_$$TARGET

# Load mobilityconfig if Qt Mobility is available
load(mobilityconfig, true)
contains(MOBILITY_CONFIG, multimedia) {
    # This define is used by tests depending on Qt Multimedia
    DEFINES -= WTF_USE_QT_MULTIMEDIA=0
    DEFINES += WTF_USE_QT_MULTIMEDIA=1
}

SOURCES += $${TARGET}.cpp
INCLUDEPATH += \
    $$PWD \
    $$PWD/../Api

QT += testlib network
haveQt(5): QT += widgets

CONFIG += qtwebkit

haveQt(5) {
    contains(QT_CONFIG, qtquick1): QT += declarative qtquick1
} else {
    contains(QT_CONFIG, declarative): QT += declarative
}

# This define is used by some tests to look up resources in the source tree
DEFINES += TESTS_SOURCE_DIR=\\\"$$PWD/\\\"
