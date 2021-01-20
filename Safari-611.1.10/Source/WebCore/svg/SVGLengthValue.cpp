/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
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
#include "CSSPrimitiveValue.h"
#include "SVGLengthContext.h"
#include "SVGParserUtilities.h"
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static inline const char* lengthTypeToString(SVGLengthType lengthType)
{
    switch (lengthType) {
    case SVGLengthType::Unknown:
    case SVGLengthType::Number:
        return "";    
    case SVGLengthType::Percentage:
        return "%";
    case SVGLengthType::Ems:
        return "em";
    case SVGLengthType::Exs:
        return "ex";
    case SVGLengthType::Pixels:
        return "px";
    case SVGLengthType::Centimeters:
        return "cm";
    case SVGLengthType::Millimeters:
        return "mm";
    case SVGLengthType::Inches:
        return "in";
    case SVGLengthType::Points:
        return "pt";
    case SVGLengthType::Picas:
        return "pc";
    }

    ASSERT_NOT_REACHED();
    return "";
}

template<typename CharacterType> static inline SVGLengthType parseLengthType(StringParsingBuffer<CharacterType>& buffer)
{
    if (buffer.atEnd())
        return SVGLengthType::Number;

    auto firstChar = *buffer++;

    if (buffer.atEnd())
        return firstChar == '%' ? SVGLengthType::Percentage : SVGLengthType::Unknown;

    auto secondChar = *buffer++;

    if (!buffer.atEnd())
        return SVGLengthType::Unknown;

    if (firstChar == 'e' && secondChar == 'm')
        return SVGLengthType::Ems;
    if (firstChar == 'e' && secondChar == 'x')
        return SVGLengthType::Exs;
    if (firstChar == 'p' && secondChar == 'x')
        return SVGLengthType::Pixels;
    if (firstChar == 'c' && secondChar == 'm')
        return SVGLengthType::Centimeters;
    if (firstChar == 'm' && secondChar == 'm')
        return SVGLengthType::Millimeters;
    if (firstChar == 'i' && secondChar == 'n')
        return SVGLengthType::Inches;
    if (firstChar == 'p' && secondChar == 't')
        return SVGLengthType::Points;
    if (firstChar == 'p' && secondChar == 'c')
        return SVGLengthType::Picas;

    return SVGLengthType::Unknown;
}

static inline SVGLengthType primitiveTypeToLengthType(CSSUnitType primitiveType)
{
    switch (primitiveType) {
    case CSSUnitType::CSS_UNKNOWN:
        return SVGLengthType::Unknown;
    case CSSUnitType::CSS_NUMBER:
        return SVGLengthType::Number;
    case CSSUnitType::CSS_PERCENTAGE:
        return SVGLengthType::Percentage;
    case CSSUnitType::CSS_EMS:
        return SVGLengthType::Ems;
    case CSSUnitType::CSS_EXS:
        return SVGLengthType::Exs;
    case CSSUnitType::CSS_PX:
        return SVGLengthType::Pixels;
    case CSSUnitType::CSS_CM:
        return SVGLengthType::Centimeters;
    case CSSUnitType::CSS_MM:
        return SVGLengthType::Millimeters;
    case CSSUnitType::CSS_IN:
        return SVGLengthType::Inches;
    case CSSUnitType::CSS_PT:
        return SVGLengthType::Points;
    case CSSUnitType::CSS_PC:
        return SVGLengthType::Picas;
    default:
        return SVGLengthType::Unknown;
    }

    return SVGLengthType::Unknown;
}

static inline CSSUnitType lengthTypeToPrimitiveType(SVGLengthType lengthType)
{
    switch (lengthType) {
    case SVGLengthType::Unknown:
        return CSSUnitType::CSS_UNKNOWN;
    case SVGLengthType::Number:
        return CSSUnitType::CSS_NUMBER;
    case SVGLengthType::Percentage:
        return CSSUnitType::CSS_PERCENTAGE;
    case SVGLengthType::Ems:
        return CSSUnitType::CSS_EMS;
    case SVGLengthType::Exs:
        return CSSUnitType::CSS_EXS;
    case SVGLengthType::Pixels:
        return CSSUnitType::CSS_PX;
    case SVGLengthType::Centimeters:
        return CSSUnitType::CSS_CM;
    case SVGLengthType::Millimeters:
        return CSSUnitType::CSS_MM;
    case SVGLengthType::Inches:
        return CSSUnitType::CSS_IN;
    case SVGLengthType::Points:
        return CSSUnitType::CSS_PT;
    case SVGLengthType::Picas:
        return CSSUnitType::CSS_PC;
    }

    ASSERT_NOT_REACHED();
    return CSSUnitType::CSS_UNKNOWN;
}

SVGLengthValue::SVGLengthValue(SVGLengthMode lengthMode, const String& valueAsString)
    : m_lengthMode(lengthMode)
{
    setValueAsString(valueAsString);
}

SVGLengthValue::SVGLengthValue(float valueInSpecifiedUnits, SVGLengthType lengthType, SVGLengthMode lengthMode)
    : m_valueInSpecifiedUnits(valueInSpecifiedUnits)
    , m_lengthType(lengthType)
    , m_lengthMode(lengthMode)
{
    ASSERT(m_lengthType != SVGLengthType::Unknown);
}

SVGLengthValue::SVGLengthValue(const SVGLengthContext& context, float value, SVGLengthType lengthType, SVGLengthMode lengthMode)
    : m_lengthType(lengthType)
    , m_lengthMode(lengthMode)
{
    setValue(context, value);
}

Optional<SVGLengthValue> SVGLengthValue::construct(SVGLengthMode lengthMode, StringView valueAsString)
{
    SVGLengthValue length { lengthMode };
    if (length.setValueAsString(valueAsString).hasException())
        return WTF::nullopt;
    return length;
}

SVGLengthValue SVGLengthValue::construct(SVGLengthMode lengthMode, StringView valueAsString, SVGParsingError& parseError, SVGLengthNegativeValuesMode negativeValuesMode)
{
    SVGLengthValue length(lengthMode);

    if (length.setValueAsString(valueAsString).hasException())
        parseError = ParsingAttributeFailedError;
    else if (negativeValuesMode == SVGLengthNegativeValuesMode::Forbid && length.valueInSpecifiedUnits() < 0)
        parseError = NegativeValueForbiddenError;

    return length;
}

SVGLengthValue SVGLengthValue::blend(const SVGLengthValue& from, const SVGLengthValue& to, float progress)
{
    if ((from.isZero() && to.isZero())
        || from.lengthType() == SVGLengthType::Unknown
        || to.lengthType() == SVGLengthType::Unknown
        || (!from.isZero() && from.lengthType() != SVGLengthType::Percentage && to.lengthType() == SVGLengthType::Percentage)
        || (!to.isZero() && from.lengthType() == SVGLengthType::Percentage && to.lengthType() != SVGLengthType::Percentage)
        || (!from.isZero() && !to.isZero() && (from.lengthType() == SVGLengthType::Ems || from.lengthType() == SVGLengthType::Exs) && from.lengthType() != to.lengthType()))
        return to;

    if (from.lengthType() == SVGLengthType::Percentage || to.lengthType() == SVGLengthType::Percentage) {
        auto fromPercent = from.valueAsPercentage() * 100;
        auto toPercent = to.valueAsPercentage() * 100;
        return { WebCore::blend(fromPercent, toPercent, progress), SVGLengthType::Percentage };
    }

    if (from.lengthType() == to.lengthType() || from.isZero() || to.isZero() || from.isRelative()) {
        auto fromValue = from.valueInSpecifiedUnits();
        auto toValue = to.valueInSpecifiedUnits();
        return { WebCore::blend(fromValue, toValue, progress), to.isZero() ? from.lengthType() : to.lengthType() };
    }

    SVGLengthContext nonRelativeLengthContext(nullptr);
    auto fromValueInUserUnits = nonRelativeLengthContext.convertValueToUserUnits(from.valueInSpecifiedUnits(), from.lengthType(), from.lengthMode());
    if (fromValueInUserUnits.hasException())
        return { };

    auto fromValue = nonRelativeLengthContext.convertValueFromUserUnits(fromValueInUserUnits.releaseReturnValue(), to.lengthType(), to.lengthMode());
    if (fromValue.hasException())
        return { };

    float toValue = to.valueInSpecifiedUnits();
    return { WebCore::blend(fromValue.releaseReturnValue(), toValue, progress), to.lengthType() };
}

SVGLengthValue SVGLengthValue::fromCSSPrimitiveValue(const CSSPrimitiveValue& value)
{
    // FIXME: This needs to call value.computeLength() so it can correctly resolve non-absolute units (webkit.org/b/204826).
    SVGLengthType lengthType = primitiveTypeToLengthType(value.primitiveType());
    return lengthType == SVGLengthType::Unknown ? SVGLengthValue() : SVGLengthValue(value.floatValue(), lengthType);
}

Ref<CSSPrimitiveValue> SVGLengthValue::toCSSPrimitiveValue(const SVGLengthValue& length)
{
    return CSSPrimitiveValue::create(length.valueInSpecifiedUnits(), lengthTypeToPrimitiveType(length.lengthType()));
}

ExceptionOr<void> SVGLengthValue::setValueAsString(StringView valueAsString, SVGLengthMode lengthMode)
{
    m_valueInSpecifiedUnits = 0;
    m_lengthMode = lengthMode;
    m_lengthType = SVGLengthType::Number;
    return setValueAsString(valueAsString);
}

float SVGLengthValue::value(const SVGLengthContext& context) const
{
    auto result = valueForBindings(context);
    if (result.hasException())
        return 0;
    return result.releaseReturnValue();
}

String SVGLengthValue::valueAsString() const
{
    return makeString(m_valueInSpecifiedUnits, lengthTypeToString(m_lengthType));
}

ExceptionOr<float> SVGLengthValue::valueForBindings(const SVGLengthContext& context) const
{
    return context.convertValueToUserUnits(m_valueInSpecifiedUnits, m_lengthType, m_lengthMode);
}

ExceptionOr<void> SVGLengthValue::setValue(const SVGLengthContext& context, float value)
{
    // 100% = 100.0 instead of 1.0 for historical reasons, this could eventually be changed
    if (m_lengthType == SVGLengthType::Percentage)
        value = value / 100;

    auto convertedValue = context.convertValueFromUserUnits(value, m_lengthType, m_lengthMode);
    if (convertedValue.hasException())
        return convertedValue.releaseException();
    m_valueInSpecifiedUnits = convertedValue.releaseReturnValue();
    return { };
}

ExceptionOr<void> SVGLengthValue::setValue(const SVGLengthContext& context, float value, SVGLengthType lengthType, SVGLengthMode lengthMode)
{
    // FIXME: Seems like a bug that we change the value of m_unit even if setValue throws an exception.
    m_lengthMode = lengthMode;
    m_lengthType = lengthType;
    return setValue(context, value);
}

ExceptionOr<void> SVGLengthValue::setValueAsString(StringView string)
{
    if (string.isEmpty())
        return { };

    return readCharactersForParsing(string, [&](auto buffer) -> ExceptionOr<void> {
        auto convertedNumber = parseNumber(buffer, SuffixSkippingPolicy::DontSkip);
        if (!convertedNumber)
            return Exception { SyntaxError };

        auto lengthType = parseLengthType(buffer);
        if (lengthType == SVGLengthType::Unknown)
            return Exception { SyntaxError };

        m_lengthType = lengthType;
        m_valueInSpecifiedUnits = *convertedNumber;
        return { };
    });
}

ExceptionOr<void> SVGLengthValue::convertToSpecifiedUnits(const SVGLengthContext& context, SVGLengthType lengthType)
{
    auto valueInUserUnits = valueForBindings(context);
    if (valueInUserUnits.hasException())
        return valueInUserUnits.releaseException();

    auto originalLengthType = m_lengthType;
    m_lengthType = lengthType;
    auto result = setValue(context, valueInUserUnits.releaseReturnValue());
    if (!result.hasException())
        return { };

    m_lengthType = originalLengthType;
    return result.releaseException();
}

TextStream& operator<<(TextStream& ts, const SVGLengthValue& length)
{
    ts << length.valueAsString();
    return ts;
}

}
