/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "CachedResource.h"

#include "Cache.h"
#include "DocLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "KURL.h"
#include "Request.h"
#include "SystemTime.h"
#include <wtf/Vector.h>

namespace WebCore {

CachedResource::CachedResource(const String& url, Type type, bool forCache, bool sendResourceLoadCallbacks)
    : m_url(url)
    , m_lastDecodedAccessTime(0)
    , m_sendResourceLoadCallbacks(sendResourceLoadCallbacks)
    , m_inCache(forCache)
    , m_docLoader(0)
{
    m_type = type;
    m_status = Pending;
    m_encodedSize = 0;
    m_decodedSize = 0;
    m_request = 0;

    m_accessCount = 0;
    m_inLiveDecodedResourcesList = false;
    
    m_nextInAllResourcesList = 0;
    m_prevInAllResourcesList = 0;
    
    m_nextInLiveResourcesList = 0;
    m_prevInLiveResourcesList = 0;

#ifndef NDEBUG
    m_deleted = false;
    m_lruIndex = 0;
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
    
    if (m_docLoader)
        m_docLoader->removeCachedResource(this);
}

void CachedResource::finish()
{
    m_status = Cached;
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
    if (!referenced() && inCache())
        cache()->addToLiveResourcesSize(this);
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
        cache()->removeFromLiveDecodedResourcesList(this);
        allReferencesRemoved();
        cache()->prune();
    }
}

void CachedResource::setDecodedSize(unsigned size)
{
    if (size == m_decodedSize)
        return;

    int delta = size - m_decodedSize;

    // The object must now be moved to a different queue, since its size has been changed.
    // We have to remove explicitly before updating m_decodedSize, so that we find the correct previous
    // queue.
    if (inCache())
        cache()->removeFromLRUList(this);
    
    m_decodedSize = size;
   
    if (inCache()) { 
        // Now insert into the new LRU list.
        cache()->insertInLRUList(this);
        
        // Insert into or remove from the live decoded list if necessary.
        if (m_decodedSize && !m_inLiveDecodedResourcesList && referenced())
            cache()->insertInLiveDecodedResourcesList(this);
        else if (!m_decodedSize && m_inLiveDecodedResourcesList)
            cache()->removeFromLiveDecodedResourcesList(this);

        // Update the cache's size totals.
        cache()->adjustSize(referenced(), delta);
    }
}

void CachedResource::setEncodedSize(unsigned size)
{
    if (size == m_encodedSize)
        return;

    // The size cannot ever shrink (unless it is being nulled out because of an error).  If it ever does, assert.
    ASSERT(size == 0 || size >= m_encodedSize);
    
    int delta = size - m_encodedSize;

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
        cache()->adjustSize(referenced(), delta);
    }
}

void CachedResource::didAccessDecodedData(double timeStamp)
{
    m_lastDecodedAccessTime = timeStamp;
    
    if (inCache()) {
        if (m_inLiveDecodedResourcesList) {
            cache()->removeFromLiveDecodedResourcesList(this);
            cache()->insertInLiveDecodedResourcesList(this);
        }
        cache()->prune();
    }
}

}
