NULL =

noinst_LIBRARIES = libkjs.o
libkjs_o_ldflags = -Wl,-r -nostdlib
libkjs_o_AR = $(OBJCXXLD) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) $(libkjs_o_ldflags) -o

INCLUDES = $(KWQ_INCLUDES)

libkjs_o_SOURCES = \
	array_object.cpp \
	array_object.h \
	bool_object.cpp \
	bool_object.h \
	collector.cpp \
	collector.h \
	date_object.cpp \
	date_object.h \
	debugger.cpp \
	debugger.h \
	error_object.cpp \
	error_object.h \
	function.cpp \
	function.h \
	function_object.cpp \
	function_object.h \
	grammar.cpp \
	grammar.h \
	internal.cpp \
	internal.h \
	interpreter.cpp \
	interpreter.h \
	lexer.cpp \
	lexer.h \
	lexer.lut.h \
	lookup.cpp \
	lookup.h \
	math_object.cpp \
	math_object.h \
	math_object.lut.h \
	nodes.cpp \
	nodes.h \
	number_object.cpp \
	number_object.h \
	object.cpp \
	object.h \
	object_object.cpp \
	object_object.h \
	operations.cpp \
	operations.h \
	property_map.cpp \
	property_map.h \
	regexp.cpp \
	regexp.h \
	regexp_object.cpp \
	regexp_object.h \
	string_object.cpp \
	string_object.h \
	types.cpp \
	types.h \
	ustring.cpp \
	ustring.h \
	value.cpp \
	value.h \
	$(NULL)

noinst_PROGRAMS = testkjs

testkjs_SOURCES = testkjs.cpp

testkjs_LDADD = $(srcdir)/libkjs.o

YACCFLAGS = -d --output-file=grammar.cpp --file-prefix=grammar --name-prefix=kjsyy

GRAMMAR_FILES = grammar.h grammar.cpp.h grammar.cpp

$(GRAMMAR_FILES): grammar-stamp

grammar-stamp: grammar.y
	$(YACC) $(YACCFLAGS) $<
	ln -sf grammar.cpp.h grammar.h
	touch ./grammar-stamp

LUT_FILES = math_object.lut.h lexer.lut.h array_object.lut.h date_object.lut.h string_object.lut.h number_object.lut.h

lexer.lut.h: keywords.table
	./create_hash_table keywords.table -i > lexer.lut.h;

array_object.lut.h: array_object.cpp
	./create_hash_table array_object.cpp -i > array_object.lut.h

math_object.lut.h: math_object.cpp
	./create_hash_table math_object.cpp -i > math_object.lut.h

date_object.lut.h: date_object.cpp
	./create_hash_table date_object.cpp -i > date_object.lut.h

number_object.lut.h: number_object.cpp
	./create_hash_table number_object.cpp -i > number_object.lut.h

string_object.lut.h: string_object.cpp
	./create_hash_table string_object.cpp -i > string_object.lut.h

BUILT_SOURCES = $(GRAMMAR_FILES) $(LUT_FILES) grammar-stamp

CLEANFILES = $(BUILT_SOURCES)

