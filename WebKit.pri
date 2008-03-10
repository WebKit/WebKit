# Include file to make it easy to include WebKit into Qt projects


isEmpty(OUTPUT_DIR) {
    CONFIG(release):OUTPUT_DIR=$$PWD/WebKitBuild/Release
    CONFIG(debug):OUTPUT_DIR=$$PWD/WebKitBuild/Debug
}

!gtk-port:CONFIG += qt-port
qt-port:DEFINES += BUILDING_QT__=1
qt-port:!building-libs {
    QMAKE_LIBDIR = $$OUTPUT_DIR/lib $$QMAKE_LIBDIR
    LIBS += -lQtWebKit
    DEPENDPATH += $$PWD/WebKit/qt/Api
}

gtk-port:!building-libs {
    QMAKE_LIBDIR = $$OUTPUT_DIR/lib $$QMAKE_LIBDIR
    LIBS += -lWebKitGtk
    DEPENDPATH += $$PWD/WebKit/gtk $$PWD/WebKit/gtk/WebCoreSupport $$PWD/WebKit/gtk/webkit
}

gtk-port {
    CONFIG += link_pkgconfig

    DEFINES += BUILDING_CAIRO__=1 BUILDING_GTK__=1

    # We use FreeType directly with Cairo
    PKGCONFIG += cairo-ft

    directfb: PKGCONFIG += cairo-directfb gtk+-directfb-2.0
    else: PKGCONFIG += cairo gtk+-2.0

    # Set a CONFIG flag for the GTK+ target (x11, quartz, win32, directfb)
    CONFIG += $$system(pkg-config --variable=target $$PKGCONFIG)

    # We use the curl http backend on all platforms
    PKGCONFIG += libcurl
    DEFINES += WTF_USE_CURL=1

    LIBS += -lWebKitGtk -ljpeg -lpng

    QMAKE_CXXFLAGS += $$system(icu-config --cppflags)
    QMAKE_LIBS += $$system(icu-config --ldflags)

    # This set of warnings is borrowed from the Mac build
    QMAKE_CXXFLAGS += -Wall -W -Wcast-align -Wchar-subscripts -Wformat-security -Wmissing-format-attribute -Wpointer-arith -Wwrite-strings -Wno-format-y2k -Wno-unused-parameter -Wundef

    # These flags are based on optimization experience from the Mac port:
    # Helps code size significantly and speed a little
    QMAKE_CXXFLAGS += -fno-exceptions -fno-rtti

    DEPENDPATH += $$PWD/JavaScriptCore/API
    INCLUDEPATH += $$PWD
}

DEFINES += USE_SYSTEM_MALLOC
CONFIG(release) {
    DEFINES += NDEBUG
}

gtk-port:CONFIG(debug) {
    DEFINES += G_DISABLE_DEPRECATED GDK_PIXBUF_DISABLE_DEPRECATED GDK_DISABLE_DEPRECATED GTK_DISABLE_DEPRECATED PANGO_DISABLE_DEPRECATED
# maybe useful for debugging   DEFINES += GDK_MULTIHEAD_SAFE GTK_MULTIHEAD_SAFE
}

BASE_DIR = $$PWD
qt-port:INCLUDEPATH += \
    $$PWD/WebKit/qt/Api
gtk-port:INCLUDEPATH += \
    $$BASE_DIR/WebCore/platform/gtk \
    $$BASE_DIR/WebCore/platform/network/curl \
    $$BASE_DIR/WebCore/platform/graphics/cairo \
    $$BASE_DIR/WebCore/loader/gtk \
    $$BASE_DIR/WebCore/page/gtk \
    $$BASE_DIR/WebKit/gtk \
    $$BASE_DIR/WebKit/gtk/WebCoreSupport \
    $$BASE_DIR/WebKit/gtk/webkit
INCLUDEPATH += \
    $$BASE_DIR/JavaScriptCore/ \
    $$BASE_DIR/JavaScriptCore/kjs \
    $$BASE_DIR/JavaScriptCore/bindings \
    $$BASE_DIR/JavaScriptCore/bindings/c \
    $$BASE_DIR/JavaScriptCore/wtf \
    $$BASE_DIR/JavaScriptCore/ForwardingHeaders \
    $$BASE_DIR/WebCore \
    $$BASE_DIR/WebCore/ForwardingHeaders \
    $$BASE_DIR/WebCore/platform \
    $$BASE_DIR/WebCore/platform/network \
    $$BASE_DIR/WebCore/platform/graphics \
    $$BASE_DIR/WebCore/loader \
    $$BASE_DIR/WebCore/page \
    $$BASE_DIR/WebCore/css \
    $$BASE_DIR/WebCore/dom \
    $$BASE_DIR/WebCore/bridge \
    $$BASE_DIR/WebCore/editing \
    $$BASE_DIR/WebCore/rendering \
    $$BASE_DIR/WebCore/history \
    $$BASE_DIR/WebCore/xml \
    $$BASE_DIR/WebCore/html \
    $$BASE_DIR/WebCore/plugins


macx {
	INCLUDEPATH += /usr/include/libxml2
	LIBS += -lxml2 -lxslt
}
