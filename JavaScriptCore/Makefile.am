NULL=

SUBDIRS = kjs

INCLUDES = -I $(top_srcdir)/JavaScriptCore/kjs

# FIXME: need to build kjs test program
noinst_PROGRAMS = kjs/testkjs
kjs_testkjs_SOURCES = kjs/testkjs.cpp
kjs_testkjs_DEPENDENCIES = JavaScriptCore-stamp
kjs_testkjs_LDFLAGS = -F$(SYMROOTS) -framework JavaScriptCore

FRAMEWORK_NAME = JavaScriptCore
FRAMEWORK_DIR = $(SYMROOTS)/$(FRAMEWORK_NAME).framework
FRAMEWORK_DYLIB = $(FRAMEWORK_DIR)/Versions/Current/$(FRAMEWORK_NAME)
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

all-am: JavaScriptCore-stamp

JavaScriptCore-stamp: kjs/*.cpp kjs/*.h
	pbxbuild -buildstyle $(BUILDSTYLE) && touch $@

clean-am:
	pbxbuild clean

CLEANFILES = JavaScriptCore-stamp
