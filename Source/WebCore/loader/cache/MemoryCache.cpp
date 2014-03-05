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
#include "MemoryCache.h"

#include "BitmapImage.h"
#include "CachedImage.h"
#include "CachedImageClient.h"
#include "CachedResource.h"
#include "CachedResourceHandle.h"
#include "CrossThreadTask.h"
#include "Document.h"
#include "FrameLoader.h"
#include "FrameLoaderTypes.h"
#include "FrameView.h"
#include "Image.h"
#include "Logging.h"
#include "PublicSuffix.h"
#include "SecurityOrigin.h"
#include "SecurityOriginHash.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include <stdio.h>
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>
#include <wtf/TemporaryChange.h>
#include <wtf/text/CString.h>

#if ENABLE(DISK_IMAGE_CACHE)
#include "DiskImageCacheIOS.h"
#include "ResourceBuffer.h"
#endif

namespace WebCore {

static const int cDefaultCacheCapacity = 8192 * 1024;
static const double cMinDelayBeforeLiveDecodedPrune = 1; // Seconds.
static const float cTargetPrunePercentage = .95f; // Percentage of capacity toward which we prune, to avoid immediately pruning again.
static const double cDefaultDecodedDataDeletionInterval = 0;

MemoryCache* memoryCache()
{
    static MemoryCache* staticCache = new MemoryCache;
    ASSERT(WTF::isMainThread());

    return staticCache;
}

MemoryCache::MemoryCache()
    : m_disabled(false)
    , m_pruneEnabled(true)
    , m_inPruneResources(false)
    , m_capacity(cDefaultCacheCapacity)
    , m_minDeadCapacity(0)
    , m_maxDeadCapacity(cDefaultCacheCapacity)
    , m_deadDecodedDataDeletionInterval(cDefaultDecodedDataDeletionInterval)
    , m_liveSize(0)
    , m_deadSize(0)
{
}

MemoryCache::CachedResourceMap& MemoryCache::getSessionMap(SessionID sessionID)
{
    ASSERT(sessionID.isValid());
    CachedResourceMap* map = m_sessionResources.get(sessionID);
    if (!map) {
        m_sessionResources.set(sessionID, std::make_unique<CachedResourceMap>());
        map = m_sessionResources.get(sessionID);
    }
    return *map;
}

URL MemoryCache::removeFragmentIdentifierIfNeeded(const URL& originalURL)
{
    if (!originalURL.hasFragmentIdentifier())
        return originalURL;
    // Strip away fragment identifier from HTTP URLs.
    // Data URLs must be unmodified. For file and custom URLs clients may expect resources 
    // to be unique even when they differ by the fragment identifier only.
    if (!originalURL.protocolIsInHTTPFamily())
        return originalURL;
    URL url = originalURL;
    url.removeFragmentIdentifier();
    return url;
}

bool MemoryCache::add(CachedResource* resource)
{
    if (disabled())
        return false;

    ASSERT(WTF::isMainThread());

    CachedResourceMap& resources = getSessionMap(resource->sessionID());
#if ENABLE(CACHE_PARTITIONING)
    CachedResourceItem* originMap = resources.get(resource->url());
    if (!originMap) {
        originMap = new CachedResourceItem;
        resources.set(resource->url(), adoptPtr(originMap));
    }
    originMap->set(resource->cachePartition(), resource);
#else
    resources.set(resource->url(), resource);
#endif
    resource->setInCache(true);
    
    resourceAccessed(resource);
    
    LOG(ResourceLoading, "MemoryCache::add Added '%s', resource %p\n", resource->url().string().latin1().data(), resource);
    return true;
}

void MemoryCache::revalidationSucceeded(CachedResource* revalidatingResource, const ResourceResponse& response)
{
    CachedResource* resource = revalidatingResource->resourceToRevalidate();
    ASSERT(resource);
    ASSERT(!resource->inCache());
    ASSERT(resource->isLoaded());
    ASSERT(revalidatingResource->inCache());

    // Calling evict() can potentially delete revalidatingResource, which we use
    // below. This mustn't be the case since revalidation means it is loaded
    // and so canDelete() is false.
    ASSERT(!revalidatingResource->canDelete());

    evict(revalidatingResource);

    CachedResourceMap& resources = getSessionMap(resource->sessionID());
#if ENABLE(CACHE_PARTITIONING)
    ASSERT(!resources.get(resource->url()) || !resources.get(resource->url())->get(resource->cachePartition()));
    CachedResourceItem* originMap = resources.get(resource->url());
    if (!originMap) {
        originMap = new CachedResourceItem;
        resources.set(resource->url(), adoptPtr(originMap));
    }
    originMap->set(resource->cachePartition(), resource);
#else
    ASSERT(!resources.get(resource->url()));
    resources.set(resource->url(), resource);
#endif
    resource->setInCache(true);
    resource->updateResponseAfterRevalidation(response);
    insertInLRUList(resource);
    int delta = resource->size();
    if (resource->decodedSize() && resource->hasClients())
        insertInLiveDecodedResourcesList(resource);
    if (delta)
        adjustSize(resource->hasClients(), delta);
    
    revalidatingResource->switchClientsToRevalidatedResource();
    ASSERT(!revalidatingResource->m_deleted);
    // this deletes the revalidating resource
    revalidatingResource->clearResourceToRevalidate();
}

void MemoryCache::revalidationFailed(CachedResource* revalidatingResource)
{
    ASSERT(WTF::isMainThread());
    LOG(ResourceLoading, "Revalidation failed for %p", revalidatingResource);
    ASSERT(revalidatingResource->resourceToRevalidate());
    revalidatingResource->clearResourceToRevalidate();
}

CachedResource* MemoryCache::resourceForURL(const URL& resourceURL)
{
    return resourceForURL(resourceURL, SessionID::defaultSessionID());
}

CachedResource* MemoryCache::resourceForURL(const URL& resourceURL, SessionID sessionID)
{
    return resourceForRequest(ResourceRequest(resourceURL), sessionID);
}

CachedResource* MemoryCache::resourceForRequest(const ResourceRequest& request, SessionID sessionID)
{
    return resourceForRequestImpl(request, getSessionMap(sessionID));
}

CachedResource* MemoryCache::resourceForRequestImpl(const ResourceRequest& request, CachedResourceMap& resources)
{
    ASSERT(WTF::isMainThread());
    URL url = removeFragmentIdentifierIfNeeded(request.url());

#if ENABLE(CACHE_PARTITIONING)
    CachedResourceItem* item = resources.get(url);
    CachedResource* resource = 0;
    if (item)
        resource = item->get(request.cachePartition());
#else
    CachedResource* resource = resources.get(url);
#endif
    bool wasPurgeable = MemoryCache::shouldMakeResourcePurgeableOnEviction() && resource && resource->isPurgeable();
    if (resource && !resource->makePurgeable(false)) {
        ASSERT(!resource->hasClients());
        evict(resource);
        return 0;
    }
    // Add the size back since we had subtracted it when we marked the memory as purgeable.
    if (wasPurgeable)
        adjustSize(resource->hasClients(), resource->size());
    return resource;
}

unsigned MemoryCache::deadCapacity() const 
{
    // Dead resource capacity is whatever space is not occupied by live resources, bounded by an independent minimum and maximum.
    unsigned capacity = m_capacity - std::min(m_liveSize, m_capacity); // Start with available capacity.
    capacity = std::max(capacity, m_minDeadCapacity); // Make sure it's above the minimum.
    capacity = std::min(capacity, m_maxDeadCapacity); // Make sure it's below the maximum.
    return capacity;
}

unsigned MemoryCache::liveCapacity() const 
{ 
    // Live resource capacity is whatever is left over after calculating dead resource capacity.
    return m_capacity - deadCapacity();
}

#if USE(CG)
// FIXME: Remove the USE(CG) once we either make NativeImagePtr a smart pointer on all platforms or
// remove the usage of CFRetain() in MemoryCache::addImageToCache() so as to make the code platform-independent.
static CachedImageClient& dummyCachedImageClient()
{
    DEFINE_STATIC_LOCAL(CachedImageClient, client, ());
    return client;
}

bool MemoryCache::addImageToCache(NativeImagePtr image, const URL& url, const String& cachePartition)
{
    ASSERT(image);
    SessionID sessionID = SessionID::defaultSessionID();
    removeImageFromCache(url, cachePartition); // Remove cache entry if it already exists.

    RefPtr<BitmapImage> bitmapImage = BitmapImage::create(image, nullptr);
    if (!bitmapImage)
        return false;

    std::unique_ptr<CachedImage> cachedImage = std::make_unique<CachedImage>(url, bitmapImage.get(), CachedImage::ManuallyCached, sessionID);

    // Actual release of the CGImageRef is done in BitmapImage.
    CFRetain(image);
    cachedImage->addClient(&dummyCachedImageClient());
    cachedImage->setDecodedSize(bitmapImage->decodedSize());
#if ENABLE(CACHE_PARTITIONING)
    cachedImage->resourceRequest().setCachePartition(cachePartition);
#endif
    return true;
}

void MemoryCache::removeImageFromCache(const URL& url, const String& cachePartition)
{
    CachedResourceMap& resources = getSessionMap(SessionID::defaultSessionID());
#if ENABLE(CACHE_PARTITIONING)
    CachedResource* resource;
    if (CachedResourceItem* item = resources.get(url))
        resource = item->get(ResourceRequest::partitionName(cachePartition));
    else
        resource = nullptr;
#else
    UNUSED_PARAM(cachePartition);
    CachedResource* resource = resources.get(url);
#endif
    if (!resource)
        return;

    // A resource exists and is not a manually cached image, so just remove it.
    if (!resource->isImage() || !toCachedImage(resource)->isManuallyCached()) {
        evict(resource);
        return;
    }

    // Removing the last client of a CachedImage turns the resource
    // into a dead resource which will eventually be evicted when
    // dead resources are pruned. That might be immediately since
    // removing the last client triggers a MemoryCache::prune, so the
    // resource may be deleted after this call.
    toCachedImage(resource)->removeClient(&dummyCachedImageClient());
}
#endif

void MemoryCache::pruneLiveResources(bool shouldDestroyDecodedDataForAllLiveResources)
{
    if (!m_pruneEnabled)
        return;

    unsigned capacity = shouldDestroyDecodedDataForAllLiveResources ? 0 : liveCapacity();
    if (capacity && m_liveSize <= capacity)
        return;

    unsigned targetSize = static_cast<unsigned>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.

    pruneLiveResourcesToSize(targetSize, shouldDestroyDecodedDataForAllLiveResources);
}

void MemoryCache::pruneLiveResourcesToPercentage(float prunePercentage)
{
    if (!m_pruneEnabled)
        return;

    if (prunePercentage < 0.0f  || prunePercentage > 0.95f)
        return;

    unsigned currentSize = m_liveSize + m_deadSize;
    unsigned targetSize = static_cast<unsigned>(currentSize * prunePercentage);

    pruneLiveResourcesToSize(targetSize);
}

void MemoryCache::pruneLiveResourcesToSize(unsigned targetSize, bool shouldDestroyDecodedDataForAllLiveResources)
{
    if (m_inPruneResources)
        return;
    TemporaryChange<bool> reentrancyProtector(m_inPruneResources, true);

    double currentTime = FrameView::currentPaintTimeStamp();
    if (!currentTime) // In case prune is called directly, outside of a Frame paint.
        currentTime = monotonicallyIncreasingTime();
    
    // Destroy any decoded data in live objects that we can.
    // Start from the tail, since this is the least recently accessed of the objects.

    // The list might not be sorted by the m_lastDecodedAccessTime. The impact
    // of this weaker invariant is minor as the below if statement to check the
    // elapsedTime will evaluate to false as the currentTime will be a lot
    // greater than the current->m_lastDecodedAccessTime.
    // For more details see: https://bugs.webkit.org/show_bug.cgi?id=30209
    CachedResource* current = m_liveDecodedResources.m_tail;
    while (current) {
        CachedResource* prev = current->m_prevInLiveResourcesList;
        ASSERT(current->hasClients());
        if (current->isLoaded() && current->decodedSize()) {
            // Check to see if the remaining resources are too new to prune.
            double elapsedTime = currentTime - current->m_lastDecodedAccessTime;
            if (!shouldDestroyDecodedDataForAllLiveResources && elapsedTime < cMinDelayBeforeLiveDecodedPrune)
                return;

            // Destroy our decoded data. This will remove us from 
            // m_liveDecodedResources, and possibly move us to a different LRU 
            // list in m_allResources.
            current->destroyDecodedData();

            if (targetSize && m_liveSize <= targetSize)
                return;
        }
        current = prev;
    }
}

void MemoryCache::pruneDeadResources()
{
    if (!m_pruneEnabled)
        return;

    unsigned capacity = deadCapacity();
    if (capacity && m_deadSize <= capacity)
        return;

    unsigned targetSize = static_cast<unsigned>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.
    pruneDeadResourcesToSize(targetSize);
}

void MemoryCache::pruneDeadResourcesToPercentage(float prunePercentage)
{
    if (!m_pruneEnabled)
        return;

    if (prunePercentage < 0.0f  || prunePercentage > 0.95f)
        return;

    unsigned currentSize = m_liveSize + m_deadSize;
    unsigned targetSize = static_cast<unsigned>(currentSize * prunePercentage);

    pruneDeadResourcesToSize(targetSize);
}

void MemoryCache::pruneDeadResourcesToSize(unsigned targetSize)
{
    if (m_inPruneResources)
        return;
    TemporaryChange<bool> reentrancyProtector(m_inPruneResources, true);

    int size = m_allResources.size();
 
    // See if we have any purged resources we can evict.
    for (int i = 0; i < size; i++) {
        CachedResource* current = m_allResources[i].m_tail;
        while (current) {
            CachedResource* prev = current->m_prevInAllResourcesList;
            if (current->wasPurged()) {
                ASSERT(!current->hasClients());
                ASSERT(!current->isPreloaded());
                evict(current);
            }
            current = prev;
        }
    }
    if (targetSize && m_deadSize <= targetSize)
        return;

    bool canShrinkLRULists = true;
    for (int i = size - 1; i >= 0; i--) {
        // Remove from the tail, since this is the least frequently accessed of the objects.
        CachedResource* current = m_allResources[i].m_tail;
        
        // First flush all the decoded data in this queue.
        while (current) {
            // Protect 'previous' so it can't get deleted during destroyDecodedData().
            CachedResourceHandle<CachedResource> previous = current->m_prevInAllResourcesList;
            ASSERT(!previous || previous->inCache());
            if (!current->hasClients() && !current->isPreloaded() && current->isLoaded()) {
                // Destroy our decoded data. This will remove us from 
                // m_liveDecodedResources, and possibly move us to a different 
                // LRU list in m_allResources.
                current->destroyDecodedData();

                if (targetSize && m_deadSize <= targetSize)
                    return;
            }
            // Decoded data may reference other resources. Stop iterating if 'previous' somehow got
            // kicked out of cache during destroyDecodedData().
            if (previous && !previous->inCache())
                break;
            current = previous.get();
        }

        // Now evict objects from this queue.
        current = m_allResources[i].m_tail;
        while (current) {
            CachedResourceHandle<CachedResource> previous = current->m_prevInAllResourcesList;
            ASSERT(!previous || previous->inCache());
            if (!current->hasClients() && !current->isPreloaded() && !current->isCacheValidator()) {
                if (!makeResourcePurgeable(current))
                    evict(current);

                if (targetSize && m_deadSize <= targetSize)
                    return;
            }
            if (previous && !previous->inCache())
                break;
            current = previous.get();
        }
            
        // Shrink the vector back down so we don't waste time inspecting
        // empty LRU lists on future prunes.
        if (m_allResources[i].m_head)
            canShrinkLRULists = false;
        else if (canShrinkLRULists)
            m_allResources.resize(i);
    }
}

#if ENABLE(DISK_IMAGE_CACHE)
void MemoryCache::flushCachedImagesToDisk()
{
    if (!diskImageCache().isEnabled())
        return;

#ifndef NDEBUG
    double start = WTF::currentTimeMS();
    unsigned resourceCount = 0;
    unsigned cachedSize = 0;
#endif

    for (size_t i = m_allResources.size(); i; ) {
        --i;
        CachedResource* current = m_allResources[i].m_tail;
        while (current) {
            CachedResource* previous = current->m_prevInAllResourcesList;

            if (!current->isUsingDiskImageCache() && current->canUseDiskImageCache()) {
                current->useDiskImageCache();
                current->destroyDecodedData();
#ifndef NDEBUG
                LOG(DiskImageCache, "Cache::diskCacheResources(): attempting to save (%d) bytes", current->resourceBuffer()->sharedBuffer()->size());
                ++resourceCount;
                cachedSize += current->resourceBuffer()->sharedBuffer()->size();
#endif
            }

            current = previous;
        }
    }

#ifndef NDEBUG
    double end = WTF::currentTimeMS();
    LOG(DiskImageCache, "DiskImageCache: took (%f) ms to cache (%d) bytes for (%d) resources", end - start, cachedSize, resourceCount);
#endif
}
#endif // ENABLE(DISK_IMAGE_CACHE)

void MemoryCache::setCapacities(unsigned minDeadBytes, unsigned maxDeadBytes, unsigned totalBytes)
{
    ASSERT(minDeadBytes <= maxDeadBytes);
    ASSERT(maxDeadBytes <= totalBytes);
    m_minDeadCapacity = minDeadBytes;
    m_maxDeadCapacity = maxDeadBytes;
    m_capacity = totalBytes;
    prune();
}

bool MemoryCache::makeResourcePurgeable(CachedResource* resource)
{
    if (!MemoryCache::shouldMakeResourcePurgeableOnEviction())
        return false;

    if (!resource->inCache())
        return false;

    if (resource->isPurgeable())
        return true;

    if (!resource->isSafeToMakePurgeable())
        return false;

    if (!resource->makePurgeable(true))
        return false;

    adjustSize(resource->hasClients(), -static_cast<int>(resource->size()));

    return true;
}

void MemoryCache::evict(CachedResource* resource)
{
    ASSERT(WTF::isMainThread());
    LOG(ResourceLoading, "Evicting resource %p for '%s' from cache", resource, resource->url().string().latin1().data());
    // The resource may have already been removed by someone other than our caller,
    // who needed a fresh copy for a reload. See <http://bugs.webkit.org/show_bug.cgi?id=12479#c6>.
    CachedResourceMap& resources = getSessionMap(resource->sessionID());
    if (resource->inCache()) {
        // Remove from the resource map.
#if ENABLE(CACHE_PARTITIONING)
        CachedResourceItem* item = resources.get(resource->url());
        if (item) {
            item->remove(resource->cachePartition());
            if (!item->size())
                resources.remove(resource->url());
        }
#else
        resources.remove(resource->url());
#endif
        resource->setInCache(false);

        // Remove from the appropriate LRU list.
        removeFromLRUList(resource);
        removeFromLiveDecodedResourcesList(resource);

        // If the resource was purged, it means we had already decremented the size when we made the
        // resource purgeable in makeResourcePurgeable(). So adjust the size if we are evicting a
        // resource that was not marked as purgeable.
        if (!MemoryCache::shouldMakeResourcePurgeableOnEviction() || !resource->isPurgeable())
            adjustSize(resource->hasClients(), -static_cast<int>(resource->size()));
    } else
#if ENABLE(CACHE_PARTITIONING)
        ASSERT(!resources.get(resource->url()) || resources.get(resource->url())->get(resource->cachePartition()) != resource);
#else
        ASSERT(resources.get(resource->url()) != resource);
#endif

    resource->deleteIfPossible();
}

MemoryCache::LRUList* MemoryCache::lruListFor(CachedResource* resource)
{
    unsigned accessCount = std::max(resource->accessCount(), 1U);
    unsigned queueIndex = WTF::fastLog2(resource->size() / accessCount);
#ifndef NDEBUG
    resource->m_lruIndex = queueIndex;
#endif
    if (m_allResources.size() <= queueIndex)
        m_allResources.grow(queueIndex + 1);
    return &m_allResources[queueIndex];
}

void MemoryCache::removeFromLRUList(CachedResource* resource)
{
    // If we've never been accessed, then we're brand new and not in any list.
    if (resource->accessCount() == 0)
        return;

#if !ASSERT_DISABLED
    unsigned oldListIndex = resource->m_lruIndex;
#endif

    LRUList* list = lruListFor(resource);

#if !ASSERT_DISABLED
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

void MemoryCache::insertInLRUList(CachedResource* resource)
{
    // Make sure we aren't in some list already.
    ASSERT(!resource->m_nextInAllResourcesList && !resource->m_prevInAllResourcesList);
    ASSERT(resource->inCache());
    ASSERT(resource->accessCount() > 0);
    
    LRUList* list = lruListFor(resource);

    resource->m_nextInAllResourcesList = list->m_head;
    if (list->m_head)
        list->m_head->m_prevInAllResourcesList = resource;
    list->m_head = resource;
    
    if (!resource->m_nextInAllResourcesList)
        list->m_tail = resource;
        
#if !ASSERT_DISABLED
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

void MemoryCache::resourceAccessed(CachedResource* resource)
{
    ASSERT(resource->inCache());
    
    // Need to make sure to remove before we increase the access count, since
    // the queue will possibly change.
    removeFromLRUList(resource);
    
    // If this is the first time the resource has been accessed, adjust the size of the cache to account for its initial size.
    if (!resource->accessCount())
        adjustSize(resource->hasClients(), resource->size());
    
    // Add to our access count.
    resource->increaseAccessCount();
    
    // Now insert into the new queue.
    insertInLRUList(resource);
}

void MemoryCache::removeResourcesWithOrigin(SecurityOrigin* origin)
{
    Vector<CachedResource*> resourcesWithOrigin;

    for (auto& resources : m_sessionResources) {
        CachedResourceMap::iterator e = resources.value->end();
#if ENABLE(CACHE_PARTITIONING)
        String originPartition = ResourceRequest::partitionName(origin->host());
#endif

        for (CachedResourceMap::iterator it = resources.value->begin(); it != e; ++it) {
#if ENABLE(CACHE_PARTITIONING)
            for (CachedResourceItem::iterator itemIterator = it->value->begin(); itemIterator != it->value->end(); ++itemIterator) {
                CachedResource* resource = itemIterator->value;
                String partition = itemIterator->key;
                if (partition == originPartition) {
                    resourcesWithOrigin.append(resource);
                    continue;
                }
#else
                CachedResource* resource = it->value;
#endif
                RefPtr<SecurityOrigin> resourceOrigin = SecurityOrigin::createFromString(resource->url());
                if (!resourceOrigin)
                    continue;
                if (resourceOrigin->equal(origin))
                    resourcesWithOrigin.append(resource);
#if ENABLE(CACHE_PARTITIONING)
            }
#endif
        }
    }

    for (size_t i = 0; i < resourcesWithOrigin.size(); ++i)
        remove(resourcesWithOrigin[i]);
}

void MemoryCache::getOriginsWithCache(SecurityOriginSet& origins)
{
#if ENABLE(CACHE_PARTITIONING)
    DEFINE_STATIC_LOCAL(String, httpString, ("http"));
#endif
    for (auto& resources : m_sessionResources) {
        CachedResourceMap::iterator e = resources.value->end();
        for (CachedResourceMap::iterator it = resources.value->begin(); it != e; ++it) {
#if ENABLE(CACHE_PARTITIONING)
            if (it->value->begin()->key == emptyString())
                origins.add(SecurityOrigin::createFromString(it->value->begin()->value->url()));
            else
                origins.add(SecurityOrigin::create(httpString, it->value->begin()->key, 0));
#else
            origins.add(SecurityOrigin::createFromString(it->value->url()));
#endif
        }
    }
}

void MemoryCache::removeFromLiveDecodedResourcesList(CachedResource* resource)
{
    // If we've never been accessed, then we're brand new and not in any list.
    if (!resource->m_inLiveDecodedResourcesList)
        return;
    resource->m_inLiveDecodedResourcesList = false;

#if !ASSERT_DISABLED
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

void MemoryCache::insertInLiveDecodedResourcesList(CachedResource* resource)
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
        
#if !ASSERT_DISABLED
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

void MemoryCache::addToLiveResourcesSize(CachedResource* resource)
{
    m_liveSize += resource->size();
    m_deadSize -= resource->size();
}

void MemoryCache::removeFromLiveResourcesSize(CachedResource* resource)
{
    m_liveSize -= resource->size();
    m_deadSize += resource->size();
}

void MemoryCache::adjustSize(bool live, int delta)
{
    if (live) {
        ASSERT(delta >= 0 || ((int)m_liveSize + delta >= 0));
        m_liveSize += delta;
    } else {
        ASSERT(delta >= 0 || ((int)m_deadSize + delta >= 0));
        m_deadSize += delta;
    }
}

void MemoryCache::removeUrlFromCache(ScriptExecutionContext* context, const String& urlString, SessionID sessionID)
{
    removeRequestFromCache(context, ResourceRequest(urlString), sessionID);
}

void MemoryCache::removeRequestFromCache(ScriptExecutionContext* context, const ResourceRequest& request, SessionID sessionID)
{
    if (context->isWorkerGlobalScope()) {
        toWorkerGlobalScope(context)->thread().workerLoaderProxy().postTaskToLoader(createCallbackTask(&crossThreadRemoveRequestFromCache, request, sessionID));
        return;
    }

    removeRequestFromCacheImpl(context, request, sessionID);
}

void MemoryCache::removeRequestFromCacheImpl(ScriptExecutionContext*, const ResourceRequest& request, SessionID sessionID)
{
    if (CachedResource* resource = memoryCache()->resourceForRequest(request, sessionID))
        memoryCache()->remove(resource);
}

void MemoryCache::removeRequestFromSessionCaches(ScriptExecutionContext* context, const ResourceRequest& request)
{
    if (context->isWorkerGlobalScope()) {
        toWorkerGlobalScope(context)->thread().workerLoaderProxy().postTaskToLoader(createCallbackTask(&crossThreadRemoveRequestFromSessionCaches, request));
        return;
    }
    removeRequestFromSessionCachesImpl(context, request);
}

void MemoryCache::removeRequestFromSessionCachesImpl(ScriptExecutionContext*, const ResourceRequest& request)
{
    for (auto& resources : memoryCache()->m_sessionResources) {
        if (CachedResource* resource = memoryCache()->resourceForRequestImpl(request, *resources.value))
        memoryCache()->remove(resource);
    }
}

void MemoryCache::crossThreadRemoveRequestFromCache(ScriptExecutionContext* context, PassOwnPtr<WebCore::CrossThreadResourceRequestData> requestData, SessionID sessionID)
{
    OwnPtr<ResourceRequest> request(ResourceRequest::adopt(requestData));
    MemoryCache::removeRequestFromCacheImpl(context, *request, sessionID);
}

void MemoryCache::crossThreadRemoveRequestFromSessionCaches(ScriptExecutionContext* context, PassOwnPtr<WebCore::CrossThreadResourceRequestData> requestData)
{
    OwnPtr<ResourceRequest> request(ResourceRequest::adopt(requestData));
    MemoryCache::removeRequestFromSessionCaches(context, *request);
}

void MemoryCache::TypeStatistic::addResource(CachedResource* o)
{
    bool purged = o->wasPurged();
    bool purgeable = o->isPurgeable() && !purged; 
    int pageSize = (o->encodedSize() + o->overheadSize() + 4095) & ~4095;
    count++;
    size += purged ? 0 : o->size(); 
    liveSize += o->hasClients() ? o->size() : 0;
    decodedSize += o->decodedSize();
    purgeableSize += purgeable ? pageSize : 0;
    purgedSize += purged ? pageSize : 0;
#if ENABLE(DISK_IMAGE_CACHE)
    // Only the data inside the resource was mapped, not the entire resource.
    mappedSize += o->isUsingDiskImageCache() ? o->resourceBuffer()->sharedBuffer()->size() : 0;
#endif
}

MemoryCache::Statistics MemoryCache::getStatistics()
{
    Statistics stats;

    for (auto& resources : m_sessionResources) {
        CachedResourceMap::iterator e = resources.value->end();
        for (CachedResourceMap::iterator i = resources.value->begin(); i != e; ++i) {
#if ENABLE(CACHE_PARTITIONING)
            for (CachedResourceItem::iterator itemIterator = i->value->begin(); itemIterator != i->value->end(); ++itemIterator) {
                CachedResource* resource = itemIterator->value;
#else
                CachedResource* resource = i->value;
#endif
                switch (resource->type()) {
                case CachedResource::ImageResource:
                    stats.images.addResource(resource);
                    break;
                case CachedResource::CSSStyleSheet:
                    stats.cssStyleSheets.addResource(resource);
                    break;
                case CachedResource::Script:
                    stats.scripts.addResource(resource);
                    break;
#if ENABLE(XSLT)
                case CachedResource::XSLStyleSheet:
                    stats.xslStyleSheets.addResource(resource);
                    break;
#endif
                case CachedResource::FontResource:
                    stats.fonts.addResource(resource);
                    break;
                default:
                    break;
                }
#if ENABLE(CACHE_PARTITIONING)
            }
#endif
        }
    }
    return stats;
}

void MemoryCache::setDisabled(bool disabled)
{
    m_disabled = disabled;
    if (!m_disabled)
        return;

    for (;;) {
        SessionCachedResourceMap::iterator sessionIterator = m_sessionResources.begin();
        if (sessionIterator == m_sessionResources.end())
            break;
        CachedResourceMap::iterator outerIterator = sessionIterator->value->begin();
        if (outerIterator == sessionIterator->value->end())
            break;
#if ENABLE(CACHE_PARTITIONING)
        CachedResourceItem::iterator innerIterator = outerIterator->value->begin();
        evict(innerIterator->value);
#else
        evict(outerIterator->value);
#endif
    }
}

void MemoryCache::evictResources()
{
    if (disabled())
        return;

    setDisabled(true);
    setDisabled(false);
}

void MemoryCache::prune()
{
    if (m_liveSize + m_deadSize <= m_capacity && m_deadSize <= m_maxDeadCapacity) // Fast path.
        return;
        
    pruneDeadResources(); // Prune dead first, in case it was "borrowing" capacity from live.
    pruneLiveResources();
}

void MemoryCache::pruneToPercentage(float targetPercentLive)
{
    pruneDeadResourcesToPercentage(targetPercentLive); // Prune dead first, in case it was "borrowing" capacity from live.
    pruneLiveResourcesToPercentage(targetPercentLive);
}


#ifndef NDEBUG
void MemoryCache::dumpStats()
{
    Statistics s = getStatistics();
#if ENABLE(DISK_IMAGE_CACHE)
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s %-13s %-13s\n", "", "Count", "Size", "LiveSize", "DecodedSize", "PurgeableSize", "PurgedSize", "Mapped", "\"Real\"");
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s %-13s %-13s\n", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------");
    printf("%-13s %13d %13d %13d %13d %13d %13d %13d %13d\n", "Images", s.images.count, s.images.size, s.images.liveSize, s.images.decodedSize, s.images.purgeableSize, s.images.purgedSize, s.images.mappedSize, s.images.size - s.images.mappedSize);
#else
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s\n", "", "Count", "Size", "LiveSize", "DecodedSize", "PurgeableSize", "PurgedSize");
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s\n", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------");
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "Images", s.images.count, s.images.size, s.images.liveSize, s.images.decodedSize, s.images.purgeableSize, s.images.purgedSize);
#endif
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "CSS", s.cssStyleSheets.count, s.cssStyleSheets.size, s.cssStyleSheets.liveSize, s.cssStyleSheets.decodedSize, s.cssStyleSheets.purgeableSize, s.cssStyleSheets.purgedSize);
#if ENABLE(XSLT)
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "XSL", s.xslStyleSheets.count, s.xslStyleSheets.size, s.xslStyleSheets.liveSize, s.xslStyleSheets.decodedSize, s.xslStyleSheets.purgeableSize, s.xslStyleSheets.purgedSize);
#endif
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "JavaScript", s.scripts.count, s.scripts.size, s.scripts.liveSize, s.scripts.decodedSize, s.scripts.purgeableSize, s.scripts.purgedSize);
    printf("%-13s %13d %13d %13d %13d %13d %13d\n", "Fonts", s.fonts.count, s.fonts.size, s.fonts.liveSize, s.fonts.decodedSize, s.fonts.purgeableSize, s.fonts.purgedSize);
    printf("%-13s %-13s %-13s %-13s %-13s %-13s %-13s\n\n", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------", "-------------");
}

void MemoryCache::dumpLRULists(bool includeLive) const
{
#if ENABLE(DISK_IMAGE_CACHE)
    printf("LRU-SP lists in eviction order (Kilobytes decoded, Kilobytes encoded, Access count, Referenced, isPurgeable, wasPurged, isMemoryMapped):\n");
#else
    printf("LRU-SP lists in eviction order (Kilobytes decoded, Kilobytes encoded, Access count, Referenced, isPurgeable, wasPurged):\n");
#endif

    int size = m_allResources.size();
    for (int i = size - 1; i >= 0; i--) {
        printf("\n\nList %d: ", i);
        CachedResource* current = m_allResources[i].m_tail;
        while (current) {
            CachedResource* prev = current->m_prevInAllResourcesList;
            if (includeLive || !current->hasClients())
#if ENABLE(DISK_IMAGE_CACHE)
                printf("(%.1fK, %.1fK, %uA, %dR, %d, %d, %d); ", current->decodedSize() / 1024.0f, (current->encodedSize() + current->overheadSize()) / 1024.0f, current->accessCount(), current->hasClients(), current->isPurgeable(), current->wasPurged(), current->isUsingDiskImageCache());
#else
                printf("(%.1fK, %.1fK, %uA, %dR, %d, %d); ", current->decodedSize() / 1024.0f, (current->encodedSize() + current->overheadSize()) / 1024.0f, current->accessCount(), current->hasClients(), current->isPurgeable(), current->wasPurged());
#endif

            current = prev;
        }
    }
}
#endif

} // namespace WebCore
