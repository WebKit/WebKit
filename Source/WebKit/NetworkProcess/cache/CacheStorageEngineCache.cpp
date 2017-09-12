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

#include "CacheStorageEngineCaches.h"
#include "NetworkCacheIOChannel.h"
#include "NetworkCacheKey.h"
#include "NetworkProcess.h"
#include <WebCore/CacheQueryOptions.h>
#include <WebCore/HTTPParsers.h>
#include <pal/SessionID.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>


using namespace WebCore;
using namespace WebCore::DOMCacheEngine;
using namespace WebKit::NetworkCache;

namespace WebKit {

namespace CacheStorage {

static inline Vector<uint64_t> queryCache(const Vector<RecordData>* records, const ResourceRequest& request, const CacheQueryOptions& options)
{
    if (!records)
        return { };

    if (!options.ignoreMethod && request.httpMethod() != "GET")
        return { };

    Vector<uint64_t> results;
    for (const auto& record : *records) {
        if (WebCore::DOMCacheEngine::queryCacheMatch(request, record.url, record.hasVaryStar, record.varyHeaders, options))
            results.append(record.identifier);
    }
    return results;
}

static inline void updateVaryInformation(RecordData& recordData)
{
    auto varyValue = recordData.data->response.httpHeaderField(WebCore::HTTPHeaderName::Vary);
    if (varyValue.isNull()) {
        recordData.hasVaryStar = false;
        recordData.varyHeaders = { };
        return;
    }

    varyValue.split(',', false, [&](StringView view) {
        if (!recordData.hasVaryStar && stripLeadingAndTrailingHTTPSpaces(view) == "*")
            recordData.hasVaryStar = true;
        String headerName = view.toString();
        recordData.varyHeaders.add(headerName, recordData.data->request.httpHeaderField(headerName));
    });

    if (recordData.hasVaryStar)
        recordData.varyHeaders = { };
}

static inline Record toRecord(const RecordData& record)
{
    return { record.identifier, record.updateResponseCounter, record.data->requestHeadersGuard, record.data->request, record.data->options, record.data->referrer, record.data->responseHeadersGuard, record.data->response, copyResponseBody(record.data->responseBody) };
}

static inline RecordData toRecordData(Record&& record)
{
    auto url = record.request.url();
    RecordData::Data data = { record.requestHeadersGuard, WTFMove(record.request), WTFMove(record.options), WTFMove(record.referrer), WTFMove(record.responseHeadersGuard), WTFMove(record.response), WTFMove(record.responseBody) };
    RecordData recordData = { { }, monotonicallyIncreasingTimeMS(), record.identifier, 0, url, false, { }, WTFMove(data) };

    updateVaryInformation(recordData);

    return recordData;
}

Cache::Cache(Caches& caches, uint64_t identifier, State state, String&& name)
    : m_caches(caches)
    , m_state(state)
    , m_identifier(identifier)
    , m_name(WTFMove(name))
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
    readRecordsList([caches = makeRef(m_caches), identifier = m_identifier, callback = WTFMove(callback)](std::optional<Error>&& error) mutable {
        auto* cache = caches->find(identifier);
        if (!cache) {
            callback(Error::Internal);
            return;
        }
        cache->finishOpening(WTFMove(callback), WTFMove(error));
    });
}

void Cache::finishOpening(CompletionCallback&& callback, std::optional<Error>&& error)
{
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

Vector<Record> Cache::retrieveRecords(const URL& url) const
{
    if (url.isNull()) {
        Vector<Record> result;
        for (auto& records : m_records.values()) {
            for (auto& record : records)
                result.append(toRecord(record));
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a.identifier < b.identifier;
        });
        return result;
    }

    const auto* records = recordsFromURL(url);
    if (!records)
        return { };

    Vector<Record> result;
    result.reserveInitialCapacity(records->size());
    for (auto& record : *records)
        result.append(toRecord(record));
    return result;
}

static inline String computeKeyURL(const URL& url)
{
    URL keyURL = url;
    if (keyURL.hasQuery())
        keyURL.setQuery({ });
    keyURL.removeFragmentIdentifier();
    return keyURL;
}

RecordData& Cache::addRecord(Vector<RecordData>* records, Record&& record)
{
    if (!records) {
        auto key = computeKeyURL(record.request.url());
        ASSERT(!m_records.contains(key));
        records = &m_records.set(key, Vector<RecordData> { }).iterator->value;
    }
    records->append(toRecordData(WTFMove(record)));
    return records->last();
}

Vector<RecordData>* Cache::recordsFromURL(const URL& url)
{
    auto iterator = m_records.find(computeKeyURL(url));
    if (iterator == m_records.end())
        return nullptr;
    return &iterator->value;
}

const Vector<RecordData>* Cache::recordsFromURL(const URL& url) const
{
    auto iterator = m_records.find(computeKeyURL(url));
    if (iterator == m_records.end())
        return nullptr;
    return &iterator->value;
}

void Cache::put(Vector<Record>&& records, RecordIdentifiersCallback&& callback)
{
    bool shouldWriteRecordList { false };
    WebCore::CacheQueryOptions options;
    Vector<uint64_t> recordIdentifiers;
    recordIdentifiers.reserveInitialCapacity(records.size());
    for (auto& record : records) {
        auto* sameURLRecords = recordsFromURL(record.request.url());

        auto matchingRecords = queryCache(sameURLRecords, record.request, options);
        if (matchingRecords.isEmpty()) {
            record.identifier = ++m_nextRecordIdentifier;
            recordIdentifiers.uncheckedAppend(record.identifier);

            shouldWriteRecordList = true;
            auto& recordToWrite = addRecord(sameURLRecords, WTFMove(record));
            writeRecordToDisk(recordToWrite);
        } else {
            auto identifier = matchingRecords[0];
            auto position = sameURLRecords->findMatching([&](const auto& item) { return item.identifier == identifier; });
            ASSERT(position != notFound);
            if (position != notFound) {
                auto& existingRecord = (*sameURLRecords)[position];
                recordIdentifiers.uncheckedAppend(identifier);
                ++existingRecord.updateResponseCounter;

                // FIXME: Handle the case where data is null.
                ASSERT(existingRecord.data);
                existingRecord.data->responseHeadersGuard = record.responseHeadersGuard;
                existingRecord.data->response = WTFMove(record.response);
                existingRecord.data->responseBody = WTFMove(record.responseBody);
                updateVaryInformation(existingRecord);
                writeRecordToDisk(existingRecord);
            }
        }
    }

    if (!shouldWriteRecordList) {
        callback(WTFMove(recordIdentifiers));
        return;
    }
    writeRecordsList([callback = WTFMove(callback), recordIdentifiers = WTFMove(recordIdentifiers)](std::optional<Error>&& error) mutable {
        if (error) {
            callback(makeUnexpected(error.value()));
            return;
        }
        callback(WTFMove(recordIdentifiers));
    });
}

void Cache::remove(WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, RecordIdentifiersCallback&& callback)
{
    auto* records = recordsFromURL(request.url());
    auto recordIdentifiers = queryCache(records, request, options);
    if (recordIdentifiers.isEmpty()) {
        callback({ });
        return;
    }

    records->removeAllMatching([this, &recordIdentifiers](auto& item) {
        bool shouldRemove = recordIdentifiers.findMatching([&item](auto identifier) { return identifier == item.identifier; }) != notFound;
        if (shouldRemove)
            this->removeRecordFromDisk(item);
        return shouldRemove;
    });

    writeRecordsList([callback = WTFMove(callback), recordIdentifiers = WTFMove(recordIdentifiers)](std::optional<Error>&& error) mutable {
        if (error) {
            callback(makeUnexpected(error.value()));
            return;
        }
        callback(WTFMove(recordIdentifiers));
    });
}

void Cache::readRecordsList(CompletionCallback&& callback)
{
    // FIXME: Implement this.
    callback(std::nullopt);
}

void Cache::writeRecordsList(CompletionCallback&& callback)
{
    // FIXME: Implement this.
    callback(std::nullopt);
}

void Cache::writeRecordToDisk(RecordData&)
{
    // FIXME: Implement this.
}

void Cache::removeRecordFromDisk(RecordData&)
{
    // FIXME: Implement this.
}

} // namespace CacheStorage

} // namespace WebKit
