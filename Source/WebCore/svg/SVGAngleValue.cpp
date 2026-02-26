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

#include "ExceptionOr.h"
#include "SVGParserUtilities.h"
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/FastCharacterComparison.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAngleValue);

float SVGAngleValue::value() const
{
    switch (m_unitType) {
    case Type::Grad:
        return grad2deg(m_valueInSpecifiedUnits);
    case Type::Rad:
        return rad2deg(m_valueInSpecifiedUnits);
    case Type::Turn:
        return turn2deg(m_valueInSpecifiedUnits);
    case Type::Unspecified:
    case Type::Unknown:
    case Type::Deg:
        return m_valueInSpecifiedUnits;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void SVGAngleValue::setValue(float value)
{
    switch (m_unitType) {
    case Type::Grad:
        m_valueInSpecifiedUnits = deg2grad(value);
        return;
    case Type::Rad:
        m_valueInSpecifiedUnits = deg2rad(value);
        return;
    case Type::Turn:
        m_valueInSpecifiedUnits = deg2turn(value);
        return;
    case Type::Unspecified:
    case Type::Unknown:
    case Type::Deg:
        m_valueInSpecifiedUnits = value;
        return;
    }
    ASSERT_NOT_REACHED();
}

String SVGAngleValue::valueAsString() const
{
    switch (m_unitType) {
    case Type::Deg:
        return makeString(m_valueInSpecifiedUnits, "deg"_s);
    case Type::Rad:
        return makeString(m_valueInSpecifiedUnits, "rad"_s);
    case Type::Turn:
        return makeString(m_valueInSpecifiedUnits, "turn"_s);
    case Type::Grad:
        return makeString(m_valueInSpecifiedUnits, "grad"_s);
    case Type::Unspecified:
    case Type::Unknown:
        return String::number(m_valueInSpecifiedUnits);
    }

    ASSERT_NOT_REACHED();
    return String();
}

template<typename CharacterType> static inline SVGAngleValue::Type NODELETE parseAngleType(StringParsingBuffer<CharacterType> buffer)
{
    switch (buffer.lengthRemaining()) {
    case 0:
        return SVGAngleValue::Type::Unspecified;
    case 3:
        if (compareCharacters(buffer.position(), 'd', 'e', 'g'))
            return SVGAngleValue::Type::Deg;
        if (compareCharacters(buffer.position(), 'r', 'a', 'd'))
            return SVGAngleValue::Type::Rad;
        break;
    case 4:
        if (compareCharacters(buffer.position(), 'g', 'r', 'a', 'd'))
            return SVGAngleValue::Type::Grad;
        if (compareCharacters(buffer.position(), 't', 'u', 'r', 'n'))
            return SVGAngleValue::Type::Turn;
        break;
    }
    return SVGAngleValue::Type::Unknown;
}

ExceptionOr<void> SVGAngleValue::setValueAsString(const String& value)
{
    if (value.isEmpty()) {
        m_unitType = Type::Unspecified;
        return { };
    }

    return readCharactersForParsing(value, [&](auto buffer) -> ExceptionOr<void> {
        auto valueInSpecifiedUnits = parseNumber(buffer, SuffixSkippingPolicy::DontSkip);
        if (!valueInSpecifiedUnits)
            return Exception { ExceptionCode::SyntaxError };

        auto unitType = parseAngleType(buffer);
        if (unitType == SVGAngleValue::Type::Unknown)
            return Exception { ExceptionCode::SyntaxError };

        m_unitType = unitType;
        m_valueInSpecifiedUnits = *valueInSpecifiedUnits;
        return { };
    });
}

ExceptionOr<void> SVGAngleValue::newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits)
{
    auto type = static_cast<Type>(unitType);
    if (type == Type::Unknown || type > Type::Grad)
        return Exception { ExceptionCode::NotSupportedError };

    m_unitType = type;
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
    return { };
}

ExceptionOr<void> SVGAngleValue::convertToSpecifiedUnits(unsigned short unitType)
{
    auto targetType = static_cast<Type>(unitType);
    if (targetType == Type::Unknown || m_unitType == Type::Unknown || targetType > Type::Grad)
        return Exception { ExceptionCode::NotSupportedError };

    if (targetType == m_unitType)
        return { };

    switch (m_unitType) {
    case Type::Turn:
        switch (targetType) {
        case Type::Grad:
            m_valueInSpecifiedUnits = turn2grad(m_valueInSpecifiedUnits);
            break;
        case Type::Unspecified:
        case Type::Deg:
            m_valueInSpecifiedUnits = turn2deg(m_valueInSpecifiedUnits);
            break;
        case Type::Rad:
            m_valueInSpecifiedUnits = deg2rad(turn2deg(m_valueInSpecifiedUnits));
            break;
        case Type::Turn:
        case Type::Unknown:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case Type::Rad:
        switch (targetType) {
        case Type::Grad:
            m_valueInSpecifiedUnits = rad2grad(m_valueInSpecifiedUnits);
            break;
        case Type::Unspecified:
        case Type::Deg:
            m_valueInSpecifiedUnits = rad2deg(m_valueInSpecifiedUnits);
            break;
        case Type::Turn:
            m_valueInSpecifiedUnits = deg2turn(rad2deg(m_valueInSpecifiedUnits));
            break;
        case Type::Rad:
        case Type::Unknown:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case Type::Grad:
        switch (targetType) {
        case Type::Rad:
            m_valueInSpecifiedUnits = grad2rad(m_valueInSpecifiedUnits);
            break;
        case Type::Unspecified:
        case Type::Deg:
            m_valueInSpecifiedUnits = grad2deg(m_valueInSpecifiedUnits);
            break;
        case Type::Turn:
            m_valueInSpecifiedUnits = grad2turn(m_valueInSpecifiedUnits);
            break;
        case Type::Grad:
        case Type::Unknown:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case Type::Unspecified:
        // Spec: For angles, a unitless value is treated the same as if degrees were specified.
    case Type::Deg:
        switch (targetType) {
        case Type::Rad:
            m_valueInSpecifiedUnits = deg2rad(m_valueInSpecifiedUnits);
            break;
        case Type::Grad:
            m_valueInSpecifiedUnits = deg2grad(m_valueInSpecifiedUnits);
            break;
        case Type::Turn:
            m_valueInSpecifiedUnits = deg2turn(m_valueInSpecifiedUnits);
            break;
        case Type::Unspecified:
        case Type::Deg:
            break;
        case Type::Unknown:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case Type::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    m_unitType = targetType;

    return { };
}

}
