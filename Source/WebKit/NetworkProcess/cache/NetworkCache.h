/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "NetworkCacheEntry.h"
#include "NetworkCacheStorage.h"
#include "PolicyDecision.h"
#include "WebPageProxyIdentifier.h"
#include "WebsiteData.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/ShareableResource.h>
#include <pal/SessionID.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Hasher.h>
#include <wtf/OptionSet.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class LowPowerModeNotifier;
class ResourceRequest;
class FragmentedSharedBuffer;
enum class AdvancedPrivacyProtections : uint16_t;
}

namespace WebKit {
namespace NetworkCache {

struct GlobalFrameID {
    WebPageProxyIdentifier webPageProxyID;
    WebCore::PageIdentifier webPageID;
    WebCore::FrameIdentifier frameID;
};

inline void add(Hasher& hasher, const GlobalFrameID& identifier)
{
    add(hasher, identifier.webPageID, identifier.frameID);
}

inline bool operator==(const GlobalFrameID& a, const GlobalFrameID& b)
{
    // No need to check webPageProxyID since webPageIDs are globally unique and a given WebPage is always
    // associated with the same WebPageProxy.
    return a.webPageID == b.webPageID &&  a.frameID == b.frameID;
}

}
}

namespace WTF {

struct GlobalFrameIDHash {
    static unsigned hash(const WebKit::NetworkCache::GlobalFrameID& key) { return computeHash(key); }
    static bool equal(const WebKit::NetworkCache::GlobalFrameID& a, const WebKit::NetworkCache::GlobalFrameID& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<WebKit::NetworkCache::GlobalFrameID> : GenericHashTraits<WebKit::NetworkCache::GlobalFrameID> {
    static WebKit::NetworkCache::GlobalFrameID emptyValue() { return { HashTraits<WebKit::WebPageProxyIdentifier>::emptyValue(), HashTraits<WebCore::PageIdentifier>::emptyValue(), HashTraits<WebCore::FrameIdentifier>::emptyValue() }; }
    static bool isEmptyValue(const WebKit::NetworkCache::GlobalFrameID& slot) { return slot.webPageID.isHashTableEmptyValue(); }

    static void constructDeletedValue(WebKit::NetworkCache::GlobalFrameID& slot) { new (NotNull, &slot.webPageID) WebCore::PageIdentifier(WTF::HashTableDeletedValue); }

    static bool isDeletedValue(const WebKit::NetworkCache::GlobalFrameID& slot) { return slot.webPageID.isHashTableDeletedValue(); }
};

template<> struct DefaultHash<WebKit::NetworkCache::GlobalFrameID> : GlobalFrameIDHash { };

}

namespace WebKit {

class NetworkProcess;

namespace NetworkCache {

class AsyncRevalidation;
class Cache;
class SpeculativeLoadManager;

struct MappedBody {
#if ENABLE(SHAREABLE_RESOURCE)
    RefPtr<WebCore::ShareableResource> shareableResource;
    std::optional<WebCore::ShareableResource::Handle> shareableResourceHandle;
#endif
};

enum class RetrieveDecision {
    Yes,
    NoDueToHTTPMethod,
    NoDueToConditionalRequest,
    NoDueToReloadIgnoringCache,
    NoDueToStreamingMedia
};

enum class StoreDecision {
    Yes,
    NoDueToProtocol,
    NoDueToHTTPMethod,
    NoDueToNoStoreResponse,
    NoDueToHTTPStatusCode,
    NoDueToNoStoreRequest,
    NoDueToUnlikelyToReuse,
    NoDueToStreamingMedia,
    NoDueToRequestContainingFragments
};

enum class UseDecision {
    Use,
    Validate,
    AsyncRevalidate,
    NoDueToVaryingHeaderMismatch,
    NoDueToMissingValidatorFields,
    NoDueToDecodeFailure,
    NoDueToExpiredRedirect,
    NoDueToRequestContainingFragments
};

enum class CacheOption : uint8_t {
    // In testing mode we try to eliminate sources of randomness. Cache does not shrink and there are no read timeouts.
    TestingMode = 1 << 0,
    RegisterNotify = 1 << 1,
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    SpeculativeRevalidation = 1 << 2,
#endif
};

class Cache : public RefCounted<Cache>, public CanMakeWeakPtr<Cache> {
public:
    ~Cache();
    static RefPtr<Cache> open(NetworkProcess&, const String& cachePath, OptionSet<CacheOption>, PAL::SessionID);

    size_t capacity() const;
    void updateCapacity();

    // Completion handler may get called back synchronously on failure.
    struct RetrieveInfo {
        MonotonicTime startTime;
        MonotonicTime completionTime;
        unsigned priority;
        Storage::Timings storageTimings;
        bool wasSpeculativeLoad { false };

        WTF_MAKE_TZONE_ALLOCATED(RetrieveInfo);
    };
    using RetrieveCompletionHandler = Function<void(std::unique_ptr<Entry>, const RetrieveInfo&)>;
    void retrieve(const WebCore::ResourceRequest&, std::optional<GlobalFrameID>, std::optional<NavigatingToAppBoundDomain>, bool allowPrivacyProxy, OptionSet<WebCore::AdvancedPrivacyProtections>, RetrieveCompletionHandler&&);
    std::unique_ptr<Entry> store(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, PrivateRelayed, RefPtr<WebCore::FragmentedSharedBuffer>&&, Function<void(MappedBody&&)>&& = nullptr);
    std::unique_ptr<Entry> storeRedirect(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, const WebCore::ResourceRequest& redirectRequest, std::optional<Seconds> maxAgeCap);
    std::unique_ptr<Entry> update(const WebCore::ResourceRequest&, const Entry&, const WebCore::ResourceResponse& validatingResponse, PrivateRelayed);

    struct TraversalEntry {
        const Entry& entry;
        const Storage::RecordInfo& recordInfo;
    };
    void traverse(Function<void(const TraversalEntry*)>&&);
    void traverse(const String& partition, Function<void(const TraversalEntry*)>&&);
    void remove(const Key&);
    void remove(const WebCore::ResourceRequest&);
    void remove(const Vector<Key>&, Function<void()>&&);

    void clear();
    void clear(WallTime modifiedSince, Function<void()>&&);

    std::unique_ptr<Entry> makeEntry(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, PrivateRelayed, RefPtr<WebCore::FragmentedSharedBuffer>&&);
    std::unique_ptr<Entry> makeRedirectEntry(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, const WebCore::ResourceRequest& redirectRequest);

    void dumpContentsToFile();

    String recordsPathIsolatedCopy() const;

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    SpeculativeLoadManager* speculativeLoadManager() { return m_speculativeLoadManager.get(); }
#endif

#if ENABLE(NETWORK_CACHE_STALE_WHILE_REVALIDATE)
    void startAsyncRevalidationIfNeeded(const WebCore::ResourceRequest&, const NetworkCache::Key&, std::unique_ptr<Entry>&&, const GlobalFrameID&, std::optional<NavigatingToAppBoundDomain>, bool allowPrivacyProxy, OptionSet<WebCore::AdvancedPrivacyProtections>);
#endif

    void browsingContextRemoved(WebPageProxyIdentifier, WebCore::PageIdentifier, WebCore::FrameIdentifier);

    NetworkProcess& networkProcess() { return m_networkProcess.get(); }
    Ref<NetworkProcess> protectedNetworkProcess();
    PAL::SessionID sessionID() const { return m_sessionID; }
    const String& storageDirectory() const { return m_storageDirectory; }
    void fetchData(bool shouldComputeSize, CompletionHandler<void(Vector<WebsiteData::Entry>&&)>&&);
    void deleteData(const Vector<WebCore::SecurityOriginData>&, CompletionHandler<void()>&&);
    void deleteDataForRegistrableDomains(const Vector<WebCore::RegistrableDomain>&, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);

private:
    Cache(NetworkProcess&, const String& storageDirectory, Ref<Storage>&&, OptionSet<CacheOption>, PAL::SessionID);

    Key makeCacheKey(const WebCore::ResourceRequest&);

    static void completeRetrieve(RetrieveCompletionHandler&&, std::unique_ptr<Entry>, RetrieveInfo&);

    String dumpFilePath() const;
    void deleteDumpFile();

    std::optional<Seconds> maxAgeCap(Entry&, const WebCore::ResourceRequest&, PAL::SessionID);

    Ref<Storage> m_storage;
    Ref<NetworkProcess> m_networkProcess;

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    std::unique_ptr<WebCore::LowPowerModeNotifier> m_lowPowerModeNotifier;
    std::unique_ptr<SpeculativeLoadManager> m_speculativeLoadManager;
#endif

#if ENABLE(NETWORK_CACHE_STALE_WHILE_REVALIDATE)
    HashMap<Key, Ref<AsyncRevalidation>> m_pendingAsyncRevalidations;
    HashMap<GlobalFrameID, WeakHashSet<AsyncRevalidation>> m_pendingAsyncRevalidationByPage;
#endif

    unsigned m_traverseCount { 0 };
    PAL::SessionID m_sessionID;
    String m_storageDirectory;
};

} // namespace NetworkCache
} // namespace WebKit
