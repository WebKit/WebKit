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
#include "ShareableResource.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/CompletionHandler.h>
#include <wtf/OptionSet.h>
#include <wtf/Seconds.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class LowPowerModeNotifier;
class ResourceRequest;
class SharedBuffer;
}

namespace WebKit {

class NetworkProcess;

namespace NetworkCache {

class Cache;
class SpeculativeLoadManager;

struct MappedBody {
#if ENABLE(SHAREABLE_RESOURCE)
    RefPtr<ShareableResource> shareableResource;
    ShareableResource::Handle shareableResourceHandle;
#endif
};

enum class RetrieveDecision {
    Yes,
    NoDueToHTTPMethod,
    NoDueToConditionalRequest,
    NoDueToReloadIgnoringCache,
    NoDueToStreamingMedia,
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
};

enum class UseDecision {
    Use,
    Validate,
    NoDueToVaryingHeaderMismatch,
    NoDueToMissingValidatorFields,
    NoDueToDecodeFailure,
    NoDueToExpiredRedirect
};

using GlobalFrameID = std::pair<WebCore::PageIdentifier, WebCore::FrameIdentifier>; // FIXME: Use GlobalFrameIdentifier.

enum class CacheOption : uint8_t {
    // In testing mode we try to eliminate sources of randomness. Cache does not shrink and there are no read timeouts.
    TestingMode = 1 << 0,
    RegisterNotify = 1 << 1,
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    SpeculativeRevalidation = 1 << 2,
#endif
};

class Cache : public RefCounted<Cache> {
public:
    static RefPtr<Cache> open(NetworkProcess&, const String& cachePath, OptionSet<CacheOption>, PAL::SessionID);

    void setCapacity(size_t);

    // Completion handler may get called back synchronously on failure.
    struct RetrieveInfo {
        MonotonicTime startTime;
        MonotonicTime completionTime;
        unsigned priority;
        Storage::Timings storageTimings;
        bool wasSpeculativeLoad { false };

        WTF_MAKE_FAST_ALLOCATED;
    };
    using RetrieveCompletionHandler = Function<void(std::unique_ptr<Entry>, const RetrieveInfo&)>;
    void retrieve(const WebCore::ResourceRequest&, const GlobalFrameID&, RetrieveCompletionHandler&&);
    std::unique_ptr<Entry> store(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, RefPtr<WebCore::SharedBuffer>&&, Function<void(MappedBody&)>&& = nullptr);
    std::unique_ptr<Entry> storeRedirect(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, const WebCore::ResourceRequest& redirectRequest, Optional<Seconds> maxAgeCap);
    std::unique_ptr<Entry> update(const WebCore::ResourceRequest&, const GlobalFrameID&, const Entry&, const WebCore::ResourceResponse& validatingResponse);

    struct TraversalEntry {
        const Entry& entry;
        const Storage::RecordInfo& recordInfo;
    };
    void traverse(Function<void(const TraversalEntry*)>&&);
    void remove(const Key&);
    void remove(const WebCore::ResourceRequest&);
    void remove(const Vector<Key>&, Function<void()>&&);

    void clear();
    void clear(WallTime modifiedSince, Function<void()>&&);

    void retrieveData(const DataKey&, Function<void(const uint8_t*, size_t)>);
    void storeData(const DataKey&,  const uint8_t* data, size_t);
    
    std::unique_ptr<Entry> makeEntry(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, RefPtr<WebCore::SharedBuffer>&&);
    std::unique_ptr<Entry> makeRedirectEntry(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, const WebCore::ResourceRequest& redirectRequest);

    void dumpContentsToFile();

    String recordsPathIsolatedCopy() const;

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    SpeculativeLoadManager* speculativeLoadManager() { return m_speculativeLoadManager.get(); }
#endif

    NetworkProcess& networkProcess() { return m_networkProcess.get(); }
    const PAL::SessionID& sessionID() const { return m_sessionID; }

    ~Cache();

private:
    Cache(NetworkProcess&, Ref<Storage>&&, OptionSet<CacheOption>, PAL::SessionID);

    Key makeCacheKey(const WebCore::ResourceRequest&);

    static void completeRetrieve(RetrieveCompletionHandler&&, std::unique_ptr<Entry>, RetrieveInfo&);

    String dumpFilePath() const;
    void deleteDumpFile();

    Optional<Seconds> maxAgeCap(Entry&, const WebCore::ResourceRequest&, PAL::SessionID);

    Ref<Storage> m_storage;
    Ref<NetworkProcess> m_networkProcess;

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    std::unique_ptr<WebCore::LowPowerModeNotifier> m_lowPowerModeNotifier;
    std::unique_ptr<SpeculativeLoadManager> m_speculativeLoadManager;
#endif

    unsigned m_traverseCount { 0 };
    PAL::SessionID m_sessionID;
};

} // namespace NetworkCache
} // namespace WebKit
