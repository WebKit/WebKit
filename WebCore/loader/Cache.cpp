/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
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
#include "Cache.h"

#include "CachedCSSStyleSheet.h"
#include "CachedImage.h"
#include "CachedScript.h"
#include "CachedXSLStyleSheet.h"
#include "DocLoader.h"
#include "Document.h"
#include "FrameLoader.h"
#include "Image.h"
#include "ResourceHandle.h"

using namespace std;

namespace WebCore {

const int cDefaultCacheSize = 8192 * 1024;

Cache* cache()
{
    static Cache cache;
    return &cache;
}

Cache::Cache()
: m_disabled(false)
, m_maximumSize(cDefaultCacheSize)
, m_currentSize(0)
, m_liveResourcesSize(0)
{
}

static CachedResource* createResource(CachedResource::Type type, DocLoader* docLoader, const KURL& url, time_t expireDate, const String* charset)
{
    switch (type) {
    case CachedResource::ImageResource:
        // User agent images need to null check the docloader.  No other resources need to.
        return new CachedImage(docLoader, url.url(), docLoader ? docLoader->cachePolicy() : CachePolicyCache, expireDate);
    case CachedResource::CSSStyleSheet:
        return new CachedCSSStyleSheet(docLoader, url.url(), docLoader->cachePolicy(), expireDate, *charset);
    case CachedResource::Script:
        return new CachedScript(docLoader, url.url(), docLoader->cachePolicy(), expireDate, *charset);
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
        return new CachedXSLStyleSheet(docLoader, url.url(), docLoader->cachePolicy(), expireDate);
#endif
#if ENABLE(XBL)
    case CachedResource::XBLStyleSheet:
        return new CachedXBLDocument(docLoader, url.url(), docLoader->cachePolicy(), expireDate);
#endif
    default:
        break;
    }

    return 0;
}

CachedResource* Cache::requestResource(DocLoader* docLoader, CachedResource::Type type, const KURL& url, time_t expireDate, const String* charset)
{
    // Look up the resource in our map.
    CachedResource* resource = m_resources.get(url.url());

    if (resource) {
        if (FrameLoader::restrictAccessToLocal()
         && !FrameLoader::canLoad(*resource, docLoader->doc()))
            return 0;
    } else {
        if (FrameLoader::restrictAccessToLocal()
         && !FrameLoader::canLoad(url, docLoader->doc()))
            return 0;

        // The resource does not exist.  Create it.
        resource = createResource(type, docLoader, url, expireDate, charset);
        ASSERT(resource);
        resource->setInCache(!disabled());
        if (!disabled())
            m_resources.set(url.url(), resource);  // The size will be added in later once the resource is loaded and calls back to us with the new size.
    }

    // This will move the resource to the front of its LRU list and increase its access count.
    resourceAccessed(resource);

    if (resource->type() != type)
        return 0;

    return resource;
}

CachedResource* Cache::resourceForURL(const String& url)
{
    return m_resources.get(url);
}

void Cache::prune()
{
    // No need to prune if all of our objects fit.
    if (m_currentSize <= m_maximumSize)
        return;

    // We allow the cache to get as big as the # of live objects + the maximum cache size
    // before we do a prune.  Once we do decide to prune though, we are aggressive about it.
    // We will include the live objects as part of the overall cache size when pruning, so will often
    // kill every last object that isn't referenced by a Web page.
    unsigned unreferencedResourcesSize = m_currentSize - m_liveResourcesSize;
    if (unreferencedResourcesSize < m_maximumSize)
        return;

    bool canShrinkLRULists = true;
    unsigned size = m_lruLists.size();
    for (int i = size - 1; i >= 0; i--) {
        // Remove from the tail, since this is the least frequently accessed of the objects.
        CachedResource* current = m_lruLists[i].m_tail;
        while (current) {
            CachedResource* prev = current->m_prevInLRUList;
            if (!current->referenced()) {
                remove(current);
                
                // Stop pruning if our total cache size is back under the maximum or if every
                // remaining object in the cache is live (meaning there is nothing left we are able
                // to prune).
                if (m_currentSize <= m_maximumSize || m_currentSize == m_liveResourcesSize)
                    return;
            }
            current = prev;
        }
            
        // Shrink the vector back down so we don't waste time inspecting
        // empty LRU lists on future prunes.
        if (m_lruLists[i].m_head)
            canShrinkLRULists = false;
        else if (canShrinkLRULists)
            m_lruLists.resize(i);
    }
}

void Cache::setMaximumSize(unsigned bytes)
{
    m_maximumSize = bytes;
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

        // Notify all doc loaders that might be observing this object still that it has been
        // extracted from the set of resources.
        HashSet<DocLoader*>::iterator end = m_docLoaders.end();
        for (HashSet<DocLoader*>::iterator itr = m_docLoaders.begin(); itr != end; ++itr)
            (*itr)->removeCachedResource(resource);

        // Subtract from our size totals.
        m_currentSize -= resource->size();
        if (resource->referenced())
            m_liveResourcesSize -= resource->size();
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

LRUList* Cache::lruListFor(CachedResource* resource)
{
    unsigned accessCount = max(resource->accessCount(), 1U);
    unsigned queueIndex = fastLog2(resource->size() / accessCount);
#ifndef NDEBUG
    resource->m_lruIndex = queueIndex;
#endif
    if (m_lruLists.size() <= queueIndex)
        m_lruLists.resize(queueIndex + 1);
    return &m_lruLists[queueIndex];
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
    for (CachedResource* current = list->m_head; current; current = current->m_nextInLRUList) {
        if (current == resource) {
            found = true;
            break;
        }
    }
    ASSERT(found);
#endif

    CachedResource* next = resource->m_nextInLRUList;
    CachedResource* prev = resource->m_prevInLRUList;
    
    if (next == 0 && prev == 0 && list->m_head != resource)
        return;
    
    resource->m_nextInLRUList = 0;
    resource->m_prevInLRUList = 0;
    
    if (next)
        next->m_prevInLRUList = prev;
    else if (list->m_tail == resource)
        list->m_tail = prev;

    if (prev)
        prev->m_nextInLRUList = next;
    else if (list->m_head == resource)
        list->m_head = next;
}

void Cache::insertInLRUList(CachedResource* resource)
{
    // Make sure we aren't in some list already.
    ASSERT(!resource->m_nextInLRUList && !resource->m_prevInLRUList);

    LRUList* list = lruListFor(resource);

    resource->m_nextInLRUList = list->m_head;
    if (list->m_head)
        list->m_head->m_prevInLRUList = resource;
    list->m_head = resource;
    
    if (!resource->m_nextInLRUList)
        list->m_tail = resource;
        
#ifndef NDEBUG
    // Verify that we are in now in the list like we should be.
    list = lruListFor(resource);
    bool found = false;
    for (CachedResource* current = list->m_head; current; current = current->m_nextInLRUList) {
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
    // Need to make sure to remove before we increase the access count, since
    // the queue will possibly change.
    removeFromLRUList(resource);
    
    // Add to our access count.
    resource->increaseAccessCount();
    
    // Now insert into the new queue.
    insertInLRUList(resource);
}

void Cache::adjustSize(bool live, unsigned oldResourceSize, unsigned newResourceSize)
{
    m_currentSize -= oldResourceSize;
    if (live)
        m_liveResourcesSize -= oldResourceSize;
        
    m_currentSize += newResourceSize;
    if (live)
        m_liveResourcesSize += newResourceSize;
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
                break;

            case CachedResource::CSSStyleSheet:
                stats.cssStyleSheets.count++;
                stats.cssStyleSheets.size += o->size();
                break;

            case CachedResource::Script:
                stats.scripts.count++;
                stats.scripts.size += o->size();
                break;
#if ENABLE(XSLT)
            case CachedResource::XSLStyleSheet:
                stats.xslStyleSheets.count++;
                stats.xslStyleSheets.size += o->size();
                break;
#endif
#if ENABLE(XBL)
            case CachedResource::XBL:
                stats.xblDocs.count++;
                stats.xblDocs.size += o->size();
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

}
