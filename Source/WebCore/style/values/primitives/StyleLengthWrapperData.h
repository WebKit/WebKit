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

#include <WebCore/CalculationValue.h>
#include <WebCore/StyleZoomPrimitives.h>

namespace WebCore {
namespace Style {

enum class LengthWrapperDataKind : uint8_t {
    Default,
    Calculation,
    Empty,
    HashTableEmpty,
    HashTableDeleted
};

enum class LengthWrapperDataEvaluationKind : uint8_t {
    Fixed,
    Percentage,
    Calculation,
    Flag
};

struct LengthWrapperData {
    LengthWrapperData(uint8_t opaqueType);
    LengthWrapperData(uint8_t opaqueType, float value, bool hasQuirk = false);
    WEBCORE_EXPORT explicit LengthWrapperData(uint8_t opaqueType, Ref<CalculationValue>&&);

    explicit LengthWrapperData(WTF::HashTableEmptyValueType);
    explicit LengthWrapperData(WTF::HashTableDeletedValueType);

    LengthWrapperData(const LengthWrapperData&);
    LengthWrapperData(LengthWrapperData&&);
    LengthWrapperData& operator=(const LengthWrapperData&);
    LengthWrapperData& operator=(LengthWrapperData&&);

    ~LengthWrapperData();

    bool operator==(const LengthWrapperData&) const;

    uint8_t type() const { return m_opaqueType; }
    bool hasQuirk() const { return m_hasQuirk; }

    float value() const { ASSERT(m_kind != LengthWrapperDataKind::Calculation); return m_floatValue; }
    CalculationValue& calculationValue() const;
    Ref<CalculationValue> protectedCalculationValue() const;

    struct IPCData {
        float value;
        uint8_t opaqueType;
        bool hasQuirk;
    };
    WEBCORE_EXPORT LengthWrapperData(IPCData&&);
    WEBCORE_EXPORT IPCData ipcData() const;

    bool isKnownZero(LengthWrapperDataEvaluationKind) const;
    bool isKnownPositive(LengthWrapperDataEvaluationKind) const;
    bool isKnownNegative(LengthWrapperDataEvaluationKind) const;

    bool isPossiblyZero(LengthWrapperDataEvaluationKind) const;
    bool isPossiblyPositive(LengthWrapperDataEvaluationKind) const;
    bool isPossiblyNegative(LengthWrapperDataEvaluationKind) const;

    template<typename ReturnType, typename MaximumType>
    ReturnType minimumValueForLengthWrapperDataWithLazyMaximum(LengthWrapperDataEvaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomNeeded) const;
    template<typename ReturnType, typename MaximumType>
    ReturnType minimumValueForLengthWrapperDataWithLazyMaximum(LengthWrapperDataEvaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomFactor) const;

    template<typename ReturnType, typename MaximumType>
    ReturnType valueForLengthWrapperDataWithLazyMaximum(LengthWrapperDataEvaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomNeeded) const;
    template<typename ReturnType, typename MaximumType>
    ReturnType valueForLengthWrapperDataWithLazyMaximum(LengthWrapperDataEvaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomFactor) const;

private:
    WEBCORE_EXPORT float nonNanCalculatedValue(float maxValue) const;
    bool isCalculatedEqual(const LengthWrapperData&) const;

    void initialize(const LengthWrapperData&);
    void initialize(LengthWrapperData&&);

    WEBCORE_EXPORT void ref() const;
    WEBCORE_EXPORT void deref() const;

    union {
        float m_floatValue { 0.0f };
        unsigned m_calculationValueHandle;
    };
    uint8_t m_opaqueType { 0 };
    LengthWrapperDataKind m_kind;
    bool m_hasQuirk { false };
};

inline LengthWrapperData::LengthWrapperData(uint8_t opaqueType)
    : m_floatValue { 0.0f }
    , m_opaqueType { opaqueType }
    , m_kind { LengthWrapperDataKind::Default }
    , m_hasQuirk { false }
{
}

inline LengthWrapperData::LengthWrapperData(uint8_t opaqueType, float value, bool hasQuirk)
    : m_floatValue { value }
    , m_opaqueType { opaqueType }
    , m_kind { LengthWrapperDataKind::Default }
    , m_hasQuirk { hasQuirk }
{
}

inline LengthWrapperData::LengthWrapperData(WTF::HashTableEmptyValueType)
    : m_kind { LengthWrapperDataKind::HashTableEmpty }
{
}

inline LengthWrapperData::LengthWrapperData(WTF::HashTableDeletedValueType)
    : m_kind { LengthWrapperDataKind::HashTableDeleted }
{
}

inline LengthWrapperData::LengthWrapperData(const LengthWrapperData& other)
{
    initialize(other);
}

inline LengthWrapperData::LengthWrapperData(LengthWrapperData&& other)
{
    initialize(WTFMove(other));
}

inline LengthWrapperData& LengthWrapperData::operator=(const LengthWrapperData& other)
{
    if (this == &other)
        return *this;

    if (m_kind == LengthWrapperDataKind::Calculation)
        deref();

    initialize(other);
    return *this;
}

inline LengthWrapperData& LengthWrapperData::operator=(LengthWrapperData&& other)
{
    if (this == &other)
        return *this;

    if (m_kind == LengthWrapperDataKind::Calculation)
        deref();

    initialize(WTFMove(other));
    return *this;
}

inline void LengthWrapperData::initialize(const LengthWrapperData& other)
{
    m_opaqueType = other.m_opaqueType;
    m_hasQuirk = other.m_hasQuirk;
    m_kind = other.m_kind;

    switch (m_kind) {
    case LengthWrapperDataKind::Calculation:
        m_calculationValueHandle = other.m_calculationValueHandle;
        ref();
        break;
    case LengthWrapperDataKind::Default:
    case LengthWrapperDataKind::Empty:
    case LengthWrapperDataKind::HashTableEmpty:
    case LengthWrapperDataKind::HashTableDeleted:
        m_floatValue = other.m_floatValue;
        break;
    }
}

inline void LengthWrapperData::initialize(LengthWrapperData&& other)
{
    m_opaqueType = other.m_opaqueType;
    m_hasQuirk = other.m_hasQuirk;
    m_kind = other.m_kind;

    switch (m_kind) {
    case LengthWrapperDataKind::Calculation:
        m_calculationValueHandle = std::exchange(other.m_calculationValueHandle, 0);
        break;
    case LengthWrapperDataKind::Default:
    case LengthWrapperDataKind::Empty:
    case LengthWrapperDataKind::HashTableEmpty:
    case LengthWrapperDataKind::HashTableDeleted:
        m_floatValue = other.m_floatValue;
        break;
    }

    other.m_kind = LengthWrapperDataKind::Default;
}

inline LengthWrapperData::~LengthWrapperData()
{
    if (m_kind == LengthWrapperDataKind::Calculation)
        deref();
}

inline bool LengthWrapperData::operator==(const LengthWrapperData& other) const
{
    if (type() != other.type() || hasQuirk() != other.hasQuirk())
        return false;
    if (m_kind == LengthWrapperDataKind::Calculation)
        return isCalculatedEqual(other);
    return value() == other.value();
}

inline bool LengthWrapperData::isKnownZero(LengthWrapperDataEvaluationKind evaluationKind) const
{
    if (evaluationKind == LengthWrapperDataEvaluationKind::Fixed || evaluationKind == LengthWrapperDataEvaluationKind::Percentage)
        return !m_floatValue;
    return false;
}

inline bool LengthWrapperData::isKnownPositive(LengthWrapperDataEvaluationKind evaluationKind) const
{
    if (evaluationKind == LengthWrapperDataEvaluationKind::Fixed || evaluationKind == LengthWrapperDataEvaluationKind::Percentage)
        return m_floatValue > 0;
    return false;
}

inline bool LengthWrapperData::isKnownNegative(LengthWrapperDataEvaluationKind evaluationKind) const
{
    if (evaluationKind == LengthWrapperDataEvaluationKind::Fixed || evaluationKind == LengthWrapperDataEvaluationKind::Percentage)
        return m_floatValue < 0;
    return false;
}

inline bool LengthWrapperData::isPossiblyZero(LengthWrapperDataEvaluationKind evaluationKind) const
{
    if (evaluationKind == LengthWrapperDataEvaluationKind::Fixed || evaluationKind == LengthWrapperDataEvaluationKind::Percentage)
        return !m_floatValue;
    return true;
}

inline bool LengthWrapperData::isPossiblyPositive(LengthWrapperDataEvaluationKind evaluationKind) const
{
    if (evaluationKind == LengthWrapperDataEvaluationKind::Fixed || evaluationKind == LengthWrapperDataEvaluationKind::Percentage)
        return m_floatValue > 0;
    return true;
}

inline bool LengthWrapperData::isPossiblyNegative(LengthWrapperDataEvaluationKind evaluationKind) const
{
    if (evaluationKind == LengthWrapperDataEvaluationKind::Fixed || evaluationKind == LengthWrapperDataEvaluationKind::Percentage)
        return m_floatValue < 0;
    return true;
}

template<typename ReturnType, typename MaximumType>
ReturnType LengthWrapperData::minimumValueForLengthWrapperDataWithLazyMaximum(LengthWrapperDataEvaluationKind evaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomNeeded) const
{
    switch (evaluationKind) {
    case LengthWrapperDataEvaluationKind::Fixed:
        ASSERT(m_kind == LengthWrapperDataKind::Default);
        return ReturnType(m_floatValue);
    case LengthWrapperDataEvaluationKind::Percentage:
        ASSERT(m_kind == LengthWrapperDataKind::Default);
        return ReturnType(static_cast<float>(lazyMaximumValueFunctor() * m_floatValue / 100.0f));
    case LengthWrapperDataEvaluationKind::Calculation:
        ASSERT(m_kind == LengthWrapperDataKind::Calculation);
        return ReturnType(nonNanCalculatedValue(lazyMaximumValueFunctor()));
    case LengthWrapperDataEvaluationKind::Flag:
        ASSERT(m_kind == LengthWrapperDataKind::Default);
        return ReturnType(0);
    }
    ASSERT_NOT_REACHED();
    return ReturnType(0);
}

template<typename ReturnType, typename MaximumType>
ReturnType LengthWrapperData::minimumValueForLengthWrapperDataWithLazyMaximum(LengthWrapperDataEvaluationKind evaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomFactor zoom) const
{
    switch (evaluationKind) {
    case LengthWrapperDataEvaluationKind::Fixed:
        ASSERT(m_kind == LengthWrapperDataKind::Default);
        return ReturnType(m_floatValue * zoom.value);
    case LengthWrapperDataEvaluationKind::Percentage:
        ASSERT(m_kind == LengthWrapperDataKind::Default);
        return ReturnType(static_cast<float>(lazyMaximumValueFunctor() * m_floatValue / 100.0f));
    case LengthWrapperDataEvaluationKind::Calculation:
        ASSERT(m_kind == LengthWrapperDataKind::Calculation);
        return ReturnType(nonNanCalculatedValue(lazyMaximumValueFunctor()));
    case LengthWrapperDataEvaluationKind::Flag:
        ASSERT(m_kind == LengthWrapperDataKind::Default);
        return ReturnType(0);
    }
    ASSERT_NOT_REACHED();
    return ReturnType(0);
}

template<typename ReturnType, typename MaximumType>
ReturnType LengthWrapperData::valueForLengthWrapperDataWithLazyMaximum(LengthWrapperDataEvaluationKind evaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomNeeded) const
{
    switch (evaluationKind) {
    case LengthWrapperDataEvaluationKind::Fixed:
        ASSERT(m_kind == LengthWrapperDataKind::Default);
        return ReturnType(m_floatValue);
    case LengthWrapperDataEvaluationKind::Percentage:
        ASSERT(m_kind == LengthWrapperDataKind::Default);
        return ReturnType(static_cast<float>(lazyMaximumValueFunctor() * m_floatValue / 100.0f));
    case LengthWrapperDataEvaluationKind::Calculation:
        ASSERT(m_kind == LengthWrapperDataKind::Calculation);
        return ReturnType(nonNanCalculatedValue(lazyMaximumValueFunctor()));
    case LengthWrapperDataEvaluationKind::Flag:
        ASSERT(m_kind == LengthWrapperDataKind::Default);
        return ReturnType(lazyMaximumValueFunctor());
    }
    ASSERT_NOT_REACHED();
    return ReturnType(0);
}

template<typename ReturnType, typename MaximumType>
ReturnType LengthWrapperData::valueForLengthWrapperDataWithLazyMaximum(LengthWrapperDataEvaluationKind evaluationKind, NOESCAPE const Invocable<MaximumType()> auto& lazyMaximumValueFunctor, ZoomFactor zoom) const
{
    switch (evaluationKind) {
    case LengthWrapperDataEvaluationKind::Fixed:
        ASSERT(m_kind == LengthWrapperDataKind::Default);
        return ReturnType(m_floatValue * zoom.value);
    case LengthWrapperDataEvaluationKind::Percentage:
        ASSERT(m_kind == LengthWrapperDataKind::Default);
        return ReturnType(static_cast<float>(lazyMaximumValueFunctor() * m_floatValue / 100.0f));
    case LengthWrapperDataEvaluationKind::Calculation:
        ASSERT(m_kind == LengthWrapperDataKind::Calculation);
        return ReturnType(nonNanCalculatedValue(lazyMaximumValueFunctor()));
    case LengthWrapperDataEvaluationKind::Flag:
        ASSERT(m_kind == LengthWrapperDataKind::Default);
        return ReturnType(lazyMaximumValueFunctor());
    }
    ASSERT_NOT_REACHED();
    return ReturnType(0);
}

} // namespace Style
} // namespace WebCore
