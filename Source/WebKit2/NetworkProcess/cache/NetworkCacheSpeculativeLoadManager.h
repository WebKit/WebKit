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

#ifndef NetworkCacheSpeculativeLoadManager_h
#define NetworkCacheSpeculativeLoadManager_h

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)

#include "NetworkCache.h"
#include "NetworkCacheStorage.h"
#include <WebCore/ResourceRequest.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebKit {

namespace NetworkCache {

class Entry;
class SpeculativeLoad;

class SpeculativeLoadManager {
public:
    explicit SpeculativeLoadManager(Storage&);
    ~SpeculativeLoadManager();

    void registerLoad(const GlobalFrameID&, const WebCore::ResourceRequest&, const Key& resourceKey);

    typedef std::function<void (std::unique_ptr<Entry>)> RetrieveCompletionHandler;
    bool retrieve(const Key& storageKey, const RetrieveCompletionHandler&);

    void startSpeculativeRevalidation(const WebCore::ResourceRequest&, const GlobalFrameID&, const Key& storageKey);

private:
    void addPreloadedEntry(std::unique_ptr<Entry>);
    void preloadEntry(const Key&, const GlobalFrameID&);
    void retrieveEntryFromStorage(const Key&, const RetrieveCompletionHandler&);
    void revalidateEntry(std::unique_ptr<Entry>, const GlobalFrameID&);
    bool satisfyPendingRequests(const Key&, Entry*);

    Storage& m_storage;

    class PendingFrameLoad;
    HashMap<GlobalFrameID, std::unique_ptr<PendingFrameLoad>> m_pendingFrameLoads;

    HashMap<Key, std::unique_ptr<SpeculativeLoad>> m_pendingPreloads;
    HashMap<Key, std::unique_ptr<Vector<RetrieveCompletionHandler>>> m_pendingRetrieveRequests;

    class PreloadedEntry;
    HashMap<Key, std::unique_ptr<PreloadedEntry>> m_preloadedEntries;
};

} // namespace NetworkCache

} // namespace WebKit

#endif // ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)

#endif // NetworkCacheSpeculativeLoadManager_h
