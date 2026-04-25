/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <bit>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <type_traits>
#include <wtf/Assertions.h>
#include <wtf/EnumTraits.h>
#include <wtf/Forward.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// EnumSet is a class that represents a set of enumerators in a space-efficient manner.
// Unlike with OptionSet the enumerators don't need be powers of two but the highest value must be less than 64.
// If the enum has a member called HighestEnumValue it will be used to compute the storage size. Otherwise 64 bits are used.
template<typename E> class EnumSet {
    static_assert(std::is_enum_v<E>, "E is not an enum type");

public:
    using value_type = E;

    static constexpr size_t storageSize()
    {
        if constexpr (requires { E::HighestEnumValue; })
            return std::bit_ceil((static_cast<size_t>(E::HighestEnumValue) >> 3) + 1);
        return 8;
    }
    using StorageType = SizedUnsignedTrait<storageSize()>::Type;

    template<typename StorageType> class Iterator {
    public:
        E operator*() const { return static_cast<E>(std::countr_zero(m_value)); }

        // Iterates from smallest to largest enum value by turning off the rightmost set bit.
        Iterator& operator++()
        {
            m_value &= m_value - 1;
            return *this;
        }

        Iterator& operator++(int) = delete;

        bool operator==(const Iterator&) const = default;

    private:
        Iterator(StorageType value)
            : m_value(value)
        { }
        friend EnumSet;

        StorageType m_value;
    };

    using iterator = Iterator<StorageType>;

    static constexpr EnumSet fromRaw(StorageType rawValue)
    {
        return EnumSet(rawValue, FromRawValue);
    }

    constexpr EnumSet() = default;

    constexpr EnumSet(E e)
    {
        set(e);
    }

    constexpr EnumSet(std::initializer_list<E> initializerList)
    {
        for (auto& e : initializerList)
            set(e);
    }

    constexpr EnumSet(std::optional<E> optional)
    {
        if (optional)
            set(*optional);
    }

    constexpr StorageType toRaw() const { return m_storage; }

    constexpr bool isEmpty() const { return !m_storage; }
    constexpr size_t size() const { return std::popcount(m_storage); }

    constexpr iterator begin() const { return m_storage; }
    constexpr iterator end() const { return 0; }

    constexpr explicit operator bool() const { return !isEmpty(); }

    constexpr bool contains(E e) const
    {
        return get(e);
    }

    constexpr bool containsAny(EnumSet other) const
    {
        return !!(*this & other);
    }

    constexpr bool containsAll(EnumSet other) const
    {
        return (*this & other) == other;
    }

    constexpr bool containsOnly(EnumSet other) const
    {
        return *this == (*this & other);
    }

    constexpr void add(EnumSet other)
    {
        m_storage |= other.m_storage;
    }

    constexpr void remove(EnumSet optionSet)
    {
        m_storage &= ~optionSet.m_storage;
    }

    constexpr void set(EnumSet optionSet, bool value)
    {
        if (value)
            add(optionSet);
        else
            remove(optionSet);
    }

    constexpr bool hasExactlyOneBitSet() const
    {
        auto storage = m_storage;
        return storage && !(storage & (storage - 1));
    }

    constexpr std::optional<E> toSingleValue() const
    {
        return hasExactlyOneBitSet() ? std::optional<E>(static_cast<E>(std::countr_zero(m_storage))) : std::nullopt;
    }

    friend constexpr bool operator==(const EnumSet&, const EnumSet&) = default;

    constexpr friend EnumSet operator|(EnumSet lhs, EnumSet rhs)
    {
        return fromRaw(lhs.m_storage | rhs.m_storage);
    }

    constexpr EnumSet& operator|=(const EnumSet& other)
    {
        add(other);
        return *this;
    }

    constexpr friend EnumSet operator&(EnumSet lhs, EnumSet rhs)
    {
        return fromRaw(lhs.m_storage & rhs.m_storage);
    }

    constexpr friend EnumSet operator-(EnumSet lhs, EnumSet rhs)
    {
        return fromRaw(lhs.m_storage & ~rhs.m_storage);
    }

    constexpr friend EnumSet operator^(EnumSet lhs, EnumSet rhs)
    {
        return fromRaw(lhs.m_storage ^ rhs.m_storage);
    }

    static constexpr ptrdiff_t storageMemoryOffset() { return OBJECT_OFFSETOF(EnumSet, m_storage); }

private:
    constexpr bool get(E e) const { return m_storage & (static_cast<StorageType>(1) << std::to_underlying(e)); }
    constexpr void set(E e) { m_storage |= (static_cast<StorageType>(1) << std::to_underlying(e)); }

    enum InitializationTag { FromRawValue };
    constexpr EnumSet(StorageType rawValue, InitializationTag)
        : m_storage(rawValue)
    {
    }
    StorageType m_storage { 0 };
};

} // namespace WTF

using WTF::EnumSet;
