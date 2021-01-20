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
#include "MemoryObjectStoreCursor.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBGetResult.h"
#include "Logging.h"
#include "MemoryObjectStore.h"

namespace WebCore {
namespace IDBServer {

MemoryObjectStoreCursor::MemoryObjectStoreCursor(MemoryObjectStore& objectStore, const IDBCursorInfo& info)
    : MemoryCursor(info)
    , m_objectStore(objectStore)
    , m_remainingRange(info.range())
{
    LOG(IndexedDB, "MemoryObjectStoreCursor::MemoryObjectStoreCursor %s", info.range().loggingString().utf8().data());

    auto* orderedKeys = objectStore.orderedKeys();
    if (!orderedKeys)
        return;

    setFirstInRemainingRange(*orderedKeys);
}

void MemoryObjectStoreCursor::objectStoreCleared()
{
    m_iterator = WTF::nullopt;
}

void MemoryObjectStoreCursor::keyDeleted(const IDBKeyData& key)
{
    if (m_currentPositionKey != key)
        return;

    m_iterator = WTF::nullopt;
}

void MemoryObjectStoreCursor::keyAdded(IDBKeyDataSet::iterator iterator)
{
    if (m_iterator)
        return;

    if (*iterator == m_currentPositionKey)
        m_iterator = iterator;
}

void MemoryObjectStoreCursor::setFirstInRemainingRange(IDBKeyDataSet& set)
{
    m_iterator = WTF::nullopt;

    if (m_info.isDirectionForward()) {
        setForwardIteratorFromRemainingRange(set);
        if (m_iterator) {
            m_remainingRange.lowerKey = **m_iterator;
            m_remainingRange.lowerOpen = true;
        }
    } else {
        setReverseIteratorFromRemainingRange(set);
        if (m_iterator) {
            m_remainingRange.upperKey = **m_iterator;
            m_remainingRange.upperOpen = true;
        }
    }

    ASSERT(!m_iterator || *m_iterator != set.end());
}

void MemoryObjectStoreCursor::setForwardIteratorFromRemainingRange(IDBKeyDataSet& set)
{
    if (!set.size()) {
        m_iterator = WTF::nullopt;
        return;
    }

    if (m_remainingRange.isExactlyOneKey()) {
        m_iterator = set.find(m_remainingRange.lowerKey);
        if (*m_iterator == set.end())
            m_iterator = WTF::nullopt;

        return;
    }

    m_iterator = WTF::nullopt;

    auto lowest = set.lower_bound(m_remainingRange.lowerKey);
    if (lowest == set.end())
        return;

    if (m_remainingRange.lowerOpen && *lowest == m_remainingRange.lowerKey) {
        ++lowest;
        if (lowest == set.end())
            return;
    }

    if (!m_remainingRange.upperKey.isNull()) {
        if (lowest->compare(m_remainingRange.upperKey) > 0)
            return;

        if (m_remainingRange.upperOpen && *lowest == m_remainingRange.upperKey)
            return;
    }

    m_iterator = lowest;
}

void MemoryObjectStoreCursor::setReverseIteratorFromRemainingRange(IDBKeyDataSet& set)
{
    if (!set.size()) {
        m_iterator = WTF::nullopt;
        return;
    }

    if (m_remainingRange.isExactlyOneKey()) {
        m_iterator = set.find(m_remainingRange.lowerKey);
        if (*m_iterator == set.end())
            m_iterator = WTF::nullopt;

        return;
    }

    if (!m_remainingRange.upperKey.isValid()) {
        m_iterator = --set.end();
        if (!m_remainingRange.containsKey(**m_iterator))
            m_iterator = WTF::nullopt;

        return;
    }

    m_iterator = WTF::nullopt;

    // This is one record past the actual key we're looking for.
    auto highest = set.upper_bound(m_remainingRange.upperKey);

    if (highest == set.begin())
        return;

    // This is one record before that, which *is* the key we're looking for.
    --highest;

    if (m_remainingRange.upperOpen && *highest == m_remainingRange.upperKey) {
        if (highest == set.begin())
            return;
        --highest;
    }

    if (!m_remainingRange.lowerKey.isNull()) {
        if (highest->compare(m_remainingRange.lowerKey) < 0)
            return;

        if (m_remainingRange.lowerOpen && *highest == m_remainingRange.lowerKey)
            return;
    }

    m_iterator = highest;
}

void MemoryObjectStoreCursor::currentData(IDBGetResult& data)
{
    if (!m_iterator) {
        m_currentPositionKey = { };
        data = { };
        return;
    }

    m_currentPositionKey = **m_iterator;
    if (m_info.cursorType() == IndexedDB::CursorType::KeyOnly)
        data = { m_currentPositionKey, m_currentPositionKey };
    else {
        IDBValue value = { m_objectStore.valueForKeyRange(m_currentPositionKey), { }, { } };
        data = { m_currentPositionKey, m_currentPositionKey, WTFMove(value), m_objectStore.info().keyPath() };
    }
}

void MemoryObjectStoreCursor::incrementForwardIterator(IDBKeyDataSet& set, const IDBKeyData& key, uint32_t count)
{
    // We might need to re-grab the current iterator.
    // e.g. If the record it was pointed to had been deleted.
    bool didResetIterator = false;
    if (!m_iterator) {
        if (!m_currentPositionKey.isValid())
            return;

        m_remainingRange.lowerKey = m_currentPositionKey;
        m_remainingRange.lowerOpen = false;
        setFirstInRemainingRange(set);

        didResetIterator = true;
    }

    if (!m_iterator)
        return;

    ASSERT(*m_iterator != set.end());

    if (key.isValid()) {
        // If iterating to a key, the count passed in must be zero, as there is no way to iterate by both a count and to a key.
        ASSERT(!count);

        if (!m_info.range().containsKey(key))
            return;

        if ((*m_iterator)->compare(key) < 0) {
            m_remainingRange.lowerKey = key;
            m_remainingRange.lowerOpen = false;
            setFirstInRemainingRange(set);
        }

        return;
    }

    if (!count)
        count = 1;

    // If the forwardIterator was reset because it's previous record had been deleted,
    // we might have already advanced past the current position, eating one one of the iteration counts.
    if (didResetIterator && (*m_iterator)->compare(m_currentPositionKey) > 0)
        --count;

    while (count) {
        --count;
        ++*m_iterator;

        if (*m_iterator == set.end() || !m_info.range().containsKey(**m_iterator)) {
            m_iterator = WTF::nullopt;
            return;
        }
    }
}

void MemoryObjectStoreCursor::incrementReverseIterator(IDBKeyDataSet& set, const IDBKeyData& key, uint32_t count)
{
    // We might need to re-grab the current iterator.
    // e.g. If the record it was pointed to had been deleted.
    bool didResetIterator = false;
    if (!m_iterator) {
        if (!m_currentPositionKey.isValid())
            return;

        m_remainingRange.upperKey = m_currentPositionKey;
        m_remainingRange.upperOpen = false;
        setFirstInRemainingRange(set);

        didResetIterator = true;
    }

    if (!m_iterator || *m_iterator == set.end())
        return;

    if (key.isValid()) {
        // If iterating to a key, the count passed in must be zero, as there is no way to iterate by both a count and to a key.
        ASSERT(!count);

        if (!m_info.range().containsKey(key))
            return;

        if ((*m_iterator)->compare(key) > 0) {
            m_remainingRange.upperKey = key;
            m_remainingRange.upperOpen = false;

            setFirstInRemainingRange(set);
        }

        return;
    }

    if (!count)
        count = 1;

    // If the reverseIterator was reset because it's previous record had been deleted,
    // we might have already advanced past the current position, eating one one of the iteration counts.
    if (didResetIterator && (*m_iterator)->compare(m_currentPositionKey) < 0)
        --count;

    while (count) {
        if (*m_iterator == set.begin()) {
            m_iterator = WTF::nullopt;
            return;
        }

        --count;
        --*m_iterator;

        if (!m_info.range().containsKey(**m_iterator)) {
            m_iterator = WTF::nullopt;
            return;
        }
    }
}

void MemoryObjectStoreCursor::iterate(const IDBKeyData& key, const IDBKeyData& primaryKeyData, uint32_t count, IDBGetResult& outData)
{
    LOG(IndexedDB, "MemoryObjectStoreCursor::iterate to key %s", key.loggingString().utf8().data());

    ASSERT_UNUSED(primaryKeyData, primaryKeyData.isNull());

    if (!m_objectStore.orderedKeys()) {
        m_currentPositionKey = { };
        outData = { };
        return;
    }

    if (key.isValid() && !m_info.range().containsKey(key)) {
        m_currentPositionKey = { };
        outData = { };
        return;
    }

    auto* set = m_objectStore.orderedKeys();
    if (set) {
        if (m_info.isDirectionForward())
            incrementForwardIterator(*set, key, count);
        else
            incrementReverseIterator(*set, key, count);
    }

    m_currentPositionKey = { };

    if (!m_iterator) {
        outData = { };
        return;
    }

    currentData(outData);
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
