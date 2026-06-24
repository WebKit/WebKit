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
#include "StyleUnevaluatedCalculation.h"

#include "StyleCalculationValue.h"
#include "StyleZoomPrimitives.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

UnevaluatedCalculationBase::UnevaluatedCalculationBase(Calculation::Value& value)
    : m_calc { value }
{
}

UnevaluatedCalculationBase::UnevaluatedCalculationBase(Ref<Calculation::Value>&& value)
    : m_calc { WTF::move(value) }
{
}

UnevaluatedCalculationBase::UnevaluatedCalculationBase(Calculation::Child&& root)
    : m_calc { Calculation::Value::create(Calculation::Tree { WTF::move(root) }) }
{
}

UnevaluatedCalculationBase::UnevaluatedCalculationBase(const UnevaluatedCalculationBase&) = default;
UnevaluatedCalculationBase::UnevaluatedCalculationBase(UnevaluatedCalculationBase&&) = default;
UnevaluatedCalculationBase& UnevaluatedCalculationBase::operator=(const UnevaluatedCalculationBase&) = default;
UnevaluatedCalculationBase& UnevaluatedCalculationBase::operator=(UnevaluatedCalculationBase&&) = default;

UnevaluatedCalculationBase::~UnevaluatedCalculationBase() = default;

Ref<Calculation::Value> CLANG_POINTER_CONVERSION Calculation::protect(Calculation::Value& value)
{
    return value;
}

Calculation::Value& UnevaluatedCalculationBase::leakRef()
{
    return m_calc.leakRef();
}

double UnevaluatedCalculationBase::evaluateBase(CSS::Range range, double percentageBasis, const ZoomFactor& zoom) const
{
    return protect(m_calc)->evaluate(range, percentageBasis, zoom);
}

double UnevaluatedCalculationBase::evaluateBase(CSS::Range range, double percentageBasis, const ZoomNeeded& token) const
{
    return protect(m_calc)->evaluate(range, percentageBasis, token);
}

bool UnevaluatedCalculationBase::equal(const UnevaluatedCalculationBase& other) const
{
    return arePointingToEqualData(m_calc, other.m_calc);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const UnevaluatedCalculationBase& value)
{
    return ts << protect(value.calculation()).get();
}

} // namespace Style
} // namespace WebCore
