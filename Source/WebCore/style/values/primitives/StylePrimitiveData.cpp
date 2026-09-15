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

#include "config.h"
#include "StylePrimitiveData.h"

#include "StyleCalcSizeValue+Evaluation.h"
#include "StyleCalcSizeValue.h"
#include "StyleCalculationValue.h"
#include "StyleUnevaluatedCalcSize.h"
#include "StyleUnevaluatedCalculation.h"
#include "StyleValueHandleMap.h"
#include <cmath>

namespace WebCore {
namespace Style {

PrimitiveData::PrimitiveData(uint8_t opaqueType, UnevaluatedCalculationBase&& value)
    : m_opaqueType { opaqueType }
    , m_kind { PrimitiveDataKind::Calculation }
{
    m_calculationValueHandle = ValueHandleMap<Calculation::Value>::singleton().insert(value.leakRef());
}

PrimitiveData::PrimitiveData(uint8_t opaqueType, const UnevaluatedCalculationBase& value)
    : m_opaqueType { opaqueType }
    , m_kind { PrimitiveDataKind::Calculation }
{
    m_calculationValueHandle = ValueHandleMap<Calculation::Value>::singleton().insert(value.calculation());
}

PrimitiveData::PrimitiveData(uint8_t opaqueType, UnevaluatedCalcSize&& value)
    : m_opaqueType { opaqueType }
    , m_kind { PrimitiveDataKind::CalcSize }
{
    m_calculationValueHandle = ValueHandleMap<CalcSizeValue>::singleton().insert(value.leakRef());
}

PrimitiveData::PrimitiveData(uint8_t opaqueType, const UnevaluatedCalcSize& value)
    : m_opaqueType { opaqueType }
    , m_kind { PrimitiveDataKind::CalcSize }
{
    m_calculationValueHandle = ValueHandleMap<CalcSizeValue>::singleton().insert(protect(value.calcSize()));
}

Calculation::Value& PrimitiveData::calculationValue() const
{
    ASSERT(m_kind == PrimitiveDataKind::Calculation);
    return ValueHandleMap<Calculation::Value>::singleton().get(m_calculationValueHandle);
}

CalcSizeValue& PrimitiveData::calcSizeValue() const
{
    ASSERT(m_kind == PrimitiveDataKind::CalcSize);
    return ValueHandleMap<CalcSizeValue>::singleton().get(m_calculationValueHandle);
}

void PrimitiveData::ref() const
{
    ASSERT(usesHandle());
    if (m_kind == PrimitiveDataKind::CalcSize) {
        ValueHandleMap<CalcSizeValue>::singleton().ref(m_calculationValueHandle);
        return;
    }
    ValueHandleMap<Calculation::Value>::singleton().ref(m_calculationValueHandle);
}

void PrimitiveData::deref() const
{
    ASSERT(usesHandle());
    if (m_kind == PrimitiveDataKind::CalcSize) {
        ValueHandleMap<CalcSizeValue>::singleton().deref(m_calculationValueHandle);
        return;
    }
    ValueHandleMap<Calculation::Value>::singleton().deref(m_calculationValueHandle);
}

float PrimitiveData::nonNanCalculatedValue(CSS::Range range, float maxValue, const ZoomFactor& usedZoom) const
{
    ASSERT(m_kind == PrimitiveDataKind::Calculation);
    float result = protect(calculationValue())->evaluate(range, maxValue, usedZoom);
    if (std::isnan(result))
        return 0;
    return result;
}

double PrimitiveData::nonNanCalcSizeValue(CSS::Range range, double maxValue, const ZoomFactor& usedZoom) const
{
    ASSERT(m_kind == PrimitiveDataKind::CalcSize);
    return evaluateCalcSize(protect(calcSizeValue()), range, maxValue, usedZoom);
}

bool PrimitiveData::isCalculatedEqual(const PrimitiveData& other) const
{
    return calculationValue() == other.calculationValue();
}

bool PrimitiveData::isCalcSizeEqual(const PrimitiveData& other) const
{
    return calcSizeValue() == other.calcSizeValue();
}

} // namespace Style
} // namespace WebCore
