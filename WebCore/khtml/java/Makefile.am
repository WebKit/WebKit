NULL = 

noinst_LIBRARIES = libkhtmljava.o
LDFLAGS = -Wl,-r -nostdlib
libkhtmljava_o_AR = $(OBJCXXLD) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o

libkhtmljava_o_SOURCES = \
	kjavaapplet.cpp \
	kjavaapplet.h \
	kjavaappletcontext.cpp \
	kjavaappletcontext.h \
	kjavaappletserver.cpp \
	kjavaappletserver.h \
	kjavaappletwidget.cpp \
	kjavaappletwidget.h \
	kjavadownloader.cpp \
	kjavadownloader.h \
	kjavaprocess.cpp \
	kjavaprocess.h \
	$(NULL)


INCLUDES = \
	$(KWQ_INCLUDES) \
	$(KDELIBS_INCLUDES) \
	$(NULL)


kjavaapplet.moc:
	touch $@

kjavaappletcontext.moc:
	touch $@

kjavaappletserver.moc:
	touch $@

kjavaappletwidget.moc:
	touch $@

kjavadownloader.moc:
	touch $@

kjavaprocess.moc:
	touch $@

BUILT_SOURCES = \
	kjavaapplet.moc \
	kjavaappletcontext.moc \
	kjavaappletserver.moc \
	kjavaappletwidget.moc \
	kjavadownloader.moc \
	kjavaprocess.moc \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)
