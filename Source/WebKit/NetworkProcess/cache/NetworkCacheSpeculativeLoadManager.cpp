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
#include <pal/HysteresisActivity.h>
#include <wtf/HashCountedSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>

namespace WebKit {

namespace NetworkCache {

using namespace WebCore;

static const Seconds preloadedEntryLifetime { 10_s };

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

static void logSpeculativeLoadingDiagnosticMessage(NetworkProcess& networkProcess, const GlobalFrameID& frameID, const String& message)
{
#if !LOG_DISABLED
    if (WebKit2LogNetworkCacheSpeculativePreloading.state == WTFLogChannelOn)
        allSpeculativeLoadingDiagnosticMessages().add(message);
#endif
    networkProcess.logDiagnosticMessage(frameID.first, WebCore::DiagnosticLoggingKeys::networkCacheKey(), message, WebCore::ShouldSample::Yes);
}

static const AtomicString& subresourcesType()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<const AtomicString> resource("SubResources", AtomicString::ConstructFromLiteral);
    return resource;
}

static inline Key makeSubresourcesKey(const Key& resourceKey, const Salt& salt)
{
    return Key(resourceKey.partition(), subresourcesType(), resourceKey.range(), resourceKey.identifier(), salt);
}

static inline ResourceRequest constructRevalidationRequest(const Key& key, const SubresourceInfo& subResourceInfo, const Entry* entry)
{
    ResourceRequest revalidationRequest(key.identifier());
    revalidationRequest.setHTTPHeaderFields(subResourceInfo.requestHeaders());
    revalidationRequest.setFirstPartyForCookies(subResourceInfo.firstPartyForCookies());
    revalidationRequest.setIsSameSite(subResourceInfo.isSameSite());
    revalidationRequest.setIsTopSite(subResourceInfo.isTopSite());
    if (!key.partition().isEmpty())
        revalidationRequest.setCachePartition(key.partition());
    ASSERT_WITH_MESSAGE(key.range().isEmpty(), "range is not supported");
    
    revalidationRequest.makeUnconditional();
    if (entry) {
        String eTag = entry->response().httpHeaderField(HTTPHeaderName::ETag);
        if (!eTag.isEmpty())
            revalidationRequest.setHTTPHeaderField(HTTPHeaderName::IfNoneMatch, eTag);

        String lastModified = entry->response().httpHeaderField(HTTPHeaderName::LastModified);
        if (!lastModified.isEmpty())
            revalidationRequest.setHTTPHeaderField(HTTPHeaderName::IfModifiedSince, lastModified);
    }
    
    revalidationRequest.setPriority(subResourceInfo.priority());

    return revalidationRequest;
}

static bool responseNeedsRevalidation(const ResourceResponse& response, WallTime timestamp)
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
    explicit ExpiringEntry(WTF::Function<void()>&& expirationHandler)
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
    PreloadedEntry(std::unique_ptr<Entry> entry, Optional<ResourceRequest>&& speculativeValidationRequest, WTF::Function<void()>&& lifetimeReachedHandler)
        : ExpiringEntry(WTFMove(lifetimeReachedHandler))
        , m_entry(WTFMove(entry))
        , m_speculativeValidationRequest(WTFMove(speculativeValidationRequest))
    { }

    std::unique_ptr<Entry> takeCacheEntry()
    {
        ASSERT(m_entry);
        return WTFMove(m_entry);
    }

    const Optional<ResourceRequest>& revalidationRequest() const { return m_speculativeValidationRequest; }
    bool wasRevalidated() const { return !!m_speculativeValidationRequest; }

private:
    std::unique_ptr<Entry> m_entry;
    Optional<ResourceRequest> m_speculativeValidationRequest;
};

class SpeculativeLoadManager::PendingFrameLoad : public RefCounted<PendingFrameLoad> {
public:
    static Ref<PendingFrameLoad> create(Storage& storage, const Key& mainResourceKey, WTF::Function<void()>&& loadCompletionHandler)
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
    PendingFrameLoad(Storage& storage, const Key& mainResourceKey, WTF::Function<void()>&& loadCompletionHandler)
        : m_storage(storage)
        , m_mainResourceKey(mainResourceKey)
        , m_loadCompletionHandler(WTFMove(loadCompletionHandler))
        , m_loadHysteresisActivity([this](PAL::HysteresisState state) { if (state == PAL::HysteresisState::Stopped) markLoadAsCompleted(); })
    {
        m_loadHysteresisActivity.impulse();
    }

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
            SubresourcesEntry entry(makeSubresourcesKey(m_mainResourceKey, m_storage.salt()), m_subresourceLoads);
            m_storage.store(entry.encodeAsStorageRecord(), [](const Data&) { });
        }
    }

    Storage& m_storage;
    Key m_mainResourceKey;
    Vector<std::unique_ptr<SubresourceLoad>> m_subresourceLoads;
    WTF::Function<void()> m_loadCompletionHandler;
    PAL::HysteresisActivity m_loadHysteresisActivity;
    std::unique_ptr<SubresourcesEntry> m_existingEntry;
    bool m_didFinishLoad { false };
    bool m_didRetrieveExistingEntry { false };
};

SpeculativeLoadManager::SpeculativeLoadManager(Cache& cache, Storage& storage)
    : m_cache(cache)
    , m_storage(storage)
{
}

SpeculativeLoadManager::~SpeculativeLoadManager()
{
}

#if !LOG_DISABLED

static void dumpHTTPHeadersDiff(const HTTPHeaderMap& headersA, const HTTPHeaderMap& headersB)
{
    auto aEnd = headersA.end();
    for (auto it = headersA.begin(); it != aEnd; ++it) {
        String valueB = headersB.get(it->key);
        if (valueB.isNull())
            LOG(NetworkCacheSpeculativePreloading, "* '%s' HTTP header is only in first request (value: %s)", it->key.utf8().data(), it->value.utf8().data());
        else if (it->value != valueB)
            LOG(NetworkCacheSpeculativePreloading, "* '%s' HTTP header differs in both requests: %s != %s", it->key.utf8().data(), it->value.utf8().data(), valueB.utf8().data());
    }
    auto bEnd = headersB.end();
    for (auto it = headersB.begin(); it != bEnd; ++it) {
        if (!headersA.contains(it->key))
            LOG(NetworkCacheSpeculativePreloading, "* '%s' HTTP header is only in second request (value: %s)", it->key.utf8().data(), it->value.utf8().data());
    }
}

#endif

static bool requestsHeadersMatch(const ResourceRequest& speculativeValidationRequest, const ResourceRequest& actualRequest)
{
    ASSERT(!actualRequest.isConditional());
    ResourceRequest speculativeRequest = speculativeValidationRequest;
    speculativeRequest.makeUnconditional();

    if (speculativeRequest.httpHeaderFields() != actualRequest.httpHeaderFields()) {
        LOG(NetworkCacheSpeculativePreloading, "Cannot reuse speculatively validated entry because HTTP headers used for validation do not match");
#if !LOG_DISABLED
        dumpHTTPHeadersDiff(speculativeRequest.httpHeaderFields(), actualRequest.httpHeaderFields());
#endif
        return false;
    }
    return true;
}

bool SpeculativeLoadManager::canUsePreloadedEntry(const PreloadedEntry& entry, const ResourceRequest& actualRequest)
{
    if (!entry.wasRevalidated())
        return true;

    ASSERT(entry.revalidationRequest());
    return requestsHeadersMatch(*entry.revalidationRequest(), actualRequest);
}

bool SpeculativeLoadManager::canUsePendingPreload(const SpeculativeLoad& load, const ResourceRequest& actualRequest)
{
    return requestsHeadersMatch(load.originalRequest(), actualRequest);
}

bool SpeculativeLoadManager::canRetrieve(const Key& storageKey, const WebCore::ResourceRequest& request, const GlobalFrameID& frameID) const
{
    // Check already preloaded entries.
    if (auto preloadedEntry = m_preloadedEntries.get(storageKey)) {
        if (!canUsePreloadedEntry(*preloadedEntry, request)) {
            LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Retrieval: Could not use preloaded entry to satisfy request for '%s' due to HTTP headers mismatch:", storageKey.identifier().utf8().data());
            logSpeculativeLoadingDiagnosticMessage(m_cache.networkProcess(), frameID, preloadedEntry->wasRevalidated() ? DiagnosticLoggingKeys::wastedSpeculativeWarmupWithRevalidationKey() : DiagnosticLoggingKeys::wastedSpeculativeWarmupWithoutRevalidationKey());
            return false;
        }

        LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Retrieval: Using preloaded entry to satisfy request for '%s':", storageKey.identifier().utf8().data());
        logSpeculativeLoadingDiagnosticMessage(m_cache.networkProcess(), frameID, preloadedEntry->wasRevalidated() ? DiagnosticLoggingKeys::successfulSpeculativeWarmupWithRevalidationKey() : DiagnosticLoggingKeys::successfulSpeculativeWarmupWithoutRevalidationKey());
        return true;
    }

    // Check pending speculative revalidations.
    auto* pendingPreload = m_pendingPreloads.get(storageKey);
    if (!pendingPreload) {
        if (m_notPreloadedEntries.get(storageKey))
            logSpeculativeLoadingDiagnosticMessage(m_cache.networkProcess(), frameID, DiagnosticLoggingKeys::entryWronglyNotWarmedUpKey());
        else
            logSpeculativeLoadingDiagnosticMessage(m_cache.networkProcess(), frameID, DiagnosticLoggingKeys::unknownEntryRequestKey());

        return false;
    }

    if (!canUsePendingPreload(*pendingPreload, request)) {
        LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Retrieval: revalidation already in progress for '%s' but unusable due to HTTP headers mismatch:", storageKey.identifier().utf8().data());
        logSpeculativeLoadingDiagnosticMessage(m_cache.networkProcess(), frameID, DiagnosticLoggingKeys::wastedSpeculativeWarmupWithRevalidationKey());
        return false;
    }

    LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Retrieval: revalidation already in progress for '%s':", storageKey.identifier().utf8().data());

    return true;
}

void SpeculativeLoadManager::retrieve(const Key& storageKey, RetrieveCompletionHandler&& completionHandler)
{
    if (auto preloadedEntry = m_preloadedEntries.take(storageKey)) {
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), cacheEntry = preloadedEntry->takeCacheEntry()] () mutable {
            completionHandler(WTFMove(cacheEntry));
        });
        return;
    }
    ASSERT(m_pendingPreloads.contains(storageKey));
    // FIXME: This breaks incremental loading when the revalidation is not successful.
    auto addResult = m_pendingRetrieveRequests.ensure(storageKey, [] {
        return std::make_unique<Vector<RetrieveCompletionHandler>>();
    });
    addResult.iterator->value->append(WTFMove(completionHandler));
}

void SpeculativeLoadManager::registerLoad(const GlobalFrameID& frameID, const ResourceRequest& request, const Key& resourceKey)
{
    ASSERT(RunLoop::isMain());
    ASSERT(request.url().protocolIsInHTTPFamily());

    if (request.httpMethod() != "GET")
        return;
    if (!request.httpHeaderField(HTTPHeaderName::Range).isEmpty())
        return;

    auto isMainResource = request.requester() == ResourceRequest::Requester::Main;
    if (isMainResource) {
        // Mark previous load in this frame as completed if necessary.
        if (auto* pendingFrameLoad = m_pendingFrameLoads.get(frameID))
            pendingFrameLoad->markLoadAsCompleted();

        ASSERT(!m_pendingFrameLoads.contains(frameID));

        // Start tracking loads in this frame.
        auto pendingFrameLoad = PendingFrameLoad::create(m_storage, resourceKey, [this, frameID] {
            bool wasRemoved = m_pendingFrameLoads.remove(frameID);
            ASSERT_UNUSED(wasRemoved, wasRemoved);
        });
        m_pendingFrameLoads.add(frameID, pendingFrameLoad.copyRef());

        // Retrieve the subresources entry if it exists to start speculative revalidation and to update it.
        retrieveSubresourcesEntry(resourceKey, [this, frameID, pendingFrameLoad = WTFMove(pendingFrameLoad)](std::unique_ptr<SubresourcesEntry> entry) {
            if (entry)
                startSpeculativeRevalidation(frameID, *entry);

            pendingFrameLoad->setExistingSubresourcesEntry(WTFMove(entry));
        });
        return;
    }

    if (auto* pendingFrameLoad = m_pendingFrameLoads.get(frameID))
        pendingFrameLoad->registerSubresourceLoad(request, resourceKey);
}

void SpeculativeLoadManager::addPreloadedEntry(std::unique_ptr<Entry> entry, const GlobalFrameID& frameID, Optional<ResourceRequest>&& revalidationRequest)
{
    ASSERT(entry);
    ASSERT(!entry->needsValidation());
    auto key = entry->key();
    m_preloadedEntries.add(key, std::make_unique<PreloadedEntry>(WTFMove(entry), WTFMove(revalidationRequest), [this, key, frameID] {
        auto preloadedEntry = m_preloadedEntries.take(key);
        ASSERT(preloadedEntry);
        if (preloadedEntry->wasRevalidated())
            logSpeculativeLoadingDiagnosticMessage(m_cache.networkProcess(), frameID, DiagnosticLoggingKeys::wastedSpeculativeWarmupWithRevalidationKey());
        else
            logSpeculativeLoadingDiagnosticMessage(m_cache.networkProcess(), frameID, DiagnosticLoggingKeys::wastedSpeculativeWarmupWithoutRevalidationKey());
    }));
}

void SpeculativeLoadManager::retrieveEntryFromStorage(const SubresourceInfo& info, RetrieveCompletionHandler&& completionHandler)
{
    m_storage.retrieve(info.key(), static_cast<unsigned>(info.priority()), [completionHandler = WTFMove(completionHandler)](auto record, auto timings) {
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
        if (responseNeedsRevalidation(response, entry->timeStamp())) {
            // Do not use cached redirects that have expired.
            if (entry->redirectRequest()) {
                completionHandler(nullptr);
                return true;
            }
            entry->setNeedsValidation(true);
        }

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

void SpeculativeLoadManager::revalidateSubresource(const SubresourceInfo& subresourceInfo, std::unique_ptr<Entry> entry, const GlobalFrameID& frameID)
{
    ASSERT(!entry || entry->needsValidation());

    auto& key = subresourceInfo.key();

    // Range is not supported.
    if (!key.range().isEmpty())
        return;

    ResourceRequest revalidationRequest = constructRevalidationRequest(key, subresourceInfo, entry.get());

    LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Speculatively revalidating '%s':", key.identifier().utf8().data());

    auto revalidator = std::make_unique<SpeculativeLoad>(m_cache, frameID, revalidationRequest, WTFMove(entry), [this, key, revalidationRequest, frameID](std::unique_ptr<Entry> revalidatedEntry) {
        ASSERT(!revalidatedEntry || !revalidatedEntry->needsValidation());
        ASSERT(!revalidatedEntry || revalidatedEntry->key() == key);

        auto protectRevalidator = m_pendingPreloads.take(key);
        LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Speculative revalidation completed for '%s':", key.identifier().utf8().data());

        if (satisfyPendingRequests(key, revalidatedEntry.get())) {
            if (revalidatedEntry)
                logSpeculativeLoadingDiagnosticMessage(m_cache.networkProcess(), frameID, DiagnosticLoggingKeys::successfulSpeculativeWarmupWithRevalidationKey());
            return;
        }

        if (revalidatedEntry)
            addPreloadedEntry(WTFMove(revalidatedEntry), frameID, revalidationRequest);
    });
    m_pendingPreloads.add(key, WTFMove(revalidator));
}
    
static bool canRevalidate(const SubresourceInfo& subresourceInfo, const Entry* entry)
{
    ASSERT(!subresourceInfo.isTransient());
    ASSERT(!entry || entry->needsValidation());
    
    if (entry && entry->response().hasCacheValidatorFields())
        return true;
    
    auto seenAge = subresourceInfo.lastSeen() - subresourceInfo.firstSeen();
    if (seenAge == 0_ms) {
        LOG(NetworkCacheSpeculativePreloading, "Speculative load: Seen only once");
        return false;
    }
    
    auto now = WallTime::now();
    auto firstSeenAge = now - subresourceInfo.firstSeen();
    auto lastSeenAge = now - subresourceInfo.lastSeen();
    // Sanity check.
    if (seenAge <= 0_ms || firstSeenAge <= 0_ms || lastSeenAge <= 0_ms)
        return false;
    
    // Load full resources speculatively if they seem to stay the same.
    const auto minimumAgeRatioToLoad = 2. / 3;
    const auto recentMinimumAgeRatioToLoad = 1. / 3;
    const auto recentThreshold = 5_min;
    
    auto ageRatio = seenAge / firstSeenAge;
    auto minimumAgeRatio = lastSeenAge > recentThreshold ? minimumAgeRatioToLoad : recentMinimumAgeRatioToLoad;
    
    LOG(NetworkCacheSpeculativePreloading, "Speculative load: ok=%d ageRatio=%f entry=%d", ageRatio > minimumAgeRatio, ageRatio, !!entry);
    
    if (ageRatio > minimumAgeRatio)
        return true;
    
    return false;
}

void SpeculativeLoadManager::preloadEntry(const Key& key, const SubresourceInfo& subresourceInfo, const GlobalFrameID& frameID)
{
    if (m_pendingPreloads.contains(key))
        return;
    m_pendingPreloads.add(key, nullptr);
    
    retrieveEntryFromStorage(subresourceInfo, [this, key, subresourceInfo, frameID](std::unique_ptr<Entry> entry) {
        ASSERT(!m_pendingPreloads.get(key));
        bool removed = m_pendingPreloads.remove(key);
        ASSERT_UNUSED(removed, removed);

        if (satisfyPendingRequests(key, entry.get())) {
            if (entry)
                logSpeculativeLoadingDiagnosticMessage(m_cache.networkProcess(), frameID, DiagnosticLoggingKeys::successfulSpeculativeWarmupWithoutRevalidationKey());
            return;
        }
        
        if (!entry || entry->needsValidation()) {
            if (canRevalidate(subresourceInfo, entry.get()))
                revalidateSubresource(subresourceInfo, WTFMove(entry), frameID);
            return;
        }
        
        addPreloadedEntry(WTFMove(entry), frameID);
    });
}

void SpeculativeLoadManager::startSpeculativeRevalidation(const GlobalFrameID& frameID, SubresourcesEntry& entry)
{
    for (auto& subresourceInfo : entry.subresources()) {
        auto& key = subresourceInfo.key();
        if (!subresourceInfo.isTransient())
            preloadEntry(key, subresourceInfo, frameID);
        else {
            LOG(NetworkCacheSpeculativePreloading, "(NetworkProcess) Not preloading '%s' because it is marked as transient", key.identifier().utf8().data());
            m_notPreloadedEntries.add(key, std::make_unique<ExpiringEntry>([this, key, frameID] {
                logSpeculativeLoadingDiagnosticMessage(m_cache.networkProcess(), frameID, DiagnosticLoggingKeys::entryRightlyNotWarmedUpKey());
                m_notPreloadedEntries.remove(key);
            }));
        }
    }
}

void SpeculativeLoadManager::retrieveSubresourcesEntry(const Key& storageKey, WTF::Function<void (std::unique_ptr<SubresourcesEntry>)>&& completionHandler)
{
    ASSERT(storageKey.type() == "Resource");
    auto subresourcesStorageKey = makeSubresourcesKey(storageKey, m_storage.salt());
    m_storage.retrieve(subresourcesStorageKey, static_cast<unsigned>(ResourceLoadPriority::Medium), [completionHandler = WTFMove(completionHandler)](auto record, auto timings) {
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
