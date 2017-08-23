/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "CacheStorageEngineCache.h"
#include <wtf/HashMap.h>

namespace IPC {
class Connection;
}

namespace WebCore {
class SessionID;
}

namespace WebKit {

namespace CacheStorage {

class Engine {
public:
    static Engine& from(PAL::SessionID);
    static void destroyEngine(PAL::SessionID);

    void open(const String& origin, const String& cacheName, WebCore::DOMCache::CacheIdentifierCallback&&);
    void remove(uint64_t cacheIdentifier, WebCore::DOMCache::CacheIdentifierCallback&&);
    void retrieveCaches(const String& origin, WebCore::DOMCache::CacheInfosCallback&&);

    void retrieveRecords(uint64_t cacheIdentifier, WebCore::DOMCache::RecordsCallback&&);
    void putRecords(uint64_t cacheIdentifier, Vector<WebCore::DOMCache::Record>&&, WebCore::DOMCache::RecordIdentifiersCallback&&);
    void deleteMatchingRecords(uint64_t cacheIdentifier, WebCore::ResourceRequest&&, WebCore::CacheQueryOptions&&, WebCore::DOMCache::RecordIdentifiersCallback&&);

private:
    static Engine& defaultEngine();

    void writeCachesToDisk(WebCore::DOMCache::CompletionCallback&&);

    using CachesOrError = Expected<std::reference_wrapper<Vector<Cache>>, WebCore::DOMCache::Error>;
    using CachesCallback = WTF::Function<void(CachesOrError&&)>;
    void readCachesFromDisk(const String& origin, CachesCallback&&);

    using CacheOrError = Expected<std::reference_wrapper<Cache>, WebCore::DOMCache::Error>;
    using CacheCallback = WTF::Function<void(CacheOrError&&)>;
    void readCache(uint64_t cacheIdentifier, CacheCallback&&);

    void writeCacheRecords(uint64_t cacheIdentifier, Vector<uint64_t>&&, WebCore::DOMCache::RecordIdentifiersCallback&&);
    void removeCacheRecords(uint64_t cacheIdentifier, Vector<uint64_t>&&, WebCore::DOMCache::RecordIdentifiersCallback&&);

    Vector<uint64_t> queryCache(const Vector<WebCore::DOMCache::Record>&, const WebCore::ResourceRequest&, const WebCore::CacheQueryOptions&);

    Cache* cache(uint64_t cacheIdentifier);

    HashMap<String, Vector<Cache>> m_caches;
    Vector<Cache> m_removedCaches;
    uint64_t m_nextCacheIdentifier { 0 };
};

} // namespace CacheStorage

} // namespace WebKit
