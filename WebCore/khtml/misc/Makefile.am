NULL =

noinst_HEADERS = \
	htmlattrs.c \
	htmltags.c \
	$(NULL)

TAGFILES = \
	htmltags.c \
	htmltags.h \
	htmltags.gperf \
	$(NULL)

ATTRSFILES = \
	htmlattrs.c \
	htmlattrs.h \
	htmlattrs.gperf \
	$(NULL)

$(TAGFILES): tag-stamp
tag-stamp: maketags htmltags.in
	perl $< && touch $@

$(ATTRSFILES): attr-stamp
attr-stamp: makeattrs htmlattrs.in
	perl $< && touch $@

BUILT_SOURCES = $(TAGFILES) $(ATTRSFILES) tag-stamp attr-stamp
CLEANFILES = $(BUILT_SOURCES)
