/*
    Copyright (C) 2005 Frans Englich 		<frans.englich@telia.com>

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

#include <qbuffer.h>
#include <qobject.h>
#include <qtextcodec.h>

#include <kdebug.h>

#include "KDOMCache.h"
#include "KDOMCachedDocument.h"
#include "KDOMCachedObjectClient.h"
#include "KDOMLoader.h"

using namespace KDOM;

class CachedDocument::Private
{
	public:
		Private(): errorCode(0)
		{ }

		int errorCode;
		DOMString document;
		QString errorDescription;
		QString charset;
		QBuffer docBuffer;
};


CachedDocument::CachedDocument(DocumentLoader *docLoader, const DOMString &url, KIO::CacheControl _cachePolicy, const char *accept)
: CachedObject(url, CachedObject::TextDocument, _cachePolicy, 0), d(new Private())
{
	QString ah = QString::fromLatin1(accept);
	if(!ah.isEmpty())
		ah += QString::fromLatin1(",");

	ah += QString::fromLatin1("*/*;q=0.1");
	setAccept(ah);

	Cache::loader()->load(docLoader, this, false);
	m_loading = true;
}

void CachedDocument::ref(CachedObjectClient *c)
{
	CachedObject::ref(c);

	if(!m_loading)
		if(m_hadError)
			c->error(d->errorCode, d->errorDescription);
		else
			c->notifyFinished(this);
}

void CachedDocument::data(QBuffer &buffer, bool eof)
{
	kdDebug() << k_funcinfo << endl;

	if(!eof)
		return;

	setSize(buffer.buffer().size());
	buffer.close();
	d->docBuffer.setBuffer(buffer.buffer());

	QTextCodec *c = codecForBuffer(d->charset, buffer.buffer());
	QString data = c->toUnicode( buffer.buffer().data(), m_size );
	d->document = (data[0].unicode() == QChar::byteOrderMark) ? DOMString(data.mid(1)) : DOMString(data);

	m_loading = false;
	checkNotify();
}

void CachedDocument::checkNotify()
{
	if(m_loading) 
		return;

	for(QPtrDictIterator<CachedObjectClient> it(m_clients); it.current();)
		it()->notifyFinished(this);
}

void CachedDocument::error(int err, const char *text)
{
	m_hadError = true;
	d->errorCode = err;
	d->errorDescription = text;
	m_loading = false;

	for(QPtrDictIterator<CachedObjectClient> it( m_clients ); it.current();)
		it()->error(d->errorCode, d->errorDescription);
}

bool CachedDocument::schedule() const
{
	return true;
}

void CachedDocument::setCharset(const QString &charset)
{
	d->charset = charset;
}

QBuffer* CachedDocument::documentBuffer() const
{
	return &d->docBuffer;
}

const DOMString& CachedDocument::document() const
{
	return d->document;
}


// vim:ts=4:noet
