NULL =

noinst_LTLIBRARIES = libkhtmlhtml.la

libkhtmlhtml_la_SOURCES = \
	dtd.cpp \
	dtd.h \
	html_baseimpl.cpp \
	html_baseimpl.h \
	html_blockimpl.cpp \
	html_blockimpl.h \
	html_documentimpl.cpp \
	html_documentimpl.h \
	html_elementimpl.cpp \
	html_elementimpl.h \
	html_formimpl.cpp \
	html_formimpl.h \
	html_headimpl.cpp \
	html_headimpl.h \
	html_imageimpl.cpp \
	html_imageimpl.h \
	html_inlineimpl.cpp \
	html_inlineimpl.h \
	html_listimpl.cpp \
	html_listimpl.h \
	html_miscimpl.cpp \
	html_miscimpl.h \
	html_objectimpl.cpp \
	html_objectimpl.h \
	html_tableimpl.cpp \
	html_tableimpl.h \
	htmlparser.cpp \
	htmlparser.h \
	htmltokenizer.cpp \
	htmltokenizer.h \
	$(NULL)

INCLUDES = \
	-I$(top_srcdir) \
	$(KWQ_INCLUDES) \
	$(KDELIBS_INCLUDES) \
	-I. \
	$(NULL)

htmltokenizer.moc:
	touch $@

html_documentimpl.moc:
	touch $@

BUILT_SOURCES = htmltokenizer.moc html_documentimpl.moc

CLEANFILES = $(BUILT_SOURCES)
