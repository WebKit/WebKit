/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
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
#include "Cache.h"

#include "CachedCSSStyleSheet.h"
#include "CachedFont.h"
#include "CachedImage.h"
#include "CachedScript.h"
#include "CachedXSLStyleSheet.h"
#include "DocLoader.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Image.h"
#include "ResourceHandle.h"
#include "SystemTime.h"

using namespace std;

namespace WebCore {

static const int cDefaultCacheCapacity = 8192 * 1024;
static const double cMinDelayBeforeLiveDecodedPrune = 1; // Seconds.
static const float cTargetPrunePercentage = .95f; // Percentage of capacity toward which we prune, to avoid immediately pruning again.

Cache* cache()
{
    static Cache* staticCache = new Cache;
    return staticCache;
}

Cache::Cache()
: m_disabled(false)
, m_pruneEnabled(true)
, m_capacity(cDefaultCacheCapacity)
, m_minDeadCapacity(0)
, m_maxDeadCapacity(cDefaultCacheCapacity)
, m_liveSize(0)
, m_deadSize(0)
{
}

static CachedResource* createResource(CachedResource::Type type, DocLoader* docLoader, const KURL& url, const String* charset, bool skipCanLoadCheck = false, bool sendResourceLoadCallbacks = true)
{
    switch (type) {
    case CachedResource::ImageResource:
        // User agent images need to null check the docloader.  No other resources need to.
        return new CachedImage(docLoader, url.string(), true /* for cache */);
    case CachedResource::CSSStyleSheet:
        return new CachedCSSStyleSheet(docLoader, url.string(), *charset, skipCanLoadCheck, sendResourceLoadCallbacks);
    case CachedResource::Script:
        return new CachedScript(docLoader, url.string(), *charset);
    case CachedResource::FontResource:
        return new CachedFont(docLoader, url.string());
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
        return new CachedXSLStyleSheet(docLoader, url.string());
#endif
#if ENABLE(XBL)
    case CachedResource::XBLStyleSheet:
        return new CachedXBLDocument(docLoader, url.string());
#endif
    default:
        break;
    }

    return 0;
}

CachedResource* Cache::requestResource(DocLoader* docLoader, CachedResource::Type type, const KURL& url, const String* charset, bool skipCanLoadCheck, bool sendResourceLoadCallbacks)
{
    // FIXME: Do we really need to special-case an empty URL?
    // Would it be better to just go on with the cache code and let it fail later?
    if (url.isEmpty())
        return 0;
    
    // Look up the resource in our map.
    CachedResource* resource = m_resources.get(url.string());

    if (resource) {
        if (!skipCanLoadCheck && FrameLoader::restrictAccessToLocal() && !FrameLoader::canLoad(*resource, docLoader->doc())) {
            Document* doc = docLoader->doc();
            if(doc)
                FrameLoader::reportLocalLoadFailed(doc->page(), resource->url());

            return 0;
        }
    } else {
        if (!skipCanLoadCheck && FrameLoader::restrictAccessToLocal() && !FrameLoader::canLoad(url, docLoader->doc())) {
            Document* doc = docLoader->doc();
            if(doc)
                FrameLoader::reportLocalLoadFailed(doc->page(), url.string());

            return 0;
        }

        // The resource does not exist. Create it.
        resource = createResource(type, docLoader, url, charset, skipCanLoadCheck, sendResourceLoadCallbacks);
        ASSERT(resource);
        ASSERT(resource->inCache());
        if (!disabled()) {
            m_resources.set(url.string(), resource);  // The size will be added in later once the resource is loaded and calls back to us with the new size.
            
            // This will move the resource to the front of its LRU list and increase its access count.
            resourceAccessed(resource);
        } else {
            // Kick the resource out of the cache, because the cache is disabled.
            resource->setInCache(false);
            resource->setDocLoader(docLoader);
            if (resource->errorOccurred()) {
                // We don't support immediate loads, but we do support immediate failure.
                // In that case we should to delete the resource now and return 0 because otherwise
                // it would leak if no ref/deref was ever done on it.
                delete resource;
                return 0;
            }
        }
    }

    if (resource->type() != type)
        return 0;

#if USE(LOW_BANDWIDTH_DISPLAY)
    // addLowBandwidthDisplayRequest() returns true if requesting CSS or JS during low bandwidth display.
    // Here, return 0 to not block parsing or layout.
    if (docLoader->frame() && docLoader->frame()->loader()->addLowBandwidthDisplayRequest(resource))
        return 0;
#endif

    return resource;
}

CachedResource* Cache::resourceForURL(const String& url)
{
    return m_resources.get(url);
}

unsigned Cache::deadCapacity() const 
{
    // Dead resource capacity is whatever space is not occupied by live resources, bounded by an independent minimum and maximum.
    unsigned capacity = m_capacity - min(m_liveSize, m_capacity); // Start with available capacity.
    capacity = max(capacity, m_minDeadCapacity); // Make sure it's above the minimum.
    capacity = min(capacity, m_maxDeadCapacity); // Make sure it's below the maximum.
    return capacity;
}

unsigned Cache::liveCapacity() const 
{ 
    // Live resource capacity is whatever is left over after calculating dead resource capacity.
    return m_capacity - deadCapacity();
}

void Cache::pruneLiveResources()
{
    if (!m_pruneEnabled)
        return;

    unsigned capacity = liveCapacity();
    if (m_liveSize <= capacity)
        return;

    unsigned targetSize = static_cast<unsigned>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.
    double currentTime = Frame::currentPaintTimeStamp();
    if (!currentTime) // In case prune is called directly, outside of a Frame paint.
        currentTime = WebCore::currentTime();
    
    // Destroy any decoded data in live objects that we can.
    // Start from the tail, since this is the least recently accessed of the objects.
    CachedResource* current = m_liveDecodedResources.m_tail;
    while (current) {
        CachedResource* prev = current->m_prevInLiveResourcesList;
        ASSERT(current->referenced());
        if (current->isLoaded() && current->decodedSize()) {
            // Check to see if the remaining resources are too new to prune.
            double elapsedTime = currentTime - current->m_lastDecodedAccessTime;
            if (elapsedTime < cMinDelayBeforeLiveDecodedPrune)
                return;

            // Destroy our decoded data. This will remove us from 
            // m_liveDecodedResources, and possibly move us to a differnt LRU 
            // list in m_allResources.
            current->destroyDecodedData();

            if (m_liveSize <= targetSize)
                return;
        }
        current = prev;
    }
}

void Cache::pruneDeadResources()
{
    if (!m_pruneEnabled)
        return;

    unsigned capacity = deadCapacity();
    if (m_deadSize <= capacity)
        return;

    unsigned targetSize = static_cast<unsigned>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.
    int size = m_allResources.size();
    bool canShrinkLRULists = true;
    for (int i = size - 1; i >= 0; i--) {
        // Remove from the tail, since this is the least frequently accessed of the objects.
        CachedResource* current = m_allResources[i].m_tail;
        
        // First flush all the decoded data in this queue.
        while (current) {
            CachedResource* prev = current->m_prevInAllResourcesList;
            if (!current->referenced() && current->isLoaded() && current->decodedSize()) {
                // Destroy our decoded data. This will remove us from 
                // m_liveDecodedResources, and possibly move us to a differnt 
                // LRU list in m_allResources.
                current->destroyDecodedData();
                
                if (m_deadSize <= targetSize)
                    return;
            }
            current = prev;
        }

        // Now evict objects from this queue.
        current = m_allResources[i].m_tail;
        while (current) {
            CachedResource* prev = current->m_prevInAllResourcesList;
            if (!current->referenced()) {
                remove(current);

                if (m_deadSize <= targetSize)
                    return;
            }
            current = prev;
        }
            
        // Shrink the vector back down so we don't waste time inspecting
        // empty LRU lists on future prunes.
        if (m_allResources[i].m_head)
            canShrinkLRULists = false;
        else if (canShrinkLRULists)
            m_allResources.resize(i);
    }
}

void Cache::setCapacities(unsigned minDeadBytes, unsigned maxDeadBytes, unsigned totalBytes)
{
    ASSERT(minDeadBytes <= maxDeadBytes);
    ASSERT(maxDeadBytes <= totalBytes);
    m_minDeadCapacity = minDeadBytes;
    m_maxDeadCapacity = maxDeadBytes;
    m_capacity = totalBytes;
    prune();
}

void Cache::remove(CachedResource* resource)
{
    // The resource may have already been removed by someone other than our caller,
    // who needed a fresh copy for a reload. See <http://bugs.webkit.org/show_bug.cgi?id=12479#c6>.
    if (resource->inCache()) {
        // Remove from the resource map.
        m_resources.remove(resource->url());
        resource->setInCache(false);

        // Remove from the appropriate LRU list.
        removeFromLRUList(resource);
        removeFromLiveDecodedResourcesList(resource);
        
        // Notify all doc loaders that might be observing this object still that it has been
        // extracted from the set of resources.
        HashSet<DocLoader*>::iterator end = m_docLoaders.end();
        for (HashSet<DocLoader*>::iterator itr = m_docLoaders.begin(); itr != end; ++itr)
            (*itr)->removeCachedResource(resource);

        // Subtract from our size totals.
        int delta = -static_cast<int>(resource->size());
        if (delta)
            adjustSize(resource->referenced(), delta);
    }

    if (resource->canDelete())
        delete resource;
}

void Cache::addDocLoader(DocLoader* docLoader)
{
    m_docLoaders.add(docLoader);
}

void Cache::removeDocLoader(DocLoader* docLoader)
{
    m_docLoaders.remove(docLoader);
}

static inline unsigned fastLog2(unsigned i)
{
    unsigned log2 = 0;
    if (i & (i - 1))
        log2 += 1;
    if (i >> 16)
        log2 += 16, i >>= 16;
    if (i >> 8)
        log2 += 8, i >>= 8;
    if (i >> 4)
        log2 += 4, i >>= 4;
    if (i >> 2)
        log2 += 2, i >>= 2;
    if (i >> 1)
        log2 += 1;
    return log2;
}

Cache::LRUList* Cache::lruListFor(CachedResource* resource)
{
    unsigned accessCount = max(resource->accessCount(), 1U);
    unsigned queueIndex = fastLog2(resource->size() / accessCount);
#ifndef NDEBUG
    resource->m_lruIndex = queueIndex;
#endif
    if (m_allResources.size() <= queueIndex)
        m_allResources.grow(queueIndex + 1);
    return &m_allResources[queueIndex];
}

void Cache::removeFromLRUList(CachedResource* resource)
{
    // If we've never been accessed, then we're brand new and not in any list.
    if (resource->accessCount() == 0)
        return;

#ifndef NDEBUG
    unsigned oldListIndex = resource->m_lruIndex;
#endif

    LRUList* list = lruListFor(resource);

#ifndef NDEBUG
    // Verify that the list we got is the list we want.
    ASSERT(resource->m_lruIndex == oldListIndex);

    // Verify that we are in fact in this list.
    bool found = false;
    for (CachedResource* current = list->m_head; current; current = current->m_nextInAllResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

    CachedResource* next = resource->m_nextInAllResourcesList;
    CachedResource* prev = resource->m_prevInAllResourcesList;
    
    if (next == 0 && prev == 0 && list->m_head != resource)
        return;
    
    resource->m_nextInAllResourcesList = 0;
    resource->m_prevInAllResourcesList = 0;
    
    if (next)
        next->m_prevInAllResourcesList = prev;
    else if (list->m_tail == resource)
        list->m_tail = prev;

    if (prev)
        prev->m_nextInAllResourcesList = next;
    else if (list->m_head == resource)
        list->m_head = next;
}

void Cache::insertInLRUList(CachedResource* resource)
{
    // Make sure we aren't in some list already.
    ASSERT(!resource->m_nextInAllResourcesList && !resource->m_prevInAllResourcesList);
    ASSERT(resource->inCache());
    
    LRUList* list = lruListFor(resource);

    resource->m_nextInAllResourcesList = list->m_head;
    if (list->m_head)
        list->m_head->m_prevInAllResourcesList = resource;
    list->m_head = resource;
    
    if (!resource->m_nextInAllResourcesList)
        list->m_tail = resource;
        
#ifndef NDEBUG
    // Verify that we are in now in the list like we should be.
    list = lruListFor(resource);
    bool found = false;
    for (CachedResource* current = list->m_head; current; current = current->m_nextInAllResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

}

void Cache::resourceAccessed(CachedResource* resource)
{
    ASSERT(resource->inCache());
    
    // Need to make sure to remove before we increase the access count, since
    // the queue will possibly change.
    removeFromLRUList(resource);
    
    // Add to our access count.
    resource->increaseAccessCount();
    
    // Now insert into the new queue.
    insertInLRUList(resource);
}

void Cache::removeFromLiveDecodedResourcesList(CachedResource* resource)
{
    // If we've never been accessed, then we're brand new and not in any list.
    if (!resource->m_inLiveDecodedResourcesList)
        return;
    resource->m_inLiveDecodedResourcesList = false;

#ifndef NDEBUG
    // Verify that we are in fact in this list.
    bool found = false;
    for (CachedResource* current = m_liveDecodedResources.m_head; current; current = current->m_nextInLiveResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

    CachedResource* next = resource->m_nextInLiveResourcesList;
    CachedResource* prev = resource->m_prevInLiveResourcesList;
    
    if (next == 0 && prev == 0 && m_liveDecodedResources.m_head != resource)
        return;
    
    resource->m_nextInLiveResourcesList = 0;
    resource->m_prevInLiveResourcesList = 0;
    
    if (next)
        next->m_prevInLiveResourcesList = prev;
    else if (m_liveDecodedResources.m_tail == resource)
        m_liveDecodedResources.m_tail = prev;

    if (prev)
        prev->m_nextInLiveResourcesList = next;
    else if (m_liveDecodedResources.m_head == resource)
        m_liveDecodedResources.m_head = next;
}

void Cache::insertInLiveDecodedResourcesList(CachedResource* resource)
{
    // Make sure we aren't in the list already.
    ASSERT(!resource->m_nextInLiveResourcesList && !resource->m_prevInLiveResourcesList && !resource->m_inLiveDecodedResourcesList);
    resource->m_inLiveDecodedResourcesList = true;

    resource->m_nextInLiveResourcesList = m_liveDecodedResources.m_head;
    if (m_liveDecodedResources.m_head)
        m_liveDecodedResources.m_head->m_prevInLiveResourcesList = resource;
    m_liveDecodedResources.m_head = resource;
    
    if (!resource->m_nextInLiveResourcesList)
        m_liveDecodedResources.m_tail = resource;
        
#ifndef NDEBUG
    // Verify that we are in now in the list like we should be.
    bool found = false;
    for (CachedResource* current = m_liveDecodedResources.m_head; current; current = current->m_nextInLiveResourcesList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

}

void Cache::addToLiveResourcesSize(CachedResource* resource)
{
    m_liveSize += resource->size();
    m_deadSize -= resource->size();
}

void Cache::removeFromLiveResourcesSize(CachedResource* resource)
{
    m_liveSize -= resource->size();
    m_deadSize += resource->size();
}

void Cache::adjustSize(bool live, int delta)
{
    if (live) {
        ASSERT(delta >= 0 || ((int)m_liveSize + delta >= 0));
        m_liveSize += delta;
    } else {
        ASSERT(delta >= 0 || ((int)m_deadSize + delta >= 0));
        m_deadSize += delta;
    }
}

Cache::Statistics Cache::getStatistics()
{
    Statistics stats;
    CachedResourceMap::iterator e = m_resources.end();
    for (CachedResourceMap::iterator i = m_resources.begin(); i != e; ++i) {
        CachedResource *o = i->second;
        switch (o->type()) {
            case CachedResource::ImageResource:
                stats.images.count++;
                stats.images.size += o->size();
                stats.images.liveSize += o->referenced() ? o->size() : 0;
                stats.images.decodedSize += o->decodedSize();
                break;

            case CachedResource::CSSStyleSheet:
                stats.cssStyleSheets.count++;
                stats.cssStyleSheets.size += o->size();
                stats.cssStyleSheets.liveSize += o->referenced() ? o->size() : 0;
                stats.cssStyleSheets.decodedSize += o->decodedSize();
                break;

            case CachedResource::Script:
                stats.scripts.count++;
                stats.scripts.size += o->size();
                stats.scripts.liveSize += o->referenced() ? o->size() : 0;
                stats.scripts.decodedSize += o->decodedSize();
                break;
#if ENABLE(XSLT)
            case CachedResource::XSLStyleSheet:
                stats.xslStyleSheets.count++;
                stats.xslStyleSheets.size += o->size();
                stats.xslStyleSheets.liveSize += o->referenced() ? o->size() : 0;
                stats.xslStyleSheets.decodedSize += o->decodedSize();
                break;
#endif
            case CachedResource::FontResource:
                stats.fonts.count++;
                stats.fonts.size += o->size();
                stats.fonts.liveSize += o->referenced() ? o->size() : 0;
                stats.fonts.decodedSize += o->decodedSize();
                break;
#if ENABLE(XBL)
            case CachedResource::XBL:
                stats.xblDocs.count++;
                stats.xblDocs.size += o->size();
                stats.xblDocs.liveSize += o->referenced() ? o->size() : 0;
                stats.xblDocs.decodedSize += o->decodedSize();
                break;
#endif
            default:
                break;
        }
    }
    
    return stats;
}

void Cache::setDisabled(bool disabled)
{
    m_disabled = disabled;
    if (!m_disabled)
        return;

    for (;;) {
        CachedResourceMap::iterator i = m_resources.begin();
        if (i == m_resources.end())
            break;
        remove(i->second);
    }
}

#ifndef NDEBUG
void Cache::dumpLRULists(bool includeLive) const
{
    printf("LRU-SP lists in eviction order (Kilobytes decoded, Kilobytes encoded, Access count, Referenced):\n");

    int size = m_allResources.size();
    for (int i = size - 1; i >= 0; i--) {
        printf("\n\nList %d: ", i);
        CachedResource* current = m_allResources[i].m_tail;
        while (current) {
            CachedResource* prev = current->m_prevInAllResourcesList;
            if (includeLive || !current->referenced())
                printf("(%.1fK, %.1fK, %uA, %dR); ", current->decodedSize() / 1024.0f, current->encodedSize() / 1024.0f, current->accessCount(), current->referenced());
            current = prev;
        }
    }
}
#endif

} // namespace WebCore
