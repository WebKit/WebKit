/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StyleZoomPrimitives.h>

namespace WebCore {

namespace CSS {
struct Range;
}

namespace Style {

namespace Calculation {
class Value;
}

class UnevaluatedCalculationBase;

enum class PrimitiveDataKind : uint8_t {
    Default,
    Calculation,
    Empty,
    HashTableEmpty,
    HashTableDeleted
};

enum class PrimitiveDataEvaluationKind : uint8_t {
    Fixed,
    Percentage,
    Calculation,
    Flag
};

struct PrimitiveData {
    PrimitiveData(uint8_t opaqueType);
    PrimitiveData(uint8_t opaqueType, float value, bool hasQuirk = false);
    WEBCORE_EXPORT explicit PrimitiveData(uint8_t opaqueType, UnevaluatedCalculationBase&&);
    WEBCORE_EXPORT explicit PrimitiveData(uint8_t opaqueType, const UnevaluatedCalculationBase&);

    explicit PrimitiveData(WTF::HashTableEmptyValueType);
    explicit PrimitiveData(WTF::HashTableDeletedValueType);

    PrimitiveData(const PrimitiveData&);
    PrimitiveData(PrimitiveData&&);
    PrimitiveData& operator=(const PrimitiveData&);
    PrimitiveData& operator=(PrimitiveData&&);

    ~PrimitiveData();

    bool operator==(const PrimitiveData&) const;

    uint8_t type() const { return m_opaqueType; }
    bool hasQuirk() const { return m_hasQuirk; }

    float value() const { ASSERT(m_kind != PrimitiveDataKind::Calculation); return m_floatValue; }
    Calculation::Value& calculationValue() const;

    bool isKnownZero(PrimitiveDataEvaluationKind) const;
    bool isKnownPositive(PrimitiveDataEvaluationKind) const;
    bool isKnownNegative(PrimitiveDataEvaluationKind) const;

    bool isPossiblyZero(PrimitiveDataEvaluationKind) const;
    bool isPossiblyPositive(PrimitiveDataEvaluationKind) const;
    bool isPossiblyNegative(PrimitiveDataEvaluationKind) const;

    template<auto range, typename ReturnType, typename MaximumType>
    ReturnType minimumValueForPrimitiveDataWithLazyMaximum(PrimitiveDataEvaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomNeeded) const;
    template<auto range, typename ReturnType, typename MaximumType>
    ReturnType minimumValueForPrimitiveDataWithLazyMaximum(PrimitiveDataEvaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomFactor) const;

    template<auto range, typename ReturnType, typename MaximumType>
    ReturnType valueForPrimitiveDataWithLazyMaximum(PrimitiveDataEvaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomNeeded) const;
    template<auto range, typename ReturnType, typename MaximumType>
    ReturnType valueForPrimitiveDataWithLazyMaximum(PrimitiveDataEvaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomFactor) const;

private:
    WEBCORE_EXPORT float nonNanCalculatedValue(CSS::Range, float maxValue, const ZoomFactor& usedZoom) const;
    WEBCORE_EXPORT float nonNanCalculatedValue(CSS::Range, float maxValue, const ZoomNeeded&) const;
    bool isCalculatedEqual(const PrimitiveData&) const;

    void initialize(const PrimitiveData&);
    void initialize(PrimitiveData&&);

    WEBCORE_EXPORT void NODELETE ref() const;
    WEBCORE_EXPORT void deref() const;

    union {
        float m_floatValue { 0.0f };
        unsigned m_calculationValueHandle;
    };
    uint8_t m_opaqueType { 0 };
    PrimitiveDataKind m_kind;
    bool m_hasQuirk { false };
};

inline PrimitiveData::PrimitiveData(uint8_t opaqueType)
    : m_floatValue { 0.0f }
    , m_opaqueType { opaqueType }
    , m_kind { PrimitiveDataKind::Default }
    , m_hasQuirk { false }
{
}

inline PrimitiveData::PrimitiveData(uint8_t opaqueType, float value, bool hasQuirk)
    : m_floatValue { value }
    , m_opaqueType { opaqueType }
    , m_kind { PrimitiveDataKind::Default }
    , m_hasQuirk { hasQuirk }
{
}

inline PrimitiveData::PrimitiveData(WTF::HashTableEmptyValueType)
    : m_kind { PrimitiveDataKind::HashTableEmpty }
{
}

inline PrimitiveData::PrimitiveData(WTF::HashTableDeletedValueType)
    : m_kind { PrimitiveDataKind::HashTableDeleted }
{
}

inline PrimitiveData::PrimitiveData(const PrimitiveData& other)
{
    initialize(other);
}

inline PrimitiveData::PrimitiveData(PrimitiveData&& other)
{
    initialize(WTF::move(other));
}

inline PrimitiveData& PrimitiveData::operator=(const PrimitiveData& other)
{
    if (this == &other)
        return *this;

    if (m_kind == PrimitiveDataKind::Calculation)
        deref();

    initialize(other);
    return *this;
}

inline PrimitiveData& PrimitiveData::operator=(PrimitiveData&& other)
{
    if (this == &other)
        return *this;

    if (m_kind == PrimitiveDataKind::Calculation)
        deref();

    initialize(WTF::move(other));
    return *this;
}

inline void PrimitiveData::initialize(const PrimitiveData& other)
{
    m_opaqueType = other.m_opaqueType;
    m_hasQuirk = other.m_hasQuirk;
    m_kind = other.m_kind;

    switch (m_kind) {
    case PrimitiveDataKind::Calculation:
        m_calculationValueHandle = other.m_calculationValueHandle;
        ref();
        break;
    case PrimitiveDataKind::Default:
    case PrimitiveDataKind::Empty:
    case PrimitiveDataKind::HashTableEmpty:
    case PrimitiveDataKind::HashTableDeleted:
        m_floatValue = other.m_floatValue;
        break;
    }
}

inline void PrimitiveData::initialize(PrimitiveData&& other)
{
    m_opaqueType = other.m_opaqueType;
    m_hasQuirk = other.m_hasQuirk;
    m_kind = other.m_kind;

    switch (m_kind) {
    case PrimitiveDataKind::Calculation:
        m_calculationValueHandle = std::exchange(other.m_calculationValueHandle, 0);
        break;
    case PrimitiveDataKind::Default:
    case PrimitiveDataKind::Empty:
    case PrimitiveDataKind::HashTableEmpty:
    case PrimitiveDataKind::HashTableDeleted:
        m_floatValue = other.m_floatValue;
        break;
    }

    other.m_kind = PrimitiveDataKind::Default;
}

inline PrimitiveData::~PrimitiveData()
{
    if (m_kind == PrimitiveDataKind::Calculation)
        deref();
}

inline bool PrimitiveData::operator==(const PrimitiveData& other) const
{
    if (type() != other.type() || hasQuirk() != other.hasQuirk())
        return false;
    if (m_kind == PrimitiveDataKind::Calculation)
        return isCalculatedEqual(other);
    return value() == other.value();
}

inline bool PrimitiveData::isKnownZero(PrimitiveDataEvaluationKind evaluationKind) const
{
    if (evaluationKind == PrimitiveDataEvaluationKind::Fixed || evaluationKind == PrimitiveDataEvaluationKind::Percentage)
        return !m_floatValue;
    return false;
}

inline bool PrimitiveData::isKnownPositive(PrimitiveDataEvaluationKind evaluationKind) const
{
    if (evaluationKind == PrimitiveDataEvaluationKind::Fixed || evaluationKind == PrimitiveDataEvaluationKind::Percentage)
        return m_floatValue > 0;
    return false;
}

inline bool PrimitiveData::isKnownNegative(PrimitiveDataEvaluationKind evaluationKind) const
{
    if (evaluationKind == PrimitiveDataEvaluationKind::Fixed || evaluationKind == PrimitiveDataEvaluationKind::Percentage)
        return m_floatValue < 0;
    return false;
}

inline bool PrimitiveData::isPossiblyZero(PrimitiveDataEvaluationKind evaluationKind) const
{
    if (evaluationKind == PrimitiveDataEvaluationKind::Fixed || evaluationKind == PrimitiveDataEvaluationKind::Percentage)
        return !m_floatValue;
    return true;
}

inline bool PrimitiveData::isPossiblyPositive(PrimitiveDataEvaluationKind evaluationKind) const
{
    if (evaluationKind == PrimitiveDataEvaluationKind::Fixed || evaluationKind == PrimitiveDataEvaluationKind::Percentage)
        return m_floatValue > 0;
    return true;
}

inline bool PrimitiveData::isPossiblyNegative(PrimitiveDataEvaluationKind evaluationKind) const
{
    if (evaluationKind == PrimitiveDataEvaluationKind::Fixed || evaluationKind == PrimitiveDataEvaluationKind::Percentage)
        return m_floatValue < 0;
    return true;
}

template<auto range, typename ReturnType, typename MaximumType>
ReturnType PrimitiveData::minimumValueForPrimitiveDataWithLazyMaximum(PrimitiveDataEvaluationKind evaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomNeeded token) const
{
    switch (evaluationKind) {
    case PrimitiveDataEvaluationKind::Fixed:
        ASSERT(m_kind == PrimitiveDataKind::Default);
        return ReturnType(m_floatValue);
    case PrimitiveDataEvaluationKind::Percentage:
        ASSERT(m_kind == PrimitiveDataKind::Default);
        return ReturnType(static_cast<float>(lazyMaximumValueFunctor() * m_floatValue / 100.0f));
    case PrimitiveDataEvaluationKind::Calculation:
        ASSERT(m_kind == PrimitiveDataKind::Calculation);
        return ReturnType(nonNanCalculatedValue(range, lazyMaximumValueFunctor(), token));
    case PrimitiveDataEvaluationKind::Flag:
        ASSERT(m_kind == PrimitiveDataKind::Default);
        return ReturnType(0);
    }
    ASSERT_NOT_REACHED();
    return ReturnType(0);
}

template<auto range, typename ReturnType, typename MaximumType>
ReturnType PrimitiveData::minimumValueForPrimitiveDataWithLazyMaximum(PrimitiveDataEvaluationKind evaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomFactor zoom) const
{
    switch (evaluationKind) {
    case PrimitiveDataEvaluationKind::Fixed:
        ASSERT(m_kind == PrimitiveDataKind::Default);
        return ReturnType(m_floatValue * zoom.value);
    case PrimitiveDataEvaluationKind::Percentage:
        ASSERT(m_kind == PrimitiveDataKind::Default);
        return ReturnType(static_cast<float>(lazyMaximumValueFunctor() * m_floatValue / 100.0f));
    case PrimitiveDataEvaluationKind::Calculation:
        ASSERT(m_kind == PrimitiveDataKind::Calculation);
        return ReturnType(nonNanCalculatedValue(range, lazyMaximumValueFunctor(), zoom));
    case PrimitiveDataEvaluationKind::Flag:
        ASSERT(m_kind == PrimitiveDataKind::Default);
        return ReturnType(0);
    }
    ASSERT_NOT_REACHED();
    return ReturnType(0);
}

template<auto range, typename ReturnType, typename MaximumType>
ReturnType PrimitiveData::valueForPrimitiveDataWithLazyMaximum(PrimitiveDataEvaluationKind evaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomNeeded token) const
{
    switch (evaluationKind) {
    case PrimitiveDataEvaluationKind::Fixed:
        ASSERT(m_kind == PrimitiveDataKind::Default);
        return ReturnType(m_floatValue);
    case PrimitiveDataEvaluationKind::Percentage:
        ASSERT(m_kind == PrimitiveDataKind::Default);
        return ReturnType(static_cast<float>(lazyMaximumValueFunctor() * m_floatValue / 100.0f));
    case PrimitiveDataEvaluationKind::Calculation:
        ASSERT(m_kind == PrimitiveDataKind::Calculation);
        return ReturnType(nonNanCalculatedValue(range, lazyMaximumValueFunctor(), token));
    case PrimitiveDataEvaluationKind::Flag:
        ASSERT(m_kind == PrimitiveDataKind::Default);
        return ReturnType(lazyMaximumValueFunctor());
    }
    ASSERT_NOT_REACHED();
    return ReturnType(0);
}

template<auto range, typename ReturnType, typename MaximumType>
ReturnType PrimitiveData::valueForPrimitiveDataWithLazyMaximum(PrimitiveDataEvaluationKind evaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomFactor zoom) const
{
    switch (evaluationKind) {
    case PrimitiveDataEvaluationKind::Fixed:
        ASSERT(m_kind == PrimitiveDataKind::Default);
        return ReturnType(m_floatValue * zoom.value);
    case PrimitiveDataEvaluationKind::Percentage:
        ASSERT(m_kind == PrimitiveDataKind::Default);
        return ReturnType(static_cast<float>(lazyMaximumValueFunctor() * m_floatValue / 100.0f));
    case PrimitiveDataEvaluationKind::Calculation:
        ASSERT(m_kind == PrimitiveDataKind::Calculation);
        return ReturnType(nonNanCalculatedValue(range, lazyMaximumValueFunctor(), zoom));
    case PrimitiveDataEvaluationKind::Flag:
        ASSERT(m_kind == PrimitiveDataKind::Default);
        return ReturnType(lazyMaximumValueFunctor());
    }
    ASSERT_NOT_REACHED();
    return ReturnType(0);
}

} // namespace Style
} // namespace WebCore
