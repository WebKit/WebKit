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
#include "Connection.h"
#include "Logging.h"
#include <WebCore/CacheQueryOptions.h>
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/HTTPHeaderMap.h>
#include <WebCore/OriginAccessPatterns.h>
#include <WebCore/ResourceError.h>
#include <ranges>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK_COMPLETION(assertion, connection, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, completion)

namespace WebKit {

std::optional<String> CacheStorageCache::computeKeyURL(const URL& url)
{
    ASSERT(url.isValid());
    ASSERT(!url.isEmpty());
    if (!url.isValid() || url.isEmpty())
        return std::nullopt;
    URL keyURL { url };
    keyURL.removeQueryAndFragmentIdentifier();
    auto keyURLString = keyURL.string();
    ASSERT(RecordsMap::isValidKey(keyURLString));
    if (!RecordsMap::isValidKey(keyURLString))
        return std::nullopt;
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
    return CacheStorageDiskStore::create(uniqueName, path, WTF::move(queue));
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(CacheStorageCache);

Ref<CacheStorageCache> CacheStorageCache::create(CacheStorageManager& manager, const String& name, const String& uniqueName, const String& path, Ref<WorkQueue>&& queue)
{
    return adoptRef(*new CacheStorageCache(manager, name, uniqueName, path, WTF::move(queue)));
}

CacheStorageCache::CacheStorageCache(CacheStorageManager& manager, const String& name, const String& uniqueName, const String& path, Ref<WorkQueue>&& queue)
    : m_manager(manager)
    , m_name(name)
    , m_uniqueName(uniqueName)
#if ASSERT_ENABLED
    , m_queue(queue.copyRef())
#endif
    , m_store(createStore(uniqueName, path, WTF::move(queue)))
{
    assertIsOnCorrectQueue();
}

CacheStorageCache::~CacheStorageCache()
{
    assertIsOnCorrectQueue();
    for (auto& callback : m_pendingInitializationCallbacks)
        callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));
}

CacheStorageManager* CacheStorageCache::manager()
{
    return m_manager.get();
}

std::optional<WebCore::ClientOrigin> CacheStorageCache::origin() const
{
    if (RefPtr manager = m_manager.get())
        return manager->origin();
    return std::nullopt;
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

    m_store->readAllRecordInfos([callback = WTF::move(callback)](auto&& recordInfos) mutable {
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

    m_pendingInitializationCallbacks.append(WTF::move(callback));
    if (m_pendingInitializationCallbacks.size() > 1)
        return;

    m_store->readAllRecordInfos([weakThis = WeakPtr { *this }](auto&& recordInfos) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->assertIsOnCorrectQueue();

        std::ranges::sort(recordInfos, { }, &CacheStorageRecordInformation::insertionTime);

        for (auto&& recordInfo : recordInfos) {
            recordInfo.setIdentifier(nextRecordIdentifier());
            protectedThis->m_records.ensure(*computeKeyURL(recordInfo.url()), [] {
                return Vector<CacheStorageRecordInformation> { };
            }).iterator->value.append(WTF::move(recordInfo));
        }

        protectedThis->m_isInitialized = true;
        for (auto& callback : protectedThis->m_pendingInitializationCallbacks)
            callback(WebCore::DOMCacheEngine::CacheIdentifierOperationResult { protectedThis->identifier(), false });
        protectedThis->m_pendingInitializationCallbacks.clear();
    });
}

static CacheStorageRecord toCacheStorageRecord(WebCore::DOMCacheEngine::CrossThreadRecord&& record, FileSystem::Salt salt, const String& uniqueName)
{
    NetworkCache::Key key { "record"_s, uniqueName, { }, createVersion4UUIDString(), salt };
    CacheStorageRecordInformation recordInfo { WTF::move(key), MonotonicTime::now().secondsSinceEpoch().milliseconds(), record.identifier, 0 , record.responseBodySize, URL { record.request.url() }, false, HashMap<String, String> { } };
    recordInfo.updateVaryHeaders(record.request, record.response);

    return CacheStorageRecord { WTF::move(recordInfo), record.requestHeadersGuard, WTF::move(record.request), record.options, WTF::move(record.referrer), record.responseHeadersGuard, WTF::move(record.response), record.responseBodySize, WTF::move(record.responseBody) };
}

void CacheStorageCache::retrieveRecords(IPC::Connection& connection, WebCore::RetrieveRecordsOptions&& options, WebCore::DOMCacheEngine::CrossThreadRecordsCallback&& callback)
{
    if (!options.request.url().isNull())
        MESSAGE_CHECK_COMPLETION(computeKeyURL(options.request.url()), connection, callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal)));

    auto targetRecordInfos = findRecords(options);
    retrieveRecords(targetRecordInfos, WTF::move(options), WTF::move(callback));
}

Vector<CacheStorageRecordInformation> CacheStorageCache::findRecords(const WebCore::RetrieveRecordsOptions& options)
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
            targetRecordInfos.appendVector(WTF::move(newTargetRecordInfos));
        }
    } else {
        if (!options.ignoreMethod && options.request.httpMethod() != "GET"_s)
            return { };

        auto keyURL = computeKeyURL(url);
        if (!keyURL)
            return { };

        auto iterator = m_records.find(*keyURL);
        if (iterator == m_records.end())
            return { };

        WebCore::CacheQueryOptions queryOptions { options.ignoreSearch, options.ignoreMethod, options.ignoreVary };
        for (auto& record : iterator->value) {
            if (WebCore::DOMCacheEngine::queryCacheMatch(options.request, record.url(), record.hasVaryStar(), record.varyHeaders(), queryOptions))
                targetRecordInfos.append(record);
        }
    }

    return targetRecordInfos;
}

void CacheStorageCache::retrieveRecords(const Vector<CacheStorageRecordInformation>& targetRecordInfos, WebCore::RetrieveRecordsOptions&& options, WebCore::DOMCacheEngine::CrossThreadRecordsCallback&& callback)
{
    if (targetRecordInfos.isEmpty())
        return callback({ });
    
    m_store->readRecords(targetRecordInfos, [options = WTF::move(options), callback = WTF::move(callback)](auto&& cacheStorageRecords) mutable {
        using namespace WebCore::DOMCacheEngine;
        Vector<CrossThreadRecord> result;
        result.reserveInitialCapacity(cacheStorageRecords.size());
        for (auto&& cacheStorageRecord : cacheStorageRecords) {
            if (!cacheStorageRecord)
                continue;
    
            CrossThreadRecord record { cacheStorageRecord->info.identifier(), 0, cacheStorageRecord->requestHeadersGuard, WTF::move(cacheStorageRecord->request), cacheStorageRecord->options, WTF::move(cacheStorageRecord->referrer), cacheStorageRecord->responseHeadersGuard, { }, nullptr, 0 };
            if (options.shouldProvideResponse) {
                record.response = WTF::move(cacheStorageRecord->responseData);
                record.responseBody = WTF::move(cacheStorageRecord->responseBody);
                record.responseBodySize = cacheStorageRecord->responseBodySize;
            }

            if (record.response.type == WebCore::ResourceResponse::Type::Opaque) {
                if (WebCore::validateCrossOriginResourcePolicy(options.crossOriginEmbedderPolicy.value, options.sourceOrigin, record.request.url(), false, record.response.url, record.response.httpHeaderFields.get(WebCore::HTTPHeaderName::CrossOriginResourcePolicy), WebCore::ForNavigation::No, WebCore::EmptyOriginAccessPatterns::singleton()))
                    return callback(makeUnexpected(Error::CORP));
            }

            result.append(WTF::move(record));
        }

        std::ranges::sort(result, { }, &CrossThreadRecord::identifier);

        callback(WTF::move(result));
    });
}

void CacheStorageCache::removeRecords(IPC::Connection& connection, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    ASSERT(m_isInitialized);
    assertIsOnCorrectQueue();

    if (!options.ignoreMethod && request.httpMethod() != "GET"_s)
        return callback({ });

    auto keyURL = computeKeyURL(request.url());
    MESSAGE_CHECK_COMPLETION(keyURL, connection, callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal)));

    auto iterator = m_records.find(*keyURL);
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

    m_store->deleteRecords(targetRecordInfos, [targetIdentifiers = WTF::move(targetRecordIdentifiers), callback = WTF::move(callback)](bool succeeded) mutable {
        if (!succeeded)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::WriteDisk));

        callback(WTF::move(targetIdentifiers));
    });
}

Expected<CacheStorageRecordInformation*, WebCore::DOMCacheEngine::Error> CacheStorageCache::findExistingRecord(const WebCore::ResourceRequest& request, std::optional<uint64_t> identifier)
{
    assertIsOnCorrectQueue();

    auto keyURL = computeKeyURL(request.url());
    if (!keyURL)
        return makeUnexpected(WebCore::DOMCacheEngine::Error::Internal);

    auto iterator = m_records.find(*keyURL);
    if (iterator == m_records.end())
        return nullptr;

    WebCore::CacheQueryOptions options;
    auto index = iterator->value.findIf([&] (auto& record) {
        bool hasMatchedIdentifier = !identifier || identifier == record.identifier();
        return hasMatchedIdentifier && WebCore::DOMCacheEngine::queryCacheMatch(request, record.url(), record.hasVaryStar(), record.varyHeaders(), options);
    });
    if (index == notFound)
        return nullptr;

    return &iterator->value[index];
}

void CacheStorageCache::putRecords(IPC::Connection& connection, Vector<WebCore::DOMCacheEngine::CrossThreadRecord>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    ASSERT(m_isInitialized);
    assertIsOnCorrectQueue();

    RefPtr manager = m_manager.get();
    if (!manager)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    CheckedUint64 spaceRequested = 0;
    CheckedUint64 spaceAvailable = 0;
    bool isSpaceRequestedValid = true;
    std::optional<WebCore::DOMCacheEngine::Error> encounteredError;
    auto cacheStorageRecords = WTF::compactMap(WTF::move(records), [&](WebCore::DOMCacheEngine::CrossThreadRecord&& record) -> std::optional<CacheStorageRecord> {
        if (encounteredError)
            return std::nullopt;

        if (isSpaceRequestedValid) {
            spaceRequested += record.responseBodySize;
            auto existingRecord = findExistingRecord(record.request);
            if (!existingRecord.has_value()) {
                encounteredError = existingRecord.error();
                return std::nullopt;
            }
            if (*existingRecord)
                spaceAvailable += (*existingRecord)->size();
            if (spaceRequested.hasOverflowed() || spaceAvailable.hasOverflowed())
                isSpaceRequestedValid = false;
            else {
                uint64_t spaceUsed = std::min(spaceRequested, spaceAvailable);
                spaceRequested -= spaceUsed;
                spaceAvailable -= spaceUsed;
            }
        }
        return toCacheStorageRecord(WTF::move(record), manager->salt(), m_uniqueName);
    });

    MESSAGE_CHECK_COMPLETION(!encounteredError, connection, callback(makeUnexpected(*encounteredError)));

    // The request still needs to go through quota check to keep ordering.
    if (!isSpaceRequestedValid)
        spaceRequested = 0;

    manager->requestSpace(spaceRequested, [weakThis = WeakPtr { *this }, connection = Ref { connection }, records = WTF::move(cacheStorageRecords), callback = WTF::move(callback), isSpaceRequestedValid](bool granted) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

        if (!isSpaceRequestedValid) {
            RELEASE_LOG_ERROR(Storage, "CacheStorageCache::putRecords failed because the amount of space requested is invalid");
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));
        }

        if (!granted)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::QuotaExceeded));

        protectedThis->putRecordsAfterQuotaCheck(connection, WTF::move(records), WTF::move(callback));
    });
}

void CacheStorageCache::putRecordsAfterQuotaCheck(IPC::Connection& connection, Vector<CacheStorageRecord>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    ASSERT(m_isInitialized);
    assertIsOnCorrectQueue();

    Vector<CacheStorageRecordInformation> targetRecordInfos;
    for (auto& record : records) {
        auto existingRecord = findExistingRecord(record.request);
        MESSAGE_CHECK_COMPLETION(existingRecord.has_value(), connection, callback(makeUnexpected(existingRecord.error())));
        if (*existingRecord) {
            record.info.setIdentifier((*existingRecord)->identifier());
            targetRecordInfos.append(**existingRecord);
        }
    }

    auto readRecordsCallback = [weakThis = WeakPtr { *this }, connection = Ref { connection }, records = WTF::move(records), callback = WTF::move(callback)](auto existingCacheStorageRecords) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

        protectedThis->putRecordsInStore(connection, WTF::move(records), WTF::move(existingCacheStorageRecords), WTF::move(callback));
    };

    m_store->readRecords(targetRecordInfos, WTF::move(readRecordsCallback));
}

void CacheStorageCache::putRecordsInStore(IPC::Connection& connection, Vector<CacheStorageRecord>&& records, Vector<std::optional<CacheStorageRecord>>&& existingRecords, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    assertIsOnCorrectQueue();

    Vector<uint64_t> targetIdentifiers;
    uint64_t sizeIncreased = 0, sizeDecreased = 0;
    for (auto& record : records) {
        if (!record.info.identifier()) {
            record.info.setIdentifier(nextRecordIdentifier());
            sizeIncreased += record.info.size();
            auto keyURL = computeKeyURL(record.info.url());
            MESSAGE_CHECK_COMPLETION(keyURL, connection, callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal)));
            m_records.ensure(*keyURL, [] {
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
            auto existingRecordInfoOrError = findExistingRecord(record.request, record.info.identifier());
            MESSAGE_CHECK_COMPLETION(existingRecordInfoOrError.has_value(), connection, callback(makeUnexpected(existingRecordInfoOrError.error())));
            auto* existingRecordInfo = *existingRecordInfoOrError;
            if (!existingRecordInfo) {
                record.info.setIdentifier(0);
                continue;
            }

            record.info.setKey(existingRecordInfo->key());
            record.info.setInsertionTime(existingRecordInfo->insertionTime());
            record.info.setURL(existingRecordInfo->url());
            record.requestHeadersGuard = existingRecord->requestHeadersGuard;
            record.request = WTF::move(existingRecord->request);
            record.options = WTF::move(existingRecord->options);
            record.referrer = WTF::move(existingRecord->referrer);
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

    m_store->writeRecords(WTF::move(records), [targetIdentifiers = WTF::move(targetIdentifiers), callback = WTF::move(callback)](bool succeeded) mutable {
        if (!succeeded)
            return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::WriteDisk));

        callback(WTF::move(targetIdentifiers));
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
