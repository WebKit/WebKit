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

#include "CSSPrimitiveDataIndex.h"
#include "CSSPrimitiveDataPayload.h"
#include "CSSPrimitiveKeywords.h"

namespace WebCore {
namespace CSS {

// `PrimitiveData` is a bespoke implementation of `std::variant<Numeric, Ks...>`
// optimized for memory use by allowing numeric types with multiple unit representations
// (e.g. <length>, <angle>, etc.) to utilize multiple indices for a single smaller payload.
//
// FIXME: Generalize this concept to support arbitrary types through traits.

// MARK: - PrimitiveData

template<Numeric N, PrimitiveKeyword... Ks> struct PrimitiveData {
    using Index = PrimitiveDataIndex<N, Ks...>;
    using Payload = PrimitiveDataPayload;

    using KeywordList = typename Index::KeywordList;
    using Keywords = PrimitiveKeywords<N, Ks...>;

    using Numeric = N;
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

    PrimitiveData(ValidKeywordForList<KeywordList> auto keyword)
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
            payload.calc->ref();
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
            payload.calc->deref();
        if (other.isCalc())
            other.payload.calc->ref();

        index = other.index;
        payload = other.payload;

        return *this;
    }

    PrimitiveData& operator=(PrimitiveData&& other)
    {
        if (isCalc())
            payload.calc->deref();

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
            other.payload.calc->ref();
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
            payload.calc->deref();
        if (other.isCalc())
            other.payload.calc->ref();

        index = other.index;
        payload = other.payload;

        return *this;
    }

    template<SubsumesChildPrimitiveData<PrimitiveData> T>
    PrimitiveData& operator=(T&& other)
    {
        if (isCalc())
            payload.calc->deref();

        index = other.index;
        payload = other.payload;

        other.setAsMovedFrom();

        return *this;
    }

    ~PrimitiveData()
    {
        if (isCalc())
            payload.calc->deref();
    }

    bool operator==(const PrimitiveData& other) const
    {
        if (index != other.index)
            return false;

        if (isCalc())
            return protectedCalc()->equals(other.protectedCalc());
        return payload.number == other.payload.number;
    }

    bool operator==(ValidKeywordForList<KeywordList> auto other) const
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
        return protectedCalc()->equals(calc.protectedCalc());
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

    // MARK: Accessors

    N asSpecified() const
    {
        ASSERT(isSpecified());
        return N { *this };
    }

    Keywords asKeywords() const
    {
        ASSERT(isAnyKeyword());
        return Keywords { index };
    }

    Ref<CSSCalcValue> protectedCalc() const
    {
        ASSERT(isCalc());
        return Ref(*payload.calc);
    }

    Raw asRaw() const
    {
        ASSERT(isRaw());
        return Raw { index.unit(), payload.number };
    }

    Calc asCalc() const
    {
        ASSERT(isCalc());
        return Calc { protectedCalc() };
    }

    // MARK: Conditional Accessors

    std::optional<N> specified() const
    {
        if (isSpecified())
            return asSpecified();
        return std::nullopt;
    }

    std::optional<Keywords> keywords() const
    {
        if (isAnyKeyword())
            return asKeywords();
        return std::nullopt;
    }

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

    constexpr bool isSpecified() const { return index.isSpecified(); }
    constexpr bool isAnyKeyword() const { return index.isAnyKeyword(); }

    constexpr bool isRaw() const { return index.isRaw(); }
    constexpr bool isCalc() const { return index.isCalc(); }
    constexpr bool isKeyword(ValidKeywordForList<KeywordList> auto keyword) const { return index.isKeyword(keyword); }
    constexpr bool isEmpty() const { return index.isEmpty(); }
    constexpr bool isMovedFrom() const { return index.isMovedFrom(); }

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
