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
#include "StyleUnevaluatedCalculation.h"

#include "CalculationValue.h"

namespace WebCore {
namespace Style {

UnevaluatedCalculationBase::UnevaluatedCalculationBase(CalculationValue& value)
    : calc { value }
{
}

UnevaluatedCalculationBase::UnevaluatedCalculationBase(Ref<CalculationValue>&& value)
    : calc { WTFMove(value) }
{
}

UnevaluatedCalculationBase::UnevaluatedCalculationBase(Calculation::Child&& root, Calculation::Category category, CSS::Range range)
    : calc {
        CalculationValue::create(
            category,
            Calculation::Range { range.min, range.max },
            Calculation::Tree { WTFMove(root) }
        )
    }
{
}

UnevaluatedCalculationBase::UnevaluatedCalculationBase(const UnevaluatedCalculationBase&) = default;
UnevaluatedCalculationBase::UnevaluatedCalculationBase(UnevaluatedCalculationBase&&) = default;
UnevaluatedCalculationBase& UnevaluatedCalculationBase::operator=(const UnevaluatedCalculationBase&) = default;
UnevaluatedCalculationBase& UnevaluatedCalculationBase::operator=(UnevaluatedCalculationBase&&) = default;

UnevaluatedCalculationBase::~UnevaluatedCalculationBase() = default;

Ref<CalculationValue> UnevaluatedCalculationBase::protectedCalculation() const
{
    return calc;
}

bool UnevaluatedCalculationBase::equal(const UnevaluatedCalculationBase& other) const
{
    return arePointingToEqualData(calc, other.calc);
}

} // namespace Style
} // namespace WebCore
