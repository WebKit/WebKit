/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

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

#include <time.h>

#include <kglobal.h>
#include <kcharsets.h>

#include <qtextcodec.h>

#include "KDOMCache.h"
#include "KDOMCacheHelper.h"
#include "KDOMCachedObject.h"
#include "KDOMCachedObjectClient.h"

using namespace KDOM;

Request::Request(DocumentLoader *_docLoader, CachedObject *_object, bool _incremental)
{
	object = _object;
	docLoader = _docLoader;
	incremental = _incremental;

	object->setRequest(this);
}

Request::~Request()
{
	object->setRequest(0);
}

CachedObject::CachedObject(const DOMString &url, Type type, KIO::CacheControl cachePolicy, int size)
{
	m_url = url;
	m_type = type;
	m_size = size;
	m_cachePolicy = cachePolicy;

	m_prev = 0;
	m_next = 0;
	m_request = 0;
	m_expireDate = 0;
	m_accessCount = 0;

	m_free = false;
	m_deleted = false;
	m_loading = false;
	m_hadError = false;
	m_wasBlocked = false;

	m_status = Pending;
}

CachedObject::~CachedObject()
{
	Cache::removeFromLRUList(this);
}

void CachedObject::ref(CachedObjectClient *consumer)
{
	// unfortunately we can be ref'ed multiple times from the
	// same object,  because it uses e.g. the same foreground
	// and the same background picture. so deal with it.
	m_clients.insert(consumer, consumer);
	Cache::removeFromLRUList(this);
	m_accessCount++;
}

void CachedObject::deref(CachedObjectClient *consumer)
{
	Q_ASSERT(consumer);
	Q_ASSERT(m_clients.count());
	Q_ASSERT(!canDelete());
	Q_ASSERT(m_clients.find(consumer));

	Cache::flush();

	m_clients.remove(consumer);

	if(allowInLRUList())
		Cache::insertInLRUList(this);
}

bool CachedObject::schedule() const
{
	return false;
}

void CachedObject::finish()
{
	m_status = Cached;
}

QTextCodec *CachedObject::codecForBuffer(const QString &charset, const QByteArray &buffer) const
{
	// we don't use heuristicContentMatch here since it is a) far too slow and
	// b) having too much functionality for our case.
	unsigned char *d = (unsigned char *) buffer.data();
	int s = buffer.size();

	if(s >= 3 && d[0] == 0xef && d[1] == 0xbb && d[2] == 0xbf)
#ifdef APPLE_CHANGES
		return QTextCodec::codecForName("utf-8"); // UTF-8
#else
		return QTextCodec::codecForMib( 106 ); // UTF-8
#endif

	if(s >= 2 && ((d[0] == 0xff && d[1] == 0xfe) || (d[0] == 0xfe && d[1] == 0xff)))
#ifdef APPLE_CHANGES
		return QTextCodec::codecForName("ucs-2"); // UCS-2
#else
		return QTextCodec::codecForMib( 1000 ); // UCS-2
#endif

#ifndef APPLE_COMPILE_HACK
	if(!charset.isEmpty())
	{
		QTextCodec *c = KGlobal::charsets()->codecForName(charset);
		if(c->mibEnum() == 11) // iso8859-8 (visually ordered)
			c = QTextCodec::codecForName("iso8859-8-i");

		return c;
	}
#endif

#ifdef APPLE_CHANGES
	return QTextCodec::codecForName("latin-1");; // latin-1
#else
	return QTextCodec::codecForMib(4); // latin-1
#endif
}
	
void CachedObject::setRequest(Request *request)
{
	if(request && !m_request)
		m_status = Pending;

	if(allowInLRUList())
		Cache::removeFromLRUList(this);

	m_request = request;

	if(allowInLRUList())
		Cache::insertInLRUList(this);
}

bool CachedObject::isExpired() const
{
	if(!m_expireDate)
		return false;

	time_t now = time(0);
	return (difftime(now, m_expireDate) >= 0);
}

void CachedObject::setSize(int size)
{
	bool sizeChanged;

	if(!m_next && !m_prev && lruListFor(this)->head != this)
		sizeChanged = false;
	else
		sizeChanged = (size - m_size) != 0;

	// The object must now be moved to a different queue,
	// since its size has been changed.
	if(sizeChanged  && allowInLRUList())
		Cache::removeFromLRUList(this);

	m_size = size;

	if(sizeChanged && allowInLRUList())
		Cache::insertInLRUList(this);
}
		
// vim:ts=4:noet
