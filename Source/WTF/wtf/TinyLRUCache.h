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
#include <wtf/Assertions.h>
#include <wtf/NeverDestroyed.h>

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
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(TinyLRUCache);
public:
    TinyLRUCache() = default;

    ~TinyLRUCache()
    {
        invalidateIterators();
    }

    TinyLRUCache(const TinyLRUCache& other)
        : m_cacheBuffer(other.m_cacheBuffer)
        , m_size(other.m_size)
    {
        invalidateIterators();
        other.invalidateIterators();
    }

    TinyLRUCache& operator=(const TinyLRUCache& other)
    {
        if (this == &other)
            return *this;
        invalidateIterators();
        other.invalidateIterators();
        m_cacheBuffer = other.m_cacheBuffer;
        m_size = other.m_size;
        return *this;
    }

    TinyLRUCache(TinyLRUCache&&) = delete;
    TinyLRUCache& operator=(TinyLRUCache&&) = delete;

    class FindResult {
    public:
        FindResult() = default;

        FindResult(const FindResult&) = delete;

        FindResult& operator=(const FindResult& other) = delete;

        ~FindResult()
        {
            if (m_cache) {
                ASSERT(m_cache->m_findResult == this);
                m_cache->invalidateIterators();
            }
        }

        explicit operator bool() const
        {
            return m_ptr;
        }

        const ValueType& operator*() const
        {
            RELEASE_ASSERT(m_ptr);
            return *m_ptr;
        }

        const ValueType* operator->() const
        {
            RELEASE_ASSERT(m_ptr);
            return m_ptr;
        }

    private:
        friend class TinyLRUCache;

        using Cache = TinyLRUCache<KeyType, ValueType, capacity, Policy>;

        FindResult(const ValueType* ptr, Cache& cache)
            : m_ptr(ptr)
            , m_cache(ptr ? &cache : nullptr)
        {
            if (m_cache)
                m_cache->trackFindResult(this);
        }

        void assertValid() const
        {
            RELEASE_ASSERT_IMPLIES(m_ptr, m_cache);
        }

        const ValueType* m_ptr { nullptr };
        Cache* m_cache { nullptr };
    };

    const ValueType& get(const KeyType& key)
    {
        invalidateIterators();

        if (Policy::isKeyNull(key)) {
            static NeverDestroyed<ValueType> valueForNull = Policy::createValueForNullKey();
            return valueForNull;
        }

        if (auto* found = findInternal(key))
            return *found;

        insertInternal(key, Policy::createValueForKey(key));
        return cacheBuffer()[m_size - 1].second;
    }

    FindResult findIfCached(const KeyType& key)
    {
        invalidateIterators();

        if (Policy::isKeyNull(key))
            return { nullptr, *this };

        return { findInternal(key), *this };
    }

    void insert(const KeyType& key, ValueType&& value)
    {
        ASSERT(!Policy::isKeyNull(key));
#if ASSERT_ENABLED
        {
            auto cacheBuffer = this->cacheBuffer();
            for (size_t i = 0; i < m_size; ++i)
                ASSERT(!(cacheBuffer[i].first == key));
        }
#endif
        invalidateIterators();
        insertInternal(key, WTF::move(value));
    }

    void clear()
    {
        invalidateIterators();
        auto cacheBuffer = this->cacheBuffer();
        for (size_t i = 0; i < m_size; ++i)
            cacheBuffer[i] = Entry { };
        m_size = 0;
    }

    size_t size() const { return m_size; }

private:
    void trackFindResult(FindResult* p)
    {
        ASSERT(p);
        ASSERT(m_findResult != p);
        invalidateIterators();
        m_findResult = p;
    }

    void invalidateIterators()
    {
        if (m_findResult) {
            ASSERT(m_findResult->m_cache == this);
            m_findResult->m_cache = nullptr;
            m_findResult->m_ptr = nullptr;
            m_findResult = nullptr;
        }
    }

    ALWAYS_INLINE const ValueType* findInternal(const KeyType& key)
    {
        auto cacheBuffer = this->cacheBuffer();
        for (size_t i = m_size; i-- > 0;) {
            if (cacheBuffer[i].first == key) {
                if (i < m_size - 1) {
                    auto entry = WTF::move(cacheBuffer[i]);
                    do {
                        cacheBuffer[i] = WTF::move(cacheBuffer[i + 1]);
                    } while (++i < m_size - 1);
                    cacheBuffer[m_size - 1] = WTF::move(entry);
                }
                return &cacheBuffer[m_size - 1].second;
            }
        }
        return nullptr;
    }

    ALWAYS_INLINE void insertInternal(const KeyType& key, ValueType&& value)
    {
        auto cacheBuffer = this->cacheBuffer();
        if (m_size == capacity) {
            for (size_t i = 0; i < m_size - 1; ++i)
                cacheBuffer[i] = WTF::move(cacheBuffer[i + 1]);
        } else
            ++m_size;
        cacheBuffer[m_size - 1] = std::pair { Policy::createKeyForStorage(key), WTF::move(value) };
    }

    using Entry = std::pair<KeyType, ValueType>;
    std::span<Entry, capacity> cacheBuffer() { return m_cacheBuffer; }

    alignas(Entry) std::array<Entry, capacity> m_cacheBuffer;
    size_t m_size { 0 };
    FindResult* m_findResult { nullptr };
};

} // namespace WTF

using WTF::TinyLRUCache;
using WTF::TinyLRUCachePolicy;
