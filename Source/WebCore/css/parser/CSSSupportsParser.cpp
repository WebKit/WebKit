// Copyright 2015 The Chromium Authors. All rights reserved.
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
#include "CSSSupportsParser.h"

#include "CSSParserImpl.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSSelectorParser.h"
#include "FontCustomPlatformData.h"
#include "StyleRule.h"

namespace WebCore {

CSSSupportsParser::SupportsResult CSSSupportsParser::supportsCondition(CSSParserTokenRange range, CSSParserImpl& parser, ParsingMode mode, CSSParserEnum::IsNestedContext isNestedContext)
{
    // FIXME: The spec allows leading whitespace in @supports but not CSS.supports,
    // but major browser vendors allow it in CSS.supports also.
    range.consumeWhitespace();
    CSSSupportsParser supportsParser(parser, isNestedContext);

    auto result = supportsParser.consumeCondition(range);
    if (mode != ParsingMode::AllowBareDeclarationAndGeneralEnclosed || result != Invalid)
        return result;

    // window.CSS.supports requires parsing as-if the condition was wrapped in
    // parenthesis. The only productions that wouldn't have parsed above are the
    // declaration condition or the general enclosed productions.
    return supportsParser.consumeSupportsFeatureOrGeneralEnclosed(range);
}

enum ClauseType { Unresolved, Conjunction, Disjunction };

CSSSupportsParser::SupportsResult CSSSupportsParser::consumeCondition(CSSParserTokenRange range)
{
    if (range.peek().type() == IdentToken) {
        if (equalLettersIgnoringASCIICase(range.peek().value(), "not"_s))
            return consumeNegation(range);
    }

    bool result = false;
    ClauseType clauseType = Unresolved;

    auto previousTokenType = IdentToken;

    while (true) {
        auto nextResult = consumeConditionInParenthesis(range, previousTokenType);
        if (nextResult == Invalid)
            return Invalid;
        bool nextSupported = nextResult;
        if (clauseType == Unresolved)
            result = nextSupported;
        else if (clauseType == Conjunction)
            result &= nextSupported;
        else
            result |= nextSupported;

        if (range.atEnd())
            break;
        range.consumeWhitespace();
        if (range.atEnd())
            break;

        const CSSParserToken& token = range.peek();
        if (token.type() != IdentToken)
            return Invalid;

        previousTokenType = token.type();

        if (clauseType == Unresolved)
            clauseType = token.value().length() == 3 ? Conjunction : Disjunction;
        if ((clauseType == Conjunction && !equalLettersIgnoringASCIICase(token.value(), "and"_s))
            || (clauseType == Disjunction && !equalLettersIgnoringASCIICase(token.value(), "or"_s)))
            return Invalid;

        range.consume();
        if (range.peek().type() != WhitespaceToken)
            return Invalid;
        range.consumeWhitespace();
    }
    return result ? Supported : Unsupported;
}

CSSSupportsParser::SupportsResult CSSSupportsParser::consumeNegation(CSSParserTokenRange range)
{
    ASSERT(range.peek().type() == IdentToken);
    auto tokenType = range.peek().type();

    if (range.peek().type() == IdentToken)
        range.consume();
    if (range.peek().type() != WhitespaceToken)
        return Invalid;
    range.consumeWhitespace();
    auto result = consumeConditionInParenthesis(range, tokenType);
    range.consumeWhitespace();
    if (!range.atEnd() || result == Invalid)
        return Invalid;

    return result ? Unsupported : Supported;
}

// <function-token> <any-value>? | <supports-selector-fn> | <supports-font-format-fn> | <supports-font-tech-fn>
CSSSupportsParser::SupportsResult CSSSupportsParser::consumeSupportsFunction(CSSParserTokenRange& range)
{
    if (range.peek().type() != FunctionToken)
        return Invalid;

    switch (range.peek().functionId()) {
    case CSSValueSelector:
        return consumeSupportsSelectorFunction(range);
    case CSSValueFontFormat:
        return consumeSupportsFontFormatFunction(range);
    case CSSValueFontTech:
        return consumeSupportsFontTechFunction(range);
    default: // Unknown functions should parse as unsupported.
        range.consumeComponentValue();
        return Unsupported;
    }
}

// <supports-feature> | <general-enclosed>
CSSSupportsParser::SupportsResult CSSSupportsParser::consumeSupportsFeatureOrGeneralEnclosed(CSSParserTokenRange& range)
{
    if (range.peek().type() == FunctionToken) {
        if (auto result = consumeSupportsFunction(range); result != Invalid)
            return result;
    }

    return range.peek().type() == IdentToken && m_parser.supportsDeclaration(range) ? Supported : Unsupported;
}

// <supports-selector-fn>
CSSSupportsParser::SupportsResult CSSSupportsParser::consumeSupportsSelectorFunction(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == FunctionToken && range.peek().functionId() == CSSValueSelector);

    auto block = range.consumeBlock();
    block.consumeWhitespace();

    return CSSSelectorParser::supportsComplexSelector(block, m_parser.context(), m_isNestedContext) ? Supported : Unsupported;
}

// <supports-font-format-fn>
CSSSupportsParser::SupportsResult CSSSupportsParser::consumeSupportsFontFormatFunction(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == FunctionToken && range.peek().functionId() == CSSValueFontFormat);
    auto format = CSSPropertyParserHelpers::consumeFontFormat(range, true);
    if (format.isNull())
        return Unsupported;
    return FontCustomPlatformData::supportsFormat(format) ? Supported : Unsupported;
}

// <supports-font-tech-fn>
CSSSupportsParser::SupportsResult CSSSupportsParser::consumeSupportsFontTechFunction(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == FunctionToken && range.peek().functionId() == CSSValueFontTech);
    auto technologies = CSSPropertyParserHelpers::consumeFontTech(range, true);
    if (technologies.isEmpty())
        return Unsupported;
    ASSERT(technologies.size() == 1);
    return FontCustomPlatformData::supportsTechnology(technologies[0]) ? Supported : Unsupported;
}

// <supports-in-parens> = ( <supports-condition> ) | <supports-feature> | <general-enclosed>
CSSSupportsParser::SupportsResult CSSSupportsParser::consumeConditionInParenthesis(CSSParserTokenRange& range,  CSSParserTokenType startTokenType)
{
    if (startTokenType == IdentToken && range.peek().type() != LeftParenthesisToken) {
        if (auto result = consumeSupportsFunction(range); result != Invalid)
            return result;
        return Invalid;
    }

    auto innerRange = range.consumeBlock();
    innerRange.consumeWhitespace();

    if (auto result = consumeCondition(innerRange); result != Invalid)
        return result;

    return consumeSupportsFeatureOrGeneralEnclosed(innerRange);
}

} // namespace WebCore
