#    This file is part of the KDE libraries
#    Copyright (C) 1997 Martin Jones (mjones@kde.org)
#              (C) 1997 Torben Weis (weis@kde.org)

#    This library is free software; you can redistribute it and/or
#    modify it under the terms of the GNU Library General Public
#    License as published by the Free Software Foundation; either
#    version 2 of the License, or (at your option) any later version.

#    This library is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#    Library General Public License for more details.

#    You should have received a copy of the GNU Library General Public License
#    along with this library; see the file COPYING.LIB.  If not, write to
#    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
#    Boston, MA 02111-1307, USA.

KDE_CXXFLAGS = $(USE_EXCEPTIONS)

noinst_LTLIBRARIES = libkhtmldom.la
libkhtmldom_la_SOURCES = \
	dom_misc.cpp        html_block.cpp      html_inline.cpp \
	css_rule.cpp        dom_node.cpp        html_document.cpp   html_list.cpp \
	css_stylesheet.cpp  dom_string.cpp      html_element.cpp    html_misc.cpp \
	css_value.cpp       dom_text.cpp        html_object.cpp \
	dom_doc.cpp         dom_xml.cpp         html_head.cpp \
	dom_element.cpp     html_base.cpp       html_image.cpp      html_form.cpp \
	dom2_range.cpp      html_table.cpp      dom2_traversal.cpp  dom2_events.cpp \
	dom2_views.cpp
#	css_extensions.cpp

libkhtmldom_la_METASOURCES = AUTO

myincludedir = $(includedir)/dom
myinclude_HEADERS = \
	dom_exception.h   html_base.h       html_inline.h \
	css_rule.h        dom_html.h        html_block.h      html_list.h \
	css_stylesheet.h  dom_misc.h        html_document.h   html_misc.h \
	css_value.h       dom_node.h        html_element.h    html_object.h \
	dom_core.h        dom_string.h      html_form.h       html_table.h \
	dom_doc.h         dom_text.h        html_head.h \
	dom_element.h     dom_xml.h         html_image.h \
	dom2_range.h      dom2_traversal.h  dom2_events.h \
	dom2_views.h
#	css_extensions.h

INCLUDES = -I$(top_srcdir)/kimgio -I$(top_srcdir)/kio -I$(top_srcdir)/dcop \
 -I$(top_srcdir)/khtml -I$(top_srcdir)/khtml/misc -I$(top_srcdir)/khtml/css \
 -I$(top_srcdir)/khtml/dom -I$(top_srcdir)/khtml/xml -I$(top_srcdir)/khtml/html \
 $(all_includes)

SRCDOC_DEST=$(kde_htmldir)/en/kdelibs/khtml

## generate lib documentation
srcdoc:
	$(mkinstalldirs) $(SRCDOC_DEST)
	kdoc -H -d $(SRCDOC_DEST) kdecore \
	         $(include_HEADERS) -lqt

