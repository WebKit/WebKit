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
#include "NetworkCacheStorage.h"

namespace WebKit {

namespace CacheStorage {

class Engine;

class Caches : public RefCounted<Caches> {
public:
    static Ref<Caches> create(Engine& engine, String&& origin) { return adoptRef(*new Caches { engine, WTFMove(origin) }); }

    void initialize(WebCore::DOMCacheEngine::CompletionCallback&&);
    void open(const String& name, WebCore::DOMCacheEngine::CacheIdentifierCallback&&);
    void remove(uint64_t identifier, WebCore::DOMCacheEngine::CacheIdentifierCallback&&);
    void clearMemoryRepresentation();
    void dispose(Cache&);

    void detach();

    bool isInitialized() const { return m_isInitialized; }
    WebCore::DOMCacheEngine::CacheInfos cacheInfos(uint64_t updateCounter) const;

    Cache* find(uint64_t identifier);
    void appendRepresentation(StringBuilder&) const;

    void readRecordsList(Cache&, NetworkCache::Storage::TraverseHandler&&);
    void readRecord(const NetworkCache::Key&, WTF::Function<void(Expected<WebCore::DOMCacheEngine::Record, WebCore::DOMCacheEngine::Error>&&)>&&);
    void writeRecord(const Cache&, const RecordInformation&, WebCore::DOMCacheEngine::Record&&, WebCore::DOMCacheEngine::CompletionCallback&&);
    void removeRecord(const NetworkCache::Key&);

    const NetworkCache::Salt& salt() const;

    bool shouldPersist() const { return !m_rootPath.isNull(); }

private:
    Caches(Engine&, String&& origin);

    void readCachesFromDisk(WTF::Function<void(Expected<Vector<Cache>, WebCore::DOMCacheEngine::Error>&&)>&&);
    void writeCachesToDisk(WebCore::DOMCacheEngine::CompletionCallback&&);

    Cache* find(const String& name);

    void makeDirty() { ++m_updateCounter; }
    bool isDirty(uint64_t updateCounter) const;

    bool m_isInitialized { false };
    Engine* m_engine { nullptr };
    uint64_t m_updateCounter { 0 };
    String m_origin;
    String m_rootPath;
    Vector<Cache> m_caches;
    Vector<Cache> m_removedCaches;
    RefPtr<NetworkCache::Storage> m_storage;
    HashMap<NetworkCache::Key, WebCore::DOMCacheEngine::Record> m_volatileStorage;
    mutable std::optional<NetworkCache::Salt> m_volatileSalt;
    Vector<WebCore::DOMCacheEngine::CompletionCallback> m_pendingInitializationCallbacks;
};

} // namespace CacheStorage

} // namespace WebKit
