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

#include "SVGParserUtilities.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

float SVGAngleValue::value() const
{
    switch (m_unitType) {
    case SVG_ANGLETYPE_GRAD:
        return grad2deg(m_valueInSpecifiedUnits);
    case SVG_ANGLETYPE_RAD:
        return rad2deg(m_valueInSpecifiedUnits);
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
        return makeString(m_valueInSpecifiedUnits, "deg");
    case SVG_ANGLETYPE_RAD:
        return makeString(m_valueInSpecifiedUnits, "rad");
    case SVG_ANGLETYPE_GRAD:
        return makeString(m_valueInSpecifiedUnits, "grad");
    case SVG_ANGLETYPE_UNSPECIFIED:
    case SVG_ANGLETYPE_UNKNOWN:
        return String::number(m_valueInSpecifiedUnits);
    }

    ASSERT_NOT_REACHED();
    return String();
}

template<typename CharacterType> static inline SVGAngleValue::Type parseAngleType(const CharacterType* ptr, const CharacterType* end)
{
    switch (end - ptr) {
    case 0:
        return SVGAngleValue::SVG_ANGLETYPE_UNSPECIFIED;
    case 3:
        if (ptr[0] == 'd' && ptr[1] == 'e' && ptr[2] == 'g')
            return SVGAngleValue::SVG_ANGLETYPE_DEG;
        if (ptr[0] == 'r' && ptr[1] == 'a' && ptr[2] == 'd')
            return SVGAngleValue::SVG_ANGLETYPE_RAD;
        break;
    case 4:
        if (ptr[0] == 'g' && ptr[1] == 'r' && ptr[2] == 'a' && ptr[3] == 'd')
            return SVGAngleValue::SVG_ANGLETYPE_GRAD;
        break;
    }
    return SVGAngleValue::SVG_ANGLETYPE_UNKNOWN;
}

ExceptionOr<void> SVGAngleValue::setValueAsString(const String& value)
{
    if (value.isEmpty()) {
        m_unitType = SVG_ANGLETYPE_UNSPECIFIED;
        return { };
    }

    auto helper = [&](auto* ptr, auto* end) -> ExceptionOr<void> {
        auto valueInSpecifiedUnits = parseNumber(ptr, end, SuffixSkippingPolicy::DontSkip);
        if (!valueInSpecifiedUnits)
            return Exception { SyntaxError };

        auto unitType = parseAngleType(ptr, end);
        if (unitType == SVGAngleValue::SVG_ANGLETYPE_UNKNOWN)
            return Exception { SyntaxError };

        m_unitType = unitType;
        m_valueInSpecifiedUnits = *valueInSpecifiedUnits;
        return { };
    };

    if (value.is8Bit()) {
        auto* ptr = value.characters8();
        return helper(ptr, ptr + value.length());
    }

    auto* ptr = value.characters16();
    return helper(ptr, ptr + value.length());
}

ExceptionOr<void> SVGAngleValue::newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits)
{
    if (unitType == SVG_ANGLETYPE_UNKNOWN || unitType > SVG_ANGLETYPE_GRAD)
        return Exception { NotSupportedError };

    m_unitType = static_cast<Type>(unitType);
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
    return { };
}

ExceptionOr<void> SVGAngleValue::convertToSpecifiedUnits(unsigned short unitType)
{
    if (unitType == SVG_ANGLETYPE_UNKNOWN || m_unitType == SVG_ANGLETYPE_UNKNOWN || unitType > SVG_ANGLETYPE_GRAD)
        return Exception { NotSupportedError };

    if (unitType == m_unitType)
        return { };

    switch (m_unitType) {
    case SVG_ANGLETYPE_RAD:
        switch (unitType) {
        case SVG_ANGLETYPE_GRAD:
            m_valueInSpecifiedUnits = rad2grad(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_UNSPECIFIED:
        case SVG_ANGLETYPE_DEG:
            m_valueInSpecifiedUnits = rad2deg(m_valueInSpecifiedUnits);
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
