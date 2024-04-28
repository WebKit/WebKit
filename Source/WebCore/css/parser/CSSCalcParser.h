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

#pragma once

#include "CSSCalcValue.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CalculationCategory.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSCalcSymbolTable;
class CSSPrimitiveValue;

enum CSSParserMode : uint8_t;

enum class CalculationCategory : uint8_t;
enum class ValueRange : uint8_t;

namespace CSSPropertyParserHelpers {

class CalcParser {
public:
    explicit CalcParser(CSSParserTokenRange&, CalculationCategory, ValueRange, const CSSCalcSymbolTable&, NegativePercentagePolicy = NegativePercentagePolicy::Forbid);

    const CSSCalcValue* value() const { return m_value.get(); }

    RefPtr<CSSPrimitiveValue> consumeValue();
    RefPtr<CSSPrimitiveValue> consumeValueIfCategory(CalculationCategory);

private:
    CSSParserTokenRange& m_sourceRange;
    CSSParserTokenRange m_range;
    RefPtr<CSSCalcValue> m_value;
};

bool canConsumeCalcValue(CalculationCategory, CSSParserMode);
RefPtr<CSSCalcValue> consumeCalcRawWithKnownTokenTypeFunction(CSSParserTokenRange&, CalculationCategory, const CSSCalcSymbolTable&, ValueRange);

}
}
