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
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSTokenizer.h"
#include "CSSValueKeywords.h"
#include "StyleCustomProperty.h"
#include <stack>

namespace WebCore {

bool CSSVariableParser::isValidVariableName(const CSSParserToken& token)
{
    if (token.type() != IdentToken)
        return false;

    return isCustomPropertyName(token.value());
}

static bool isValidConstantName(const CSSParserToken& token)
{
    return token.type() == IdentToken;
}

static bool isValidVariableReference(CSSParserTokenRange, const CSSParserContext&);
static bool isValidConstantReference(CSSParserTokenRange, const CSSParserContext&);
static bool isValidDashedFunction(CSSParserTokenRange, const CSSParserContext&);

struct ClassifyBlockResult {
    bool hasReferences { false };
    bool hasTopLevelBraceBlockMixedWithOtherValues { false };
    bool hasEmptyTopLevelBraceBlock { false };
};

static std::optional<ClassifyBlockResult> classifyBlock(CSSParserTokenRange range, const CSSParserContext& parserContext)
{
    struct ClassifyBlockState {
        CSSParserTokenRange range;
        bool isTopLevelBlock = true;
        bool hasOtherValues = false;
        unsigned topLevelBraceBlocks = 0;
        bool doneWithThisRange = false;
    };
    ClassifyBlockState initialState { .range = range };

    std::stack<ClassifyBlockState> stack;
    stack.push(initialState);

    auto result = ClassifyBlockResult { };

    while (!stack.empty()) {
        auto& current = stack.top();
        if (current.doneWithThisRange) {
            // If there is a top level brace block, the value should contains only that.
            if (current.topLevelBraceBlocks > 1 || (current.topLevelBraceBlocks == 1 && current.hasOtherValues))
                result.hasTopLevelBraceBlockMixedWithOtherValues = true;
            stack.pop();
            continue;
        }

        if (current.range.atEnd()) {
            current.doneWithThisRange = true;
            continue;
        }

        if (current.isTopLevelBlock) {
            auto tokenType = current.range.peek().type();
            if (!CSSTokenizer::isWhitespace(tokenType)) {
                if (tokenType == LeftBraceToken)
                    current.topLevelBraceBlocks++;
                else
                    current.hasOtherValues = true;
            }
        }

        if (current.range.peek().getBlockType() == CSSParserToken::BlockStart) {
            const CSSParserToken& token = current.range.peek();
            CSSParserTokenRange block = current.range.consumeBlock();
            block.consumeWhitespace();

            if (token.type() == LeftBraceToken && current.isTopLevelBlock && block.atEnd())
                result.hasEmptyTopLevelBraceBlock = true;

            if (token.functionId() == CSSValueVar) {
                if (!isValidVariableReference(block, parserContext))
                    return { };
                result.hasReferences = true;
                continue;
            }
            if (token.functionId() == CSSValueEnv) {
                if (!isValidConstantReference(block, parserContext))
                    return { };
                result.hasReferences = true;
                continue;
            }
            if (token.type() == FunctionToken && isCustomPropertyName(token.value()) && parserContext.propertySettings.cssFunctionAtRuleEnabled) {
                // https://drafts.csswg.org/css-mixins/#typedef-dashed-function
                if (!isValidDashedFunction(block, parserContext))
                    return { };
                result.hasReferences = true;
                continue;
            }
            stack.push(ClassifyBlockState {
                .range = block,
                .isTopLevelBlock = false, // Nested block, not top-level
            });
            continue;
        }

        ASSERT(current.range.peek().getBlockType() != CSSParserToken::BlockEnd);

        const CSSParserToken& token = current.range.consume();
        switch (token.type()) {
        case AtKeywordToken:
            break;
        case DelimiterToken: {
            if (token.delimiter() == '!' && current.isTopLevelBlock)
                return { };
            break;
        }
        case RightParenthesisToken:
        case RightBraceToken:
        case RightBracketToken:
        case BadStringToken:
        case BadUrlToken:
            return { };
        case SemicolonToken:
            if (current.isTopLevelBlock)
                return { };
            break;
        default:
            break;
        }

    }

    return result;
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

    return !!classifyBlock(range, parserContext);
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

    return !!classifyBlock(range, parserContext);
}

bool isValidDashedFunction(CSSParserTokenRange range, const CSSParserContext& parserContext)
{
    // <dashed-function> --*( <declaration-value>#? )
    range.consumeWhitespace();

    auto validateArgument = [&](auto argumentRange) {
        if (argumentRange.atEnd())
            return false;

        // https://drafts.csswg.org/css-values-5/#component-function-commas
        // Empty brace blocks are just empty values.
        auto result = classifyBlock(argumentRange, parserContext);
        return result && !result->hasTopLevelBraceBlockMixedWithOtherValues && !result->hasEmptyTopLevelBraceBlock;
    };

    unsigned index = 0;
    while (auto argumentRange = CSSPropertyParserHelpers::consumeArgument(range, index)) {
        if (!validateArgument(*argumentRange))
            return false;
        ++index;
    }
    return true;
}

struct VariableType {
    std::optional<CSSWideKeyword> cssWideKeyword { };
    ClassifyBlockResult classifyBlockResult { };
};

static std::optional<VariableType> classifyVariableRange(CSSParserTokenRange range, const CSSParserContext& parserContext)
{
    range.consumeWhitespace();

    if (range.peek().type() == IdentToken) {
        auto rangeCopy = range;
        CSSValueID id = range.consumeIncludingWhitespace().id();
        if (auto keyword = parseCSSWideKeyword(id); range.atEnd() && keyword)
            return VariableType { *keyword };
        // No fast path, restart with the complete range.
        range = rangeCopy;
    }

    auto classifyBlockResult = classifyBlock(range, parserContext);
    if (!classifyBlockResult)
        return { };

    return VariableType { { }, WTFMove(*classifyBlockResult) };
}

bool CSSVariableParser::containsValidVariableReferences(CSSParserTokenRange range, const CSSParserContext& parserContext)
{
    auto type = classifyVariableRange(range, parserContext);
    if (!type)
        return false;

    return type->classifyBlockResult.hasReferences && !type->classifyBlockResult.hasTopLevelBraceBlockMixedWithOtherValues;
}

RefPtr<CSSCustomPropertyValue> CSSVariableParser::parseDeclarationValue(const AtomString& variableName, CSSParserTokenRange range, const CSSParserContext& parserContext)
{
    auto type = classifyVariableRange(range, parserContext);
    if (!type)
        return nullptr;

    if (type->cssWideKeyword)
        return CSSCustomPropertyValue::createWithCSSWideKeyword(variableName, *type->cssWideKeyword);

    if (type->classifyBlockResult.hasReferences)
        return CSSCustomPropertyValue::createUnresolved(variableName, CSSVariableReferenceValue::create(range, parserContext));

    return CSSCustomPropertyValue::createSyntaxAll(variableName, CSSVariableData::create(range, parserContext));
}

RefPtr<const Style::CustomProperty> CSSVariableParser::parseInitialValueForUniversalSyntax(const AtomString& variableName, CSSParserTokenRange range)
{
    auto type = classifyVariableRange(range, strictCSSParserContext());

    if (!type || type->cssWideKeyword || type->classifyBlockResult.hasReferences)
        return nullptr;

    return Style::CustomProperty::createForVariableData(variableName, CSSVariableData::create(range));
}

} // namespace WebCore
