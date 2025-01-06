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
#include "Logging.h"
#include <WebCore/CacheQueryOptions.h>
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/HTTPHeaderMap.h>
#include <WebCore/OriginAccessPatterns.h>
#include <WebCore/ResourceError.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

static String computeKeyURL(const URL& url)
{
    RELEASE_ASSERT(url.isValid());
    RELEASE_ASSERT(!url.isEmpty());
    URL keyURL { url };
    keyURL.removeQueryAndFragmentIdentifier();
    auto keyURLString = keyURL.string();
    RELEASE_ASSERT(!keyURLString.isEmpty());
    return keyURLString;
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

WTF_MAKE_TZONE_ALLOCATED_IMPL(CacheStorageCache);

Ref<CacheStorageCache> CacheStorageCache::create(CacheStorageManager& manager, const String& name, const String& uniqueName, const String& path, Ref<WorkQueue>&& queue)
{
    return adoptRef(*new CacheStorageCache(manager, name, uniqueName, path, WTFMove(queue)));
}

CacheStorageCache::CacheStorageCache(CacheStorageManager& manager, const String& name, const String& uniqueName, const String& path, Ref<WorkQueue>&& queue)
    : m_manager(manager)
    , m_name(name)
    , m_uniqueName(uniqueName)
#if ASSERT_ENABLED
    , m_queue(queue.copyRef())
#endif
    , m_store(createStore(uniqueName, path, WTFMove(queue)))
{
    assertIsOnCorrectQueue();
}

CacheStorageCache::~CacheStorageCache()
{
    for (auto& callback : m_pendingInitializationCallbacks)
        callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));
}

CacheStorageManager* CacheStorageCache::manager()
{
    return m_manager.get();
}

void CacheStorageCache::getSize(CompletionHandler<void(uint64_t)>&& callback)
{
    assertIsOnCorrectQueue();

    if (m_isInitialized) {
        uint64_t size = 0;
        for (auto& urlRecords : m_records.values()) {
            for (auto& record : urlRecords)
                size += record.size();
        }
        return callback(size);
    }

    m_store->readAllRecordInfos([callback = WTFMove(callback)](auto&& recordInfos) mutable {
        uint64_t size = 0;
        for (auto& recordInfo : recordInfos)
            size += recordInfo.size();

        callback(size);
    });
}

void CacheStorageCache::open(WebCore::DOMCacheEngine::CacheIdentifierCallback&& callback)
{
    assertIsOnCorrectQueue();

    if (m_isInitialized)
        return callback(WebCore::DOMCacheEngine::CacheIdentifierOperationResult { identifier(), false });

    m_pendingInitializationCallbacks.append(WTFMove(callback));
    if (m_pendingInitializationCallbacks.size() > 1)
        return;

    m_store->readAllRecordInfos([this, weakThis = WeakPtr { *this }](auto&& recordInfos) mutable {
        if (!weakThis)
            return;

        assertIsOnCorrectQueue();

        std::sort(recordInfos.begin(), recordInfos.end(), [](auto& a, auto& b) {
            return a.insertionTime() < b.insertionTime();
        });

        for (auto&& recordInfo : recordInfos) {
            RELEASE_ASSERT(!recordInfo.url().string().impl()->isAtom());
            recordInfo.setIdentifier(nextRecordIdentifier());
            m_records.ensure(computeKeyURL(recordInfo.url()), [] {
                return Vector<CacheStorageRecordInformation> { };
            }).iterator->value.append(WTFMove(recordInfo));
        }

        m_isInitialized = true;
        for (auto& callback : m_pendingInitializationCallbacks)
            callback(WebCore::DOMCacheEngine::CacheIdentifierOperationResult { identifier(), false });
        m_pendingInitializationCallbacks.clear();
    });
}

static CacheStorageRecord toCacheStorageRecord(WebCore::DOMCacheEngine::CrossThreadRecord&& record, FileSystem::Salt salt, const String& uniqueName)
{
    NetworkCache::Key key { "record"_s, uniqueName, { }, createVersion4UUIDString(), salt };
    auto requestURL = record.request.url();
    CacheStorageRecordInformation recordInfo { WTFMove(key), MonotonicTime::now().secondsSinceEpoch().milliseconds(), record.identifier, 0 , record.responseBodySize, WTFMove(requestURL), false, HashMap<String, String> { } };
    recordInfo.updateVaryHeaders(record.request, record.response);

    return CacheStorageRecord { WTFMove(recordInfo), record.requestHeadersGuard, WTFMove(record.request), record.options, WTFMove(record.referrer), record.responseHeadersGuard, WTFMove(record.response), record.responseBodySize, WTFMove(record.responseBody) };
}

void CacheStorageCache::retrieveRecords(WebCore::RetrieveRecordsOptions&& options, WebCore::DOMCacheEngine::CrossThreadRecordsCallback&& callback)
{
    ASSERT(m_isInitialized);
    assertIsOnCorrectQueue();

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
            RELEASE_ASSERT(!record.url().string().impl()->isAtom());
            if (WebCore::DOMCacheEngine::queryCacheMatch(options.request, record.url(), record.hasVaryStar(), record.varyHeaders(), queryOptions))
                targetRecordInfos.append(record);
        }
    }

    if (targetRecordInfos.isEmpty())
        return callback({ });
    
    m_store->readRecords(targetRecordInfos, [options = WTFMove(options), callback = WTFMove(callback)](auto&& cacheStorageRecords) mutable {
        Vector<WebCore::DOMCacheEngine::CrossThreadRecord> result;
        result.reserveInitialCapacity(cacheStorageRecords.size());
        for (auto&& cacheStorageRecord : cacheStorageRecords) {
            if (!cacheStorageRecord)
                continue;
    
            WebCore::DOMCacheEngine::CrossThreadRecord record { cacheStorageRecord->info.identifier(), 0, cacheStorageRecord->requestHeadersGuard, WTFMove(cacheStorageRecord->request), cacheStorageRecord->options, WTFMove(cacheStorageRecord->referrer), cacheStorageRecord->responseHeadersGuard, { }, nullptr, 0 };
            if (options.shouldProvideResponse) {
                record.response = WTFMove(cacheStorageRecord->responseData);
                record.responseBody = WTFMove(cacheStorageRecord->responseBody);
                record.responseBodySize = cacheStorageRecord->responseBodySize;
            }

            if (record.response.type == WebCore::ResourceResponse::Type::Opaque) {
                if (WebCore::validateCrossOriginResourcePolicy(options.crossOriginEmbedderPolicy.value, options.sourceOrigin, record.request.url(), false, record.response.url, record.response.httpHeaderFields.get(WebCore::HTTPHeaderName::CrossOriginResourcePolicy), WebCore::ForNavigation::No, WebCore::EmptyOriginAccessPatterns::singleton()))
                    return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::CORP));
            }

            result.append(WTFMove(record));
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
    assertIsOnCorrectQueue();
    
    if (!options.ignoreMethod && request.httpMethod() != "GET"_s)
        return callback({ });

    auto iterator = m_records.find(computeKeyURL(request.url()));
    if (iterator == m_records.end())
        return callback({ });

    Vector<uint64_t> targetRecordIdentifiers;
    Vector<CacheStorageRecordInformation> targetRecordInfos;
    uint64_t sizeDecreased = 0;
    iterator->value.removeAllMatching([&](auto& record) {
        if (!WebCore::DOMCacheEngine::queryCacheMatch(request, record.url(), record.hasVaryStar(), record.varyHeaders(), options))
            return false;

        targetRecordIdentifiers.append(record.identifier());
        targetRecordInfos.append(record);
        sizeDecreased += record.size();
        return true;
    });
    if (iterator->value.isEmpty())
        m_records.remove(iterator);

    if (RefPtr manager = m_manager.get(); manager && sizeDecreased)
        manager->sizeDecreased(sizeDecreased);

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
        RELEASE_ASSERT(!record.url().string().impl()->isAtom());
        bool hasMatchedIdentifier = !identifier || identifier == record.identifier();
        return hasMatchedIdentifier && WebCore::DOMCacheEngine::queryCacheMatch(request, record.url(), record.hasVaryStar(), record.varyHeaders(), options);
    });
    if (index == notFound)
        return nullptr;

    return &iterator->value[index];
}

void CacheStorageCache::putRecords(Vector<WebCore::DOMCacheEngine::CrossThreadRecord>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    ASSERT(m_isInitialized);
    assertIsOnCorrectQueue();

    RefPtr manager = m_manager.get();
    if (!manager)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    CheckedUint64 spaceRequested = 0;
    CheckedUint64 spaceAvailable = 0;
    bool isSpaceRequestedValid = true;
    auto cacheStorageRecords = WTF::map(WTFMove(records), [&](WebCore::DOMCacheEngine::CrossThreadRecord&& record) {
        if (isSpaceRequestedValid) {
            spaceRequested += record.responseBodySize;
            if (auto* existingRecord = findExistingRecord(record.request))
                spaceAvailable += existingRecord->size();
            if (spaceRequested.hasOverflowed() || spaceAvailable.hasOverflowed())
                isSpaceRequestedValid = false;
            else {
                uint64_t spaceUsed = std::min(spaceRequested, spaceAvailable);
                spaceRequested -= spaceUsed;
                spaceAvailable -= spaceUsed;
            }
        }
        return toCacheStorageRecord(WTFMove(record), manager->salt(), m_uniqueName);
    });

    // The request still needs to go through quota check to keep ordering.
    if (!isSpaceRequestedValid)
        spaceRequested = 0;

    manager->requestSpace(spaceRequested, [this, weakThis = WeakPtr { *this }, records = WTFMove(cacheStorageRecords), callback = WTFMove(callback), isSpaceRequestedValid](bool granted) mutable {
        if (!weakThis)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

        if (!isSpaceRequestedValid) {
            RELEASE_LOG_ERROR(Storage, "CacheStorageCache::putRecords failed because the amount of space requested is invalid");
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));
        }

        if (!granted)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::QuotaExceeded));

        putRecordsAfterQuotaCheck(WTFMove(records), WTFMove(callback));
    });
}

void CacheStorageCache::putRecordsAfterQuotaCheck(Vector<CacheStorageRecord>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    ASSERT(m_isInitialized);
    assertIsOnCorrectQueue();

    Vector<CacheStorageRecordInformation> targetRecordInfos;
    for (auto& record : records) {
        RELEASE_ASSERT(!record.info.url().string().impl()->isAtom());
        if (auto* existingRecord = findExistingRecord(record.request)) {
            record.info.setIdentifier(existingRecord->identifier());
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
        if (!record.info.identifier()) {
            record.info.setIdentifier(nextRecordIdentifier());
            sizeIncreased += record.info.size();
            m_records.ensure(computeKeyURL(record.info.url()), [] {
                return Vector<CacheStorageRecordInformation> { };
            }).iterator->value.append(record.info);
        } else {
            auto index = existingRecords.findIf([&](auto& existingRecord) {
                return existingRecord && existingRecord->info.identifier() == record.info.identifier();
            });
            // Ensure record still exists.
            if (index == notFound) {
                record.info.setIdentifier(0);
                continue;
            }

            auto& existingRecord = existingRecords[index];
            // Ensure identifier still exists.
            auto* existingRecordInfo = findExistingRecord(record.request, record.info.identifier());
            if (!existingRecordInfo) {
                record.info.setIdentifier(0);
                continue;
            }

            auto existingKey = existingRecordInfo->key();
            record.info.setKey(WTFMove(existingKey));
            record.info.setInsertionTime(existingRecordInfo->insertionTime());
            // FIXME: Remove isolatedCopy() when rdar://105122133 is resolved.
            record.info.setURL(existingRecordInfo->url().isolatedCopy());
            record.requestHeadersGuard = existingRecord->requestHeadersGuard;
            record.request = WTFMove(existingRecord->request);
            record.options = WTFMove(existingRecord->options);
            record.referrer = WTFMove(existingRecord->referrer);
            record.info.updateVaryHeaders(record.request, record.responseData);
            sizeIncreased += record.info.size();
            sizeDecreased += existingRecordInfo->size();
            existingRecordInfo->setSize(record.info.size());
        }

        targetIdentifiers.append(record.info.identifier());
    }

    records.removeAllMatching([&](auto& record) {
        return !record.info.identifier();
    });

    if (RefPtr manager = m_manager.get()) {
        if (sizeIncreased > sizeDecreased)
            manager->sizeIncreased(sizeIncreased - sizeDecreased);
        else if (sizeDecreased > sizeIncreased)
            manager->sizeDecreased(sizeDecreased - sizeIncreased);
    }

    m_store->writeRecords(WTFMove(records), [targetIdentifiers = WTFMove(targetIdentifiers), callback = WTFMove(callback)](bool succeeded) mutable {
        if (!succeeded)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::WriteDisk));

        callback(WTFMove(targetIdentifiers));
    });
}

void CacheStorageCache::removeAllRecords()
{
    assertIsOnCorrectQueue();

    uint64_t sizeDecreased = 0;
    Vector<CacheStorageRecordInformation> targetRecordInfos;
    for (auto& urlRecords : m_records.values()) {
        for (auto& record : urlRecords) {
            targetRecordInfos.append(record);
            sizeDecreased += record.size();
        }
    }

    if (RefPtr manager = m_manager.get(); manager && sizeDecreased)
        manager->sizeDecreased(sizeDecreased);

    m_records.clear();
    m_store->deleteRecords(targetRecordInfos, [](auto) { });
}

void CacheStorageCache::close()
{
    assertIsOnCorrectQueue();

    m_records.clear();
    m_isInitialized = false;
}

} // namespace WebKit
