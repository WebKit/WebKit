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
	loader.h \
	loader_client.h \
	loader_jpeg.cpp \
	loader_jpeg.h \
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

khtmldata.h:
	touch $@

$(TAGFILES): tag-stamp

tag-stamp: maketags
	perl ./maketags
	touch ./tag-stamp

$(ATTRSFILES): attr-stamp

attr-stamp: makeattrs
	perl ./makeattrs
	touch ./attr-stamp

BUILT_SOURCES = khtmldata.h $(TAGFILES) $(ATTRSFILES) tag-stamp attr-stamp

CLEANFILES = $(BUILT_SOURCES)
