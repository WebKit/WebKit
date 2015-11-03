/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "MemoryObjectStore.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBKeyRangeData.h"
#include "Logging.h"
#include "MemoryBackingStoreTransaction.h"

namespace WebCore {
namespace IDBServer {

std::unique_ptr<MemoryObjectStore> MemoryObjectStore::create(const IDBObjectStoreInfo& info)
{
    return std::make_unique<MemoryObjectStore>(info);
}

MemoryObjectStore::MemoryObjectStore(const IDBObjectStoreInfo& info)
    : m_info(info)
{
}

MemoryObjectStore::~MemoryObjectStore()
{
    ASSERT(!m_writeTransaction);
}

void MemoryObjectStore::writeTransactionStarted(MemoryBackingStoreTransaction& transaction)
{
    LOG(IndexedDB, "MemoryObjectStore::writeTransactionStarted");

    ASSERT(!m_writeTransaction);
    m_writeTransaction = &transaction;
}

void MemoryObjectStore::writeTransactionFinished(MemoryBackingStoreTransaction& transaction)
{
    LOG(IndexedDB, "MemoryObjectStore::writeTransactionFinished");

    ASSERT_UNUSED(transaction, m_writeTransaction == &transaction);
    m_writeTransaction = nullptr;
}

bool MemoryObjectStore::containsRecord(const IDBKeyData& key)
{
    if (!m_keyValueStore)
        return false;

    return m_keyValueStore->contains(key);
}

void MemoryObjectStore::clear()
{
    LOG(IndexedDB, "MemoryObjectStore::clear");
    ASSERT(m_writeTransaction);

    m_writeTransaction->objectStoreCleared(*this, WTF::move(m_keyValueStore));
}

void MemoryObjectStore::replaceKeyValueStore(std::unique_ptr<KeyValueMap>&& store)
{
    ASSERT(m_writeTransaction);
    ASSERT(m_writeTransaction->isAborting());

    m_keyValueStore = WTF::move(store);
}

void MemoryObjectStore::deleteRecord(const IDBKeyData& key)
{
    LOG(IndexedDB, "MemoryObjectStore::deleteRecord");

    ASSERT(m_writeTransaction);
    m_writeTransaction->recordValueChanged(*this, key);

    if (!m_keyValueStore)
        return;

    ASSERT(m_orderedKeys);

    m_keyValueStore->remove(key);
    m_orderedKeys->erase(key);
}

void MemoryObjectStore::deleteRange(const IDBKeyRangeData& inputRange)
{
    LOG(IndexedDB, "MemoryObjectStore::deleteRange");

    ASSERT(m_writeTransaction);

    if (inputRange.isExactlyOneKey()) {
        deleteRecord(inputRange.lowerKey);
        return;
    }

    IDBKeyRangeData range = inputRange;
    while (true) {
        auto key = lowestKeyWithRecordInRange(range);
        if (key.isNull())
            break;

        deleteRecord(key);

        range.lowerKey = key;
        range.lowerOpen = true;
    }
}

void MemoryObjectStore::putRecord(MemoryBackingStoreTransaction& transaction, const IDBKeyData& keyData, const ThreadSafeDataBuffer& value)
{
    LOG(IndexedDB, "MemoryObjectStore::putRecord");

    ASSERT(m_writeTransaction);
    ASSERT_UNUSED(transaction, m_writeTransaction == &transaction);

    m_writeTransaction->recordValueChanged(*this, keyData);

    setKeyValue(keyData, value);
}

void MemoryObjectStore::setKeyValue(const IDBKeyData& keyData, const ThreadSafeDataBuffer& value)
{
    if (!m_keyValueStore) {
        ASSERT(!m_orderedKeys);
        m_keyValueStore = std::make_unique<KeyValueMap>();
        m_orderedKeys = std::make_unique<std::set<IDBKeyData>>();
    }

    auto result = m_keyValueStore->set(keyData, value);
    if (result.isNewEntry)
        m_orderedKeys->insert(keyData);
}

uint64_t MemoryObjectStore::countForKeyRange(const IDBKeyRangeData& inRange) const
{
    LOG(IndexedDB, "MemoryObjectStore::countForKeyRange");

    if (!m_keyValueStore)
        return 0;

    uint64_t count = 0;
    IDBKeyRangeData range = inRange;
    while (true) {
        auto key = lowestKeyWithRecordInRange(range);
        if (key.isNull())
            break;

        ++count;
        range.lowerKey = key;
        range.lowerOpen = true;
    }

    return count;
}

ThreadSafeDataBuffer MemoryObjectStore::valueForKeyRange(const IDBKeyRangeData& keyRangeData) const
{
    LOG(IndexedDB, "MemoryObjectStore::valueForKey");

    IDBKeyData key = lowestKeyWithRecordInRange(keyRangeData);
    if (key.isNull())
        return ThreadSafeDataBuffer();

    ASSERT(m_keyValueStore);
    return m_keyValueStore->get(key);
}

IDBKeyData MemoryObjectStore::lowestKeyWithRecordInRange(const IDBKeyRangeData& keyRangeData) const
{
    if (!m_keyValueStore)
        return { };

    if (keyRangeData.isExactlyOneKey() && m_keyValueStore->contains(keyRangeData.lowerKey))
        return keyRangeData.lowerKey;

    ASSERT(m_orderedKeys);

    auto lowestInRange = m_orderedKeys->lower_bound(keyRangeData.lowerKey);

    if (lowestInRange == m_orderedKeys->end())
        return { };

    if (keyRangeData.lowerOpen && *lowestInRange == keyRangeData.lowerKey)
        ++lowestInRange;

    if (lowestInRange == m_orderedKeys->end())
        return { };

    if (!keyRangeData.upperKey.isNull()) {
        if (lowestInRange->compare(keyRangeData.upperKey) > 0)
            return { };
        if (keyRangeData.upperOpen && *lowestInRange == keyRangeData.upperKey)
            return { };
    }

    return *lowestInRange;
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
