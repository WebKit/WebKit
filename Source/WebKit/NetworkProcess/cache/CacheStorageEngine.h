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

#include "CacheStorageEngineCaches.h"
#include "NetworkCacheData.h"
#include "WebsiteData.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/DOMCacheIdentifier.h>
#include <pal/SessionID.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>

namespace IPC {
class Connection;
}

namespace WebCore {
struct RetrieveRecordsOptions;
}

namespace WebKit {

class NetworkProcess;
class NetworkSession;

namespace CacheStorage {

using CacheIdentifier = WebCore::DOMCacheIdentifier;
using LockCount = uint64_t;

class Engine : public RefCounted<Engine>, public CanMakeWeakPtr<Engine> {
public:
    static Ref<Engine> create(NetworkSession&, const String& rootPath);
    ~Engine();

    static void fetchEntries(NetworkSession&, bool shouldComputeSize, CompletionHandler<void(Vector<WebsiteData::Entry>)>&&);

    static void open(NetworkSession&, WebCore::ClientOrigin&&, String&& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&&);
    static void remove(NetworkSession&, WebCore::DOMCacheIdentifier, WebCore::DOMCacheEngine::RemoveCacheIdentifierCallback&&);
    static void retrieveCaches(NetworkSession&, WebCore::ClientOrigin&&, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&&);

    static void retrieveRecords(NetworkSession&, WebCore::DOMCacheIdentifier, WebCore::RetrieveRecordsOptions&&, WebCore::DOMCacheEngine::RecordsCallback&&);
    static void putRecords(NetworkSession&, WebCore::DOMCacheIdentifier, Vector<WebCore::DOMCacheEngine::Record>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    static void deleteMatchingRecords(NetworkSession&, WebCore::DOMCacheIdentifier, WebCore::ResourceRequest&&, WebCore::CacheQueryOptions&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);

    static void lock(NetworkSession&, WebCore::DOMCacheIdentifier);
    static void unlock(NetworkSession&, WebCore::DOMCacheIdentifier);

    static void clearMemoryRepresentation(NetworkSession&, WebCore::ClientOrigin&&, WebCore::DOMCacheEngine::CompletionCallback&&);
    static void representation(NetworkSession&, CompletionHandler<void(String&&)>&&);

    static void clearAllCaches(NetworkSession&, CompletionHandler<void()>&&);
    static void clearCachesForOrigin(NetworkSession&, WebCore::SecurityOriginData&&, CompletionHandler<void()>&&);

    static void initializeQuotaUser(NetworkSession&, const WebCore::ClientOrigin&, CompletionHandler<void()>&&);

    static String storagePath(const String& rootDirectory, const WebCore::ClientOrigin&);
    static uint64_t diskUsage(const String& originDirectory);
    void requestSpace(const WebCore::ClientOrigin&, uint64_t spaceRequested, CompletionHandler<void(bool)>&&);

    bool shouldPersist() const { return !!m_ioQueue;}

    void writeFile(String&& filename, NetworkCache::Data&&, WebCore::DOMCacheEngine::CompletionCallback&&);
    void readFile(String&& filename, CompletionHandler<void(const NetworkCache::Data&, int error)>&&);
    void removeFile(String&& filename);
    void writeSizeFile(String&&, uint64_t size, CompletionHandler<void()>&&);
    static std::optional<uint64_t> readSizeFile(const String&);

    const String& rootPath() const { return m_rootPath; }
    const NetworkCache::Salt& salt() const { return m_salt.value(); }

private:
    Engine(NetworkSession&, const String& rootPath);

    void open(const WebCore::ClientOrigin&, const String& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&&);
    void remove(WebCore::DOMCacheIdentifier, WebCore::DOMCacheEngine::RemoveCacheIdentifierCallback&&);
    void retrieveCaches(const WebCore::ClientOrigin&, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&&);

    void clearAllCaches(CompletionHandler<void()>&&);
    void clearAllCachesFromDisk(CompletionHandler<void()>&&);
    void clearCachesForOrigin(const WebCore::SecurityOriginData&, CompletionHandler<void()>&&);
    void clearCachesForOriginFromDisk(const WebCore::SecurityOriginData&, CompletionHandler<void()>&&);
    void deleteNonEmptyDirectoryOnBackgroundThread(String&& path, CompletionHandler<void()>&&);

    void clearMemoryRepresentation(const WebCore::ClientOrigin&, WebCore::DOMCacheEngine::CompletionCallback&&);
    String representation();

    void retrieveRecords(WebCore::DOMCacheIdentifier, WebCore::RetrieveRecordsOptions&&, WebCore::DOMCacheEngine::RecordsCallback&&);
    void putRecords(WebCore::DOMCacheIdentifier, Vector<WebCore::DOMCacheEngine::Record>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void deleteMatchingRecords(WebCore::DOMCacheIdentifier, WebCore::ResourceRequest&&, WebCore::CacheQueryOptions&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);

    void lock(CacheIdentifier);
    void unlock(CacheIdentifier);

    String cachesRootPath(const WebCore::ClientOrigin&);

    void fetchEntries(bool /* shouldComputeSize */, CompletionHandler<void(Vector<WebsiteData::Entry>)>&&);

    void getDirectories(CompletionHandler<void(const Vector<String>&)>&&);
    void fetchDirectoryEntries(bool shouldComputeSize, const Vector<String>& folderPaths, CompletionHandler<void(Vector<WebsiteData::Entry>)>&&);
    void clearCachesForOriginFromDirectories(const Vector<String>&, const WebCore::SecurityOriginData&, CompletionHandler<void()>&&);

    void initialize(WebCore::DOMCacheEngine::CompletionCallback&&);

    using CachesOrError = Expected<std::reference_wrapper<Caches>, WebCore::DOMCacheEngine::Error>;
    using CachesCallback = Function<void(CachesOrError&&)>;
    void readCachesFromDisk(const WebCore::ClientOrigin&, CachesCallback&&);

    using CacheOrError = Expected<std::reference_wrapper<Cache>, WebCore::DOMCacheEngine::Error>;
    using CacheCallback = Function<void(CacheOrError&&)>;
    void readCache(WebCore::DOMCacheIdentifier, CacheCallback&&);

    CompletionHandler<void()> createClearTask(CompletionHandler<void()>&&);

    Cache* cache(WebCore::DOMCacheIdentifier);

    PAL::SessionID m_sessionID;
    WeakPtr<NetworkProcess> m_networkProcess;
    HashMap<WebCore::ClientOrigin, RefPtr<Caches>> m_caches;
    String m_rootPath;
    RefPtr<WorkQueue> m_ioQueue;
    std::optional<NetworkCache::Salt> m_salt;
    HashMap<CacheIdentifier, LockCount> m_cacheLocks;
    Vector<WebCore::DOMCacheEngine::CompletionCallback> m_initializationCallbacks;
    HashMap<uint64_t, WebCore::DOMCacheEngine::CompletionCallback> m_pendingWriteCallbacks;
    HashMap<uint64_t, CompletionHandler<void(const NetworkCache::Data&, int error)>> m_pendingReadCallbacks;
    uint64_t m_pendingCallbacksCounter { 0 };
    Vector<WebCore::DOMCacheEngine::CompletionCallback> m_pendingClearCallbacks;
    uint64_t m_clearTaskCounter { 0 };
};

} // namespace CacheStorage

} // namespace WebKit
