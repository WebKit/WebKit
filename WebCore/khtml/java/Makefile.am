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
