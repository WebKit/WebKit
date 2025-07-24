/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPrimitiveKeywordList.h"
#include <wtf/MathExtras.h>

namespace WebCore {
namespace CSS {

// `OptionSet`-like type for sets of CSS keywords.
template<typename Derived, PrimitiveKeyword EmptyCase, PrimitiveKeyword... Ks> struct PrimitiveKeywordSet {
    using Base = PrimitiveKeywordSet<Derived, EmptyCase, Ks...>;
    using Keywords = PrimitiveKeywordList<Ks...>;

    static constexpr auto bits = Keywords::count;
    static_assert(bits <= 32);

    using Storage = std::conditional_t<bits <= 8, uint8_t, std::conditional_t<bits <= 16, uint16_t, uint32_t>>;

    static constexpr auto emptyCase = EmptyCase { };

    struct Value {
        constexpr Value(ValidKeywordForList<Keywords> auto keyword)
            : value(static_cast<uint8_t>(Keywords::offsetForKeyword(keyword)))
        {
        }

        PrimitiveKeywordSet toSet() const { return PrimitiveKeywordSet::fromRaw(1 << value); }

        bool operator==(const Value&) const = default;

    private:
        uint8_t value;
    };

    static constexpr Derived all() { return fromRaw(-1); }

    static constexpr Derived fromRaw(Storage raw) { return Derived(raw); }

    constexpr PrimitiveKeywordSet() = default;

    constexpr PrimitiveKeywordSet(EmptyCase)
        : m_storage { 0 }
    {
    }

    constexpr PrimitiveKeywordSet(Storage storage)
        : m_storage { storage }
    {
    }

    constexpr PrimitiveKeywordSet(ValidKeywordForList<Keywords> auto keyword)
        : m_storage(toBit(keyword))
    {
    }

    constexpr PrimitiveKeywordSet(Value value)
        : PrimitiveKeywordSet { value.toSet() }
    {
    }

    constexpr PrimitiveKeywordSet(std::initializer_list<PrimitiveKeywordSet> initializerList)
    {
        for (auto& keywordSet : initializerList) {
            ASSERT_UNDER_CONSTEXPR_CONTEXT(hasOneBitSet(keywordSet.m_storage));
            m_storage |= keywordSet.m_storage;
        }
    }

    constexpr Storage toRaw() const { return m_storage; }

    constexpr bool isEmpty() const { return !m_storage; }
    constexpr explicit operator bool() const { return !isEmpty(); }

    // MARK: - Operators

    constexpr friend Derived operator|(PrimitiveKeywordSet lhs, PrimitiveKeywordSet rhs)
    {
        return fromRaw(lhs.m_storage | rhs.m_storage);
    }

    constexpr PrimitiveKeywordSet& operator|=(const PrimitiveKeywordSet& other)
    {
        add(other);
        return *this;
    }

    constexpr friend Derived operator&(PrimitiveKeywordSet lhs, PrimitiveKeywordSet rhs)
    {
        return fromRaw(lhs.m_storage & rhs.m_storage);
    }

    constexpr friend Derived operator-(PrimitiveKeywordSet lhs, PrimitiveKeywordSet rhs)
    {
        return fromRaw(lhs.m_storage & ~rhs.m_storage);
    }

    constexpr friend Derived operator^(PrimitiveKeywordSet lhs, PrimitiveKeywordSet rhs)
    {
        return fromRaw(lhs.m_storage ^ rhs.m_storage);
    }

    constexpr bool contains(ValidKeywordForList<Keywords> auto keyword) const
    {
        return containsAny(keyword);
    }

    constexpr bool contains(Value keyword) const
    {
        return containsAny(keyword.toSet());
    }

    constexpr bool containsAny(PrimitiveKeywordSet set) const
    {
        return !!(*this & set);
    }

    constexpr bool containsAll(PrimitiveKeywordSet set) const
    {
        return (*this & set) == set;
    }

    constexpr bool containsOnly(PrimitiveKeywordSet set) const
    {
        return *this == (*this & set);
    }

    constexpr void add(PrimitiveKeywordSet set)
    {
        m_storage |= set.m_storage;
    }

    constexpr void remove(PrimitiveKeywordSet set)
    {
        m_storage &= ~set.m_storage;
    }

    constexpr void set(PrimitiveKeywordSet set, bool value)
    {
        if (value)
            add(set);
        else
            remove(set);
    }

    constexpr bool hasExactlyOneBitSet() const
    {
        return m_storage && !(m_storage & (m_storage - 1));
    }

    constexpr bool operator==(const PrimitiveKeywordSet&) const = default;

private:
    static constexpr Storage toBit(ValidKeywordForList<Keywords> auto keyword)
    {
        return static_cast<Storage>(1 << Keywords::offsetForKeyword(keyword));
    }

    Storage m_storage;
};

// MARK: - Concepts

template<typename T> concept PrimitiveKeywordSetDerived = WTF::IsBaseOfTemplate<PrimitiveKeywordSet, T>::value  && VariantLike<T>;

} // namespace CSS
} // namespace WebCore
