NULL=

noinst_LIBRARIES = libkhtmlxml.a

libkhtmlxml_a_SOURCES = \
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


# FIXME: Need a hack here to give the xml tokenizer text stream
# defines from Qt. The NEED_BOGUS_TEXTSTREAMS symbol
# can be removed when we have Qt text streams implemented

INCLUDES = \
    -DNEED_BOGUS_TEXTSTREAMS \
    $(KWQ_INCLUDES) \
    $(KDELIBS_INCLUDES) \
    $(NULL)

dom_docimpl.moc:
	touch $@

xml_tokenizer.moc:
	touch $@

BUILT_SOURCES = dom_docimpl.moc xml_tokenizer.moc

CLEANFILES = $(BUILT_SOURCES)
