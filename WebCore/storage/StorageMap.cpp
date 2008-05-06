/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

namespace WebCore {

PassRefPtr<StorageMap> StorageMap::create()
{
    return adoptRef(new StorageMap);
}
    
StorageMap::StorageMap()
    : m_iterator(m_map.end())
    , m_iteratorIndex(UINT_MAX)
{
}

PassRefPtr<StorageMap> StorageMap::copy()
{
    RefPtr<StorageMap> newMap = create();
    newMap->m_map = m_map;
    return newMap.release();
}

void StorageMap::invalidateIterator()
{
    m_iterator = m_map.end();
    m_iteratorIndex = UINT_MAX;
}

void StorageMap::setIteratorToIndex(unsigned index) const
{
    // FIXME: Once we have bidirectional iterators for HashMap we can be more intelligent about this.
    // The requested index will be closest to begin(), our current iterator, or end(), and we 
    // can take the shortest route.
    // Until that mechanism is available, we'll always increment our iterator from begin() or current.
    
    if (m_iteratorIndex == index)
        return;
    
    if (index < m_iteratorIndex) {
        m_iteratorIndex = 0;
        m_iterator = m_map.begin();
        ASSERT(m_iterator != m_map.end());
    }
    
    while (m_iteratorIndex < index) {
        ++m_iteratorIndex;
        ++m_iterator;
        ASSERT(m_iterator != m_map.end());
    }
}

unsigned StorageMap::length() const
{
    return m_map.size();
}

bool StorageMap::key(unsigned index, String& key) const
{
    if (index >= length())
        return false;
    
    setIteratorToIndex(index);
    
    key = m_iterator->first;
    return true;
}

String StorageMap::getItem(const String& key) const
{
    return m_map.get(key);
}

PassRefPtr<StorageMap> StorageMap::setItem(const String& key, const String& value, String& oldValue)
{
    ASSERT(!value.isNull());
    
    // Implement copy-on-write semantics here.  We're guaranteed that the only refs of StorageMaps belong to Storage objects
    // so if more than one Storage object refs this map, copy it before mutating it.
    if (refCount() > 1) {
        RefPtr<StorageMap> newStorageMap = copy();
        newStorageMap->setItem(key, value, oldValue);
        return newStorageMap.release();
    }

    pair<HashMap<String, String>::iterator, bool> addResult = m_map.add(key, value);

    if (addResult.second) {
        // There was no "oldValue" so null it out.
        oldValue = String();
    } else {
        oldValue = addResult.first->second;
        addResult.first->second = value;
    }

    invalidateIterator();

    return 0;
}

PassRefPtr<StorageMap> StorageMap::removeItem(const String& key, String& oldValue)
{
    // Implement copy-on-write semantics here.  We're guaranteed that the only refs of StorageMaps belong to Storage objects
    // so if more than one Storage object refs this map, copy it before mutating it.
    if (refCount() > 1) {
        RefPtr<StorageMap> newStorage = copy();
        newStorage->removeItem(key, oldValue);
        return newStorage.release();
    }

    oldValue = m_map.take(key);
    if (!oldValue.isNull())
        invalidateIterator();

    return 0;
}

bool StorageMap::contains(const String& key) const
{
    return m_map.contains(key);
}

void StorageMap::importItem(const String& key, const String& value) const
{
    // Be sure to copy the keys/values as items imported on a background thread are destined
    // to cross a thread boundary
    pair<HashMap<String, String>::iterator, bool> result = m_map.add(key.copy(), String());

    if (result.second)
        result.first->second = value.copy();
}

}
