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

noinst_LTLIBRARIES = libkhtmlhtml.la
libkhtmlhtml_la_SOURCES = \
	htmlparser.cpp         htmltokenizer.cpp \
	dtd.cpp                html_headimpl.cpp      html_blockimpl.cpp  \
	html_elementimpl.cpp   html_inlineimpl.cpp    html_documentimpl.cpp \
	html_baseimpl.cpp      html_imageimpl.cpp     html_listimpl.cpp \
	html_miscimpl.cpp      html_formimpl.cpp      html_objectimpl.cpp \
	html_tableimpl.cpp

libkhtmlhtml_la_METASOURCES = AUTO


noinst_HEADERS = \
	dtd.h                html_headimpl.h      html_tableimpl.h \
	html_baseimpl.h      html_imageimpl.h     htmlparser.h \
	html_blockimpl.h     html_inlineimpl.h    \
	html_documentimpl.h  html_listimpl.h      htmltokenizer.h \
	html_elementimpl.h   html_miscimpl.h \
	html_formimpl.h      html_objectimpl.h

INCLUDES = -I$(top_srcdir)/kimgio -I$(top_srcdir)/kio -I$(top_srcdir)/dcop \
 -I$(top_srcdir)/kjs -I$(top_srcdir)/khtml/java \
 -I$(top_srcdir)/khtml -I$(top_srcdir)/khtml/misc -I$(top_srcdir)/khtml/css \
 -I$(top_srcdir)/khtml/dom -I$(top_srcdir)/khtml/xml -I$(top_srcdir)/khtml/html \
 -I$(top_srcdir) \
 $(all_includes)

SRCDOC_DEST=$(kde_htmldir)/en/kdelibs/khtml

## generate lib documentation 
srcdoc:
	$(mkinstalldirs) $(SRCDOC_DEST)
	kdoc -H -d $(SRCDOC_DEST) kdecore \
	         $(include_HEADERS) -lqt

