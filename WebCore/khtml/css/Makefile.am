NULL =

noinst_HEADERS = \
	cssproperties.c \
	cssvalues.c \
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
