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

#include "Logging.h"
#include "MemoryObjectStore.h"

namespace WebCore {
namespace IDBServer {

std::unique_ptr<MemoryObjectStoreCursor> MemoryObjectStoreCursor::create(MemoryObjectStore& objectStore, const IDBCursorInfo& info)
{
    return std::make_unique<MemoryObjectStoreCursor>(objectStore, info);
}

MemoryObjectStoreCursor::MemoryObjectStoreCursor(MemoryObjectStore& objectStore, const IDBCursorInfo& info)
    : MemoryCursor(info)
    , m_objectStore(objectStore)
{
    m_remainingRange = m_info.range();

    auto* orderedKeys = objectStore.orderedKeys();
    if (!orderedKeys)
        return;

    setFirstInRemainingRange(*orderedKeys);
}

void MemoryObjectStoreCursor::objectStoreCleared()
{
    clearIterators();
}

void MemoryObjectStoreCursor::keyDeleted(const IDBKeyData& key)
{
    if (m_currentPositionKey != key)
        return;

    clearIterators();
}

void MemoryObjectStoreCursor::keyAdded(std::set<IDBKeyData>::iterator iterator)
{
    ASSERT(m_currentPositionKey.isValid());

    if (hasIterators())
        return;

    if (*iterator != m_currentPositionKey)
        return;

    if (m_info.isDirectionForward())
        m_forwardIterator = iterator;
    else
        m_reverseIterator = --std::set<IDBKeyData>::reverse_iterator(iterator);
}

void MemoryObjectStoreCursor::setFirstInRemainingRange(std::set<IDBKeyData>& set)
{
    clearIterators();

    if (m_info.isDirectionForward()) {
        m_forwardIterator = firstForwardIteratorInRemainingRange(set);
        if (*m_forwardIterator != set.end()) {
            m_remainingRange.lowerKey = **m_forwardIterator;
            m_remainingRange.lowerOpen = true;
        }
    } else {
        m_reverseIterator = firstReverseIteratorInRemainingRange(set);
        if (*m_reverseIterator != set.rend()) {
            m_remainingRange.upperKey = **m_reverseIterator;
            m_remainingRange.upperOpen = true;
        }
    }
}

std::set<IDBKeyData>::iterator MemoryObjectStoreCursor::firstForwardIteratorInRemainingRange(std::set<IDBKeyData>& set)
{
    if (m_remainingRange.isExactlyOneKey())
        return set.find(m_remainingRange.lowerKey);

    auto lowest = set.lower_bound(m_remainingRange.lowerKey);
    if (lowest == set.end())
        return lowest;

    if (m_remainingRange.lowerOpen && *lowest == m_remainingRange.lowerKey) {
        ++lowest;
        if (lowest == set.end())
            return lowest;
    }

    if (!m_remainingRange.upperKey.isNull()) {
        if (lowest->compare(m_remainingRange.upperKey) > 0)
            return set.end();

        if (m_remainingRange.upperOpen && *lowest == m_remainingRange.upperKey)
            return set.end();
    }

    return lowest;
}

std::set<IDBKeyData>::reverse_iterator MemoryObjectStoreCursor::firstReverseIteratorInRemainingRange(std::set<IDBKeyData>& set)
{
    if (m_remainingRange.isExactlyOneKey()) {
        auto iterator = set.find(m_remainingRange.lowerKey);
        if (iterator == set.end())
            return set.rend();

        return --std::set<IDBKeyData>::reverse_iterator(iterator);
    }

    if (!m_remainingRange.upperKey.isValid())
        return set.rbegin();

    // This is one record past the actual key we're looking for.
    auto highest = set.upper_bound(m_remainingRange.upperKey);

    // This is one record before that, which *is* the key we're looking for.
    auto reverse = std::set<IDBKeyData>::reverse_iterator(highest);
    if (reverse == set.rend())
        return reverse;

    if (m_remainingRange.upperOpen && *reverse == m_remainingRange.upperKey) {
        ++reverse;
        if (reverse == set.rend())
            return reverse;
    }

    if (!m_remainingRange.lowerKey.isNull()) {
        if (reverse->compare(m_remainingRange.lowerKey) < 0)
            return set.rend();

        if (m_remainingRange.lowerOpen && *reverse == m_remainingRange.lowerKey)
            return set.rend();
    }

    return reverse;
}

void MemoryObjectStoreCursor::currentData(IDBGetResult& data)
{
    if (!hasIterators()) {
        m_currentPositionKey = { };
        data = { };
        return;
    }

    auto* set = m_objectStore.orderedKeys();
    ASSERT(set);
    if (m_info.isDirectionForward()) {
        if (!m_forwardIterator || *m_forwardIterator == set->end()) {
            data = { };
            return;
        }

        m_currentPositionKey = **m_forwardIterator;
        data.keyData = **m_forwardIterator;
        data.primaryKeyData = **m_forwardIterator;
        data.valueBuffer = m_objectStore.valueForKeyRange(**m_forwardIterator);
    } else {
        if (!m_reverseIterator || *m_reverseIterator == set->rend()) {
            data = { };
            return;
        }

        m_currentPositionKey = **m_reverseIterator;
        data.keyData = **m_reverseIterator;
        data.primaryKeyData = **m_reverseIterator;
        data.valueBuffer = m_objectStore.valueForKeyRange(**m_reverseIterator);
    }
}

void MemoryObjectStoreCursor::incrementForwardIterator(std::set<IDBKeyData>& set, const IDBKeyData& key, uint32_t count)
{
    // We might need to re-grab the current iterator.
    // e.g. If the record it was pointed to had been deleted.
    bool didResetIterator = false;
    if (!hasIterators()) {
        if (!m_currentPositionKey.isValid())
            return;

        m_remainingRange.lowerKey = m_currentPositionKey;
        m_remainingRange.lowerOpen = false;
        setFirstInRemainingRange(set);

        didResetIterator = true;
    }

    if (*m_forwardIterator == set.end())
        return;

    if (key.isValid()) {
        // If iterating to a key, the count passed in must be zero, as there is no way to iterate by both a count and to a key.
        ASSERT(!count);

        if (!m_info.range().containsKey(key))
            return;

        if ((*m_forwardIterator)->compare(key) < 0) {
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
    if (didResetIterator && (*m_forwardIterator)->compare(m_currentPositionKey) > 0)
        --count;

    while (count) {
        --count;

        ++*m_forwardIterator;

        if (*m_forwardIterator == set.end())
            return;

        if (!m_info.range().containsKey(**m_forwardIterator))
            return;
    }
}

void MemoryObjectStoreCursor::incrementReverseIterator(std::set<IDBKeyData>& set, const IDBKeyData& key, uint32_t count)
{
    // We might need to re-grab the current iterator.
    // e.g. If the record it was pointed to had been deleted.
    bool didResetIterator = false;
    if (!hasIterators()) {
        if (!m_currentPositionKey.isValid())
            return;

        m_remainingRange.upperKey = m_currentPositionKey;
        m_remainingRange.upperOpen = false;
        setFirstInRemainingRange(set);

        didResetIterator = true;
    }

    if (*m_reverseIterator == set.rend())
        return;

    if (key.isValid()) {
        // If iterating to a key, the count passed in must be zero, as there is no way to iterate by both a count and to a key.
        ASSERT(!count);

        if (!m_info.range().containsKey(key))
            return;

        if ((*m_reverseIterator)->compare(key) > 0) {
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
    if (didResetIterator && (*m_reverseIterator)->compare(m_currentPositionKey) < 0)
        --count;

    while (count) {
        --count;

        ++*m_reverseIterator;

        if (*m_reverseIterator == set.rend())
            return;

        if (!m_info.range().containsKey(**m_reverseIterator))
            return;
    }
}

bool MemoryObjectStoreCursor::hasIterators() const
{
    return m_forwardIterator || m_reverseIterator;
}

bool MemoryObjectStoreCursor::hasValidPosition() const
{
    if (!hasIterators())
        return false;

    auto* set = m_objectStore.orderedKeys();
    if (!set)
        return false;

    if (m_info.isDirectionForward())
        return *m_forwardIterator != set->end();

    return *m_reverseIterator != set->rend();
}

void MemoryObjectStoreCursor::clearIterators()
{
    m_forwardIterator = Nullopt;
    m_reverseIterator = Nullopt;
}

void MemoryObjectStoreCursor::iterate(const IDBKeyData& key, uint32_t count, IDBGetResult& outData)
{
    LOG(IndexedDB, "MemoryObjectStoreCursor::iterate to key %s", key.loggingString().utf8().data());

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

    if (!hasValidPosition()) {
        outData = { };
        return;
    }

    currentData(outData);
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
