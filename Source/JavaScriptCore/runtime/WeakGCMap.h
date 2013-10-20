/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef WeakGCMap_h
#define WeakGCMap_h

#include "Weak.h"
#include "WeakInlines.h"
#include <wtf/HashMap.h>

namespace JSC {

// A HashMap with Weak<JSCell> values, which automatically removes values once they're garbage collected.

template<typename KeyArg, typename ValueArg, typename HashArg = typename DefaultHash<KeyArg>::Hash,
    typename KeyTraitsArg = HashTraits<KeyArg>>
class WeakGCMap {
    typedef Weak<ValueArg> ValueType;
    typedef HashMap<KeyArg, ValueType, HashArg, KeyTraitsArg> HashMapType;

public:
    typedef typename HashMapType::KeyType KeyType;
    typedef typename HashMapType::AddResult AddResult;
    typedef typename HashMapType::iterator iterator;
    typedef typename HashMapType::const_iterator const_iterator;

    WeakGCMap()
        : m_gcThreshold(minGCThreshold)
    {
    }

    ValueArg* get(const KeyType& key) const
    {
        return m_map.get(key);
    }

    AddResult set(const KeyType& key, ValueType value)
    {
        gcMapIfNeeded();
        return m_map.set(key, std::move(value));
    }

    AddResult add(const KeyType& key, ValueType value)
    {
        gcMapIfNeeded();
        AddResult addResult = m_map.add(key, nullptr);
        if (!addResult.iterator->value) { // New value or found a zombie value.
            addResult.isNewEntry = true;
            addResult.iterator->value = std::move(value);
        }
        return addResult;
    }

    bool remove(const KeyType& key)
    {
        return m_map.remove(key);
    }

    void clear()
    {
        m_map.clear();
    }

    iterator find(const KeyType& key)
    {
        iterator it = m_map.find(key);
        iterator end = m_map.end();
        if (it != end && !it->value) // Found a zombie value.
            return end;
        return it;
    }

    const_iterator find(const KeyType& key) const
    {
        return const_cast<WeakGCMap*>(this)->find(key);
    }

    bool contains(const KeyType& key) const
    {
        return find(key) != m_map.end();
    }

private:
    static const int minGCThreshold = 3;

    void gcMap()
    {
        Vector<KeyType, 4> zombies;

        for (iterator it = m_map.begin(), end = m_map.end(); it != end; ++it) {
            if (!it->value)
                zombies.append(it->key);
        }

        for (size_t i = 0; i < zombies.size(); ++i)
            m_map.remove(zombies[i]);
    }

    void gcMapIfNeeded()
    {
        if (m_map.size() < m_gcThreshold)
            return;

        gcMap();
        m_gcThreshold = std::max(minGCThreshold, m_map.size() * 2 - 1);
    }

    HashMapType m_map;
    int m_gcThreshold;
};

template<typename KeyArg, typename RawMappedArg, typename HashArg, typename KeyTraitsArg>
const int WeakGCMap<KeyArg, RawMappedArg, HashArg, KeyTraitsArg>::minGCThreshold;

} // namespace JSC

#endif // WeakGCMap_h
