SUBDIRS = pcre kjs

# FIXME: need to build the other kjs test program too

INCLUDES = -I $(top_srcdir)/JavaScriptCore/kjs
noinst_PROGRAMS = kjs/testkjs
kjs_testkjs_SOURCES = kjs/testkjs.cpp
kjs_testkjs_DEPENDENCIES = JavaScriptCore-stamp
kjs_testkjs_LDFLAGS = -F$(SYMROOTS) -framework JavaScriptCore

all-am: JavaScriptCore-stamp
JavaScriptCore-stamp: kjs/*.cpp kjs/*.h pcre/*.c pcre/*.h bindings/*.h bindings/*.cpp bindings/jni/*.cpp bindings/jni/*.h bindings/objc/*.h bindings/objc/*.mm bindings/c/*.h bindings/c/*.cpp
	xcodebuild -buildstyle $(BUILDSTYLE) OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS) && touch $@
clean-am:
	xcodebuild clean OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS)
	rm -rf $(SYMROOTS)/JavaScriptCore.build
	rm -rf $(SYMROOTS)/JavaScriptCore.framework
	rm -rf $(SYMROOTS)/Safari.app/Frameworks/JavaScriptCore.framework

CLEANFILES = JavaScriptCore-stamp
