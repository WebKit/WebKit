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
#include "FloatConversion.h"
#include "StyleBuilderState.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace CSS {

bool unevaluatedCalcEqual(const Ref<CSSCalcValue>& a, const Ref<CSSCalcValue>& b)
{
    return a->equals(b.get());
}

bool unevaluatedCalcRequiresConversionData(const Ref<CSSCalcValue>& calc)
{
    return calc->requiresConversionData();
}

void unevaluatedCalcSerialization(StringBuilder& builder, const Ref<CSSCalcValue>& calc)
{
    builder.append(calc->customCSSText());
}

void unevaluatedCalcCollectComputedStyleDependencies(ComputedStyleDependencies& dependencies, const Ref<CSSCalcValue>& calc)
{
    calc->collectComputedStyleDependencies(dependencies);
}

Ref<CSSCalcValue> unevaluatedCalcSimplify(const Ref<CSSCalcValue>& calc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return calc->copySimplified(conversionData, symbolTable);
}

float unevaluatedCalcEvaluate(const Ref<CSSCalcValue>& calc, const Style::BuilderState& state, Calculation::Category category)
{
    return unevaluatedCalcEvaluate(calc, state.cssToLengthConversionData(), { }, category);
}

float unevaluatedCalcEvaluate(const Ref<CSSCalcValue>& calc, const Style::BuilderState& state, const CSSCalcSymbolTable& symbolTable, Calculation::Category category)
{
    return unevaluatedCalcEvaluate(calc, state.cssToLengthConversionData(), symbolTable, category);
}

float unevaluatedCalcEvaluate(const Ref<CSSCalcValue>& calc, const CSSToLengthConversionData& conversionData, Calculation::Category category)
{
    return unevaluatedCalcEvaluate(calc, conversionData, { }, category);
}

float unevaluatedCalcEvaluate(const Ref<CSSCalcValue>& calc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable, Calculation::Category category)
{
    ASSERT_UNUSED(category, calc->category() == category);
    return narrowPrecisionToFloat(calc->doubleValue(conversionData, symbolTable));
}

float unevaluatedCalcEvaluateNoConversionDataRequired(const Ref<CSSCalcValue>& calc, Calculation::Category category)
{
    return unevaluatedCalcEvaluateNoConversionDataRequired(calc, { }, category);
}

float unevaluatedCalcEvaluateNoConversionDataRequired(const Ref<CSSCalcValue>& calc, const CSSCalcSymbolTable& symbolTable, Calculation::Category category)
{
    ASSERT_UNUSED(category, calc->category() == category);
    return narrowPrecisionToFloat(calc->doubleValueNoConversionDataRequired(symbolTable));
}

} // namespace CSS
} // namespace WebCore
