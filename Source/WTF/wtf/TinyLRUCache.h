/*
 * Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

namespace WTF {

template<typename KeyType, typename ValueType>
struct TinyLRUCachePolicy {
    static bool isKeyNull(const KeyType&) { return false; }
    static ValueType createValueForNullKey() { return { }; }
    static ValueType createValueForKey(const KeyType&) { return { }; }
    static KeyType createKeyForStorage(const KeyType& key) { return key; }
};

template<typename KeyType, typename ValueType, size_t capacity = 4, typename Policy = TinyLRUCachePolicy<KeyType, ValueType>>
class TinyLRUCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    const ValueType& get(const KeyType& key)
    {
        if (Policy::isKeyNull(key)) {
            static NeverDestroyed<ValueType> valueForNull = Policy::createValueForNullKey();
            return valueForNull;
        }

        auto index = m_cache.reverseFindIf([&key](auto& entry) { return entry.first == key; });
        if (index != notFound) {
            // Move entry to the end of the cache if necessary.
            if (index != m_cache.size() - 1) {
                auto entry = WTFMove(m_cache[index]);
                m_cache.remove(index);
                m_cache.uncheckedAppend(WTFMove(entry));
            }
            return m_cache[m_cache.size() - 1].second;
        }

        // m_cache[0] is the LRU entry, so remove it.
        if (m_cache.size() == capacity)
            m_cache.remove(0);

        m_cache.uncheckedAppend(std::pair { Policy::createKeyForStorage(key), Policy::createValueForKey(key) });
        return m_cache.last().second;
    }

private:
    using Entry = std::pair<KeyType, ValueType>;
    using Cache = Vector<Entry, capacity>;
    Cache m_cache;
};

} // namespace WTF

using WTF::TinyLRUCache;
using WTF::TinyLRUCachePolicy;
