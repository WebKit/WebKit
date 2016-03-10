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
#include "NetworkProcess.h"
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/HysteresisActivity.h>
#include <wtf/HashCountedSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>

namespace WebKit {

namespace NetworkCache {

using namespace WebCore;

static const auto preloadedEntryLifetime = 10_s;

#if !LOG_DISABLED
static HashCountedSet<String>& allSpeculativeLoadingDiagnosticMessages()
{
    static NeverDestroyed<HashCountedSet<String>> messages;
    return messages;
}

static void printSpeculativeLoadingDiagnosticMessageCounts()
{
    LOG(NetworkCacheSpeculativePreloading, "-- Speculative loading statistics --");
    for (auto& pair : allSpeculativeLoadingDiagnosticMessages())
        LOG(NetworkCacheSpeculativePreloading, "%s: %u", pair.key.utf8().data(), pair.value);
}
#endif

static void logSpeculativeLoadingDiagnosticMessage(const GlobalFrameID& frameID, const String& message)
{
#if !LOG_DISABLED
    if (WebKit2LogNetworkCacheSpeculativePreloading.state == WTFLogChannelOn)
        allSpeculativeLoadingDiagnosticMessages().add(message);
#endif
    NetworkProcess::singleton().logDiagnosticMessage(frameID.first, WebCore::DiagnosticLoggingKeys::networkCacheKey(), message, WebCore::ShouldSample::Yes);
}

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

static inline ResourceRequest constructRevalidationRequest(const Entry& entry, const URL& firstPartyForCookies)
{
    ResourceRequest revalidationRequest(entry.key().identifier());
    revalidationRequest.setFirstPartyForCookies(firstPartyForCookies);
#if ENABLE(CACHE_PARTITIONING)
    if (entry.key().hasPartition())
        revalidationRequest.setCachePartition(entry.key().partition());
#endif
    ASSERT_WITH_MESSAGE(entry.key().range().isEmpty(), "range is not supported");

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

class SpeculativeLoadManager::ExpiringEntry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ExpiringEntry(std::function<void()>&& expirationHandler)
        : m_lifetimeTimer(WTFMove(expirationHandler))
    {
        m_lifetimeTimer.startOneShot(preloadedEntryLifetime);
    }

private:
    Timer m_lifetimeTimer;
};

class SpeculativeLoadManager::PreloadedEntry : private ExpiringEntry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PreloadedEntry(std::unique_ptr<Entry> entry, WasRevalidated wasRevalidated, std::function<void()>&& lifetimeReachedHandler)
        : ExpiringEntry(WTFMove(lifetimeReachedHandler))
        , m_entry(WTFMove(entry))
        , m_wasRevalidated(wasRevalidated == WasRevalidated::Yes)
    { }

    std::unique_ptr<Entry> takeCacheEntry()
    {
        ASSERT(m_entry);
        return WTFMove(m_entry);
    }

    bool wasRevalidated() const { return m_wasRevalidated; }

private:
    std::unique_ptr<Entry> m_entry;
    bool m_wasRevalidated;
};

class SpeculativeLoadManager::PendingFrameLoad : public RefCounted<PendingFrameLoad> {
public:
    static Ref<PendingFrameLoad> create(Storage& storage, const Key& mainResourceKey, std::function<void()>&& loadCompletionHandler)
    {
        return adoptRef(*new PendingFrameLoad(storage, mainResourceKey, WTFMove(loadCompletionHandler)));
    }

    ~PendingFrameLoad()
    {
        ASSERT(m_didFinishLoad);
        ASSERT(m_didRetrieveExistingEntry);
    }

    void registerSubresourceLoad(const ResourceRequest& request, const Key& subresourceKey)
    {
        ASSERT(RunLoop::isMain());
        m_subresourceLoads.append(std::make_unique<SubresourceLoad>(request, subresourceKey));
        m_loadHysteresisActivity.impulse();
    }

    void markLoadAsCompleted()
    {
        ASSERT(RunLoop::isMain());
        if (m_didFinishLoad)
            return;

#if !LOG_DISABLED
        printSpeculativeLoadingDiagnosticMessageCounts();
#endif

        m_didFinishLoad = true;
        saveToDiskIfReady();
        m_loadCompletionHandler();
    }

    void setExistingSubresourcesEntry(std::unique_ptr<SubresourcesEntry> entry)
    {
        ASSERT(!m_existingEntry);
        ASSERT(!m_didRetrieveExistingEntry);

        m_existingEntry = WTFMove(entry);
        m_didRetrieveExistingEntry = true;
        saveToDiskIfReady();
    }

private:
    PendingFrameLoad(Storage& storage, const Key& mainResourceKey, std::function<void()>&& loadCompletionHandler)
        : m_storage(storage)
        , m_mainResourceKey(mainResourceKey)
        , m_loadCompletionHandler(WTFMove(loadCompletionHandler))
        , m_loadHysteresisActivity([this](HysteresisState state) { if (state == HysteresisState::Stopped) markLoadAsCompleted(); })
    { }

    void saveToDiskIfReady()
    {
        if (!m_didFinishLoad || !m_didRetrieveExistingEntry)
            return;

        if (m_subresourceLoads.isEmpty())
            return;

#if !LOG_DISABLED
        LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Saving to disk list of subresources for '%s':", m_mainResourceKey.identifier().utf8().data());
        for (auto& subresourceLoad : m_subresourceLoads)
            LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) * Subresource: '%s'.", subresourceLoad->key.identifier().utf8().data());
#endif

        if (m_existingEntry) {
            m_existingEntry->updateSubresourceLoads(m_subresourceLoads);
            m_storage.store(m_existingEntry->encodeAsStorageRecord(), [](const Data&) { });
        } else {
            SubresourcesEntry entry(makeSubresourcesKey(m_mainResourceKey), m_subresourceLoads);
            m_storage.store(entry.encodeAsStorageRecord(), [](const Data&) { });
        }
    }

    Storage& m_storage;
    Key m_mainResourceKey;
    Vector<std::unique_ptr<SubresourceLoad>> m_subresourceLoads;
    std::function<void()> m_loadCompletionHandler;
    HysteresisActivity m_loadHysteresisActivity;
    std::unique_ptr<SubresourcesEntry> m_existingEntry;
    bool m_didFinishLoad { false };
    bool m_didRetrieveExistingEntry { false };
};

SpeculativeLoadManager::SpeculativeLoadManager(Storage& storage)
    : m_storage(storage)
{
}

SpeculativeLoadManager::~SpeculativeLoadManager()
{
}

bool SpeculativeLoadManager::retrieve(const GlobalFrameID& frameID, const Key& storageKey, const RetrieveCompletionHandler& completionHandler)
{
    // Check already preloaded entries.
    if (auto preloadedEntry = m_preloadedEntries.take(storageKey)) {
        LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Retrieval: Using preloaded entry to satisfy request for '%s':", storageKey.identifier().utf8().data());
        if (preloadedEntry->wasRevalidated())
            logSpeculativeLoadingDiagnosticMessage(frameID, DiagnosticLoggingKeys::successfulSpeculativeWarmupWithRevalidationKey());
        else
            logSpeculativeLoadingDiagnosticMessage(frameID, DiagnosticLoggingKeys::successfulSpeculativeWarmupWithoutRevalidationKey());

        completionHandler(preloadedEntry->takeCacheEntry());
        return true;
    }

    // Check pending speculative revalidations.
    if (!m_pendingPreloads.contains(storageKey)) {
        if (m_notPreloadedEntries.remove(storageKey))
            logSpeculativeLoadingDiagnosticMessage(frameID, DiagnosticLoggingKeys::entryWronglyNotWarmedUpKey());
        else
            logSpeculativeLoadingDiagnosticMessage(frameID, DiagnosticLoggingKeys::unknownEntryRequestKey());

        return false;
    }

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
            pendingFrameLoad->markLoadAsCompleted();

        ASSERT(!m_pendingFrameLoads.contains(frameID));

        // Start tracking loads in this frame.
        RefPtr<PendingFrameLoad> pendingFrameLoad = PendingFrameLoad::create(m_storage, resourceKey, [this, frameID] {
            bool wasRemoved = m_pendingFrameLoads.remove(frameID);
            ASSERT_UNUSED(wasRemoved, wasRemoved);
        });
        m_pendingFrameLoads.add(frameID, pendingFrameLoad);

        // Retrieve the subresources entry if it exists to start speculative revalidation and to update it.
        retrieveSubresourcesEntry(resourceKey, [this, frameID, pendingFrameLoad](std::unique_ptr<SubresourcesEntry> entry) {
            if (entry)
                startSpeculativeRevalidation(frameID, *entry);

            pendingFrameLoad->setExistingSubresourcesEntry(WTFMove(entry));
        });
        return;
    }

    if (auto* pendingFrameLoad = m_pendingFrameLoads.get(frameID))
        pendingFrameLoad->registerSubresourceLoad(request, resourceKey);
}

void SpeculativeLoadManager::addPreloadedEntry(std::unique_ptr<Entry> entry, const GlobalFrameID& frameID, WasRevalidated wasRevalidated)
{
    ASSERT(entry);
    ASSERT(!entry->needsValidation());
    auto key = entry->key();
    m_preloadedEntries.add(key, std::make_unique<PreloadedEntry>(WTFMove(entry), wasRevalidated, [this, key, frameID] {
        auto preloadedEntry = m_preloadedEntries.take(key);
        ASSERT(preloadedEntry);
        if (preloadedEntry->wasRevalidated())
            logSpeculativeLoadingDiagnosticMessage(frameID, DiagnosticLoggingKeys::wastedSpeculativeWarmupWithRevalidationKey());
        else
            logSpeculativeLoadingDiagnosticMessage(frameID, DiagnosticLoggingKeys::wastedSpeculativeWarmupWithoutRevalidationKey());
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
            entry->setNeedsValidation(true);

        completionHandler(WTFMove(entry));
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

void SpeculativeLoadManager::revalidateEntry(std::unique_ptr<Entry> entry, const SubresourceInfo& subresourceInfo, const GlobalFrameID& frameID)
{
    ASSERT(entry);
    ASSERT(entry->needsValidation());

    auto key = entry->key();

    // Range is not supported.
    if (!key.range().isEmpty())
        return;

    ResourceRequest revalidationRequest = constructRevalidationRequest(*entry, subresourceInfo.firstPartyForCookies);

    LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Speculatively revalidating '%s':", key.identifier().utf8().data());
    auto revalidator = std::make_unique<SpeculativeLoad>(frameID, revalidationRequest, WTFMove(entry), [this, key, frameID](std::unique_ptr<Entry> revalidatedEntry) {
        ASSERT(!revalidatedEntry || !revalidatedEntry->needsValidation());
        ASSERT(!revalidatedEntry || revalidatedEntry->key() == key);

        auto protectRevalidator = m_pendingPreloads.take(key);
        LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Speculative revalidation completed for '%s':", key.identifier().utf8().data());

        if (satisfyPendingRequests(key, revalidatedEntry.get())) {
            if (revalidatedEntry)
                logSpeculativeLoadingDiagnosticMessage(frameID, DiagnosticLoggingKeys::successfulSpeculativeWarmupWithRevalidationKey());
            return;
        }

        if (revalidatedEntry)
            addPreloadedEntry(WTFMove(revalidatedEntry), frameID, WasRevalidated::Yes);
    });
    m_pendingPreloads.add(key, WTFMove(revalidator));
}

void SpeculativeLoadManager::preloadEntry(const Key& key, const SubresourceInfo& subResourceInfo, const GlobalFrameID& frameID)
{
    m_pendingPreloads.add(key, nullptr);
    URLCapture firstPartyForCookies(subResourceInfo.firstPartyForCookies);
    retrieveEntryFromStorage(key, [this, key, firstPartyForCookies, frameID](std::unique_ptr<Entry> entry) {
        m_pendingPreloads.remove(key);

        if (satisfyPendingRequests(key, entry.get())) {
            if (entry)
                logSpeculativeLoadingDiagnosticMessage(frameID, DiagnosticLoggingKeys::successfulSpeculativeWarmupWithoutRevalidationKey());
            return;
        }

        if (!entry)
            return;

        if (entry->needsValidation())
            revalidateEntry(WTFMove(entry), firstPartyForCookies.url(), frameID);
        else
            addPreloadedEntry(WTFMove(entry), frameID, WasRevalidated::No);
    });
}

void SpeculativeLoadManager::startSpeculativeRevalidation(const GlobalFrameID& frameID, SubresourcesEntry& entry)
{
    for (auto& subresourcePair : entry.subresources()) {
        auto key = subresourcePair.key;
        auto subresourceInfo = subresourcePair.value;
        if (!subresourceInfo.isTransient)
            preloadEntry(key, subresourceInfo, frameID);
        else {
            LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Not preloading '%s' because it is marked as transient", key.identifier().utf8().data());
            m_notPreloadedEntries.add(key, std::make_unique<ExpiringEntry>([this, key, frameID] {
                logSpeculativeLoadingDiagnosticMessage(frameID, DiagnosticLoggingKeys::entryRightlyNotWarmedUpKey());
                m_notPreloadedEntries.remove(key);
            }));
        }
    }
}

void SpeculativeLoadManager::retrieveSubresourcesEntry(const Key& storageKey, std::function<void (std::unique_ptr<SubresourcesEntry>)> completionHandler)
{
    ASSERT(storageKey.type() == "resource");
    auto subresourcesStorageKey = makeSubresourcesKey(storageKey);
    m_storage.retrieve(subresourcesStorageKey, static_cast<unsigned>(ResourceLoadPriority::Medium), [completionHandler](std::unique_ptr<Storage::Record> record) {
        if (!record) {
            completionHandler(nullptr);
            return false;
        }

        auto subresourcesEntry = SubresourcesEntry::decodeStorageRecord(*record);
        if (!subresourcesEntry) {
            completionHandler(nullptr);
            return false;
        }

        completionHandler(WTFMove(subresourcesEntry));
        return true;
    });
}

} // namespace NetworkCache

} // namespace WebKit

#endif // ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
