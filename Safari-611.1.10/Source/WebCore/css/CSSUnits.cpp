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

#include <wtf/text/TextStream.h>

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

TextStream& operator<<(TextStream& ts, CSSUnitCategory category)
{
    switch (category) {
    case CSSUnitCategory::Number: ts << "Number"; break;
    case CSSUnitCategory::Percent: ts << "Percent"; break;
    case CSSUnitCategory::Length: ts << "Length"; break;
    case CSSUnitCategory::Angle: ts << "Angle"; break;
    case CSSUnitCategory::Time: ts << "Time"; break;
    case CSSUnitCategory::Frequency: ts << "Frequency"; break;
    case CSSUnitCategory::Resolution: ts << "Resolution"; break;
    case CSSUnitCategory::Other: ts << "Other"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, CSSUnitType unitType)
{
    switch (unitType) {
    case CSSUnitType::CSS_UNKNOWN: ts << "unknown"; break;
    case CSSUnitType::CSS_NUMBER: ts << "number"; break;
    case CSSUnitType::CSS_PERCENTAGE: ts << "percentage"; break;
    case CSSUnitType::CSS_EMS: ts << "ems"; break;
    case CSSUnitType::CSS_EXS: ts << "exs"; break;
    case CSSUnitType::CSS_PX: ts << "px"; break;
    case CSSUnitType::CSS_CM: ts << "cm"; break;
    case CSSUnitType::CSS_MM: ts << "mm"; break;
    case CSSUnitType::CSS_IN: ts << "in"; break;
    case CSSUnitType::CSS_PT: ts << "pt"; break;
    case CSSUnitType::CSS_PC: ts << "pc"; break;
    case CSSUnitType::CSS_DEG: ts << "deg"; break;
    case CSSUnitType::CSS_RAD: ts << "rad"; break;
    case CSSUnitType::CSS_GRAD: ts << "grad"; break;
    case CSSUnitType::CSS_MS: ts << "ms"; break;
    case CSSUnitType::CSS_S: ts << "s"; break;
    case CSSUnitType::CSS_HZ: ts << "hz"; break;
    case CSSUnitType::CSS_KHZ: ts << "khz"; break;
    case CSSUnitType::CSS_DIMENSION: ts << "dimension"; break;
    case CSSUnitType::CSS_STRING: ts << "string"; break;
    case CSSUnitType::CSS_URI: ts << "uri"; break;
    case CSSUnitType::CSS_IDENT: ts << "ident"; break;
    case CSSUnitType::CSS_ATTR: ts << "attr"; break;
    case CSSUnitType::CSS_COUNTER: ts << "counter"; break;
    case CSSUnitType::CSS_RECT: ts << "rect"; break;
    case CSSUnitType::CSS_RGBCOLOR: ts << "rgbcolor"; break;
    case CSSUnitType::CSS_VW: ts << "vw"; break;
    case CSSUnitType::CSS_VH: ts << "vh"; break;
    case CSSUnitType::CSS_VMIN: ts << "vmin"; break;
    case CSSUnitType::CSS_VMAX: ts << "vmax"; break;
    case CSSUnitType::CSS_DPPX: ts << "dppx"; break;
    case CSSUnitType::CSS_DPI: ts << "dpi"; break;
    case CSSUnitType::CSS_DPCM: ts << "dpcm"; break;
    case CSSUnitType::CSS_FR: ts << "fr"; break;
    case CSSUnitType::CSS_Q: ts << "q"; break;
    case CSSUnitType::CSS_LHS: ts << "lh"; break;
    case CSSUnitType::CSS_RLHS: ts << "rlh"; break;
    case CSSUnitType::CSS_PAIR: ts << "pair"; break;
    case CSSUnitType::CSS_UNICODE_RANGE: ts << "unicode_range"; break;
    case CSSUnitType::CSS_TURN: ts << "turn"; break;
    case CSSUnitType::CSS_REMS: ts << "rems"; break;
    case CSSUnitType::CSS_CHS: ts << "chs"; break;
    case CSSUnitType::CSS_COUNTER_NAME: ts << "counter_name"; break;
    case CSSUnitType::CSS_SHAPE: ts << "shape"; break;
    case CSSUnitType::CSS_QUAD: ts << "quad"; break;
    case CSSUnitType::CSS_CALC: ts << "calc"; break;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER: ts << "calc_percentage_with_number"; break;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH: ts << "calc_percentage_with_length"; break;
    case CSSUnitType::CSS_FONT_FAMILY: ts << "font_family"; break;
    case CSSUnitType::CSS_PROPERTY_ID: ts << "property_id"; break;
    case CSSUnitType::CSS_VALUE_ID: ts << "value_id"; break;
    case CSSUnitType::CSS_QUIRKY_EMS: ts << "quirky_ems"; break;
    }
    return ts;
}

} // namespace WebCore
