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
#include "CSSPropertyParserConsumer+UnevaluatedCalc.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

bool unevaluatedCalcEqual(const Ref<CSSCalcValue>& a, const Ref<CSSCalcValue>& b)
{
    return a->equals(b.get());
}

void unevaluatedCalcSerialization(StringBuilder& builder, const Ref<CSSCalcValue>& calc)
{
    builder.append(calc->customCSSText());
}

bool unevaluatedCalcRequiresConversionData(const Ref<CSSCalcValue>& calc)
{
    return calc->requiresConversionData();
}

Ref<CSSCalcValue> unevaluatedCalcSimplify(const Ref<CSSCalcValue>& calc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return calc->copySimplified(conversionData, symbolTable);
}

AngleRaw evaluateCalc(const UnevaluatedCalc<AngleRaw>& calc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->angleValue(conversionData, symbolTable);
}

AngleRaw evaluateCalcNoConversionDataRequired(const UnevaluatedCalc<AngleRaw>& calc, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->angleValueDeprecated(symbolTable);
}

NumberRaw evaluateCalc(const UnevaluatedCalc<NumberRaw>& calc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->numberValue(conversionData, symbolTable);
}

NumberRaw evaluateCalcNoConversionDataRequired(const UnevaluatedCalc<NumberRaw>& calc, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->numberValueDeprecated(symbolTable);
}

PercentRaw evaluateCalc(const UnevaluatedCalc<PercentRaw>& calc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->percentValue(conversionData, symbolTable);
}

PercentRaw evaluateCalcNoConversionDataRequired(const UnevaluatedCalc<PercentRaw>& calc, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->percentValueDeprecated(symbolTable);
}

LengthRaw evaluateCalc(const UnevaluatedCalc<LengthRaw>& calc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->lengthValue(conversionData, symbolTable);
}

LengthRaw evaluateCalcNoConversionDataRequired(const UnevaluatedCalc<LengthRaw>& calc, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->lengthValueDeprecated(symbolTable);
}

ResolutionRaw evaluateCalc(const UnevaluatedCalc<ResolutionRaw>& calc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->resolutionValue(conversionData, symbolTable);
}

ResolutionRaw evaluateCalcNoConversionDataRequired(const UnevaluatedCalc<ResolutionRaw>& calc, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->resolutionValueDeprecated(symbolTable);
}

TimeRaw evaluateCalc(const UnevaluatedCalc<TimeRaw>& calc, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->timeValue(conversionData, symbolTable);
}

TimeRaw evaluateCalcNoConversionDataRequired(const UnevaluatedCalc<TimeRaw>& calc, const CSSCalcSymbolTable& symbolTable)
{
    return calc.calc->timeValueDeprecated(symbolTable);
}

} // namespace WebCore
