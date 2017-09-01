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
#include "NetworkCacheKey.h"
#include "NetworkProcess.h"
#include <WebCore/CacheQueryOptions.h>
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

Cache::Cache(uint64_t identifier, String&& name)
    : m_identifier(identifier)
    , m_name(WTFMove(name))
{
}

Vector<Record> Cache::records() const
{
    Vector<Record> records;
    records.reserveInitialCapacity(m_records.size());
    for (auto& record : m_records)
        records.uncheckedAppend(record.copy());
    return records;
}

void Cache::put(Vector<Record>&& records, RecordIdentifiersCallback&& callback)
{
    bool shouldWriteRecordList { false };
    WebCore::CacheQueryOptions options;
    Vector<uint64_t> recordIdentifiers;
    recordIdentifiers.reserveInitialCapacity(records.size());
    for (auto& record : records) {
        auto matchingRecords = queryCache(record.request, options);
        if (matchingRecords.isEmpty()) {
            record.identifier = ++m_nextRecordIdentifier;
            recordIdentifiers.uncheckedAppend(record.identifier);
            m_records.append(WTFMove(record));

            shouldWriteRecordList = true;
            writeRecordToDisk(m_records.last());
        } else {
            auto identifier = matchingRecords[0];
            auto position = m_records.findMatching([&](const auto& item) { return item.identifier == identifier; });
            ASSERT(position != notFound);
            if (position != notFound) {
                auto& existingRecord = m_records[position];
                recordIdentifiers.uncheckedAppend(identifier);
                existingRecord.responseHeadersGuard = record.responseHeadersGuard;
                existingRecord.response = WTFMove(record.response);
                existingRecord.responseBody = WTFMove(record.responseBody);
                ++existingRecord.updateResponseCounter;

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
    auto recordIdentifiers = queryCache(request, options);
    if (recordIdentifiers.isEmpty()) {
        callback({ });
        return;
    }

    Vector<Record> recordsToKeep;
    for (auto& record : m_records) {
        if (recordIdentifiers.findMatching([&](auto item) { return item == record.identifier; }) == notFound)
            recordsToKeep.append(WTFMove(record));
        else
            removeRecordFromDisk(record);
    }
    m_records = WTFMove(recordsToKeep);

    writeRecordsList([callback = WTFMove(callback), recordIdentifiers = WTFMove(recordIdentifiers)](std::optional<Error>&& error) mutable {
        if (error) {
            callback(makeUnexpected(error.value()));
            return;
        }
        callback(WTFMove(recordIdentifiers));
    });
}

Vector<uint64_t> Cache::queryCache(const ResourceRequest& request, const CacheQueryOptions& options)
{
    if (!options.ignoreMethod && request.httpMethod() != "GET")
        return { };

    Vector<uint64_t> results;
    for (const auto& record : m_records) {
        if (WebCore::DOMCacheEngine::queryCacheMatch(request, record.request, record.response, options))
            results.append(record.identifier);
    }
    return results;
}

void Cache::writeRecordsList(CompletionCallback&& callback)
{
    // FIXME: Implement this.
    callback(std::nullopt);
}

void Cache::writeRecordToDisk(Record& record)
{
    // FIXME: Implement this.
}

void Cache::removeRecordFromDisk(Record& record)
{
    // FIXME: Implement this.
}

} // namespace CacheStorage

} // namespace WebKit
