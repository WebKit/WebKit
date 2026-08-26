/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "SVGAngleValue.h"

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSTokenizer.h"
#include "ExceptionOr.h"
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAngleValue);

float SVGAngleValue::value() const
{
    switch (m_unitType) {
    case SVG_ANGLETYPE_GRAD:
        return grad2deg(m_valueInSpecifiedUnits);
    case SVG_ANGLETYPE_RAD:
        return rad2deg(m_valueInSpecifiedUnits);
    case SVG_ANGLETYPE_TURN:
        return turn2deg(m_valueInSpecifiedUnits);
    case SVG_ANGLETYPE_UNSPECIFIED:
    case SVG_ANGLETYPE_UNKNOWN:
    case SVG_ANGLETYPE_DEG:
        return m_valueInSpecifiedUnits;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void SVGAngleValue::setValue(float value)
{
    switch (m_unitType) {
    case SVG_ANGLETYPE_GRAD:
        m_valueInSpecifiedUnits = deg2grad(value);
        return;
    case SVG_ANGLETYPE_RAD:
        m_valueInSpecifiedUnits = deg2rad(value);
        return;
    case SVG_ANGLETYPE_TURN:
        m_valueInSpecifiedUnits = deg2turn(value);
        return;
    case SVG_ANGLETYPE_UNSPECIFIED:
    case SVG_ANGLETYPE_UNKNOWN:
    case SVG_ANGLETYPE_DEG:
        m_valueInSpecifiedUnits = value;
        return;
    }
    ASSERT_NOT_REACHED();
}

String SVGAngleValue::valueAsString() const
{
    switch (m_unitType) {
    case SVG_ANGLETYPE_DEG:
        return makeString(m_valueInSpecifiedUnits, "deg"_s);
    case SVG_ANGLETYPE_RAD:
        return makeString(m_valueInSpecifiedUnits, "rad"_s);
    case SVG_ANGLETYPE_TURN:
        return makeString(m_valueInSpecifiedUnits, "turn"_s);
    case SVG_ANGLETYPE_GRAD:
        return makeString(m_valueInSpecifiedUnits, "grad"_s);
    case SVG_ANGLETYPE_UNSPECIFIED:
    case SVG_ANGLETYPE_UNKNOWN:
        return String::number(m_valueInSpecifiedUnits);
    }

    ASSERT_NOT_REACHED();
    return String();
}

static inline SVGAngleValue::Type NODELETE cssAngleUnitToSVGAngleType(CSS::AngleUnit unit)
{
    switch (unit) {
    case CSS::AngleUnit::Deg:   return SVGAngleValue::SVG_ANGLETYPE_DEG;
    case CSS::AngleUnit::Rad:   return SVGAngleValue::SVG_ANGLETYPE_RAD;
    case CSS::AngleUnit::Grad:  return SVGAngleValue::SVG_ANGLETYPE_GRAD;
    case CSS::AngleUnit::Turn:  return SVGAngleValue::SVG_ANGLETYPE_TURN;
    }
    ASSERT_NOT_REACHED();
    return SVGAngleValue::SVG_ANGLETYPE_UNKNOWN;
}

ExceptionOr<void> SVGAngleValue::setValueAsString(const String& value)
{
    if (value.isEmpty()) {
        m_unitType = SVG_ANGLETYPE_UNSPECIFIED;
        return { };
    }

    // Unresolved CSS primitive numeric types always hold their value as a double, and
    // CSS::All neither rejects nor clamps, so guard against values that are not
    // representable as the float this stores.
    auto isFloatOverflow = [](double value) {
        return value > FLT_MAX || value < -FLT_MAX;
    };

    auto parserContext = CSSParserContext { SVGAttributeMode };
    auto parserState = CSS::PropertyParserState {
        .context = parserContext
    };

    CSSTokenizer tokenizer(value);
    auto tokenRange = tokenizer.tokenRange();

    // Leading whitespace is allowed; the consumers below take care of the trailing
    // whitespace by way of CSSParserTokenRange::consumeIncludingWhitespace().
    tokenRange.consumeWhitespace();

    // A unitless value must stay SVG_ANGLETYPE_UNSPECIFIED rather than becoming
    // SVG_ANGLETYPE_DEG, so consume <number> ahead of <angle>. The raw types are used
    // so that calc() is not consumed at all.
    // FIXME: Add support for calculated angles.
    auto parsedValue = CSSPropertyParserHelpers::MetaConsumer<CSS::NumberRaw<>, CSS::AngleRaw<>>::consume(tokenRange, parserState, { });
    if (!parsedValue || !tokenRange.atEnd())
        return Exception { ExceptionCode::SyntaxError };

    return WTF::switchOn(*parsedValue,
        [&](const CSS::NumberRaw<>& number) -> ExceptionOr<void> {
            if (isFloatOverflow(number.value))
                return Exception { ExceptionCode::SyntaxError };
            m_unitType = SVG_ANGLETYPE_UNSPECIFIED;
            m_valueInSpecifiedUnits = number.value;
            return { };
        },
        [&](const CSS::AngleRaw<>& angle) -> ExceptionOr<void> {
            if (isFloatOverflow(angle.value))
                return Exception { ExceptionCode::SyntaxError };
            m_unitType = cssAngleUnitToSVGAngleType(angle.unit);
            m_valueInSpecifiedUnits = angle.value;
            return { };
        }
    );
}

ExceptionOr<void> SVGAngleValue::newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits)
{
    if (unitType == SVG_ANGLETYPE_UNKNOWN || unitType > SVG_ANGLETYPE_GRAD)
        return Exception { ExceptionCode::NotSupportedError };

    m_unitType = static_cast<Type>(unitType);
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
    return { };
}

ExceptionOr<void> SVGAngleValue::convertToSpecifiedUnits(unsigned short unitType)
{
    if (unitType == SVG_ANGLETYPE_UNKNOWN || m_unitType == SVG_ANGLETYPE_UNKNOWN || unitType > SVG_ANGLETYPE_GRAD)
        return Exception { ExceptionCode::NotSupportedError };

    if (unitType == m_unitType)
        return { };

    switch (m_unitType) {
    case SVG_ANGLETYPE_TURN:
        switch (unitType) {
        case SVG_ANGLETYPE_GRAD:
            m_valueInSpecifiedUnits = turn2grad(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_UNSPECIFIED:
        case SVG_ANGLETYPE_DEG:
            m_valueInSpecifiedUnits = turn2deg(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_RAD:
            m_valueInSpecifiedUnits = deg2rad(turn2deg(m_valueInSpecifiedUnits));
            break;
        case SVG_ANGLETYPE_TURN:
        case SVG_ANGLETYPE_UNKNOWN:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case SVG_ANGLETYPE_RAD:
        switch (unitType) {
        case SVG_ANGLETYPE_GRAD:
            m_valueInSpecifiedUnits = rad2grad(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_UNSPECIFIED:
        case SVG_ANGLETYPE_DEG:
            m_valueInSpecifiedUnits = rad2deg(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_TURN:
            m_valueInSpecifiedUnits = deg2turn(rad2deg(m_valueInSpecifiedUnits));
            break;
        case SVG_ANGLETYPE_RAD:
        case SVG_ANGLETYPE_UNKNOWN:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case SVG_ANGLETYPE_GRAD:
        switch (unitType) {
        case SVG_ANGLETYPE_RAD:
            m_valueInSpecifiedUnits = grad2rad(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_UNSPECIFIED:
        case SVG_ANGLETYPE_DEG:
            m_valueInSpecifiedUnits = grad2deg(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_TURN:
            m_valueInSpecifiedUnits = grad2turn(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_GRAD:
        case SVG_ANGLETYPE_UNKNOWN:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case SVG_ANGLETYPE_UNSPECIFIED:
        // Spec: For angles, a unitless value is treated the same as if degrees were specified.
    case SVG_ANGLETYPE_DEG:
        switch (unitType) {
        case SVG_ANGLETYPE_RAD:
            m_valueInSpecifiedUnits = deg2rad(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_GRAD:
            m_valueInSpecifiedUnits = deg2grad(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_TURN:
            m_valueInSpecifiedUnits = deg2turn(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_UNSPECIFIED:
        case SVG_ANGLETYPE_DEG:
            break;
        case SVG_ANGLETYPE_UNKNOWN:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case SVG_ANGLETYPE_UNKNOWN:
        ASSERT_NOT_REACHED();
        break;
    }

    m_unitType = static_cast<Type>(unitType);

    return { };
}

}
