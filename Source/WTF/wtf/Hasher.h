/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <optional>
#include <variant>
#include <wtf/CheckedPtr.h>
#include <wtf/Int128.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/SuperFastHash.h>

namespace WTF {

template<typename... Types> uint32_t computeHash(const Types&...);
template<typename T, typename... OtherTypes> uint32_t computeHash(std::initializer_list<T>, std::initializer_list<OtherTypes>...);
template<typename UnsignedInteger> std::enable_if_t<std::is_unsigned_v<UnsignedInteger> && sizeof(UnsignedInteger) <= sizeof(uint32_t) && !std::is_enum_v<UnsignedInteger>, void> add(Hasher&, UnsignedInteger);

class Hasher {
    WTF_MAKE_FAST_ALLOCATED;
public:
    template<typename... Types> friend uint32_t computeHash(const Types&... values)
    {
        Hasher hasher;
        addArgs(hasher, values...);
        return hasher.m_underlyingHasher.hash();
    }

    template<typename T, typename... OtherTypes> friend uint32_t computeHash(std::initializer_list<T> list, std::initializer_list<OtherTypes>... otherLists)
    {
        Hasher hasher;
        add(hasher, list);
        addArgs(hasher, otherLists...);
        return hasher.m_underlyingHasher.hash();
    }

    template<typename UnsignedInteger> friend std::enable_if_t<std::is_unsigned_v<UnsignedInteger> && sizeof(UnsignedInteger) <= sizeof(uint32_t) && !std::is_enum_v<UnsignedInteger>, void> add(Hasher& hasher, UnsignedInteger integer)
    {
        // We can consider adding a more efficient code path for hashing booleans or individual bytes if needed.
        // We can consider adding a more efficient code path for hashing 16-bit values if needed, perhaps using addCharacter,
        // but getting rid of "assuming aligned" would make hashing values 32-bit or larger slower.
        uint32_t sizedInteger = integer;
        hasher.m_underlyingHasher.addCharactersAssumingAligned(sizedInteger, sizedInteger >> 16);
    }

    unsigned hash() const
    {
        return m_underlyingHasher.hash();
    }

private:
    SuperFastHash m_underlyingHasher;
};

template<typename UnsignedInteger> std::enable_if_t<std::is_unsigned<UnsignedInteger>::value && sizeof(UnsignedInteger) == sizeof(uint64_t), void> add(Hasher& hasher, UnsignedInteger integer)
{
    add(hasher, static_cast<uint32_t>(integer));
    add(hasher, static_cast<uint32_t>(integer >> 32));
}

inline void add(Hasher& hasher, UInt128 value)
{
    auto high = static_cast<uint64_t>(value >> 64);
    auto low = static_cast<uint64_t>(value);
    add(hasher, high);
    add(hasher, low);
}

template<typename SignedArithmetic> std::enable_if_t<std::is_signed<SignedArithmetic>::value, void> add(Hasher& hasher, SignedArithmetic number)
{
    // We overloaded for double and float below, just deal with integers here.
    add(hasher, static_cast<std::make_unsigned_t<SignedArithmetic>>(number));
}

inline void add(Hasher& hasher, bool boolean)
{
    add(hasher, static_cast<uint8_t>(boolean));
}

inline void add(Hasher& hasher, double number)
{
    add(hasher, bitwise_cast<uint64_t>(number));
}

inline void add(Hasher& hasher, float number)
{
    add(hasher, bitwise_cast<uint32_t>(number));
}

template<typename T> inline void add(Hasher& hasher, T* ptr)
{
    add(hasher, bitwise_cast<uintptr_t>(ptr));
}

inline void add(Hasher& hasher, const String& string)
{
    // Chose to hash the characters here. Assuming this is better than hashing the possibly-already-computed hash of the characters.
    bool remainder = string.length() & 1;
    unsigned roundedLength = string.length() - remainder;
    for (unsigned i = 0; i < roundedLength; i += 2)
        add(hasher, (string[i] << 16) | string[i + 1]);
    if (remainder)
        add(hasher, string[roundedLength]);
}

inline void add(Hasher& hasher, const AtomString& string)
{
    // Chose to hash the pointer here. Assuming this is better than hashing the characters or hashing the already-computed hash of the characters.
    add(hasher, bitwise_cast<uintptr_t>(string.impl()));
}

inline void add(Hasher& hasher, const URL& url)
{
    add(hasher, url.string());
}

template<typename Enumeration> std::enable_if_t<std::is_enum_v<Enumeration>, void> add(Hasher& hasher, Enumeration value)
{
    add(hasher, static_cast<std::underlying_type_t<Enumeration>>(value));
}

template<typename, typename = void> inline constexpr bool HasBeginFunctionMember = false;
template<typename T> inline constexpr bool HasBeginFunctionMember<T, std::void_t<decltype(std::declval<T>().begin())>> = true;

template<typename Container> std::enable_if_t<HasBeginFunctionMember<Container> && !IsTypeComplete<std::tuple_size<Container>>, void> add(Hasher& hasher, const Container& container)
{
    for (const auto& value : container)
        add(hasher, value);
}

inline void addArgs(Hasher&)
{
}

template<typename Arg, typename ...Args> void addArgs(Hasher& hasher, const Arg& arg, const Args&... args)
{
    add(hasher, arg);
    addArgs(hasher, args...);
}

template<typename, typename = void> inline constexpr bool HasGetFunctionMember = false;
template<typename T> inline constexpr bool HasGetFunctionMember<T, std::void_t<decltype(std::declval<T>().template get<0>())>> = true;

template<typename TupleLike, std::size_t ...I> void addTupleLikeHelper(Hasher& hasher, const TupleLike& tupleLike, std::index_sequence<I...>)
{
    if constexpr (HasGetFunctionMember<TupleLike>)
        addArgs(hasher, tupleLike.template get<I>()...);
    else {
        using std::get;
        addArgs(hasher, get<I>(tupleLike)...);
    }
}

template<typename TupleLike> std::enable_if_t<IsTypeComplete<std::tuple_size<TupleLike>>, void> add(Hasher& hasher, const TupleLike& tuple)
{
    addTupleLikeHelper(hasher, tuple, std::make_index_sequence<std::tuple_size<TupleLike>::value> { });
}

template<typename T> void add(Hasher& hasher, const std::optional<T>& optional)
{
    add(hasher, optional.has_value());
    if (optional.has_value())
        add(hasher, optional.value());
}

template<typename... Types> void add(Hasher& hasher, const std::variant<Types...>& variant)
{
    add(hasher, variant.index());
    std::visit([&hasher] (auto& value) {
        add(hasher, value);
    }, variant);
}

template<typename T1, typename T2, typename... OtherTypes> void add(Hasher& hasher, const T1& value1, const T2& value2, const OtherTypes&... otherValues)
{
    add(hasher, value1);
    add(hasher, value2);
    addArgs(hasher, otherValues...);
}

template<typename T> void add(Hasher& hasher, std::initializer_list<T> values)
{
    for (auto& value : values)
        add(hasher, value);
}

template<typename T, typename U, typename V> void add(Hasher& hasher, const RefPtr<T, U, V>& refPtr)
{
    add(hasher, refPtr.get());
}

template<typename T, typename U> void add(Hasher& hasher, const CheckedPtr<T, U>& checkedPtr)
{
    add(hasher, checkedPtr.get());
}

} // namespace WTF

using WTF::computeHash;
using WTF::Hasher;
