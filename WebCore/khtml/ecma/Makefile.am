NULL =

LUT_FILES = \
	kjs_css.lut.h \
	kjs_dom.lut.h \
	kjs_html.lut.h \
	kjs_events.lut.h \
	kjs_navigator.lut.h \
	kjs_range.lut.h \
	kjs_traversal.lut.h \
	kjs_views.lut.h \
	kjs_window.lut.h \
	xmlhttprequest.lut.h \
	xmlserializer.lut.h \
	$(NULL)

CREATE_HASH_TABLE = $(top_srcdir)/JavaScriptCore/kjs/create_hash_table

%.lut.h: %.cpp $(CREATE_HASH_TABLE)
	$(CREATE_HASH_TABLE) $< > $@

BUILT_SOURCES = $(LUT_FILES)
CLEANFILES = $(BUILT_SOURCES)
