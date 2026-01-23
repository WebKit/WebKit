// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2025 Apple Inc. All rights reserved.
// Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSCalcTree+Evaluation.h"
#include "CSSCalcTree+Parser.h"
#include "CSSCalcTree+Simplification.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericCategory.h"
#include "CSSPrimitiveNumericRange.h"
#include "CSSPrimitiveNumericUnits.h"
#include "CSSPropertyParserOptions.h"
#include "CSSPropertyParserState.h"
#include "CSSToLengthConversionData.h"
#include "CSSTokenizer.h"
#include "FontCascade.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryParser.h"
#include "MediaQueryParserContext.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderView.h"
#include "StyleLengthResolution.h"
#include "StyleScope.h"
#include <wtf/Scope.h>

namespace WebCore {

SizesAttributeParser::SizesAttributeParser(const String& attribute, const Document& document)
    : m_document(document)
{
    m_result = parse(CSSTokenizer(attribute).tokenRange(), CSSParserContext(document));
}

float SizesAttributeParser::effectiveSize()
{
    if (m_result)
        return *m_result;
    return effectiveSizeDefaultValue();
}

float SizesAttributeParser::effectiveSizeDefaultValue()
{
    auto conversionData = this->conversionData();
    if (!conversionData)
        return 0;
    return CSS::clampToRange<CSS::Nonnegative, float>(Style::computeNonCalcLengthDouble(100.0, CSS::LengthUnit::Vw, *conversionData));
}

std::optional<float> SizesAttributeParser::parse(CSSParserTokenRange tokens, const CSSParserContext& context)
{
    // Split on a comma token and parse the result tokens as (media-condition, length) pairs
    while (!tokens.atEnd()) {
        auto mediaConditionStart = tokens;
        // The length is the last component value before the comma which isn't whitespace or a comment
        auto lengthTokenStart = tokens;
        auto lengthTokenEnd = tokens;
        while (!tokens.atEnd() && tokens.peek().type() != CommaToken) {
            lengthTokenStart = tokens;
            tokens.consumeComponentValue();
            lengthTokenEnd = tokens;
            tokens.consumeWhitespace();
        }
        tokens.consume();

        auto length = parseLength(lengthTokenStart.rangeUntil(lengthTokenEnd), context);
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
        return length;
    }
    return std::nullopt;
}

std::optional<float> SizesAttributeParser::parseDimension(CSSParserTokenRange tokens, const CSSParserContext&)
{
    ASSERT(tokens.peek().type() == DimensionToken);

    auto& token = tokens.consumeIncludingWhitespace();

    auto unit = CSS::toLengthUnit(token.unitType());
    if (!unit)
        return std::nullopt;

    auto conversionData = this->conversionData();
    if (!conversionData)
        return std::nullopt;

    auto value = token.numericValue();

    auto resolve = [&] -> std::optional<float> {
        auto result = CSS::clampToRange<CSS::All, float>(Style::computeNonCalcLengthDouble(value, *unit, *conversionData));
        if (result < 0)
            return std::nullopt;
        return result;
    };

    // Because we evaluate "sizes" at parse time (before style has been resolved), the font metrics used for these specific units
    // are not available. The font selector's internal consistency isn't guaranteed just yet, so we can just temporarily clear
    // the pointer to it for the duration of the unit evaluation. This is acceptable because the style always comes from the
    // RenderView, which has its font information hardcoded in resolveForDocument() to be -webkit-standard, whose operations
    // don't require a font selector.
    if (unit == CSS::LengthUnit::Ex || unit == CSS::LengthUnit::Cap || unit == CSS::LengthUnit::Ch || unit == CSS::LengthUnit::Ic) {
        RefPtr fontSelector = conversionData->style()->fontCascade().fontSelector();
        conversionData->style()->fontCascade().update(nullptr);
        auto resetFontSelectorScope = makeScopeExit([&] { conversionData->style()->fontCascade().update(fontSelector.get()); });

        return resolve();
    }

    return resolve();
}

std::optional<float> SizesAttributeParser::parseFunction(CSSParserTokenRange tokens, const CSSParserContext& context)
{
    // Per https://html.spec.whatwg.org/#sizes-attributes
    //   "A <source-size-value> that is a <length> must not be negative, and must
    //    not use CSS functions other than the math functions."

    static constexpr auto category = CSS::Category::Length;
    static constexpr auto range = CSS::Nonnegative;

    ASSERT(tokens.peek().type() == FunctionToken);

    auto conversionData = this->conversionData();
    if (!conversionData)
        return std::nullopt;

    auto parserState = CSS::PropertyParserState {
        .context = context
    };
    auto parserOptions = CSSCalc::ParserOptions {
        .category = category,
        .range = range,
        .allowedSymbols = { },
        .propertyOptions = { },
    };
    auto simplificationOptions = CSSCalc::SimplificationOptions {
        .category = category,
        .range = range,
        .conversionData = conversionData,
        .symbolTable = { },
        .allowZeroValueLengthRemovalFromSum = true,
    };

    // See `parseDimension` for why this unset/set of the font selector is needed.
    // FIXME: This could be made more efficient if we only did this when actually
    // needed. That could be accomplished via new simplification/evaluation options
    // or by adding delegation for dimension resolution.
    RefPtr fontSelector = conversionData->style()->fontCascade().fontSelector();
    conversionData->style()->fontCascade().update(nullptr);
    auto resetFontSelectorScope = makeScopeExit([&] { conversionData->style()->fontCascade().update(fontSelector.get()); });

    auto tree = CSSCalc::parseAndSimplify(tokens, parserState, parserOptions, simplificationOptions);
    if (!tree)
        return std::nullopt;

    auto evaluationOptions = CSSCalc::EvaluationOptions {
        .category = category,
        .range = range,
        .conversionData = conversionData,
        .symbolTable = { },
    };
    auto result = CSSCalc::evaluateDouble(*tree, evaluationOptions);
    if (!result)
        return std::nullopt;
    return CSS::clampToRange<range, float>(*result);
}

std::optional<float> SizesAttributeParser::parseLength(CSSParserTokenRange tokens, const CSSParserContext& context)
{
    // FIXME: Consider making `MetaConsumer<CSS::Length<CSS::Nonnegative>>` support immediate resolution of calc() and relative lengths via an optional `CSSToLengthConversionData` parameter and using it here.

    switch (tokens.peek().type()) {
    case DimensionToken:
        return parseDimension(tokens, context);
    case FunctionToken:
        return parseFunction(tokens, context);
    case NumberToken:
        if (!tokens.peek().numericValue())
            return 0;
        break;
    default:
        break;
    }

    return std::nullopt;
}

bool SizesAttributeParser::mediaConditionMatches(const MQ::MediaQuery& mediaCondition)
{
    // A Media Condition cannot have a media type other than screen.
    Ref document = m_document.get();
    CheckedPtr renderer = document->renderView();
    if (!renderer)
        return false;
    CheckedRef style = renderer->style();
    return MQ::MediaQueryEvaluator { screenAtom(), document, style.ptr() }.evaluate(mediaCondition);
}

Ref<const Document> SizesAttributeParser::protectedDocument() const
{
    return m_document.get();
}

std::optional<CSSToLengthConversionData> SizesAttributeParser::conversionData() const
{
    CheckedPtr renderer = protectedDocument()->renderView();
    if (!renderer)
        return std::nullopt;
    CheckedRef style = renderer->style();
    return CSSToLengthConversionData { style, style.ptr(), renderer->parentStyle(), renderer.get() };
}

} // namespace WebCore
