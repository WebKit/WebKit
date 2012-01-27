# -------------------------------------------------------------------
# This file contains shared rules used both when building WTF itself
# and for targets that depend in some way on WTF.
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

load(features)

SOURCE_DIR = $${ROOT_WEBKIT_DIR}/Source/WTF
OLD_SOURCE_DIR = $${ROOT_WEBKIT_DIR}/Source/JavaScriptCore/wtf

INCLUDEPATH += \
    $$OLD_SOURCE_DIR/.. \
    $$OLD_SOURCE_DIR \
    $$OLD_SOURCE_DIR/gobject \
    $$OLD_SOURCE_DIR/qt \
    $$OLD_SOURCE_DIR/unicode

haveQt(5) {
    mac {
        # Mac OS does ship libicu but not the associated header files.
        # Therefore WebKit provides adequate header files.
        INCLUDEPATH += $${ROOT_WEBKIT_DIR}/Source/WTF/icu
        LIBS += -licucore
    } else {
        contains(QT_CONFIG,icu) {
            unix: LIBS += $$system(icu-config --ldflags-searchpath --ldflags-libsonly)
            else: LIBS += -licuin
        } else {
            error("To build QtWebKit with Qt 5 you need ICU")
        }
    }
}

v8 {
    !haveQt(5): error("To build QtWebKit+V8 you need to use Qt 5")
    DEFINES *= WTF_USE_V8=1
    INCLUDEPATH += $${ROOT_WEBKIT_DIR}/Source/WebKit/qt/v8/ForwardingHeaders
    QT += v8-private declarative
}

linux-*:!contains(DEFINES, USE_QTMULTIMEDIA=1) {
    !contains(QT_CONFIG, no-pkg-config):system(pkg-config --exists glib-2.0 gio-2.0 gstreamer-0.10): {
        DEFINES += ENABLE_GLIB_SUPPORT=1
        PKGCONFIG += glib-2.0 gio-2.0
    }
}

win32-* {
    LIBS += -lwinmm
    LIBS += -lgdi32
}
