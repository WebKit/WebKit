/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

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
#include "CachedResourceHandle.h"
#include "DocLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "KURL.h"
#include "PurgeableBuffer.h"
#include "Request.h"
#include "SystemTime.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/Vector.h>

using namespace WTF;

namespace WebCore {

#ifndef NDEBUG
static RefCountedLeakCounter cachedResourceLeakCounter("CachedResource");
#endif

CachedResource::CachedResource(const String& url, Type type)
    : m_url(url)
    , m_lastDecodedAccessTime(0)
    , m_sendResourceLoadCallbacks(true)
    , m_preloadCount(0)
    , m_preloadResult(PreloadNotReferenced)
    , m_requestedFromNetworkingLayer(false)
    , m_inCache(false)
    , m_loading(false)
    , m_docLoader(0)
    , m_handleCount(0)
    , m_resourceToRevalidate(0)
    , m_isBeingRevalidated(false)
    , m_expirationDate(0)
{
#ifndef NDEBUG
    cachedResourceLeakCounter.increment();
#endif

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
}

CachedResource::~CachedResource()
{
    ASSERT(!inCache());
    ASSERT(!m_deleted);
    ASSERT(url().isNull() || cache()->resourceForURL(url()) != this);
#ifndef NDEBUG
    m_deleted = true;
    cachedResourceLeakCounter.decrement();
#endif

    if (m_resourceToRevalidate)
        m_resourceToRevalidate->m_isBeingRevalidated = false;

    if (m_docLoader)
        m_docLoader->removeCachedResource(this);
}
    
void CachedResource::load(DocLoader* docLoader, bool incremental, bool skipCanLoadCheck, bool sendResourceLoadCallbacks)
{
    m_sendResourceLoadCallbacks = sendResourceLoadCallbacks;
    cache()->loader()->load(docLoader, this, incremental, skipCanLoadCheck, sendResourceLoadCallbacks);
    m_loading = true;
}

void CachedResource::finish()
{
    m_status = Cached;
}

bool CachedResource::isExpired() const
{
    if (!m_expirationDate)
        return false;
    time_t now = time(0);
    return difftime(now, m_expirationDate) >= 0;
}

void CachedResource::setResponse(const ResourceResponse& response)
{
    m_response = response;
    m_expirationDate = response.expirationDate();
}

void CachedResource::setRequest(Request* request)
{
    if (request && !m_request)
        m_status = Pending;
    m_request = request;
    if (canDelete() && !inCache())
        delete this;
}

void CachedResource::addClient(CachedResourceClient *c)
{
    ASSERT(!isPurgeable());

    if (m_preloadResult == PreloadNotReferenced) {
        if (isLoaded())
            m_preloadResult = PreloadReferencedWhileComplete;
        else if (m_requestedFromNetworkingLayer)
            m_preloadResult = PreloadReferencedWhileLoading;
        else
            m_preloadResult = PreloadReferenced;
    }
    if (!hasClients() && inCache())
        cache()->addToLiveResourcesSize(this);
    m_clients.add(c);
}

void CachedResource::removeClient(CachedResourceClient *c)
{
    ASSERT(m_clients.contains(c));
    m_clients.remove(c);
    if (canDelete() && !inCache())
        delete this;
    else if (!hasClients() && inCache()) {
        cache()->removeFromLiveResourcesSize(this);
        cache()->removeFromLiveDecodedResourcesList(this);
        allClientsRemoved();
        cache()->prune();
    }
}

void CachedResource::deleteIfPossible()
{
    if (canDelete() && !inCache())
        delete this;
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
        if (m_decodedSize && !m_inLiveDecodedResourcesList && hasClients())
            cache()->insertInLiveDecodedResourcesList(this);
        else if (!m_decodedSize && m_inLiveDecodedResourcesList)
            cache()->removeFromLiveDecodedResourcesList(this);

        // Update the cache's size totals.
        cache()->adjustSize(hasClients(), delta);
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
        cache()->adjustSize(hasClients(), delta);
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
    
void CachedResource::setResourceToRevalidate(CachedResource* resource) 
{ 
    ASSERT(resource);
    ASSERT(!m_resourceToRevalidate);
    ASSERT(resource != this);
    ASSERT(!resource->m_isBeingRevalidated);
    ASSERT(m_handlesToRevalidate.isEmpty());
    ASSERT(resource->type() == type());
    resource->m_isBeingRevalidated = true;
    m_resourceToRevalidate = resource;
}

void CachedResource::clearResourceToRevalidate() 
{ 
    ASSERT(m_resourceToRevalidate);
    ASSERT(m_resourceToRevalidate->m_isBeingRevalidated);
    m_resourceToRevalidate->m_isBeingRevalidated = false;
    m_resourceToRevalidate->deleteIfPossible();
    m_handlesToRevalidate.clear();
    m_resourceToRevalidate = 0;
    deleteIfPossible();
}
    
void CachedResource::switchClientsToRevalidatedResource()
{
    ASSERT(m_resourceToRevalidate);
    ASSERT(!inCache());

    HashSet<CachedResourceHandleBase*>::iterator end = m_handlesToRevalidate.end();
    for (HashSet<CachedResourceHandleBase*>::iterator it = m_handlesToRevalidate.begin(); it != end; ++it) {
        CachedResourceHandleBase* handle = *it;
        handle->m_resource = m_resourceToRevalidate;
        m_resourceToRevalidate->registerHandle(handle);
        --m_handleCount;
    }
    ASSERT(!m_handleCount);
    m_handlesToRevalidate.clear();

    Vector<CachedResourceClient*> clientsToMove;
    HashCountedSet<CachedResourceClient*>::iterator end2 = m_clients.end();
    for (HashCountedSet<CachedResourceClient*>::iterator it = m_clients.begin(); it != end2; ++it) {
        CachedResourceClient* client = it->first;
        unsigned count = it->second;
        while (count) {
            clientsToMove.append(client);
            --count;
        }
    }
    // Equivalent of calling removeClient() for all clients
    m_clients.clear();
    
    unsigned moveCount = clientsToMove.size();
    for (unsigned n = 0; n < moveCount; ++n)
        m_resourceToRevalidate->addClient(clientsToMove[n]);
}
    
bool CachedResource::canUseCacheValidator() const
{
    return !m_loading && (!m_response.httpHeaderField("Last-Modified").isEmpty() || !m_response.httpHeaderField("ETag").isEmpty());
}
    
bool CachedResource::mustRevalidate(CachePolicy cachePolicy) const
{
    if (m_loading)
        return false;

    // FIXME: Also look at max-age, min-fresh, max-stale in Cache-Control
    if (cachePolicy == CachePolicyCache)
        return m_response.cacheControlContainsNoCache() || (isExpired() && m_response.cacheControlContainsMustRevalidate());
    return isExpired() || m_response.cacheControlContainsNoCache();
}

bool CachedResource::makePurgeable(bool purgeable) 
{ 
    if (purgeable) {
        ASSERT(!hasClients());

        if (m_purgeableData) {
            ASSERT(!m_data);
            return true;
        }
        if (!m_data)
            return false;
        
        // Should not make buffer purgeable if it has refs othen than this since we don't want two copies.
        if (!m_data->hasOneRef())
            return false;
        
        // Purgeable buffers are allocated in multiples of the page size (4KB in common CPUs) so it does not make sense for very small buffers.
        const size_t purgeableThreshold = 4 * 4096;
        if (m_data->size() < purgeableThreshold)
            return false;
        
        if (m_data->hasPurgeableBuffer()) {
            m_purgeableData.set(m_data->releasePurgeableBuffer());
        } else {
            m_purgeableData.set(PurgeableBuffer::create(m_data->data(), m_data->size()));
            if (!m_purgeableData)
                return false;
        }
        
        m_purgeableData->makePurgeable(true);
        m_data.clear();
        return true;
    }

    if (!m_purgeableData)
        return true;
    ASSERT(!m_data);
    ASSERT(!hasClients());

    if (!m_purgeableData->makePurgeable(false))
        return false; 

    m_data = SharedBuffer::adoptPurgeableBuffer(m_purgeableData.release());
    return true;
}

bool CachedResource::isPurgeable() const
{
    return m_purgeableData && m_purgeableData->isPurgeable();
}

bool CachedResource::wasPurged() const
{
    return m_purgeableData && m_purgeableData->wasPurged();
}

unsigned CachedResource::overheadSize() const
{
   
    // FIXME: Find some programmatic lighweight way to calculate response size, and size of the different CachedResource classes.  
    // This is a rough estimate of resource overhead based on stats collected from the stress test.
    return sizeof(CachedResource) + 3648;
    
    /*  sizeof(CachedResource) + 
        192 +                        // average size of m_url.
        384 +                        // average size of m_clients hash map.
        1280 * 2 +                   // average size of ResourceResponse.  Doubled to account for the WebCore copy and the CF copy. 
                                     // Mostly due to the size of the hash maps, the Header Map strings and the URL.
        256 * 2                      // Overhead from ResourceRequest, doubled to account for WebCore copy and CF copy. 
                                     // Mostly due to the URL and Header Map.
    */
}

}
