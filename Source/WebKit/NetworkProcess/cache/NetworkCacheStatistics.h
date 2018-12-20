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

#include "NetworkCache.h"
#include "NetworkCacheKey.h"
#include <WebCore/SQLiteDatabase.h>
#include <WebCore/Timer.h>
#include <wtf/WorkQueue.h>

namespace WebCore {
class ResourceRequest;
}

namespace WebKit {
namespace NetworkCache {

class Statistics {
public:
    static std::unique_ptr<Statistics> open(Cache&, const String& cachePath);
    explicit Statistics(Cache&, const String& databasePath);

    void clear();

    void recordRetrievalRequest(uint64_t webPageID);
    void recordNotCachingResponse(const Key&, StoreDecision);
    void recordNotUsingCacheForRequest(uint64_t webPageID, const Key&, const WebCore::ResourceRequest&, RetrieveDecision);
    void recordRetrievalFailure(uint64_t webPageID, const Key&, const WebCore::ResourceRequest&);
    void recordRetrievedCachedEntry(uint64_t webPageID, const Key&, const WebCore::ResourceRequest&, UseDecision);
    void recordRevalidationSuccess(uint64_t webPageID, const Key&, const WebCore::ResourceRequest&);

private:
    WorkQueue& serialBackgroundIOQueue() { return m_serialBackgroundIOQueue.get(); }

    void initialize(const String& databasePath);
    void bootstrapFromNetworkCache(const String& networkCachePath);
    void shrinkIfNeeded();

    void addHashesToDatabase(const HashSet<String>& hashes);
    void addStoreDecisionsToDatabase(const HashMap<String, NetworkCache::StoreDecision>&);
    void writeTimerFired();

    typedef Function<void (bool wasEverRequested, const Optional<StoreDecision>&)> RequestedCompletionHandler;
    enum class NeedUncachedReason { No, Yes };
    void queryWasEverRequested(const String&, NeedUncachedReason, RequestedCompletionHandler&&);
    void markAsRequested(const String& hash);

    struct EverRequestedQuery {
        String hash;
        bool needUncachedReason;
        RequestedCompletionHandler completionHandler;
    };

    Cache& m_cache;

    std::atomic<size_t> m_approximateEntryCount { 0 };

    mutable Ref<WorkQueue> m_serialBackgroundIOQueue;
    mutable HashSet<std::unique_ptr<const EverRequestedQuery>> m_activeQueries;
    WebCore::SQLiteDatabase m_database;
    HashSet<String> m_hashesToAdd;
    HashMap<String, NetworkCache::StoreDecision> m_storeDecisionsToAdd;
    WebCore::Timer m_writeTimer;
};

}
}

#endif // NetworkCacheStatistics_h
