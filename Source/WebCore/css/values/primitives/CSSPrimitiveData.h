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

#include <WebCore/CSSPrimitiveKeywordList.h>
#include <WebCore/CSSPrimitiveNumericConcepts.h>
#include <WebCore/CSSPrimitiveNumericRaw.h>
#include <WebCore/CSSUnevaluatedCalc.h>
#include <limits>
#include <type_traits>
#include <wtf/EnumTraits.h>

namespace WebCore {
namespace CSS {

// `PrimitiveData` is a bespoke implementation of `Variant<Numeric, Keywords...>`
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
    using Keywords = PrimitiveKeywordList<Ks...>;

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
    static constexpr Storage indexStorageForLastKeyword    = UnitTraits::count + Keywords::count;
    // (... gap ...)
    static constexpr Storage indexStorageForEmpty          = std::numeric_limits<Storage>::max() - 1;
    static constexpr Storage indexStorageForMovedFrom      = std::numeric_limits<Storage>::max();

    static constexpr Storage indexStorageForUnit(UnitType unit)
    {
        return indexStorageForFirstRaw + enumToUnderlyingType(unit);
    }

    static consteval Storage indexStorageForKeyword(ValidKeywordForList<Keywords> auto keyword)
    {
        return indexStorageForFirstKeyword + Keywords::offsetForKeyword(keyword);
    }

    static_assert(UnitTraits::count + Keywords::count + 2 <= std::numeric_limits<Storage>::max());

    // MARK: Construction

    PrimitiveDataIndex(const PrimitiveDataIndex<N, Ks...>&) = default;

    template<typename T>
        requires (Keywords::count != 0) && (requires {
            requires std::same_as<T, PrimitiveDataIndex<typename N::Base>>;
        })
    PrimitiveDataIndex(const T& other)
        : storage { other.storage }
    {
    }

    template<typename T>
        requires (Keywords::count != 0) && (requires {
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

    constexpr PrimitiveDataIndex(ValidKeywordForList<Keywords> auto keyword)
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
        ASSERT_UNDER_CONSTEXPR_CONTEXT(storage <= indexStorageForLastKeyword);
        return Keywords::visitKeywordAtOffset(storage - indexStorageForFirstKeyword, std::forward<F>(f));
    }

    // MARK: Predicates

    constexpr bool isRaw() const
    {
        return storage >= indexStorageForFirstRaw && storage <= indexStorageForLastRaw;
    }

    constexpr bool isCalc() const
    {
        return storage == indexStorageForCalc;
    }

    constexpr bool isKeyword(ValidKeywordForList<Keywords> auto keyword) const
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

// MARK: - Payload

union PrimitiveDataPayload {
    double number;
    CSSCalcValue* calc;

    PrimitiveDataPayload(double number)
        : number { number }
    {
    }

    PrimitiveDataPayload(CSSCalcValue* calc)
        : calc { calc }
    {
    }
};

// MARK: - PrimitiveData

template<Numeric N, PrimitiveKeyword... Ks> struct PrimitiveData {
    using Index = PrimitiveDataIndex<N, Ks...>;
    using Payload = PrimitiveDataPayload;

    using Keywords = typename Index::Keywords;
    using Raw = typename N::Raw;
    using Calc = typename N::Calc;
    using UnitType = typename N::UnitType;
    using UnitTraits = typename N::UnitTraits;

    PrimitiveData(Raw raw)
        : payload { raw.value }
        , index { raw }
    {
    }

    PrimitiveData(Calc calc)
        : payload { &calc.protectedCalc().leakRef() }
        , index { calc }
    {
    }

    PrimitiveData(ValidKeywordForList<Keywords> auto keyword)
        : payload { 0.0 }
        , index { keyword }
    {
    }

    PrimitiveData(PrimitiveDataEmptyToken token)
        : payload { 0.0 }
        , index { token }
    {
    }

    PrimitiveData(const PrimitiveData& other)
        : payload { other.payload }
        , index { other.index }
    {
        if (isCalc())
            SUPPRESS_UNCOUNTED_ARG unevaluatedCalcRef(payload.calc);
    }

    PrimitiveData(PrimitiveData&& other)
        : payload { other.payload }
        , index { other.index }
    {
        other.setAsMovedFrom();
    }

    PrimitiveData& operator=(const PrimitiveData& other)
    {
        if (isCalc())
            SUPPRESS_UNCOUNTED_ARG unevaluatedCalcDeref(payload.calc);
        if (other.isCalc())
            SUPPRESS_UNCOUNTED_ARG unevaluatedCalcRef(other.payload.calc);

        index = other.index;
        payload = other.payload;

        return *this;
    }

    PrimitiveData& operator=(PrimitiveData&& other)
    {
        if (isCalc())
            SUPPRESS_UNCOUNTED_ARG unevaluatedCalcDeref(payload.calc);

        index = other.index;
        payload = other.payload;

        other.setAsMovedFrom();

        return *this;
    }

    // MARK: Constructor/Assignment for NumericType-only PrimitiveData
    // Allows PrimitiveNumeric<T> to be efficiently assigned to PrimitiveNumericOrKeyword<T, Ks...>.

    template<SubsumesChildPrimitiveData<PrimitiveData> T>
    PrimitiveData(const T& other)
        : payload { other.payload }
        , index { other.index }
    {
        if (other.isCalc())
            SUPPRESS_UNCOUNTED_ARG unevaluatedCalcRef(other.payload.calc);
    }

    template<SubsumesChildPrimitiveData<PrimitiveData> T>
    PrimitiveData(T&& other)
        : payload { other.payload }
        , index { other.index }
    {
        other.setAsMovedFrom();
    }

    template<SubsumesChildPrimitiveData<PrimitiveData> T>
    PrimitiveData& operator=(const T& other)
    {
        if (isCalc())
            SUPPRESS_UNCOUNTED_ARG unevaluatedCalcDeref(payload.calc);
        if (other.isCalc())
            SUPPRESS_UNCOUNTED_ARG unevaluatedCalcRef(other.payload.calc);

        index = other.index;
        payload = other.payload;

        return *this;
    }

    template<SubsumesChildPrimitiveData<PrimitiveData> T>
    PrimitiveData& operator=(T&& other)
    {
        if (isCalc())
            SUPPRESS_UNCOUNTED_ARG unevaluatedCalcDeref(payload.calc);

        index = other.index;
        payload = other.payload;

        other.setAsMovedFrom();

        return *this;
    }

    ~PrimitiveData()
    {
        if (isCalc())
            SUPPRESS_UNCOUNTED_ARG unevaluatedCalcDeref(payload.calc);
    }

    bool operator==(const PrimitiveData& other) const
    {
        if (index != other.index)
            return false;

        if (isCalc())
            return asCalc() == other.asCalc();
        return payload.number == other.payload.number;
    }

    bool operator==(ValidKeywordForList<Keywords> auto other) const
    {
        return index == Index(other);
    }

    bool operator==(const Raw& raw) const
    {
        if (index != Index(raw))
            return false;

        ASSERT(isRaw());
        return payload.number == raw.value;
    }

    bool operator==(const Calc& calc) const
    {
        if (!isCalc())
            return false;
        return asCalc() == calc;
    }

    template<typename T>
        requires NumericRaw<T> && NestedUnitEnumOf<typename T::UnitType, UnitType>
    constexpr bool operator==(const T& raw) const
    {
        if (index != Index(unitUpcast<UnitType>(raw.unit)))
            return false;

        ASSERT(isRaw());
        return payload.number == raw.value;
    }

    template<UnitType unitValue>
    bool operator==(const ValueLiteral<unitValue>& literal) const
    {
        if (index != Index(literal.unit))
            return false;

        ASSERT(isRaw());
        return payload.number == literal.value;
    }

    template<NestedUnitEnumOf<UnitType> E, E unitValue>
    bool operator==(const ValueLiteral<unitValue>& literal) const
    {
        if (index != Index(unitUpcast<UnitType>(literal.unit)))
            return false;

        ASSERT(isRaw());
        return payload.number == literal.value;
    }

    // MARK: Conditional Accessors

    std::optional<Raw> raw() const
    {
        if (isRaw())
            return asRaw();
        return std::nullopt;
    }

    std::optional<Calc> calc() const
    {
        if (isCalc())
            return asCalc();
        return std::nullopt;
    }

    // MARK: Accessors

    Raw asRaw() const
    {
        ASSERT(isRaw());
        return Raw { index.unit(), payload.number };
    }

    Calc asCalc() const
    {
        ASSERT(isCalc());
        return Calc { *payload.calc };
    }

    constexpr bool isRaw() const { return index.isRaw(); }
    constexpr bool isCalc() const { return index.isCalc(); }
    constexpr bool isKeyword(ValidKeywordForList<Keywords> auto keyword) const { return index.isKeyword(keyword); }
    constexpr bool isEmpty() const { return index.isEmpty(); }
    constexpr bool isMovedFrom() const { return index.isMovedFrom(); }

    template<typename T> bool holdsAlternative() const
    {
        if constexpr (std::same_as<T, Calc>)
            return index.isCalc();
        else if constexpr (std::same_as<T, Raw>)
            return index.isRaw();
        else if constexpr (ValidKeywordForList<T, Keywords>)
            return index.isKeyword(T { });
    }

    template<typename F> decltype(auto) visit(F&& f) const
    {
        if (isRaw())
            return f(asRaw());
        if (isCalc())
            return f(asCalc());
        return index.visitKeyword(std::forward<F>(f));
    }

    void setAsMovedFrom()
    {
        index.setAsMovedFrom();
        payload.number = 0;
    }

    Payload payload;
    Index index;
};

} // namespace CSS
} // namespace WebCore
