NULL =

YACC=bison

EXTRA_DIST = parser.y

parser.cpp: $(srcdir)/parser.y
	cd $(srcdir); \
	$(YACC) -d -p cssyy parser.y && mv parser.tab.c $<; \
	if test -f parser.tab.h; then \
	if cmp -s parser.tab.h parser.h; then rm -f parser.tab.h; \
	else mv parser.tab.h parser.h; fi \
	else :; fi

noinst_HEADERS = \
	cssproperties.c \
	cssvalues.c \
	parser.cpp \
	$(NULL)

VALUEFILES = \
	cssvalues.c \
	cssvalues.h \
	cssvalues.gperf \
	$(NULL)

PROPFILES = \
	cssproperties.c \
	cssproperties.h \
	cssproperties.gperf \
	$(NULL)

$(VALUEFILES): value-stamp
value-stamp: makevalues cssvalues.in
	sh $< && touch $@

$(PROPFILES): prop-stamp
prop-stamp: makeprop cssproperties.in
	sh $< && touch $@

BUILT_SOURCES = $(VALUEFILES) $(PROPFILES) value-stamp prop-stamp
CLEANFILES = $(BUILT_SOURCES)
