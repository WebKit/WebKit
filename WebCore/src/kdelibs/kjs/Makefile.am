NULL =

noinst_LTLIBRARIES = libkjs.la

libkjs_la_SOURCES = \
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
	global_object.cpp \
	grammar.cpp \
	grammar.h \
	internal.cpp \
	internal.h \
	kjs.cpp \
	kjs.h \
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
	$(NULL)

BUILT_SOURCES = grammar.cpp grammar.cpp.h grammar.h

noinst_PROGRAMS = testkjs

testkjs_SOURCES = testkjs.cpp

testkjs_LDADD = $(srcdir)/libkjs.la

CLEANFILES = $(BUILT_SOURCES) \
	$(NULL)

YACCFLAGS = -d --output-file=grammar.cpp --file-prefix=grammar --name-prefix=kjsyy

grammar.h grammar.cpp.h grammar.cpp: grammar.y
	$(YACC) $(YACCFLAGS) $<
	ln -sf grammar.cpp.h grammar.h

