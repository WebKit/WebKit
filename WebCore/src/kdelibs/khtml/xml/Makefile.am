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

noinst_LTLIBRARIES = libkhtmlxml.la
libkhtmlxml_la_SOURCES = \
	dom_docimpl.cpp      dom_nodeimpl.cpp     dom_textimpl.cpp \
	dom_elementimpl.cpp  dom_stringimpl.cpp   dom2_rangeimpl.cpp \
	dom2_traversalimpl.cpp xml_tokenizer.cpp  dom_xmlimpl.cpp \
	dom2_eventsimpl.cpp  dom2_viewsimpl.cpp

#libkhtmlxml_la_LDFLAGS = -no-undefined 
libkhtmlxml_la_METASOURCES = AUTO

noinst_HEADERS = \
	dom_docimpl.h      dom_nodeimpl.h     dom_textimpl.h \
	dom_elementimpl.h  dom_stringimpl.h   dom2_rangeimpl.h \
	dom2_traversalimpl.h xml_tokenizer.h  dom_xmlimpl.h \
	dom2_eventsimpl.h  dom2_viewsimpl.h

INCLUDES = -I$(top_srcdir)/kimgio -I$(top_srcdir)/kio -I$(top_srcdir)/dcop \
 -I$(top_srcdir)/khtml -I$(top_srcdir)/khtml/misc -I$(top_srcdir)/khtml/css \
 -I$(top_srcdir)/khtml/dom -I$(top_srcdir)/khtml/xml -I$(top_srcdir)/khtml/html \
 -I$(top_srcdir) $(all_includes)

SRCDOC_DEST=$(kde_htmldir)/en/kdelibs/khtml

## generate lib documentation
srcdoc:
	$(mkinstalldirs) $(SRCDOC_DEST)
	kdoc -H -d $(SRCDOC_DEST) kdecore \
	         $(include_HEADERS) -lqt

