/*
 * Copyright (C) 2017-2022 Apple Inc.  All rights reserved.
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

#include "CacheStorageEngineCaches.h"
#include "NetworkCacheCoders.h"
#include "NetworkCacheIOChannel.h"
#include "NetworkCacheKey.h"
#include "NetworkProcess.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/CacheQueryOptions.h>
#include <WebCore/CrossOriginAccessControl.h>
#include <WebCore/HTTPParsers.h>
#include <WebCore/RetrieveRecordsOptions.h>
#include <pal/SessionID.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/UUID.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebKit {

namespace CacheStorage {

using namespace WebCore;
using namespace WebCore::DOMCacheEngine;
using namespace NetworkCache;

static inline String computeKeyURL(const URL& url)
{
    URL keyURL { url };
    keyURL.removeQueryAndFragmentIdentifier();
    return keyURL.string();
}

static inline Vector<uint64_t> queryCache(const Vector<RecordInformation>* records, const ResourceRequest& request, const CacheQueryOptions& options)
{
    if (!records)
        return { };

    if (!options.ignoreMethod && request.httpMethod() != "GET"_s)
        return { };

    Vector<uint64_t> results;
    for (const auto& record : *records) {
        if (WebCore::DOMCacheEngine::queryCacheMatch(request, record.url, record.hasVaryStar, record.varyHeaders, options))
            results.append(record.identifier);
    }
    return results;
}

static inline void updateVaryInformation(RecordInformation& recordInformation, const ResourceRequest& request, const ResourceResponse& response)
{
    auto varyValue = response.httpHeaderField(WebCore::HTTPHeaderName::Vary);
    if (varyValue.isNull()) {
        recordInformation.hasVaryStar = false;
        recordInformation.varyHeaders = { };
        return;
    }

    varyValue.split(',', [&](StringView view) {
        if (!recordInformation.hasVaryStar && stripLeadingAndTrailingHTTPSpaces(view) == "*"_s)
            recordInformation.hasVaryStar = true;
        recordInformation.varyHeaders.add(view.toString(), request.httpHeaderField(view));
    });

    if (recordInformation.hasVaryStar)
        recordInformation.varyHeaders = { };
}

RecordInformation Cache::toRecordInformation(const Record& record)
{
    Key key { "record"_s, m_uniqueName, { }, createVersion4UUIDString(), m_caches.salt() };
    RecordInformation recordInformation { WTFMove(key), MonotonicTime::now().secondsSinceEpoch().milliseconds(), record.identifier, 0 , record.responseBodySize, record.request.url(), false, { } };

    updateVaryInformation(recordInformation, record.request, record.response);

    return recordInformation;
}

Cache::Cache(Caches& caches, DOMCacheIdentifier identifier, State state, String&& name, String&& uniqueName)
    : m_caches(caches)
    , m_state(state)
    , m_identifier(identifier)
    , m_name(WTFMove(name))
    , m_uniqueName(WTFMove(uniqueName))
{
}

void Cache::dispose()
{
    m_caches.dispose(*this);
}

void Cache::clearMemoryRepresentation()
{
    m_records = { };
    m_nextRecordIdentifier = 0;
    m_state = State::Uninitialized;
}

RecordInformation RecordInformation::isolatedCopy() &&
{
    return { key, insertionTime, identifier, updateResponseCounter, size, WTFMove(url).isolatedCopy(), hasVaryStar, crossThreadCopy(WTFMove(varyHeaders)) };
}

struct TraversalResult {
    WebCore::DOMCacheIdentifier cacheIdentifier;
    HashMap<String, Vector<RecordInformation>> records;
    Vector<Key> failedRecords;

    TraversalResult isolatedCopy() &&;
};

TraversalResult TraversalResult::isolatedCopy() &&
{
    // No need to isolate keys since they are isolated through the copy constructor.
    return { cacheIdentifier, crossThreadCopy(WTFMove(records)), WTFMove(failedRecords) };
}

void Cache::open(CompletionCallback&& callback)
{
    if (m_state == State::Open) {
        callback(std::nullopt);
        return;
    }
    if (m_state == State::Opening) {
        m_pendingOpeningCallbacks.append(WTFMove(callback));
        return;
    }
    m_state = State::Opening;
    TraversalResult traversalResult { m_identifier, { }, { } };
    m_caches.readRecordsList(*this, [caches = Ref { m_caches }, callback = WTFMove(callback), traversalResult = WTFMove(traversalResult)](const auto* storageRecord, const auto&) mutable {
        if (!storageRecord) {
            RunLoop::main().dispatch([caches = WTFMove(caches), callback = WTFMove(callback), traversalResult = WTFMove(traversalResult).isolatedCopy() ]() mutable {
                for (auto& key : traversalResult.failedRecords)
                    caches->removeCacheEntry(key);

                auto* cache = caches->find(traversalResult.cacheIdentifier);
                if (!cache) {
                    callback(Error::Internal);
                    return;
                }
                cache->m_records = WTFMove(traversalResult.records);
                cache->finishOpening(WTFMove(callback), std::nullopt);
            });
            return;
        }

        auto decoded = decodeRecordHeader(*storageRecord);
        if (!decoded) {
            traversalResult.failedRecords.append(storageRecord->key);
            return;
        }

        auto& record = decoded->record;
        auto insertionTime = decoded->insertionTime;

        RecordInformation recordInformation { storageRecord->key, insertionTime, 0, 0, record.responseBodySize, record.request.url(), false, { } };
        updateVaryInformation(recordInformation, record.request, record.response);

        auto& sameURLRecords = traversalResult.records.ensure(computeKeyURL(recordInformation.url), [] { return Vector<RecordInformation> { }; }).iterator->value;
        sameURLRecords.append(WTFMove(recordInformation));
    });
}

void Cache::finishOpening(CompletionCallback&& callback, std::optional<Error>&& error)
{
    Vector<std::reference_wrapper<RecordInformation>> records;
    for (auto& value : m_records.values()) {
        for (auto& record : value)
            records.append(record);
    }
    std::sort(records.begin(), records.end(), [&](const auto& a, const auto& b) {
        return a.get().insertionTime < b.get().insertionTime;
    });
    for (auto& record : records)
        record.get().identifier = ++m_nextRecordIdentifier;

    if (error) {
        m_state = State::Uninitialized;
        callback(error.value());
        auto callbacks = WTFMove(m_pendingOpeningCallbacks);
        for (auto& callback : callbacks)
            callback(error.value());
        return;
    }
    m_state = State::Open;

    callback(std::nullopt);
    auto callbacks = WTFMove(m_pendingOpeningCallbacks);
    for (auto& callback : callbacks)
        callback(std::nullopt);
}

class ReadRecordTaskCounter : public RefCounted<ReadRecordTaskCounter> {
public:
    using ReadRecordsCallback = WTF::Function<void(Vector<Record>&&, Vector<uint64_t>&&)>;
    static Ref<ReadRecordTaskCounter> create(ReadRecordsCallback&& callback) { return adoptRef(*new ReadRecordTaskCounter(WTFMove(callback))); }

    ~ReadRecordTaskCounter()
    {
        ASSERT(RunLoop::isMain());
        if (!m_callback)
            return;
        std::sort(m_records.begin(), m_records.end(), [&] (const auto& a, const auto& b) {
            return a.identifier < b.identifier;
        });
        m_callback(WTFMove(m_records), WTFMove(m_failedRecords));
    }

    void appendRecord(Expected<Record, Error>&& result, uint64_t recordIdentifier, uint64_t updateCounter)
    {
        ASSERT(RunLoop::isMain());
        if (!result.has_value()) {
            m_failedRecords.append(recordIdentifier);
            return;
        }
        result.value().identifier = recordIdentifier;
        result.value().updateResponseCounter = updateCounter;
        m_records.append(WTFMove(result.value()));
    }

private:
    explicit ReadRecordTaskCounter(ReadRecordsCallback&& callback)
        : m_callback(WTFMove(callback))
    {
    }

    ReadRecordsCallback m_callback;
    Vector<Record> m_records;
    Vector<uint64_t> m_failedRecords;
};

void Cache::retrieveRecord(const RecordInformation& record, Ref<ReadRecordTaskCounter>&& taskCounter)
{
    readRecordFromDisk(record, [caches = Ref { m_caches }, identifier = m_identifier, recordIdentifier = record.identifier, updateCounter = record.updateResponseCounter, taskCounter = WTFMove(taskCounter)](Expected<Record, Error>&& result) mutable {
        auto* cache = caches->find(identifier);
        if (!cache)
            return;
        taskCounter->appendRecord(WTFMove(result), recordIdentifier, updateCounter);
    });
}

void Cache::retrieveRecords(const RetrieveRecordsOptions& options, RecordsCallback&& callback)
{
    ASSERT(m_state == State::Open);

    auto taskCounter = ReadRecordTaskCounter::create([caches = Ref { m_caches }, identifier = m_identifier, options, callback = WTFMove(callback)](Vector<Record>&& records, Vector<uint64_t>&& failedRecordIdentifiers) mutable {
        auto* cache = caches->find(identifier);
        if (cache)
            cache->removeFromRecordList(failedRecordIdentifiers);

        // https://w3c.github.io/ServiceWorker/#dom-cache-matchall (Step 5.4)
        for (auto& record : records) {
            if (record.response.type() != ResourceResponse::Type::Opaque)
                continue;

            if (validateCrossOriginResourcePolicy(options.crossOriginEmbedderPolicy.value, options.sourceOrigin, record.request.url(), record.response, ForNavigation::No)) {
                callback(makeUnexpected(DOMCacheEngine::Error::CORP));
                return;
            }
        }

        if (!options.shouldProvideResponse) {
            for (auto& record : records) {
                record.response = { };
                record.responseBody = nullptr;
                record.responseBodySize = 0;
            }
        }
        callback(WTFMove(records));
    });

    if (options.request.url().isNull()) {
        for (auto& records : m_records.values()) {
            for (auto& record : records)
                retrieveRecord(record, taskCounter.copyRef());
        }
        return;
    }

    if (!options.ignoreMethod && options.request.httpMethod() != "GET"_s)
        return;

    auto* records = recordsFromURL(options.request.url());
    if (!records)
        return;

    CacheQueryOptions queryOptions { options.ignoreSearch, options.ignoreMethod, options.ignoreVary };
    for (auto& record : *records) {
        if (DOMCacheEngine::queryCacheMatch(options.request, record.url, record.hasVaryStar, record.varyHeaders, queryOptions))
            retrieveRecord(record, taskCounter.copyRef());
    }
}

RecordInformation& Cache::addRecord(Vector<RecordInformation>* records, const Record& record)
{
    if (!records) {
        auto key = computeKeyURL(record.request.url());
        ASSERT(!m_records.contains(key));
        records = &m_records.set(key, Vector<RecordInformation> { }).iterator->value;
    }
    records->append(toRecordInformation(record));
    return records->last();
}

Vector<RecordInformation>* Cache::recordsFromURL(const URL& url)
{
    auto iterator = m_records.find(computeKeyURL(url));
    if (iterator == m_records.end())
        return nullptr;
    return &iterator->value;
}

const Vector<RecordInformation>* Cache::recordsFromURL(const URL& url) const
{
    auto iterator = m_records.find(computeKeyURL(url));
    if (iterator == m_records.end())
        return nullptr;
    return &iterator->value;
}

class AsynchronousPutTaskCounter : public RefCounted<AsynchronousPutTaskCounter> {
public:
    static Ref<AsynchronousPutTaskCounter> create(RecordIdentifiersCallback&& callback) { return adoptRef(*new AsynchronousPutTaskCounter(WTFMove(callback))); }
    ~AsynchronousPutTaskCounter()
    {
        ASSERT(RunLoop::isMain());
        if (!m_callback)
            return;
        if (m_error) {
            m_callback(makeUnexpected(m_error.value()));
            return;
        }
        m_callback(WTFMove(m_recordIdentifiers));
    }

    void setError(Error error)
    {
        ASSERT(RunLoop::isMain());
        if (m_error)
            return;

        m_error = error;
    }

    void addRecordIdentifier(uint64_t identifier)
    {
        m_recordIdentifiers.append(identifier);
    }

private:
    explicit AsynchronousPutTaskCounter(RecordIdentifiersCallback&& callback)
        : m_callback(WTFMove(callback))
    {
    }

    std::optional<Error> m_error;
    RecordIdentifiersCallback m_callback;
    Vector<uint64_t> m_recordIdentifiers;
};

void Cache::storeRecords(Vector<Record>&& records, RecordIdentifiersCallback&& callback)
{
    auto taskCounter = AsynchronousPutTaskCounter::create(WTFMove(callback));

    WebCore::CacheQueryOptions options;
    for (auto& record : records) {
        auto* sameURLRecords = recordsFromURL(record.request.url());
        auto matchingRecords = queryCache(sameURLRecords, record.request, options);

        auto position = !matchingRecords.isEmpty() ? sameURLRecords->findIf([&](const auto& item) { return item.identifier == matchingRecords[0]; }) : notFound;

        if (position == notFound) {
            record.identifier = ++m_nextRecordIdentifier;
            taskCounter->addRecordIdentifier(record.identifier);

            auto& recordToWrite = addRecord(sameURLRecords, record);
            writeRecordToDisk(recordToWrite, WTFMove(record), taskCounter.copyRef(), 0);
        } else {
            auto& existingRecord = sameURLRecords->at(position);
            taskCounter->addRecordIdentifier(existingRecord.identifier);
            updateRecordToDisk(existingRecord, WTFMove(record), taskCounter.copyRef());
        }
    }
}

void Cache::put(Vector<Record>&& records, RecordIdentifiersCallback&& callback)
{
    ASSERT(m_state == State::Open);

    WebCore::CacheQueryOptions options;
    CheckedUint64 spaceRequired = 0;

    for (auto& record : records) {
        auto* sameURLRecords = recordsFromURL(record.request.url());
        auto matchingRecords = queryCache(sameURLRecords, record.request, options);

        auto position = (sameURLRecords && !matchingRecords.isEmpty()) ? sameURLRecords->findIf([&](const auto& item) { return item.identifier == matchingRecords[0]; }) : notFound;

        spaceRequired += record.responseBodySize;
        if (position != notFound) {
            uint64_t spaceDecreased = sameURLRecords->at(position).size;
            if (spaceRequired >= spaceDecreased)
                spaceRequired -= spaceDecreased;
        }
    }

    if (spaceRequired.hasOverflowed()) {
        callback(makeUnexpected(DOMCacheEngine::Error::QuotaExceeded));
        return;
    }

    m_caches.requestSpace(spaceRequired, [caches = Ref { m_caches }, identifier = m_identifier, records = WTFMove(records), callback = WTFMove(callback)](std::optional<DOMCacheEngine::Error>&& error) mutable {
        if (error) {
            callback(makeUnexpected(error.value()));
            return;
        }
        auto* cache = caches->find(identifier);
        if (!cache) {
            callback(makeUnexpected(DOMCacheEngine::Error::Internal));
            return;
        }
        cache->storeRecords(WTFMove(records), WTFMove(callback));
    });
}

void Cache::remove(WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, RecordIdentifiersCallback&& callback)
{
    ASSERT(m_state == State::Open);

    auto* records = recordsFromURL(request.url());
    auto recordIdentifiers = queryCache(records, request, options);
    if (recordIdentifiers.isEmpty()) {
        callback({ });
        return;
    }

    records->removeAllMatching([this, &recordIdentifiers](auto& item) {
        bool shouldRemove = recordIdentifiers.findIf([&item](auto identifier) { return identifier == item.identifier; }) != notFound;
        if (shouldRemove)
            this->removeRecordFromDisk(item);
        return shouldRemove;
    });

    // This operation would change caches size, so make sure callback finishes after size file is updated.
    m_caches.updateSizeFile([callback = WTFMove(callback), recordIdentifiers = WTFMove(recordIdentifiers)]() mutable {
        callback(WTFMove(recordIdentifiers));
    });
}

void Cache::removeFromRecordList(const Vector<uint64_t>& recordIdentifiers)
{
    if (recordIdentifiers.isEmpty())
        return;

    for (auto& records : m_records.values()) {
        auto* cache = this;
        records.removeAllMatching([cache, &recordIdentifiers](const auto& item) {
            return notFound != recordIdentifiers.findIf([cache, &item](const auto& identifier) {
                if (item.identifier != identifier)
                    return false;
                cache->removeRecordFromDisk(item);
                return true;
            });
        });
    }
}

void Cache::writeRecordToDisk(const RecordInformation& recordInformation, Record&& record, Ref<AsynchronousPutTaskCounter>&& taskCounter, uint64_t previousRecordSize)
{
    m_caches.writeRecord(*this, recordInformation, WTFMove(record), previousRecordSize, [taskCounter = WTFMove(taskCounter)](std::optional<Error>&& error) {
        if (error)
            taskCounter->setError(error.value());
    });
}

void Cache::updateRecordToDisk(RecordInformation& existingRecord, Record&& record, Ref<AsynchronousPutTaskCounter>&& taskCounter)
{
    ++existingRecord.updateResponseCounter;
    readRecordFromDisk(existingRecord, [caches = Ref { m_caches }, identifier = m_identifier, recordIdentifier = existingRecord.identifier, record = WTFMove(record), taskCounter = WTFMove(taskCounter)](Expected<Record, Error>&& result) mutable {
        if (!result.has_value())
            return;

        auto* cache = caches->find(identifier);
        if (!cache)
            return;

        auto* sameURLRecords = cache->recordsFromURL(result.value().request.url());
        if (!sameURLRecords)
            return;

        auto position = sameURLRecords->findIf([&] (const auto& item) { return item.identifier == recordIdentifier; });
        if (position == notFound)
            return;
        auto& recordInfo = sameURLRecords->at(position);
        auto previousSize = recordInfo.size;
        recordInfo.size = record.responseBodySize;

        auto& recordFromDisk = result.value();
        record.requestHeadersGuard = recordFromDisk.requestHeadersGuard;
        record.request = WTFMove(recordFromDisk.request);
        record.options = WTFMove(recordFromDisk.options);
        record.referrer = WTFMove(recordFromDisk.referrer);

        updateVaryInformation(recordInfo, record.request, record.response);

        cache->writeRecordToDisk(recordInfo, WTFMove(record), WTFMove(taskCounter), previousSize);
    });
}

void Cache::readRecordFromDisk(const RecordInformation& record, WTF::Function<void(Expected<Record, Error>&&)>&& callback)
{
    m_caches.readRecord(record.key, WTFMove(callback));
}

void Cache::removeRecordFromDisk(const RecordInformation& record)
{
    m_caches.removeRecord(record);
}

Storage::Record Cache::encode(const RecordInformation& recordInformation, const Record& record)
{
    WTF::Persistence::Encoder encoder;
    encoder << recordInformation.insertionTime;
    encoder << recordInformation.size;
    encoder << record.requestHeadersGuard;
    encoder << record.request;
    record.options.encodePersistent(encoder);
    encoder << record.referrer;

    encoder << record.responseHeadersGuard;
    encoder << record.response;
    encoder << record.responseBodySize;

    encoder.encodeChecksum();

    Data header(encoder.buffer(), encoder.bufferSize());
    Data body;
    WTF::switchOn(record.responseBody, [](const Ref<WebCore::FormData>& formData) {
        // FIXME: Store form data body.
    }, [&](const Ref<WebCore::SharedBuffer>& buffer) {
        body = { buffer->data(), buffer->size() };
    }, [](const std::nullptr_t&) {
    });

    return { recordInformation.key, { }, header, body, { } };
}

static std::optional<WebCore::DOMCacheEngine::Record> decodeDOMCacheRecord(WTF::Persistence::Decoder& decoder)
{
    std::optional<FetchHeaders::Guard> requestHeadersGuard;
    decoder >> requestHeadersGuard;
    if (!requestHeadersGuard)
        return std::nullopt;
    
    std::optional<ResourceRequest> request;
    decoder >> request;
    if (!request)
        return std::nullopt;
    
    FetchOptions options;
    if (!FetchOptions::decodePersistent(decoder, options))
        return std::nullopt;
    
    std::optional<String> referrer;
    decoder >> referrer;
    if (!referrer)
        return std::nullopt;
    
    std::optional<FetchHeaders::Guard> responseHeadersGuard;
    decoder >> responseHeadersGuard;
    if (!responseHeadersGuard)
        return std::nullopt;

    ResourceResponse response;
    if (!ResourceResponse::decode(decoder, response))
        return std::nullopt;
    
    std::optional<uint64_t> responseBodySize;
    decoder >> responseBodySize;
    if (!responseBodySize)
        return std::nullopt;

    if (!decoder.verifyChecksum())
        return std::nullopt;

    return {{
        0,
        0,
        WTFMove(*requestHeadersGuard),
        WTFMove(*request),
        WTFMove(options),
        WTFMove(*referrer),
        WTFMove(*responseHeadersGuard),
        WTFMove(response),
        { },
        WTFMove(*responseBodySize)
    }};
}

std::optional<Cache::DecodedRecord> Cache::decodeRecordHeader(const Storage::Record& storage)
{
    WTF::Persistence::Decoder decoder(storage.header.span());

    std::optional<double> insertionTime;
    decoder >> insertionTime;
    if (!insertionTime)
        return std::nullopt;

    std::optional<uint64_t> size;
    decoder >> size;
    if (!size)
        return std::nullopt;

    std::optional<WebCore::DOMCacheEngine::Record> record = decodeDOMCacheRecord(decoder);
    if (!record)
        return std::nullopt;

    return {{
        WTFMove(*insertionTime),
        WTFMove(*size),
        WTFMove(*record)
    }};
}

std::optional<Record> Cache::decode(const Storage::Record& storage)
{
    auto result = decodeRecordHeader(storage);

    if (!result)
        return std::nullopt;

    auto record = WTFMove(result->record);
    record.responseBody = WebCore::SharedBuffer::create(storage.body.data(), storage.body.size());

    return record;
}

Vector<Key> Cache::keys() const
{
    Vector<Key> keys;
    for (auto& records : m_records.values()) {
        for (auto& record : records)
            keys.append(record.key);
    }
    return keys;
}

} // namespace CacheStorage

} // namespace WebKit
