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


if AlexanderTest
LIBWEBCORE_INSTALL_PATH=@executable_path/../Frameworks
else
LIBWEBCORE_INSTALL_PATH=$(symrootsdir)
endif

LDFLAGS = \
	-framework Cocoa \
	-framework CoreFoundation \
	-F$(symrootsdir) -framework WebFoundation \
        -dynamiclib \
	-twolevel_namespace \
	-prebind \
        -undefined error \
        -all_load \
	-seg1addr 0x2200000 \
        -install_name $(LIBWEBCORE_INSTALL_PATH)/libwebcore.dylib \
	$(NULL)

libwebcore_dylib_AR = $(OBJCXXLD) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o

noinst_DATA = webcore-install-stamp

webcore-install-stamp: libwebcore.dylib
	$(MAKE) install-symrootsLIBRARIES
	touch ./webcore-install-stamp

