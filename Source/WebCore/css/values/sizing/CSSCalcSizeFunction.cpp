/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CSSCalcSizeFunction.h"

#include "CSSCalcTree+ComputedStyleDependencies.h"
#include "CSSCalcTree+Copy.h"
#include "CSSCalcTree+Serialization.h"
#include "CSSPrimitiveNumericRange.h"
#include "CSSSerializationContext.h"
#include "CSSValueKeywords.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

CalcSizeParameters::CalcSizeParameters(CalcSizeBasis&& basis, CalcSizeCalculation&& calculation)
    : basis(WTF::move(basis))
    , calculation(WTF::move(calculation))
{
}

static CalcSizeBasis copyBasis(const CalcSizeBasis& basis)
{
    return WTF::switchOn(basis,
        []<CSSValueID Id>(const Constant<Id>& keyword) { return CalcSizeBasis { keyword }; },
        [](const CalcSizeCalculation& calculation) { return CalcSizeBasis { CSSCalc::copy(calculation) }; },
        [](const UniqueRef<CalcSizeFunction>& nested) { return CalcSizeBasis { makeUniqueRef<CalcSizeFunction>(nested.get()) }; }
    );
}

CalcSizeParameters::CalcSizeParameters(const CalcSizeParameters& other)
    : basis(copyBasis(other.basis))
    , calculation(CSSCalc::copy(other.calculation))
{
}

CalcSizeParameters& CalcSizeParameters::operator=(const CalcSizeParameters& other)
{
    basis = copyBasis(other.basis);
    calculation = CSSCalc::copy(other.calculation);
    return *this;
}

CalcSizeParameters::CalcSizeParameters(CalcSizeParameters&&) = default;
CalcSizeParameters& CalcSizeParameters::operator=(CalcSizeParameters&&) = default;
CalcSizeParameters::~CalcSizeParameters() = default;

CSSValueID CalcSizeParameters::basisKeyword() const
{
    return WTF::switchOn(basis,
        []<CSSValueID Id>(const Constant<Id>&) { return Id; },
        [](const CalcSizeCalculation&) { return CSSValueID::Invalid; },
        [](const UniqueRef<CalcSizeFunction>& nested) { return nested->value.parameters.basisKeyword(); }
    );
}

void CalcSizeParameters::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    WTF::switchOn(basis,
        []<CSSValueID Id>(const Constant<Id>&) { },
        [&](const CalcSizeCalculation& basis) { CSSCalc::collectComputedStyleDependencies(basis, dependencies); },
        [&](const UniqueRef<CalcSizeFunction>& nested) { nested->value.parameters.collectComputedStyleDependencies(dependencies); }
    );

    CSSCalc::collectComputedStyleDependencies(calculation, dependencies);
}

bool CalcSizeParameters::operator==(const CalcSizeParameters& other) const
{
    return basis == other.basis && calculation == other.calculation;
}

static void serializeCalcSum(StringBuilder& builder, const SerializationContext& context, const CalcSizeCalculation& calculation)
{
    CSSCalc::serializationForCSSAsFunctionArgument(builder, calculation, { All, context });
}

void Serialize<CalcSizeParameters>::operator()(StringBuilder& builder, const SerializationContext& context, const CalcSizeParameters& value)
{
    WTF::switchOn(value.basis,
        [&]<CSSValueID Id>(const Constant<Id>& keyword) { serializationForCSS(builder, context, keyword); },
        [&](const CalcSizeCalculation& basis) { serializeCalcSum(builder, context, basis); },
        [&](const UniqueRef<CalcSizeFunction>& nested) { serializationForCSS(builder, context, nested.get()); }
    );

    builder.append(", "_s);
    serializeCalcSum(builder, context, value.calculation);
}

} // namespace CSS
} // namespace WebCore
