NULL=

noinst_LIBRARIES = libkhtmlxml.o
LDFLAGS = -Wl,-r -nostdlib
libkhtmlxml_o_AR = $(OBJCXXLD) $(AM_OBJCXXFLAGS) $(OBJCXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o

libkhtmlxml_o_SOURCES = \
        dom2_eventsimpl.cpp \
        dom2_eventsimpl.h \
        dom2_rangeimpl.cpp \
        dom2_rangeimpl.h \
        dom2_traversalimpl.cpp \
        dom2_traversalimpl.h \
        dom2_viewsimpl.cpp \
        dom2_viewsimpl.h \
        dom_docimpl.cpp \
        dom_docimpl.h \
        dom_elementimpl.cpp \
        dom_elementimpl.h \
        dom_nodeimpl.cpp \
        dom_nodeimpl.h \
        dom_stringimpl.cpp \
        dom_stringimpl.h \
        dom_textimpl.cpp \
        dom_textimpl.h \
        dom_xmlimpl.cpp \
        dom_xmlimpl.h \
        xml_tokenizer.cpp \
        xml_tokenizer.h \
	$(NULL)

INCLUDES = \
    $(KWQ_INCLUDES) \
    $(KDELIBS_INCLUDES) \
    $(NULL)

dom_docimpl.moc:
	touch $@

xml_tokenizer.moc:
	touch $@

BUILT_SOURCES = dom_docimpl.moc xml_tokenizer.moc

CLEANFILES = $(BUILT_SOURCES)
