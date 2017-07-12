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
#include "IndexValueEntry.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorInfo.h"

namespace WebCore {
namespace IDBServer {

IndexValueEntry::IndexValueEntry(bool unique)
    : m_unique(unique)
{
    if (m_unique)
        m_key = nullptr;
    else
        m_orderedKeys = new IDBKeyDataSet;
}

IndexValueEntry::~IndexValueEntry()
{
    if (m_unique)
        delete m_key;
    else
        delete m_orderedKeys;
}

void IndexValueEntry::addKey(const IDBKeyData& key)
{
    if (m_unique) {
        delete m_key;
        m_key = new IDBKeyData(key);
        return;
    }

    m_orderedKeys->insert(key);
}

bool IndexValueEntry::removeKey(const IDBKeyData& key)
{
    if (m_unique) {
        if (m_key && *m_key == key) {
            delete m_key;
            m_key = nullptr;
            return true;
        }

        return false;
    }

    return m_orderedKeys->erase(key);
}

const IDBKeyData* IndexValueEntry::getLowest() const
{
    if (m_unique)
        return m_key;

    if (m_orderedKeys->empty())
        return nullptr;

    return &(*m_orderedKeys->begin());
}

uint64_t IndexValueEntry::getCount() const
{
    if (m_unique)
        return m_key ? 1 : 0;

    return m_orderedKeys->size();
}

IndexValueEntry::Iterator::Iterator(IndexValueEntry& entry)
    : m_entry(&entry)
{
    ASSERT(m_entry->m_key);
}

IndexValueEntry::Iterator::Iterator(IndexValueEntry& entry, IDBKeyDataSet::iterator iterator)
    : m_entry(&entry)
    , m_forwardIterator(iterator)
{
}

IndexValueEntry::Iterator::Iterator(IndexValueEntry& entry, IDBKeyDataSet::reverse_iterator iterator)
    : m_entry(&entry)
    , m_forward(false)
    , m_reverseIterator(iterator)
{
}

const IDBKeyData& IndexValueEntry::Iterator::key() const
{
    ASSERT(isValid());
    if (m_entry->unique()) {
        ASSERT(m_entry->m_key);
        return *m_entry->m_key;
    }

    return m_forward ? *m_forwardIterator : *m_reverseIterator;
}

bool IndexValueEntry::Iterator::isValid() const
{
#if !LOG_DISABLED
    if (m_entry) {
        if (m_entry->m_unique)
            ASSERT(m_entry->m_key);
        else
            ASSERT(m_entry->m_orderedKeys);
    }
#endif

    return m_entry;
}

void IndexValueEntry::Iterator::invalidate()
{
    m_entry = nullptr;
}

IndexValueEntry::Iterator& IndexValueEntry::Iterator::operator++()
{
    if (!isValid())
        return *this;

    if (m_entry->m_unique) {
        invalidate();
        return *this;
    }

    if (m_forward) {
        ++m_forwardIterator;
        if (m_forwardIterator == m_entry->m_orderedKeys->end())
            invalidate();
    } else {
        ++m_reverseIterator;
        if (m_reverseIterator == m_entry->m_orderedKeys->rend())
            invalidate();
    }

    return *this;
}

IndexValueEntry::Iterator IndexValueEntry::begin()
{
    if (m_unique) {
        ASSERT(m_key);
        return { *this };
    }

    ASSERT(m_orderedKeys);
    return { *this, m_orderedKeys->begin() };
}

IndexValueEntry::Iterator IndexValueEntry::reverseBegin(CursorDuplicity duplicity)
{
    if (m_unique) {
        ASSERT(m_key);
        return { *this };
    }

    ASSERT(m_orderedKeys);

    if (duplicity == CursorDuplicity::Duplicates)
        return { *this, m_orderedKeys->rbegin() };

    auto iterator = m_orderedKeys->rend();
    --iterator;
    return { *this, iterator };
}

IndexValueEntry::Iterator IndexValueEntry::find(const IDBKeyData& key)
{
    if (m_unique) {
        ASSERT(m_key);
        return *m_key == key ? IndexValueEntry::Iterator(*this) : IndexValueEntry::Iterator();
    }

    ASSERT(m_orderedKeys);
    auto iterator = m_orderedKeys->lower_bound(key);
    if (iterator == m_orderedKeys->end())
        return { };

    return { *this, iterator };
}

IndexValueEntry::Iterator IndexValueEntry::reverseFind(const IDBKeyData& key, CursorDuplicity)
{
    if (m_unique) {
        ASSERT(m_key);
        return *m_key == key ? IndexValueEntry::Iterator(*this) : IndexValueEntry::Iterator();
    }

    ASSERT(m_orderedKeys);
    auto iterator = IDBKeyDataSet::reverse_iterator(m_orderedKeys->upper_bound(key));
    if (iterator == m_orderedKeys->rend())
        return { };

    return { *this, iterator };
}


} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
