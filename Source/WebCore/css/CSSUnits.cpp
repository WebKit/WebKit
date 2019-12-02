/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2012, 2013, 2019 Apple Inc. All rights reserved.
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
#include "CSSUnits.h"

namespace WebCore {

CSSUnitCategory unitCategory(CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::CSS_NUMBER:
        return CSSUnitCategory::Number;
    case CSSUnitType::CSS_PERCENTAGE:
        return CSSUnitCategory::Percent;
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_Q:
        return CSSUnitCategory::Length;
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
        return CSSUnitCategory::Time;
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
        return CSSUnitCategory::Angle;
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
        return CSSUnitCategory::Frequency;
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
        return CSSUnitCategory::Resolution;
    default:
        return CSSUnitCategory::Other;
    }
}

CSSUnitType canonicalUnitTypeForCategory(CSSUnitCategory category)
{
    switch (category) {
    case CSSUnitCategory::Number:
        return CSSUnitType::CSS_NUMBER;
    case CSSUnitCategory::Length:
        return CSSUnitType::CSS_PX;
    case CSSUnitCategory::Percent:
        return CSSUnitType::CSS_UNKNOWN; // Cannot convert between numbers and percent.
    case CSSUnitCategory::Time:
        return CSSUnitType::CSS_MS;
    case CSSUnitCategory::Angle:
        return CSSUnitType::CSS_DEG;
    case CSSUnitCategory::Frequency:
        return CSSUnitType::CSS_HZ;
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
    case CSSUnitCategory::Resolution:
        return CSSUnitType::CSS_DPPX;
#endif
    default:
        return CSSUnitType::CSS_UNKNOWN;
    }
}

CSSUnitType canonicalUnitType(CSSUnitType unitType)
{
    return canonicalUnitTypeForCategory(unitCategory(unitType));
}

} // namespace WebCore
