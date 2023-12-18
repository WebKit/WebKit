/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "CSSStyleValueFactory.h"

#include "CSSCalcValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSKeywordValue.h"
#include "CSSNumericFactory.h"
#include "CSSParser.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPropertyParser.h"
#include "CSSStyleImageValue.h"
#include "CSSStyleValue.h"
#include "CSSTokenizer.h"
#include "CSSTransformListValue.h"
#include "CSSTransformValue.h"
#include "CSSUnitValue.h"
#include "CSSUnparsedValue.h"
#include "CSSValueList.h"
#include "CSSVariableData.h"
#include "CSSVariableReferenceValue.h"
#include "StylePropertiesInlines.h"
#include "StylePropertyShorthand.h"
#include <wtf/FixedVector.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>

namespace WebCore {

RefPtr<CSSStyleValue> CSSStyleValueFactory::constructStyleValueForShorthandSerialization(const String& serialization, const CSSParserContext& parserContext)
{
    if (serialization.isNull())
        return nullptr;

    CSSTokenizer tokenizer(serialization);
    if (serialization.contains("var("_s))
        return CSSUnparsedValue::create(tokenizer.tokenRange());
    return CSSStyleValue::create(CSSVariableReferenceValue::create(tokenizer.tokenRange(), parserContext));
}

ExceptionOr<RefPtr<CSSValue>> CSSStyleValueFactory::extractCSSValue(const CSSPropertyID& propertyID, const String& cssText, const CSSParserContext& parserContext)
{
    auto styleDeclaration = MutableStyleProperties::create();
    
    constexpr bool important = true;
    CSSParser::ParseResult parseResult = CSSParser::parseValue(styleDeclaration, propertyID, cssText, important, parserContext);

    if (parseResult == CSSParser::ParseResult::Error)
        return Exception { ExceptionCode::TypeError, makeString(cssText, " cannot be parsed.") };

    return styleDeclaration->getPropertyCSSValue(propertyID);
}

ExceptionOr<RefPtr<CSSStyleValue>> CSSStyleValueFactory::extractShorthandCSSValues(const CSSPropertyID& propertyID, const String& cssText, const CSSParserContext& parserContext)
{
    auto styleDeclaration = MutableStyleProperties::create();

    constexpr bool important = true;
    CSSParser::ParseResult parseResult = CSSParser::parseValue(styleDeclaration, propertyID, cssText, important, parserContext);

    if (parseResult == CSSParser::ParseResult::Error)
        return Exception { ExceptionCode::TypeError, makeString(cssText, " cannot be parsed.") };

    return constructStyleValueForShorthandSerialization(styleDeclaration->getPropertyValue(propertyID), parserContext);
}

ExceptionOr<Ref<CSSUnparsedValue>> CSSStyleValueFactory::extractCustomCSSValues(const String& cssText)
{
    if (cssText.isEmpty())
        return Exception { ExceptionCode::TypeError, "Value cannot be parsed"_s };

    CSSTokenizer tokenizer(cssText);
    return { CSSUnparsedValue::create(tokenizer.tokenRange()) };
}

// https://www.w3.org/TR/css-typed-om-1/#cssstylevalue
ExceptionOr<Vector<Ref<CSSStyleValue>>> CSSStyleValueFactory::parseStyleValue(const AtomString& cssProperty, const String& cssText, bool parseMultiple, const CSSParserContext& parserContext)
{
    // Extract the CSSValue from cssText given cssProperty
    if (isCustomPropertyName(cssProperty)) {
        auto result = extractCustomCSSValues(cssText);
        if (result.hasException())
            return result.releaseException();
        return Vector { Ref<CSSStyleValue> { result.releaseReturnValue() } };
    }

    auto property = cssProperty.convertToASCIILowercase();
    auto propertyID = cssPropertyID(property);

    if (propertyID == CSSPropertyInvalid)
        return Exception { ExceptionCode::TypeError, "Property String is not a valid CSS property."_s };

    if (isShorthand(propertyID)) {
        auto result = extractShorthandCSSValues(propertyID, cssText, parserContext);
        if (result.hasException())
            return result.releaseException();
        auto cssValue = result.releaseReturnValue();
        if (!cssValue)
            return Vector<Ref<CSSStyleValue>> { };
        return Vector { cssValue.releaseNonNull() };
    }

    auto result = extractCSSValue(propertyID, cssText, parserContext);
    if (result.hasException())
        return result.releaseException();

    Vector<Ref<CSSValue>> cssValues;
    if (auto cssValue = result.releaseReturnValue()) {
        // https://drafts.css-houdini.org/css-typed-om/#subdivide-into-iterations
        if (CSSProperty::isListValuedProperty(propertyID)) {
            if (auto* values = dynamicDowncast<CSSValueContainingVector>(*cssValue)) {
                for (auto& value : *values)
                    cssValues.append(Ref { const_cast<CSSValue&>(value) });
            }
        }
        if (cssValues.isEmpty())
            cssValues.append(cssValue.releaseNonNull());
    }

    Vector<Ref<CSSStyleValue>> results;

    for (auto& cssValue : cssValues) {
        auto reifiedValue = reifyValue(WTFMove(cssValue), propertyID);
        if (reifiedValue.hasException())
            return reifiedValue.releaseException();

        results.append(reifiedValue.releaseReturnValue());
        
        if (!parseMultiple)
            break;
    }
    
    return results;
}

static bool mayConvertCSSValueListToSingleValue(std::optional<CSSPropertyID> propertyID)
{
    if (!propertyID)
        return true;

    // Even though the CSS Parser uses a CSSValueList to represent these, they are not
    // really lists and CSS-Typed-OM does not expect them to treat them as such.
    return *propertyID != CSSPropertyGridRowStart
        && *propertyID != CSSPropertyGridRowEnd
        && *propertyID != CSSPropertyGridColumnStart
        && *propertyID != CSSPropertyGridColumnEnd;
}

ExceptionOr<Ref<CSSStyleValue>> CSSStyleValueFactory::reifyValue(const CSSValue& cssValue, std::optional<CSSPropertyID> propertyID, Document* document)
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(cssValue)) {
        if (primitiveValue->isCalculated()) {
            auto* calcValue = primitiveValue->cssCalcValue();
            auto result = CSSNumericValue::reifyMathExpression(calcValue->expressionNode());
            if (result.hasException())
                return result.releaseException();
            return static_reference_cast<CSSStyleValue>(result.releaseReturnValue());
        }
        switch (primitiveValue->primitiveType()) {
        case CSSUnitType::CSS_NUMBER:
        case CSSUnitType::CSS_INTEGER:
            return Ref<CSSStyleValue> { CSSNumericFactory::number(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_PERCENTAGE:
            return Ref<CSSStyleValue> { CSSNumericFactory::percent(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_EM:
            return Ref<CSSStyleValue> { CSSNumericFactory::em(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_REM:
            return Ref<CSSStyleValue> { CSSNumericFactory::rem(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_EX:
            return Ref<CSSStyleValue> { CSSNumericFactory::ex(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_CAP:
            return Ref<CSSStyleValue> { CSSNumericFactory::cap(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_RCAP:
            return Ref<CSSStyleValue> { CSSNumericFactory::rcap(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_CH:
            return Ref<CSSStyleValue> { CSSNumericFactory::ch(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_RCH:
            return Ref<CSSStyleValue> { CSSNumericFactory::rch(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_IC:
            return Ref<CSSStyleValue> { CSSNumericFactory::ic(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_RIC:
            return Ref<CSSStyleValue> { CSSNumericFactory::ric(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_LH:
            return Ref<CSSStyleValue> { CSSNumericFactory::lh(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_RLH:
            return Ref<CSSStyleValue> { CSSNumericFactory::rlh(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_VW:
            return Ref<CSSStyleValue> { CSSNumericFactory::vw(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_VH:
            return Ref<CSSStyleValue> { CSSNumericFactory::vh(primitiveValue->doubleValue()) };
        // FIXME: Add CSSNumericFactory::vi & ::vb
        case CSSUnitType::CSS_VMIN:
            return Ref<CSSStyleValue> { CSSNumericFactory::vmin(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_VMAX:
            return Ref<CSSStyleValue> { CSSNumericFactory::vmax(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_CM:
            return Ref<CSSStyleValue> { CSSNumericFactory::cm(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_MM:
            return Ref<CSSStyleValue> { CSSNumericFactory::mm(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_Q:
            return Ref<CSSStyleValue> { CSSNumericFactory::q(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_IN:
            return Ref<CSSStyleValue> { CSSNumericFactory::in(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_PT:
            return Ref<CSSStyleValue> { CSSNumericFactory::pt(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_PC:
            return Ref<CSSStyleValue> { CSSNumericFactory::pc(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_PX:
            return Ref<CSSStyleValue> { CSSNumericFactory::px(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_DEG:
            return Ref<CSSStyleValue> { CSSNumericFactory::deg(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_GRAD:
            return Ref<CSSStyleValue> { CSSNumericFactory::grad(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_RAD:
            return Ref<CSSStyleValue> { CSSNumericFactory::rad(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_TURN:
            return Ref<CSSStyleValue> { CSSNumericFactory::turn(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_S:
            return Ref<CSSStyleValue> { CSSNumericFactory::s(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_MS:
            return Ref<CSSStyleValue> { CSSNumericFactory::ms(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_HZ:
            return Ref<CSSStyleValue> { CSSNumericFactory::hz(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_KHZ:
            return Ref<CSSStyleValue> { CSSNumericFactory::kHz(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_DPI:
            return Ref<CSSStyleValue> { CSSNumericFactory::dpi(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_DPCM:
            return Ref<CSSStyleValue> { CSSNumericFactory::dpcm(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_DPPX:
            return Ref<CSSStyleValue> { CSSNumericFactory::dppx(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_FR:
            return Ref<CSSStyleValue> { CSSNumericFactory::fr(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_CQW:
            return Ref<CSSStyleValue> { CSSNumericFactory::cqw(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_CQH:
            return Ref<CSSStyleValue> { CSSNumericFactory::cqh(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_CQI:
            return Ref<CSSStyleValue> { CSSNumericFactory::cqi(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_CQB:
            return Ref<CSSStyleValue> { CSSNumericFactory::cqb(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_CQMIN:
            return Ref<CSSStyleValue> { CSSNumericFactory::cqmin(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_CQMAX:
            return Ref<CSSStyleValue> { CSSNumericFactory::cqmax(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_IDENT:
            // Per the specification, the CSSKeywordValue's value slot should be set to the serialization
            // of the identifier. As a result, the identifier will be lowercase:
            // https://drafts.css-houdini.org/css-typed-om-1/#reify-ident
            return static_reference_cast<CSSStyleValue>(CSSKeywordValue::rectifyKeywordish(primitiveValue->cssText()));
        default:
            break;
        }
    } else if (auto* imageValue = dynamicDowncast<CSSImageValue>(cssValue))
        return Ref<CSSStyleValue> { CSSStyleImageValue::create(const_cast<CSSImageValue&>(*imageValue), document) };
    else if (auto* referenceValue = dynamicDowncast<CSSVariableReferenceValue>(cssValue)) {
        return Ref<CSSStyleValue> { CSSUnparsedValue::create(referenceValue->data().tokenRange()) };
    } else if (auto* substitutionValue = dynamicDowncast<CSSPendingSubstitutionValue>(cssValue)) {
        return Ref<CSSStyleValue> { CSSUnparsedValue::create(substitutionValue->shorthandValue().data().tokenRange()) };
    } else if (auto* customPropertyValue = dynamicDowncast<CSSCustomPropertyValue>(cssValue)) {
        // FIXME: remove CSSStyleValue::create(WTFMove(cssValue)), add reification control flow
        return WTF::switchOn(customPropertyValue->value(), [&](const std::monostate&) {
            return ExceptionOr<Ref<CSSStyleValue>> { CSSStyleValue::create(Ref(const_cast<CSSValue&>(cssValue))) };
        }, [&](const Ref<CSSVariableReferenceValue>& value) {
            return reifyValue(value, propertyID, document);
        }, [&](Ref<CSSVariableData>& value) {
            return reifyValue(CSSVariableReferenceValue::create(WTFMove(value)), propertyID);
        }, [&](const CSSValueID&) {
            return ExceptionOr<Ref<CSSStyleValue>> { CSSStyleValue::create(Ref(const_cast<CSSValue&>(cssValue))) };
        }, [&](const CSSCustomPropertyValue::SyntaxValue& syntaxValue) -> ExceptionOr<Ref<CSSStyleValue>> {
            if (auto styleValue = constructStyleValueForCustomPropertySyntaxValue(syntaxValue))
                return { *styleValue };
            CSSTokenizer tokenizer(customPropertyValue->customCSSText());
            return { CSSUnparsedValue::create(tokenizer.tokenRange()) };
        }, [&](const CSSCustomPropertyValue::SyntaxValueList& syntaxValueList) -> ExceptionOr<Ref<CSSStyleValue>> {
            if (auto styleValue = constructStyleValueForCustomPropertySyntaxValue(syntaxValueList.values[0]))
                return { *styleValue };
            CSSTokenizer tokenizer(customPropertyValue->customCSSText());
            return { CSSUnparsedValue::create(tokenizer.tokenRange()) };
        }, [&](auto&) {
            CSSTokenizer tokenizer(customPropertyValue->customCSSText());
            return ExceptionOr<Ref<CSSStyleValue>> { CSSUnparsedValue::create(tokenizer.tokenRange()) };
        });
    } else if (auto* transformList = dynamicDowncast<CSSTransformListValue>(cssValue)) {
        auto transformValue = CSSTransformValue::create(*transformList);
        if (transformValue.hasException())
            return transformValue.releaseException();
        return Ref<CSSStyleValue> { transformValue.releaseReturnValue() };
    } else if (auto* valueList = dynamicDowncast<CSSValueList>(cssValue)) {
        // Reifying the first value in value list.
        // FIXME: Verify this is the expected behavior.
        // Refer to LayoutTests/imported/w3c/web-platform-tests/css/css-typed-om/the-stylepropertymap/inline/get.html
        if (!valueList->length())
            return Exception { ExceptionCode::TypeError, "The CSSValueList should not be empty."_s };
        if ((valueList->length() == 1 && mayConvertCSSValueListToSingleValue(propertyID)) || (propertyID && CSSProperty::isListValuedProperty(*propertyID)))
            return reifyValue((*valueList)[0], propertyID, document);
    }
    
    return CSSStyleValue::create(Ref(const_cast<CSSValue&>(cssValue)));
}

RefPtr<CSSStyleValue> CSSStyleValueFactory::constructStyleValueForCustomPropertySyntaxValue(const CSSCustomPropertyValue::SyntaxValue& syntaxValue)
{
    return WTF::switchOn(syntaxValue, [&](const Length& length) -> RefPtr<CSSStyleValue> {
        if (length.isFixed())
            return CSSUnitValue::create(length.value(), CSSUnitType::CSS_PX);
        if (length.isPercent())
            return CSSUnitValue::create(length.percent(), CSSUnitType::CSS_PERCENTAGE);
        // FIXME: Calc.
        return nullptr;
    }, [&](const CSSCustomPropertyValue::NumericSyntaxValue& numericValue) -> RefPtr<CSSStyleValue>  {
        return CSSUnitValue::create(numericValue.value, numericValue.unitType);
    }, [&](const StyleColor& colorValue) -> RefPtr<CSSStyleValue> {
        if (colorValue.isCurrentColor())
            return CSSKeywordValue::rectifyKeywordish(nameLiteral(CSSValueCurrentcolor));
        return CSSStyleValue::create(CSSValuePool::singleton().createColorValue(colorValue.absoluteColor()));
    }, [&](const URL& urlValue) -> RefPtr<CSSStyleValue> {
        return CSSStyleValue::create(CSSPrimitiveValue::createURI(urlValue.string()));
    }, [&](const String& identValue) -> RefPtr<CSSStyleValue> {
        return CSSKeywordValue::rectifyKeywordish(identValue);
    }, [&](const RefPtr<StyleImage>&) -> RefPtr<CSSStyleValue>  {
        // FIXME: <image>.
        return nullptr;
    }, [&](const CSSCustomPropertyValue::TransformSyntaxValue&) -> RefPtr<CSSStyleValue>  {
        // FIXME: <transform>, <transform-list>.
        return nullptr;
    });
}

ExceptionOr<Vector<Ref<CSSStyleValue>>> CSSStyleValueFactory::vectorFromStyleValuesOrStrings(const AtomString& property, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&& values, const CSSParserContext& parserContext)
{
    Vector<Ref<CSSStyleValue>> styleValues;
    for (auto&& value : WTFMove(values)) {
        std::optional<Exception> exception;
        switchOn(WTFMove(value), [&](RefPtr<CSSStyleValue>&& styleValue) {
            ASSERT(styleValue);
            styleValues.append(styleValue.releaseNonNull());
        }, [&](String&& string) {
            constexpr bool parseMultiple = true;
            auto result = CSSStyleValueFactory::parseStyleValue(property, string, parseMultiple, parserContext);
            if (result.hasException()) {
                exception = result.releaseException();
                return;
            }
            styleValues.appendVector(result.releaseReturnValue());
        });
        if (exception)
            return { WTFMove(*exception) };
    }
    return { WTFMove(styleValues) };
}

} // namespace WebCore
