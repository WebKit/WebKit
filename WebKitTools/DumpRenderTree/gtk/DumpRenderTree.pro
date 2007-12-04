TEMPLATE = app
SOURCES += DumpRenderTree.cpp \
           ../LayoutTestController.cpp \
           ../GCController.cpp \
           ../WorkQueue.cpp \
           GCControllerGtk.cpp \
           LayoutTestControllerGtk.cpp \
           WorkQueueItemGtk.cpp

CONFIG -= app_bundle

BASE_DIR = $$PWD/../../..

include(../../../WebKit.pri)

INCLUDEPATH += \
    $$BASE_DIR/WebKitTools/DumpRenderTree

QMAKE_RPATHDIR += $$OUTPUT_DIR/lib
