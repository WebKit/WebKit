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
#include "CSSCalcParser.h"

#include "CSSCalcSymbolsAllowed.h"
#include "CSSCalcValue.h"
#include "CSSParserMode.h"
#include "Logging.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

CalcParser::CalcParser(CSSParserTokenRange& range, CalculationCategory destinationCategory, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
    : m_sourceRange(range)
    , m_range(range)
{
    auto functionId = range.peek().functionId();
    if (CSSCalcValue::isCalcFunction(functionId))
        m_value = CSSCalcValue::create(functionId, consumeFunction(m_range), destinationCategory, options.valueRange, WTFMove(symbolsAllowed), options.negativePercentage == NegativePercentagePolicy::Allow);
}

RefPtr<CSSPrimitiveValue> CalcParser::consumeValue()
{
    if (!m_value)
        return nullptr;
    m_sourceRange = m_range;
    return CSSPrimitiveValue::create(m_value.releaseNonNull());
}

RefPtr<CSSPrimitiveValue> CalcParser::consumeValueIfCategory(CalculationCategory category)
{
    if (m_value && m_value->category() != category) {
        LOG_WITH_STREAM(Calc, stream << "CalcParser::consumeValueIfCategory - failing because calc category " << m_value->category() << " does not match requested category " << category);
        return nullptr;
    }
    return consumeValue();
}

bool canConsumeCalcValue(CalculationCategory category, CSSPropertyParserOptions options)
{
    if (category == CalculationCategory::Length || category == CalculationCategory::Percent || category == CalculationCategory::PercentLength)
        return true;

    if (options.parserMode != SVGAttributeMode)
        return false;

    if (category == CalculationCategory::Number || category == CalculationCategory::PercentNumber)
        return true;

    return false;
}

RefPtr<CSSCalcValue> consumeCalcRawWithKnownTokenTypeFunction(CSSParserTokenRange& range, CalculationCategory category, CSSCalcSymbolsAllowed symbolsAllowed, CSSPropertyParserOptions options)
{
    ASSERT(range.peek().type() == FunctionToken);

    auto functionId = range.peek().functionId();
    if (!CSSCalcValue::isCalcFunction(functionId))
        return nullptr;

    RefPtr calcValue = CSSCalcValue::create(functionId, consumeFunction(range), category, options.valueRange, WTFMove(symbolsAllowed));
    if (calcValue && calcValue->category() == category)
        return calcValue;

    return nullptr;
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
