NULL =

noinst_LIBRARIES = libkhtmlecma.o
LDFLAGS = -Wl,-r -nostdlib
libkhtmlecma_o_AR = $(OBJCXXLD) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o

libkhtmlecma_o_SOURCES = \
	kjs_binding.cpp \
	kjs_binding.h \
	kjs_css.cpp \
	kjs_css.h \
	kjs_debugwin.cpp \
	kjs_debugwin.h \
	kjs_dom.cpp \
	kjs_dom.h \
	kjs_events.cpp \
	kjs_events.h \
	kjs_html.cpp \
	kjs_html.h \
	kjs_navigator.cpp \
	kjs_navigator.h \
	kjs_proxy.cpp \
	kjs_proxy.h \
	kjs_range.cpp \
	kjs_range.h \
	kjs_text.cpp \
	kjs_text.h \
	kjs_traversal.cpp \
	kjs_traversal.h \
	kjs_views.cpp \
	kjs_views.h \
	kjs_window.cpp \
	kjs_window.h \
	$(NULL)

# FIXME - can't build ecma test because kwq is not yet built at this point
#
# noinst_PROGRAMS = testecma
#
# testecma_SOURCES = \
#	testecma.cpp \
#	$(NULL)
#
# testecma_LDADD = ./libkhtmlecma.a

INCLUDES = \
	$(KWQ_INCLUDES) \
	$(KDELIBS_INCLUDES) \
	$(NULL)

kjs_window.moc:
	touch $@

BUILT_SOURCES = kjs_window.moc
CLEANFILES = $(BUILT_SOURCES)


