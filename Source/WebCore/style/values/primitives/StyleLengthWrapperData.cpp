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

#include "config.h"
#include "StyleLengthWrapperData.h"

#include "CalculationValue.h"
#include "CalculationValueMap.h"
#include <cmath>

namespace WebCore {
namespace Style {

LengthWrapperData::LengthWrapperData(uint8_t opaqueType, Ref<CalculationValue>&& value)
    : m_opaqueType { opaqueType }
    , m_kind { LengthWrapperDataKind::Calculation }
{
    m_calculationValueHandle = CalculationValueMap::calculationValues().insert(WTFMove(value));
}

CalculationValue& LengthWrapperData::calculationValue() const
{
    ASSERT(m_kind == LengthWrapperDataKind::Calculation);
    return CalculationValueMap::calculationValues().get(m_calculationValueHandle);
}

Ref<CalculationValue> LengthWrapperData::protectedCalculationValue() const
{
    return calculationValue();
}

void LengthWrapperData::ref() const
{
    ASSERT(m_kind == LengthWrapperDataKind::Calculation);
    CalculationValueMap::calculationValues().ref(m_calculationValueHandle);
}

void LengthWrapperData::deref() const
{
    ASSERT(m_kind == LengthWrapperDataKind::Calculation);
    CalculationValueMap::calculationValues().deref(m_calculationValueHandle);
}

LengthWrapperData::LengthWrapperData(IPCData&& data)
    : m_floatValue { data.value }
    , m_opaqueType { data.opaqueType }
    , m_kind { LengthWrapperDataKind::Default }
    , m_hasQuirk { data.hasQuirk }
{
}

auto LengthWrapperData::ipcData() const -> IPCData
{
    ASSERT(m_kind == LengthWrapperDataKind::Default);

    return IPCData {
        .value = value(),
        .opaqueType = m_opaqueType,
        .hasQuirk = m_hasQuirk
    };
}

float LengthWrapperData::nonNanCalculatedValue(float maxValue) const
{
    ASSERT(m_kind == LengthWrapperDataKind::Calculation);
    float result = protectedCalculationValue()->evaluate(maxValue);
    if (std::isnan(result))
        return 0;
    return result;
}

bool LengthWrapperData::isCalculatedEqual(const LengthWrapperData& other) const
{
    return calculationValue() == other.calculationValue();
}

} // namespace Style
} // namespace WebCore
