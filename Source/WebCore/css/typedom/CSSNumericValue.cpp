/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSNumericValue.h"

#include "CSSCalcExpressionNode.h"
#include "CSSCalcExpressionNodeParser.h"
#include "CSSCalcInvertNode.h"
#include "CSSCalcNegateNode.h"
#include "CSSCalcOperationNode.h"
#include "CSSCalcPrimitiveValueNode.h"
#include "CSSCalcSymbolTable.h"
#include "CSSMathClamp.h"
#include "CSSMathInvert.h"
#include "CSSMathMax.h"
#include "CSSMathMin.h"
#include "CSSMathNegate.h"
#include "CSSMathProduct.h"
#include "CSSMathSum.h"
#include "CSSNumericArray.h"
#include "CSSNumericFactory.h"
#include "CSSNumericType.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSTokenizer.h"
#include "CSSUnitValue.h"
#include "ExceptionOr.h"
#include <wtf/Algorithms.h>
#include <wtf/FixedVector.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSNumericValue);

// Explicitly prefixed name used to avoid conflicts with existing macros that can be indirectly #included.
#define CSS_NUMERIC_RETURN_IF_EXCEPTION(resultVariable, expression) \
    auto resultOrException = expression; \
    if (resultOrException.hasException()) \
        return resultOrException.releaseException(); \
    auto resultVariable = resultOrException.releaseReturnValue()

static CalcOperator canonicalOperator(CalcOperator calcOperator)
{
    if (calcOperator == CalcOperator::Add || calcOperator == CalcOperator::Subtract)
        return CalcOperator::Add;
    return CalcOperator::Multiply;
}

static bool canCombineNodes(const CSSCalcOperationNode& root, const CSSCalcExpressionNode& node)
{
    auto operationNode = dynamicDowncast<CSSCalcOperationNode>(node);
    return operationNode && canonicalOperator(root.calcOperator()) == canonicalOperator(operationNode->calcOperator());
}

static Ref<CSSNumericValue> negateOrInvertIfRequired(CalcOperator parentOperator, Ref<CSSNumericValue>&& value)
{
    if (parentOperator == CalcOperator::Subtract)
        return CSSMathNegate::create(WTFMove(value));
    if (parentOperator == CalcOperator::Divide)
        return CSSMathInvert::create(WTFMove(value));
    return WTFMove(value);
}

template<typename T> static ExceptionOr<Ref<CSSNumericValue>> convertToExceptionOrNumericValue(ExceptionOr<Ref<T>>&& input)
{
    CSS_NUMERIC_RETURN_IF_EXCEPTION(result, WTFMove(input));
    return static_reference_cast<CSSNumericValue>(WTFMove(result));
}

template<typename T> static ExceptionOr<Ref<CSSNumericValue>> convertToExceptionOrNumericValue(Ref<T>&& input)
{
    return static_reference_cast<CSSNumericValue>(WTFMove(input));
}

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalcExpressionNode&);

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalcPrimitiveValueNode& root)
{
    auto unit = root.primitiveType();
    return convertToExceptionOrNumericValue(CSSUnitValue::create(root.doubleValue(unit), unit));
}

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalcNegateNode& root)
{
    CSS_NUMERIC_RETURN_IF_EXCEPTION(child, reifyMathExpression(root.child()));
    return convertToExceptionOrNumericValue(CSSMathNegate::create(WTFMove(child)));
}

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalcInvertNode& root)
{
    CSS_NUMERIC_RETURN_IF_EXCEPTION(child, reifyMathExpression(root.child()));
    return convertToExceptionOrNumericValue(CSSMathInvert::create(WTFMove(child)));
}

static ExceptionOr<Vector<Ref<CSSNumericValue>>> reifyMathExpressions(const Vector<Ref<CSSCalcExpressionNode>>& nodes)
{
    Vector<Ref<CSSNumericValue>> values;
    values.reserveInitialCapacity(nodes.size());
    for (auto& node : nodes) {
        CSS_NUMERIC_RETURN_IF_EXCEPTION(value, reifyMathExpression(node));
        values.uncheckedAppend(WTFMove(value));
    }
    return values;
}

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalcOperationNode& root)
{
    if (root.calcOperator() == CalcOperator::Min) {
        CSS_NUMERIC_RETURN_IF_EXCEPTION(values, reifyMathExpressions(root.children()));
        return convertToExceptionOrNumericValue(CSSMathMin::create(WTFMove(values)));
    }
    if (root.calcOperator() == CalcOperator::Max) {
        CSS_NUMERIC_RETURN_IF_EXCEPTION(values, reifyMathExpressions(root.children()));
        return convertToExceptionOrNumericValue(CSSMathMax::create(WTFMove(values)));
    }
    if (root.calcOperator() == CalcOperator::Clamp) {
        CSS_NUMERIC_RETURN_IF_EXCEPTION(values, reifyMathExpressions(root.children()));
        return convertToExceptionOrNumericValue(CSSMathClamp::create(WTFMove(values[0]), WTFMove(values[1]), WTFMove(values[2])));
    }

    Vector<Ref<CSSNumericValue>> values;
    const CSSCalcExpressionNode* currentNode = &root;
    do {
        auto* binaryOperation = downcast<CSSCalcOperationNode>(currentNode);
        ASSERT(binaryOperation->children().size() == 2);
        CSS_NUMERIC_RETURN_IF_EXCEPTION(value, reifyMathExpression(binaryOperation->children()[1].get()));
        values.append(negateOrInvertIfRequired(binaryOperation->calcOperator(), WTFMove(value)));
        currentNode = binaryOperation->children()[0].ptr();
    } while (canCombineNodes(root, *currentNode));

    ASSERT(currentNode);
    CSS_NUMERIC_RETURN_IF_EXCEPTION(reifiedCurrentNode, reifyMathExpression(*currentNode));
    values.append(WTFMove(reifiedCurrentNode));

    std::reverse(values.begin(), values.end());
    if (root.calcOperator() == CalcOperator::Add || root.calcOperator() == CalcOperator::Subtract)
        return convertToExceptionOrNumericValue(CSSMathSum::create(WTFMove(values)));
    return convertToExceptionOrNumericValue(CSSMathProduct::create(WTFMove(values)));
}

// https://drafts.css-houdini.org/css-typed-om/#reify-a-math-expression
static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalcExpressionNode& root)
{
    switch (root.type()) {
    case CSSCalcExpressionNode::CssCalcPrimitiveValue:
        return reifyMathExpression(downcast<CSSCalcPrimitiveValueNode>(root));
    case CSSCalcExpressionNode::CssCalcOperation:
        return reifyMathExpression(downcast<CSSCalcOperationNode>(root));
    case CSSCalcExpressionNode::CssCalcNegate:
        return reifyMathExpression(downcast<CSSCalcNegateNode>(root));
    case CSSCalcExpressionNode::CssCalcInvert:
        return reifyMathExpression(downcast<CSSCalcInvertNode>(root));
    }
    ASSERT_NOT_REACHED();
    return Exception { SyntaxError };
}

static Ref<CSSNumericValue> negate(Ref<CSSNumericValue>&& value)
{
    // https://drafts.css-houdini.org/css-typed-om/#cssmath-negate-a-cssnumericvalue
    if (auto* mathNegate = dynamicDowncast<CSSMathNegate>(value.get()))
        return mathNegate->value();
    if (auto* unitValue = dynamicDowncast<CSSUnitValue>(value.get()))
        return CSSUnitValue::create(-unitValue->value(), unitValue->unitEnum());
    return CSSMathNegate::create(WTFMove(value));
}

static ExceptionOr<Ref<CSSNumericValue>> invert(Ref<CSSNumericValue>&& value)
{
    // https://drafts.css-houdini.org/css-typed-om/#cssmath-invert-a-cssnumericvalue
    if (auto* mathInvert = dynamicDowncast<CSSMathInvert>(value.get()))
        return Ref { mathInvert->value() };

    if (auto* unitValue = dynamicDowncast<CSSUnitValue>(value.get())) {
        if (unitValue->unitEnum() == CSSUnitType::CSS_NUMBER) {
            if (unitValue->value() == 0.0 || unitValue->value() == -0.0)
                return Exception { RangeError };
            return Ref<CSSNumericValue> { CSSUnitValue::create(1.0 / unitValue->value(), unitValue->unitEnum()) };
        }
    }

    return Ref<CSSNumericValue> { CSSMathInvert::create(WTFMove(value)) };
}

template<typename T>
static RefPtr<CSSNumericValue> operationOnValuesOfSameUnit(T&& operation, const Vector<Ref<CSSNumericValue>>& values)
{
    bool allValuesHaveSameUnit = values.size() && WTF::allOf(values, [&] (const Ref<CSSNumericValue>& value) {
        auto* unitValue = dynamicDowncast<CSSUnitValue>(value.get());
        return unitValue ? unitValue->unitEnum() == downcast<CSSUnitValue>(values[0].get()).unitEnum() : false;
    });
    if (allValuesHaveSameUnit) {
        auto& firstUnitValue = downcast<CSSUnitValue>(values[0].get());
        auto unit = firstUnitValue.unitEnum();
        double result = firstUnitValue.value();
        for (size_t i = 1; i < values.size(); ++i)
            result = operation(result, downcast<CSSUnitValue>(values[i].get()).value());
        return CSSUnitValue::create(result, unit);
    }
    return nullptr;
}

template<typename T> Vector<Ref<CSSNumericValue>> CSSNumericValue::prependItemsOfTypeOrThis(Vector<Ref<CSSNumericValue>>&& numericValues)
{
    Vector<Ref<CSSNumericValue>> values;
    if (T* t = dynamicDowncast<T>(*this))
        values.appendVector(t->values().array());
    else
        values.append(*this);
    values.appendVector(numericValues);
    return values;
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::addInternal(Vector<Ref<CSSNumericValue>>&& numericValues)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-add
    auto values = prependItemsOfTypeOrThis<CSSMathSum>(WTFMove(numericValues));

    if (auto result = operationOnValuesOfSameUnit(std::plus<double>(), values))
        return { *result };

    return convertToExceptionOrNumericValue(CSSMathSum::create(WTFMove(values)));
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::add(FixedVector<CSSNumberish>&& values)
{
    return addInternal(WTF::map(WTFMove(values), rectifyNumberish));
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::sub(FixedVector<CSSNumberish>&& values)
{
    return addInternal(WTF::map(WTFMove(values), [] (CSSNumberish&& numberish) {
        return negate(rectifyNumberish(WTFMove(numberish)));
    }));
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::multiplyInternal(Vector<Ref<CSSNumericValue>>&& numericValues)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-mul
    auto values = prependItemsOfTypeOrThis<CSSMathProduct>(WTFMove(numericValues));

    bool allUnitValues = WTF::allOf(values, [&] (const Ref<CSSNumericValue>& value) {
        return is<CSSUnitValue>(value.get());
    });
    if (allUnitValues) {
        bool multipleUnitsFound { false };
        std::optional<size_t> nonNumberUnitIndex;
        for (size_t i = 0; i < values.size(); ++i) {
            auto unit = downcast<CSSUnitValue>(values[i].get()).unitEnum();
            if (unit == CSSUnitType::CSS_NUMBER)
                continue;
            if (nonNumberUnitIndex) {
                multipleUnitsFound = true;
                break;
            }
            nonNumberUnitIndex = i;
        }
        if (!multipleUnitsFound) {
            double product = 1;
            for (const Ref<CSSNumericValue>& value : values)
                product *= downcast<CSSUnitValue>(value.get()).value();
            auto unit = nonNumberUnitIndex ? downcast<CSSUnitValue>(values[*nonNumberUnitIndex].get()).unitEnum() : CSSUnitType::CSS_NUMBER;
            return { CSSUnitValue::create(product, unit) };
        }
    }

    return convertToExceptionOrNumericValue(CSSMathProduct::create(WTFMove(values)));
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::mul(FixedVector<CSSNumberish>&& values)
{
    return multiplyInternal(WTF::map(WTFMove(values), rectifyNumberish));
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::div(FixedVector<CSSNumberish>&& values)
{
    Vector<Ref<CSSNumericValue>> invertedValues;
    invertedValues.reserveInitialCapacity(values.size());
    for (auto&& value : WTFMove(values)) {
        CSS_NUMERIC_RETURN_IF_EXCEPTION(inverted, invert(rectifyNumberish(WTFMove(value))));
        invertedValues.uncheckedAppend(WTFMove(inverted));
    }
    return multiplyInternal(WTFMove(invertedValues));
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::min(FixedVector<CSSNumberish>&& numberishes)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-min
    auto values = prependItemsOfTypeOrThis<CSSMathMin>(WTF::map(WTFMove(numberishes), rectifyNumberish));
    
    if (auto result = operationOnValuesOfSameUnit<const double&(*)(const double&, const double&)>(std::min<double>, values))
        return { *result };

    return convertToExceptionOrNumericValue(CSSMathMin::create(WTFMove(values)));
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::max(FixedVector<CSSNumberish>&& numberishes)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-max
    auto values = prependItemsOfTypeOrThis<CSSMathMax>(WTF::map(WTFMove(numberishes), rectifyNumberish));
    
    if (auto result = operationOnValuesOfSameUnit<const double&(*)(const double&, const double&)>(std::max<double>, values))
        return { *result };

    return convertToExceptionOrNumericValue(CSSMathMax::create(WTFMove(values)));
}

Ref<CSSNumericValue> CSSNumericValue::rectifyNumberish(CSSNumberish&& numberish)
{
    // https://drafts.css-houdini.org/css-typed-om/#rectify-a-numberish-value
    return WTF::switchOn(numberish, [](RefPtr<CSSNumericValue>& value) {
        RELEASE_ASSERT(!!value);
        return Ref<CSSNumericValue> { *value };
    }, [](double value) {
        return Ref<CSSNumericValue> { CSSNumericFactory::number(value) };
    });
}

bool CSSNumericValue::equals(FixedVector<CSSNumberish>&& values)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-equals
    auto numericValues = WTF::map(WTFMove(values), rectifyNumberish);
    return WTF::allOf(numericValues, [&] (const Ref<CSSNumericValue>& value) {
        return this->equals(value.get());
    });
}

ExceptionOr<Ref<CSSUnitValue>> CSSNumericValue::to(String&& unit)
{
    return to(CSSUnitValue::parseUnit(unit));
}

// https://drafts.css-houdini.org/css-typed-om/#create-a-cssunitvalue-from-a-sum-value-item
static RefPtr<CSSUnitValue> createCSSUnitValueFromAddend(CSSNumericValue::Addend addend)
{
    if (addend.units.size() > 1)
        return nullptr;
    if (addend.units.isEmpty())
        return CSSUnitValue::create(addend.value, CSSUnitType::CSS_NUMBER);
    auto unit = addend.units.begin();
    if (unit->value != 1)
        return nullptr;
    return CSSUnitValue::create(addend.value, unit->key);
}

ExceptionOr<Ref<CSSUnitValue>> CSSNumericValue::to(CSSUnitType unit)
{
    // https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-to
    auto type = CSSNumericType::create(unit);
    if (!type)
        return Exception { SyntaxError };

    auto sumValue = toSumValue();
    if (!sumValue || sumValue->size() != 1)
        return Exception { TypeError };

    auto& addend = (*sumValue)[0];
    auto unconverted = createCSSUnitValueFromAddend(addend);
    if (!unconverted)
        return Exception { TypeError };

    auto converted = unconverted->convertTo(unit);
    if (!converted)
        return Exception { TypeError };
    return converted.releaseNonNull();
}

// https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-tosum
ExceptionOr<Ref<CSSMathSum>> CSSNumericValue::toSum(FixedVector<String>&& units)
{
    UNUSED_PARAM(units);
    Vector<CSSUnitType> parsedUnits;
    parsedUnits.reserveInitialCapacity(units.size());
    for (auto& unit : units) {
        auto parsedUnit = CSSUnitValue::parseUnit(unit);
        if (parsedUnit == CSSUnitType::CSS_UNKNOWN)
            return Exception { SyntaxError, "Invalid unit parameter"_s };
        parsedUnits.uncheckedAppend(parsedUnit);
    }
    auto sumValue = toSumValue();
    if (!sumValue)
        return Exception { TypeError, "Could not create a sum value"_s };

    Vector<Ref<CSSNumericValue>> values;
    values.reserveInitialCapacity(sumValue->size());
    for (auto& addend : *sumValue) {
        auto cssUnitValue = createCSSUnitValueFromAddend(addend);
        if (!cssUnitValue)
            return Exception { TypeError, "Could not create CSSUnitValue"_s };
        values.uncheckedAppend(cssUnitValue.releaseNonNull());
    }

    if (parsedUnits.isEmpty()) {
        std::sort(values.begin(), values.end(), [](auto& a, auto& b) {
            return strcmp(static_reference_cast<CSSUnitValue>(a)->unitSerialization().characters(), static_reference_cast<CSSUnitValue>(b)->unitSerialization().characters()) < 0;
        });
        return CSSMathSum::create(WTFMove(values));
    }

    Vector<Ref<CSSNumericValue>> result;
    for (auto& parsedUnit : parsedUnits) {
        auto temp = CSSUnitValue::create(0, parsedUnit);
        for (size_t i = 0; i < values.size();) {
            auto value = static_reference_cast<CSSUnitValue>(values[i]);
            if (auto convertedValue = value->convertTo(parsedUnit)) {
                temp->setValue(temp->value() + convertedValue->value());
                values.remove(i);
            } else
                ++i;
        }
        result.append(WTFMove(temp));
    }

    if (!values.isEmpty())
        return Exception { TypeError, "Failed to convert all values"_s };

    return CSSMathSum::create(WTFMove(result));
}

// https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-parse
ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::parse(String&& cssText)
{
    CSSTokenizer tokenizer(cssText);
    auto range = tokenizer.tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return Exception { SyntaxError, "Failed to parse CSS text"_s };
    const CSSParserToken* componentValueStart = &range.peek();
    range.consumeComponentValue();
    const CSSParserToken* componentValueEnd = &range.peek();
    range.consumeWhitespace();
    if (!range.atEnd())
        return Exception { SyntaxError, "Failed to parse CSS text"_s };

    auto componentValueRange = range.makeSubRange(componentValueStart, componentValueEnd);
    // https://drafts.css-houdini.org/css-typed-om/#reify-a-numeric-value
    switch (componentValueRange.peek().type()) {
    case CSSParserTokenType::DimensionToken:
    case CSSParserTokenType::NumberToken:
    case CSSParserTokenType::PercentageToken: {
        auto& token = componentValueRange.consumeIncludingWhitespace();
        if (token.type() == CSSParserTokenType::DimensionToken && !CSSNumericType::create(token.unitType()))
            return Exception { SyntaxError, "Failed to parse CSS text"_s };
        return convertToExceptionOrNumericValue(CSSUnitValue::create(token.numericValue(), token.unitType()));
    }
    case CSSParserTokenType::FunctionToken: {
        auto functionID = componentValueRange.peek().functionId();
        if (functionID == CSSValueCalc || functionID == CSSValueMin || functionID == CSSValueMax || functionID == CSSValueClamp) {
            CSSCalcExpressionNodeParser parser(CalculationCategory::Length, { });
            if (auto expression = parser.parseCalc(CSSPropertyParserHelpers::consumeFunction(componentValueRange), functionID, false))
                return reifyMathExpression(*expression);
        }
        break;
    }
    default:
        break;
    }
    return Exception { SyntaxError, "Failed to parse CSS text"_s };
}

} // namespace WebCore

#undef CSS_NUMERIC_RETURN_IF_EXCEPTION
