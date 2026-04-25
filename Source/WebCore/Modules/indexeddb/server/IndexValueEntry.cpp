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

#include "IDBCursorInfo.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace IDBServer {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IndexValueEntry);

static Variant<std::optional<IDBKeyData>, IDBKeyDataSet> constructEmptyKeys(bool isUnique)
{
    if (isUnique)
        return std::nullopt;

    return IDBKeyDataSet { };
}

IndexValueEntry::IndexValueEntry(bool isUnique)
    : m_keys(constructEmptyKeys(isUnique))
{
}

void IndexValueEntry::addKey(const IDBKeyData& key)
{
    WTF::switchOn(m_keys, [&key](std::optional<IDBKeyData>& singleKey) {
        singleKey = key;
    }, [&key](IDBKeyDataSet& orderedKeys) {
        orderedKeys.insert(key);
    });
}

bool IndexValueEntry::removeKey(const IDBKeyData& key)
{
    return WTF::switchOn(m_keys, [&key](std::optional<IDBKeyData>& singleKey) {
        if (singleKey == key) {
            singleKey = std::nullopt;
            return true;
        }
        return false;
    }, [&key](IDBKeyDataSet& orderedKeys) {
        return !!orderedKeys.erase(key);
    });
}

bool IndexValueEntry::contains(const IDBKeyData& key)
{
    return WTF::switchOn(m_keys, [&key](const std::optional<IDBKeyData>& singleKey) {
        return singleKey && *singleKey == key;
    }, [&key](const IDBKeyDataSet& orderedKeys) {
        return orderedKeys.contains(key);
    });
}

const IDBKeyData* IndexValueEntry::getLowest() const LIFETIME_BOUND
{
    return WTF::switchOn(m_keys, [](const std::optional<IDBKeyData>& singleKey) -> const IDBKeyData* {
        return singleKey ? &*singleKey : nullptr;
    }, [](const IDBKeyDataSet& orderedKeys) -> const IDBKeyData* {
        return orderedKeys.empty() ? nullptr : &(*orderedKeys.begin());
    });
}

uint64_t IndexValueEntry::getCount() const
{
    return WTF::switchOn(m_keys, [](const std::optional<IDBKeyData>& singleKey) -> uint64_t {
        return singleKey ? 1 : 0;
    }, [](const IDBKeyDataSet& orderedKeys) -> uint64_t {
        return orderedKeys.size();
    });
}

IndexValueEntry::Iterator::Iterator(IndexValueEntry& entry)
    : m_entry(&entry)
{
    ASSERT(entry.isUnique());
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
    return WTF::switchOn(m_entry->m_keys, [](const std::optional<IDBKeyData>& singleKey) -> const IDBKeyData& {
        ASSERT(singleKey);
        return *singleKey;
    }, [this](const IDBKeyDataSet&) -> const IDBKeyData& {
        return m_forward ? *m_forwardIterator : *m_reverseIterator;
    });
}

bool IndexValueEntry::Iterator::isValid() const
{
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

    WTF::switchOn(m_entry->m_keys, [this](const std::optional<IDBKeyData>&) {
        invalidate();
    }, [this](const IDBKeyDataSet& orderedKeys) {
        if (m_forward) {
            ++m_forwardIterator;
            if (m_forwardIterator == orderedKeys.end())
                invalidate();
        } else {
            ++m_reverseIterator;
            if (m_reverseIterator == orderedKeys.rend())
                invalidate();
        }
    });

    return *this;
}

IndexValueEntry::Iterator IndexValueEntry::begin() LIFETIME_BOUND
{
    return WTF::switchOn(m_keys, [this](const std::optional<IDBKeyData>& singleKey) -> Iterator {
        ASSERT_UNUSED(singleKey, singleKey);
        return { *this };
    }, [this](IDBKeyDataSet& orderedKeys) -> Iterator {
        ASSERT(!orderedKeys.empty());
        return { *this, orderedKeys.begin() };
    });
}

IndexValueEntry::Iterator IndexValueEntry::reverseBegin(CursorDuplicity duplicity) LIFETIME_BOUND
{
    return WTF::switchOn(m_keys, [this](const std::optional<IDBKeyData>& singleKey) -> Iterator {
        ASSERT_UNUSED(singleKey, singleKey);
        return { *this };
    }, [this, duplicity](IDBKeyDataSet& orderedKeys) -> Iterator {
        ASSERT(!orderedKeys.empty());
        if (duplicity == CursorDuplicity::Duplicates)
            return { *this, orderedKeys.rbegin() };
        auto iterator = orderedKeys.rend();
        --iterator;
        return { *this, iterator };
    });
}

IndexValueEntry::Iterator IndexValueEntry::find(const IDBKeyData& key) LIFETIME_BOUND
{
    return WTF::switchOn(m_keys, [this, &key](const std::optional<IDBKeyData>& singleKey) -> Iterator {
        ASSERT(singleKey);
        return *singleKey == key ? Iterator(*this) : Iterator();
    }, [this, &key](IDBKeyDataSet& orderedKeys) -> Iterator {
        ASSERT(!orderedKeys.empty());
        auto iterator = orderedKeys.lower_bound(key);
        if (iterator == orderedKeys.end())
            return { };
        return { *this, iterator };
    });
}

IndexValueEntry::Iterator IndexValueEntry::reverseFind(const IDBKeyData& key, CursorDuplicity) LIFETIME_BOUND
{
    return WTF::switchOn(m_keys, [this, &key](const std::optional<IDBKeyData>& singleKey) -> Iterator {
        ASSERT(singleKey);
        return *singleKey == key ? Iterator(*this) : Iterator();
    }, [this, &key](IDBKeyDataSet& orderedKeys) -> Iterator {
        ASSERT(!orderedKeys.empty());
        auto iterator = IDBKeyDataSet::reverse_iterator(orderedKeys.upper_bound(key));
        if (iterator == orderedKeys.rend())
            return { };
        return { *this, iterator };
    });
}

Vector<IDBKeyData> IndexValueEntry::keys() const
{
    Vector<IDBKeyData> result;
    WTF::switchOn(m_keys, [&result](const std::optional<IDBKeyData>& singleKey) {
        if (singleKey)
            result.append(*singleKey);
    }, [&result](const IDBKeyDataSet& orderedKeys) {
        for (auto& key : orderedKeys)
            result.append(key);
    });
    return result;
}

} // namespace IDBServer
} // namespace WebCore
