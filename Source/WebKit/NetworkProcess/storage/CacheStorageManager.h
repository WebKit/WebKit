/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include <WebCore/DOMCacheEngine.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
struct ClientOrigin;
}

namespace WebKit {
class CacheStorageCache;
class CacheStorageRegistry;
class CacheStorageStore;
struct CacheStorageRecord;
struct CacheStorageRecordInformation;


class CacheStorageManager : public CanMakeWeakPtr<CacheStorageManager> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static FileSystem::Salt cacheStorageSalt(const String& rootDirectory);
    static String cacheStorageOriginDirectory(const String& rootDirectory, FileSystem::Salt, const WebCore::ClientOrigin&);
    static HashSet<WebCore::ClientOrigin> originsOfCacheStorageData(const String& rootDirectory);
    static uint64_t cacheStorageSize(const String& originDirectory);
    static bool hasCacheList(const String& cacheListDirectory);

    using QuotaCheckFunction = Function<void(uint64_t spaceRequested, CompletionHandler<void(bool)>&&)>;
    CacheStorageManager(const String& path, FileSystem::Salt, CacheStorageRegistry&, const WebCore::ClientOrigin&, QuotaCheckFunction&&, Ref<WorkQueue>&&);
    ~CacheStorageManager();
    void openCache(const String& name, WebCore::DOMCacheEngine::CacheIdentifierCallback&&);
    void removeCache(WebCore::DOMCacheIdentifier, WebCore::DOMCacheEngine::RemoveCacheIdentifierCallback&&);
    void allCaches(uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&&);
    void reference(IPC::Connection::UniqueID, WebCore::DOMCacheIdentifier);
    void dereference(IPC::Connection::UniqueID, WebCore::DOMCacheIdentifier);

    void connectionClosed(IPC::Connection::UniqueID);
    bool hasDataInMemory();
    bool isActive();
    String representationString();
    FileSystem::Salt salt() const { return m_salt; }
    void requestSpace(uint64_t size, CompletionHandler<void(bool)>&&);
    void sizeIncreased(uint64_t amount);
    void sizeDecreased(uint64_t amount);

private:
    void makeDirty();
    bool initializeCaches();
    void removeUnusedCache(WebCore::DOMCacheIdentifier);
    void initializeCacheSize(CacheStorageCache&);
    void finishInitializingSize();
    void requestSpaceAfterInitializingSize(uint64_t size, CompletionHandler<void(bool)>&&);

    bool m_isInitialized { false };
    uint64_t m_updateCounter;
    std::optional<uint64_t> m_size;
    std::pair<uint64_t, size_t> m_pendingSize;
    String m_path;
    FileSystem::Salt m_salt;
    CacheStorageRegistry& m_registry;
    QuotaCheckFunction m_quotaCheckFunction;
    Vector<std::unique_ptr<CacheStorageCache>> m_caches;
    HashMap<WebCore::DOMCacheIdentifier, std::unique_ptr<CacheStorageCache>> m_removedCaches;
    HashMap<WebCore::DOMCacheIdentifier, Vector<IPC::Connection::UniqueID>> m_cacheRefConnections;
    Ref<WorkQueue> m_queue;
    Deque<std::pair<uint64_t, CompletionHandler<void(bool)>>> m_pendingSpaceRequests;
};

} // namespace WebKit
