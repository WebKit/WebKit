SUBDIRS = kjs

# FIXME: need to build the other kjs test program too

INCLUDES = -I $(top_srcdir)/JavaScriptCore/kjs
noinst_PROGRAMS = kjs/testkjs
kjs_testkjs_SOURCES = kjs/testkjs.cpp
kjs_testkjs_DEPENDENCIES = JavaScriptCore-stamp
kjs_testkjs_LDFLAGS = -F$(SYMROOTS) -framework JavaScriptCore

all-am: JavaScriptCore-stamp
JavaScriptCore-stamp: kjs/*.cpp kjs/*.h
	pbxbuild -buildstyle $(BUILDSTYLE) && touch $@
clean-am:
	pbxbuild clean
CLEANFILES = JavaScriptCore-stamp
