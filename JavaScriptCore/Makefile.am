NULL=

SUBDIRS = kjs

symrootsdir = $(SYMROOTS)

symroots_LIBRARIES = libJavaScriptCore.dylib

libJavaScriptCore_dylib_SOURCES = \
	dummy.cpp \
	$(NULL)

libJavaScriptCore_dylib_LIBADD = \
	./kjs/libkjs.o \
	$(NULL)

LIBJAVASCRIPTCORE_INSTALL_PATH = @executable_path/../Frameworks

DYLIB_NAME = libJavaScriptCore.dylib
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
        -dynamiclib \
	-twolevel_namespace \
	-prebind \
        -undefined error \
        -all_load \
	-seg1addr 0x6000000 \
        -install_name $(LIBJAVASCRIPTCORE_INSTALL_PATH)/libJavaScriptCore.dylib \
	$(NULL)

libJavaScriptCore_dylib_AR = $(OBJCXXLD) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o

noinst_DATA = JavaScriptCore-install-stamp

JavaScriptCore-install-stamp: libJavaScriptCore.dylib
	$(MAKE) install-symrootsLIBRARIES
	touch ./JavaScriptCore-install-stamp

