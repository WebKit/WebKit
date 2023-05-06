/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#include <variant>
#include <wtf/Forward.h>
#include <wtf/HashTraits.h>

namespace WTF {

template<typename Key, typename HashArg = DefaultHash<Key>>
class GenericHashKey final {
    WTF_MAKE_FAST_ALLOCATED;

    struct EmptyKey { };
    struct DeletedKey { };

public:
    constexpr GenericHashKey(Key&& key)
        : m_value(std::in_place_type_t<Key>(), WTFMove(key))
    {
    }

    template<typename K>
    constexpr GenericHashKey(K&& key)
        : m_value(std::in_place_type_t<Key>(), std::forward<K>(key))
    {
    }

    constexpr GenericHashKey(HashTableEmptyValueType)
        : m_value(EmptyKey { })
    {
    }

    constexpr GenericHashKey(HashTableDeletedValueType)
        : m_value(DeletedKey { })
    {
    }

    constexpr const Key& key() const { return std::get<Key>(m_value); }
    constexpr unsigned hash() const
    {
        ASSERT_UNDER_CONSTEXPR_CONTEXT(!isHashTableDeletedValue() && !isHashTableEmptyValue());
        return HashArg::hash(key());
    }

    constexpr bool isHashTableDeletedValue() const { return std::holds_alternative<DeletedKey>(m_value); }
    constexpr bool isHashTableEmptyValue() const { return std::holds_alternative<EmptyKey>(m_value); }

    constexpr bool operator==(const GenericHashKey& other) const
    {
        if (m_value.index() != other.m_value.index())
            return false;
        if (!std::holds_alternative<Key>(m_value))
            return true;
        return HashArg::equal(key(), other.key());
    }

private:
    std::variant<Key, EmptyKey, DeletedKey> m_value;
};

template<typename K, typename H> struct HashTraits<GenericHashKey<K, H>> : GenericHashTraits<GenericHashKey<K, H>> {
    static GenericHashKey<K, H> emptyValue() { return GenericHashKey<K, H> { HashTableEmptyValue }; }
    static void constructDeletedValue(GenericHashKey<K, H>& slot) { slot = GenericHashKey<K, H> { HashTableDeletedValue }; }
    static bool isDeletedValue(const GenericHashKey<K, H>& value) { return value.isHashTableDeletedValue(); }
};

template<typename K, typename H> struct DefaultHash<GenericHashKey<K, H>> {
    static unsigned hash(const GenericHashKey<K, H>& key) { return key.hash(); }
    static bool equal(const GenericHashKey<K, H>& a, const GenericHashKey<K, H>& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

}

using WTF::GenericHashKey;
