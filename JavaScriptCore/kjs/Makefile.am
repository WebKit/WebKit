YACCFLAGS = -d --output-file=grammar.cpp --file-prefix=grammar --name-prefix=kjsyy

GRAMMAR_FILES = grammar.h grammar.cpp.h grammar.cpp

$(GRAMMAR_FILES): grammar-stamp

grammar-stamp: grammar.y
	$(YACC) $(YACCFLAGS) $< && cp -f grammar.cpp.h grammar.h && touch $@

LUT_FILES = math_object.lut.h lexer.lut.h array_object.lut.h date_object.lut.h string_object.lut.h number_object.lut.h

CREATE_HASH_TABLE = $(srcdir)/create_hash_table

lexer.lut.h: keywords.table $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< -i > $@

%.lut.h: %.cpp $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< -i > $@

BUILT_SOURCES = $(GRAMMAR_FILES) $(LUT_FILES) grammar-stamp

noinst_DATA = $(BUILT_SOURCES)
