/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <algorithm>
#include <iterator>
#include <unicode/umachine.h>
#include <utility>
#include <wtf/Optional.h>

namespace WebCore {

extern const std::pair<UChar, uint16_t> jis0208[7724];
extern const std::pair<uint16_t, UChar> jis0212[6067];
extern const std::pair<UChar32, uint16_t> big5EncodingMap[14686];
extern const std::pair<uint16_t, UChar32> big5DecodingExtras[3904];
extern const std::pair<uint16_t, UChar> eucKRDecodingIndex[17048];

void checkEncodingTableInvariants();

// Functions for using sorted arrays of pairs as a map.
// FIXME: Consider moving these functions to StdLibExtras.h for uses other than encoding tables.
template<typename CollectionType> void sortByFirst(CollectionType&);
template<typename CollectionType> bool isSortedByFirst(const CollectionType&);
template<typename CollectionType> bool sortedFirstsAreUnique(const CollectionType&);
template<typename CollectionType, typename KeyType> static auto findFirstInSortedPairs(const CollectionType& collection, const KeyType&) -> Optional<decltype(std::begin(collection)->second)>;
template<typename CollectionType, typename KeyType> static auto findLastInSortedPairs(const CollectionType& collection, const KeyType&) -> Optional<decltype(std::begin(collection)->second)>;
template<typename CollectionType, typename KeyType> static auto findInSortedPairs(const CollectionType& collection, const KeyType&) -> std::pair<decltype(std::begin(collection)), decltype(std::begin(collection))>;

#if !ASSERT_ENABLED
inline void checkEncodingTableInvariants() { }
#endif

struct CompareFirst {
    template<typename TypeA, typename TypeB> bool operator()(const TypeA& a, const TypeB& b)
    {
        return a.first < b.first;
    }
};

struct EqualFirst {
    template<typename TypeA, typename TypeB> bool operator()(const TypeA& a, const TypeB& b)
    {
        return a.first == b.first;
    }
};

template<typename T> struct FirstAdapter {
    const T& first;
};
template<typename T> FirstAdapter<T> makeFirstAdapter(const T& value)
{
    return { value };
}

template<typename CollectionType> void sortByFirst(CollectionType& collection)
{
    std::sort(std::begin(collection), std::end(collection), CompareFirst { });
}

template<typename CollectionType> bool isSortedByFirst(const CollectionType& collection)
{
    return std::is_sorted(std::begin(collection), std::end(collection), CompareFirst { });
}

template<typename CollectionType> bool sortedFirstsAreUnique(const CollectionType& collection)
{
    return std::adjacent_find(std::begin(collection), std::end(collection), EqualFirst { }) == std::end(collection);
}

template<typename CollectionType, typename KeyType> static auto findFirstInSortedPairs(const CollectionType& collection, const KeyType& key) -> Optional<decltype(std::begin(collection)->second)>
{
    if constexpr (std::is_integral_v<KeyType>) {
        if (key != decltype(std::begin(collection)->first)(key))
            return WTF::nullopt;
    }
    auto iterator = std::lower_bound(std::begin(collection), std::end(collection), makeFirstAdapter(key), CompareFirst { });
    if (iterator == std::end(collection) || key < iterator->first)
        return WTF::nullopt;
    return iterator->second;
}

template<typename CollectionType, typename KeyType> static auto findLastInSortedPairs(const CollectionType& collection, const KeyType& key) -> Optional<decltype(std::begin(collection)->second)>
{
    if constexpr (std::is_integral_v<KeyType>) {
        if (key != decltype(std::begin(collection)->first)(key))
            return WTF::nullopt;
    }
    auto iterator = std::upper_bound(std::begin(collection), std::end(collection), makeFirstAdapter(key), CompareFirst { });
    if (iterator == std::begin(collection) || (--iterator)->first < key)
        return WTF::nullopt;
    return iterator->second;
}

template<typename CollectionType, typename KeyType> static auto findInSortedPairs(const CollectionType& collection, const KeyType& key) -> std::pair<decltype(std::begin(collection)), decltype(std::begin(collection))>
{
    if constexpr (std::is_integral_v<KeyType>) {
        if (key != decltype(std::begin(collection)->first)(key))
            return { std::end(collection), std::end(collection) };
    }
    return std::equal_range(std::begin(collection), std::end(collection), makeFirstAdapter(key), CompareFirst { });
}

}
