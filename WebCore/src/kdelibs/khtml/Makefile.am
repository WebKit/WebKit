NULL =

SUBDIRS = \
	misc \
	css \
	dom \
	ecma \
	xml \
	java \
	html \
	rendering \
	$(NULL)

noinst_LIBRARIES = libkhtml.o
LDFLAGS = -Wl,-r -nostdlib
libkhtml_o_AR = $(OBJCXXLD) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o

libkhtml_o_SOURCES = \
	khtml_events.cpp \
	khtmlview.cpp \
	$(NULL)

libkhtml_o_LIBADD = \
	./misc/libkhtmlmisc.o \
	./css/libkhtmlcss.o \
	./dom/libkhtmldom.o \
	./ecma/libkhtmlecma.o \
	./xml/libkhtmlxml.o \
	./java/libkhtmljava.o \
	./html/libkhtmlhtml.o \
	./rendering/libkhtmlrendering.o \
	$(NULL)

INCLUDES = \
	$(KWQ_INCLUDES) \
	$(KDELIBS_INCLUDES) \
	$(NULL)

khtmlview.moc:
	echo '#include <khtmlview.h>' > $@

BUILT_SOURCES = khtmlview.moc

CLEANFILES = $(BUILT_SOURCES)

