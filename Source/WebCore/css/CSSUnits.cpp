/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2012, 2013, 2019 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
        return CSSUnitCategory::Number;
    case CSSUnitType::Percentage:
        return CSSUnitCategory::Percent;
    // https://drafts.csswg.org/css-values-4/#absolute-lengths
    case CSSUnitType::Px:
    case CSSUnitType::Cm:
    case CSSUnitType::Mm:
    case CSSUnitType::In:
    case CSSUnitType::Pt:
    case CSSUnitType::Pc:
    case CSSUnitType::Q:
        return CSSUnitCategory::AbsoluteLength;
    // https://drafts.csswg.org/css-values-4/#font-relative-lengths
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::Lh:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Rlh:
        return CSSUnitCategory::FontRelativeLength;
    // https://drafts.csswg.org/css-values-4/#viewport-relative-lengths
    case CSSUnitType::Vw:
    case CSSUnitType::Svw:
    case CSSUnitType::Lvw:
    case CSSUnitType::Dvw:
    case CSSUnitType::Vh:
    case CSSUnitType::Svh:
    case CSSUnitType::Lvh:
    case CSSUnitType::Dvh:
    case CSSUnitType::Vi:
    case CSSUnitType::Svi:
    case CSSUnitType::Lvi:
    case CSSUnitType::Dvi:
    case CSSUnitType::Vb:
    case CSSUnitType::Svb:
    case CSSUnitType::Lvb:
    case CSSUnitType::Dvb:
    case CSSUnitType::Vmin:
    case CSSUnitType::Lvmin:
    case CSSUnitType::Svmin:
    case CSSUnitType::Dvmin:
    case CSSUnitType::Vmax:
    case CSSUnitType::Svmax:
    case CSSUnitType::Lvmax:
    case CSSUnitType::Dvmax:
        return CSSUnitCategory::ViewportPercentageLength;
    // https://drafts.csswg.org/css-values-4/#time
    case CSSUnitType::Ms:
    case CSSUnitType::S:
        return CSSUnitCategory::Time;
    // https://drafts.csswg.org/css-values-4/#angles
    case CSSUnitType::Deg:
    case CSSUnitType::Rad:
    case CSSUnitType::Grad:
    case CSSUnitType::Turn:
        return CSSUnitCategory::Angle;
    // https://drafts.csswg.org/css-values-4/#frequency
    case CSSUnitType::Hz:
    case CSSUnitType::Khz:
        return CSSUnitCategory::Frequency;
    // https://drafts.csswg.org/css-values-4/#resolution
    case CSSUnitType::Dppx:
    case CSSUnitType::X:
    case CSSUnitType::Dpi:
    case CSSUnitType::Dpcm:
        return CSSUnitCategory::Resolution;
    case CSSUnitType::Fr:
        return CSSUnitCategory::Flex;
    case CSSUnitType::Cqw:
    case CSSUnitType::Cqh:
    case CSSUnitType::Cqi:
    case CSSUnitType::Cqb:
    case CSSUnitType::Cqmin:
    case CSSUnitType::Cqmax:
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Unknown:
        return CSSUnitCategory::Other;
    }
    ASSERT_NOT_REACHED();
    return CSSUnitCategory::Other;
}

CSSUnitType canonicalUnitTypeForCategory(CSSUnitCategory category)
{
    switch (category) {
    case CSSUnitCategory::Number:
        return CSSUnitType::Number;
    case CSSUnitCategory::AbsoluteLength:
        return CSSUnitType::Px;
    case CSSUnitCategory::Percent:
        return CSSUnitType::Percentage;
    case CSSUnitCategory::Time:
        return CSSUnitType::S;
    case CSSUnitCategory::Angle:
        return CSSUnitType::Deg;
    case CSSUnitCategory::Frequency:
        return CSSUnitType::Hz;
    case CSSUnitCategory::Resolution:
        return CSSUnitType::Dppx;
    case CSSUnitCategory::Flex:
        return CSSUnitType::Fr;
    case CSSUnitCategory::FontRelativeLength:
    case CSSUnitCategory::ViewportPercentageLength:
    case CSSUnitCategory::Other:
        return CSSUnitType::Unknown;
    }
    ASSERT_NOT_REACHED();
    return CSSUnitType::Unknown;
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
    case CSSUnitType::Unknown: ts << "unknown"_s; break;
    case CSSUnitType::Number: ts << "number"_s; break;
    case CSSUnitType::Integer: ts << "integer"_s; break;
    case CSSUnitType::Percentage: ts << "percentage"_s; break;
    case CSSUnitType::Em: ts << "em"_s; break;
    case CSSUnitType::Ex: ts << "ex"_s; break;
    case CSSUnitType::Px: ts << "px"_s; break;
    case CSSUnitType::Cm: ts << "cm"_s; break;
    case CSSUnitType::Mm: ts << "mm"_s; break;
    case CSSUnitType::In: ts << "in"_s; break;
    case CSSUnitType::Pt: ts << "pt"_s; break;
    case CSSUnitType::Pc: ts << "pc"_s; break;
    case CSSUnitType::Deg: ts << "deg"_s; break;
    case CSSUnitType::Rad: ts << "rad"_s; break;
    case CSSUnitType::Grad: ts << "grad"_s; break;
    case CSSUnitType::Ms: ts << "ms"_s; break;
    case CSSUnitType::S: ts << 's'; break;
    case CSSUnitType::Hz: ts << "hz"_s; break;
    case CSSUnitType::Khz: ts << "khz"_s; break;
    case CSSUnitType::Vw: ts << "vw"_s; break;
    case CSSUnitType::Vh: ts << "vh"_s; break;
    case CSSUnitType::Vmin: ts << "vmin"_s; break;
    case CSSUnitType::Vmax: ts << "vmax"_s; break;
    case CSSUnitType::Vb: ts << "vb"_s; break;
    case CSSUnitType::Vi: ts << "vi"_s; break;
    case CSSUnitType::Svw: ts << "svw"_s; break;
    case CSSUnitType::Svh: ts << "svh"_s; break;
    case CSSUnitType::Svmin: ts << "svmin"_s; break;
    case CSSUnitType::Svmax: ts << "svmax"_s; break;
    case CSSUnitType::Svb: ts << "svb"_s; break;
    case CSSUnitType::Svi: ts << "svi"_s; break;
    case CSSUnitType::Lvw: ts << "lvw"_s; break;
    case CSSUnitType::Lvh: ts << "lvh"_s; break;
    case CSSUnitType::Lvmin: ts << "lvmin"_s; break;
    case CSSUnitType::Lvmax: ts << "lvmax"_s; break;
    case CSSUnitType::Lvb: ts << "lvb"_s; break;
    case CSSUnitType::Lvi: ts << "lvi"_s; break;
    case CSSUnitType::Dvw: ts << "dvw"_s; break;
    case CSSUnitType::Dvh: ts << "dvh"_s; break;
    case CSSUnitType::Dvmin: ts << "dvmin"_s; break;
    case CSSUnitType::Dvmax: ts << "dvmax"_s; break;
    case CSSUnitType::Dvb: ts << "dvb"_s; break;
    case CSSUnitType::Dvi: ts << "dvi"_s; break;
    case CSSUnitType::Dppx: ts << "dppx"_s; break;
    case CSSUnitType::X: ts << 'x'; break;
    case CSSUnitType::Dpi: ts << "dpi"_s; break;
    case CSSUnitType::Dpcm: ts << "dpcm"_s; break;
    case CSSUnitType::Fr: ts << "fr"_s; break;
    case CSSUnitType::Q: ts << 'q'; break;
    case CSSUnitType::Lh: ts << "lh"_s; break;
    case CSSUnitType::Rlh: ts << "rlh"_s; break;
    case CSSUnitType::Cqw: ts << "cqw"_s; break;
    case CSSUnitType::Cqh: ts << "cqh"_s; break;
    case CSSUnitType::Cqi: ts << "cqi"_s; break;
    case CSSUnitType::Cqb: ts << "cqb"_s; break;
    case CSSUnitType::Cqmax: ts << "cqmax"_s; break;
    case CSSUnitType::Cqmin: ts << "cqmin"_s; break;
    case CSSUnitType::Turn: ts << "turn"_s; break;
    case CSSUnitType::Rcap: ts << "rcap"_s; break;
    case CSSUnitType::Rch: ts << "rch"_s; break;
    case CSSUnitType::Rem: ts << "rem"_s; break;
    case CSSUnitType::Rex: ts << "rex"_s; break;
    case CSSUnitType::Ric: ts << "ric"_s; break;
    case CSSUnitType::Cap: ts << "cap"_s; break;
    case CSSUnitType::Ch: ts << "ch"_s; break;
    case CSSUnitType::Ic: ts << "ic"_s; break;
    case CSSUnitType::Calc: ts << "calc"_s; break;
    case CSSUnitType::CalcPercentageWithAngle: ts << "calc_percentage_with_angle"_s; break;
    case CSSUnitType::CalcPercentageWithLength: ts << "calc_percentage_with_length"_s; break;
    case CSSUnitType::QuirkyEm: ts << "quirky_em"_s; break;
    }
    return ts;
}

std::optional<double> conversionToCanonicalUnitsScaleFactor(CSSUnitType unit)
{
    switch (unit) {
    case CSSUnitType::Px:
    case CSSUnitType::Deg:
    case CSSUnitType::S:
    case CSSUnitType::Hz:
    case CSSUnitType::Dppx:
        return 1.0;
    case CSSUnitType::Ms:
        return CSS::secondsPerMillisecond;
    case CSSUnitType::Cm:
        return CSS::pixelsPerCm;
    case CSSUnitType::Dpcm:
        return CSS::dppxPerDpcm;
    case CSSUnitType::Mm:
        return CSS::pixelsPerMm;
    case CSSUnitType::Q:
        return CSS::pixelsPerQ;
    case CSSUnitType::In:
        return CSS::pixelsPerInch;
    case CSSUnitType::Dpi:
        return CSS::dppxPerDpi;
    case CSSUnitType::Pt:
        return CSS::pixelsPerPt;
    case CSSUnitType::Pc:
        return CSS::pixelsPerPc;
    case CSSUnitType::Rad:
        return degreesPerRadianDouble;
    case CSSUnitType::Grad:
        return degreesPerGradientDouble;
    case CSSUnitType::Turn:
        return degreesPerTurnDouble;
    case CSSUnitType::Khz:
        return CSS::hertzPerKilohertz;
    default:
        return std::nullopt;
    }
}

bool conversionToCanonicalUnitRequiresConversionData(CSSUnitType unit)
{
    switch (unit) {
    case CSSUnitType::Cm:
    case CSSUnitType::Mm:
    case CSSUnitType::Q:
    case CSSUnitType::In:
    case CSSUnitType::Pt:
    case CSSUnitType::Pc:
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::Lh:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Rlh:
    case CSSUnitType::Vw:
    case CSSUnitType::Vh:
    case CSSUnitType::Vmin:
    case CSSUnitType::Vmax:
    case CSSUnitType::Vb:
    case CSSUnitType::Vi:
    case CSSUnitType::Svw:
    case CSSUnitType::Svh:
    case CSSUnitType::Svmin:
    case CSSUnitType::Svmax:
    case CSSUnitType::Svb:
    case CSSUnitType::Svi:
    case CSSUnitType::Lvw:
    case CSSUnitType::Lvh:
    case CSSUnitType::Lvmin:
    case CSSUnitType::Lvmax:
    case CSSUnitType::Lvb:
    case CSSUnitType::Lvi:
    case CSSUnitType::Dvw:
    case CSSUnitType::Dvh:
    case CSSUnitType::Dvmin:
    case CSSUnitType::Dvmax:
    case CSSUnitType::Dvb:
    case CSSUnitType::Dvi:
    case CSSUnitType::Cqw:
    case CSSUnitType::Cqh:
    case CSSUnitType::Cqi:
    case CSSUnitType::Cqb:
    case CSSUnitType::Cqmin:
    case CSSUnitType::Cqmax:
        return true;

    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Px:
    case CSSUnitType::Deg:
    case CSSUnitType::Rad:
    case CSSUnitType::Grad:
    case CSSUnitType::Turn:
    case CSSUnitType::S:
    case CSSUnitType::Ms:
    case CSSUnitType::Hz:
    case CSSUnitType::Khz:
    case CSSUnitType::Dppx:
    case CSSUnitType::X:
    case CSSUnitType::Dpi:
    case CSSUnitType::Dpcm:
    case CSSUnitType::Fr:
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Unknown:
        break;
    }

    return false;
}

ASCIILiteral unitTypeString(CSSUnitType unitType)
{
    switch (unitType) {
    case CSSUnitType::Cap: return "cap"_s;
    case CSSUnitType::Ch: return "ch"_s;
    case CSSUnitType::Cm: return "cm"_s;
    case CSSUnitType::Cqb: return "cqb"_s;
    case CSSUnitType::Cqh: return "cqh"_s;
    case CSSUnitType::Cqi: return "cqi"_s;
    case CSSUnitType::Cqmax: return "cqmax"_s;
    case CSSUnitType::Cqmin: return "cqmin"_s;
    case CSSUnitType::Cqw: return "cqw"_s;
    case CSSUnitType::Deg: return "deg"_s;
    case CSSUnitType::Dpcm: return "dpcm"_s;
    case CSSUnitType::Dpi: return "dpi"_s;
    case CSSUnitType::Dppx: return "dppx"_s;
    case CSSUnitType::Dvb: return "dvb"_s;
    case CSSUnitType::Dvh: return "dvh"_s;
    case CSSUnitType::Dvi: return "dvi"_s;
    case CSSUnitType::Dvmax: return "dvmax"_s;
    case CSSUnitType::Dvmin: return "dvmin"_s;
    case CSSUnitType::Dvw: return "dvw"_s;
    case CSSUnitType::Em: return "em"_s;
    case CSSUnitType::Ex: return "ex"_s;
    case CSSUnitType::Fr: return "fr"_s;
    case CSSUnitType::Grad: return "grad"_s;
    case CSSUnitType::Hz: return "hz"_s;
    case CSSUnitType::Ic: return "ic"_s;
    case CSSUnitType::In: return "in"_s;
    case CSSUnitType::Khz: return "khz"_s;
    case CSSUnitType::Lh: return "lh"_s;
    case CSSUnitType::Lvb: return "lvb"_s;
    case CSSUnitType::Lvh: return "lvh"_s;
    case CSSUnitType::Lvi: return "lvi"_s;
    case CSSUnitType::Lvmax: return "lvmax"_s;
    case CSSUnitType::Lvmin: return "lvmin"_s;
    case CSSUnitType::Lvw: return "lvw"_s;
    case CSSUnitType::Mm: return "mm"_s;
    case CSSUnitType::Ms: return "ms"_s;
    case CSSUnitType::Pc: return "pc"_s;
    case CSSUnitType::Percentage: return "%"_s;
    case CSSUnitType::Pt: return "pt"_s;
    case CSSUnitType::Px: return "px"_s;
    case CSSUnitType::Q: return "q"_s;
    case CSSUnitType::Rad: return "rad"_s;
    case CSSUnitType::Rcap: return "rcap"_s;
    case CSSUnitType::Rch: return "rch"_s;
    case CSSUnitType::Rem: return "rem"_s;
    case CSSUnitType::Rex: return "rex"_s;
    case CSSUnitType::Ric: return "ric"_s;
    case CSSUnitType::Rlh: return "rlh"_s;
    case CSSUnitType::S: return "s"_s;
    case CSSUnitType::Svb: return "svb"_s;
    case CSSUnitType::Svh: return "svh"_s;
    case CSSUnitType::Svi: return "svi"_s;
    case CSSUnitType::Svmax: return "svmax"_s;
    case CSSUnitType::Svmin: return "svmin"_s;
    case CSSUnitType::Svw: return "svw"_s;
    case CSSUnitType::Turn: return "turn"_s;
    case CSSUnitType::Vb: return "vb"_s;
    case CSSUnitType::Vh: return "vh"_s;
    case CSSUnitType::Vi: return "vi"_s;
    case CSSUnitType::Vmax: return "vmax"_s;
    case CSSUnitType::Vmin: return "vmin"_s;
    case CSSUnitType::Vw: return "vw"_s;
    case CSSUnitType::X: return "x"_s;

    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::Integer:
    case CSSUnitType::Number:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Unknown:
        return ""_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

} // namespace WebCore
