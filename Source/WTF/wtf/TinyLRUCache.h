/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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

#include <algorithm>
#include <cstddef>
#include <span>
#include <wtf/NeverDestroyed.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

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

        auto cacheBuffer = this->cacheBuffer();
        for (size_t i = m_size; i-- > 0;) {
            if (cacheBuffer[i].first == key) {
                if (i < m_size - 1) {
                    // Move entry to the end of the cache if necessary.
                    auto entry = WTFMove(cacheBuffer[i]);
                    do {
                        cacheBuffer[i] = WTFMove(cacheBuffer[i + 1]);
                    } while (++i < m_size - 1);
                    cacheBuffer[m_size - 1] = WTFMove(entry);
                }
                return cacheBuffer[m_size - 1].second;
            }
        }

        // cacheBuffer[0] is the LRU entry, so remove it.
        if (m_size == capacity) {
            for (size_t i = 0; i < m_size - 1; ++i)
                cacheBuffer[i] = WTFMove(cacheBuffer[i + 1]);
        } else
            ++m_size;

        cacheBuffer[m_size - 1] = std::pair { Policy::createKeyForStorage(key), Policy::createValueForKey(key) };
        return cacheBuffer[m_size - 1].second;
    }

private:
    using Entry = std::pair<KeyType, ValueType>;
    std::span<Entry, capacity> cacheBuffer() { return m_cacheBuffer; }

    alignas(Entry) std::array<Entry, capacity> m_cacheBuffer;
    size_t m_size { 0 };
};

} // namespace WTF

using WTF::TinyLRUCache;
using WTF::TinyLRUCachePolicy;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
