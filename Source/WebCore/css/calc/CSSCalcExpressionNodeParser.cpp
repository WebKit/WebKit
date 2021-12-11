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

#include "config.h"
#include "CSSCalcExpressionNodeParser.h"

#include "CSSCalcCategoryMapping.h"
#include "CSSCalcInvertNode.h"
#include "CSSCalcNegateNode.h"
#include "CSSCalcOperationNode.h"
#include "CSSCalcPrimitiveValueNode.h"
#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSParserToken.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserHelpers.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

static constexpr int maxExpressionDepth = 100;

// <https://drafts.csswg.org/css-values-4/#calc-syntax>:
// <calc()>  = calc( <calc-sum> )
// <min()>   = min( <calc-sum># )
// <max()>   = max( <calc-sum># )
// <clamp()> = clamp( <calc-sum>#{3} )
// <sin()>   = sin( <calc-sum> )
// <cos()>   = cos( <calc-sum> )
// <tan()>   = tan( <calc-sum> )
// <asin()>  = asin( <calc-sum> )
// <acos()>  = acos( <calc-sum> )
// <atan()>  = atan( <calc-sum> )
// <atan2()> = atan2( <calc-sum>, <calc-sum> )
// <pow()>   = pow( <calc-sum>, <calc-sum> )
// <sqrt()>  = sqrt( <calc-sum> )
// <hypot()> = hypot( <calc-sum># )
// <calc-sum> = <calc-product> [ [ '+' | '-' ] <calc-product> ]*
// <calc-product> = <calc-value> [ [ '*' | '/' ] <calc-value> ]*
// <calc-value> = <number> | <dimension> | <percentage> | ( <calc-sum> )
RefPtr<CSSCalcExpressionNode> CSSCalcExpressionNodeParser::parseCalc(CSSParserTokenRange tokens, CSSValueID function, bool allowsNegativePercentage)
{
    std::function<void(CSSCalcExpressionNode&)> setAllowsNegativePercentageReferenceIfNeeded = [&](CSSCalcExpressionNode& expression) {
        if (is<CSSCalcOperationNode>(expression)) {
            auto& operationNode = downcast<CSSCalcOperationNode>(expression);
            if (operationNode.isMinOrMaxNode())
                operationNode.setAllowsNegativePercentageReference();

            for (auto& child : operationNode.children())
                setAllowsNegativePercentageReferenceIfNeeded(child);
        }
    };

    tokens.consumeWhitespace();

    RefPtr<CSSCalcExpressionNode> result;
    bool ok = parseCalcFunction(tokens, function, 0, result);
    if (!ok || !tokens.atEnd())
        return nullptr;

    if (!result)
        return nullptr;

    LOG_WITH_STREAM(Calc, stream << "CSSCalcExpressionNodeParser::parseCalc " << prettyPrintNode(*result));

    if (allowsNegativePercentage)
        setAllowsNegativePercentageReferenceIfNeeded(*result);

    result = CSSCalcOperationNode::simplify(result.releaseNonNull());

    LOG_WITH_STREAM(Calc, stream << "CSSCalcExpressionNodeParser::parseCalc - after simplification " << prettyPrintNode(*result));

    return result;
}

char CSSCalcExpressionNodeParser::operatorValue(const CSSParserToken& token)
{
    if (token.type() == DelimiterToken)
        return token.delimiter();
    return 0;
}

enum ParseState {
    OK,
    TooDeep,
    NoMoreTokens
};

static const CSSCalcSymbolTable getConstantTable()
{
    return {
        { CSSValuePi, CSSUnitType::CSS_NUMBER, piDouble }, { CSSValueE, CSSUnitType::CSS_NUMBER, std::exp(1.0) },
        { CSSValueNegativeInfinity, CSSUnitType::CSS_NUMBER, -1 * std::numeric_limits<double>::infinity() },
        { CSSValueInfinity, CSSUnitType::CSS_NUMBER, std::numeric_limits<double>::infinity() },
        { CSSValueNaN, CSSUnitType::CSS_NUMBER, std::numeric_limits<double>::quiet_NaN() },
    };
}

static ParseState checkDepthAndIndex(int depth, CSSParserTokenRange tokens)
{
    if (tokens.atEnd())
        return NoMoreTokens;
    if (depth > maxExpressionDepth) {
        LOG_WITH_STREAM(Calc, stream << "Depth " << depth << " exceeded maxExpressionDepth " << maxExpressionDepth);
        return TooDeep;
    }
    return OK;
}

bool CSSCalcExpressionNodeParser::parseCalcFunction(CSSParserTokenRange& tokens, CSSValueID functionID, int depth, RefPtr<CSSCalcExpressionNode>& result)
{
    if (checkDepthAndIndex(depth, tokens) != OK)
        return false;

    // "arguments" refers to things between commas.
    unsigned minArgumentCount = 1;
    std::optional<unsigned> maxArgumentCount;

    switch (functionID) {
    case CSSValueMin:
    case CSSValueMax:
    case CSSValueHypot:
        maxArgumentCount = std::nullopt;
        break;
    case CSSValueClamp:
        minArgumentCount = 3;
        maxArgumentCount = 3;
        break;
    case CSSValueLog:
        maxArgumentCount = 2;
        break;
    case CSSValueRound:
        minArgumentCount = 2;
        maxArgumentCount = 3;
        break;
    case CSSValueMod:
    case CSSValueRem:
        minArgumentCount = 2;
        maxArgumentCount = 2;
        break;
    case CSSValueExp:
    case CSSValueSin:
    case CSSValueCos:
    case CSSValueTan:
    case CSSValueAcos:
    case CSSValueAsin:
    case CSSValueAtan:
    case CSSValueSign:
    case CSSValueAbs:
    case CSSValueCalc:
        maxArgumentCount = 1;
        break;
    case CSSValueAtan2:
        maxArgumentCount = 2;
        break;
    case CSSValuePow:
        minArgumentCount = 2;
        maxArgumentCount = 2;
        break;
    case CSSValueSqrt:
        maxArgumentCount = 1;
        break;
    default:
        break;
    }

    Vector<Ref<CSSCalcExpressionNode>> nodes;

    bool requireComma = false;
    unsigned argumentCount = 0;
    while (!tokens.atEnd()) {
        tokens.consumeWhitespace();
        if (requireComma && !CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(tokens))
            return false;

        RefPtr<CSSCalcExpressionNode> node;
        if (!parseCalcSum(tokens, functionID, depth, node))
            return false;

        ++argumentCount;
        if (maxArgumentCount && argumentCount > maxArgumentCount.value())
            return false;

        nodes.append(node.releaseNonNull());
        requireComma = true;
    }
    
    if (argumentCount < minArgumentCount)
        return false;

    switch (functionID) {
    case CSSValueMin:
        result = CSSCalcOperationNode::createMinOrMaxOrClamp(CalcOperator::Min, WTFMove(nodes), m_destinationCategory);
        break;
    case CSSValueMax:
        result = CSSCalcOperationNode::createMinOrMaxOrClamp(CalcOperator::Max, WTFMove(nodes), m_destinationCategory);
        break;
    case CSSValueClamp:
        result = CSSCalcOperationNode::createMinOrMaxOrClamp(CalcOperator::Clamp, WTFMove(nodes), m_destinationCategory);
        break;
    case CSSValueSin:
        result = CSSCalcOperationNode::createTrig(CalcOperator::Sin, WTFMove(nodes));
        break;
    case CSSValueCos:
        result = CSSCalcOperationNode::createTrig(CalcOperator::Cos, WTFMove(nodes));
        break;
    case CSSValueTan:
        result = CSSCalcOperationNode::createTrig(CalcOperator::Tan, WTFMove(nodes));
        break;
    case CSSValueRound:
        result = CSSCalcOperationNode::createRound(WTFMove(nodes));
        break;
    case CSSValueMod:
        result = CSSCalcOperationNode::createStep(CalcOperator::Mod, WTFMove(nodes));
        break;
    case CSSValueRem:
        result = CSSCalcOperationNode::createStep(CalcOperator::Rem, WTFMove(nodes));
        break;
    case CSSValueWebkitCalc:
    case CSSValueCalc:
        result = CSSCalcOperationNode::createSum(WTFMove(nodes));
        break;
    case CSSValueLog:
        result = CSSCalcOperationNode::createLog(WTFMove(nodes));
        break;
    case CSSValueExp:
        result = CSSCalcOperationNode::createExp(WTFMove(nodes));
        break;
    case CSSValueAcos:
        result = CSSCalcOperationNode::createInverseTrig(CalcOperator::Acos, WTFMove(nodes));
        break;
    case CSSValueAsin:
        result = CSSCalcOperationNode::createInverseTrig(CalcOperator::Asin, WTFMove(nodes));
        break;
    case CSSValueAtan:
        result = CSSCalcOperationNode::createInverseTrig(CalcOperator::Atan, WTFMove(nodes));
        break;
    case CSSValueAtan2:
        result = CSSCalcOperationNode::createAtan2(WTFMove(nodes));
        break;
    case CSSValueAbs:
        result = CSSCalcOperationNode::createSign(CalcOperator::Abs, WTFMove(nodes));
        break;
    case CSSValueSign:
        result = CSSCalcOperationNode::createSign(CalcOperator::Sign, WTFMove(nodes));
        break;
    case CSSValuePow:
        result = CSSCalcOperationNode::createPowOrSqrt(CalcOperator::Pow, WTFMove(nodes));
        break;
    case CSSValueSqrt:
        result = CSSCalcOperationNode::createPowOrSqrt(CalcOperator::Sqrt, WTFMove(nodes));
        break;
    case CSSValueHypot:
        result = CSSCalcOperationNode::createHypot(WTFMove(nodes));
        break;
    default:
        break;
    }

    return !!result;
}

static bool checkRoundKeyword(CSSValueID functionID, RefPtr<CSSCalcExpressionNode>& result, CSSValueID constantID)
{
    if (functionID != CSSValueRound)
        return false;
    switch (constantID) {
    case CSSValueNearest:
        result = CSSCalcOperationNode::createRoundConstant(CalcOperator::Nearest);
        return true;
    case CSSValueToZero:
        result = CSSCalcOperationNode::createRoundConstant(CalcOperator::ToZero);
        return true;
    case CSSValueUp:
        result = CSSCalcOperationNode::createRoundConstant(CalcOperator::Up);
        return true;
    case CSSValueDown:
        result = CSSCalcOperationNode::createRoundConstant(CalcOperator::Down);
        return true;
    default:
        return false;
    }
}

bool CSSCalcExpressionNodeParser::parseValue(CSSParserTokenRange& tokens, CSSValueID functionID, RefPtr<CSSCalcExpressionNode>& result)
{
    auto makeCSSCalcPrimitiveValueNode = [&] (CSSUnitType type, double value) -> bool {
        if (calcUnitCategory(type) == CalculationCategory::Other)
            return false;
        
        result = CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(value, type));
        return true;
    };

    auto token = tokens.consumeIncludingWhitespace();

    switch (token.type()) {
    case IdentToken: {
        if (checkRoundKeyword(functionID, result, token.id()))
            return true;
        auto value = m_symbolTable.get(token.id());
        value = value ? value : getConstantTable().get(token.id());
        if (!value)
            return false;
        return makeCSSCalcPrimitiveValueNode(value->type, value->value);
    }

    case NumberToken:
    case PercentageToken:
    case DimensionToken:
        return makeCSSCalcPrimitiveValueNode(token.unitType(), token.numericValue());

    default:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool CSSCalcExpressionNodeParser::parseCalcValue(CSSParserTokenRange& tokens, CSSValueID functionID, int depth, RefPtr<CSSCalcExpressionNode>& result)
{
    if (checkDepthAndIndex(depth, tokens) != OK)
        return false;

    auto findFunctionId = [&](CSSValueID& functionId) {
        if (tokens.peek().type() == LeftParenthesisToken) {
            functionId = CSSValueCalc;
            return true;
        }

        functionId = tokens.peek().functionId();
        return CSSCalcValue::isCalcFunction(functionId);
    };

    CSSValueID functionId;
    if (findFunctionId(functionId)) {
        CSSParserTokenRange innerRange = tokens.consumeBlock();
        tokens.consumeWhitespace();
        innerRange.consumeWhitespace();
        return parseCalcFunction(innerRange, functionId, depth + 1, result);
    }

    return parseValue(tokens, functionID, result);
}

bool CSSCalcExpressionNodeParser::parseCalcProduct(CSSParserTokenRange& tokens, CSSValueID functionID, int depth, RefPtr<CSSCalcExpressionNode>& result)
{
    if (checkDepthAndIndex(depth, tokens) != OK)
        return false;

    RefPtr<CSSCalcExpressionNode> firstValue;
    if (!parseCalcValue(tokens, functionID, depth, firstValue))
        return false;

    Vector<Ref<CSSCalcExpressionNode>> nodes;

    while (!tokens.atEnd()) {
        char operatorCharacter = operatorValue(tokens.peek());
        if (operatorCharacter != static_cast<char>(CalcOperator::Multiply) && operatorCharacter != static_cast<char>(CalcOperator::Divide))
            break;
        tokens.consumeIncludingWhitespace();
        
        RefPtr<CSSCalcExpressionNode> nextValue;
        if (!parseCalcValue(tokens, functionID, depth, nextValue) || !nextValue)
            return false;

        if (operatorCharacter == static_cast<char>(CalcOperator::Divide))
            nextValue = CSSCalcInvertNode::create(nextValue.releaseNonNull());

        if (firstValue)
            nodes.append(firstValue.releaseNonNull());

        nodes.append(nextValue.releaseNonNull());
    }

    if (nodes.isEmpty()) {
        result = WTFMove(firstValue);
        return !!result;
    }

    result = CSSCalcOperationNode::createProduct(WTFMove(nodes));
    return !!result;
}

bool CSSCalcExpressionNodeParser::parseCalcSum(CSSParserTokenRange& tokens, CSSValueID functionID, int depth, RefPtr<CSSCalcExpressionNode>& result)
{
    if (checkDepthAndIndex(depth, tokens) != OK)
        return false;

    RefPtr<CSSCalcExpressionNode> firstValue;
    if (!parseCalcProduct(tokens, functionID, depth, firstValue))
        return false;

    Vector<Ref<CSSCalcExpressionNode>> nodes;

    while (!tokens.atEnd()) {
        char operatorCharacter = operatorValue(tokens.peek());
        if (operatorCharacter != static_cast<char>(CalcOperator::Add) && operatorCharacter != static_cast<char>(CalcOperator::Subtract))
            break;

        if ((&tokens.peek() - 1)->type() != WhitespaceToken)
            return false; // calc(1px+ 2px) is invalid

        tokens.consume();
        if (tokens.peek().type() != WhitespaceToken)
            return false; // calc(1px +2px) is invalid

        tokens.consumeIncludingWhitespace();

        RefPtr<CSSCalcExpressionNode> nextValue;
        if (!parseCalcProduct(tokens, functionID, depth, nextValue) || !nextValue)
            return false;

        if (operatorCharacter == static_cast<char>(CalcOperator::Subtract))
            nextValue = CSSCalcNegateNode::create(nextValue.releaseNonNull());

        if (firstValue)
            nodes.append(firstValue.releaseNonNull());

        nodes.append(nextValue.releaseNonNull());
    }

    if (nodes.isEmpty()) {
        result = WTFMove(firstValue);
        return !!result;
    }

    result = CSSCalcOperationNode::createSum(WTFMove(nodes));
    return !!result;
}

}
