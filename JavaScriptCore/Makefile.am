SUBDIRS = pcre kjs

all-am: JavaScriptCore-stamp
JavaScriptCore-stamp: kjs/*.cpp kjs/*.h pcre/*.c pcre/*.h bindings/*.h bindings/*.cpp bindings/jni/*.cpp bindings/jni/*.h bindings/objc/*.h bindings/objc/*.mm bindings/c/*.h bindings/c/*.cpp
	xcodebuild -target All -buildstyle $(BUILDSTYLE) OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS) && touch $@
clean-am:
	xcodebuild -target All clean OBJROOT=$(SYMROOTS) SYMROOT=$(SYMROOTS) DSTROOT=$(SYMROOTS)
	rm -rf $(SYMROOTS)/JavaScriptCore.build
	rm -rf $(SYMROOTS)/JavaScriptCore.framework
	rm -rf $(SYMROOTS)/Safari.app/Frameworks/JavaScriptCore.framework

CLEANFILES = JavaScriptCore-stamp
