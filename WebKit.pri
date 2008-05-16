# Include file to make it easy to include WebKit into Qt projects


isEmpty(OUTPUT_DIR) {
    CONFIG(release):OUTPUT_DIR=$$PWD/WebKitBuild/Release
    CONFIG(debug):OUTPUT_DIR=$$PWD/WebKitBuild/Debug
}

DEFINES += BUILDING_QT__=1
!building-libs {
    QMAKE_LIBDIR = $$OUTPUT_DIR/lib $$QMAKE_LIBDIR
    LIBS += -lQtWebKit
    DEPENDPATH += $$PWD/WebKit/qt/Api
}

DEFINES += USE_SYSTEM_MALLOC
CONFIG(release) {
    DEFINES += NDEBUG
}

BASE_DIR = $$PWD
INCLUDEPATH += \
    $$PWD/WebKit/qt/Api \
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
