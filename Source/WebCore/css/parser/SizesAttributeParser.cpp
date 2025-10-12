// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#include "CSSPrimitiveNumericUnits.h"
#include "CSSToLengthConversionData.h"
#include "CSSTokenizer.h"
#include "FontCascade.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryParser.h"
#include "MediaQueryParserContext.h"
#include "RenderView.h"
#include "RenderStyleInlines.h"
#include "SizesCalcParser.h"
#include "StyleLengthResolution.h"
#include "StyleScope.h"

namespace WebCore {

float SizesAttributeParser::computeLength(double value, CSS::LengthUnit unit, const Document& document)
{
    auto* renderer = document.renderView();
    if (!renderer)
        return 0;
    auto& style = renderer->style();

    CSSToLengthConversionData conversionData(style, &style, renderer->parentStyle(), renderer);
    // Because we evaluate "sizes" at parse time (before style has been resolved), the font metrics used for these specific units
    // are not available. The font selector's internal consistency isn't guaranteed just yet, so we can just temporarily clear
    // the pointer to it for the duration of the unit evaluation. This is acceptable because the style always comes from the
    // RenderView, which has its font information hardcoded in resolveForDocument() to be -webkit-standard, whose operations
    // don't require a font selector.
    if (unit == CSS::LengthUnit::Ex || unit == CSS::LengthUnit::Cap || unit == CSS::LengthUnit::Ch || unit == CSS::LengthUnit::Ic) {
        RefPtr<FontSelector> fontSelector = style.fontCascade().fontSelector();
        style.fontCascade().update(nullptr);
        float result = clampTo<float>(Style::computeNonCalcLengthDouble(value, unit, conversionData));
        style.fontCascade().update(fontSelector.get());
        return result;
    }
    return clampTo<float>(Style::computeNonCalcLengthDouble(value, unit, conversionData));
}
    
SizesAttributeParser::SizesAttributeParser(const String& attribute, const Document& document)
    : m_document(document)
{
    m_isValid = parse(CSSTokenizer(attribute).tokenRange(), CSSParserContext(document));
}

float SizesAttributeParser::length()
{
    if (m_isValid)
        return effectiveSize();
    return effectiveSizeDefaultValue();
}

std::optional<float> SizesAttributeParser::calculateLengthInPixels(CSSParserTokenRange range)
{
    const CSSParserToken& startToken = range.peek();
    CSSParserTokenType type = startToken.type();
    if (type == DimensionToken) {
        auto lengthUnit = CSS::toLengthUnit(startToken.unitType());
        if (!lengthUnit)
            return std::nullopt;
        float result = computeLength(startToken.numericValue(), *lengthUnit, protectedDocument());
        if (result >= 0)
            return result;
    } else if (type == FunctionToken) {
        SizesCalcParser calcParser(range, protectedDocument());
        if (!calcParser.isValid())
            return std::nullopt;
        return calcParser.result();
    } else if (type == NumberToken && !startToken.numericValue())
        return 0;

    return std::nullopt;
}

bool SizesAttributeParser::mediaConditionMatches(const MQ::MediaQuery& mediaCondition)
{
    // A Media Condition cannot have a media type other than screen.
    Ref document = m_document.get();
    CheckedPtr renderer = document->renderView();
    if (!renderer)
        return false;
    auto& style = renderer->style();
    return MQ::MediaQueryEvaluator { screenAtom(), document, &style }.evaluate(mediaCondition);
}

bool SizesAttributeParser::parse(CSSParserTokenRange range, const CSSParserContext& context)
{
    // Split on a comma token and parse the result tokens as (media-condition, length) pairs
    while (!range.atEnd()) {
        auto mediaConditionStart = range;
        // The length is the last component value before the comma which isn't whitespace or a comment
        auto lengthTokenStart = range;
        auto lengthTokenEnd = range;
        while (!range.atEnd() && range.peek().type() != CommaToken) {
            lengthTokenStart = range;
            range.consumeComponentValue();
            lengthTokenEnd = range;
            range.consumeWhitespace();
        }
        range.consume();

        auto length = calculateLengthInPixels(lengthTokenStart.rangeUntil(lengthTokenEnd));
        if (!length)
            continue;
        auto mediaCondition = MQ::MediaQueryParser::parseCondition(mediaConditionStart.rangeUntil(lengthTokenStart), context);
        if (!mediaCondition)
            continue;
        bool matches = mediaConditionMatches(*mediaCondition);
        MQ::MediaQueryEvaluator evaluator { screenAtom() };
        if (!evaluator.collectDynamicDependencies(*mediaCondition).isEmpty())
            m_dynamicMediaQueryResults.append({ MQ::MediaQueryList { *mediaCondition }, matches });
        if (!matches)
            continue;
        m_length = *length;
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
    auto* renderer = protectedDocument()->renderView();
    if (!renderer)
        return 0;
    auto& style = renderer->style();
    return clampTo<float>(Style::computeNonCalcLengthDouble(100.0, CSS::LengthUnit::Vw, { style, &style, renderer->parentStyle(), renderer }));
}

Ref<const Document> SizesAttributeParser::protectedDocument() const
{
    return m_document.get();
}

} // namespace WebCore
