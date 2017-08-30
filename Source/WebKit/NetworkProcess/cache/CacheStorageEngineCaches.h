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
    static Ref<Caches> create(Engine& engine, const String& origin) { return adoptRef(*new Caches { engine, origin }); }

    void initialize(WebCore::DOMCache::CompletionCallback&&);

    bool isInitialized() const { return m_isInitialized; }

    Cache* find(const String& name);
    Cache* find(uint64_t identifier);

    void open(String&& name, WebCore::DOMCache::CacheIdentifierCallback&&);
    void remove(uint64_t identifier, WebCore::DOMCache::CompletionCallback&&);

    Vector<WebCore::DOMCache::CacheInfo> cacheInfos() const;

    void clearMemoryRepresentation();
    void detach() { m_engine = nullptr; }

private:
    Caches(Engine&, const String& rootPath);

    void readCachesFromDisk(WTF::Function<void(Expected<Vector<Cache>, WebCore::DOMCache::Error>&&)>&&);
    void writeCachesToDisk(WebCore::DOMCache::CompletionCallback&&);

    bool m_isInitialized { false };
    Engine* m_engine { nullptr };
    String m_rootPath;
    Vector<Cache> m_caches;
    Vector<Cache> m_removedCaches;
    RefPtr<NetworkCache::Storage> m_storage;
    Vector<WebCore::DOMCache::CompletionCallback> m_pendingInitializationCallbacks;
};

} // namespace CacheStorage

} // namespace WebKit
