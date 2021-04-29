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

template<typename ArrayType> class SortedArraySet {
public:
    constexpr SortedArraySet(const ArrayType&);
    template<typename KeyArgument> bool contains(const KeyArgument&) const;

private:
    const ArrayType& m_array;
};

struct ComparableStringView {
    StringView string;
};

// NoUppercaseLettersOptimized means no characters with the 0x20 bit set.
// That means the strings can't include control characters, uppercase letters, or any of @[\]_.
enum class ASCIISubset { All, NoUppercaseLetters, NoUppercaseLettersOptimized };

template<ASCIISubset> struct ComparableASCIISubsetLiteral {
    ASCIILiteral literal;
    template<std::size_t size> constexpr ComparableASCIISubsetLiteral(const char (&characters)[size]);
    static Optional<ComparableStringView> parse(StringView string) { return { { string } }; }
};

using ComparableASCIILiteral = ComparableASCIISubsetLiteral<ASCIISubset::All>;
using ComparableCaseFoldingASCIILiteral = ComparableASCIISubsetLiteral<ASCIISubset::NoUppercaseLetters>;
using ComparableLettersLiteral = ComparableASCIISubsetLiteral<ASCIISubset::NoUppercaseLettersOptimized>;

template<ASCIISubset subset> constexpr bool operator==(ComparableASCIISubsetLiteral<subset>, ComparableASCIISubsetLiteral<subset>);
template<ASCIISubset subset> constexpr bool operator<(ComparableASCIISubsetLiteral<subset>, ComparableASCIISubsetLiteral<subset>);

bool operator==(ComparableStringView, ComparableASCIILiteral);
bool operator==(ComparableStringView, ComparableCaseFoldingASCIILiteral);
bool operator==(ComparableStringView, ComparableLettersLiteral);
bool operator<(ComparableStringView, ComparableASCIILiteral);
bool operator<(ComparableStringView, ComparableCaseFoldingASCIILiteral);
bool operator<(ComparableStringView, ComparableLettersLiteral);

template<typename OtherType> bool operator==(OtherType, ComparableStringView);
template<typename OtherType> bool operator!=(ComparableStringView, OtherType);

template<ASCIISubset subset> constexpr bool isInSubset(char character)
{
    if (!(character && isASCII(character)))
        return false;
    switch (subset) {
    case ASCIISubset::All:
        return true;
    case ASCIISubset::NoUppercaseLetters:
        return !isASCIIUpper(character);
    case ASCIISubset::NoUppercaseLettersOptimized:
        return character == toASCIILowerUnchecked(character);
    }
}

template<ASCIISubset subset> template<std::size_t size> constexpr ComparableASCIISubsetLiteral<subset>::ComparableASCIISubsetLiteral(const char (&characters)[size])
    : literal { ASCIILiteral::fromLiteralUnsafe(characters) }
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(allOfConstExpr(&characters[0], &characters[size - 1], [] (char character) {
        return isInSubset<subset>(character);
    }));
    ASSERT_UNDER_CONSTEXPR_CONTEXT(!characters[size - 1]);
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

template<typename ArrayType> constexpr SortedArraySet<ArrayType>::SortedArraySet(const ArrayType& array)
    : m_array { array }
{
    ASSERT_UNDER_CONSTEXPR_CONTEXT(isSortedConstExpr(std::begin(array), std::end(array)));
}

template<typename ArrayType> template<typename KeyArgument> inline bool SortedArraySet<ArrayType>::contains(const KeyArgument& key) const
{
    using ElementType = typename std::remove_extent_t<ArrayType>;
    auto parsedKey = ElementType::parse(key);
    // FIXME: We should enhance tryBinarySearch so it can deduce ElementType.
    // FIXME: If the array's size is small enough, should do linear search since it is more efficient than binary search.
    return parsedKey && tryBinarySearch<ElementType>(m_array, std::size(m_array), *parsedKey, [] (auto* element) {
        return *element;
    });
}

constexpr int strcmpConstExpr(const char* a, const char* b)
{
    while (*a == *b && *a && *b) {
        ++a;
        ++b;
    }
    return *a == *b ? 0 : *a < *b ? -1 : 1;
}

template<typename CharacterType> inline bool lessThanASCIICaseFolding(const CharacterType* characters, unsigned length, const char* literalWithNoUppercase)
{
    for (unsigned i = 0; i < length; ++i) {
        if (!literalWithNoUppercase[i])
            return false;
        auto character = toASCIILower(characters[i]);
        if (character != literalWithNoUppercase[i])
            return character < literalWithNoUppercase[i];
    }
    return true;
}

inline bool lessThanASCIICaseFolding(StringView string, const char* literalWithNoUppercase)
{
    if (string.is8Bit())
        return lessThanASCIICaseFolding(string.characters8(), string.length(), literalWithNoUppercase);
    return lessThanASCIICaseFolding(string.characters16(), string.length(), literalWithNoUppercase);
}

template<ASCIISubset subset> constexpr bool operator==(ComparableASCIISubsetLiteral<subset> a, ComparableASCIISubsetLiteral<subset> b)
{
    return !strcmpConstExpr(a.literal.characters(), b.literal.characters());
}

template<ASCIISubset subset> constexpr bool operator<(ComparableASCIISubsetLiteral<subset> a, ComparableASCIISubsetLiteral<subset> b)
{
    return strcmpConstExpr(a.literal.characters(), b.literal.characters()) < 0;
}

inline bool operator==(ComparableStringView a, ComparableASCIILiteral b)
{
    return a.string == b.literal;
}

inline bool operator<(ComparableStringView a, ComparableASCIILiteral b)
{
    return codePointCompare(a.string, b.literal.characters()) < 0;
}

inline bool operator==(ComparableStringView a, ComparableLettersLiteral b)
{
    return equalLettersIgnoringASCIICaseCommonWithoutLength(a.string, b.literal);
}

inline bool operator<(ComparableStringView a, ComparableLettersLiteral b)
{
    return lessThanASCIICaseFolding(a.string, b.literal);
}

inline bool operator==(ComparableStringView a, ComparableCaseFoldingASCIILiteral b)
{
    return equalIgnoringASCIICase(a.string, b.literal);
}

inline bool operator<(ComparableStringView a, ComparableCaseFoldingASCIILiteral b)
{
    return lessThanASCIICaseFolding(a.string, b.literal);
}

template<typename OtherType> inline bool operator==(OtherType a, ComparableStringView b)
{
    return b == a;
}

template<typename OtherType> inline bool operator!=(ComparableStringView a, OtherType b)
{
    return !(a == b);
}

}

using WTF::ASCIISubset;
using WTF::ComparableASCIILiteral;
using WTF::ComparableASCIISubsetLiteral;
using WTF::ComparableCaseFoldingASCIILiteral;
using WTF::ComparableLettersLiteral;
using WTF::SortedArrayMap;
using WTF::SortedArraySet;
