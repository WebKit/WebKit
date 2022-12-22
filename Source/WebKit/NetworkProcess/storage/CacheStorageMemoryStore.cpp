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
#include "CacheStorageMemoryStore.h"

#include <WebCore/DOMCacheEngine.h>

namespace WebKit {

Ref<CacheStorageMemoryStore> CacheStorageMemoryStore::create()
{
    return adoptRef(*new CacheStorageMemoryStore);
}

void CacheStorageMemoryStore::readAllRecords(ReadAllRecordsCallback&& callback)
{
    callback(WTF::map(m_records.values(), [](const auto& record) {
        RELEASE_ASSERT(record);
        return record->copy();
    }));
}

void CacheStorageMemoryStore::readRecords(const Vector<CacheStorageRecordInformation>& recordInfos, ReadRecordsCallback&& callback)
{
    auto result = WTF::map(recordInfos, [&](auto& recordInfo) -> std::optional<CacheStorageRecord> {
        auto iterator = m_records.find(recordInfo.identifier);
        if (iterator == m_records.end())
            return std::nullopt;
        return { iterator->value->copy() };
    });
    return callback(WTFMove(result));
}

void CacheStorageMemoryStore::deleteRecords(const Vector<CacheStorageRecordInformation>& recordInfos, WriteRecordsCallback&& callback)
{
    for (auto& recordInfo : recordInfos)
        m_records.remove(recordInfo.identifier);

    callback(true);
}

void CacheStorageMemoryStore::writeRecords(Vector<CacheStorageRecord>&& records, WriteRecordsCallback&& callback)
{
    for (auto&& record : records)
        m_records.set(record.info.identifier, makeUnique<CacheStorageRecord>(WTFMove(record)));

    callback(true);
}

}

