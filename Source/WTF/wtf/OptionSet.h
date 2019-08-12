/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include <wtf/MathExtras.h>

namespace WTF {

// OptionSet is a class that represents a set of enumerators in a space-efficient manner. The enumerators
// must be powers of two greater than 0. This class is useful as a replacement for passing a bitmask of
// enumerators around.
template<typename T> class OptionSet {
    WTF_MAKE_FAST_ALLOCATED;
    static_assert(std::is_enum<T>::value, "T is not an enum type");
    typedef typename std::make_unsigned<typename std::underlying_type<T>::type>::type StorageType;

public:
    template<typename StorageType> class Iterator {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        // Isolate the rightmost set bit.
        T operator*() const { return static_cast<T>(m_value & -m_value); }

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

    static constexpr OptionSet fromRaw(StorageType storageType)
    {
        return OptionSet(static_cast<T>(storageType), FromRawValue);
    }

    constexpr OptionSet() = default;

    constexpr OptionSet(T t)
        : m_storage(static_cast<StorageType>(t))
    {
        ASSERT_WITH_MESSAGE(!m_storage || hasOneBitSet(m_storage), "Enumerator is not a zero or a positive power of two.");
    }

    constexpr OptionSet(std::initializer_list<T> initializerList)
    {
        for (auto& option : initializerList) {
            ASSERT_WITH_MESSAGE(hasOneBitSet(static_cast<StorageType>(option)), "Enumerator is not a positive power of two.");
            m_storage |= static_cast<StorageType>(option);
        }
    }

    constexpr StorageType toRaw() const { return m_storage; }

    constexpr bool isEmpty() const { return !m_storage; }

    constexpr iterator begin() const { return m_storage; }
    constexpr iterator end() const { return 0; }

    constexpr explicit operator bool() { return !isEmpty(); }

    constexpr bool contains(T option) const
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
    constexpr OptionSet(T t, InitializationTag)
        : m_storage(static_cast<StorageType>(t))
    {
    }
    StorageType m_storage { 0 };
};

}

using WTF::OptionSet;
