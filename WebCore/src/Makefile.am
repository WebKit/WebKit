NULL=

SUBDIRS = \
	kdelibs \
	kwq \
	$(NULL)


symrootsdir = $(SYMROOTS)

symroots_LIBRARIES = libwebcore.dylib

libwebcore_dylib_SOURCES = \
	dummy.mm \
	$(NULL)

libwebcore_dylib_LIBADD = \
	./kdelibs/kjs/libkjs.a \
	./kdelibs/khtml/libkhtml.a \
	./kdelibs/khtml/misc/libkhtmlmisc.a \
	./kdelibs/khtml/css/libkhtmlcss.a \
	./kdelibs/khtml/dom/libkhtmldom.a \
	./kdelibs/khtml/ecma/libkhtmlecma.a \
	./kdelibs/khtml/xml/libkhtmlxml.a \
	./kdelibs/khtml/java/libkhtmljava.a \
	./kdelibs/khtml/html/libkhtmlhtml.a \
	./kdelibs/khtml/rendering/libkhtmlrendering.a \
	./kwq/libkwq.a \
	./kwq/qt/libqt_borrowed.a \
	./kwq/kde/libkde_borrowed.a \
	-ljpeg \
	$(NULL)

LDFLAGS = \
        -dynamiclib \
        -flat_namespace -undefined suppress \
        -all_load \
        -install_name $(symrootsdir)/libwebcore.dylib
	-version-info $(WEBCORE_VERSION_INFO) \
	$(NULL)

libwebcore_dylib_AR = $(OBJCXXLD) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o

all: install-am
