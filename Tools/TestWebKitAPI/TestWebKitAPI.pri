
INCLUDEPATH += $$PWD $${ROOT_WEBKIT_DIR}/Source/ThirdParty/gtest/include
WEBKIT += wtf javascriptcore

DEFINES += QT_NO_CAST_FROM_ASCII

QT += core gui webkit

CONFIG += compiling_thirdparty_code

SOURCES += $$PWD/*.cpp
SOURCES += $$PWD/qt/*.cpp

LIBS += -L$${ROOT_BUILD_DIR}/Source/ThirdParty/gtest/$$activeBuildConfig() -lgtest
