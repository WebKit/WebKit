/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1998 Lars Knoll <knoll@kde.org>
    Copyright (C) 2001-2003 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2003 Apple Computer, Inc

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <kdebug.h>

#include <qtextcodec.h>

#include "KDOMCache.h"
#include "KDOMLoader.h"
#include "KDOMCachedStyleSheet.h"
#include "KDOMCachedObjectClient.h"

using namespace KDOM;

CachedStyleSheet::CachedStyleSheet(DocumentLoader *docLoader, const DOMString &url, KIO::CacheControl _cachePolicy, const char *accept)
: CachedObject(url, StyleSheet, _cachePolicy, 0)
{
	// Set the type we want (probably css or xml)
	QString ah = QString::fromLatin1(accept);
	if(!ah.isEmpty())
		ah += QString::fromLatin1(",");
		
	ah += QString::fromLatin1("*/*;q=0.1");
	setAccept(ah);
	
	m_err = 0;
	m_hadError = false;

	// load the file
	Cache::loader()->load(docLoader, this, false);
	m_loading = true;
}

CachedStyleSheet::CachedStyleSheet(const DOMString &url, const QString &stylesheet_data)
: CachedObject(url, StyleSheet, KIO::CC_Verify, stylesheet_data.length())
{
	m_loading = false;
	m_status = Persistent;
	m_sheet = DOMString(stylesheet_data);
}


void CachedStyleSheet::ref(CachedObjectClient *c)
{
	CachedObject::ref(c);

	if(!m_loading)
		if(m_hadError)
			c->error(m_err, m_errText);
		else
			c->setStyleSheet(m_url, m_sheet);
}

void CachedStyleSheet::data(QBuffer &buffer, bool eof)
{
	if(!eof)
		return;

	buffer.close();
	setSize(buffer.buffer().size());

	QTextCodec* c = codecForBuffer(m_charset, buffer.buffer());
	QString data = c->toUnicode(buffer.buffer().data(), m_size);
	// workaround Qt bugs
	m_sheet = (data[0].unicode() == QChar::byteOrderMark) ? DOMString(data.mid(1)) : DOMString(data);
	kdDebug() << "m_sheet now is : " << data << endl;
	m_loading = false;

	checkNotify();
}

void CachedStyleSheet::checkNotify()
{
	if(m_loading || m_hadError) return;

	kdDebug(6060) << "CachedStyleSheet:: finishedLoading " << m_url.string() << endl;

	// it() first increments, then returnes the current item.
	// this avoids skipping an item when setStyleSheet deletes the "current" one.
	for(QPtrDictIterator<CachedObjectClient> it( m_clients ); it.current();)
		it()->setStyleSheet(m_url, m_sheet);
}


void CachedStyleSheet::error(int err, const char *text)
{
	m_hadError = true;
	m_err = err;
	m_errText = text;
	m_loading = false;

	// it() first increments, then returnes the current item.
	// this avoids skipping an item when setStyleSheet deletes the "current" one.
	for(QPtrDictIterator<CachedObjectClient> it( m_clients ); it.current();)
		it()->error(m_err, m_errText);
}

// vim:ts=4:noet
