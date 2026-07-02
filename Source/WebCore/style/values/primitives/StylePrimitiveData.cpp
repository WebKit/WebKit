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

#include "StyleCalculationValue.h"
#include "StyleCalculationValueMap.h"
#include "StyleUnevaluatedCalculation.h"
#include <cmath>

namespace WebCore {
namespace Style {

PrimitiveData::PrimitiveData(uint8_t opaqueType, UnevaluatedCalculationBase&& value)
    : m_opaqueType { opaqueType }
    , m_kind { PrimitiveDataKind::Calculation }
{
    m_calculationValueHandle = Calculation::ValueMap::calculationValues().insert(value.leakRef());
}

PrimitiveData::PrimitiveData(uint8_t opaqueType, const UnevaluatedCalculationBase& value)
    : m_opaqueType { opaqueType }
    , m_kind { PrimitiveDataKind::Calculation }
{
    m_calculationValueHandle = Calculation::ValueMap::calculationValues().insert(value.calculation());
}

Calculation::Value& PrimitiveData::calculationValue() const
{
    ASSERT(m_kind == PrimitiveDataKind::Calculation);
    return Calculation::ValueMap::calculationValues().get(m_calculationValueHandle);
}

void PrimitiveData::ref() const
{
    ASSERT(m_kind == PrimitiveDataKind::Calculation);
    Calculation::ValueMap::calculationValues().ref(m_calculationValueHandle);
}

void PrimitiveData::deref() const
{
    ASSERT(m_kind == PrimitiveDataKind::Calculation);
    Calculation::ValueMap::calculationValues().deref(m_calculationValueHandle);
}

float PrimitiveData::nonNanCalculatedValue(CSS::Range range, float maxValue, const ZoomFactor& usedZoom) const
{
    ASSERT(m_kind == PrimitiveDataKind::Calculation);
    float result = protect(calculationValue())->evaluate(range, maxValue, usedZoom);
    if (std::isnan(result))
        return 0;
    return result;
}

float PrimitiveData::nonNanCalculatedValue(CSS::Range range, float maxValue, const ZoomNeeded& token) const
{
    ASSERT(m_kind == PrimitiveDataKind::Calculation);
    float result = protect(calculationValue())->evaluate(range, maxValue, token);
    if (std::isnan(result))
        return 0;
    return result;
}

bool PrimitiveData::isCalculatedEqual(const PrimitiveData& other) const
{
    return calculationValue() == other.calculationValue();
}

} // namespace Style
} // namespace WebCore
