/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "CSSUnevaluatedCalc.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcSymbolsAllowed.h"
#include "CSSCalcValue.h"
#include "CSSNoConversionDataRequiredToken.h"
#include "CSSPropertyParserOptions.h"
#include "StyleBuilderState.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

void unevaluatedCalcRef(CSSCalcValue* calc)
{
    calc->ref();
}

void unevaluatedCalcDeref(CSSCalcValue* calc)
{
    calc->deref();
}

UnevaluatedCalcBase::UnevaluatedCalcBase(CSSCalcValue& value)
    : calc { value }
{
}

UnevaluatedCalcBase::UnevaluatedCalcBase(Ref<CSSCalcValue>&& value)
    : calc { WTFMove(value) }
{
}

UnevaluatedCalcBase::UnevaluatedCalcBase(const UnevaluatedCalcBase&) = default;
UnevaluatedCalcBase::UnevaluatedCalcBase(UnevaluatedCalcBase&&) = default;
UnevaluatedCalcBase& UnevaluatedCalcBase::operator=(const UnevaluatedCalcBase&) = default;
UnevaluatedCalcBase& UnevaluatedCalcBase::operator=(UnevaluatedCalcBase&&) = default;

UnevaluatedCalcBase::~UnevaluatedCalcBase() = default;

Ref<CSSCalcValue> UnevaluatedCalcBase::protectedCalc() const
{
    return calc;
}

CSSCalcValue& UnevaluatedCalcBase::leakRef()
{
    return calc.leakRef();
}

bool UnevaluatedCalcBase::equal(const UnevaluatedCalcBase& other) const
{
    return protectedCalc()->equals(other.calc.get());
}

bool UnevaluatedCalcBase::requiresConversionData() const
{
    return protectedCalc()->requiresConversionData();
}

void UnevaluatedCalcBase::serializationForCSS(StringBuilder& builder, const CSS::SerializationContext& context) const
{
    builder.append(protectedCalc()->customCSSText(context));
}

void UnevaluatedCalcBase::collectComputedStyleDependencies(ComputedStyleDependencies& dependencies) const
{
    protectedCalc()->collectComputedStyleDependencies(dependencies);
}

IterationStatus UnevaluatedCalcBase::visitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    return func(calc);
}

UnevaluatedCalcBase UnevaluatedCalcBase::simplifyBase(const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    return UnevaluatedCalcBase { protectedCalc()->copySimplified(conversionData, symbolTable) };
}

double UnevaluatedCalcBase::evaluate(Calculation::Category category, const Style::BuilderState& state) const
{
    return evaluate(category, state.cssToLengthConversionData(), { });
}

double UnevaluatedCalcBase::evaluate(Calculation::Category category, const Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable) const
{
    return evaluate(category, state.cssToLengthConversionData(), symbolTable);
}

double UnevaluatedCalcBase::evaluate(Calculation::Category category, const CSSToLengthConversionData& conversionData) const
{
    return evaluate(category, conversionData, { });
}

double UnevaluatedCalcBase::evaluate(Calculation::Category category, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT_UNUSED(category, protectedCalc()->category() == category);
    return protectedCalc()->doubleValue(conversionData, symbolTable);
}

double UnevaluatedCalcBase::evaluate(Calculation::Category category, NoConversionDataRequiredToken token) const
{
    return evaluate(category, token, { });
}

double UnevaluatedCalcBase::evaluate(Calculation::Category category, NoConversionDataRequiredToken token, const CSSCalcSymbolTable& symbolTable) const
{
    ASSERT_UNUSED(category, protectedCalc()->category() == category);
    return protectedCalc()->doubleValue(token, symbolTable);
}

} // namespace CSS
} // namespace WebCore
