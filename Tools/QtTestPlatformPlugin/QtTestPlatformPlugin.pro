TARGET = testplatform
DESTDIR = $$ROOT_BUILD_DIR/lib

load(qt_plugin)
QT = core gui core-private gui-private platformsupport-private

HEADERS = \
    TestIntegration.h \

SOURCES = \
    main.cpp \
    TestIntegration.cpp \

mac {
    LIBS += -framework Foundation
    OBJECTIVE_HEADERS += mac/TestFontDatabase.h
    OBJECTIVE_SOURCES += \
        mac/TestFontDatabase.mm \
        mac/TestIntegrationMac.mm \
}
