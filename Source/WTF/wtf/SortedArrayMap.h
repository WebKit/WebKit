/*

Copyright (C) 2021 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#pragma once

#include <wtf/text/StringView.h>

namespace WTF {

// SortedArrayMap is a map like HashMap, but it's read-only. It uses much less memory than HashMap.
// It uses binary search instead of hashing, so can be outperformed by HashMap for large maps.
// The array passed to the constructor has std::pair elements: keys first and values second.
// The array and the SortedArrayMap should typically both be global constant expressions.

template<typename ArrayType> class SortedArrayMap {
public:
    using ElementType = typename std::remove_extent_t<ArrayType>;
    using ValueType = typename ElementType::second_type;

    constexpr SortedArrayMap(const ArrayType&);

    // FIXME: To match HashMap interface better, would be nice to get the default value from traits.
    template<typename KeyArgument> ValueType get(const KeyArgument&, const ValueType& defaultValue = { }) const;

    // FIXME: Should add a function like this to HashMap so the two kinds of maps are more interchangable.
    template<typename KeyArgument> const ValueType* tryGet(const KeyArgument&) const;

private:
    const ArrayType& m_array;
};

struct ComparableStringView {
    StringView string;
};

struct ComparableASCIILiteral {
    const char* literal;
    template<std::size_t size> constexpr ComparableASCIILiteral(const char (&characters)[size]) : literal { characters } { }
    static Optional<ComparableStringView> parse(StringView string) { return { { string } }; }
};

constexpr bool operator==(ComparableASCIILiteral, ComparableASCIILiteral);
constexpr bool operator<(ComparableASCIILiteral, ComparableASCIILiteral);

bool operator==(ComparableStringView, ComparableASCIILiteral);
bool operator<(ComparableStringView, ComparableASCIILiteral);
bool operator!=(ComparableStringView, ComparableASCIILiteral);
bool operator==(ComparableASCIILiteral, ComparableStringView);

// FIXME: Use std::is_sorted instead of this and remove it, once we require C++20.
template<typename Iterator, typename Predicate> constexpr bool isSortedConstExpr(Iterator begin, Iterator end, Predicate predicate)
{
    if (begin == end)
        return true;
    auto current = begin;
    auto previous = current;
    while (++current != end) {
        if (!predicate(*previous, *current))
            return false;
        previous = current;
    }
    return true;
}

template<typename ArrayType> constexpr SortedArrayMap<ArrayType>::SortedArrayMap(const ArrayType& array)
    : m_array { array }
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(isSortedConstExpr(std::begin(array), std::end(array), [] (auto& a, auto b) {
        return a.first < b.first;
    }));
}

template<typename ArrayType> template<typename KeyArgument> inline auto SortedArrayMap<ArrayType>::tryGet(const KeyArgument& key) const -> const ValueType*
{
    auto parsedKey = ElementType::first_type::parse(key);
    if (!parsedKey)
        return nullptr;
    // FIXME: We should enhance tryBinarySearch so it can deduce ElementType.
    // FIXME: If the array's size is small enough, should do linear search since it is more efficient than binary search.
    auto element = tryBinarySearch<ElementType>(m_array, std::size(m_array), *parsedKey, [] (auto* element) {
        return element->first;
    });
    return element ? &element->second : nullptr;
}

template<typename ArrayType> template<typename KeyArgument> inline auto SortedArrayMap<ArrayType>::get(const KeyArgument& key, const ValueType& defaultValue) const -> ValueType
{
    auto result = tryGet(key);
    return result ? *result : defaultValue;
}

constexpr int strcmpConstExpr(const char* a, const char* b)
{
    while (*a == *b && *a && *b) {
        ++a;
        ++b;
    }
    return *a == *b ? 0 : *a < *b ? -1 : 1;
}

constexpr bool operator==(ComparableASCIILiteral a, ComparableASCIILiteral b)
{
    return !strcmpConstExpr(a.literal, b.literal);
}

constexpr bool operator<(ComparableASCIILiteral a, ComparableASCIILiteral b)
{
    return strcmpConstExpr(a.literal, b.literal) < 0;
}

inline bool operator==(ComparableStringView a, ComparableASCIILiteral b)
{
    return a.string == b.literal;
}

inline bool operator<(ComparableStringView a, ComparableASCIILiteral b)
{
    return codePointCompare(a.string, b.literal) < 0;
}

inline bool operator!=(ComparableStringView a, ComparableASCIILiteral b)
{
    return !(a == b);
}

inline bool operator==(ComparableASCIILiteral a, ComparableStringView b)
{
    return b == a;
}

}

using WTF::ComparableASCIILiteral;
using WTF::SortedArrayMap;
