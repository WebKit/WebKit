NULL =

YACCFLAGS = -d --output-file=grammar.cpp --file-prefix=grammar --name-prefix=kjsyy

GRAMMAR_FILES = grammar.h grammar.cpp.h grammar.cpp

$(GRAMMAR_FILES): grammar-stamp

grammar-stamp: grammar.y
	$(YACC) $(YACCFLAGS) $<
	cp -f grammar.cpp.h grammar.h
	touch $@

LUT_FILES = math_object.lut.h lexer.lut.h array_object.lut.h date_object.lut.h string_object.lut.h number_object.lut.h

lexer.lut.h: keywords.table
	$(srcdir)/create_hash_table $< -i > $@

array_object.lut.h: array_object.cpp
	$(srcdir)/create_hash_table $< -i > $@

math_object.lut.h: math_object.cpp
	$(srcdir)/create_hash_table $< -i > $@

date_object.lut.h: date_object.cpp
	$(srcdir)/create_hash_table $< -i > $@

number_object.lut.h: number_object.cpp
	$(srcdir)/create_hash_table $< -i > $@

string_object.lut.h: string_object.cpp
	$(srcdir)/create_hash_table $< -i > $@

BUILT_SOURCES = $(GRAMMAR_FILES) $(LUT_FILES) grammar-stamp

noinst_DATA = $(BUILT_SOURCES)
