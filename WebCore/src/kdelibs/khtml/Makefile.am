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

noinst_LTLIBRARIES = libkhtml.la

libkhtml_la_SOURCES = \
	khtml_events.cpp \
	khtmlview.cpp \
	$(NULL)

libkhtml_la_LIBADD = \
	./misc/libkhtmlmisc.la \
	./css/libkhtmlcss.la \
	./dom/libkhtmldom.la \
	./ecma/libkhtmlecma.la \
	./xml/libkhtmlxml.la \
	./java/libkhtmljava.la \
	./html/libkhtmlhtml.la \
	./rendering/libkhtmlrendering.la \
	$(NULL)

INCLUDES = \
	$(KWQ_INCLUDES) \
	$(KDELIBS_INCLUDES) \
	$(NULL)

khtmlview.moc:
	echo '#include <khtmlview.h>' > $@

BUILT_SOURCES = khtmlview.moc

CLEANFILES = $(BUILT_SOURCES)

