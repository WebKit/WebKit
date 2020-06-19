/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <wtf/Assertions.h>
#include <wtf/EnumTraits.h>
#include <wtf/MathExtras.h>
#include <wtf/Optional.h>

namespace WTF {

// Detecting in C++ whether a type is defined, part 3: SFINAE and incomplete types
// <https://devblogs.microsoft.com/oldnewthing/20190710-00/?p=102678>
template<typename, typename = void>
constexpr bool is_type_complete_v = false;

template<typename T>
constexpr bool is_type_complete_v<T, std::void_t<decltype(sizeof(T))>> = true;


template<typename E> class OptionSet;


template<typename T, typename E> struct OptionSetValueChecker;

template<typename T, typename E, E e, E... es>
struct OptionSetValueChecker<T, EnumValues<E, e, es...>> {
    static constexpr T allValidBits()
    {
        return static_cast<T>(e) | OptionSetValueChecker<T, EnumValues<E, es...>>::allValidBits();
    }

    static constexpr bool isValidOptionSetEnum(T t)
    {
        return (static_cast<T>(e) == t) ? true : OptionSetValueChecker<T, EnumValues<E, es...>>::isValidOptionSetEnum(t);
    }
};

template<typename T, typename E>
struct OptionSetValueChecker<T, EnumValues<E>> {
    static constexpr T allValidBits()
    {
        return 0;
    }

    static constexpr bool isValidOptionSetEnum(T)
    {
        return false;
    }
};


template<typename E, std::enable_if_t<std::is_enum<E>::value && is_type_complete_v<EnumTraits<E>>>* = nullptr>
constexpr bool isValidOptionSetEnum(E e)
{
    return OptionSetValueChecker<std::underlying_type_t<E>, typename EnumTraits<E>::values>::isValidOptionSetEnum(static_cast<std::underlying_type_t<E>>(e));
}

template<typename E, std::enable_if_t<std::is_enum<E>::value && !is_type_complete_v<EnumTraits<E>>>* = nullptr>
constexpr bool isValidOptionSetEnum(E e)
{
    // FIXME: Remove once all OptionSet<> enums have EnumTraits<> defined.
    return hasOneBitSet(static_cast<typename OptionSet<E>::StorageType>(e));
}


template<typename E, std::enable_if_t<std::is_enum<E>::value && is_type_complete_v<EnumTraits<E>>>* = nullptr>
constexpr typename OptionSet<E>::StorageType maskRawValue(typename OptionSet<E>::StorageType rawValue)
{
    auto allValidBitsValue = OptionSetValueChecker<std::underlying_type_t<E>, typename EnumTraits<E>::values>::allValidBits();
    return rawValue & allValidBitsValue;
}

template<typename E, std::enable_if_t<std::is_enum<E>::value && !is_type_complete_v<EnumTraits<E>>>* = nullptr>
constexpr typename OptionSet<E>::StorageType maskRawValue(typename OptionSet<E>::StorageType rawValue)
{
    // FIXME: Remove once all OptionSet<> enums have EnumTraits<> defined.
    return rawValue;
}


// OptionSet is a class that represents a set of enumerators in a space-efficient manner. The enumerators
// must be powers of two greater than 0. This class is useful as a replacement for passing a bitmask of
// enumerators around.
template<typename E> class OptionSet {
    WTF_MAKE_FAST_ALLOCATED;
    static_assert(std::is_enum<E>::value, "T is not an enum type");

public:
    using StorageType = std::make_unsigned_t<std::underlying_type_t<E>>;

    template<typename StorageType> class Iterator {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        // Isolate the rightmost set bit.
        E operator*() const { return static_cast<E>(m_value & -m_value); }

        // Iterates from smallest to largest enum value by turning off the rightmost set bit.
        Iterator& operator++()
        {
            m_value &= m_value - 1;
            return *this;
        }

        Iterator& operator++(int) = delete;

        bool operator==(const Iterator& other) const { return m_value == other.m_value; }
        bool operator!=(const Iterator& other) const { return m_value != other.m_value; }

    private:
        Iterator(StorageType value) : m_value(value) { }
        friend OptionSet;

        StorageType m_value;
    };
    using iterator = Iterator<StorageType>;

    static constexpr OptionSet fromRaw(StorageType rawValue)
    {
        return OptionSet(static_cast<E>(maskRawValue<E>(rawValue)), FromRawValue);
    }

    constexpr OptionSet() = default;

    constexpr OptionSet(E e)
        : m_storage(static_cast<StorageType>(e))
    {
        ASSERT(!m_storage || isValidOptionSetEnum(e));
    }

    constexpr OptionSet(std::initializer_list<E> initializerList)
    {
        for (auto& option : initializerList) {
            ASSERT(isValidOptionSetEnum(option));
            m_storage |= static_cast<StorageType>(option);
        }
    }

    constexpr OptionSet(Optional<E> optional)
        : m_storage(optional ? static_cast<StorageType>(*optional) : 0)
    {
    }

    constexpr StorageType toRaw() const { return m_storage; }

    constexpr bool isEmpty() const { return !m_storage; }

    constexpr iterator begin() const { return m_storage; }
    constexpr iterator end() const { return 0; }

    constexpr explicit operator bool() { return !isEmpty(); }

    constexpr bool contains(E option) const
    {
        return containsAny(option);
    }

    constexpr bool containsAny(OptionSet optionSet) const
    {
        return !!(*this & optionSet);
    }

    constexpr bool containsAll(OptionSet optionSet) const
    {
        return (*this & optionSet) == optionSet;
    }

    constexpr void add(OptionSet optionSet)
    {
        m_storage |= optionSet.m_storage;
    }

    constexpr void remove(OptionSet optionSet)
    {
        m_storage &= ~optionSet.m_storage;
    }

    constexpr bool hasExactlyOneBitSet() const
    {
        return m_storage && !(m_storage & (m_storage - 1));
    }

    constexpr Optional<E> toSingleValue() const
    {
        return hasExactlyOneBitSet() ? Optional<E>(static_cast<E>(m_storage)) : WTF::nullopt;
    }

    constexpr friend bool operator==(OptionSet lhs, OptionSet rhs)
    {
        return lhs.m_storage == rhs.m_storage;
    }

    constexpr friend bool operator!=(OptionSet lhs, OptionSet rhs)
    {
        return lhs.m_storage != rhs.m_storage;
    }

    constexpr friend OptionSet operator|(OptionSet lhs, OptionSet rhs)
    {
        return fromRaw(lhs.m_storage | rhs.m_storage);
    }

    constexpr friend OptionSet operator&(OptionSet lhs, OptionSet rhs)
    {
        return fromRaw(lhs.m_storage & rhs.m_storage);
    }

    constexpr friend OptionSet operator-(OptionSet lhs, OptionSet rhs)
    {
        return fromRaw(lhs.m_storage & ~rhs.m_storage);
    }

    constexpr friend OptionSet operator^(OptionSet lhs, OptionSet rhs)
    {
        return fromRaw(lhs.m_storage ^ rhs.m_storage);
    }

private:
    enum InitializationTag { FromRawValue };
    constexpr OptionSet(E e, InitializationTag)
        : m_storage(static_cast<StorageType>(e))
    {
    }
    StorageType m_storage { 0 };
};

template<typename E>
WARN_UNUSED_RETURN constexpr bool isValidOptionSet(OptionSet<E> optionSet)
{
    auto allValidBitsValue = OptionSetValueChecker<std::make_unsigned_t<std::underlying_type_t<E>>, typename EnumTraits<E>::values>::allValidBits();
    return (optionSet.toRaw() | allValidBitsValue) == allValidBitsValue;
}

} // namespace WTF

using WTF::OptionSet;
