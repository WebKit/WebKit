NULL=

SUBDIRS = \
	kdelibs \
	kwq \
	$(NULL)


symrootsdir = $(SYMROOTS)

symroots_LTLIBRARIES = libwebcore.la

libwebcore_la_SOURCES = \
	dummy.mm \
	$(NULL)

libwebcore_la_LIBADD = \
	./kdelibs/kjs/libkjs.la \
	./kdelibs/khtml/libkhtml.la \
	./kwq/libkwq.la \
	/usr/lib/libjpeg.a \
	/usr/lib/libstdc++.a \
	$(NULL)

libwebcore_la_LDFLAGS = \
	-version-info $(WEBCORE_VERSION_INFO) \
	$(NULL)

all: install-am
