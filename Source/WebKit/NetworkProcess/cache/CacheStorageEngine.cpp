/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CacheStorageEngine.h"

#include "NetworkCacheFileSystem.h"
#include "NetworkCacheIOChannel.h"
#include "NetworkProcess.h"
#include "WebsiteDataType.h"
#include <WebCore/CacheQueryOptions.h>
#include <WebCore/SecurityOrigin.h>
#include <pal/SessionID.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

using namespace WebCore::DOMCacheEngine;
using namespace WebKit::NetworkCache;

namespace WebKit {

namespace CacheStorage {

static HashMap<PAL::SessionID, RefPtr<Engine>>& globalEngineMap()
{
    static NeverDestroyed<HashMap<PAL::SessionID, RefPtr<Engine>>> map;

    return map;
}

String Engine::cachesRootPath(const WebCore::ClientOrigin& origin)
{
    if (!shouldPersist())
        return { };

    Key key(origin.topOrigin.toString(), origin.clientOrigin.toString(), { }, { }, salt());
    return WebCore::FileSystem::pathByAppendingComponent(rootPath(), key.hashAsString());
}

Engine::~Engine()
{
    for (auto& caches : m_caches.values())
        caches->detach();
}

Engine& Engine::from(PAL::SessionID sessionID)
{
    auto addResult = globalEngineMap().add(sessionID, nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = Engine::create(NetworkProcess::singleton().cacheStorageDirectory(sessionID));
    return *addResult.iterator->value;
}

void Engine::destroyEngine(PAL::SessionID sessionID)
{
    ASSERT(sessionID != PAL::SessionID::defaultSessionID());
    globalEngineMap().remove(sessionID);
}

void Engine::fetchEntries(PAL::SessionID sessionID, bool shouldComputeSize, WTF::CompletionHandler<void(Vector<WebsiteData::Entry>)>&& completionHandler)
{
    from(sessionID).fetchEntries(shouldComputeSize, WTFMove(completionHandler));
}

Engine& Engine::defaultEngine()
{
    auto sessionID = PAL::SessionID::defaultSessionID();
    static NeverDestroyed<Ref<Engine>> defaultEngine = { Engine::create(NetworkProcess::singleton().cacheStorageDirectory(sessionID)) };
    return defaultEngine.get();
}

Engine::Engine(String&& rootPath)
    : m_rootPath(WTFMove(rootPath))
{
    if (!m_rootPath.isNull())
        m_ioQueue = WorkQueue::create("com.apple.WebKit.CacheStorageEngine.serialBackground", WorkQueue::Type::Serial, WorkQueue::QOS::Background);
}

void Engine::open(const WebCore::ClientOrigin& origin, const String& cacheName, CacheIdentifierCallback&& callback)
{
    readCachesFromDisk(origin, [cacheName, callback = WTFMove(callback)](CachesOrError&& cachesOrError) mutable {
        if (!cachesOrError.has_value()) {
            callback(makeUnexpected(cachesOrError.error()));
            return;
        }

        cachesOrError.value().get().open(cacheName, WTFMove(callback));
    });
}

void Engine::remove(uint64_t cacheIdentifier, CacheIdentifierCallback&& callback)
{
    Caches* cachesToModify = nullptr;
    for (auto& caches : m_caches.values()) {
        auto* cacheToRemove = caches->find(cacheIdentifier);
        if (cacheToRemove) {
            cachesToModify = caches.get();
            break;
        }
    }
    if (!cachesToModify) {
        callback(makeUnexpected(Error::Internal));
        return;
    }

    cachesToModify->remove(cacheIdentifier, WTFMove(callback));
}

void Engine::retrieveCaches(const WebCore::ClientOrigin& origin, uint64_t updateCounter, CacheInfosCallback&& callback)
{
    readCachesFromDisk(origin, [updateCounter, callback = WTFMove(callback)](CachesOrError&& cachesOrError) mutable {
        if (!cachesOrError.has_value()) {
            callback(makeUnexpected(cachesOrError.error()));
            return;
        }

        cachesOrError.value().get().cacheInfos(updateCounter, WTFMove(callback));
    });
}

void Engine::retrieveRecords(uint64_t cacheIdentifier, WebCore::URL&& url, RecordsCallback&& callback)
{
    readCache(cacheIdentifier, [url = WTFMove(url), callback = WTFMove(callback)](CacheOrError&& result) mutable {
        if (!result.has_value()) {
            callback(makeUnexpected(result.error()));
            return;
        }
        result.value().get().retrieveRecords(url, WTFMove(callback));
    });
}

void Engine::putRecords(uint64_t cacheIdentifier, Vector<Record>&& records, RecordIdentifiersCallback&& callback)
{
    readCache(cacheIdentifier, [records = WTFMove(records), callback = WTFMove(callback)](CacheOrError&& result) mutable {
        if (!result.has_value()) {
            callback(makeUnexpected(result.error()));
            return;
        }

        result.value().get().put(WTFMove(records), WTFMove(callback));
    });
}

void Engine::deleteMatchingRecords(uint64_t cacheIdentifier, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, RecordIdentifiersCallback&& callback)
{
    readCache(cacheIdentifier, [request = WTFMove(request), options = WTFMove(options), callback = WTFMove(callback)](CacheOrError&& result) mutable {
        if (!result.has_value()) {
            callback(makeUnexpected(result.error()));
            return;
        }

        result.value().get().remove(WTFMove(request), WTFMove(options), WTFMove(callback));
    });
}

void Engine::initialize(Function<void(std::optional<Error>&&)>&& callback)
{
    if (m_salt) {
        callback(std::nullopt);
        return;
    }

    if (!shouldPersist()) {
        callback(std::nullopt);
        return;
    }

    String saltPath = WebCore::FileSystem::pathByAppendingComponent(m_rootPath, ASCIILiteral("salt"));
    m_ioQueue->dispatch([protectedThis = makeRef(*this), this, callback = WTFMove(callback), saltPath = WTFMove(saltPath)] () mutable {
        WebCore::FileSystem::makeAllDirectories(m_rootPath);
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), this, salt = readOrMakeSalt(saltPath), callback = WTFMove(callback)]() mutable {
            if (!salt) {
                callback(Error::WriteDisk);
                return;
            }
            m_salt = WTFMove(salt);
            callback(std::nullopt);
        });
    });
}

void Engine::readCachesFromDisk(const WebCore::ClientOrigin& origin, CachesCallback&& callback)
{
    initialize([this, origin, callback = WTFMove(callback)](std::optional<Error>&& error) mutable {
        auto& caches = m_caches.ensure(origin, [&origin, this] {
            auto path = cachesRootPath(origin);
            return Caches::create(*this, WebCore::ClientOrigin { origin }, WTFMove(path), NetworkProcess::singleton().cacheStoragePerOriginQuota());
        }).iterator->value;

        if (caches->isInitialized()) {
            callback(std::reference_wrapper<Caches> { *caches });
            return;
        }

        if (error) {
            callback(makeUnexpected(error.value()));
            return;
        }

        caches->initialize([callback = WTFMove(callback), caches = caches.copyRef()](std::optional<Error>&& error) mutable {
            if (error) {
                callback(makeUnexpected(error.value()));
                return;
            }

            callback(std::reference_wrapper<Caches> { *caches });
        });
    });
}

void Engine::readCache(uint64_t cacheIdentifier, CacheCallback&& callback)
{
    auto* cache = this->cache(cacheIdentifier);
    if (!cache) {
        callback(makeUnexpected(Error::Internal));
        return;
    }
    if (!cache->isOpened()) {
        cache->open([this, protectedThis = makeRef(*this), cacheIdentifier, callback = WTFMove(callback)](std::optional<Error>&& error) mutable {
            if (error) {
                callback(makeUnexpected(error.value()));
                return;
            }

            auto* cache = this->cache(cacheIdentifier);
            if (!cache) {
                callback(makeUnexpected(Error::Internal));
                return;
            }
            ASSERT(cache->isOpened());
            callback(std::reference_wrapper<Cache> { *cache });
        });
        return;
    }
    callback(std::reference_wrapper<Cache> { *cache });
}

Cache* Engine::cache(uint64_t cacheIdentifier)
{
    Cache* result = nullptr;
    for (auto& caches : m_caches.values()) {
        if ((result = caches->find(cacheIdentifier)))
            break;
    }
    return result;
}

void Engine::writeFile(const String& filename, NetworkCache::Data&& data, WebCore::DOMCacheEngine::CompletionCallback&& callback)
{
    if (!shouldPersist()) {
        callback(std::nullopt);
        return;
    }

    m_ioQueue->dispatch([callback = WTFMove(callback), data = WTFMove(data), filename = filename.isolatedCopy()] () mutable {
        auto channel = IOChannel::open(filename, IOChannel::Type::Create);
        channel->write(0, data, nullptr, [callback = WTFMove(callback)](int error) mutable {
            ASSERT(RunLoop::isMain());
            if (error) {
                callback(Error::WriteDisk);
                return;
            }
            callback(std::nullopt);
        });
    });
}

void Engine::readFile(const String& filename, WTF::Function<void(const NetworkCache::Data&, int error)>&& callback)
{
    if (!shouldPersist()) {
        callback(Data { }, 0);
        return;
    }

    m_ioQueue->dispatch([callback = WTFMove(callback), filename = filename.isolatedCopy()]() mutable {
        auto channel = IOChannel::open(filename, IOChannel::Type::Read);
        if (channel->fileDescriptor() < 0) {
            RunLoop::main().dispatch([callback = WTFMove(callback)]() mutable {
                callback(Data { }, 0);
            });
            return;
        }

        channel->read(0, std::numeric_limits<size_t>::max(), nullptr, [callback = WTFMove(callback)](const Data& data, int error) mutable {
            // FIXME: We should do the decoding in the background thread.
            ASSERT(RunLoop::isMain());
            callback(data, error);
        });
    });
}

void Engine::removeFile(const String& filename)
{
    if (!shouldPersist())
        return;

    m_ioQueue->dispatch([filename = filename.isolatedCopy()]() mutable {
        WebCore::FileSystem::deleteFile(filename);
    });
}

class ReadOriginsTaskCounter : public RefCounted<ReadOriginsTaskCounter> {
public:
    static Ref<ReadOriginsTaskCounter> create(WTF::CompletionHandler<void(Vector<WebsiteData::Entry>)>&& callback)
    {
        return adoptRef(*new ReadOriginsTaskCounter(WTFMove(callback)));
    }

    ~ReadOriginsTaskCounter()
    {
        m_callback(WTFMove(m_entries));
    }

    void addOrigin(WebCore::SecurityOriginData&& origin, uint64_t size)
    {
        m_entries.append(WebsiteData::Entry { WTFMove(origin), WebsiteDataType::DOMCache, size });
    }

private:
    explicit ReadOriginsTaskCounter(WTF::CompletionHandler<void(Vector<WebsiteData::Entry>)>&& callback)
        : m_callback(WTFMove(callback))
    {
    }

    WTF::CompletionHandler<void(Vector<WebsiteData::Entry>)> m_callback;
    Vector<WebsiteData::Entry> m_entries;
};

void Engine::fetchEntries(bool shouldComputeSize, WTF::CompletionHandler<void(Vector<WebsiteData::Entry>)>&& completionHandler)
{
    if (!shouldPersist()) {
        auto entries = WTF::map(m_caches, [] (auto& pair) {
            return WebsiteData::Entry { pair.value->origin().clientOrigin, WebsiteDataType::DOMCache, 0 };
        });
        completionHandler(WTFMove(entries));
        return;
    }

    auto taskCounter = ReadOriginsTaskCounter::create(WTFMove(completionHandler));
    for (auto& folderPath : WebCore::FileSystem::listDirectory(m_rootPath, "*")) {
        if (!WebCore::FileSystem::fileIsDirectory(folderPath, WebCore::FileSystem::ShouldFollowSymbolicLinks::No))
            continue;
        Caches::retrieveOriginFromDirectory(folderPath, *m_ioQueue, [protectedThis = makeRef(*this), shouldComputeSize, taskCounter = taskCounter.copyRef()] (auto&& origin) mutable {
            ASSERT(RunLoop::isMain());
            if (!origin)
                return;

            if (!shouldComputeSize) {
                taskCounter->addOrigin(WTFMove(origin->topOrigin), 0);
                taskCounter->addOrigin(WTFMove(origin->clientOrigin), 0);
                return;
            }

            protectedThis->readCachesFromDisk(origin.value(), [origin = origin.value(), taskCounter = WTFMove(taskCounter)] (CachesOrError&& result) mutable {
                if (!result.has_value())
                    return;
                taskCounter->addOrigin(WTFMove(origin.topOrigin), 0);
                taskCounter->addOrigin(WTFMove(origin.clientOrigin), result.value().get().storageSize());
            });
        });
    }
}

void Engine::clearAllCaches(CallbackAggregator& taskHandler)
{
    for (auto& caches : m_caches.values())
        caches->clear([taskHandler = makeRef(taskHandler)] { });

    if (!shouldPersist())
        return;

    m_ioQueue->dispatch([path = m_rootPath.isolatedCopy(), taskHandler = makeRef(taskHandler)] {
        for (auto& filename : WebCore::FileSystem::listDirectory(path, "*")) {
            if (WebCore::FileSystem::fileIsDirectory(filename, WebCore::FileSystem::ShouldFollowSymbolicLinks::No))
                deleteDirectoryRecursively(filename);
        }
    });
}

void Engine::clearCachesForOrigin(const WebCore::SecurityOriginData& origin, CallbackAggregator& taskHandler)
{
    for (auto& keyValue : m_caches) {
        if (keyValue.key.topOrigin == origin || keyValue.key.clientOrigin == origin)
            keyValue.value->clear([taskHandler = makeRef(taskHandler)] { });
    }

    if (!shouldPersist())
        return;

    for (auto& folderPath : WebCore::FileSystem::listDirectory(m_rootPath, "*")) {
        if (!WebCore::FileSystem::fileIsDirectory(folderPath, WebCore::FileSystem::ShouldFollowSymbolicLinks::No))
            continue;
        Caches::retrieveOriginFromDirectory(folderPath, *m_ioQueue, [this, protectedThis = makeRef(*this), origin, taskHandler = makeRef(taskHandler), folderPath] (std::optional<WebCore::ClientOrigin>&& folderOrigin) mutable {
            if (!folderOrigin)
                return;
            if (folderOrigin->topOrigin != origin && folderOrigin->clientOrigin != origin)
                return;

            m_ioQueue->dispatch([path = folderPath.isolatedCopy(), taskHandler = WTFMove(taskHandler)] {
                deleteDirectoryRecursively(path);
            });
        });
    }
}

void Engine::clearMemoryRepresentation(const WebCore::ClientOrigin& origin, WebCore::DOMCacheEngine::CompletionCallback&& callback)
{
    readCachesFromDisk(origin, [callback = WTFMove(callback)](CachesOrError&& result) {
        if (!result.has_value()) {
            callback(result.error());
            return;
        }
        result.value().get().clearMemoryRepresentation();
        callback(std::nullopt);
    });
}

void Engine::lock(uint64_t cacheIdentifier)
{
    auto& counter = m_cacheLocks.ensure(cacheIdentifier, []() {
        return 0;
    }).iterator->value;

    ++counter;
}

void Engine::unlock(uint64_t cacheIdentifier)
{
    auto lockCount = m_cacheLocks.find(cacheIdentifier);
    if (lockCount == m_cacheLocks.end())
        return;

    ASSERT(lockCount->value);
    if (--lockCount->value)
        return;

    auto* cache = this->cache(cacheIdentifier);
    if (!cache)
        return;

    cache->dispose();
}

String Engine::representation()
{
    bool isFirst = true;
    StringBuilder builder;
    builder.append("[");
    for (auto& keyValue : m_caches) {
        if (!isFirst)
            builder.append(",");
        isFirst = false;

        builder.append("\n{ \"origin\" : { \"topOrigin\" : \"");
        builder.append(keyValue.key.topOrigin.toString());
        builder.append("\", \"clientOrigin\": \"");
        builder.append(keyValue.key.clientOrigin.toString());
        builder.append("\" }, \"caches\" : ");
        keyValue.value->appendRepresentation(builder);
        builder.append("}");
    }
    builder.append("\n]");
    return builder.toString();
}

} // namespace CacheStorage

} // namespace WebKit
