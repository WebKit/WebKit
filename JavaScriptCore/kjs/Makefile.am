NULL =

YACCFLAGS = -d --output-file=grammar.cpp --file-prefix=grammar --name-prefix=kjsyy

GRAMMAR_FILES = grammar.h grammar.cpp.h grammar.cpp

$(GRAMMAR_FILES): grammar-stamp

grammar-stamp: grammar.y
	$(YACC) $(YACCFLAGS) $<
	cp -f grammar.cpp.h grammar.h
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

noinst_DATA = $(BUILT_SOURCES)


