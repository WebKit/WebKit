NULL =

noinst_LIBRARIES = libkhtmlcss.o
LDFLAGS = -Wl,-r -nostdlib
libkhtmlcss_o_AR = $(OBJCXXLD) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o

libkhtmlcss_o_SOURCES = \
	css_ruleimpl.cpp \
	css_ruleimpl.h \
	css_stylesheetimpl.cpp \
	css_stylesheetimpl.h \
	css_valueimpl.cpp \
	css_valueimpl.h \
	csshelper.cpp \
	csshelper.h \
	cssparser.cpp \
	cssparser.h \
	cssproperties.h \
	cssstyleselector.cpp \
	cssstyleselector.h \
	cssvalues.h \
	$(NULL)

noinst_HEADERS = \
	cssproperties.c \
	cssvalues.c \
	$(NULL)

VALUEFILES = \
	cssvalues.c \
	cssvalues.h \
	cssvalues.gperf \
	$(NULL)

PROPFILES = \
	cssproperties.c \
	cssproperties.h \
	cssproperties.gperf \
	$(NULL)


BUILT_SOURCES = $(VALUEFILES) $(PROPFILES)

CLEANFILES = $(BUILT_SOURCES)

INCLUDES = \
	$(KWQ_INCLUDES) \
	$(KDELIBS_INCLUDES) \
	$(NULL)

$(VALUEFILES):
	sh ./makevalues

$(PROPFILES):
	sh ./makeprop

