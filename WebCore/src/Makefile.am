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


LIBWEBCORE_INSTALL_PATH = @executable_path/../Frameworks

DYLIB_NAME = libwebcore.dylib
EMBED_HOST = $(SYMROOTS)/Alexander.app
EMBED_DIR = $(EMBED_HOST)/Contents/Frameworks

embed:
	@if test -f "$(DYLIB_NAME)"; then \
		INSTALL_PATH=`otool -D "$(DYLIB_NAME)"`; \
		WILL_EMBED=`echo $$INSTALL_PATH | sed -n -e "s/@executable_path//p"`; \
		if test -n "$$WILL_EMBED"; then \
			if [ -d "$(EMBED_HOST)" ]; then \
				if [ ! -d "$(EMBED_DIR)" ]; then \
					mkdir -p "$(EMBED_DIR)"; \
				fi; \
				echo "embedding $(DYLIB_NAME) into $(EMBED_HOST)..."; \
				cp -f $(DYLIB_NAME) $(EMBED_DIR); \
			else \
				echo "$(EMBED_HOST) not found. Not embedding framework"; \
			fi; \
		fi; \
        else \
		echo "can't find: $(DYLIB_NAME)"; \
		exit 1; \
        fi

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

