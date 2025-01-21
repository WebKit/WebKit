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
#include "CSSPrimitiveNumericConcepts.h"
#include "CSSPrimitiveNumericRaw.h"
#include "CSSUnevaluatedCalc.h"
#include <limits>
#include <type_traits>
#include <wtf/EnumTraits.h>

namespace WebCore {
namespace CSS {

// `PrimitiveData` is a bespoke implementation of `std::variant<Numeric, Ks...>`
// optimized for memory use by allowing numeric types with multiple unit representations
// (e.g. <length>, <angle>, etc.) to utilize multiple indices for a single smaller payload.
//
// FIXME: Generalize this concept to support arbitrary types through traits.

// MARK: - Concepts

// Concept for use checking if a `ChildPrimitiveData`'s types are a subset of
// `ParentPrimitiveData`'s types.
// FIXME: Currently limited to the case of Parent<NumericA, KeywordB, ...> and Child == NumericA.
template<typename ChildPrimitiveData, typename ParentPrimitiveData> concept SubsumesChildPrimitiveData
    = (!std::same_as<ChildPrimitiveData, ParentPrimitiveData>)
   && (std::same_as<typename ChildPrimitiveData::Index, typename ParentPrimitiveData::Index::NumericType::Base::Index>);

// MARK: - Markable Support

struct PrimitiveDataEmptyToken { constexpr bool operator==(const PrimitiveDataEmptyToken&) const = default; };

template<typename T> struct PrimitiveDataMarkableTraits {
    static bool isEmptyValue(const T& value) { return value.isEmpty(); }
    static T emptyValue() { return T(PrimitiveDataEmptyToken { }); }
};

// MARK: - Index

template<Numeric N, PrimitiveKeyword... Ks> struct PrimitiveDataIndex {
    using NumericType = N;
    using KeywordList = PrimitiveKeywordList<Ks...>;

    using Raw = typename N::Raw;
    using Calc = typename N::Calc;
    using UnitType = typename N::UnitType;
    using UnitTraits = typename N::UnitTraits;

    using Storage = std::underlying_type_t<typename N::UnitType>;

    // The potential values for the `index` are:
    //  - 0 ... # of units - 1                              -> Raw
    //  - # of units                                        -> Calc
    //  - # of units + 1 ... # of units + # of keywords     -> Constant<Id>
    //
    // (... gap ...)
    //
    //  - max(index_type) - 1                               -> Empty (for Markable)
    //  - max(index_type)                                   -> Moved from

    static constexpr Storage indexStorageForFirstRaw       = 0;
    static constexpr Storage indexStorageForLastRaw        = UnitTraits::count - 1;
    static constexpr Storage indexStorageForCalc           = UnitTraits::count;
    static constexpr Storage indexStorageForFirstKeyword   = UnitTraits::count + 1;
    static constexpr Storage indexStorageForLastKeyword    = UnitTraits::count + KeywordList::count;
    // (... gap ...)
    static constexpr Storage indexStorageForEmpty          = std::numeric_limits<Storage>::max() - 1;
    static constexpr Storage indexStorageForMovedFrom      = std::numeric_limits<Storage>::max();

    static constexpr Storage indexStorageForUnit(UnitType unit)
    {
        return indexStorageForFirstRaw + enumToUnderlyingType(unit);
    }

    static consteval Storage indexStorageForKeyword(ValidKeywordForList<KeywordList> auto keyword)
    {
        return indexStorageForFirstKeyword + KeywordList::offsetForKeyword(keyword);
    }

    static_assert(UnitTraits::count + KeywordList::count + 2 <= std::numeric_limits<Storage>::max());

    // MARK: Construction

    PrimitiveDataIndex(const PrimitiveDataIndex<N, Ks...>&) = default;

    template<typename T>
        requires (KeywordList::count != 0) && (requires {
            requires std::same_as<T, PrimitiveDataIndex<typename N::Base>>;
        })
    PrimitiveDataIndex(const T& other)
        : storage { other.storage }
    {
    }

    template<typename T>
        requires (KeywordList::count != 0) && (requires {
            requires std::same_as<T, PrimitiveDataIndex<typename N::Base>>;
        })
    PrimitiveDataIndex& operator=(const T& other)
    {
        storage = other.storage;
        return *this;
    }

    constexpr explicit PrimitiveDataIndex(Storage storage)
        : storage { storage }
    {
    }

    constexpr PrimitiveDataIndex(UnitType unit)
        : storage { indexStorageForUnit(unit) }
    {
    }

    constexpr PrimitiveDataIndex(const Raw& raw)
        : storage { indexStorageForUnit(raw.unit) }
    {
    }

    constexpr PrimitiveDataIndex(const Calc&)
        : storage { indexStorageForCalc }
    {
    }

    constexpr PrimitiveDataIndex(ValidKeywordForList<KeywordList> auto keyword)
        : storage { indexStorageForKeyword(keyword)  }
    {
    }

    constexpr PrimitiveDataIndex(PrimitiveDataEmptyToken)
        : storage { indexStorageForEmpty }
    {
    }

    // MARK: Assignment

    PrimitiveDataIndex& operator=(const PrimitiveDataIndex<N, Ks...>&) = default;


    // MARK: Raw Unit

    constexpr typename NumericType::Raw::UnitType unit() const
    {
        ASSERT_UNDER_CONSTEXPR_CONTEXT(isRaw());
        return static_cast<UnitType>(storage);
    }

    // MARK: Keyword

    template<typename F> constexpr decltype(auto) visitKeyword(F&& f) const
    {
        ASSERT_UNDER_CONSTEXPR_CONTEXT(isAnyKeyword());
        return KeywordList::visitKeywordAtOffset(storage - indexStorageForFirstKeyword, std::forward<F>(f));
    }

    // MARK: Predicates

    constexpr bool isSpecified() const
    {
        return storage >= indexStorageForFirstRaw && storage <= indexStorageForCalc;
    }

    constexpr bool isAnyKeyword() const
    {
        if constexpr (KeywordList::count == 0)
            return false;
        else
            return storage >= indexStorageForFirstKeyword && storage <= indexStorageForLastKeyword;
    }

    constexpr bool isRaw() const
    {
        return storage >= indexStorageForFirstRaw && storage <= indexStorageForLastRaw;
    }

    constexpr bool isCalc() const
    {
        return storage == indexStorageForCalc;
    }

    constexpr bool isKeyword(ValidKeywordForList<KeywordList> auto keyword) const
    {
        return storage == indexStorageForKeyword(keyword);
    }

    constexpr bool isEmpty() const
    {
        return storage == indexStorageForEmpty;
    }

    constexpr bool isMovedFrom() const
    {
        return storage == indexStorageForMovedFrom;
    }

    void setAsMovedFrom()
    {
        storage = indexStorageForMovedFrom;
    }

    constexpr bool operator==(const PrimitiveDataIndex&) const = default;
    constexpr bool operator==(Storage other) const { return storage == other; }

    Storage storage;
};

} // namespace CSS
} // namespace WebCore
