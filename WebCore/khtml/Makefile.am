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

noinst_LIBRARIES = libkhtml.a

libkhtml_a_SOURCES = \
	khtml_events.cpp \
	khtmlview.cpp \
	$(NULL)

INCLUDES = \
	$(KWQ_INCLUDES) \
	$(KDELIBS_INCLUDES) \
	$(NULL)

khtmlview.moc:
	echo '#include <khtmlview.h>' > $@

BUILT_SOURCES = khtmlview.moc

CLEANFILES = $(BUILT_SOURCES)

