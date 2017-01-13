/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#ifndef NetworkCache_h
#define NetworkCache_h

#if ENABLE(NETWORK_CACHE)

#include "NetworkCacheEntry.h"
#include "NetworkCacheStorage.h"
#include "ShareableResource.h"
#include <WebCore/ResourceResponse.h>
#include <wtf/Function.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class ResourceRequest;
class SharedBuffer;
class URL;
}

namespace WebKit {
namespace NetworkCache {

class Cache;
class SpeculativeLoadManager;
class Statistics;

Cache& singleton();

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

// FIXME: This enum is used in the Statistics code in a way that prevents removing or reordering anything.
enum class StoreDecision {
    Yes,
    NoDueToProtocol,
    NoDueToHTTPMethod,
    NoDueToAttachmentResponse, // Unused.
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
    NoDueToExpiredRedirect,
};

using GlobalFrameID = std::pair<uint64_t /*webPageID*/, uint64_t /*webFrameID*/>;

class Cache {
    WTF_MAKE_NONCOPYABLE(Cache);
    friend class WTF::NeverDestroyed<Cache>;
public:
    struct Parameters {
        bool enableEfficacyLogging;
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
        bool enableNetworkCacheSpeculativeRevalidation;
#endif
    };
    bool initialize(const String& cachePath, const Parameters&);
    void setCapacity(size_t);

    bool isEnabled() const { return !!m_storage; }

    // Completion handler may get called back synchronously on failure.
    void retrieve(const WebCore::ResourceRequest&, const GlobalFrameID&, std::function<void (std::unique_ptr<Entry>)>&&);
    std::unique_ptr<Entry> store(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, RefPtr<WebCore::SharedBuffer>&&, Function<void (MappedBody&)>&&);
    std::unique_ptr<Entry> storeRedirect(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, const WebCore::ResourceRequest& redirectRequest);
    std::unique_ptr<Entry> update(const WebCore::ResourceRequest&, const GlobalFrameID&, const Entry&, const WebCore::ResourceResponse& validatingResponse);

    struct TraversalEntry {
        const Entry& entry;
        const Storage::RecordInfo& recordInfo;
    };
    void traverse(Function<void (const TraversalEntry*)>&&);
    void remove(const Key&);
    void remove(const WebCore::ResourceRequest&);

    void clear();
    void clear(std::chrono::system_clock::time_point modifiedSince, std::function<void ()>&& completionHandler);

    void dumpContentsToFile();

    String recordsPath() const;

private:
    Cache() = default;
    ~Cache() = delete;

    String dumpFilePath() const;
    void deleteDumpFile();

    std::unique_ptr<Storage> m_storage;
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    std::unique_ptr<SpeculativeLoadManager> m_speculativeLoadManager;
#endif
    std::unique_ptr<Statistics> m_statistics;

    unsigned m_traverseCount { 0 };
};

}
}
#endif
#endif
