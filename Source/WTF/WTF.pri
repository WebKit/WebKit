# -------------------------------------------------------------------
# This file contains shared rules used both when building WTF itself
# and for targets that depend in some way on WTF.
#
# See 'Tools/qmake/README' for an overview of the build system
# -------------------------------------------------------------------

# All external modules should include WTF headers by prefixing with "wtf" (#include <wtf/some/thing.h>).
INCLUDEPATH += $$PWD

haveQt(5) {
    mac {
        # Mac OS does ship libicu but not the associated header files.
        # Therefore WebKit provides adequate header files.
        INCLUDEPATH += $${ROOT_WEBKIT_DIR}/Source/WTF/icu
        LIBS += -licucore
    } else {
        contains(QT_CONFIG,icu) {
            win32: LIBS += -licuin -licuuc -licudt
            else: LIBS += -licui18n -licuuc -licudata
        } else {
            error("To build QtWebKit with Qt 5 you need ICU")
        }
    }
}

linux-*:contains(DEFINES, WTF_USE_GSTREAMER=1) {
    DEFINES += ENABLE_GLIB_SUPPORT=1
    PKGCONFIG += glib-2.0 gio-2.0
}

win32-* {
    LIBS += -lwinmm
    LIBS += -lgdi32
}
