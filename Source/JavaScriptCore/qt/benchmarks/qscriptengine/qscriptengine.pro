TEMPLATE = app
TARGET = tst_bench_qscriptengine

SOURCES += tst_qscriptengine.cpp

QT += testlib

include(../benchmarks.pri)

symbian* {
    TARGET.EPOCHEAPSIZE  = 0x20000 0x2000000 // Min 128kB, Max 32MB
    TARGET.EPOCSTACKSIZE = 0x14000
}
