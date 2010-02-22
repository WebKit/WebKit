TEMPLATE = subdirs
CONFIG += ordered

include(WebKit.pri)

SUBDIRS += \
        WebCore \
        WebKitTools/QtLauncher \
        WebKit/qt/QGVLauncher

!CONFIG(standalone_package) {
    SUBDIRS += JavaScriptCore/jsc.pro \
        WebKit/qt/tests \
        WebKitTools/DumpRenderTree/qt/DumpRenderTree.pro

    !win32:!symbian {
        SUBDIRS += WebKitTools/DumpRenderTree/qt/ImageDiff.pro
        SUBDIRS += WebKitTools/DumpRenderTree/qt/TestNetscapePlugin/TestNetscapePlugin.pro
    }

}

build-qtscript {
    SUBDIRS += \
        JavaScriptCore/qt/api/QtScript.pro \
        JavaScriptCore/qt/tests
}

symbian {
    # Forward the install target to WebCore. A workaround since INSTALLS is not implemented for symbian
    install.commands = $(MAKE) -C WebCore install
    QMAKE_EXTRA_TARGETS += install
}

include(WebKit/qt/docs/docs.pri)
