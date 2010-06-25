QMAKE_RPATHDIR = $$OUTPUT_DIR/lib $$QMAKE_RPATHDIR
QMAKE_LIBDIR = $$OUTPUT_DIR/lib $$QMAKE_LIBDIR
mac:!static:contains(QT_CONFIG, qt_framework):!CONFIG(webkit_no_framework) {
    LIBS += -framework QtScript
    QMAKE_FRAMEWORKPATH = $$OUTPUT_DIR/lib $$QMAKE_FRAMEWORKPATH
} else {
    win32-*|wince* {
        LIBS += -lQtScript$${QT_MAJOR_VERSION}
    } else {
        LIBS += -lQtScript
    }
}

CONFIG(release, debug|release) {
    DEFINES += NDEBUG
}

INCLUDEPATH += $$PWD/../api

