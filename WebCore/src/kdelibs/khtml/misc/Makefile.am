NULL =

noinst_LIBRARIES = libkhtmlmisc.a

libkhtmlmisc_a_SOURCES = \
	decoder.cpp \
	decoder.h \
	font.cpp \
	font.h \
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

$(TAGFILES):
	perl ./maketags

$(ATTRSFILES):
	perl ./makeattrs

BUILT_SOURCES = khtmldata.h $(TAGFILES) $(ATTRSFILES)

CLEANFILES = $(BUILT_SOURCES)
