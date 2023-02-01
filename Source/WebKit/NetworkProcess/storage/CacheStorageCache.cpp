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

#include "config.h"
#include "CacheStorageCache.h"

#include "CacheStorageDiskStore.h"
#include "CacheStorageManager.h"
#include "CacheStorageMemoryStore.h"
#include <WebCore/CacheQueryOptions.h>
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/ResourceError.h>
#include <wtf/Scope.h>

namespace WebKit {

static String computeKeyURL(const URL& url)
{
    URL keyURL { url };
    keyURL.removeQueryAndFragmentIdentifier();
    return keyURL.string();
}

static uint64_t nextRecordIdentifier()
{
    static std::atomic<uint64_t> currentRecordIdentifier;
    return ++currentRecordIdentifier;
}

static Ref<CacheStorageStore> createStore(const String& uniqueName, const String& path, Ref<WorkQueue>&& queue)
{
    if (path.isEmpty())
        return CacheStorageMemoryStore::create();
    return CacheStorageDiskStore::create(uniqueName, path, WTFMove(queue));
}

CacheStorageCache::CacheStorageCache(CacheStorageManager& manager, const String& name, const String& uniqueName, const String& path, Ref<WorkQueue>&& queue)
    : m_manager(manager)
    , m_identifier(WebCore::DOMCacheIdentifier::generateThreadSafe())
    , m_name(name)
    , m_uniqueName(uniqueName)
    , m_store(createStore(uniqueName, path, WTFMove(queue)))
{
}

CacheStorageManager* CacheStorageCache::manager()
{
    return m_manager.get();
}

void CacheStorageCache::getSize(CompletionHandler<void(uint64_t)>&& callback)
{
    if (m_isInitialized) {
        uint64_t size = 0;
        for (auto& urlRecords : m_records.values()) {
            for (auto& record : urlRecords)
                size += record.size;
        }
        return callback(size);
    }

    m_store->readAllRecordInfos([callback = WTFMove(callback)](auto&& recordInfos) mutable {
        uint64_t size = 0;
        for (auto& recordInfo : recordInfos)
            size += recordInfo.size;

        callback(size);
    });
}

void CacheStorageCache::open(WebCore::DOMCacheEngine::CacheIdentifierCallback&& callback)
{
    if (m_isInitialized)
        return callback(WebCore::DOMCacheEngine::CacheIdentifierOperationResult { m_identifier, false });

    m_store->readAllRecordInfos([this, weakThis = WeakPtr { *this }, callback = WTFMove(callback)](auto recordInfos) mutable {
        if (!weakThis)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

        std::sort(recordInfos.begin(), recordInfos.end(), [](auto& a, auto& b) {
            return a.insertionTime < b.insertionTime;
        });

        for (auto& recordInfo : recordInfos) {
            recordInfo.identifier = nextRecordIdentifier();
            m_records.ensure(computeKeyURL(recordInfo.url), [] {
                return Vector<CacheStorageRecordInformation> { };
            }).iterator->value.append(recordInfo);
        }

        m_isInitialized = true;
        callback(WebCore::DOMCacheEngine::CacheIdentifierOperationResult { m_identifier, false });
    });
}

static CacheStorageRecord toCacheStorageRecord(WebCore::DOMCacheEngine::Record&& record, FileSystem::Salt salt, const String& uniqueName)
{
    NetworkCache::Key key { "record"_s, uniqueName, { }, createVersion4UUIDString(), salt };
    CacheStorageRecordInformation recordInfo { WTFMove(key), MonotonicTime::now().secondsSinceEpoch().milliseconds(), record.identifier, 0 , record.responseBodySize, record.request.url(), false, { } };
    recordInfo.updateVaryHeaders(record.request, record.response.httpHeaderField(WebCore::HTTPHeaderName::Vary));

    return CacheStorageRecord { WTFMove(recordInfo), record.requestHeadersGuard, WTFMove(record.request), record.options, WTFMove(record.referrer), record.responseHeadersGuard, record.response.crossThreadData(), record.responseBodySize, WTFMove(record.responseBody) };
}

void CacheStorageCache::retrieveRecords(WebCore::RetrieveRecordsOptions&& options, WebCore::DOMCacheEngine::RecordsCallback&& callback)
{
    ASSERT(m_isInitialized);

    Vector<CacheStorageRecordInformation> targetRecordInfos;
    auto url = options.request.url();
    if (url.isNull()) {
        for (auto& urlRecords : m_records.values()) {
            auto newTargetRecordInfos = WTF::map(urlRecords, [&](const auto& record) {
                return record;
            });
            targetRecordInfos.appendVector(WTFMove(newTargetRecordInfos));
        }
    } else {
        if (!options.ignoreMethod && options.request.httpMethod() != "GET"_s)
            return callback({ });

        auto iterator = m_records.find(computeKeyURL(url));
        if (iterator == m_records.end())
            return callback({ });

        WebCore::CacheQueryOptions queryOptions { options.ignoreSearch, options.ignoreMethod, options.ignoreVary };
        for (auto& record : iterator->value) {
            if (WebCore::DOMCacheEngine::queryCacheMatch(options.request, record.url, record.hasVaryStar, record.varyHeaders, queryOptions))
                targetRecordInfos.append(record);
        }
    }

    if (targetRecordInfos.isEmpty())
        return callback({ });
    
    m_store->readRecords(targetRecordInfos, [options = WTFMove(options), callback = WTFMove(callback)](auto&& cacheStorageRecords) mutable {
        Vector<WebCore::DOMCacheEngine::Record> result;
        result.reserveInitialCapacity(cacheStorageRecords.size());
        for (auto& cacheStorageRecord : cacheStorageRecords) {
            if (!cacheStorageRecord)
                continue;
    
            WebCore::DOMCacheEngine::Record record { cacheStorageRecord->info.identifier, 0, cacheStorageRecord->requestHeadersGuard, cacheStorageRecord->request, cacheStorageRecord->options, cacheStorageRecord->referrer, cacheStorageRecord->responseHeadersGuard, { }, nullptr, 0 };
            if (options.shouldProvideResponse) {
                record.response = WebCore::ResourceResponse::fromCrossThreadData(WTFMove(cacheStorageRecord->responseData));
                record.responseBody = WTFMove(cacheStorageRecord->responseBody);
                record.responseBodySize = cacheStorageRecord->responseBodySize;
            }

            if (record.response.type() == WebCore::ResourceResponse::Type::Opaque) {
                if (WebCore::validateCrossOriginResourcePolicy(options.crossOriginEmbedderPolicy.value, options.sourceOrigin, record.request.url(), record.response, WebCore::ForNavigation::No))
                    return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::CORP));
            }

            result.uncheckedAppend(WTFMove(record));
        }

        std::sort(result.begin(), result.end(), [&](auto& a, auto& b) {
            return a.identifier < b.identifier;
        });

        callback(WTFMove(result));
    });
}

void CacheStorageCache::removeRecords(WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    ASSERT(m_isInitialized);
    
    if (!options.ignoreMethod && request.httpMethod() != "GET"_s)
        return callback({ });

    auto iterator = m_records.find(computeKeyURL(request.url()));
    if (iterator == m_records.end())
        return callback({ });

    auto& urlRecords = iterator->value;
    Vector<uint64_t> targetRecordIdentifiers;
    Vector<CacheStorageRecordInformation> targetRecordInfos;
    uint64_t sizeDecreased = 0;
    urlRecords.removeAllMatching([&](auto& record) {
        if (!WebCore::DOMCacheEngine::queryCacheMatch(request, record.url, record.hasVaryStar, record.varyHeaders, options))
            return false;

        targetRecordIdentifiers.append(record.identifier);
        targetRecordInfos.append(record);
        sizeDecreased += record.size;
        return true;
    });

    if (m_manager && sizeDecreased)
        m_manager->sizeDecreased(sizeDecreased);

    m_store->deleteRecords(targetRecordInfos, [targetIdentifiers = WTFMove(targetRecordIdentifiers), callback = WTFMove(callback)](bool succeeded) mutable {
        if (!succeeded)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::WriteDisk));

        callback(WTFMove(targetIdentifiers));
    });
}

CacheStorageRecordInformation* CacheStorageCache::findExistingRecord(const WebCore::ResourceRequest& request, std::optional<uint64_t> identifier)
{
    auto iterator = m_records.find(computeKeyURL(request.url()));
    if (iterator == m_records.end())
        return nullptr;

    WebCore::CacheQueryOptions options;
    auto index = iterator->value.findIf([&] (auto& record) {
        bool hasMatchedIdentifier = !identifier || identifier == record.identifier;
        return hasMatchedIdentifier && WebCore::DOMCacheEngine::queryCacheMatch(request, record.url, record.hasVaryStar, record.varyHeaders, options);
    });
    if (index == notFound)
        return nullptr;

    return &iterator->value[index];
}

void CacheStorageCache::putRecords(Vector<WebCore::DOMCacheEngine::Record>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    ASSERT(m_isInitialized);

    if (!m_manager)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    int64_t spaceRequested = 0;
    auto cacheStorageRecords = WTF::map(records, [&](auto&& record) {
        spaceRequested += record.responseBodySize;
        if (auto* existingRecord = findExistingRecord(record.request))
            spaceRequested -= existingRecord->size;
        return toCacheStorageRecord(WTFMove(record), m_manager->salt(), m_uniqueName);
    });

    // The request still needs to go through quota check to keep ordering.
    if (spaceRequested < 0)
        spaceRequested = 0;

    m_manager->requestSpace(spaceRequested, [this, weakThis = WeakPtr { *this }, records = WTFMove(cacheStorageRecords), callback = WTFMove(callback)](bool granted) mutable {
        if (!weakThis)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

        if (!granted)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::QuotaExceeded));

        putRecordsAfterQuotaCheck(WTFMove(records), WTFMove(callback));
    });
}

void CacheStorageCache::putRecordsAfterQuotaCheck(Vector<CacheStorageRecord>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    ASSERT(m_isInitialized);

    Vector<CacheStorageRecordInformation> targetRecordInfos;
    for (auto& record : records) {
        if (auto* existingRecord = findExistingRecord(record.request)) {
            record.info.identifier = existingRecord->identifier;
            targetRecordInfos.append(*existingRecord);
        }
    }

    auto readRecordsCallback = [this, weakThis = WeakPtr { *this }, records = WTFMove(records), callback = WTFMove(callback)](auto existingCacheStorageRecords) mutable {
        if (!weakThis)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

        putRecordsInStore(WTFMove(records), WTFMove(existingCacheStorageRecords), WTFMove(callback));
    };

    m_store->readRecords(targetRecordInfos, WTFMove(readRecordsCallback));
}

void CacheStorageCache::putRecordsInStore(Vector<CacheStorageRecord>&& records, Vector<std::optional<CacheStorageRecord>>&& existingRecords, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    Vector<uint64_t> targetIdentifiers;
    uint64_t sizeIncreased = 0, sizeDecreased = 0;
    for (auto& record : records) {
        if (!record.info.identifier) {
            record.info.identifier = nextRecordIdentifier();
            sizeIncreased += record.info.size;
            m_records.ensure(computeKeyURL(record.info.url), [] {
                return Vector<CacheStorageRecordInformation> { };
            }).iterator->value.append(record.info);
        } else {
            auto index = existingRecords.findIf([&](auto& existingRecord) {
                return existingRecord && existingRecord->info.identifier == record.info.identifier;
            });
            // Ensure record still exists.
            if (index == notFound) {
                record.info.identifier = 0;
                continue;
            }

            auto& existingRecord = existingRecords[index];
            // Ensure identifier still exists.
            auto* existingRecordInfo = findExistingRecord(record.request, record.info.identifier);
            if (!existingRecordInfo) {
                record.info.identifier = 0;
                continue;
            }

            record.info.key = existingRecordInfo->key;
            record.info.insertionTime = existingRecordInfo->insertionTime;
            record.info.url = existingRecordInfo->url;
            record.requestHeadersGuard = existingRecord->requestHeadersGuard;
            record.request = WTFMove(existingRecord->request);
            record.options = WTFMove(existingRecord->options);
            record.referrer = WTFMove(existingRecord->referrer);
            record.info.updateVaryHeaders(record.request, record.responseData.httpHeaderFields.get(WebCore::HTTPHeaderName::Vary));
            sizeIncreased += record.info.size;
            sizeDecreased += existingRecordInfo->size;
            existingRecordInfo->size = record.info.size;
        }

        targetIdentifiers.append(record.info.identifier);
    }

    records.removeAllMatching([&](auto& record) {
        return !record.info.identifier;
    });

    if (m_manager) {
        if (sizeIncreased > sizeDecreased)
            m_manager->sizeIncreased(sizeIncreased - sizeDecreased);
        else if (sizeDecreased > sizeIncreased)
            m_manager->sizeDecreased(sizeDecreased - sizeIncreased);
    }

    m_store->writeRecords(WTFMove(records), [targetIdentifiers = WTFMove(targetIdentifiers), callback = WTFMove(callback)](bool succeeded) mutable {
        if (!succeeded)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::WriteDisk));

        callback(WTFMove(targetIdentifiers));
    });
}

void CacheStorageCache::removeAllRecords()
{
    uint64_t sizeDecreased = 0;
    Vector<CacheStorageRecordInformation> targetRecordInfos;
    for (auto& urlRecords : m_records.values()) {
        for (auto& record : urlRecords) {
            targetRecordInfos.append(record);
            sizeDecreased += record.size;
        }
    }

    if (m_manager && sizeDecreased)
        m_manager->sizeDecreased(sizeDecreased);

    m_records.clear();
    m_store->deleteRecords(targetRecordInfos, [](auto) { });
}

void CacheStorageCache::close()
{
    m_records.clear();
    m_isInitialized = false;
}

} // namespace WebKit
