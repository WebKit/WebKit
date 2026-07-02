/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DeprecatedCSSOMPrimitiveValue.h"

#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSSerializationContext.h"
#include "CSSWideKeyword.h"
#include "DeprecatedCSSOMCounter.h"
#include "DeprecatedCSSOMPrimitiveValueData.h"
#include "DeprecatedCSSOMRGBColor.h"
#include "DeprecatedCSSOMRect.h"

namespace WebCore {

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(Function<String(const CSS::SerializationContext&)>&& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { WTF::move(value) }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(const CSS::UnconstrainedPrimitiveNumericRaw& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { value }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(const CSS::UnevaluatedCalcBase& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { value }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(const CSS::ClipRect& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { value }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(const CSS::Color& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { value }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(const CSS::ContentCounterFunctionWrapper& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { value.value }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(const CSS::ContentCountersFunctionWrapper& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { value.value }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(const CSS::ContentLegacyAttrFunctionWrapper& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { value.value }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(const CSS::CustomIdent& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { value }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(const CSS::FontFamilyName& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { value }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(const CSS::Keyword& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { value }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(const CSS::String& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { value }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(const CSS::URL& value, CSSStyleDeclaration& owner)
{
    return create(DeprecatedCSSOMPrimitiveValueData { value }, owner);
}

Ref<DeprecatedCSSOMPrimitiveValue> DeprecatedCSSOMPrimitiveValue::create(DeprecatedCSSOMPrimitiveValueData&& data, CSSStyleDeclaration& owner)
{
    return adoptRef(*new DeprecatedCSSOMPrimitiveValue(WTF::move(data), owner));
}

DeprecatedCSSOMPrimitiveValue::DeprecatedCSSOMPrimitiveValue(DeprecatedCSSOMPrimitiveValueData&& data, CSSStyleDeclaration& owner)
    : DeprecatedCSSOMValue(owner)
    , m_data(makeUniqueRef<DeprecatedCSSOMPrimitiveValueData>(WTF::move(data)))
{
}

DeprecatedCSSOMPrimitiveValue::~DeprecatedCSSOMPrimitiveValue() = default;

String DeprecatedCSSOMPrimitiveValue::cssText() const
{
    return WTF::switchOn(m_data.get(),
        [](const DeprecatedCSSOMPrimitiveValueData::TypeErasedValue& value) -> String {
            return value(CSS::defaultSerializationContext());
        },
        [](const DeprecatedCSSOMPrimitiveValueData::NumericRaw& raw) -> String {
            return CSS::serializationForCSS(CSS::defaultSerializationContext(), CSS::SerializableNumber { raw.value, unitTypeString(raw.unit) });
        },
        [](const auto& value) -> String {
            return CSS::serializationForCSS(CSS::defaultSerializationContext(), value);
        }
    );
}

unsigned short DeprecatedCSSOMPrimitiveValue::cssValueType() const
{
    // These values are exposed in the DOM, but constants for them are not.
    constexpr unsigned short CSS_INITIAL = 4;
    constexpr unsigned short CSS_UNSET = 5;
    constexpr unsigned short CSS_REVERT = 6;

    if (auto* keyword = std::get_if<CSS::Keyword>(&m_data->value)) {
        switch (keyword->value) {
        case CSSValueInherit:
            return CSS_INHERIT;
        case CSSValueInitial:
            return CSS_INITIAL;
        case CSSValueUnset:
            return CSS_UNSET;
        case CSSValueRevert:
            return CSS_REVERT;
        default:
            break;
        }
    }

    return CSS_PRIMITIVE_VALUE;
}

unsigned short DeprecatedCSSOMPrimitiveValue::primitiveType() const
{
    auto convertUnitType = [](CSSUnitType unitType) -> unsigned short {
        switch (unitType) {
        case CSSUnitType::CSS_CM:                           return CSS_CM;
        case CSSUnitType::CSS_DEG:                          return CSS_DEG;
        case CSSUnitType::CSS_EM:                           return CSS_EMS;
        case CSSUnitType::CSS_EX:                           return CSS_EXS;
        case CSSUnitType::CSS_GRAD:                         return CSS_GRAD;
        case CSSUnitType::CSS_HZ:                           return CSS_HZ;
        case CSSUnitType::CSS_INTEGER:                      return CSS_NUMBER;
        case CSSUnitType::CSS_IN:                           return CSS_IN;
        case CSSUnitType::CSS_KHZ:                          return CSS_KHZ;
        case CSSUnitType::CSS_MM:                           return CSS_MM;
        case CSSUnitType::CSS_MS:                           return CSS_MS;
        case CSSUnitType::CSS_NUMBER:                       return CSS_NUMBER;
        case CSSUnitType::CSS_PC:                           return CSS_PC;
        case CSSUnitType::CSS_PERCENTAGE:                   return CSS_PERCENTAGE;
        case CSSUnitType::CSS_PT:                           return CSS_PT;
        case CSSUnitType::CSS_PX:                           return CSS_PX;
        case CSSUnitType::CSS_RAD:                          return CSS_RAD;
        case CSSUnitType::CSS_S:                            return CSS_S;

        // All other, including newer types, should return UNKNOWN.
        default:                                            return CSS_UNKNOWN;
        }
    };

    return WTF::switchOn(m_data.get(),
        [&](const DeprecatedCSSOMPrimitiveValueData::TypeErasedValue&) -> unsigned short {
            return CSS_UNKNOWN;
        },
        [&](const DeprecatedCSSOMPrimitiveValueData::NumericRaw& raw) -> unsigned short {
            return convertUnitType(raw.unit);
        },
        [&](const DeprecatedCSSOMPrimitiveValueData::NumericCalc& calc) -> unsigned short {
            return convertUnitType(calc.primitiveType());
        },
        [](const CSS::CustomIdent&) -> unsigned short {
            return CSS_IDENT;
        },
        [](const CSS::Keyword&) -> unsigned short {
            return CSS_IDENT;
        },
        [](const CSS::String&) -> unsigned short {
            return CSS_STRING;
        },
        [](const CSS::FontFamilyName&) -> unsigned short {
            return CSS_STRING;
        },
        [](const CSS::URL&) -> unsigned short {
            return CSS_URI;
        },
        [](const CSS::Color&) -> unsigned short {
            return CSS_RGBCOLOR;
        },
        [](const CSS::ContentCounterFunction&) -> unsigned short {
            return CSS_COUNTER;
        },
        [](const CSS::ContentCountersFunction&) -> unsigned short {
            return CSS_COUNTER;
        },
        [](const CSS::ContentLegacyAttrFunction&) -> unsigned short {
            return CSS_ATTR;
        },
        [](const CSS::ClipRect&) -> unsigned short {
            return CSS_RECT;
        }
    );
}

ExceptionOr<float> DeprecatedCSSOMPrimitiveValue::getFloatValue(unsigned short unitType) const
{
    using ValueAndOptionalUnit = std::pair<double, std::optional<CSSUnitType>>;

    auto doubleValueAndOptionalUnitOrException = WTF::switchOn(m_data.get(),
        [&](const DeprecatedCSSOMPrimitiveValueData::NumericRaw& raw) -> ExceptionOr<ValueAndOptionalUnit> {
            return ValueAndOptionalUnit { raw.value, raw.unit };
        },
        [&](const DeprecatedCSSOMPrimitiveValueData::NumericCalc& calc) -> ExceptionOr<ValueAndOptionalUnit> {
            return ValueAndOptionalUnit { calc.evaluateDeprecated(), std::nullopt };
        },
        [&](const auto&) -> ExceptionOr<ValueAndOptionalUnit> {
            return Exception { ExceptionCode::InvalidAccessError };
        }
    );
    if (doubleValueAndOptionalUnitOrException.hasException())
        return doubleValueAndOptionalUnitOrException.releaseException();

    auto [doubleValue, selfUnitType] = doubleValueAndOptionalUnitOrException.releaseReturnValue();

    if (unitType == CSS_DIMENSION)
        return clampTo<float>(doubleValue);

    auto requestedUnitType = [&] -> std::optional<CSSUnitType> {
        switch (unitType) {
        case CSS_CM:            return CSSUnitType::CSS_CM;
        case CSS_DEG:           return CSSUnitType::CSS_DEG;
        case CSS_EMS:           return CSSUnitType::CSS_EM;
        case CSS_EXS:           return CSSUnitType::CSS_EX;
        case CSS_GRAD:          return CSSUnitType::CSS_GRAD;
        case CSS_HZ:            return CSSUnitType::CSS_HZ;
        case CSS_IN:            return CSSUnitType::CSS_IN;
        case CSS_KHZ:           return CSSUnitType::CSS_KHZ;
        case CSS_MM:            return CSSUnitType::CSS_MM;
        case CSS_MS:            return CSSUnitType::CSS_MS;
        case CSS_NUMBER:        return CSSUnitType::CSS_NUMBER;
        case CSS_PC:            return CSSUnitType::CSS_PC;
        case CSS_PERCENTAGE:    return CSSUnitType::CSS_PERCENTAGE;
        case CSS_PT:            return CSSUnitType::CSS_PT;
        case CSS_PX:            return CSSUnitType::CSS_PX;
        case CSS_RAD:           return CSSUnitType::CSS_RAD;
        case CSS_S:             return CSSUnitType::CSS_S;
        default:                return std::nullopt;
        }
    }();
    if (!requestedUnitType)
        return Exception { ExceptionCode::InvalidAccessError };
    auto targetUnitType = *requestedUnitType;

    if (!selfUnitType)
        return Exception { ExceptionCode::InvalidAccessError };
    auto sourceUnitType = *selfUnitType;

    if (targetUnitType == sourceUnitType)
        return clampTo<float>(doubleValue);

    auto sourceCategory = unitCategory(sourceUnitType);
    ASSERT(sourceCategory != CSSUnitCategory::Other);
    auto targetCategory = unitCategory(targetUnitType);
    ASSERT(targetCategory != CSSUnitCategory::Other);

    // Cannot convert between unrelated unit categories if one of them is not CSSUnitCategory::Number.
    if (sourceCategory != targetCategory && sourceCategory != CSSUnitCategory::Number && targetCategory != CSSUnitCategory::Number)
        return Exception { ExceptionCode::InvalidAccessError };

    if (targetCategory == CSSUnitCategory::Number) {
        // Cannot convert between numbers and percent.
        if (sourceCategory == CSSUnitCategory::Percent)
            return Exception { ExceptionCode::InvalidAccessError };
        // We interpret conversion to CSSUnitType::CSS_NUMBER as conversion to a canonical unit in this value's category.
        targetUnitType = canonicalUnitTypeForCategory(sourceCategory);
        if (targetUnitType == CSSUnitType::CSS_UNKNOWN)
            return Exception { ExceptionCode::InvalidAccessError };
    }

    if (sourceUnitType == CSSUnitType::CSS_NUMBER || sourceUnitType == CSSUnitType::CSS_INTEGER) {
        // Cannot convert between numbers and percent.
        if (targetCategory == CSSUnitCategory::Percent)
            return Exception { ExceptionCode::InvalidAccessError };
        // We interpret conversion from CSSUnitType::CSS_NUMBER in the same way as CSSParser::validUnit() while using non-strict mode.
        sourceUnitType = canonicalUnitTypeForCategory(targetCategory);
        if (sourceUnitType == CSSUnitType::CSS_UNKNOWN)
            return Exception { ExceptionCode::InvalidAccessError };
    }

    double convertedValue = doubleValue;

    // If we don't need to scale it, don't worry about if we can scale it.
    if (sourceUnitType == targetUnitType)
        return clampTo<float>(convertedValue);

    // First convert the value from the source unit type the to the canonical type.
    auto sourceFactor = conversionToCanonicalUnitsScaleFactor(sourceUnitType);
    if (!sourceFactor.has_value())
        return Exception { ExceptionCode::InvalidAccessError };
    convertedValue *= sourceFactor.value();

    // Now convert from canonical type to the target unitType.
    auto targetFactor = conversionToCanonicalUnitsScaleFactor(targetUnitType);
    if (!targetFactor.has_value())
        return Exception { ExceptionCode::InvalidAccessError };
    convertedValue /= targetFactor.value();

    return clampTo<float>(convertedValue);
}

ExceptionOr<String> DeprecatedCSSOMPrimitiveValue::getStringValue() const
{
    return WTF::switchOn(m_data.get(),
        [](const CSS::CustomIdent& customIdent) -> ExceptionOr<String> {
            return String { customIdent.value };
        },
        [](const CSS::Keyword& keyword) -> ExceptionOr<String> {
            return String { nameStringForSerialization(keyword.value) };
        },
        [](const CSS::String& string) -> ExceptionOr<String> {
            return String { string.value };
        },
        [](const CSS::FontFamilyName& fontFamilyName) -> ExceptionOr<String> {
            return String { fontFamilyName.value };
        },
        [](const CSS::URL& url) -> ExceptionOr<String> {
            return String { url.specified };
        },
        [](const CSS::ContentLegacyAttrFunction& attrFunction) -> ExceptionOr<String> {
            return CSS::serializationForCSS(CSS::defaultSerializationContext(), attrFunction);
        },
        [](const auto&) -> ExceptionOr<String> {
            return Exception { ExceptionCode::InvalidAccessError };
        }
    );
}

ExceptionOr<Ref<DeprecatedCSSOMCounter>> DeprecatedCSSOMPrimitiveValue::getCounterValue() const
{
    auto convertStyleToString = [](auto& style) -> String {
        return WTF::switchOn(style.identifier,
            [](const CSS::Keyword& predefinedKeyword) -> String {
                return nameLiteralForSerialization(predefinedKeyword.value);
            },
            [](const CSS::CustomIdent& customIdent) -> String {
                return customIdent.value.string();
            }
        );
    };

    if (auto* contentCounter = std::get_if<CSS::ContentCounterFunction>(&m_data->value)) {
        return DeprecatedCSSOMCounter::create(
            contentCounter->parameters.identifier.value,
            emptyString(),
            convertStyleToString(contentCounter->parameters.style)
        );
    }
    if (auto* contentCounters = std::get_if<CSS::ContentCountersFunction>(&m_data->value)) {
        return DeprecatedCSSOMCounter::create(
            contentCounters->parameters.identifier.value,
            contentCounters->parameters.separator.value,
            convertStyleToString(contentCounters->parameters.style)
        );
    }
    return Exception { ExceptionCode::InvalidAccessError };
}

ExceptionOr<Ref<DeprecatedCSSOMRect>> DeprecatedCSSOMPrimitiveValue::getRectValue() const
{
    if (auto* clipRect = std::get_if<CSS::ClipRect>(&m_data->value))
        return DeprecatedCSSOMRect::create(*clipRect, m_owner);
    return Exception { ExceptionCode::InvalidAccessError };
}

ExceptionOr<Ref<DeprecatedCSSOMRGBColor>> DeprecatedCSSOMPrimitiveValue::getRGBColorValue() const
{
    if (auto* color = std::get_if<CSS::Color>(&m_data->value))
        return DeprecatedCSSOMRGBColor::create(color->absoluteColor(), m_owner);
    return Exception { ExceptionCode::InvalidAccessError };
}

bool DeprecatedCSSOMPrimitiveValue::isCSSWideKeyword() const
{
    if (auto* keyword = std::get_if<CSS::Keyword>(&m_data->value))
        return WebCore::isCSSWideKeyword(keyword->value);
    return false;
}

} // namespace WebCore
