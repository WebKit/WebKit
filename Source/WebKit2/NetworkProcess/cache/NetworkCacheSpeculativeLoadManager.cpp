/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
#include "NetworkCacheSpeculativeLoadManager.h"

#include "Logging.h"
#include "NetworkCacheEntry.h"
#include "NetworkCacheSpeculativeLoad.h"
#include "NetworkCacheSubresourcesEntry.h"
#include <WebCore/HysteresisActivity.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

namespace WebKit {

namespace NetworkCache {

using namespace WebCore;

static const auto preloadedEntryLifetime = 10_s;

static const AtomicString& subresourcesType()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<const AtomicString> resource("subresources", AtomicString::ConstructFromLiteral);
    return resource;
}

static inline Key makeSubresourcesKey(const Key& resourceKey)
{
    return Key(resourceKey.partition(), subresourcesType(), resourceKey.range(), resourceKey.identifier());
}

static inline ResourceRequest constructRevalidationRequest(const Entry& entry)
{
    ResourceRequest revalidationRequest(entry.key().identifier());

    String eTag = entry.response().httpHeaderField(HTTPHeaderName::ETag);
    if (!eTag.isEmpty())
        revalidationRequest.setHTTPHeaderField(HTTPHeaderName::IfNoneMatch, eTag);

    String lastModified = entry.response().httpHeaderField(HTTPHeaderName::LastModified);
    if (!lastModified.isEmpty())
        revalidationRequest.setHTTPHeaderField(HTTPHeaderName::IfModifiedSince, lastModified);

    return revalidationRequest;
}

static bool responseNeedsRevalidation(const ResourceResponse& response, std::chrono::system_clock::time_point timestamp)
{
    if (response.cacheControlContainsNoCache())
        return true;

    auto age = computeCurrentAge(response, timestamp);
    auto lifetime = computeFreshnessLifetimeForHTTPFamily(response, timestamp);
    return age - lifetime > 0_ms;
}

class SpeculativeLoadManager::PreloadedEntry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PreloadedEntry(std::unique_ptr<Entry> entry, std::function<void()>&& lifetimeReachedHandler)
        : m_entry(WTF::move(entry))
        , m_lifetimeTimer(*this, &PreloadedEntry::lifetimeTimerFired)
        , m_lifetimeReachedHandler(WTF::move(lifetimeReachedHandler))
    {
        m_lifetimeTimer.startOneShot(preloadedEntryLifetime);
    }

    std::unique_ptr<Entry> takeCacheEntry()
    {
        ASSERT(m_entry);
        return WTF::move(m_entry);
    }

private:
    void lifetimeTimerFired()
    {
        m_lifetimeReachedHandler();
    }

    std::unique_ptr<Entry> m_entry;
    Timer m_lifetimeTimer;
    std::function<void()> m_lifetimeReachedHandler;
};

class SpeculativeLoadManager::PendingFrameLoad {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PendingFrameLoad(const Key& mainResourceKey, std::function<void()>&& completionHandler)
        : m_mainResourceKey(mainResourceKey)
        , m_completionHandler(WTF::move(completionHandler))
        , m_loadHysteresisActivity([this](HysteresisState state) { if (state == HysteresisState::Stopped) m_completionHandler(); })
    { }

    void registerSubresource(const Key& subresourceKey)
    {
        ASSERT(RunLoop::isMain());
        m_subresourceKeys.add(subresourceKey);
        m_loadHysteresisActivity.impulse();
    }

    Optional<Storage::Record> encodeAsSubresourcesRecord()
    {
        ASSERT(RunLoop::isMain());
        if (m_subresourceKeys.isEmpty())
            return { };

        auto subresourcesStorageKey = makeSubresourcesKey(m_mainResourceKey);
        Vector<Key> subresourceKeys;
        copyToVector(m_subresourceKeys, subresourceKeys);

#if !LOG_DISABLED
        LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Saving to disk list of subresources for '%s':", m_mainResourceKey.identifier().utf8().data());
        for (auto& subresourceKey : subresourceKeys)
            LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) * Subresource: '%s'.", subresourceKey.identifier().utf8().data());
#endif

        return SubresourcesEntry(WTF::move(subresourcesStorageKey), WTF::move(subresourceKeys)).encodeAsStorageRecord();
    }

    void markAsCompleted()
    {
        ASSERT(RunLoop::isMain());
        m_completionHandler();
    }

private:
    Key m_mainResourceKey;
    HashSet<Key> m_subresourceKeys;
    std::function<void()> m_completionHandler;
    HysteresisActivity m_loadHysteresisActivity;
};

SpeculativeLoadManager::SpeculativeLoadManager(Storage& storage)
    : m_storage(storage)
{
}

SpeculativeLoadManager::~SpeculativeLoadManager()
{
}

bool SpeculativeLoadManager::retrieve(const Key& storageKey, const RetrieveCompletionHandler& completionHandler)
{
    // Check already preloaded entries.
    if (auto preloadedEntry = m_preloadedEntries.take(storageKey)) {
        LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Retrieval: Using preloaded entry to satisfy request for '%s':", storageKey.identifier().utf8().data());
        completionHandler(preloadedEntry->takeCacheEntry());
        return true;
    }

    // Check pending speculative revalidations.
    if (!m_pendingPreloads.contains(storageKey))
        return false;

    LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Retrieval: revalidation already in progress for '%s':", storageKey.identifier().utf8().data());

    // FIXME: This breaks incremental loading when the revalidation is not successful.
    auto addResult = m_pendingRetrieveRequests.add(storageKey, nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = std::make_unique<Vector<RetrieveCompletionHandler>>();
    addResult.iterator->value->append(completionHandler);
    return true;
}

void SpeculativeLoadManager::registerLoad(const GlobalFrameID& frameID, const ResourceRequest& request, const Key& resourceKey)
{
    ASSERT(RunLoop::isMain());

    if (!request.url().protocolIsInHTTPFamily() || request.httpMethod() != "GET")
        return;

    auto isMainResource = request.requester() == ResourceRequest::Requester::Main;
    if (isMainResource) {
        // Mark previous load in this frame as completed if necessary.
        if (auto* pendingFrameLoad = m_pendingFrameLoads.get(frameID))
            pendingFrameLoad->markAsCompleted();

        // Start tracking loads in this frame.
        m_pendingFrameLoads.add(frameID, std::make_unique<PendingFrameLoad>(resourceKey, [this, frameID]() {
            auto frameLoad = m_pendingFrameLoads.take(frameID);
            auto optionalRecord = frameLoad->encodeAsSubresourcesRecord();
            if (!optionalRecord)
                return;
            m_storage.store(optionalRecord.value(), [](const Data&) { });
        }));
        return;
    }

    if (auto* pendingFrameLoad = m_pendingFrameLoads.get(frameID))
        pendingFrameLoad->registerSubresource(resourceKey);
}

void SpeculativeLoadManager::addPreloadedEntry(std::unique_ptr<Entry> entry)
{
    ASSERT(entry);
    ASSERT(!entry->needsValidation());
    auto key = entry->key();
    m_preloadedEntries.add(key, std::make_unique<PreloadedEntry>(WTF::move(entry), [this, key]() {
        m_preloadedEntries.remove(key);
    }));
}

void SpeculativeLoadManager::retrieveEntryFromStorage(const Key& key, const RetrieveCompletionHandler& completionHandler)
{
    m_storage.retrieve(key, static_cast<unsigned>(ResourceLoadPriority::Medium), [completionHandler](std::unique_ptr<Storage::Record> record) {
        if (!record) {
            completionHandler(nullptr);
            return false;
        }
        auto entry = Entry::decodeStorageRecord(*record);
        if (!entry) {
            completionHandler(nullptr);
            return false;
        }

        auto& response = entry->response();
        if (!response.hasCacheValidatorFields()) {
            completionHandler(nullptr);
            return true;
        }

        if (responseNeedsRevalidation(response, entry->timeStamp()))
            entry->setNeedsValidation();

        completionHandler(WTF::move(entry));
        return true;
    });
}

bool SpeculativeLoadManager::satisfyPendingRequests(const Key& key, Entry* entry)
{
    auto completionHandlers = m_pendingRetrieveRequests.take(key);
    if (!completionHandlers)
        return false;

    for (auto& completionHandler : *completionHandlers)
        completionHandler(entry ? std::make_unique<Entry>(*entry) : nullptr);

    return true;
}

void SpeculativeLoadManager::revalidateEntry(std::unique_ptr<Entry> entry, const GlobalFrameID& frameID)
{
    ASSERT(entry);
    ASSERT(entry->needsValidation());

    auto key = entry->key();
    LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Speculatively revalidating '%s':", key.identifier().utf8().data());
    auto revalidator = std::make_unique<SpeculativeLoad>(frameID, constructRevalidationRequest(*entry), WTF::move(entry), [this, key](std::unique_ptr<Entry> revalidatedEntry) {
        ASSERT(!revalidatedEntry || !revalidatedEntry->needsValidation());
        auto protectRevalidator = m_pendingPreloads.take(key);
        LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Speculative revalidation completed for '%s':", key.identifier().utf8().data());

        if (satisfyPendingRequests(key, revalidatedEntry.get()))
            return;

        if (revalidatedEntry)
            addPreloadedEntry(WTF::move(revalidatedEntry));
    });
    m_pendingPreloads.add(key, WTF::move(revalidator));
}

void SpeculativeLoadManager::preloadEntry(const Key& key, const GlobalFrameID& frameID)
{
    m_pendingPreloads.add(key, nullptr);
    retrieveEntryFromStorage(key, [this, key, frameID](std::unique_ptr<Entry> entry) {
        m_pendingPreloads.remove(key);

        if (satisfyPendingRequests(key, entry.get()))
            return;

        if (!entry)
            return;

        if (entry->needsValidation())
            revalidateEntry(WTF::move(entry), frameID);
        else
            addPreloadedEntry(WTF::move(entry));
    });
}

void SpeculativeLoadManager::startSpeculativeRevalidation(const ResourceRequest& originalRequest, const GlobalFrameID& frameID, const Key& storageKey)
{
    if (originalRequest.requester() != ResourceRequest::Requester::Main)
        return;

    auto subresourcesStorageKey = makeSubresourcesKey(storageKey);

    m_storage.retrieve(subresourcesStorageKey, static_cast<unsigned>(ResourceLoadPriority::Medium), [this, frameID](std::unique_ptr<Storage::Record> record) {
        if (!record)
            return false;

        auto subresourcesEntry = SubresourcesEntry::decodeStorageRecord(*record);
        if (!subresourcesEntry)
            return false;

        for (auto& subresourceKey : subresourcesEntry->subresourceKeys())
            preloadEntry(subresourceKey, frameID);

        return true;
    });
}

} // namespace NetworkCache

} // namespace WebKit

#endif // ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
