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
	./kdelibs/kjs/libkjs.o \
	./kdelibs/khtml/libkhtml.o \
	./kwq/libkwq.o \
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
