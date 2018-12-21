/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringHasher.h>

namespace WTF {

// Deprecated. Use Hasher instead.
class IntegerHasher {
public:
    void add(uint32_t integer)
    {
        m_underlyingHasher.addCharactersAssumingAligned(integer, integer >> 16);
    }

    unsigned hash() const
    {
        return m_underlyingHasher.hash();
    }

private:
    StringHasher m_underlyingHasher;
};

template<typename... Types> uint32_t computeHash(const Types&...);
template<typename T, typename... OtherTypes> uint32_t computeHash(std::initializer_list<T>, std::initializer_list<OtherTypes>...);

class Hasher {
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

    template<typename UnsignedInteger> friend std::enable_if_t<std::is_unsigned<UnsignedInteger>::value && sizeof(UnsignedInteger) <= sizeof(uint32_t), void> add(Hasher& hasher, UnsignedInteger integer)
    {
        // We can consider adding a more efficient code path for hashing booleans or individual bytes if needed.
        // We can consider adding a more efficient code path for hashing 16-bit values if needed, perhaps using addCharacter,
        // but getting rid of "assuming aligned" would make hashing values 32-bit or larger slower.
        uint32_t sizedInteger = integer;
        hasher.m_underlyingHasher.addCharactersAssumingAligned(sizedInteger, sizedInteger >> 16);
    }

private:
    StringHasher m_underlyingHasher;
};

template<typename UnsignedInteger> std::enable_if_t<std::is_unsigned<UnsignedInteger>::value && sizeof(UnsignedInteger) == sizeof(uint64_t), void> add(Hasher& hasher, UnsignedInteger integer)
{
    add(hasher, static_cast<uint32_t>(integer));
    add(hasher, static_cast<uint32_t>(integer >> 32));
}

template<typename SignedArithmetic> std::enable_if_t<std::is_signed<SignedArithmetic>::value, void> add(Hasher& hasher, SignedArithmetic number)
{
    // We overloaded for double and float below, just deal with integers here.
    add(hasher, static_cast<std::make_unsigned_t<SignedArithmetic>>(number));
}

inline void add(Hasher& hasher, double number)
{
    add(hasher, bitwise_cast<uint64_t>(number));
}

inline void add(Hasher& hasher, float number)
{
    add(hasher, bitwise_cast<uint32_t>(number));
}

template<typename Enumeration> std::enable_if_t<std::is_enum<Enumeration>::value, void> add(Hasher& hasher, Enumeration value)
{
    add(hasher, static_cast<std::underlying_type_t<Enumeration>>(value));
}

template<typename> struct TypeCheckHelper { };
template<typename, typename = void> struct HasBeginFunctionMember : std::false_type { };
template<typename Container> struct HasBeginFunctionMember<Container, std::conditional_t<false, TypeCheckHelper<decltype(std::declval<Container>().begin())>, void>> : std::true_type { };

template<typename Container> std::enable_if_t<HasBeginFunctionMember<Container>::value, void> add(Hasher& hasher, const Container& container)
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

template<typename Tuple, std::size_t ...i> void addTupleHelper(Hasher& hasher, const Tuple& values, std::index_sequence<i...>)
{
    addArgs(hasher, std::get<i>(values)...);
}

template<typename... Types> void add(Hasher& hasher, const std::tuple<Types...>& tuple)
{
    addTupleHelper(hasher, tuple, std::make_index_sequence<std::tuple_size<std::tuple<Types...>>::value> { });
}

template<typename T1, typename T2> void add(Hasher& hasher, const std::pair<T1, T2>& pair)
{
    add(hasher, pair.first);
    add(hasher, pair.second);
}

template<typename T> void add(Hasher& hasher, const Optional<T>& optional)
{
    add(hasher, optional.hasValue());
    if (optional.hasValue())
        add(hasher, optional.value());
}

template<typename... Types> void add(Hasher& hasher, const Variant<Types...>& variant)
{
    add(hasher, variant.index());
    visit([&hasher] (auto& value) {
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

} // namespace WTF

using WTF::computeHash;
using WTF::Hasher;
using WTF::IntegerHasher;
