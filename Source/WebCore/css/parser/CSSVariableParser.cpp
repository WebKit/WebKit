// Copyright 2015 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "CSSVariableParser.h"

#include "CSSCustomPropertyValue.h"
#include "CSSParserContext.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserHelpers.h"

namespace WebCore {

bool CSSVariableParser::isValidVariableName(const CSSParserToken& token)
{
    if (token.type() != IdentToken)
        return false;

    StringView value = token.value();
    return value.length() >= 2 && value[0] == '-' && value[1] == '-';
}

bool CSSVariableParser::isValidVariableName(const String& string)
{
    return string.length() >= 2 && string[0] == '-' && string[1] == '-';
}

static bool isValidConstantName(const CSSParserToken& token)
{
    return token.type() == IdentToken;
}

static bool isValidVariableReference(CSSParserTokenRange, const CSSParserContext&);
static bool isValidConstantReference(CSSParserTokenRange, const CSSParserContext&);

static bool classifyBlock(CSSParserTokenRange range, bool& hasReferences, const CSSParserContext& parserContext, bool isTopLevelBlock = true)
{
    while (!range.atEnd()) {
        if (range.peek().getBlockType() == CSSParserToken::BlockStart) {
            const CSSParserToken& token = range.peek();
            CSSParserTokenRange block = range.consumeBlock();
            if (token.functionId() == CSSValueVar) {
                if (!isValidVariableReference(block, parserContext))
                    return false; // Bail if any references are invalid
                hasReferences = true;
                continue;
            }
            if (token.functionId() == CSSValueEnv && parserContext.constantPropertiesEnabled) {
                if (!isValidConstantReference(block, parserContext))
                    return false; // Bail if any references are invalid
                hasReferences = true;
                continue;
            }
            if (!classifyBlock(block, hasReferences, parserContext, false))
                return false;
            continue;
        }

        ASSERT(range.peek().getBlockType() != CSSParserToken::BlockEnd);

        const CSSParserToken& token = range.consume();
        switch (token.type()) {
        case AtKeywordToken:
            break;
        case DelimiterToken: {
            if (token.delimiter() == '!' && isTopLevelBlock)
                return false;
            break;
        }
        case RightParenthesisToken:
        case RightBraceToken:
        case RightBracketToken:
        case BadStringToken:
        case BadUrlToken:
            return false;
        case SemicolonToken:
            if (isTopLevelBlock)
                return false;
            break;
        default:
            break;
        }
    }
    return true;
}

bool isValidVariableReference(CSSParserTokenRange range, const CSSParserContext& parserContext)
{
    range.consumeWhitespace();
    if (!CSSVariableParser::isValidVariableName(range.consumeIncludingWhitespace()))
        return false;
    if (range.atEnd())
        return true;

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range))
        return false;
    if (range.atEnd())
        return true;

    bool hasReferences = false;
    return classifyBlock(range, hasReferences, parserContext);
}

bool isValidConstantReference(CSSParserTokenRange range, const CSSParserContext& parserContext)
{
    range.consumeWhitespace();
    if (!isValidConstantName(range.consumeIncludingWhitespace()))
        return false;
    if (range.atEnd())
        return true;

    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range))
        return false;
    if (range.atEnd())
        return true;

    bool hasReferences = false;
    return classifyBlock(range, hasReferences, parserContext);
}

static CSSValueID classifyVariableRange(CSSParserTokenRange range, bool& hasReferences, const CSSParserContext& parserContext)
{
    hasReferences = false;

    range.consumeWhitespace();
    if (range.peek().type() == IdentToken) {
        CSSValueID id = range.consumeIncludingWhitespace().id();
        if (range.atEnd() && isCSSWideKeyword(id))
            return id;
    }

    if (classifyBlock(range, hasReferences, parserContext))
        return CSSValueInternalVariableValue;
    return CSSValueInvalid;
}

bool CSSVariableParser::containsValidVariableReferences(CSSParserTokenRange range, const CSSParserContext& parserContext)
{
    bool hasReferences;
    CSSValueID type = classifyVariableRange(range, hasReferences, parserContext);
    return type == CSSValueInternalVariableValue && hasReferences;
}

RefPtr<CSSCustomPropertyValue> CSSVariableParser::parseDeclarationValue(const AtomString& variableName, CSSParserTokenRange range, const CSSParserContext& parserContext)
{
    if (range.atEnd())
        return nullptr;

    bool hasReferences;
    CSSValueID type = classifyVariableRange(range, hasReferences, parserContext);

    if (type == CSSValueInvalid)
        return nullptr;
    if (type == CSSValueInternalVariableValue)
        return CSSCustomPropertyValue::createUnresolved(variableName, CSSVariableReferenceValue::create(range, parserContext));
    return CSSCustomPropertyValue::createUnresolved(variableName, type);
}

RefPtr<CSSCustomPropertyValue> CSSVariableParser::parseInitialValueForUniversalSyntax(const AtomString& variableName, CSSParserTokenRange range)
{
    if (range.atEnd())
        return nullptr;

    bool hasReferences;
    CSSValueID valueID = classifyVariableRange(range, hasReferences, strictCSSParserContext());

    if (hasReferences)
        return nullptr;
    if (valueID != CSSValueInternalVariableValue)
        return nullptr;

    return CSSCustomPropertyValue::createSyntaxAll(variableName, CSSVariableData::create(range));
}

} // namespace WebCore
