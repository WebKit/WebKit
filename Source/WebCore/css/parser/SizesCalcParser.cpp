// Copyright 2014-2017 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
#include "SizesCalcParser.h"

#include "CSSParserToken.h"
#include "RenderView.h"
#include "SizesAttributeParser.h"

namespace WebCore {

SizesCalcParser::SizesCalcParser(CSSParserTokenRange range, const Document& document)
    : m_result(0)
    , m_document(document)
{
    m_isValid = calcToReversePolishNotation(range) && calculate();
}

float SizesCalcParser::result() const
{
    ASSERT(m_isValid);
    return m_result;
}

static bool operatorPriority(char16_t cc, bool& highPriority)
{
    if (cc == '+' || cc == '-')
        highPriority = false;
    else if (cc == '*' || cc == '/')
        highPriority = true;
    else
        return false;
    return true;
}

bool SizesCalcParser::handleOperator(Vector<CSSParserToken>& stack, const CSSParserToken& token)
{
    // If the token is not an operator, then return. Else determine the
    // precedence of the new operator (op1).
    bool incomingOperatorPriority;
    if (!operatorPriority(token.delimiter(), incomingOperatorPriority))
        return false;
    while (!stack.isEmpty()) {
        // While there is an operator (op2) at the top of the stack,
        // determine its precedence, and...
        if (stack.last().type() != DelimiterToken)
            break;
        bool stackOperatorPriority;
        if (!operatorPriority(stack.last().delimiter(), stackOperatorPriority))
            return false;
        // ...if op1 is left-associative (all currently supported
        // operators are) and its precedence is equal to that of op2, or
        // op1 has precedence less than that of op2, ...
        if (incomingOperatorPriority && !stackOperatorPriority)
            break;
        // ...pop op2 off the stack and add it to the output queue.
        appendOperator(stack.last());
        stack.removeLast();
    }
    // Push op1 onto the stack.
    stack.append(token);
    return true;
}

bool SizesCalcParser::handleRightParenthesis(Vector<CSSParserToken>& stack)
{
    // If the token is a right parenthesis:
    // Until the token at the top of the stack is a left parenthesis or a
    // function, pop operators off the stack onto the output queue.
    // Also count the number of commas to get the number of function
    // parameters if this right parenthesis closes a function.
    unsigned commaCount = 0;
    while (!stack.isEmpty() && stack.last().type() != LeftParenthesisToken && stack.last().type() != FunctionToken) {
        if (stack.last().type() == CommaToken)
            ++commaCount;
        else {
            // Only append actual operators (DelimiterToken), not commas
            ASSERT(stack.last().type() == DelimiterToken);
            appendOperator(stack.last());
        }
        stack.removeLast();
    }
    // If the stack runs out without finding a left parenthesis, then there
    // are mismatched parentheses.
    if (stack.isEmpty())
        return false;

    CSSParserToken leftSide = stack.last();
    stack.removeLast();

    if (leftSide.type() == LeftParenthesisToken || equalLettersIgnoringASCIICase(leftSide.value(), "calc"_s)) {
        // There should be exactly one calculation within calc() or parentheses.
        return !commaCount;
    }

    if (equalLettersIgnoringASCIICase(leftSide.value(), "clamp"_s)) {
        if (commaCount != 2)
            return false;
        // Convert clamp(MIN, VAL, MAX) into max(MIN, min(VAL, MAX))
        // https://www.w3.org/TR/css-values-4/#calc-notation
        SizesCalcValue minValue;
        minValue.operation = 'm';
        m_valueList.append(minValue);
        SizesCalcValue maxValue;
        maxValue.operation = 'M';
        m_valueList.append(maxValue);
        return true;
    }

    // Break variadic min/max() into binary operations to fit in the reverse
    // polish notation.
    char16_t op = equalLettersIgnoringASCIICase(leftSide.value(), "min"_s) ? 'm' : 'M';
    for (unsigned i = 0; i < commaCount; ++i) {
        SizesCalcValue value;
        value.operation = op;
        m_valueList.append(value);
    }
    return true;
}

bool SizesCalcParser::handleComma(Vector<CSSParserToken>& stack, const CSSParserToken& token)
{
    // Treat comma as a binary right-associative operation for now, so that
    // when reaching the right parenthesis of the function, we can get the
    // number of parameters by counting the number of commas.
    while (!stack.isEmpty() && stack.last().type() != FunctionToken && stack.last().type() != LeftParenthesisToken
        && stack.last().type() != CommaToken) {
        // Only DelimiterToken should be popped here.
        if (stack.last().type() != DelimiterToken)
            return false;
        appendOperator(stack.last());
        stack.removeLast();
    }
    // Commas are allowed as function parameter separators only
    if (stack.isEmpty() || stack.last().type() == LeftParenthesisToken)
        return false;
    stack.append(token);
    return true;
}

void SizesCalcParser::appendNumber(const CSSParserToken& token)
{
    SizesCalcValue value;
    value.value = token.numericValue();
    m_valueList.append(value);
}

bool SizesCalcParser::appendLength(const CSSParserToken& token)
{
    auto lengthUnit = CSS::toLengthUnit(token.unitType());
    if (!lengthUnit)
        return false;
    SizesCalcValue value;
    double result = SizesAttributeParser::computeLength(token.numericValue(), *lengthUnit, m_document);
    value.value = result;
    value.isLength = true;
    m_valueList.append(value);
    return true;
}

void SizesCalcParser::appendOperator(const CSSParserToken& token)
{
    SizesCalcValue value;
    // Only DelimiterToken types have delimiters (+, -, *, /)
    // FunctionTokens (min, max, clamp) should not reach here.
    ASSERT(token.type() == DelimiterToken);
    value.operation = token.delimiter();
    m_valueList.append(value);
}

bool SizesCalcParser::calcToReversePolishNotation(CSSParserTokenRange range)
{
    // This method implements the shunting yard algorithm, to turn the calc syntax into a reverse polish notation.
    // http://en.wikipedia.org/wiki/Shunting-yard_algorithm

    Vector<CSSParserToken> stack;
    while (!range.atEnd()) {
        const CSSParserToken& token = range.consume();
        switch (token.type()) {
        case NumberToken:
            appendNumber(token);
            break;
        case DimensionToken:
            if (!appendLength(token))
                return false;
            break;
        case DelimiterToken:
            if (!handleOperator(stack, token))
                return false;
            break;
        case FunctionToken:
            if (equalLettersIgnoringASCIICase(token.value(), "min"_s)
                || equalLettersIgnoringASCIICase(token.value(), "max"_s)
                || equalLettersIgnoringASCIICase(token.value(), "clamp"_s)) {
                stack.append(token);
                break;
            }
            if (!equalLettersIgnoringASCIICase(token.value(), "calc"_s))
                return false;
            // "calc(" is the same as "("
            [[fallthrough]];
        case LeftParenthesisToken:
            // If the token is a left parenthesis, then push it onto the stack.
            stack.append(token);
            break;
        case RightParenthesisToken:
            if (!handleRightParenthesis(stack))
                return false;
            break;
        case CommaToken:
            if (!handleComma(stack, token))
                return false;
            break;
        case NonNewlineWhitespaceToken:
        case NewlineToken:
        case EOFToken:
            break;
        case CDOToken:
        case CDCToken:
        case AtKeywordToken:
        case HashToken:
        case UrlToken:
        case BadUrlToken:
        case PercentageToken:
        case IncludeMatchToken:
        case DashMatchToken:
        case PrefixMatchToken:
        case SuffixMatchToken:
        case SubstringMatchToken:
        case ColumnToken:
        case IdentToken:
        case ColonToken:
        case SemicolonToken:
        case LeftBraceToken:
        case LeftBracketToken:
        case RightBraceToken:
        case RightBracketToken:
        case StringToken:
        case BadStringToken:
            return false;
        case CommentToken:
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    // When there are no more tokens to read:
    // While there are still operator tokens in the stack:
    while (!stack.isEmpty()) {
        CSSParserTokenType type = stack.last().type();
        // If the operator token on the top of the stack is a left parenthesis (not a function),
        // then there are mismatched parentheses.
        if (type == LeftParenthesisToken)
            return false;

        // FunctionTokens are automatically closed at EOF per CSS spec
        if (type == FunctionToken) {
            if (!handleRightParenthesis(stack))
                return false;
            continue;
        }

        // CommaTokens at this point indicate malformed input
        // (unclosed function with trailing comma)
        if (type == CommaToken)
            return false;

        // Pop regular operators (DelimiterToken) onto the output queue
        ASSERT(type == DelimiterToken);
        appendOperator(stack.last());
        stack.removeLast();
    }
    return true;
}

static bool operateOnStack(Vector<SizesCalcValue>& stack, char16_t operation)
{
    if (stack.size() < 2)
        return false;
    SizesCalcValue rightOperand = stack.last();
    stack.removeLast();
    SizesCalcValue leftOperand = stack.last();
    stack.removeLast();
    bool isLength;
    switch (operation) {
    case '+':
        if (rightOperand.isLength != leftOperand.isLength)
            return false;
        isLength = (rightOperand.isLength && leftOperand.isLength);
        stack.append(SizesCalcValue(leftOperand.value + rightOperand.value, isLength));
        break;
    case '-':
        if (rightOperand.isLength != leftOperand.isLength)
            return false;
        isLength = (rightOperand.isLength && leftOperand.isLength);
        stack.append(SizesCalcValue(leftOperand.value - rightOperand.value, isLength));
        break;
    case '*':
        if (rightOperand.isLength && leftOperand.isLength)
            return false;
        isLength = (rightOperand.isLength || leftOperand.isLength);
        stack.append(SizesCalcValue(leftOperand.value * rightOperand.value, isLength));
        break;
    case '/':
        if (rightOperand.isLength || !rightOperand.value)
            return false;
        stack.append(SizesCalcValue(leftOperand.value / rightOperand.value, leftOperand.isLength));
        break;
    case 'm': // min
        if (rightOperand.isLength != leftOperand.isLength)
            return false;
        isLength = (rightOperand.isLength && leftOperand.isLength);
        stack.append(SizesCalcValue(std::min(leftOperand.value, rightOperand.value), isLength));
        break;
    case 'M': // max
        if (rightOperand.isLength != leftOperand.isLength)
            return false;
        isLength = (rightOperand.isLength && leftOperand.isLength);
        stack.append(SizesCalcValue(std::max(leftOperand.value, rightOperand.value), isLength));
        break;
    default:
        return false;
    }
    return true;
}

bool SizesCalcParser::calculate()
{
    Vector<SizesCalcValue> stack;
    for (const auto& value : m_valueList) {
        if (!value.operation)
            stack.append(value);
        else {
            if (!operateOnStack(stack, value.operation))
                return false;
        }
    }
    if (stack.size() == 1 && stack.last().isLength) {
        m_result = std::max(clampTo<float>(stack.last().value), 0.0f);
        return true;
    }
    return false;
}

} // namespace WebCore
