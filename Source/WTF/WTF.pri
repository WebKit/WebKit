# -------------------------------------------------------------------
# This file contains shared rules used both when building WTF itself
# and for targets that depend in some way on WTF.
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

load(features)

SOURCE_DIR = $${ROOT_WEBKIT_DIR}/Source/WTF

# FIXME: Including the root directory is a bad idea, likewise
# no other project besides WTF should need to include WTF sub directories to build!
INCLUDEPATH += \
    $$SOURCE_DIR/.. \
    $$SOURCE_DIR \
    $$SOURCE_DIR/gobject \
    $$SOURCE_DIR/qt \
    $$SOURCE_DIR/unicode \
    $$SOURCE_DIR/wtf

haveQt(5) {
    mac {
        # Mac OS does ship libicu but not the associated header files.
        # Therefore WebKit provides adequate header files.
        INCLUDEPATH += $${ROOT_WEBKIT_DIR}/Source/WTF/icu
        LIBS += -licucore
    } else {
        contains(QT_CONFIG,icu) {
            LIBS += -licui18n -licuuc -licudata
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

linux-*:contains(DEFINES, WTF_USE_GSTREAMER=1) {
    DEFINES += ENABLE_GLIB_SUPPORT=1
    PKGCONFIG += glib-2.0 gio-2.0
}

win32-* {
    LIBS += -lwinmm
    LIBS += -lgdi32
}
