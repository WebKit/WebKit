TEMPLATE = app
CONFIG -= app_bundle

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

include(../../../WebKit.pri)
QT += testlib network
qt5: QT += widgets

qt5 {
    contains(QT_CONFIG, qtquick1): QT += declarative qtquick1
} else {
    contains(QT_CONFIG, declarative): QT += declarative
}

QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR

symbian {
    TARGET.CAPABILITY = ReadUserData WriteUserData NetworkServices WriteDeviceData
}

# Use the Qt Mobile theme if it is in the CONFIG
use_qt_mobile_theme: DEFINES += WTF_USE_QT_MOBILE_THEME=1

# This define is used by some tests to look up resources in the source tree
!symbian: DEFINES += TESTS_SOURCE_DIR=\\\"$$PWD/\\\"

DEFINES -= QT_ASCII_CAST_WARNINGS

