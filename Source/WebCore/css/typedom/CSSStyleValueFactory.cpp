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
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include <wtf/FixedVector.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>

namespace WebCore {

RefPtr<CSSStyleValue> CSSStyleValueFactory::constructStyleValueForShorthandSerialization(const String& serialization)
{
    if (serialization.isNull())
        return nullptr;

    CSSTokenizer tokenizer(serialization);
    if (serialization.contains("var("_s))
        return CSSUnparsedValue::create(tokenizer.tokenRange());
    return CSSStyleValue::create(CSSVariableReferenceValue::create(tokenizer.tokenRange(), strictCSSParserContext()));
}

ExceptionOr<RefPtr<CSSValue>> CSSStyleValueFactory::extractCSSValue(const CSSPropertyID& propertyID, const String& cssText)
{
    auto styleDeclaration = MutableStyleProperties::create();
    
    constexpr bool important = true;
    CSSParser::ParseResult parseResult = CSSParser::parseValue(styleDeclaration, propertyID, cssText, important, strictCSSParserContext());

    if (parseResult == CSSParser::ParseResult::Error)
        return Exception { TypeError, makeString(cssText, " cannot be parsed.")};

    return styleDeclaration->getPropertyCSSValue(propertyID);
}

ExceptionOr<RefPtr<CSSStyleValue>> CSSStyleValueFactory::extractShorthandCSSValues(const CSSPropertyID& propertyID, const String& cssText)
{
    auto styleDeclaration = MutableStyleProperties::create();

    constexpr bool important = true;
    CSSParser::ParseResult parseResult = CSSParser::parseValue(styleDeclaration, propertyID, cssText, important, strictCSSParserContext());

    if (parseResult == CSSParser::ParseResult::Error)
        return Exception { TypeError, makeString(cssText, " cannot be parsed.")};

    return constructStyleValueForShorthandSerialization(styleDeclaration->getPropertyValue(propertyID));
}

ExceptionOr<Ref<CSSUnparsedValue>> CSSStyleValueFactory::extractCustomCSSValues(const String& cssText)
{
    if (cssText.isEmpty())
        return Exception { TypeError, "Value cannot be parsed"_s };

    CSSTokenizer tokenizer(cssText);
    return { CSSUnparsedValue::create(tokenizer.tokenRange()) };
}

// https://www.w3.org/TR/css-typed-om-1/#cssstylevalue
ExceptionOr<Vector<Ref<CSSStyleValue>>> CSSStyleValueFactory::parseStyleValue(const AtomString& cssProperty, const String& cssText, bool parseMultiple)
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
        return Exception { TypeError, "Property String is not a valid CSS property."_s };

    if (isShorthandCSSProperty(propertyID)) {
        auto result = extractShorthandCSSValues(propertyID, cssText);
        if (result.hasException())
            return result.releaseException();
        auto cssValue = result.releaseReturnValue();
        if (!cssValue)
            return Vector<Ref<CSSStyleValue>> { };
        return Vector { cssValue.releaseNonNull() };
    }

    auto result = extractCSSValue(propertyID, cssText);
    if (result.hasException())
        return result.releaseException();

    Vector<Ref<CSSValue>> cssValues;
    if (auto cssValue = result.releaseReturnValue()) {
        // https://drafts.css-houdini.org/css-typed-om/#subdivide-into-iterations
        if (CSSProperty::isListValuedProperty(propertyID)) {
            if (auto* valueList = dynamicDowncast<CSSValueList>(*cssValue)) {
                for (size_t i = 0, length = valueList->length(); i < length; ++i)
                    cssValues.append(*valueList->item(i));
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

ExceptionOr<Ref<CSSStyleValue>> CSSStyleValueFactory::reifyValue(Ref<CSSValue> cssValue, std::optional<CSSPropertyID> propertyID, Document* document)
{
    if (is<CSSPrimitiveValue>(cssValue)) {
        auto primitiveValue = downcast<CSSPrimitiveValue>(cssValue.ptr());
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
        case CSSUnitType::CSS_EMS:
            return Ref<CSSStyleValue> { CSSNumericFactory::em(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_EXS:
            return Ref<CSSStyleValue> { CSSNumericFactory::ex(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_CHS:
            return Ref<CSSStyleValue> { CSSNumericFactory::ch(primitiveValue->doubleValue()) };
        // FIXME: Add CSSNumericFactory::ic
        case CSSUnitType::CSS_REMS:
            return Ref<CSSStyleValue> { CSSNumericFactory::rem(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_LHS:
            return Ref<CSSStyleValue> { CSSNumericFactory::lh(primitiveValue->doubleValue()) };
        case CSSUnitType::CSS_RLHS:
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
    } else if (is<CSSImageValue>(cssValue))
        return Ref<CSSStyleValue> { CSSStyleImageValue::create(downcast<CSSImageValue>(cssValue.get()), document) };
    else if (is<CSSVariableReferenceValue>(cssValue)) {
        return Ref<CSSStyleValue> { CSSUnparsedValue::create(downcast<CSSVariableReferenceValue>(cssValue.get()).data().tokenRange()) };
    } else if (is<CSSPendingSubstitutionValue>(cssValue)) {
        return Ref<CSSStyleValue> { CSSUnparsedValue::create(downcast<CSSPendingSubstitutionValue>(cssValue.get()).shorthandValue().data().tokenRange()) };
    } else if (is<CSSCustomPropertyValue>(cssValue)) {
        // FIXME: remove CSSStyleValue::create(WTFMove(cssValue)), add reification control flow
        return WTF::switchOn(downcast<CSSCustomPropertyValue>(cssValue.get()).value(), [&](const std::monostate&) {
            return ExceptionOr<Ref<CSSStyleValue>> { CSSStyleValue::create(WTFMove(cssValue)) };
        }, [&](const Ref<CSSVariableReferenceValue>& value) {
            return reifyValue(value, propertyID, document);
        }, [&](Ref<CSSVariableData>& value) {
            return reifyValue(CSSVariableReferenceValue::create(WTFMove(value)), propertyID);
        }, [&](const CSSValueID&) {
            return ExceptionOr<Ref<CSSStyleValue>> { CSSStyleValue::create(WTFMove(cssValue)) };
        }, [&](auto&) {
            CSSTokenizer tokenizer(downcast<CSSCustomPropertyValue>(cssValue.get()).customCSSText());
            return ExceptionOr<Ref<CSSStyleValue>> { CSSUnparsedValue::create(tokenizer.tokenRange()) };
        });
    } else if (is<CSSTransformListValue>(cssValue)) {
        auto& transformList = downcast<CSSTransformListValue>(cssValue.get());
        auto transformValue = CSSTransformValue::create(transformList);
        if (transformValue.hasException())
            return transformValue.releaseException();
        return Ref<CSSStyleValue> { transformValue.releaseReturnValue() };
    } else if (is<CSSValueList>(cssValue)) {
        // Reifying the first value in value list.
        // FIXME: Verify this is the expected behavior.
        // Refer to LayoutTests/imported/w3c/web-platform-tests/css/css-typed-om/the-stylepropertymap/inline/get.html
        auto valueList = downcast<CSSValueList>(cssValue.ptr());
        if (!valueList->length())
            return Exception { TypeError, "The CSSValueList should not be empty."_s };
        if (valueList->length() == 1 || (propertyID && CSSProperty::isListValuedProperty(*propertyID)))
            return reifyValue(*valueList->begin(), propertyID, document);
    }
    
    return CSSStyleValue::create(WTFMove(cssValue));
}

ExceptionOr<Vector<Ref<CSSStyleValue>>> CSSStyleValueFactory::vectorFromStyleValuesOrStrings(const AtomString& property, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&& values)
{
    Vector<Ref<CSSStyleValue>> styleValues;
    for (auto&& value : WTFMove(values)) {
        std::optional<Exception> exception;
        switchOn(WTFMove(value), [&](RefPtr<CSSStyleValue>&& styleValue) {
            ASSERT(styleValue);
            styleValues.append(styleValue.releaseNonNull());
        }, [&](String&& string) {
            constexpr bool parseMultiple = true;
            auto result = CSSStyleValueFactory::parseStyleValue(property, string, parseMultiple);
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
