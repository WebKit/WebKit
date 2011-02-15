# Include file for WebCore

include(../common.pri)
include(features.pri)

SOURCE_DIR = $$replace(PWD, /WebCore, "")

CONFIG(standalone_package) {
    isEmpty(WC_GENERATED_SOURCES_DIR):WC_GENERATED_SOURCES_DIR = $$PWD/generated
    isEmpty(JSC_GENERATED_SOURCES_DIR):JSC_GENERATED_SOURCES_DIR = $$PWD/../JavaScriptCore/generated

    PRECOMPILED_HEADER = $$PWD/../WebKit/qt/WebKit_pch.h
} else {
    isEmpty(WC_GENERATED_SOURCES_DIR):WC_GENERATED_SOURCES_DIR = generated
    isEmpty(JSC_GENERATED_SOURCES_DIR):JSC_GENERATED_SOURCES_DIR = ../JavaScriptCore/generated

    !CONFIG(release, debug|release) {
        OBJECTS_DIR = obj/debug
    } else { # Release
        OBJECTS_DIR = obj/release
    }

}

# On Symbian PREPEND_INCLUDEPATH is the best way to make sure that WebKit headers
# are included before platform headers.
symbian {
    PREPEND_INCLUDEPATH = $$WC_GENERATED_SOURCES_DIR $$PREPEND_INCLUDEPATH
} else {
    INCLUDEPATH = $$WC_GENERATED_SOURCES_DIR $$INCLUDEPATH
}

QT += network

V8_DIR = "$$[QT_INSTALL_PREFIX]/src/3rdparty/v8"

v8:!exists($${V8_DIR}/include/v8.h) {
    error("Cannot build with V8. Needed file $${V8_DIR}/include/v8.h does not exist.")
}

v8 {
    message(Using V8 with QtScript)
    QT += script
    INCLUDEPATH += $${V8_DIR}/include
    DEFINES *= V8_BINDING=1
    DEFINES += WTF_CHANGES=1
    DEFINES *= WTF_USE_V8=1
    DEFINES += USING_V8_SHARED
    linux-*:LIBS += -lv8
}

v8 {
    WEBCORE_INCLUDEPATH = \
        $$SOURCE_DIR/WebCore/bindings/v8 \
        $$SOURCE_DIR/WebCore/bindings/v8/custom \
        $$SOURCE_DIR/WebCore/bindings/v8/specialization \
        $$SOURCE_DIR/WebCore/bridge/qt/v8

} else {
    WEBCORE_INCLUDEPATH = \
        $$SOURCE_DIR/WebCore/bridge/jsc \
        $$SOURCE_DIR/WebCore/bindings/js \
        $$SOURCE_DIR/WebCore/bindings/js/specialization \
        $$SOURCE_DIR/WebCore/bridge/c
}

WEBCORE_INCLUDEPATH = \
    $$SOURCE_DIR/WebCore \
    $$SOURCE_DIR/WebCore/accessibility \
    $$SOURCE_DIR/WebCore/bindings \
    $$SOURCE_DIR/WebCore/bindings/generic \
    $$SOURCE_DIR/WebCore/bridge \
    $$SOURCE_DIR/WebCore/css \
    $$SOURCE_DIR/WebCore/dom \
    $$SOURCE_DIR/WebCore/dom/default \
    $$SOURCE_DIR/WebCore/editing \
    $$SOURCE_DIR/WebCore/fileapi \
    $$SOURCE_DIR/WebCore/history \
    $$SOURCE_DIR/WebCore/html \
    $$SOURCE_DIR/WebCore/html/canvas \
    $$SOURCE_DIR/WebCore/html/parser \
    $$SOURCE_DIR/WebCore/html/shadow \
    $$SOURCE_DIR/WebCore/inspector \
    $$SOURCE_DIR/WebCore/loader \
    $$SOURCE_DIR/WebCore/loader/appcache \
    $$SOURCE_DIR/WebCore/loader/archive \
    $$SOURCE_DIR/WebCore/loader/cache \
    $$SOURCE_DIR/WebCore/loader/icon \
    $$SOURCE_DIR/WebCore/mathml \
    $$SOURCE_DIR/WebCore/notifications \
    $$SOURCE_DIR/WebCore/page \
    $$SOURCE_DIR/WebCore/page/animation \
    $$SOURCE_DIR/WebCore/platform \
    $$SOURCE_DIR/WebCore/platform/animation \
    $$SOURCE_DIR/WebCore/platform/audio \
    $$SOURCE_DIR/WebCore/platform/graphics \
    $$SOURCE_DIR/WebCore/platform/graphics/filters \
    $$SOURCE_DIR/WebCore/platform/graphics/transforms \
    $$SOURCE_DIR/WebCore/platform/image-decoders \
    $$SOURCE_DIR/WebCore/platform/mock \
    $$SOURCE_DIR/WebCore/platform/network \
    $$SOURCE_DIR/WebCore/platform/sql \
    $$SOURCE_DIR/WebCore/platform/text \
    $$SOURCE_DIR/WebCore/platform/text/transcoder \
    $$SOURCE_DIR/WebCore/plugins \
    $$SOURCE_DIR/WebCore/rendering \
    $$SOURCE_DIR/WebCore/rendering/mathml \
    $$SOURCE_DIR/WebCore/rendering/style \
    $$SOURCE_DIR/WebCore/rendering/svg \
    $$SOURCE_DIR/WebCore/storage \
    $$SOURCE_DIR/WebCore/svg \
    $$SOURCE_DIR/WebCore/svg/animation \
    $$SOURCE_DIR/WebCore/svg/graphics \
    $$SOURCE_DIR/WebCore/svg/graphics/filters \
    $$SOURCE_DIR/WebCore/svg/properties \
    $$SOURCE_DIR/WebCore/webaudio \
    $$SOURCE_DIR/WebCore/websockets \
    $$SOURCE_DIR/WebCore/wml \
    $$SOURCE_DIR/WebCore/workers \
    $$SOURCE_DIR/WebCore/xml \
    $$WEBCORE_INCLUDEPATH

WEBCORE_INCLUDEPATH = \
    $$SOURCE_DIR/WebCore/bridge/qt \
    $$SOURCE_DIR/WebCore/page/qt \
    $$SOURCE_DIR/WebCore/platform/graphics/qt \
    $$SOURCE_DIR/WebCore/platform/network/qt \
    $$SOURCE_DIR/WebCore/platform/qt \
    $$SOURCE_DIR/WebKit/qt/Api \
    $$SOURCE_DIR/WebKit/qt/WebCoreSupport \
    $$WEBCORE_INCLUDEPATH

symbian {
    PREPEND_INCLUDEPATH = $$WEBCORE_INCLUDEPATH $$PREPEND_INCLUDEPATH
} else {

    INCLUDEPATH = $$WEBCORE_INCLUDEPATH $$INCLUDEPATH
}

contains(QT_CONFIG, qpa):CONFIG += embedded

enable_fast_mobile_scrolling: DEFINES += ENABLE_FAST_MOBILE_SCROLLING=1

use_qt_mobile_theme: DEFINES += WTF_USE_QT_MOBILE_THEME=1
