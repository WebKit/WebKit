// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "SizesAttributeParser.h"

#include "CSSPrimitiveValue.h"
#include "CSSToLengthConversionData.h"
#include "CSSTokenizer.h"
#include "FontCascade.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryParser.h"
#include "MediaQueryParserContext.h"
#include "RenderView.h"
#include "SizesCalcParser.h"
#include "StyleScope.h"

namespace WebCore {

float SizesAttributeParser::computeLength(double value, CSSUnitType type, const Document& document)
{
    auto* renderer = document.renderView();
    if (!renderer)
        return 0;
    auto& style = renderer->style();

    CSSToLengthConversionData conversionData(&style, &style, renderer);
    
    // Because we evaluate "sizes" at parse time (before style has been resolved), the font metrics used for these specific units
    // are not available. The font selector's internal consistency isn't guaranteed just yet, so we can just temporarily clear
    // the pointer to it for the duration of the unit evaluation. This is acceptible because the style always comes from the
    // RenderView, which has its font information hardcoded in resolveForDocument() to be -webkit-standard, whose operations
    // don't require a font selector.
    if (type == CSSUnitType::CSS_EXS || type == CSSUnitType::CSS_CHS) {
        RefPtr<FontSelector> fontSelector = style.fontCascade().fontSelector();
        style.fontCascade().update(nullptr);
        float result = CSSPrimitiveValue::computeNonCalcLengthDouble(conversionData, type, value);
        style.fontCascade().update(fontSelector.get());
        return result;
    }
    
    return clampTo<float>(CSSPrimitiveValue::computeNonCalcLengthDouble(conversionData, type, value));
}
    
SizesAttributeParser::SizesAttributeParser(const String& attribute, const Document& document, MediaQueryDynamicResults* mediaQueryDynamicResults)
    : m_document(document)
    , m_mediaQueryDynamicResults(mediaQueryDynamicResults)
{
    m_isValid = parse(CSSTokenizer(attribute).tokenRange());
}

float SizesAttributeParser::length()
{
    if (m_isValid)
        return effectiveSize();
    return effectiveSizeDefaultValue();
}

bool SizesAttributeParser::calculateLengthInPixels(CSSParserTokenRange range, float& result)
{
    const CSSParserToken& startToken = range.peek();
    CSSParserTokenType type = startToken.type();
    if (type == DimensionToken) {
        if (!CSSPrimitiveValue::isLength(startToken.unitType()))
            return false;
        result = computeLength(startToken.numericValue(), startToken.unitType(), m_document);
        if (result >= 0)
            return true;
    } else if (type == FunctionToken) {
        SizesCalcParser calcParser(range, m_document);
        if (!calcParser.isValid())
            return false;
        result = calcParser.result();
        return true;
    } else if (type == NumberToken && !startToken.numericValue()) {
        result = 0;
        return true;
    }

    return false;
}

bool SizesAttributeParser::mediaConditionMatches(const MediaQuerySet& mediaCondition)
{
    // A Media Condition cannot have a media type other than screen.
    auto* renderer = m_document.renderView();
    if (!renderer)
        return false;
    auto& style = renderer->style();
    return MediaQueryEvaluator { "screen", m_document, &style }.evaluate(mediaCondition, m_mediaQueryDynamicResults);
}

bool SizesAttributeParser::parse(CSSParserTokenRange range)
{
    // Split on a comma token and parse the result tokens as (media-condition, length) pairs
    while (!range.atEnd()) {
        const CSSParserToken* mediaConditionStart = &range.peek();
        // The length is the last component value before the comma which isn't whitespace or a comment
        const CSSParserToken* lengthTokenStart = &range.peek();
        const CSSParserToken* lengthTokenEnd = &range.peek();
        while (!range.atEnd() && range.peek().type() != CommaToken) {
            lengthTokenStart = &range.peek();
            range.consumeComponentValue();
            lengthTokenEnd = &range.peek();
            range.consumeWhitespace();
        }
        range.consume();

        float length;
        if (!calculateLengthInPixels(range.makeSubRange(lengthTokenStart, lengthTokenEnd), length))
            continue;
        RefPtr<MediaQuerySet> mediaCondition = MediaQueryParser::parseMediaCondition(range.makeSubRange(mediaConditionStart, lengthTokenStart), MediaQueryParserContext(m_document));
        if (!mediaCondition || !mediaConditionMatches(*mediaCondition))
            continue;
        m_length = length;
        m_lengthWasSet = true;
        return true;
    }
    return false;
}

float SizesAttributeParser::effectiveSize()
{
    if (m_lengthWasSet)
        return m_length;
    return effectiveSizeDefaultValue();
}

unsigned SizesAttributeParser::effectiveSizeDefaultValue()
{
    auto* renderer = m_document.renderView();
    if (!renderer)
        return 0;
    auto& style = renderer->style();
    return clampTo<float>(CSSPrimitiveValue::computeNonCalcLengthDouble({ &style, &style, renderer }, CSSUnitType::CSS_VW, 100.0));
}

} // namespace WebCore
