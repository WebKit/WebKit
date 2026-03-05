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

#include "CSSAppleColorFilterValue.h"
#include "CSSBoxShadowPropertyValue.h"
#include "CSSCalcValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSEasingFunctionValue.h"
#include "CSSFilterValue.h"
#include "CSSKeywordValue.h"
#include "CSSNumericFactory.h"
#include "CSSParser.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPropertyParser.h"
#include "CSSSerializationContext.h"
#include "CSSStyleImageValue.h"
#include "CSSStyleValue.h"
#include "CSSTextShadowPropertyValue.h"
#include "CSSTokenizer.h"
#include "CSSTransformListValue.h"
#include "CSSTransformValue.h"
#include "CSSURLValue.h"
#include "CSSUnitValue.h"
#include "CSSUnparsedValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "CSSVariableData.h"
#include "CSSVariableReferenceValue.h"
#include "ExceptionOr.h"
#include "RenderStyle.h"
#include "ScriptWrappableInlines.h"
#include "StylePropertiesInlines.h"
#include "StylePropertyShorthand.h"
#include "StyleURL.h"
#include <wtf/FixedVector.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>

namespace WebCore {

RefPtr<CSSStyleValue> CSSStyleValueFactory::constructStyleValueForShorthandSerialization(Document& document, const String& serialization)
{
    if (serialization.isNull())
        return nullptr;

    CSSTokenizer tokenizer(serialization);
    if (serialization.contains("var("_s))
        return CSSUnparsedValue::create(tokenizer.tokenRange());
    return CSSStyleValue::create(CSSVariableReferenceValue::create(tokenizer.tokenRange(), { document }));
}

ExceptionOr<RefPtr<CSSValue>> CSSStyleValueFactory::extractCSSValue(Document& document, const CSSPropertyID& propertyID, const String& cssText)
{
    auto styleDeclaration = MutableStyleProperties::create();
    auto parseResult = CSSParser::parseValue(styleDeclaration, propertyID, cssText, IsImportant::Yes, { document });

    if (parseResult == CSSParser::ParseResult::Error)
        return Exception { ExceptionCode::TypeError, makeString(cssText, " cannot be parsed."_s) };

    return styleDeclaration->getPropertyCSSValue(propertyID);
}

ExceptionOr<RefPtr<CSSStyleValue>> CSSStyleValueFactory::extractShorthandCSSValues(Document& document, const CSSPropertyID& propertyID, const String& cssText)
{
    auto styleDeclaration = MutableStyleProperties::create();
    auto parseResult = CSSParser::parseValue(styleDeclaration, propertyID, cssText, IsImportant::Yes, { document });

    if (parseResult == CSSParser::ParseResult::Error)
        return Exception { ExceptionCode::TypeError, makeString(cssText, " cannot be parsed."_s) };

    return constructStyleValueForShorthandSerialization(document, styleDeclaration->getPropertyValue(propertyID));
}

ExceptionOr<Ref<CSSUnparsedValue>> CSSStyleValueFactory::extractCustomCSSValues(const String& cssText)
{
    CSSTokenizer tokenizer(cssText);
    return { CSSUnparsedValue::create(tokenizer.tokenRange()) };
}

// https://www.w3.org/TR/css-typed-om-1/#cssstylevalue
ExceptionOr<Vector<Ref<CSSStyleValue>>> CSSStyleValueFactory::parseStyleValue(Document& document, const AtomString& cssProperty, const String& cssText, bool parseMultiple)
{
    if (cssText.isEmpty())
        return Exception { ExceptionCode::TypeError, "Value cannot be parsed."_s };

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
        auto result = extractShorthandCSSValues(document, propertyID, cssText);
        if (result.hasException())
            return result.releaseException();
        auto cssValue = result.releaseReturnValue();
        if (!cssValue)
            return Vector<Ref<CSSStyleValue>> { };
        return Vector { cssValue.releaseNonNull() };
    }

    auto result = extractCSSValue(document, propertyID, cssText);
    if (result.hasException())
        return result.releaseException();

    Vector<Ref<CSSValue>> cssValues;
    if (auto cssValue = result.releaseReturnValue()) {
        // https://drafts.css-houdini.org/css-typed-om/#subdivide-into-iterations
        if (CSSProperty::isListValuedProperty(propertyID)) {
            if (auto* values = dynamicDowncast<CSSValueContainingVector>(*cssValue)) {
                for (Ref value : *values)
                    cssValues.append(Ref { const_cast<CSSValue&>(value.get()) });
            }
        }
        if (cssValues.isEmpty())
            cssValues.append(cssValue.releaseNonNull());
    }

    Vector<Ref<CSSStyleValue>> results;

    for (auto& cssValue : cssValues) {
        auto reifiedValue = reifyValue(document, WTF::move(cssValue), propertyID);
        if (reifiedValue.hasException())
            return reifiedValue.releaseException();

        results.append(reifiedValue.releaseReturnValue());
        
        if (!parseMultiple)
            break;
    }
    
    return results;
}

static bool NODELETE mayConvertCSSValueListToSingleValue(std::optional<CSSPropertyID> propertyID)
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

ExceptionOr<Ref<CSSStyleValue>> CSSStyleValueFactory::reifyValue(Document& document, const CSSValue& cssValue, std::optional<CSSPropertyID> propertyID)
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(cssValue)) {
        if (RefPtr calcValue = primitiveValue->cssCalcValue()) {
            auto result = CSSNumericValue::reifyMathExpression(calcValue->tree());
            if (result.hasException())
                return result.releaseException();
            return upcast<CSSStyleValue>(result.releaseReturnValue());
        }
        switch (primitiveValue->primitiveType()) {
        case CSSUnitType::CSS_NUMBER:
        case CSSUnitType::CSS_INTEGER:
            return Ref<CSSStyleValue> { CSSNumericFactory::number(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_PERCENTAGE:
            return Ref<CSSStyleValue> { CSSNumericFactory::percent(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_EM:
            return Ref<CSSStyleValue> { CSSNumericFactory::em(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_REM:
            return Ref<CSSStyleValue> { CSSNumericFactory::rem(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_EX:
            return Ref<CSSStyleValue> { CSSNumericFactory::ex(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_CAP:
            return Ref<CSSStyleValue> { CSSNumericFactory::cap(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_RCAP:
            return Ref<CSSStyleValue> { CSSNumericFactory::rcap(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_CH:
            return Ref<CSSStyleValue> { CSSNumericFactory::ch(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_RCH:
            return Ref<CSSStyleValue> { CSSNumericFactory::rch(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_IC:
            return Ref<CSSStyleValue> { CSSNumericFactory::ic(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_RIC:
            return Ref<CSSStyleValue> { CSSNumericFactory::ric(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_LH:
            return Ref<CSSStyleValue> { CSSNumericFactory::lh(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_RLH:
            return Ref<CSSStyleValue> { CSSNumericFactory::rlh(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_VW:
            return Ref<CSSStyleValue> { CSSNumericFactory::vw(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_VH:
            return Ref<CSSStyleValue> { CSSNumericFactory::vh(primitiveValue->valueNoConversionDataRequired<double>()) };
        // FIXME: Add CSSNumericFactory::vi & ::vb
        case CSSUnitType::CSS_VMIN:
            return Ref<CSSStyleValue> { CSSNumericFactory::vmin(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_VMAX:
            return Ref<CSSStyleValue> { CSSNumericFactory::vmax(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_CM:
            return Ref<CSSStyleValue> { CSSNumericFactory::cm(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_MM:
            return Ref<CSSStyleValue> { CSSNumericFactory::mm(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_Q:
            return Ref<CSSStyleValue> { CSSNumericFactory::q(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_IN:
            return Ref<CSSStyleValue> { CSSNumericFactory::in(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_PT:
            return Ref<CSSStyleValue> { CSSNumericFactory::pt(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_PC:
            return Ref<CSSStyleValue> { CSSNumericFactory::pc(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_PX:
            return Ref<CSSStyleValue> { CSSNumericFactory::px(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_DEG:
            return Ref<CSSStyleValue> { CSSNumericFactory::deg(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_GRAD:
            return Ref<CSSStyleValue> { CSSNumericFactory::grad(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_RAD:
            return Ref<CSSStyleValue> { CSSNumericFactory::rad(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_TURN:
            return Ref<CSSStyleValue> { CSSNumericFactory::turn(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_S:
            return Ref<CSSStyleValue> { CSSNumericFactory::s(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_MS:
            return Ref<CSSStyleValue> { CSSNumericFactory::ms(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_HZ:
            return Ref<CSSStyleValue> { CSSNumericFactory::hz(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_KHZ:
            return Ref<CSSStyleValue> { CSSNumericFactory::kHz(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_DPI:
            return Ref<CSSStyleValue> { CSSNumericFactory::dpi(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_DPCM:
            return Ref<CSSStyleValue> { CSSNumericFactory::dpcm(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_DPPX:
            return Ref<CSSStyleValue> { CSSNumericFactory::dppx(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_FR:
            return Ref<CSSStyleValue> { CSSNumericFactory::fr(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_CQW:
            return Ref<CSSStyleValue> { CSSNumericFactory::cqw(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_CQH:
            return Ref<CSSStyleValue> { CSSNumericFactory::cqh(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_CQI:
            return Ref<CSSStyleValue> { CSSNumericFactory::cqi(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_CQB:
            return Ref<CSSStyleValue> { CSSNumericFactory::cqb(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_CQMIN:
            return Ref<CSSStyleValue> { CSSNumericFactory::cqmin(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_CQMAX:
            return Ref<CSSStyleValue> { CSSNumericFactory::cqmax(primitiveValue->valueNoConversionDataRequired<double>()) };
        case CSSUnitType::CSS_IDENT:
            // Per the specification, the CSSKeywordValue's value slot should be set to the serialization
            // of the identifier. As a result, the identifier will be lowercase:
            // https://drafts.css-houdini.org/css-typed-om-1/#reify-ident
            return upcast<CSSStyleValue>(CSSKeywordValue::rectifyKeywordish(primitiveValue->cssText(CSS::defaultSerializationContext())));
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
        // FIXME: remove CSSStyleValue::create(WTF::move(cssValue)), add reification control flow
        return WTF::switchOn(customPropertyValue->value(),
            [&](const Ref<CSSVariableReferenceValue>& value) {
                return reifyValue(document, value, propertyID);
            },
            [&](const Ref<CSSVariableData>& value) {
                return reifyValue(document, CSSVariableReferenceValue::create(value.copyRef()), propertyID);
            },
            [&](const CSSWideKeyword&) {
                return ExceptionOr<Ref<CSSStyleValue>> { CSSStyleValue::create(Ref(const_cast<CSSValue&>(cssValue))) };
            }
        );
    } else if (RefPtr transformList = dynamicDowncast<CSSTransformListValue>(cssValue)) {
        auto transformValue = CSSTransformValue::create(transformList.releaseNonNull(), document);
        if (transformValue.hasException())
            return transformValue.releaseException();
        return Ref<CSSStyleValue> { transformValue.releaseReturnValue() };
    } else if (RefPtr property = dynamicDowncast<CSSFilterValue>(cssValue)) {
        return WTF::switchOn(property->filter(),
            [&](CSS::Keyword::None) -> ExceptionOr<Ref<CSSStyleValue>> {
                return upcast<CSSStyleValue>(CSSKeywordValue::rectifyKeywordish(nameLiteral(CSSValueNone)));
            },
            [&](const auto&) -> ExceptionOr<Ref<CSSStyleValue>> {
                return CSSStyleValue::create(Ref(const_cast<CSSValue&>(cssValue)));
            }
        );
    } else if (RefPtr property = dynamicDowncast<CSSAppleColorFilterValue>(cssValue)) {
        return WTF::switchOn(property->filter(),
            [&](CSS::Keyword::None) -> ExceptionOr<Ref<CSSStyleValue>> {
                return upcast<CSSStyleValue>(CSSKeywordValue::rectifyKeywordish(nameLiteral(CSSValueNone)));
            },
            [&](const auto&) -> ExceptionOr<Ref<CSSStyleValue>> {
                return CSSStyleValue::create(Ref(const_cast<CSSValue&>(cssValue)));
            }
        );
    } else if (RefPtr property = dynamicDowncast<CSSBoxShadowPropertyValue>(cssValue)) {
        return WTF::switchOn(property->shadow(),
            [&](CSS::Keyword::None) -> ExceptionOr<Ref<CSSStyleValue>> {
                return upcast<CSSStyleValue>(CSSKeywordValue::rectifyKeywordish(nameLiteral(CSSValueNone)));
            },
            [&](const auto&) -> ExceptionOr<Ref<CSSStyleValue>> {
                return CSSStyleValue::create(Ref(const_cast<CSSValue&>(cssValue)));
            }
        );
    } else if (RefPtr property = dynamicDowncast<CSSTextShadowPropertyValue>(cssValue)) {
        return WTF::switchOn(property->shadow(),
            [&](CSS::Keyword::None) -> ExceptionOr<Ref<CSSStyleValue>> {
                return upcast<CSSStyleValue>(CSSKeywordValue::rectifyKeywordish(nameLiteral(CSSValueNone)));
            },
            [&](const auto&) -> ExceptionOr<Ref<CSSStyleValue>> {
                return CSSStyleValue::create(Ref(const_cast<CSSValue&>(cssValue)));
            }
        );
    } else if (RefPtr property = dynamicDowncast<CSSEasingFunctionValue>(cssValue)) {
        return WTF::switchOn(property->easingFunction(),
            [&]<CSSValueID keyword>(Constant<keyword>) -> ExceptionOr<Ref<CSSStyleValue>> {
                return upcast<CSSStyleValue>(CSSKeywordValue::rectifyKeywordish(nameLiteral(keyword)));
            },
            [&](const auto&) -> ExceptionOr<Ref<CSSStyleValue>> {
                return CSSStyleValue::create(Ref(const_cast<CSSValue&>(cssValue)));
            }
        );
    } else if (auto* valueList = dynamicDowncast<CSSValueList>(cssValue)) {
        // Reifying the first value in value list.
        // FIXME: Verify this is the expected behavior.
        // Refer to LayoutTests/imported/w3c/web-platform-tests/css/css-typed-om/the-stylepropertymap/inline/get.html
        if (!valueList->length())
            return Exception { ExceptionCode::TypeError, "The CSSValueList should not be empty."_s };
        if ((valueList->length() == 1 && mayConvertCSSValueListToSingleValue(propertyID)) || (propertyID && CSSProperty::isListValuedProperty(*propertyID)))
            return reifyValue(document, protect((*valueList)[0]), propertyID);
    }

    return CSSStyleValue::create(Ref(const_cast<CSSValue&>(cssValue)));
}

ExceptionOr<Vector<Ref<CSSStyleValue>>> CSSStyleValueFactory::vectorFromStyleValuesOrStrings(Document& document, const AtomString& property, FixedVector<Variant<Ref<CSSStyleValue>, String>>&& values)
{
    Vector<Ref<CSSStyleValue>> styleValues;
    for (auto&& value : WTF::move(values)) {
        auto exception = switchOn(WTF::move(value),
            [&](Ref<CSSStyleValue>&& styleValue) -> std::optional<Exception> {
                styleValues.append(WTF::move(styleValue));
                return std::nullopt;
            },
            [&](String&& string) -> std::optional<Exception> {
                constexpr bool parseMultiple = true;
                auto result = CSSStyleValueFactory::parseStyleValue(document, property, string, parseMultiple);
                if (result.hasException())
                    return result.releaseException();
                styleValues.appendVector(result.releaseReturnValue());
                return std::nullopt;
            }
        );
        if (exception)
            return { WTF::move(*exception) };
    }
    return { WTF::move(styleValues) };
}

CSSStyleValueFactory::~CSSStyleValueFactory() = default;

} // namespace WebCore
