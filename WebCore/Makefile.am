NULL=

SUBDIRS = khtml kwq

symrootsdir = $(SYMROOTS)

noinst_LIBRARIES = libwebcore.dylib

libwebcore_dylib_SOURCES = \
	dummy.mm \
	$(NULL)

libwebcore_dylib_LIBADD = \
	khtml/libkhtml.o \
	kwq/libkwq.o \
	$(NULL)

WEBCORE_INSTALL_PATH = $(SYMROOTS)

WebCore-combined.exp: WebCore.exp WebCore-tests.exp
	cat $^ > $@

LDFLAGS = \
	-framework Cocoa \
	-framework CoreFoundation \
	-F$(symrootsdir) \
	-framework WebFoundation \
	-framework JavaScriptCore \
        -dynamiclib \
	-twolevel_namespace \
	-prebind \
        -undefined error \
        -all_load \
	-seg1addr 0x2200000 \
	$(NULL)

libwebcore_dylib_AR = touch libwebcore.dylib; :

noinst_DATA = framework-stamp framework-resource-stamp framework-dylib-stamp framework-header-stamp WebCore-combined.exp

# Framework assembly

FRAMEWORK_NAME = WebCore
FRAMEWORK_VERSION = A
FRAMEWORK_RESOURCES = Resources/Info.plist Resources/linkCursor.tiff
FRAMEWORK_LOCALIZED_RESOURCES = English.lproj/InfoPlist.strings


FRAMEWORK_DIR = $(SYMROOTS)/$(FRAMEWORK_NAME).framework
FRAMEWORK_DYLIB = $(FRAMEWORK_DIR)/Versions/$(FRAMEWORK_VERSION)/$(FRAMEWORK_NAME)
FRAMEWORK_LINKFLAGS = $(libwebcore_dylib_LIBADD) -install_name @executable_path/../Frameworks/$(FRAMEWORK_NAME).framework/Versions/$(FRAMEWORK_VERSION)/$(FRAMEWORK_NAME)
FRAMEWORK_RESOURCE_DIR = $(FRAMEWORK_DIR)/Versions/$(FRAMEWORK_VERSION)/Resources
FRAMEWORK_LOCALIZED_RESOURCE_DIR = $(FRAMEWORK_RESOURCE_DIR)/English.lproj

framework-stamp: framework-dylib-stamp framework-resource-stamp framework-header-stamp
	$(MAKE) embed && touch $@

framework-resource-stamp: $(FRAMEWORK_RESOURCES) $(FRAMEWORK_LOCALIZED_RESOURCES)
	rm -rf $(FRAMEWORK_RESOURCE_DIR)
	mkdir -p $(FRAMEWORK_RESOURCE_DIR)
	rm -rf $(FRAMEWORK_DIR)/Resources
	ln -sf Versions/Current/Resources $(FRAMEWORK_DIR)/Resources
	@list='$(FRAMEWORK_RESOURCES)'; for p in $$list; do \
		echo " $(INSTALL_DATA) $$p $(FRAMEWORK_RESOURCE_DIR)/$$p"; \
	    	$(INSTALL_DATA) $$p $(FRAMEWORK_RESOURCE_DIR); \
	done
	mkdir -p $(FRAMEWORK_LOCALIZED_RESOURCE_DIR)
	@list='$(FRAMEWORK_LOCALIZED_RESOURCES)'; for p in $$list; do \
		echo " $(INSTALL_DATA) $$p $(FRAMEWORK_LOCALIZED_RESOURCE_DIR)/$$p"; \
		$(INSTALL_DATA) $$p $(FRAMEWORK_LOCALIZED_RESOURCE_DIR); \
	done
	touch $@

framework-header-stamp:
	mkdir -p $(FRAMEWORK_DIR)/Versions/$(FRAMEWORK_VERSION)/Headers
	mkdir -p $(FRAMEWORK_DIR)/Versions/$(FRAMEWORK_VERSION)/PrivateHeaders
	ln -sf Versions/Current/Headers $(FRAMEWORK_DIR)/Headers
	ln -sf Versions/Current/PrivateHeaders $(FRAMEWORK_DIR)/PrivateHeaders
	touch $@

framework-dylib-stamp: $(libwebcore_dylib_OBJECTS) $(libwebcore_dylib_DEPENDENCIES) WebCore-combined.exp
	mkdir -p $(FRAMEWORK_DIR)/Versions/$(FRAMEWORK_VERSION)
	rm -f $(FRAMEWORK_DIR)/Versions/Current
	ln -sf $(FRAMEWORK_VERSION) $(FRAMEWORK_DIR)/Versions/Current
	$(OBJCXXLD) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) $(FRAMEWORK_LINKFLAGS) $(libwebcore_dylib_OBJECTS) -o $(FRAMEWORK_DYLIB)
	nmedit -s WebCore-combined.exp - $(FRAMEWORK_DYLIB)
	ln -sf Versions/Current/$(FRAMEWORK_NAME) $(FRAMEWORK_DIR)/$(FRAMEWORK_NAME)
	touch $@


CLEANFILES = $(noinst_DATA)
# can't use CLEANFILES for this because it doesn't work for directories
clean-am:
	rm -rf $(FRAMEWORK_DIR)

# Embedding into Alexander

EMBED_HOST = $(SYMROOTS)/Alexander.app
EMBED_DIR = $(EMBED_HOST)/Contents/Frameworks

embed:
	@if test -f "$(FRAMEWORK_DYLIB)"; then \
		INSTALL_PATH=`otool -D "$(FRAMEWORK_DYLIB)"`; \
		WILL_EMBED=`echo $$INSTALL_PATH | sed -n -e "s/@executable_path//p"`; \
		if test -n "$$WILL_EMBED"; then \
			if [ -d "$(EMBED_HOST)" ]; then \
				if [ ! -d "$(EMBED_DIR)" ]; then \
					mkdir -p "$(EMBED_DIR)"; \
				fi; \
				echo "embedding $(FRAMEWORK_NAME) into $(EMBED_HOST)..."; \
				cp -rf $(FRAMEWORK_DIR) $(EMBED_DIR); \
			else \
				echo "$(EMBED_HOST) not found. Not embedding framework"; \
			fi; \
		fi; \
        else \
		echo "can't find: $(FRAMEWORK_DYLIB)"; \
		exit 1; \
        fi
