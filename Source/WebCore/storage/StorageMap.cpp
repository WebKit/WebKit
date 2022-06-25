/*
 * Copyright (C) 2008-2021 Apple Inc. All Rights Reserved.
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
#include "StorageMap.h"

#include <wtf/CheckedArithmetic.h>
#include <wtf/SetForScope.h>

namespace WebCore {

StorageMap::StorageMap(unsigned quota)
    : m_impl(Impl::create())
    , m_quotaSize(quota) // quota measured in bytes
{
}

void StorageMap::invalidateIterator()
{
    m_impl->iterator = m_impl->map.end();
    m_impl->iteratorIndex = UINT_MAX;
}

void StorageMap::setIteratorToIndex(unsigned index)
{
    // FIXME: Once we have bidirectional iterators for HashMap we can be more intelligent about this.
    // The requested index will be closest to begin(), our current iterator, or end(), and we
    // can take the shortest route.
    // Until that mechanism is available, we'll always increment our iterator from begin() or current.

    if (m_impl->iteratorIndex == index)
        return;

    if (index < m_impl->iteratorIndex) {
        m_impl->iteratorIndex = 0;
        m_impl->iterator = m_impl->map.begin();
        ASSERT(m_impl->iterator != m_impl->map.end());
    }

    while (m_impl->iteratorIndex < index) {
        ++m_impl->iteratorIndex;
        ++m_impl->iterator;
        ASSERT(m_impl->iterator != m_impl->map.end());
    }
}

unsigned StorageMap::length() const
{
    return m_impl->map.size();
}

String StorageMap::key(unsigned index)
{
    if (index >= length())
        return String();

    setIteratorToIndex(index);
    return m_impl->iterator->key;
}

String StorageMap::getItem(const String& key) const
{
    return m_impl->map.get(key);
}

void StorageMap::setItem(const String& key, const String& value, String& oldValue, bool& quotaException)
{
    ASSERT(!value.isNull());

    quotaException = false;
    CheckedUint32 newSize = m_impl->currentSize;
    auto iter = m_impl->map.find(key);
    if (iter != m_impl->map.end()) {
        oldValue = iter->value;
        newSize -= oldValue.sizeInBytes();
    } else {
        oldValue = nullString();
        newSize += key.sizeInBytes();
    }
    newSize += value.sizeInBytes();
    if (m_quotaSize != noQuota && (newSize.hasOverflowed() || newSize > m_quotaSize)) {
        quotaException = true;
        return;
    }

    // Implement copy-on-write semantics.
    if (m_impl->refCount() > 1)
        m_impl = m_impl->copy();

    m_impl->map.set(key, value);
    m_impl->currentSize = newSize;
    invalidateIterator();
}

void StorageMap::setItemIgnoringQuota(const String& key, const String& value)
{
    SetForScope quotaSizeChange(m_quotaSize, noQuota);

    String oldValue;
    bool quotaException;
    setItem(key, value, oldValue, quotaException);
    ASSERT(!quotaException);
}

void StorageMap::removeItem(const String& key, String& oldValue)
{
    oldValue = nullString();
    CheckedUint32 newSize = m_impl->currentSize;
    auto iter = m_impl->map.find(key);
    if (iter == m_impl->map.end())
        return;
    oldValue = iter->value;
    newSize = newSize - iter->key.sizeInBytes() - oldValue.sizeInBytes();

    if (m_impl->hasOneRef())
        m_impl->map.remove(iter);
    else {
        // Implement copy-on-write semantics.
        m_impl = m_impl->copy();
        m_impl->map.remove(key);
    }

    m_impl->currentSize = newSize;
    invalidateIterator();
}

void StorageMap::clear()
{
    if (m_impl->refCount() > 1 && length()) {
        m_impl = Impl::create();
        return;
    }
    m_impl->map.clear();
    m_impl->currentSize = 0;
    invalidateIterator();
}

bool StorageMap::contains(const String& key) const
{
    return m_impl->map.contains(key);
}

void StorageMap::importItems(HashMap<String, String>&& items)
{
    RELEASE_ASSERT(m_impl->map.isEmpty());
    RELEASE_ASSERT(!m_impl->currentSize);

    CheckedUint32 newSize;
    for (auto& [key, value] : items) {
        newSize += key.sizeInBytes();
        newSize += value.sizeInBytes();
    }

    m_impl->map = WTFMove(items);
    m_impl->currentSize = newSize;
}

Ref<StorageMap::Impl> StorageMap::Impl::copy() const
{
    auto copy = Impl::create();
    copy->map = map;
    copy->currentSize = currentSize;
    return copy;
}

}
