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

#ifndef NetworkCacheStatistics_h
#define NetworkCacheStatistics_h

#if ENABLE(NETWORK_CACHE)

#include "NetworkCache.h"
#include "NetworkCacheKey.h"
#include "NetworkCacheStorage.h"
#include <WebCore/SQLiteDatabase.h>

namespace WebCore {
class ResourceRequest;
}

namespace WebKit {

class NetworkCacheStatistics {
public:
    static std::unique_ptr<NetworkCacheStatistics> open(const String& cachePath);

    void clear();

    void recordNotCachingResponse(const NetworkCacheKey&, NetworkCache::StoreDecision);
    void recordNotUsingCacheForRequest(uint64_t webPageID, const NetworkCacheKey&, const WebCore::ResourceRequest&, NetworkCache::RetrieveDecision);
    void recordRetrievalFailure(uint64_t webPageID, const NetworkCacheKey&, const WebCore::ResourceRequest&);
    void recordRetrievedCachedEntry(uint64_t webPageID, const NetworkCacheKey&, const WebCore::ResourceRequest&, NetworkCache::CachedEntryReuseFailure);

private:
    explicit NetworkCacheStatistics(const String& databasePath);

    void initialize(const String& databasePath);
    void bootstrapFromNetworkCache(const String& networkCachePath);
    void shrinkIfNeeded();

    void addHashToDatabase(const String& hash);

    typedef std::function<void (bool wasEverRequested, const Optional<NetworkCache::StoreDecision>&)> RequestedCompletionHandler;
    void queryWasEverRequested(const String&, const RequestedCompletionHandler&);
    void markAsRequested(const String& hash);

    struct EverRequestedQuery {
        String hash;
        RequestedCompletionHandler completionHandler;
    };

    std::atomic<size_t> m_approximateEntryCount { 0 };

#if PLATFORM(COCOA)
    mutable DispatchPtr<dispatch_queue_t> m_backgroundIOQueue;
#endif
    mutable HashSet<std::unique_ptr<const EverRequestedQuery>> m_activeQueries;
    WebCore::SQLiteDatabase m_database;
};

} // namespace WebKit

#endif // ENABLE(NETWORK_CACHE)

#endif // NetworkCacheStatistics_h
