TEMPLATE = subdirs
CONFIG += ordered

include(WebKit.pri)

!v8 {
    SUBDIRS += \
        JavaScriptCore/JavaScriptCore.pro \
        JavaScriptCore/jsc.pro
}

webkit2 {
    !qt5 {
        message("Building WebKit2 with Qt versions older than 5.0 is no longer supported.")
        message("Read http://www.mail-archive.com/webkit-qt@lists.webkit.org/msg01674.html for more information.")
        error("Aborting build.")
    }

    SUBDIRS += WebKit2/WebKit2.pro
}

qt5 {
    isEmpty(QT.widgets.name)|isEmpty(QT.printsupport.name) {
        message("Building WebKit against Qt 5.0 requires the QtWidgets and QtPrintSupport modules.")
        error("Aborting build.")
    }
}

SUBDIRS += WebCore
SUBDIRS += WebKit/qt/QtWebKit.pro

webkit2 {
    SUBDIRS += \
        WebKit2/WebProcess.pro \
        WebKit2/UIProcess/API/qt/tests
}

exists($$PWD/WebKit/qt/declarative) {
    qt5 {
        contains(QT_CONFIG, qtquick1): SUBDIRS += WebKit/qt/declarative
    } else {
        contains(QT_CONFIG, declarative): SUBDIRS += WebKit/qt/declarative
    }
}

SUBDIRS += WebKit/qt/tests

include(WebKit/qt/docs/docs.pri)
