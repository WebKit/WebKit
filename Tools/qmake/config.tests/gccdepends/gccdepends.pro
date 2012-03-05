TEMPLATE = aux
SOURCES += empty.cpp
OBJECTS_DIR = obj
QMAKE_CXXFLAGS += -MD

test.commands = test -f $$OBJECTS_DIR/empty.d && test ! -f empty.d && touch $$basename(PWD)
test.depends = $$OBJECTS_DIR/empty.o
QMAKE_EXTRA_TARGETS += test

default.target = all
default.depends += test
QMAKE_EXTRA_TARGETS += default
