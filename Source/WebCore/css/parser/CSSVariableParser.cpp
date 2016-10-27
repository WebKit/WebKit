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
#include "CSSVariableParser.h"

#include "CSSCustomPropertyValue.h"
#include "CSSParserTokenRange.h"

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

bool isValidVariableReference(CSSParserTokenRange, bool& hasAtApplyRule);

static bool classifyBlock(CSSParserTokenRange range, bool& hasReferences, bool& hasAtApplyRule, bool isTopLevelBlock = true)
{
    while (!range.atEnd()) {
        if (range.peek().getBlockType() == CSSParserToken::BlockStart) {
            const CSSParserToken& token = range.peek();
            CSSParserTokenRange block = range.consumeBlock();
            if (token.functionId() == CSSValueVar) {
                if (!isValidVariableReference(block, hasAtApplyRule))
                    return false; // Bail if any references are invalid
                hasReferences = true;
                continue;
            }
            if (!classifyBlock(block, hasReferences, hasAtApplyRule, false))
                return false;
            continue;
        }

        ASSERT(range.peek().getBlockType() != CSSParserToken::BlockEnd);

        const CSSParserToken& token = range.consume();
        switch (token.type()) {
        case AtKeywordToken: {
            if (equalIgnoringASCIICase(token.value(), "apply")) {
                range.consumeWhitespace();
                const CSSParserToken& variableName = range.consumeIncludingWhitespace();
                if (!CSSVariableParser::isValidVariableName(variableName)
                    || !(range.atEnd() || range.peek().type() == SemicolonToken || range.peek().type() == RightBraceToken))
                    return false;
                hasAtApplyRule = true;
            }
            break;
        }
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

bool isValidVariableReference(CSSParserTokenRange range, bool& hasAtApplyRule)
{
    range.consumeWhitespace();
    if (!CSSVariableParser::isValidVariableName(range.consumeIncludingWhitespace()))
        return false;
    if (range.atEnd())
        return true;

    if (range.consume().type() != CommaToken)
        return false;
    if (range.atEnd())
        return false;

    bool hasReferences = false;
    return classifyBlock(range, hasReferences, hasAtApplyRule);
}

static CSSValueID classifyVariableRange(CSSParserTokenRange range, bool& hasReferences, bool& hasAtApplyRule)
{
    hasReferences = false;
    hasAtApplyRule = false;

    range.consumeWhitespace();
    if (range.peek().type() == IdentToken) {
        CSSValueID id = range.consumeIncludingWhitespace().id();
        if (range.atEnd() && (id == CSSValueInherit || id == CSSValueInitial || id == CSSValueUnset || id == CSSValueRevert))
            return id;
    }

    if (classifyBlock(range, hasReferences, hasAtApplyRule))
        return CSSValueInternalVariableValue;
    return CSSValueInvalid;
}

bool CSSVariableParser::containsValidVariableReferences(CSSParserTokenRange range)
{
    bool hasReferences;
    bool hasAtApplyRule;
    CSSValueID type = classifyVariableRange(range, hasReferences, hasAtApplyRule);
    return type == CSSValueInternalVariableValue && hasReferences && !hasAtApplyRule;
}

RefPtr<CSSCustomPropertyValue> CSSVariableParser::parseDeclarationValue(const AtomicString& variableName, CSSParserTokenRange range)
{
    if (range.atEnd())
        return nullptr;

    bool hasReferences;
    bool hasAtApplyRule;
    CSSValueID type = classifyVariableRange(range, hasReferences, hasAtApplyRule);

    if (type == CSSValueInvalid)
        return nullptr;
    if (type == CSSValueInternalVariableValue)
        return CSSCustomPropertyValue::createWithVariableData(variableName, CSSVariableData::create(range, hasReferences || hasAtApplyRule));
    return CSSCustomPropertyValue::createWithID(variableName, type);
}

} // namespace WebCore
