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
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNKNOWN:
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
    case CSSUnitCategory::Number: ts << "Number"_s; break;
    case CSSUnitCategory::Percent: ts << "Percent"_s; break;
    case CSSUnitCategory::AbsoluteLength: ts << "AsboluteLength"_s; break;
    case CSSUnitCategory::ViewportPercentageLength: ts << "ViewportPercentageLength"_s; break;
    case CSSUnitCategory::FontRelativeLength: ts << "FontRelativeLength"_s; break;
    case CSSUnitCategory::Angle: ts << "Angle"_s; break;
    case CSSUnitCategory::Time: ts << "Time"_s; break;
    case CSSUnitCategory::Frequency: ts << "Frequency"_s; break;
    case CSSUnitCategory::Resolution: ts << "Resolution"_s; break;
    case CSSUnitCategory::Flex: ts << "Flex"_s; break;
    case CSSUnitCategory::Other: ts << "Other"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, CSSUnitType unitType)
{
    switch (unitType) {
    case CSSUnitType::CSS_UNKNOWN: ts << "unknown"_s; break;
    case CSSUnitType::CSS_NUMBER: ts << "number"_s; break;
    case CSSUnitType::CSS_INTEGER: ts << "integer"_s; break;
    case CSSUnitType::CSS_PERCENTAGE: ts << "percentage"_s; break;
    case CSSUnitType::CSS_EM: ts << "em"_s; break;
    case CSSUnitType::CSS_EX: ts << "ex"_s; break;
    case CSSUnitType::CSS_PX: ts << "px"_s; break;
    case CSSUnitType::CSS_CM: ts << "cm"_s; break;
    case CSSUnitType::CSS_MM: ts << "mm"_s; break;
    case CSSUnitType::CSS_IN: ts << "in"_s; break;
    case CSSUnitType::CSS_PT: ts << "pt"_s; break;
    case CSSUnitType::CSS_PC: ts << "pc"_s; break;
    case CSSUnitType::CSS_DEG: ts << "deg"_s; break;
    case CSSUnitType::CSS_RAD: ts << "rad"_s; break;
    case CSSUnitType::CSS_GRAD: ts << "grad"_s; break;
    case CSSUnitType::CSS_MS: ts << "ms"_s; break;
    case CSSUnitType::CSS_S: ts << 's'; break;
    case CSSUnitType::CSS_HZ: ts << "hz"_s; break;
    case CSSUnitType::CSS_KHZ: ts << "khz"_s; break;
    case CSSUnitType::CSS_DIMENSION: ts << "dimension"_s; break;
    case CSSUnitType::CSS_STRING: ts << "string"_s; break;
    case CSSUnitType::CSS_URI: ts << "uri"_s; break;
    case CSSUnitType::CSS_IDENT: ts << "ident"_s; break;
    case CSSUnitType::CustomIdent: ts << "custom-ident"_s; break;
    case CSSUnitType::CSS_ATTR: ts << "attr"_s; break;
    case CSSUnitType::CSS_VW: ts << "vw"_s; break;
    case CSSUnitType::CSS_VH: ts << "vh"_s; break;
    case CSSUnitType::CSS_VMIN: ts << "vmin"_s; break;
    case CSSUnitType::CSS_VMAX: ts << "vmax"_s; break;
    case CSSUnitType::CSS_VB: ts << "vb"_s; break;
    case CSSUnitType::CSS_VI: ts << "vi"_s; break;
    case CSSUnitType::CSS_SVW: ts << "svw"_s; break;
    case CSSUnitType::CSS_SVH: ts << "svh"_s; break;
    case CSSUnitType::CSS_SVMIN: ts << "svmin"_s; break;
    case CSSUnitType::CSS_SVMAX: ts << "svmax"_s; break;
    case CSSUnitType::CSS_SVB: ts << "svb"_s; break;
    case CSSUnitType::CSS_SVI: ts << "svi"_s; break;
    case CSSUnitType::CSS_LVW: ts << "lvw"_s; break;
    case CSSUnitType::CSS_LVH: ts << "lvh"_s; break;
    case CSSUnitType::CSS_LVMIN: ts << "lvmin"_s; break;
    case CSSUnitType::CSS_LVMAX: ts << "lvmax"_s; break;
    case CSSUnitType::CSS_LVB: ts << "lvb"_s; break;
    case CSSUnitType::CSS_LVI: ts << "lvi"_s; break;
    case CSSUnitType::CSS_DVW: ts << "dvw"_s; break;
    case CSSUnitType::CSS_DVH: ts << "dvh"_s; break;
    case CSSUnitType::CSS_DVMIN: ts << "dvmin"_s; break;
    case CSSUnitType::CSS_DVMAX: ts << "dvmax"_s; break;
    case CSSUnitType::CSS_DVB: ts << "dvb"_s; break;
    case CSSUnitType::CSS_DVI: ts << "dvi"_s; break;
    case CSSUnitType::CSS_DPPX: ts << "dppx"_s; break;
    case CSSUnitType::CSS_X: ts << 'x'; break;
    case CSSUnitType::CSS_DPI: ts << "dpi"_s; break;
    case CSSUnitType::CSS_DPCM: ts << "dpcm"_s; break;
    case CSSUnitType::CSS_FR: ts << "fr"_s; break;
    case CSSUnitType::CSS_Q: ts << 'q'; break;
    case CSSUnitType::CSS_LH: ts << "lh"_s; break;
    case CSSUnitType::CSS_RLH: ts << "rlh"_s; break;
    case CSSUnitType::CSS_CQW: ts << "cqw"_s; break;
    case CSSUnitType::CSS_CQH: ts << "cqh"_s; break;
    case CSSUnitType::CSS_CQI: ts << "cqi"_s; break;
    case CSSUnitType::CSS_CQB: ts << "cqb"_s; break;
    case CSSUnitType::CSS_CQMAX: ts << "cqmax"_s; break;
    case CSSUnitType::CSS_CQMIN: ts << "cqmin"_s; break;
    case CSSUnitType::CSS_TURN: ts << "turn"_s; break;
    case CSSUnitType::CSS_RCAP: ts << "rcap"_s; break;
    case CSSUnitType::CSS_RCH: ts << "rch"_s; break;
    case CSSUnitType::CSS_REM: ts << "rem"_s; break;
    case CSSUnitType::CSS_REX: ts << "rex"_s; break;
    case CSSUnitType::CSS_RIC: ts << "ric"_s; break;
    case CSSUnitType::CSS_CAP: ts << "cap"_s; break;
    case CSSUnitType::CSS_CH: ts << "ch"_s; break;
    case CSSUnitType::CSS_IC: ts << "ic"_s; break;
    case CSSUnitType::CSS_CALC: ts << "calc"_s; break;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE: ts << "calc_percentage_with_angle"_s; break;
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH: ts << "calc_percentage_with_length"_s; break;
    case CSSUnitType::CSS_FONT_FAMILY: ts << "font_family"_s; break;
    case CSSUnitType::CSS_PROPERTY_ID: ts << "property_id"_s; break;
    case CSSUnitType::CSS_VALUE_ID: ts << "value_id"_s; break;
    case CSSUnitType::CSS_QUIRKY_EM: ts << "quirky_em"_s; break;
    }
    return ts;
}

double conversionToCanonicalUnitsScaleFactor(CSSUnitType unit)
{
    switch (unit) {
    case CSSUnitType::CSS_MS:
        return CSS::secondsPerMillisecond;
    case CSSUnitType::CSS_CM:
        return CSS::pixelsPerCm;
    case CSSUnitType::CSS_DPCM:
        return CSS::dppxPerDpcm;
    case CSSUnitType::CSS_MM:
        return CSS::pixelsPerMm;
    case CSSUnitType::CSS_Q:
        return CSS::pixelsPerQ;
    case CSSUnitType::CSS_IN:
        return CSS::pixelsPerInch;
    case CSSUnitType::CSS_DPI:
        return CSS::dppxPerDpi;
    case CSSUnitType::CSS_PT:
        return CSS::pixelsPerPt;
    case CSSUnitType::CSS_PC:
        return CSS::pixelsPerPc;
    case CSSUnitType::CSS_RAD:
        return degreesPerRadianDouble;
    case CSSUnitType::CSS_GRAD:
        return degreesPerGradientDouble;
    case CSSUnitType::CSS_TURN:
        return degreesPerTurnDouble;
    case CSSUnitType::CSS_KHZ:
        return CSS::hertzPerKilohertz;
    default:
        return 1.0;
    }
}

bool conversionToCanonicalUnitRequiresConversionData(CSSUnitType unit)
{
    switch (unit) {
    case CSSUnitType::CSS_CM:
    case CSSUnitType::CSS_MM:
    case CSSUnitType::CSS_Q:
    case CSSUnitType::CSS_IN:
    case CSSUnitType::CSS_PT:
    case CSSUnitType::CSS_PC:
    case CSSUnitType::CSS_EM:
    case CSSUnitType::CSS_EX:
    case CSSUnitType::CSS_LH:
    case CSSUnitType::CSS_CAP:
    case CSSUnitType::CSS_CH:
    case CSSUnitType::CSS_IC:
    case CSSUnitType::CSS_RCAP:
    case CSSUnitType::CSS_RCH:
    case CSSUnitType::CSS_REM:
    case CSSUnitType::CSS_REX:
    case CSSUnitType::CSS_RIC:
    case CSSUnitType::CSS_RLH:
    case CSSUnitType::CSS_VW:
    case CSSUnitType::CSS_VH:
    case CSSUnitType::CSS_VMIN:
    case CSSUnitType::CSS_VMAX:
    case CSSUnitType::CSS_VB:
    case CSSUnitType::CSS_VI:
    case CSSUnitType::CSS_SVW:
    case CSSUnitType::CSS_SVH:
    case CSSUnitType::CSS_SVMIN:
    case CSSUnitType::CSS_SVMAX:
    case CSSUnitType::CSS_SVB:
    case CSSUnitType::CSS_SVI:
    case CSSUnitType::CSS_LVW:
    case CSSUnitType::CSS_LVH:
    case CSSUnitType::CSS_LVMIN:
    case CSSUnitType::CSS_LVMAX:
    case CSSUnitType::CSS_LVB:
    case CSSUnitType::CSS_LVI:
    case CSSUnitType::CSS_DVW:
    case CSSUnitType::CSS_DVH:
    case CSSUnitType::CSS_DVMIN:
    case CSSUnitType::CSS_DVMAX:
    case CSSUnitType::CSS_DVB:
    case CSSUnitType::CSS_DVI:
    case CSSUnitType::CSS_CQW:
    case CSSUnitType::CSS_CQH:
    case CSSUnitType::CSS_CQI:
    case CSSUnitType::CSS_CQB:
    case CSSUnitType::CSS_CQMIN:
    case CSSUnitType::CSS_CQMAX:
        return true;

    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_RAD:
    case CSSUnitType::CSS_GRAD:
    case CSSUnitType::CSS_TURN:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_MS:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_KHZ:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_X:
    case CSSUnitType::CSS_DPI:
    case CSSUnitType::CSS_DPCM:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_VALUE_ID:
    case CSSUnitType::CustomIdent:
        break;
    }

    return false;
}

} // namespace WebCore
