NULL =

noinst_LIBRARIES = libkhtmlmisc.o
LDFLAGS = -Wl,-r -nostdlib
libkhtmlmisc_o_AR = $(OBJCXXLD) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o

libkhtmlmisc_o_SOURCES = \
	decoder.cpp \
	decoder.h \
	helper.cpp \
	helper.h \
	htmlattrs.h \
	htmlhashes.cpp \
	htmlhashes.h \
	htmltags.h \
	khtmldata.h \
	khtmllayout.h \
	loader.cpp \
	loader.h \
	loader_client.h \
	stringit.h \
	$(NULL)

noinst_HEADERS = \
	htmlattrs.c \
	htmltags.c \
	$(NULL)

INCLUDES = \
	$(KWQ_INCLUDES) \
	$(KDELIBS_INCLUDES) \
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
