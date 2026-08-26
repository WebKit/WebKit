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

#pragma once

#include "SVGPropertyTraits.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

template<typename> class ExceptionOr;

enum class SVGAngleType : uint8_t {
    Unknown = 0,
    Unspecified = 1,
    Deg = 2,
    Rad = 3,
    Grad = 4,
    Turn = 5
};

class SVGAngleValue {
    WTF_MAKE_TZONE_ALLOCATED(SVGAngleValue);
public:
    using Type = SVGAngleType;

    Type unitType() const { return m_unitType; }

    void NODELETE setValue(float);
    float NODELETE value() const;

    void setValueInSpecifiedUnits(float valueInSpecifiedUnits) { m_valueInSpecifiedUnits = valueInSpecifiedUnits; }
    float valueInSpecifiedUnits() const { return m_valueInSpecifiedUnits; }

    ExceptionOr<void> setValueAsString(const String&);
    String valueAsString() const;

    ExceptionOr<void> newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits);
    ExceptionOr<void> convertToSpecifiedUnits(unsigned short unitType);

private:
    Type m_unitType { Type::Unspecified };
    float m_valueInSpecifiedUnits { 0 };
};

}
