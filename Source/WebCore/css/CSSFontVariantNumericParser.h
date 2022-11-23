/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "CSSParserTokenRange.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"

namespace WebCore {

class CSSFontVariantNumericParser {
public:
    enum class ParseResult : uint8_t { ConsumedValue, DisallowedValue, UnknownValue };

    CSSFontVariantNumericParser() = default;

    ParseResult consumeNumeric(CSSParserTokenRange& range)
    {
        CSSValueID valueID = range.peek().id();
        switch (valueID) {
        case CSSValueLiningNums:
        case CSSValueOldstyleNums:
            if (m_sawNumericFigureValue)
                return ParseResult::DisallowedValue;
            m_sawNumericFigureValue = true;
            break;
        case CSSValueProportionalNums:
        case CSSValueTabularNums:
            if (m_sawNumericSpacingValue)
                return ParseResult::DisallowedValue;
            m_sawNumericSpacingValue = true;
            break;
        case CSSValueDiagonalFractions:
        case CSSValueStackedFractions:
            if (m_sawNumericFractionValue)
                return ParseResult::DisallowedValue;
            m_sawNumericFractionValue = true;
            break;
        case CSSValueOrdinal:
            if (m_sawOrdinalValue)
                return ParseResult::DisallowedValue;
            m_sawOrdinalValue = true;
            break;
        case CSSValueSlashedZero:
            if (m_sawSlashedZeroValue)
                return ParseResult::DisallowedValue;
            m_sawSlashedZeroValue = true;
            break;
        default:
            return ParseResult::UnknownValue;
        }
        m_result->append(CSSPropertyParserHelpers::consumeIdent(range).releaseNonNull());
        return ParseResult::ConsumedValue;
    }

    RefPtr<CSSValue> finalizeValue()
    {
        if (!m_result->length())
            return CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);
        return WTFMove(m_result);
    }

private:
    bool m_sawNumericFigureValue = false;
    bool m_sawNumericSpacingValue = false;
    bool m_sawNumericFractionValue = false;
    bool m_sawOrdinalValue = false;
    bool m_sawSlashedZeroValue = false;
    RefPtr<CSSValueList> m_result = CSSValueList::createSpaceSeparated();
};

} // namespace WebCore
