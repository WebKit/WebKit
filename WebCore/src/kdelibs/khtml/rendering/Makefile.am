NULL =

noinst_LIBRARIES = libkhtmlrendering.a

libkhtmlrendering_a_SOURCES = \
	bidi.cpp \
	bidi.h \
	break_lines.cpp \
	break_lines.h \
	render_applet.cpp \
	render_applet.h \
	render_body.cpp \
	render_body.h \
	render_box.cpp \
	render_box.h \
	render_br.cpp \
	render_br.h \
	render_container.cpp \
	render_container.h \
	render_flow.cpp \
	render_flow.h \
	render_form.cpp \
	render_form.h \
	render_frames.cpp \
	render_frames.h \
	render_hr.cpp \
	render_hr.h \
	render_html.cpp \
	render_html.h \
	render_image.cpp \
	render_image.h \
	render_list.cpp \
	render_list.h \
	render_object.cpp \
	render_object.h \
	render_replaced.cpp \
	render_replaced.h \
	render_root.cpp \
	render_root.h \
	render_style.cpp \
	render_style.h \
	render_table.cpp \
	render_table.h \
	render_text.cpp \
	render_text.h \
	$(NULL)


INCLUDES = \
	-DNEED_BOGUS_X_DEFINES \
	$(KWQ_INCLUDES) \
	$(KDELIBS_INCLUDES) \
	$(NULL)

render_form.moc:
	touch $@

render_frames.moc:
	touch $@

render_replaced.moc:
	touch $@

BUILT_SOURCES = \
	render_form.moc \
	render_frames.moc \
	render_replaced.moc \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)

