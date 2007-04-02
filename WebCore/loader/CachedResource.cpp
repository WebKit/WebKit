/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "CachedResource.h"

#include "Cache.h"
#include "FrameLoader.h"
#include "Request.h"
#include <KURL.h>
#include <wtf/Vector.h>

namespace WebCore {

CachedResource::CachedResource(const String& URL, Type type, unsigned size)
{
    m_url = URL;
    m_type = type;
    m_status = Pending;
    m_encodedSize = size;
    m_inCache = false;
    m_request = 0;
    m_expireDateChanged = false;

    m_accessCount = 0;
    m_liveAccessCount = 0;
    
    m_nextInAllResourcesList = 0;
    m_prevInAllResourcesList = 0;
    
    m_nextInLiveResourcesList = 0;
    m_prevInLiveResourcesList = 0;

#ifndef NDEBUG
    m_deleted = false;
    m_lruIndex = 0;
    m_liveLRUIndex = 0;
#endif
    m_errorOccurred = false;
    m_shouldTreatAsLocal = FrameLoader::shouldTreatURLAsLocal(m_url);
}

CachedResource::~CachedResource()
{
    ASSERT(!inCache());
    ASSERT(!m_deleted);
#ifndef NDEBUG
    m_deleted = true;
#endif
}

void CachedResource::finish()
{
    m_status = Cached;
    KURL url(m_url.deprecatedString());
    if (m_expireDateChanged && url.protocol().startsWith("http"))
        m_expireDateChanged = false;
}

bool CachedResource::isExpired() const
{
    if (!m_response.expirationDate())
        return false;
    time_t now = time(0);
    return (difftime(now, m_response.expirationDate()) >= 0);
}

void CachedResource::setRequest(Request* request)
{
    if (request && !m_request)
        m_status = Pending;
    m_request = request;
    if (canDelete() && !inCache())
        delete this;
}

void CachedResource::ref(CachedResourceClient *c)
{
    if (!referenced() && inCache()) {
        increaseLiveAccessCount();
        cache()->addToLiveResourcesSize(this);
        cache()->insertInLiveResourcesList(this);
    }
    m_clients.add(c);
}

void CachedResource::deref(CachedResourceClient *c)
{
    ASSERT(m_clients.contains(c));
    m_clients.remove(c);
    if (canDelete() && !inCache())
        delete this;
    else if (!referenced() && inCache()) {
        cache()->removeFromLiveResourcesSize(this);
        cache()->removeFromLiveResourcesList(this);
        resetLiveAccessCount();
        allReferencesRemoved();
        cache()->pruneAllResources();
    }
}

void CachedResource::setEncodedSize(unsigned size)
{
    if (size == m_encodedSize)
        return;

    // The size cannot ever shrink.  If it ever does, assert.
    ASSERT(size >= m_encodedSize);
    
    unsigned oldSize = m_encodedSize;

    // The object must now be moved to a different queue, since its size has been changed.
    // We have to remove explicitly before updating m_encodedSize, so that we find the correct previous
    // queue.
    if (inCache())
        cache()->removeFromLRUList(this);
    
    m_encodedSize = size;
   
    if (inCache()) { 
        // Now insert into the new LRU list.
        cache()->insertInLRUList(this);
        
        // Update the cache's size totals.
        cache()->adjustSize(referenced(), size - oldSize, 0);
    }
}

void CachedResource::liveResourceAccessed()
{
    if (inCache()) {
        cache()->removeFromLiveResourcesList(this);
        increaseLiveAccessCount();
        cache()->insertInLiveResourcesList(this);
        cache()->pruneLiveResources();
    }
}

}
