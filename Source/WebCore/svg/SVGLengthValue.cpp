/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGLengthValue.h"

#include "AnimationUtilities.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSToLengthConversionData.h"
#include "CSSTokenizer.h"
#include "ExceptionOr.h"
#include "SVGElement.h"
#include "SVGLengthContext.h"
#include "SVGParsingError.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGLengthValue);

static inline SVGLengthType cssLengthUnitToSVGLengthType(CSS::LengthPercentageUnit unit)
{
    switch (unit) {
    case CSS::LengthPercentageUnit::Px:                 return SVGLengthType::Pixels;
    case CSS::LengthPercentageUnit::Percentage:         return SVGLengthType::Percentage;
    case CSS::LengthPercentageUnit::Em:                 return SVGLengthType::Ems;
    case CSS::LengthPercentageUnit::Ex:                 return SVGLengthType::Exs;
    case CSS::LengthPercentageUnit::Cm:                 return SVGLengthType::Centimeters;
    case CSS::LengthPercentageUnit::Mm:                 return SVGLengthType::Millimeters;
    case CSS::LengthPercentageUnit::In:                 return SVGLengthType::Inches;
    case CSS::LengthPercentageUnit::Pt:                 return SVGLengthType::Points;
    case CSS::LengthPercentageUnit::Pc:                 return SVGLengthType::Picas;
    case CSS::LengthPercentageUnit::Lh:                 return SVGLengthType::Lh;
    case CSS::LengthPercentageUnit::Ch:                 return SVGLengthType::Ch;
    default:                                            return SVGLengthType::Unknown;
    }
}

static inline CSS::LengthPercentageUnit svgLengthTypeToCSSLengthUnit(SVGLengthType type)
{
    switch (type) {
    case SVGLengthType::Number:       return CSS::LengthPercentageUnit::Px;
    case SVGLengthType::Pixels:       return CSS::LengthPercentageUnit::Px;
    case SVGLengthType::Percentage:   return CSS::LengthPercentageUnit::Percentage;
    case SVGLengthType::Ems:          return CSS::LengthPercentageUnit::Em;
    case SVGLengthType::Exs:          return CSS::LengthPercentageUnit::Ex;
    case SVGLengthType::Centimeters:  return CSS::LengthPercentageUnit::Cm;
    case SVGLengthType::Millimeters:  return CSS::LengthPercentageUnit::Mm;
    case SVGLengthType::Inches:       return CSS::LengthPercentageUnit::In;
    case SVGLengthType::Points:       return CSS::LengthPercentageUnit::Pt;
    case SVGLengthType::Picas:        return CSS::LengthPercentageUnit::Pc;
    case SVGLengthType::Lh:           return CSS::LengthPercentageUnit::Lh;
    case SVGLengthType::Ch:           return CSS::LengthPercentageUnit::Ch;
    default:                          return CSS::LengthPercentageUnit::Px;
    }
}

static Variant<CSS::Number<>, CSS::LengthPercentage<>> createVariantForLengthType(float value, SVGLengthType lengthType)
{
    if (lengthType == SVGLengthType::Number)
        return CSS::Number<>(value);

    if (lengthType == SVGLengthType::Unknown) {
        // For unknown types (like container units), fall back to Number
        // FIXME: Add support for container units
        return CSS::Number<>(value);
    }

    return CSS::LengthPercentage<>(svgLengthTypeToCSSLengthUnit(lengthType), value);
}


SVGLengthValue::SVGLengthValue(SVGLengthMode lengthMode, const String& valueAsString)
    : m_value(CSS::Number<>(0))
    , m_lengthMode(lengthMode)
{
    setValueAsString(valueAsString);
}

SVGLengthValue::SVGLengthValue(float valueInSpecifiedUnits, SVGLengthType lengthType, SVGLengthMode lengthMode)
    : m_value(createVariantForLengthType(valueInSpecifiedUnits, lengthType))
    , m_lengthMode(lengthMode)
{
}

SVGLengthValue::SVGLengthValue(const SVGLengthContext& context, float value, SVGLengthType lengthType, SVGLengthMode lengthMode)
    : m_value(createVariantForLengthType(0, lengthType))
    , m_lengthMode(lengthMode)
{
    setValue(context, value);
}

SVGLengthValue SVGLengthValue::construct(SVGLengthMode lengthMode, StringView valueAsString, SVGParsingError& parseError, SVGLengthNegativeValuesMode negativeValuesMode, ASCIILiteral fallbackValue)
{
    SVGLengthValue length(lengthMode);

    parseError = SVGParsingError::None;
    if (length.setValueAsString(valueAsString).hasException())
        parseError = SVGParsingError::ParsingFailed;
    else if (negativeValuesMode == SVGLengthNegativeValuesMode::Forbid && length.valueInSpecifiedUnits() < 0)
        parseError = SVGParsingError::ForbiddenNegativeValue;

    // If parsing failed or value is null, and we have a fallback, use it
    if (!fallbackValue.isNull() && (parseError != SVGParsingError::None || valueAsString.isNull()))
        return SVGLengthValue(lengthMode, fallbackValue);

    return length;
}

ExceptionOr<void> SVGLengthValue::setValueAsString(StringView valueAsString, SVGLengthMode lengthMode)
{
    m_lengthMode = lengthMode;
    return setValueAsString(valueAsString);
}

SVGLengthType SVGLengthValue::lengthType() const
{
    return WTF::switchOn(m_value,
        [](const CSS::Number<>&) -> SVGLengthType {
            return SVGLengthType::Number;
        },
        [](const CSS::LengthPercentage<>& length) -> SVGLengthType {
            if (auto raw = length.raw())
                return cssLengthUnitToSVGLengthType(raw->unit);

            return SVGLengthType::Unknown;
        }
    );
}

bool SVGLengthValue::isZero() const
{
    return WTF::switchOn(m_value,
        [](const auto& value) {
            return value.isKnownZero();
        }
    );
}

bool SVGLengthValue::isRelative() const
{
    return WTF::switchOn(m_value,
        [](const CSS::Number<>&) {
            return false;
        },
        [](const CSS::LengthPercentage<>& length) {
            if (auto raw = length.raw()) {
                using Unit = CSS::LengthPercentageUnit;
                switch (raw->unit) {
                case Unit::Percentage:
                case Unit::Em:
                case Unit::Ex:
                case Unit::Ch:
                case Unit::Lh:
                case Unit::Rem:
                case Unit::Rex:
                case Unit::Rlh:
                case Unit::Rch:
                    return true;
                default:
                    return false;
                }
            }

            return false;
        }
    );
}

float SVGLengthValue::value(const SVGLengthContext& context) const
{
    auto result = valueForBindings(context);
    if (result.hasException())
        return 0;
    return result.releaseReturnValue();
}

float SVGLengthValue::valueAsPercentage() const
{
    return WTF::switchOn(m_value,
        [](const CSS::Number<>& number) -> float {
            if (auto raw = number.raw())
                return raw->value;

            return 0.0f;
        },
        [](const CSS::LengthPercentage<>& length) -> float {
            if (auto raw = length.raw()) {
                if (raw->unit == CSS::LengthPercentageUnit::Percentage)
                    return raw->value / 100.0f;

                return raw->value;
            }

            return 0.0f;
        }
    );
}

float SVGLengthValue::valueInSpecifiedUnits() const
{
    // Per SVG spec: return 0 for non-scalar values like calc()
    // https://svgwg.org/svg2-draft/types.html#__svg__SVGLength__valueInSpecifiedUnits

    return WTF::switchOn(m_value,
        [](const auto& value) {
            if (auto raw = value.raw())
                return clampTo<float>(raw->value);

            return 0.f;
        }
    );
}

String SVGLengthValue::valueAsString() const
{
    return WTF::switchOn(m_value,
        [](const auto& value) {
            if (auto raw = value.raw()) {
                // FIXME: Handle calc() expressions and consider exponential notation for very large/small values
                float numericValue = clampTo<float>(raw->value);
                return formatCSSNumberValue(CSS::SerializableNumber { numericValue, CSS::unitString(raw->unit) });
            }

            return String();
        }
    );
}

AtomString SVGLengthValue::valueAsAtomString() const
{
    return makeAtomString(valueAsString());
}

ExceptionOr<float> SVGLengthValue::valueForBindings(const SVGLengthContext& context) const
{
    return WTF::switchOn(m_value,
        [&](const CSS::Number<>& number) -> ExceptionOr<float> {
            if (auto raw = number.raw())
                return raw->value;

            return Exception { ExceptionCode::NotFoundError };
        },
        [&](const CSS::LengthPercentage<>& length) -> ExceptionOr<float> {
            if (auto raw = length.raw())
                return context.resolveValueToUserUnits(raw->value, raw->unit, m_lengthMode);

            return Exception { ExceptionCode::NotFoundError };
        }
    );
}

void SVGLengthValue::setValueInSpecifiedUnits(float value)
{
    m_value = WTF::switchOn(m_value,
        [&](const CSS::Number<>&) -> decltype(m_value) {
            return CSS::Number<>(value);
        },
        [&](const CSS::LengthPercentage<>& current) -> decltype(m_value) {
            if (auto raw = current.raw())
                return CSS::LengthPercentage<>(raw->unit, value);

            return CSS::Number<>(value);
        }
    );
}

ExceptionOr<void> SVGLengthValue::setValue(const SVGLengthContext& context, float value)
{
    return WTF::switchOn(m_value,
        [&](const CSS::Number<>&) -> ExceptionOr<void> {
            m_value = CSS::Number<>(value);
            return { };
        },
        [&](const CSS::LengthPercentage<>& current) -> ExceptionOr<void> {
            if (auto raw = current.raw()) {
                auto resolvedValue = context.resolveValueFromUserUnits(value, raw->unit, m_lengthMode);
                if (resolvedValue.hasException())
                    return resolvedValue.releaseException();

                m_value = resolvedValue.releaseReturnValue();
                return { };
            }

            m_value = CSS::Number<>(value);
            return { };
        }
    );
}

ExceptionOr<void> SVGLengthValue::setValue(const SVGLengthContext& context, float value, SVGLengthType lengthType, SVGLengthMode lengthMode)
{
    // FIXME: Seems like a bug that we change the value of m_unit even if setValue throws an exception.
    m_lengthMode = lengthMode;
    m_value = createVariantForLengthType(value, lengthType);

    return setValue(context, value);
}

ExceptionOr<void> SVGLengthValue::setValueAsString(StringView string)
{
    if (string.isEmpty())
        return Exception { ExceptionCode::SyntaxError };

    // CSS::Range only clamps to boundaries, but we historically handled
    // overflow values like "-45e58" to 0 instead of FLT_MAX.
    // FIXME: Consider setting to a proper value
    auto isFloatOverflow = [](const auto& parsedValue) {
        if (auto raw = parsedValue.raw()) {
            double value = raw->value;
            return value > FLT_MAX || value < -FLT_MAX;
        }
        return true;
    };

    auto parserContext = CSSParserContext { SVGAttributeMode };
    auto parserState = CSS::PropertyParserState {
        .context = parserContext
    };

    String newString = string.toString();
    CSSTokenizer tokenizer(newString);
    auto tokenRange = tokenizer.tokenRange();

    if (auto number = CSSPropertyParserHelpers::MetaConsumer<CSS::Number<>>::consume(tokenRange, parserState, { })) {
        if (!tokenRange.atEnd())
            return Exception { ExceptionCode::SyntaxError };

        m_value = isFloatOverflow(*number) ? CSS::Number<>(0) : WTFMove(*number);

        return { };
    }

    tokenRange = tokenizer.tokenRange();
    if (auto length = CSSPropertyParserHelpers::MetaConsumer<CSS::LengthPercentage<>>::consume(tokenRange, parserState, { })) {
        if (!tokenRange.atEnd())
            return Exception { ExceptionCode::SyntaxError };

        // FIXME: Add support for calculated lengths.
        if (length->isCalc())
            return Exception { ExceptionCode::SyntaxError };

        m_value = WTFMove(*length);

        return { };
    }

    return Exception { ExceptionCode::SyntaxError };
}

ExceptionOr<void> SVGLengthValue::convertToSpecifiedUnits(const SVGLengthContext& context, SVGLengthType targetType)
{
    auto valueInUserUnits = valueForBindings(context);
    if (valueInUserUnits.hasException())
        return valueInUserUnits.releaseException();

    float userUnits = valueInUserUnits.releaseReturnValue();

    if (targetType == SVGLengthType::Number) {
        m_value = CSS::Number<>(userUnits);
        return { };
    }

    auto convertedValue = context.resolveValueFromUserUnits(userUnits, svgLengthTypeToCSSLengthUnit(targetType), m_lengthMode);

    if (convertedValue.hasException())
        return convertedValue.releaseException();

    m_value = convertedValue.releaseReturnValue();
    return { };
}

TextStream& operator<<(TextStream& ts, const SVGLengthValue& length)
{
    ts << length.valueAsString();
    return ts;
}

}
