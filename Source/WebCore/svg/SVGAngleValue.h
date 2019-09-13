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

#include "ExceptionOr.h"
#include "SVGPropertyTraits.h"

namespace WebCore {

class SVGAngleValue {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum Type {
        SVG_ANGLETYPE_UNKNOWN = 0,
        SVG_ANGLETYPE_UNSPECIFIED = 1,
        SVG_ANGLETYPE_DEG = 2,
        SVG_ANGLETYPE_RAD = 3,
        SVG_ANGLETYPE_GRAD = 4
    };

    Type unitType() const { return m_unitType; }

    void setValue(float);
    float value() const;

    void setValueInSpecifiedUnits(float valueInSpecifiedUnits) { m_valueInSpecifiedUnits = valueInSpecifiedUnits; }
    float valueInSpecifiedUnits() const { return m_valueInSpecifiedUnits; }

    ExceptionOr<void> setValueAsString(const String&);
    String valueAsString() const;

    ExceptionOr<void> newValueSpecifiedUnits(unsigned short unitType, float valueInSpecifiedUnits);
    ExceptionOr<void> convertToSpecifiedUnits(unsigned short unitType);

private:
    Type m_unitType { SVG_ANGLETYPE_UNSPECIFIED };
    float m_valueInSpecifiedUnits { 0 };
};

}
