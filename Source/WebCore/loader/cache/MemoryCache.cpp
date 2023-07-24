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
#include "ClientOrigin.h"
#include "Document.h"
#include "FrameLoader.h"
#include "FrameLoaderTypes.h"
#include "Image.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "PublicSuffix.h"
#include "SharedBuffer.h"
#include "WorkerGlobalScope.h"
#include "WorkerLoaderProxy.h"
#include "WorkerThread.h"
#include <pal/Logging.h>
#include <stdio.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/text/CString.h>

namespace WebCore {

static const int cDefaultCacheCapacity = 8192 * 1024;
static const Seconds cMinDelayBeforeLiveDecodedPrune { 1_s };
static const float cTargetPrunePercentage = .95f; // Percentage of capacity toward which we prune, to avoid immediately pruning again.

MemoryCache& MemoryCache::singleton()
{
    RELEASE_ASSERT(isMainThread());
    static NeverDestroyed<MemoryCache> memoryCache;
    return memoryCache;
}

MemoryCache::MemoryCache()
    : m_capacity(cDefaultCacheCapacity)
    , m_maxDeadCapacity(cDefaultCacheCapacity)
    , m_pruneTimer(*this, &MemoryCache::prune)
{
    static_assert(sizeof(long long) > sizeof(unsigned), "Numerical overflow can happen when adjusting the size of the cached memory.");

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PAL::registerNotifyCallback("com.apple.WebKit.showMemoryCache"_s, [] {
            MemoryCache::singleton().dumpStats();
            MemoryCache::singleton().dumpLRULists(true);
        });
    });
}

auto MemoryCache::sessionResourceMap(PAL::SessionID sessionID) const -> CachedResourceMap*
{
    RELEASE_ASSERT(sessionID.isValid());
    RELEASE_ASSERT(isMainThread());
    RELEASE_ASSERT(m_sessionResources.isValidKey(sessionID));
    return m_sessionResources.get(sessionID);
}

auto MemoryCache::ensureSessionResourceMap(PAL::SessionID sessionID) -> CachedResourceMap&
{
    RELEASE_ASSERT(sessionID.isValid());
    RELEASE_ASSERT(isMainThread());
    RELEASE_ASSERT(m_sessionResources.isValidKey(sessionID));
    auto& map = m_sessionResources.add(sessionID, nullptr).iterator->value;
    if (!map)
        map = makeUnique<CachedResourceMap>();
    return *map;
}

bool MemoryCache::shouldRemoveFragmentIdentifier(const URL& originalURL)
{
    if (!originalURL.hasFragmentIdentifier())
        return false;
    // Strip away fragment identifier from HTTP URLs.
    // Data URLs must be unmodified. For file and custom URLs clients may expect resources
    // to be unique even when they differ by the fragment identifier only.
    return originalURL.protocolIsInHTTPFamily();
}

URL MemoryCache::removeFragmentIdentifierIfNeeded(const URL& originalURL)
{
    if (!shouldRemoveFragmentIdentifier(originalURL))
        return originalURL;
    URL url = originalURL;
    url.removeFragmentIdentifier();
    return url;
}

bool MemoryCache::add(CachedResource& resource)
{
    RELEASE_ASSERT(isMainThread());

    if (disabled())
        return false;

    if (resource.resourceRequest().httpMethod() != "GET"_s)
        return false;

    ASSERT(isMainThread());

    auto key = std::make_pair(resource.url(), resource.cachePartition());

    auto& resources = ensureSessionResourceMap(resource.sessionID());

    RELEASE_ASSERT(!resources.get(key));
    resources.set(key, &resource);
    resource.setInCache(true);
    
    resourceAccessed(resource);

    LOG(ResourceLoading, "MemoryCache::add Added '%.255s', resource %p\n", resource.url().string().latin1().data(), &resource);
    return true;
}

void MemoryCache::revalidationSucceeded(CachedResource& revalidatingResource, const ResourceResponse& response)
{
    RELEASE_ASSERT(isMainThread());
    ASSERT(response.source() == ResourceResponse::Source::MemoryCacheAfterValidation);
    ASSERT(revalidatingResource.resourceToRevalidate());
    CachedResourceHandle protectedRevalidatingResource { revalidatingResource };
    auto& resource = *revalidatingResource.resourceToRevalidate();
    ASSERT(!resource.inCache());
    ASSERT(resource.isLoaded());

    // Calling remove() can potentially delete revalidatingResource, which we use
    // below. This mustn't be the case since revalidation means it is loaded
    // and so canDelete() is false.
    ASSERT(!revalidatingResource.canDelete());

    remove(revalidatingResource);

    // A resource with the same URL may have been added back in the cache during revalidation.
    // In this case, we remove the cached resource and replace it with our freshly revalidated
    // one.
    std::pair key { resource.url(), resource.cachePartition() };
    if (auto* existingResources = sessionResourceMap(resource.sessionID())) {
        if (auto existingResource = existingResources->get(key))
            remove(*existingResource);
    }

    // Don't move the call to ensureSessionResourceMap() in this function as the calls to
    // remove() above could invalidate the reference returned by ensureSessionResourceMap().
    auto& resources = ensureSessionResourceMap(resource.sessionID());
    ASSERT(!resources.contains(key));
    resources.add(key, &resource);
    resource.setInCache(true);
    resource.updateResponseAfterRevalidation(response);
    insertInLRUList(resource);
    long long delta = resource.size();
    if (resource.decodedSize() && resource.hasClients())
        insertInLiveDecodedResourcesList(resource);
    if (delta)
        adjustSize(resource.hasClients(), delta);

    revalidatingResource.switchClientsToRevalidatedResource();
    ASSERT(!revalidatingResource.m_deleted);
    // This deletes the revalidating resource.
    revalidatingResource.clearResourceToRevalidate();
}

void MemoryCache::revalidationFailed(CachedResource& revalidatingResource)
{
    RELEASE_ASSERT(isMainThread());
    LOG(ResourceLoading, "Revalidation failed for %p", &revalidatingResource);
    ASSERT(revalidatingResource.resourceToRevalidate());
    revalidatingResource.clearResourceToRevalidate();
}

CachedResource* MemoryCache::resourceForRequest(const ResourceRequest& request, PAL::SessionID sessionID)
{
    RELEASE_ASSERT(isMainThread());
    // FIXME: Change all clients to make sure HTTP(s) URLs have no fragment identifiers before calling here.
    // CachedResourceLoader is now doing this. Add an assertion once all other clients are doing it too.
    auto* resources = sessionResourceMap(sessionID);
    if (!resources)
        return nullptr;
    return resourceForRequestImpl(request, *resources);
}

CachedResource* MemoryCache::resourceForRequestImpl(const ResourceRequest& request, CachedResourceMap& resources)
{
    ASSERT(isMainThread());
    URL url = removeFragmentIdentifierIfNeeded(request.url());

    auto key = std::make_pair(url, request.cachePartition());
    return resources.get(key).get();
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

void MemoryCache::pruneLiveResources(bool shouldDestroyDecodedDataForAllLiveResources)
{
    RELEASE_ASSERT(isMainThread());
    unsigned capacity = shouldDestroyDecodedDataForAllLiveResources ? 0 : liveCapacity();
    if (capacity && m_liveSize <= capacity)
        return;

    unsigned targetSize = static_cast<unsigned>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.

    pruneLiveResourcesToSize(targetSize, shouldDestroyDecodedDataForAllLiveResources);
}

void MemoryCache::forEachResource(const Function<void(CachedResource&)>& function)
{
    RELEASE_ASSERT(isMainThread());
    Vector<WeakPtr<CachedResource>> allResources;
    for (auto& lruList : m_allResources) {
        allResources.reserveCapacity(allResources.size() + lruList->computeSize());
        for (auto& resource : *lruList)
            allResources.uncheckedAppend(resource);
    }
    for (auto& resource : allResources) {
        if (CachedResourceHandle resourceHandle = resource.get())
            function(*resourceHandle);
    }
}

void MemoryCache::forEachSessionResource(PAL::SessionID sessionID, const Function<void(CachedResource&)>& function)
{
    RELEASE_ASSERT(isMainThread());
    RELEASE_ASSERT(m_sessionResources.isValidKey(sessionID));
    auto it = m_sessionResources.find(sessionID);
    if (it == m_sessionResources.end())
        return;

    for (auto& resource : copyToVector(it->value->values())) {
        if (resource)
            function(*resource);
    }
}

void MemoryCache::destroyDecodedDataForAllImages()
{
    RELEASE_ASSERT(isMainThread());
    forEachResource([](CachedResource& resource) {
        if (auto cachedImage = dynamicDowncast<CachedImage>(resource)) {
            if (RefPtr image = cachedImage->image())
                image->destroyDecodedData();
        }
    });
}

void MemoryCache::pruneLiveResourcesToSize(unsigned targetSize, bool shouldDestroyDecodedDataForAllLiveResources)
{
    RELEASE_ASSERT(isMainThread());
    if (m_inPruneResources)
        return;

    LOG(ResourceLoading, "MemoryCache::pruneLiveResourcesToSize(%d, shouldDestroyDecodedDataForAllLiveResources = %d)", targetSize, shouldDestroyDecodedDataForAllLiveResources);

    SetForScope reentrancyProtector(m_inPruneResources, true);

    MonotonicTime currentTime = LocalFrameView::currentPaintTimeStamp();
    if (!currentTime) // In case prune is called directly, outside of a Frame paint.
        currentTime = MonotonicTime::now();
    
    // Destroy any decoded data in live objects that we can.
    // Start from the head, since this is the least recently accessed of the objects.

    // The list might not be sorted by the m_lastDecodedAccessTime. The impact
    // of this weaker invariant is minor as the below if statement to check the
    // elapsedTime will evaluate to false as the currentTime will be a lot
    // greater than the current->m_lastDecodedAccessTime.
    // For more details see: https://bugs.webkit.org/show_bug.cgi?id=30209
    auto it = m_liveDecodedResources.begin();
    while (it != m_liveDecodedResources.end()) {
        auto& current = *it;

        LOG(ResourceLoading, " live resource %p %.255s - loaded %d, decodedSize %u", &current, current.url().string().utf8().data(), current.isLoaded(), current.decodedSize());

        // Increment the iterator now because the call to destroyDecodedData() below
        // may cause a call to ListHashSet::remove() and invalidate the current
        // iterator. Note that this is safe because unlike iteration of most
        // WTF Hash data structures, iteration is guaranteed safe against mutation
        // of the ListHashSet, except for removal of the item currently pointed to
        // by a given iterator.
        ++it;

        ASSERT(current.hasClients());
        if (current.isLoaded() && current.decodedSize()) {
            // Check to see if the remaining resources are too new to prune.
            Seconds elapsedTime = currentTime - current.m_lastDecodedAccessTime;
            if (!shouldDestroyDecodedDataForAllLiveResources && elapsedTime < cMinDelayBeforeLiveDecodedPrune) {
                LOG(ResourceLoading, " current time is less than min delay before pruning (%.3fms)", elapsedTime.milliseconds());
                return;
            }

            // Destroy our decoded data. This will remove us from m_liveDecodedResources, and possibly move us
            // to a different LRU list in m_allResources.
            current.destroyDecodedData();

            if (targetSize && m_liveSize <= targetSize)
                return;
        }
    }
}

void MemoryCache::pruneDeadResources()
{
    LOG(ResourceLoading, "MemoryCache::pruneDeadResources");
    RELEASE_ASSERT(isMainThread());

    unsigned capacity = deadCapacity();
    if (capacity && m_deadSize <= capacity)
        return;

    unsigned targetSize = static_cast<unsigned>(capacity * cTargetPrunePercentage); // Cut by a percentage to avoid immediately pruning again.
    pruneDeadResourcesToSize(targetSize);
}

void MemoryCache::pruneDeadResourcesToSize(unsigned targetSize)
{
    RELEASE_ASSERT(isMainThread());
    if (m_inPruneResources)
        return;

    LOG(ResourceLoading, "MemoryCache::pruneDeadResourcesToSize(%d)", targetSize);

    SetForScope reentrancyProtector(m_inPruneResources, true);
 
    if (targetSize && m_deadSize <= targetSize)
        return;

    bool canShrinkLRULists = true;
    for (int i = m_allResources.size() - 1; i >= 0; i--) {
        // Make a copy of the LRUList first (and ref the resources) as calling
        // destroyDecodedData() can alter the LRUList.
        auto lruList = copyToVector(*m_allResources[i]);

        LOG(ResourceLoading, " lru list (size %lu) - flushing stage", lruList.size());

        // First flush all the decoded data in this queue.
        // Remove from the head, since this is the least frequently accessed of the objects.
        for (auto& resource : lruList) {
            if (!resource)
                continue;

            LOG(ResourceLoading, " lru resource %p - in cache %d, has clients %d, preloaded %d, loaded %d", resource.get(), resource->inCache(), resource->hasClients(), resource->isPreloaded(), resource->isLoaded());
            if (!resource->inCache())
                continue;

            if (!resource->hasClients() && !resource->isPreloaded() && resource->isLoaded()) {
                // Destroy our decoded data. This will remove us from 
                // m_liveDecodedResources, and possibly move us to a different 
                // LRU list in m_allResources.

                LOG(ResourceLoading, " lru resource %p destroyDecodedData", resource.get());

                resource->destroyDecodedData();

                if (targetSize && m_deadSize <= targetSize)
                    return;
            }
        }

        LOG(ResourceLoading, " lru list (size %lu) - eviction stage", lruList.size());

        // Now evict objects from this list.
        // Remove from the head, since this is the least frequently accessed of the objects.
        for (auto& resource : lruList) {
            if (!resource)
                continue;

            LOG(ResourceLoading, " lru resource %p - in cache %d, has clients %d, preloaded %d, loaded %d", resource.get(), resource->inCache(), resource->hasClients(), resource->isPreloaded(), resource->isLoaded());
            if (!resource->inCache())
                continue;

            if (!resource->hasClients() && !resource->isPreloaded() && !resource->isCacheValidator()) {
                remove(*resource);
                if (targetSize && m_deadSize <= targetSize)
                    return;
            }
        }
            
        // Shrink the vector back down so we don't waste time inspecting
        // empty LRU lists on future prunes.
        if (!m_allResources[i]->isEmptyIgnoringNullReferences())
            canShrinkLRULists = false;
        else if (canShrinkLRULists)
            m_allResources.shrink(i);
    }
}

void MemoryCache::setCapacities(unsigned minDeadBytes, unsigned maxDeadBytes, unsigned totalBytes)
{
    ASSERT(minDeadBytes <= maxDeadBytes);
    ASSERT(maxDeadBytes <= totalBytes);
    m_minDeadCapacity = minDeadBytes;
    m_maxDeadCapacity = maxDeadBytes;
    m_capacity = totalBytes;
    prune();
}

void MemoryCache::remove(CachedResource& resource)
{
    RELEASE_ASSERT(isMainThread());
    CachedResourceHandle protectedResource { resource };

    LOG(ResourceLoading, "Evicting resource %p for '%.255s' from cache", &resource, resource.url().string().latin1().data());
    // The resource may have already been removed by someone other than our caller,
    // who needed a fresh copy for a reload. See <http://bugs.webkit.org/show_bug.cgi?id=12479#c6>.
    if (auto* resources = sessionResourceMap(resource.sessionID())) {
        auto key = std::make_pair(resource.url(), resource.cachePartition());

        if (resource.inCache()) {
            ASSERT_WITH_MESSAGE(resource.response().source() != ResourceResponse::Source::InspectorOverride, "InspectorOverride responses should not get into the MemoryCache");

            // Remove resource from the resource map.
            resources->remove(key);
            resource.setInCache(false);

            // If the resource map is now empty, remove it from m_sessionResources.
            if (resources->isEmpty()) {
                RELEASE_ASSERT(m_sessionResources.isValidKey(resource.sessionID()));
                m_sessionResources.remove(resource.sessionID());
            }

            // Remove from the appropriate LRU list.
            removeFromLRUList(resource);
            removeFromLiveDecodedResourcesList(resource);
            adjustSize(resource.hasClients(), -static_cast<long long>(resource.size()));
        } else {
            RELEASE_ASSERT(resources->get(key) != &resource);
            LOG(ResourceLoading, "  resource %p is not in cache", &resource);
        }
    }
    RELEASE_ASSERT(!resource.inCache());
}

auto MemoryCache::lruListFor(CachedResource& resource) -> LRUList&
{
    RELEASE_ASSERT(isMainThread());
    unsigned accessCount = std::max(resource.accessCount(), 1U);
    unsigned queueIndex = WTF::fastLog2(resource.size() / accessCount);
#if ASSERT_ENABLED
    resource.m_lruIndex = queueIndex;
#endif

    m_allResources.reserveCapacity(queueIndex + 1);
    while (m_allResources.size() <= queueIndex)
        m_allResources.uncheckedAppend(makeUnique<LRUList>());
    return *m_allResources[queueIndex];
}

void MemoryCache::removeFromLRUList(CachedResource& resource)
{
    RELEASE_ASSERT(isMainThread());
    // If we've never been accessed, then we're brand new and not in any list.
    if (!resource.accessCount())
        return;

#if ASSERT_ENABLED
    unsigned oldListIndex = resource.m_lruIndex;
#endif

    LRUList& list = lruListFor(resource);

    // Verify that the list we got is the list we want.
    ASSERT(resource.m_lruIndex == oldListIndex);

    bool removed = list.remove(resource);
    ASSERT_UNUSED(removed, removed);
}

void MemoryCache::insertInLRUList(CachedResource& resource)
{
    RELEASE_ASSERT(isMainThread());
    ASSERT(resource.inCache());
    ASSERT(resource.accessCount() > 0);
    
    auto addResult = lruListFor(resource).add(resource);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void MemoryCache::resourceAccessed(CachedResource& resource)
{
    RELEASE_ASSERT(isMainThread());
    ASSERT(resource.inCache());
    
    // Need to make sure to remove before we increase the access count, since
    // the queue will possibly change.
    removeFromLRUList(resource);
    
    // If this is the first time the resource has been accessed, adjust the size of the cache to account for its initial size.
    if (!resource.accessCount())
        adjustSize(resource.hasClients(), resource.size());
    
    // Add to our access count.
    resource.increaseAccessCount();
    
    // Now insert into the new queue.
    insertInLRUList(resource);
}

bool MemoryCache::inLiveDecodedResourcesList(CachedResource& resource) const
{
    RELEASE_ASSERT(isMainThread());
    return m_liveDecodedResources.contains(resource);
}

void MemoryCache::removeResourcesWithOrigin(const SecurityOrigin& origin, const String& cachePartition)
{
    RELEASE_ASSERT(isMainThread());
    Vector<WeakPtr<CachedResource>> resourcesWithOrigin;
    for (auto& resources : m_sessionResources.values()) {
        for (auto& keyValue : *resources) {
            auto& resource = *keyValue.value;
            auto& partitionName = keyValue.key.second;
            if (partitionName == cachePartition) {
                resourcesWithOrigin.append(resource);
                continue;
            }
            auto resourceOrigin = SecurityOrigin::create(resource.url());
            if (resourceOrigin->equal(&origin))
                resourcesWithOrigin.append(resource);
        }
    }

    for (auto& resource : resourcesWithOrigin) {
        if (resource)
            remove(*resource);
    }
}

void MemoryCache::removeResourcesWithOrigin(const SecurityOrigin& origin)
{
    RELEASE_ASSERT(isMainThread());
    String originPartition = ResourceRequest::partitionName(origin.host());
    removeResourcesWithOrigin(origin, originPartition);
}

void MemoryCache::removeResourcesWithOrigin(const ClientOrigin& origin)
{
    RELEASE_ASSERT(isMainThread());
    auto cachePartition = origin.topOrigin == origin.clientOrigin ? emptyString() : ResourceRequest::partitionName(origin.topOrigin.host());
    removeResourcesWithOrigin(origin.clientOrigin.securityOrigin(), cachePartition);
}

void MemoryCache::removeResourcesWithOrigins(PAL::SessionID sessionID, const HashSet<RefPtr<SecurityOrigin>>& origins)
{
    RELEASE_ASSERT(isMainThread());
    auto* resourceMap = sessionResourceMap(sessionID);
    if (!resourceMap)
        return;

    HashSet<String> originPartitions;

    for (auto& origin : origins)
        originPartitions.add(ResourceRequest::partitionName(origin->host()));

    Vector<WeakPtr<CachedResource>> resourcesToRemove;
    for (auto& keyValuePair : *resourceMap) {
        auto& resource = *keyValuePair.value;
        auto& partitionName = keyValuePair.key.second;
        if (originPartitions.contains(partitionName)) {
            resourcesToRemove.append(resource);
            continue;
        }
        if (origins.contains(SecurityOrigin::create(resource.url()).ptr()))
            resourcesToRemove.append(resource);
    }

    for (auto& resource : resourcesToRemove) {
        if (resource)
            remove(*resource);
    }
}

void MemoryCache::getOriginsWithCache(SecurityOriginSet& origins)
{
    RELEASE_ASSERT(isMainThread());
    for (auto& resources : m_sessionResources.values()) {
        for (auto& keyValue : *resources) {
            auto& resource = *keyValue.value;
            auto& partitionName = keyValue.key.second;
            if (!partitionName.isEmpty())
                origins.add(SecurityOrigin::create("http"_s, partitionName, 0));
            else
                origins.add(SecurityOrigin::create(resource.url()));
        }
    }
}

HashSet<RefPtr<SecurityOrigin>> MemoryCache::originsWithCache(PAL::SessionID sessionID) const
{
    RELEASE_ASSERT(isMainThread());

    HashSet<RefPtr<SecurityOrigin>> origins;

    auto it = m_sessionResources.find(sessionID);
    if (it != m_sessionResources.end()) {
        for (auto& keyValue : *it->value) {
            auto& resource = *keyValue.value;
            auto& partitionName = keyValue.key.second;
            if (!partitionName.isEmpty())
                origins.add(SecurityOrigin::create("http"_s, partitionName, 0));
            else
                origins.add(SecurityOrigin::create(resource.url()));
        }
    }

    return origins;
}

void MemoryCache::removeFromLiveDecodedResourcesList(CachedResource& resource)
{
    RELEASE_ASSERT(isMainThread());
    m_liveDecodedResources.remove(resource);
}

void MemoryCache::insertInLiveDecodedResourcesList(CachedResource& resource)
{
    RELEASE_ASSERT(isMainThread());
    // Make sure we aren't in the list already.
    ASSERT(!m_liveDecodedResources.contains(resource));
    m_liveDecodedResources.add(resource);
}

void MemoryCache::addToLiveResourcesSize(CachedResource& resource)
{
    RELEASE_ASSERT(isMainThread());
    m_liveSize += resource.size();
    m_deadSize -= resource.size();
}

void MemoryCache::removeFromLiveResourcesSize(CachedResource& resource)
{
    RELEASE_ASSERT(isMainThread());
    m_liveSize -= resource.size();
    m_deadSize += resource.size();
}

void MemoryCache::adjustSize(bool live, long long delta)
{
    RELEASE_ASSERT(isMainThread());
    if (live) {
        ASSERT(delta >= 0 || (static_cast<long long>(m_liveSize) + delta >= 0));
        m_liveSize += delta;
    } else {
        ASSERT(delta >= 0 || (static_cast<long long>(m_deadSize) + delta >= 0));
        m_deadSize += delta;
    }
}

void MemoryCache::removeRequestFromSessionCaches(ScriptExecutionContext& context, const ResourceRequest& request)
{
    if (is<WorkerGlobalScope>(context)) {
        downcast<WorkerGlobalScope>(context).thread().workerLoaderProxy().postTaskToLoader([request = request.isolatedCopy()] (ScriptExecutionContext& context) {
            MemoryCache::removeRequestFromSessionCaches(context, request);
        });
        return;
    }

    auto& memoryCache = MemoryCache::singleton();
    for (auto& resources : memoryCache.m_sessionResources) {
        if (CachedResource* resource = memoryCache.resourceForRequestImpl(request, *resources.value))
            memoryCache.remove(*resource);
    }
}

void MemoryCache::TypeStatistic::addResource(CachedResource& resource)
{
    count++;
    size += resource.size();
    liveSize += resource.hasClients() ? resource.size() : 0;
    decodedSize += resource.decodedSize();
}

MemoryCache::Statistics MemoryCache::getStatistics()
{
    Statistics stats;

    for (auto& resources : m_sessionResources.values()) {
        for (auto& resource : resources->values()) {
            switch (resource->type()) {
            case CachedResource::Type::ImageResource:
                stats.images.addResource(*resource);
                break;
            case CachedResource::Type::CSSStyleSheet:
                stats.cssStyleSheets.addResource(*resource);
                break;
            case CachedResource::Type::Script:
                stats.scripts.addResource(*resource);
                break;
#if ENABLE(XSLT)
            case CachedResource::Type::XSLStyleSheet:
                stats.xslStyleSheets.addResource(*resource);
                break;
#endif
            case CachedResource::Type::SVGFontResource:
            case CachedResource::Type::FontResource:
                stats.fonts.addResource(*resource);
                break;
            default:
                break;
            }
        }
    }
    return stats;
}

void MemoryCache::setDisabled(bool disabled)
{
    RELEASE_ASSERT(isMainThread());
    m_disabled = disabled;
    if (!m_disabled)
        return;

    while (!m_sessionResources.isEmpty()) {
        auto& resources = *m_sessionResources.begin()->value;
        ASSERT(!resources.isEmpty());
        remove(*resources.begin()->value);
    }
}

void MemoryCache::evictResources()
{
    RELEASE_ASSERT(isMainThread());
    if (disabled())
        return;

    setDisabled(true);
    setDisabled(false);
}

void MemoryCache::evictResources(PAL::SessionID sessionID)
{
    RELEASE_ASSERT(isMainThread());
    if (disabled())
        return;

    forEachSessionResource(sessionID, [this] (CachedResource& resource) { remove(resource); });

    ASSERT(!m_sessionResources.contains(sessionID));
}

bool MemoryCache::needsPruning() const
{
    return m_liveSize + m_deadSize > m_capacity || m_deadSize > m_maxDeadCapacity;
}

void MemoryCache::prune()
{
    RELEASE_ASSERT(isMainThread());
    if (!needsPruning())
        return;
        
    pruneDeadResources(); // Prune dead first, in case it was "borrowing" capacity from live.
    pruneLiveResources();
}

void MemoryCache::pruneSoon()
{
    RELEASE_ASSERT(isMainThread());
    if (m_pruneTimer.isActive())
        return;
    if (!needsPruning())
        return;
    m_pruneTimer.startOneShot(0_s);
}

void MemoryCache::dumpStats()
{
    Statistics s = getStatistics();
    WTFLogAlways("\nMemory Cache");
    WTFLogAlways("%-13s %-13s %-13s %-13s %-13s\n", "", "Count", "Size", "LiveSize", "DecodedSize");
    WTFLogAlways("%-13s %-13s %-13s %-13s %-13s\n", "-------------", "-------------", "-------------", "-------------", "-------------");
    WTFLogAlways("%-13s %13d %13d %13d %13d\n", "Images", s.images.count, s.images.size, s.images.liveSize, s.images.decodedSize);
    WTFLogAlways("%-13s %13d %13d %13d %13d\n", "CSS", s.cssStyleSheets.count, s.cssStyleSheets.size, s.cssStyleSheets.liveSize, s.cssStyleSheets.decodedSize);
#if ENABLE(XSLT)
    WTFLogAlways("%-13s %13d %13d %13d %13d\n", "XSL", s.xslStyleSheets.count, s.xslStyleSheets.size, s.xslStyleSheets.liveSize, s.xslStyleSheets.decodedSize);
#endif
    WTFLogAlways("%-13s %13d %13d %13d %13d\n", "JavaScript", s.scripts.count, s.scripts.size, s.scripts.liveSize, s.scripts.decodedSize);
    WTFLogAlways("%-13s %13d %13d %13d %13d\n", "Fonts", s.fonts.count, s.fonts.size, s.fonts.liveSize, s.fonts.decodedSize);
    WTFLogAlways("%-13s %-13s %-13s %-13s %-13s\n\n", "-------------", "-------------", "-------------", "-------------", "-------------");

    unsigned countTotal = s.images.count + s.cssStyleSheets.count + s.scripts.count + s.fonts.count;
    unsigned sizeTotal = s.images.size + s.cssStyleSheets.size + s.scripts.size + s.fonts.size;
    unsigned liveSizeTotal = s.images.liveSize + s.cssStyleSheets.liveSize + s.scripts.liveSize + s.fonts.liveSize;
    unsigned decodedSizeTotal = s.images.decodedSize + s.cssStyleSheets.decodedSize + s.scripts.decodedSize + s.fonts.decodedSize;
#if ENABLE(XSLT)
    countTotal += s.xslStyleSheets.count;
    sizeTotal += s.xslStyleSheets.size;
    liveSizeTotal += s.xslStyleSheets.liveSize;
    decodedSizeTotal += s.xslStyleSheets.decodedSize;
#endif

    WTFLogAlways("%-13s %13d %11.2fKB %11.2fKB %11.2fKB\n", "Total", countTotal, sizeTotal / 1024., liveSizeTotal / 1024., decodedSizeTotal / 1024.);
}

void MemoryCache::dumpLRULists(bool includeLive) const
{
    WTFLogAlways("LRU-SP lists in eviction order (Kilobytes decoded, Kilobytes encoded, Access count, Referenced):\n");

    int size = m_allResources.size();
    for (int i = size - 1; i >= 0; i--) {
        WTFLogAlways("\nList %d:\n", i);
        for (auto& resource : *m_allResources[i]) {
            if (includeLive || !resource.hasClients())
                WTFLogAlways("  %p %.255s %.1fK, %.1fK, accesses: %u, clients: %d\n", &resource, resource.url().string().utf8().data(), resource.decodedSize() / 1024.0f, (resource.encodedSize() + resource.overheadSize()) / 1024.0f, resource.accessCount(), resource.numberOfClients());
        }
    }
}

} // namespace WebCore
