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
    case CSSUnitType::CSS_INTEGER:
        return CSSUnitCategory::Number;
    case CSSUnitType::CSS_PERCENTAGE:
        return CSSUnitCategory::Percent;
    // https://drafts.csswg.org/css-values-4/#absolute-lengths
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_Q:
        return CSSUnitCategory::AbsoluteLength;
    // https://drafts.csswg.org/css-values-4/#font-relative-lengths
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_RLH:
        return CSSUnitCategory::FontRelativeLength;
    // https://drafts.csswg.org/css-values-4/#viewport-relative-lengths
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_DVMAX:
        return CSSUnitCategory::ViewportPercentageLength;
    // https://drafts.csswg.org/css-values-4/#time
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_S:
        return CSSUnitCategory::Time;
    // https://drafts.csswg.org/css-values-4/#angles
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
        return CSSUnitCategory::Angle;
    // https://drafts.csswg.org/css-values-4/#frequency
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
        return CSSUnitCategory::Frequency;
    // https://drafts.csswg.org/css-values-4/#resolution
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
        return CSSUnitCategory::Resolution;
    case CSSUnitType::CSS_FR:
        return CSSUnitCategory::Flex;
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_RGBCOLOR:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_UNRESOLVED_COLOR:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_VALUE_ID:
    case CSSUnitType::CustomIdent:
        return CSSUnitCategory::Other;
    }
    ASSERT_NOT_REACHED();
    return CSSUnitCategory::Other;
}

CSSUnitType canonicalUnitTypeForCategory(CSSUnitCategory category)
{
    switch (category) {
    case CSSUnitCategory::Number:
        return CSSUnitType::CSS_NUMBER;
    case CSSUnitCategory::AbsoluteLength:
        return CSSUnitType::CSS_PX;
    case CSSUnitCategory::Percent:
        return CSSUnitType::CSS_PERCENTAGE;
    case CSSUnitCategory::Time:
        return CSSUnitType::CSS_S;
    case CSSUnitCategory::Angle:
        return CSSUnitType::CSS_DEG;
    case CSSUnitCategory::Frequency:
        return CSSUnitType::CSS_HZ;
    case CSSUnitCategory::Resolution:
        return CSSUnitType::CSS_DPPX;
    case CSSUnitCategory::Flex:
        return CSSUnitType::CSS_FR;
    case CSSUnitCategory::FontRelativeLength:
    case CSSUnitCategory::ViewportPercentageLength:
    case CSSUnitCategory::Other:
        return CSSUnitType::CSS_UNKNOWN;
    }
    ASSERT_NOT_REACHED();
    return CSSUnitType::CSS_UNKNOWN;
}

CSSUnitType canonicalUnitTypeForUnitType(CSSUnitType unitType)
{
    return canonicalUnitTypeForCategory(unitCategory(unitType));
}

TextStream& operator<<(TextStream& ts, CSSUnitCategory category)
{
    switch (category) {
    case CSSUnitCategory::Number: ts << "Number"; break;
    case CSSUnitCategory::Percent: ts << "Percent"; break;
    case CSSUnitCategory::AbsoluteLength: ts << "AsboluteLength"; break;
    case CSSUnitCategory::ViewportPercentageLength: ts << "ViewportPercentageLength"; break;
    case CSSUnitCategory::FontRelativeLength: ts << "FontRelativeLength"; break;
    case CSSUnitCategory::Angle: ts << "Angle"; break;
    case CSSUnitCategory::Time: ts << "Time"; break;
    case CSSUnitCategory::Frequency: ts << "Frequency"; break;
    case CSSUnitCategory::Resolution: ts << "Resolution"; break;
    case CSSUnitCategory::Flex: ts << "Flex"; break;
    case CSSUnitCategory::Other: ts << "Other"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, CSSUnitType unitType)
{
    switch (unitType) {
    case CSSUnitType::CSS_UNKNOWN: ts << "unknown"; break;
    case CSSUnitType::CSS_NUMBER: ts << "number"; break;
    case CSSUnitType::CSS_INTEGER: ts << "integer"; break;
    case CSSUnitType::CSS_PERCENTAGE: ts << "percentage"; break;
    case CSSUnitType::CSS_EM: ts << "em"; break;
    case CSSUnitType::CSS_EX: ts << "ex"; break;
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
    case CSSUnitType::CustomIdent: ts << "custom-ident"; break;
    case CSSUnitType::CSS_ATTR: ts << "attr"; break;
    case CSSUnitType::CSS_RGBCOLOR: ts << "rgbcolor"; break;
    case CSSUnitType::CSS_VW: ts << "vw"; break;
    case CSSUnitType::CSS_VH: ts << "vh"; break;
    case CSSUnitType::CSS_VMIN: ts << "vmin"; break;
    case CSSUnitType::CSS_VMAX: ts << "vmax"; break;
    case CSSUnitType::CSS_VB: ts << "vb"; break;
    case CSSUnitType::CSS_VI: ts << "vi"; break;
    case CSSUnitType::CSS_SVW: ts << "svw"; break;
    case CSSUnitType::CSS_SVH: ts << "svh"; break;
    case CSSUnitType::CSS_SVMIN: ts << "svmin"; break;
    case CSSUnitType::CSS_SVMAX: ts << "svmax"; break;
    case CSSUnitType::CSS_SVB: ts << "svb"; break;
    case CSSUnitType::CSS_SVI: ts << "svi"; break;
    case CSSUnitType::CSS_LVW: ts << "lvw"; break;
    case CSSUnitType::CSS_LVH: ts << "lvh"; break;
    case CSSUnitType::CSS_LVMIN: ts << "lvmin"; break;
    case CSSUnitType::CSS_LVMAX: ts << "lvmax"; break;
    case CSSUnitType::CSS_LVB: ts << "lvb"; break;
    case CSSUnitType::CSS_LVI: ts << "lvi"; break;
    case CSSUnitType::CSS_DVW: ts << "dvw"; break;
    case CSSUnitType::CSS_DVH: ts << "dvh"; break;
    case CSSUnitType::CSS_DVMIN: ts << "dvmin"; break;
    case CSSUnitType::CSS_DVMAX: ts << "dvmax"; break;
    case CSSUnitType::CSS_DVB: ts << "dvb"; break;
    case CSSUnitType::CSS_DVI: ts << "dvi"; break;
    case CSSUnitType::CSS_DPPX: ts << "dppx"; break;
    case CSSUnitType::CSS_X: ts << "x"; break;
    case CSSUnitType::CSS_DPI: ts << "dpi"; break;
    case CSSUnitType::CSS_DPCM: ts << "dpcm"; break;
    case CSSUnitType::CSS_FR: ts << "fr"; break;
    case CSSUnitType::CSS_Q: ts << "q"; break;
    case CSSUnitType::CSS_LH: ts << "lh"; break;
    case CSSUnitType::CSS_RLH: ts << "rlh"; break;
    case CSSUnitType::CSS_CQW: ts << "cqw"; break;
    case CSSUnitType::CSS_CQH: ts << "cqh"; break;
    case CSSUnitType::CSS_CQI: ts << "cqi"; break;
    case CSSUnitType::CSS_CQB: ts << "cqb"; break;
    case CSSUnitType::CSS_CQMAX: ts << "cqmax"; break;
    case CSSUnitType::CSS_CQMIN: ts << "cqmin"; break;
    case CSSUnitType::CSS_TURN: ts << "turn"; break;
    case CSSUnitType::CSS_RCAP: ts << "rcap"; break;
    case CSSUnitType::CSS_RCH: ts << "rch"; break;
    case CSSUnitType::CSS_REM: ts << "rem"; break;
    case CSSUnitType::CSS_REX: ts << "rex"; break;
    case CSSUnitType::CSS_RIC: ts << "ric"; break;
    case CSSUnitType::CSS_CAP: ts << "cap"; break;
    case CSSUnitType::CSS_CH: ts << "ch"; break;
    case CSSUnitType::CSS_IC: ts << "ic"; break;
    case CSSUnitType::CSS_CALC: ts << "calc"; break;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_NUMBER: ts << "calc_percentage_with_number"; break;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH: ts << "calc_percentage_with_length"; break;
    case CSSUnitType::CSS_UNRESOLVED_COLOR: ts << "unresolved_color"; break;
    case CSSUnitType::CSS_FONT_FAMILY: ts << "font_family"; break;
    case CSSUnitType::CSS_PROPERTY_ID: ts << "property_id"; break;
    case CSSUnitType::CSS_VALUE_ID: ts << "value_id"; break;
    case CSSUnitType::CSS_QUIRKY_EM: ts << "quirky_em"; break;
    }
    return ts;
}

} // namespace WebCore
