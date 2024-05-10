/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSCalcSymbolsAllowed.h"
#include "CSSValueKeywords.h"
#include "CalcOperator.h"
#include "CalculationCategory.h"
#include <wtf/WeakRef.h>

namespace WebCore {

class CSSCalcExpressionNode;
class CSSParserToken;
class CSSParserTokenRange;

class CSSCalcExpressionNodeParser {
public:
    explicit CSSCalcExpressionNodeParser(CalculationCategory destinationCategory, CSSCalcSymbolsAllowed symbolsAllowed)
        : m_destinationCategory(destinationCategory)
        , m_symbolsAllowed(WTFMove(symbolsAllowed))
    {
    }

    RefPtr<CSSCalcExpressionNode> parseCalc(CSSParserTokenRange, CSSValueID function, bool allowsNegativePercentage);
    
private:
    char operatorValue(const CSSParserToken&);

    bool parseValue(CSSParserTokenRange&, CSSValueID, RefPtr<CSSCalcExpressionNode>&);
    bool parseCalcFunction(CSSParserTokenRange&, CSSValueID, int depth, RefPtr<CSSCalcExpressionNode>&);
    bool parseCalcSum(CSSParserTokenRange&, CSSValueID, int depth, RefPtr<CSSCalcExpressionNode>&);
    bool parseCalcProduct(CSSParserTokenRange&, CSSValueID, int depth, RefPtr<CSSCalcExpressionNode>&);
    bool parseCalcValue(CSSParserTokenRange&, CSSValueID, int depth, RefPtr<CSSCalcExpressionNode>&);

    CalculationCategory m_destinationCategory;
    CSSCalcSymbolsAllowed m_symbolsAllowed;
};

}
