TEMPLATE = subdirs
CONFIG += ordered

include(WebKit.pri)

!v8 {
    exists($$PWD/JavaScriptCore/JavaScriptCore.pro): SUBDIRS += JavaScriptCore/JavaScriptCore.pro
    exists($$PWD/JavaScriptCore/jsc.pro): SUBDIRS += JavaScriptCore/jsc.pro
}

webkit2 {
    lessThan(QT_MAJOR_VERSION, 5) {
        message("Building WebKit2 with Qt versions older than 5.0 is no longer supported.")
        message("Read http://www.mail-archive.com/webkit-qt@lists.webkit.org/msg01674.html for more information.")
        error("Aborting build.")
    }
    exists($$PWD/WebKit2/WebKit2.pro): SUBDIRS += WebKit2/WebKit2.pro
}

greaterThan(QT_MAJOR_VERSION, 4) {
    isEmpty(QT.widgets.name)|isEmpty(QT.printsupport.name) {
        message("Building WebKit against Qt 5.0 requires the QtWidgets and QtPrintSupport modules.")
        error("Aborting build.")
    }
}

SUBDIRS += WebCore
SUBDIRS += WebKit/qt/QtWebKit.pro

webkit2 {
    exists($$PWD/WebKit2/WebProcess.pro): SUBDIRS += WebKit2/WebProcess.pro
    exists($$PWD/WebKit2/UIProcess/API/qt/tests): SUBDIRS += WebKit2/UIProcess/API/qt/tests
    SUBDIRS += WebKit2/UIProcess/API/qt/qmlplugin
}

exists($$PWD/WebKit/qt/declarative) {
    lessThan(QT_MAJOR_VERSION, 5) {
        contains(QT_CONFIG, declarative): SUBDIRS += WebKit/qt/declarative
    } else {
        contains(QT_CONFIG, qtquick1): SUBDIRS += WebKit/qt/declarative
    }
}

exists($$PWD/WebKit/qt/tests): SUBDIRS += WebKit/qt/tests

build-qtscript {
    SUBDIRS += \
        JavaScriptCore/qt/api/QtScript.pro \
        JavaScriptCore/qt/tests \
        JavaScriptCore/qt/benchmarks
}

symbian {
    exists($$PWD/WebKit/qt/symbian/platformplugin): SUBDIRS += WebKit/qt/symbian/platformplugin

    # Forward the install target to WebCore. A workaround since INSTALLS is not implemented for symbian
    install.commands = $(MAKE) -C WebCore install
    QMAKE_EXTRA_TARGETS += install
}

include(WebKit/qt/docs/docs.pri)
