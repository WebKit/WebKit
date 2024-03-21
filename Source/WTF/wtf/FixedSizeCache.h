/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2023 The Chromium Authors
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// A cache of fixed size, which will automatically evict members
// when there is no room for them. This is a simple 2-way associative
// cache; i.e., every element can go into one out of two neighboring
// slots. An inserted element is always overwriting whatever is in
// slot 1 (unless slot 0 is empty); on a successful lookup,
// it is moved to slot 0. This gives preference to the elements that are
// actually used, and the scheme is simple enough that it's faster than
// using a standard HashMap.
//
// There are no heap allocations after the initial setup. Deletions
// and overwrites (inserting the same key more than once) are not
// supported. Uses the given hash traits, so you should never try to
// insert or search for EmptyValue(). It can hold Oilpan members.

#pragma once

#include <array>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

template<typename Key, typename Value, unsigned cacheSize, typename Hash, typename KeyTraitsArg, typename MappedTraitsArg>
class FixedSizeCache final {
    WTF_MAKE_FAST_ALLOCATED;
    static_assert(!(cacheSize & (cacheSize - 1)), "cacheSize should be a power of two");
    static_assert(cacheSize >= 2);
public:
    using KeyTraits = KeyTraitsArg;
    using MappedTraits = MappedTraitsArg;
    using KeyType = Key;
    using MappedType = Value;
    using HashFunctions = Hash;
    using MappedPeekType = typename MappedTraits::PeekType;

    FixedSizeCache()
    {
        clear();
    }

    void clear()
    {
        m_cache.fill(std::pair { KeyTraits::emptyValue(), MappedTraits::emptyValue() });
    }

    MappedPeekType get(const Key& key) { return find(key, Hash::hash(key)); }

    MappedPeekType get(const Key& key, unsigned hash)
    {
        ASSERT(!isHashTraitsEmptyValue<KeyTraits>(key));
        ASSERT(Hash::hash(key) == hash);
        unsigned bucketSet = (hash % cacheSize) & ~1;
        uint8_t prefilter = prefilterHash(hash);

        // Search, moving to front if we find a match.
        if (m_prefilter[bucketSet] == prefilter && m_cache[bucketSet].first == key)
            return MappedTraits::peek(m_cache[bucketSet].second);

        if (m_prefilter[bucketSet + 1] == prefilter && m_cache[bucketSet + 1].first == key) {
            using std::swap;
            swap(m_prefilter[bucketSet], m_prefilter[bucketSet + 1]);
            swap(m_cache[bucketSet], m_cache[bucketSet + 1]);
            return MappedTraits::peek(m_cache[bucketSet].second);
        }

        return MappedTraits::peek(MappedTraits::emptyValue());
    }

    Value& insert(const Key& key, Value&& value)
    {
        unsigned hash = Hash::hash(key);
        return insert(key, std::forward<Value>(value), hash);
    }

    // Returns a reference to the newly inserted value.
    Value& insert(const Key& key, Value&& value, unsigned hash)
    {
        ASSERT(!isHashTraitsEmptyValue<KeyTraits>(key));
        ASSERT(Hash::hash(key) == hash);
        unsigned slot = (hash % cacheSize) & ~1;

        // Overwrites are not supported (if so, use find()
        // and modify the resulting value).
        ASSERT(m_cache[slot].first != key);
        ASSERT(m_cache[slot + 1].first != key);

        if (m_prefilter[slot])
            ++slot; // Not empty.

        m_prefilter[slot] = prefilterHash(hash);
        m_cache[slot] = std::pair { key, std::forward<Value>(value) };
        return m_cache[slot].second;
    }

private:
    static constexpr uint8_t prefilterHash(unsigned hash)
    {
        // Use the bits we didn't use for the bucket set.
        return ((hash / cacheSize) & 0xff) | 1;
    }

    // Contains some extra bits of the hash (those not used for bucketing),
    // as an extra filter before operator==, which may be expensive.
    // This is especially useful in the case where we keep missing the cache,
    // and don't want to burn the CPU's L1 cache on repeated useless lookups
    // into m_cache, especially if Key or Value are large. (This is why it's
    // kept as a separate array.)
    //
    // The lower bit is always set to 1 for a non-empty value.
    std::array<uint8_t, cacheSize> m_prefilter { };
    std::array<std::pair<Key, Value>, cacheSize> m_cache;
};

} // namespace WTF

using WTF::FixedSizeCache;
