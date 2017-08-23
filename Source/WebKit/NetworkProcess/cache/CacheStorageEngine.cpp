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

#include <WebCore/CacheQueryOptions.h>
#include <pal/SessionID.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringHash.h>

namespace WebKit {

static HashMap<PAL::SessionID, std::unique_ptr<CacheStorageEngine>>& globalEngineMap()
{
    static NeverDestroyed<HashMap<PAL::SessionID, std::unique_ptr<CacheStorageEngine>>> map;
    return map;
}

CacheStorageEngine& CacheStorageEngine::from(PAL::SessionID sessionID)
{
    if (sessionID == PAL::SessionID::defaultSessionID())
        return defaultEngine();

    return *globalEngineMap().ensure(sessionID, [] {
        return std::make_unique<CacheStorageEngine>();
    }).iterator->value;
}

void CacheStorageEngine::destroyEngine(PAL::SessionID sessionID)
{
    ASSERT(sessionID != PAL::SessionID::defaultSessionID());
    globalEngineMap().remove(sessionID);
}

CacheStorageEngine& CacheStorageEngine::defaultEngine()
{
    static NeverDestroyed<std::unique_ptr<CacheStorageEngine>> defaultEngine = { std::make_unique<CacheStorageEngine>() };
    return *defaultEngine.get();
}

void CacheStorageEngine::open(const String& origin, const String& cacheName, CacheIdentifierCallback&& callback)
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

void CacheStorageEngine::remove(uint64_t cacheIdentifier, CacheIdentifierCallback&& callback)
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

void CacheStorageEngine::retrieveCaches(const String& origin, CacheInfosCallback&& callback)
{
    readCachesFromDisk(origin, [this, callback = WTFMove(callback)](CachesOrError&& cachesOrError) mutable {
        if (!cachesOrError.hasValue()) {
            callback(makeUnexpected(cachesOrError.error()));
            return;
        }

        auto& caches = cachesOrError.value().get();

        Vector<WebCore::CacheStorageConnection::CacheInfo> cachesInfo;
        cachesInfo.reserveInitialCapacity(caches.size());
        for (auto& cache : caches)
            cachesInfo.uncheckedAppend(WebCore::CacheStorageConnection::CacheInfo { cache.identifier, cache.name});

        callback(WTFMove(cachesInfo));
    });
}

void CacheStorageEngine::retrieveRecords(uint64_t cacheIdentifier, RecordsCallback&& callback)
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

void CacheStorageEngine::putRecords(uint64_t cacheIdentifier, Vector<Record>&& records, RecordIdentifiersCallback&& callback)
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
            auto matchingRecords = CacheStorageEngine::queryCache(cache.records, record.request, options);
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
        writeCacheRecords(cacheIdentifier, WTFMove(recordIdentifiers), [cacheIdentifier, callback = WTFMove(callback)](RecordIdentifiersOrError&& result) mutable {
            callback(WTFMove(result));
        });
    });
}

void CacheStorageEngine::deleteMatchingRecords(uint64_t cacheIdentifier, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, RecordIdentifiersCallback&& callback)
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

void CacheStorageEngine::writeCachesToDisk(Function<void(std::optional<Error>&&)>&& callback)
{
    // FIXME: Implement writing.
    callback(std::nullopt);
}

void CacheStorageEngine::readCachesFromDisk(const String& origin, CachesCallback&& callback)
{
    // FIXME: Implement reading.

    auto& caches = m_caches.ensure(origin, [] {
        return Vector<Cache>();
    }).iterator->value;

    callback(std::reference_wrapper<Vector<Cache>> { caches });
}

void CacheStorageEngine::readCache(uint64_t cacheIdentifier, CacheCallback&& callback)
{
    // FIXME: Implement reading.
    auto* cache = this->cache(cacheIdentifier);
    if (!cache) {
        callback(makeUnexpected(Error::Internal));
        return;
    }
    callback(std::reference_wrapper<Cache> { *cache });
}

void CacheStorageEngine::writeCacheRecords(uint64_t cacheIdentifier, Vector<uint64_t>&& recordsIdentifiers, RecordIdentifiersCallback&& callback)
{
    // FIXME: Implement writing.
    callback(WTFMove(recordsIdentifiers));
}

void CacheStorageEngine::removeCacheRecords(uint64_t cacheIdentifier, Vector<uint64_t>&& recordsIdentifiers, RecordIdentifiersCallback&& callback)
{
    // FIXME: Implement writing.
    callback(WTFMove(recordsIdentifiers));
}

CacheStorageEngine::Cache* CacheStorageEngine::cache(uint64_t cacheIdentifier)
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

Vector<uint64_t> CacheStorageEngine::queryCache(const Vector<Record>& records, const WebCore::ResourceRequest& request, const WebCore::CacheQueryOptions& options)
{
    if (!options.ignoreMethod && request.httpMethod() != "GET")
        return { };

    Vector<uint64_t> results;
    for (const auto& record : records) {
        if (WebCore::CacheStorageConnection::queryCacheMatch(request, record.request, record.response, options))
            results.append(record.identifier);
    }
    return results;
}

}
