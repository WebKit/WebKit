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

#include "CSSCalcSymbolTable.h"
#include "CSSCalcTree+Parser.h"
#include "CSSCalcTree+Simplification.h"
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
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSTokenizer.h"
#include "CSSUnitValue.h"
#include "CalculationCategory.h"
#include "ExceptionOr.h"
#include <wtf/Algorithms.h>
#include <wtf/FixedVector.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSNumericValue);

// Explicitly prefixed name used to avoid conflicts with existing macros that can be indirectly #included.
#define CSS_NUMERIC_RETURN_IF_EXCEPTION(resultVariable, expression) \
    auto resultOrException_##resultVariable = expression; \
    if (resultOrException_##resultVariable.hasException()) \
        return resultOrException_##resultVariable.releaseException(); \
    auto resultVariable = resultOrException_##resultVariable.releaseReturnValue()

template<typename T> static ExceptionOr<Ref<CSSNumericValue>> convertToExceptionOrNumericValue(ExceptionOr<Ref<T>>&& input)
{
    CSS_NUMERIC_RETURN_IF_EXCEPTION(result, WTFMove(input));
    return static_reference_cast<CSSNumericValue>(WTFMove(result));
}

template<typename T> static ExceptionOr<Ref<CSSNumericValue>> convertToExceptionOrNumericValue(Ref<T>&& input)
{
    return static_reference_cast<CSSNumericValue>(WTFMove(input));
}

static ExceptionOr<Vector<Ref<CSSNumericValue>>> reifyMathExpressions(const CSSCalc::Children& nodes)
{
    Vector<Ref<CSSNumericValue>> values;
    values.reserveInitialCapacity(nodes.size());
    for (auto& node : nodes) {
        CSS_NUMERIC_RETURN_IF_EXCEPTION(value, CSSNumericValue::reifyMathExpression(node));
        values.append(WTFMove(value));
    }
    return values;
}

template<CSSCalc::Numeric T> static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const T& root)
{
    return convertToExceptionOrNumericValue(CSSUnitValue::create(root.value, CSSCalc::toCSSUnit(root)));
}

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalc::Symbol&)
{
    // CSS Typed OM doesn't currently support unresolved symbols.
    return Exception { ExceptionCode::UnknownError };
}

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalc::IndirectNode<CSSCalc::Sum>& root)
{
    CSS_NUMERIC_RETURN_IF_EXCEPTION(values, reifyMathExpressions(root->children));
    return convertToExceptionOrNumericValue(CSSMathSum::create(WTFMove(values)));
}

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalc::IndirectNode<CSSCalc::Product>& root)
{
    CSS_NUMERIC_RETURN_IF_EXCEPTION(values, reifyMathExpressions(root->children));
    return convertToExceptionOrNumericValue(CSSMathProduct::create(WTFMove(values)));
}

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalc::IndirectNode<CSSCalc::Negate>& root)
{
    CSS_NUMERIC_RETURN_IF_EXCEPTION(child, CSSNumericValue::reifyMathExpression(root->a));
    return convertToExceptionOrNumericValue(CSSMathNegate::create(WTFMove(child)));
}

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalc::IndirectNode<CSSCalc::Invert>& root)
{
    CSS_NUMERIC_RETURN_IF_EXCEPTION(child, CSSNumericValue::reifyMathExpression(root->a));
    return convertToExceptionOrNumericValue(CSSMathInvert::create(WTFMove(child)));
}

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalc::IndirectNode<CSSCalc::Min>& root)
{
    CSS_NUMERIC_RETURN_IF_EXCEPTION(values, reifyMathExpressions(root->children));
    return convertToExceptionOrNumericValue(CSSMathMin::create(WTFMove(values)));
}

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalc::IndirectNode<CSSCalc::Max>& root)
{
    CSS_NUMERIC_RETURN_IF_EXCEPTION(values, reifyMathExpressions(root->children));
    return convertToExceptionOrNumericValue(CSSMathMax::create(WTFMove(values)));
}

static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalc::IndirectNode<CSSCalc::Clamp>& root)
{
    CSS_NUMERIC_RETURN_IF_EXCEPTION(min, CSSNumericValue::reifyMathExpression(root->min));
    CSS_NUMERIC_RETURN_IF_EXCEPTION(val, CSSNumericValue::reifyMathExpression(root->val));
    CSS_NUMERIC_RETURN_IF_EXCEPTION(max, CSSNumericValue::reifyMathExpression(root->max));
    return convertToExceptionOrNumericValue(CSSMathClamp::create(WTFMove(min), WTFMove(val), WTFMove(max)));
}

template<typename Op> static ExceptionOr<Ref<CSSNumericValue>> reifyMathExpression(const CSSCalc::IndirectNode<Op>&)
{
    return Exception { ExceptionCode::UnknownError };
}

// https://drafts.css-houdini.org/css-typed-om/#reify-a-math-expression
ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::reifyMathExpression(const CSSCalc::Tree& tree)
{
    return CSSNumericValue::reifyMathExpression(tree.root);
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::reifyMathExpression(const CSSCalc::Child& root)
{
    return WTF::switchOn(root, [](const auto& child) { return WebCore::reifyMathExpression(child); });
}

ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::reifyMathExpression(const CSSCalc::ChildOrNone& root)
{
    return WTF::switchOn(root,
        [](const CSSCalc::Child& child) -> ExceptionOr<Ref<CSSNumericValue>> { return CSSNumericValue::reifyMathExpression(child); },
        [](const CSS::NoneRaw&) -> ExceptionOr<Ref<CSSNumericValue>> { return Exception { ExceptionCode::UnknownError }; }
    );
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
                return Exception { ExceptionCode::RangeError };
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
        return WebCore::negate(rectifyNumberish(WTFMove(numberish)));
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
        invertedValues.append(WTFMove(inverted));
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
        return Exception { ExceptionCode::SyntaxError };

    auto sumValue = toSumValue();
    if (!sumValue || sumValue->size() != 1)
        return Exception { ExceptionCode::TypeError };

    auto& addend = (*sumValue)[0];
    auto unconverted = createCSSUnitValueFromAddend(addend);
    if (!unconverted)
        return Exception { ExceptionCode::TypeError };

    auto converted = unconverted->convertTo(unit);
    if (!converted)
        return Exception { ExceptionCode::TypeError };
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
            return Exception { ExceptionCode::SyntaxError, "Invalid unit parameter"_s };
        parsedUnits.append(parsedUnit);
    }
    auto sumValue = toSumValue();
    if (!sumValue)
        return Exception { ExceptionCode::TypeError, "Could not create a sum value"_s };

    Vector<Ref<CSSNumericValue>> values;
    values.reserveInitialCapacity(sumValue->size());
    for (auto& addend : *sumValue) {
        auto cssUnitValue = createCSSUnitValueFromAddend(addend);
        if (!cssUnitValue)
            return Exception { ExceptionCode::TypeError, "Could not create CSSUnitValue"_s };
        values.append(cssUnitValue.releaseNonNull());
    }

    if (parsedUnits.isEmpty()) {
        std::sort(values.begin(), values.end(), [](auto& a, auto& b) {
            return strcmp(downcast<CSSUnitValue>(a)->unitSerialization().characters(), downcast<CSSUnitValue>(b)->unitSerialization().characters()) < 0;
        });
        return CSSMathSum::create(WTFMove(values));
    }

    Vector<Ref<CSSNumericValue>> result;
    for (auto& parsedUnit : parsedUnits) {
        auto temp = CSSUnitValue::create(0, parsedUnit);
        for (size_t i = 0; i < values.size();) {
            auto value = downcast<CSSUnitValue>(values[i]);
            if (auto convertedValue = value->convertTo(parsedUnit)) {
                temp->setValue(temp->value() + convertedValue->value());
                values.remove(i);
            } else
                ++i;
        }
        result.append(WTFMove(temp));
    }

    if (!values.isEmpty())
        return Exception { ExceptionCode::TypeError, "Failed to convert all values"_s };

    return CSSMathSum::create(WTFMove(result));
}

// https://drafts.css-houdini.org/css-typed-om/#dom-cssnumericvalue-parse
ExceptionOr<Ref<CSSNumericValue>> CSSNumericValue::parse(Document& document, String&& cssText)
{
    CSSTokenizer tokenizer(cssText);
    auto range = tokenizer.tokenRange();
    range.consumeWhitespace();
    if (range.atEnd())
        return Exception { ExceptionCode::SyntaxError, "Failed to parse CSS text"_s };
    const CSSParserToken* componentValueStart = &range.peek();
    range.consumeComponentValue();
    const CSSParserToken* componentValueEnd = &range.peek();
    range.consumeWhitespace();
    if (!range.atEnd())
        return Exception { ExceptionCode::SyntaxError, "Failed to parse CSS text"_s };

    auto componentValueRange = range.makeSubRange(componentValueStart, componentValueEnd);
    // https://drafts.css-houdini.org/css-typed-om/#reify-a-numeric-value
    switch (componentValueRange.peek().type()) {
    case CSSParserTokenType::DimensionToken:
    case CSSParserTokenType::NumberToken:
    case CSSParserTokenType::PercentageToken: {
        auto& token = componentValueRange.consumeIncludingWhitespace();
        if (token.type() == CSSParserTokenType::DimensionToken && !CSSNumericType::create(token.unitType()))
            return Exception { ExceptionCode::SyntaxError, "Failed to parse CSS text"_s };
        return convertToExceptionOrNumericValue(CSSUnitValue::create(token.numericValue(), token.unitType()));
    }
    case CSSParserTokenType::FunctionToken: {
        auto functionID = componentValueRange.peek().functionId();
        if (functionID == CSSValueCalc || functionID == CSSValueMin || functionID == CSSValueMax || functionID == CSSValueClamp) {
            // FIXME: The spec is unclear on what context to use when parsing in CSSNumericValue so for the time-being, we use `Category::LengthPercentage`, as it is the most permissive.
            // See https://github.com/w3c/csswg-drafts/issues/10753

            auto parserContext = CSSParserContext { document };
            auto parserOptions = CSSCalc::ParserOptions {
                .category = Calculation::Category::LengthPercentage,
                .range = CSS::All,
                .allowedSymbols = { },
                .propertyOptions = { },
            };
            auto simplificationOptions = CSSCalc::SimplificationOptions {
                .category = Calculation::Category::LengthPercentage,
                .conversionData = std::nullopt,
                .symbolTable = { },
                .allowZeroValueLengthRemovalFromSum = false,
                .allowUnresolvedUnits = false,
                .allowNonMatchingUnits = false
            };
            auto tree = CSSCalc::parseAndSimplify(componentValueRange, parserContext, parserOptions, simplificationOptions);
            if (!tree)
                return Exception { ExceptionCode::SyntaxError, "Failed to parse CSS text"_s };

            return reifyMathExpression(*tree);
        }
        break;
    }
    default:
        break;
    }
    return Exception { ExceptionCode::SyntaxError, "Failed to parse CSS text"_s };
}

} // namespace WebCore

#undef CSS_NUMERIC_RETURN_IF_EXCEPTION
