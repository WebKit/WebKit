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

#include "NetworkCacheIOChannel.h"
#include "NetworkProcess.h"
#include <WebCore/CacheQueryOptions.h>
#include <pal/SessionID.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringHash.h>

using namespace WebCore::DOMCache;
using namespace WebKit::NetworkCache;

namespace WebKit {

namespace CacheStorage {

static HashMap<PAL::SessionID, RefPtr<Engine>>& globalEngineMap()
{
    static NeverDestroyed<HashMap<PAL::SessionID, RefPtr<Engine>>> map;

    return map;
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

void Engine::open(const String& origin, const String& cacheName, CacheIdentifierCallback&& callback)
{
    readCachesFromDisk(origin, [this, cacheName, callback = WTFMove(callback)](CachesOrError&& cachesOrError) mutable {
        if (!cachesOrError.hasValue()) {
            callback(makeUnexpected(cachesOrError.error()));
            return;
        }

        auto& caches = cachesOrError.value().get();

        auto position = caches.findMatching([&](const auto& item) { return item.name == cacheName; });
        if (position == notFound) {
            uint64_t cacheIdentifier = ++m_nextCacheIdentifier;
            caches.append(Cache { cacheIdentifier, cacheName, Vector<Record>(), 0 });
            writeCachesToDisk([cacheIdentifier, callback = WTFMove(callback)](std::optional<Error>&& error) {
                if (error) {
                    callback(makeUnexpected(error.value()));
                    return;
                }
                callback(cacheIdentifier);
            });
        } else
            callback(caches[position].identifier);

    });
}

void Engine::remove(uint64_t cacheIdentifier, CacheIdentifierCallback&& callback)
{
    std::optional<Cache> removedCache;
    for (auto& caches : m_caches.values()) {
        auto position = caches.findMatching([&](const auto& item) { return item.identifier == cacheIdentifier; });
        if (position != notFound) {
            removedCache = WTFMove(caches[position]);
            caches.remove(position);
            break;
        }
    }
    if (!removedCache) {
        callback(makeUnexpected(Error::Internal));
        return;
    }
    m_removedCaches.append(WTFMove(removedCache.value()));
    writeCachesToDisk([cacheIdentifier, callback = WTFMove(callback)](std::optional<Error>&& error) {
        if (error) {
            callback(makeUnexpected(error.value()));
            return;
        }
        callback(cacheIdentifier);
    });
}

void Engine::retrieveCaches(const String& origin, CacheInfosCallback&& callback)
{
    readCachesFromDisk(origin, [callback = WTFMove(callback)](CachesOrError&& cachesOrError) mutable {
        if (!cachesOrError.hasValue()) {
            callback(makeUnexpected(cachesOrError.error()));
            return;
        }

        auto& caches = cachesOrError.value().get();

        Vector<CacheInfo> cachesInfo;
        cachesInfo.reserveInitialCapacity(caches.size());
        for (auto& cache : caches)
            cachesInfo.uncheckedAppend(CacheInfo { cache.identifier, cache.name});

        callback(WTFMove(cachesInfo));
    });
}

void Engine::retrieveRecords(uint64_t cacheIdentifier, RecordsCallback&& callback)
{
    readCache(cacheIdentifier, [callback = WTFMove(callback)](CacheOrError&& result) mutable {
        if (!result.hasValue()) {
            callback(makeUnexpected(result.error()));
            return;
        }
        // FIXME: Pass records by reference.
        auto& records = result.value().get().records;

        Vector<Record> copy;
        copy.reserveInitialCapacity(records.size());
        for (auto& record : result.value().get().records)
            copy.uncheckedAppend(record.copy());

        callback(WTFMove(copy));
    });
}

void Engine::putRecords(uint64_t cacheIdentifier, Vector<Record>&& records, RecordIdentifiersCallback&& callback)
{
    readCache(cacheIdentifier, [this, cacheIdentifier, records = WTFMove(records), callback = WTFMove(callback)](CacheOrError&& result) mutable {
        if (!result.hasValue()) {
            callback(makeUnexpected(result.error()));
            return;
        }

        Cache& cache = result.value();

        WebCore::CacheQueryOptions options;
        Vector<uint64_t> recordIdentifiers;
        recordIdentifiers.reserveInitialCapacity(records.size());
        for (auto& record : records) {
            auto matchingRecords = Engine::queryCache(cache.records, record.request, options);
            if (matchingRecords.isEmpty()) {
                record.identifier = ++cache.nextRecordIdentifier;
                recordIdentifiers.uncheckedAppend(record.identifier);
                cache.records.append(WTFMove(record));
            } else {
                auto identifier = matchingRecords[0];
                auto position = cache.records.findMatching([&](const auto& item) { return item.identifier == identifier; });
                ASSERT(position != notFound);
                if (position != notFound) {
                    auto& existingRecord = cache.records[position];
                    recordIdentifiers.uncheckedAppend(identifier);
                    existingRecord.responseHeadersGuard = record.responseHeadersGuard;
                    existingRecord.response = WTFMove(record.response);
                    existingRecord.responseBody = WTFMove(record.responseBody);
                    ++existingRecord.updateResponseCounter;
                }
            }
        }
        writeCacheRecords(cacheIdentifier, WTFMove(recordIdentifiers), [callback = WTFMove(callback)](RecordIdentifiersOrError&& result) mutable {
            callback(WTFMove(result));
        });
    });
}

void Engine::deleteMatchingRecords(uint64_t cacheIdentifier, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, RecordIdentifiersCallback&& callback)
{
    readCache(cacheIdentifier, [this, cacheIdentifier, request = WTFMove(request), options = WTFMove(options), callback = WTFMove(callback)](CacheOrError&& result) mutable {
        if (!result.hasValue()) {
            callback(makeUnexpected(result.error()));
            return;
        }

        auto& currentRecords = result.value().get().records;

        auto recordsToRemove = queryCache(currentRecords, request, options);
        if (recordsToRemove.isEmpty()) {
            callback({ });
            return;
        }

        Vector<Record> recordsToKeep;
        for (auto& record : currentRecords) {
            if (recordsToRemove.findMatching([&](auto item) { return item == record.identifier; }) == notFound)
                recordsToKeep.append(record.copy());
        }
        removeCacheRecords(cacheIdentifier, WTFMove(recordsToRemove), [this, cacheIdentifier, recordsToKeep = WTFMove(recordsToKeep), callback = WTFMove(callback)](RecordIdentifiersOrError&& result) mutable {
            if (!result.hasValue()) {
                callback(makeUnexpected(result.error()));
                return;
            }

            auto* writtenCache = cache(cacheIdentifier);
            if (!writtenCache) {
                callback(makeUnexpected(Error::Internal));
                return;
            }
            writtenCache->records = WTFMove(recordsToKeep);

            callback(WTFMove(result.value()));
        });
    });
}

void Engine::writeCachesToDisk(Function<void(std::optional<Error>&&)>&& callback)
{
    // FIXME: Implement writing.
    callback(std::nullopt);
}

void Engine::readCachesFromDisk(const String& origin, CachesCallback&& callback)
{
    // FIXME: Implement reading.

    auto& caches = m_caches.ensure(origin, [] {
        return Vector<Cache>();
    }).iterator->value;

    callback(std::reference_wrapper<Vector<Cache>> { caches });
}

void Engine::readCache(uint64_t cacheIdentifier, CacheCallback&& callback)
{
    // FIXME: Implement reading.
    auto* cache = this->cache(cacheIdentifier);
    if (!cache) {
        callback(makeUnexpected(Error::Internal));
        return;
    }
    callback(std::reference_wrapper<Cache> { *cache });
}

void Engine::writeCacheRecords(uint64_t cacheIdentifier, Vector<uint64_t>&& recordsIdentifiers, RecordIdentifiersCallback&& callback)
{
    // FIXME: Implement writing.
    callback(WTFMove(recordsIdentifiers));
}

void Engine::removeCacheRecords(uint64_t cacheIdentifier, Vector<uint64_t>&& recordsIdentifiers, RecordIdentifiersCallback&& callback)
{
    // FIXME: Implement writing.
    callback(WTFMove(recordsIdentifiers));
}

Cache* Engine::cache(uint64_t cacheIdentifier)
{
    Cache* result = nullptr;
    for (auto& caches : m_caches.values()) {
        auto position = caches.findMatching([&](const auto& item) { return item.identifier == cacheIdentifier; });
        if (position != notFound) {
            result = &caches[position];
            break;
        }
    }
    if (!result) {
        auto position = m_removedCaches.findMatching([&](const auto& item) { return item.identifier == cacheIdentifier; });
        if (position != notFound)
            result = &m_removedCaches[position];
    }
    return result;
}

Vector<uint64_t> Engine::queryCache(const Vector<Record>& records, const WebCore::ResourceRequest& request, const WebCore::CacheQueryOptions& options)
{
    if (!options.ignoreMethod && request.httpMethod() != "GET")
        return { };

    Vector<uint64_t> results;
    for (const auto& record : records) {
        if (WebCore::DOMCache::queryCacheMatch(request, record.request, record.response, options))
            results.append(record.identifier);
    }
    return results;
}

void Engine::writeFile(const String& filename, NetworkCache::Data&& data, WebCore::DOMCache::CompletionCallback&& callback)
{
    if (!shouldPersist()) {
        callback(std::nullopt);
        return;
    }

    m_ioQueue->dispatch([this, protectedThis = makeRef(*this), callback = WTFMove(callback), data = WTFMove(data), filename = filename.isolatedCopy()] () mutable {
        auto channel = IOChannel::open(filename, IOChannel::Type::Create);
        channel->write(0, data, m_ioQueue.get(), [callback = WTFMove(callback)](int error) mutable {
            RunLoop::main().dispatch([callback = WTFMove(callback), error]() mutable {
                if (error) {
                    // FIXME: Use specific filesystem error.
                    callback(Error::Internal);
                    return;
                }
                callback(std::nullopt);
            });
        });
    });
}

void Engine::readFile(const String& filename, WTF::Function<void(const NetworkCache::Data&, int error)>&& callback)
{
    if (!shouldPersist()) {
        callback(Data { }, 0);
        return;
    }

    m_ioQueue->dispatch([this, protectedThis = makeRef(*this), callback = WTFMove(callback), filename = filename.isolatedCopy()]() mutable {
        auto channel = IOChannel::open(filename, IOChannel::Type::Read);
        if (channel->fileDescriptor() < 0) {
            RunLoop::main().dispatch([callback = WTFMove(callback)]() mutable {
                callback(Data { }, 0);
            });
            return;
        }

        channel->read(0, std::numeric_limits<size_t>::max(), m_ioQueue.get(), [callback = WTFMove(callback)](const Data& data, int error) mutable {
            RunLoop::main().dispatch([callback = WTFMove(callback), data, error]() mutable {
                callback(data, error);
            });
        });
    });
}

void Engine::removeFile(const String& filename)
{
    if (!shouldPersist())
        return;

    m_ioQueue->dispatch([filename = filename.isolatedCopy()]() mutable {
        WebCore::deleteFile(filename);
    });
}

} // namespace CacheStorage

} // namespace WebKit
