TEMPLATE = app
TARGET = tst_wtf

SOURCES += \
    AtomicString.cpp \
    CheckedArithmeticOperations.cpp \
    CString.cpp \
    Functional.cpp \
    HashMap.cpp \
    HashSet.cpp \
    IntegerToStringConversion.cpp \
    ListHashSet.cpp \
    MathExtras.cpp \
    MediaTime.cpp \
    RedBlackTree.cpp \
    SaturatedArithmeticOperations.cpp \
    StringBuilder.cpp \
    StringImpl.cpp \
    StringOperators.cpp \
    TemporaryChange.cpp \
    VectorBasic.cpp \
    Vector.cpp \
    VectorReverse.cpp \
    WTFString.cpp

include(../../TestWebKitAPI.pri)
