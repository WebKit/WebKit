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
    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Inch:
    case CSSUnitType::Point:
    case CSSUnitType::Pica:
    case CSSUnitType::QuarterMillimeter:
        return CSSUnitCategory::AbsoluteLength;
    // https://drafts.csswg.org/css-values-4/#font-relative-lengths
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::LineHeight:
    case CSSUnitType::RootCap:
    case CSSUnitType::RootCh:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIc:
    case CSSUnitType::RootLineHeight:
        return CSSUnitCategory::FontRelativeLength;
    // https://drafts.csswg.org/css-values-4/#viewport-relative-lengths
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::ViewportMin:
    case CSSUnitType::LargeViewportMin:
    case CSSUnitType::SmallViewportMin:
    case CSSUnitType::DynamicViewportMin:
    case CSSUnitType::ViewportMax:
    case CSSUnitType::SmallViewportMax:
    case CSSUnitType::LargeViewportMax:
    case CSSUnitType::DynamicViewportMax:
        return CSSUnitCategory::ViewportPercentageLength;
    // https://drafts.csswg.org/css-values-4/#time
    case CSSUnitType::Millisecond:
    case CSSUnitType::Second:
        return CSSUnitCategory::Time;
    // https://drafts.csswg.org/css-values-4/#angles
    case CSSUnitType::Degree:
    case CSSUnitType::Radian:
    case CSSUnitType::Gradian:
    case CSSUnitType::Turn:
        return CSSUnitCategory::Angle;
    // https://drafts.csswg.org/css-values-4/#frequency
    case CSSUnitType::Hertz:
    case CSSUnitType::Kilohertz:
        return CSSUnitCategory::Frequency;
    // https://drafts.csswg.org/css-values-4/#resolution
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::X:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerCentimeter:
        return CSSUnitCategory::Resolution;
    case CSSUnitType::Fraction:
        return CSSUnitCategory::Flex;
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMin:
    case CSSUnitType::ContainerQueryMax:
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
        return CSSUnitType::Pixel;
    case CSSUnitCategory::Percent:
        return CSSUnitType::Percentage;
    case CSSUnitCategory::Time:
        return CSSUnitType::Second;
    case CSSUnitCategory::Angle:
        return CSSUnitType::Degree;
    case CSSUnitCategory::Frequency:
        return CSSUnitType::Hertz;
    case CSSUnitCategory::Resolution:
        return CSSUnitType::DotsPerPixel;
    case CSSUnitCategory::Flex:
        return CSSUnitType::Fraction;
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
    case CSSUnitType::Pixel: ts << "px"_s; break;
    case CSSUnitType::Centimeter: ts << "cm"_s; break;
    case CSSUnitType::Millimeter: ts << "mm"_s; break;
    case CSSUnitType::Inch: ts << "in"_s; break;
    case CSSUnitType::Point: ts << "pt"_s; break;
    case CSSUnitType::Pica: ts << "pc"_s; break;
    case CSSUnitType::Degree: ts << "deg"_s; break;
    case CSSUnitType::Radian: ts << "rad"_s; break;
    case CSSUnitType::Gradian: ts << "grad"_s; break;
    case CSSUnitType::Millisecond: ts << "ms"_s; break;
    case CSSUnitType::Second: ts << 's'; break;
    case CSSUnitType::Hertz: ts << "hz"_s; break;
    case CSSUnitType::Kilohertz: ts << "khz"_s; break;
    case CSSUnitType::ViewportWidth: ts << "vw"_s; break;
    case CSSUnitType::ViewportHeight: ts << "vh"_s; break;
    case CSSUnitType::ViewportMin: ts << "vmin"_s; break;
    case CSSUnitType::ViewportMax: ts << "vmax"_s; break;
    case CSSUnitType::ViewportBlock: ts << "vb"_s; break;
    case CSSUnitType::ViewportInline: ts << "vi"_s; break;
    case CSSUnitType::SmallViewportWidth: ts << "svw"_s; break;
    case CSSUnitType::SmallViewportHeight: ts << "svh"_s; break;
    case CSSUnitType::SmallViewportMin: ts << "svmin"_s; break;
    case CSSUnitType::SmallViewportMax: ts << "svmax"_s; break;
    case CSSUnitType::SmallViewportBlock: ts << "svb"_s; break;
    case CSSUnitType::SmallViewportInline: ts << "svi"_s; break;
    case CSSUnitType::LargeViewportWidth: ts << "lvw"_s; break;
    case CSSUnitType::LargeViewportHeight: ts << "lvh"_s; break;
    case CSSUnitType::LargeViewportMin: ts << "lvmin"_s; break;
    case CSSUnitType::LargeViewportMax: ts << "lvmax"_s; break;
    case CSSUnitType::LargeViewportBlock: ts << "lvb"_s; break;
    case CSSUnitType::LargeViewportInline: ts << "lvi"_s; break;
    case CSSUnitType::DynamicViewportWidth: ts << "dvw"_s; break;
    case CSSUnitType::DynamicViewportHeight: ts << "dvh"_s; break;
    case CSSUnitType::DynamicViewportMin: ts << "dvmin"_s; break;
    case CSSUnitType::DynamicViewportMax: ts << "dvmax"_s; break;
    case CSSUnitType::DynamicViewportBlock: ts << "dvb"_s; break;
    case CSSUnitType::DynamicViewportInline: ts << "dvi"_s; break;
    case CSSUnitType::DotsPerPixel: ts << "dppx"_s; break;
    case CSSUnitType::X: ts << 'x'; break;
    case CSSUnitType::DotsPerInch: ts << "dpi"_s; break;
    case CSSUnitType::DotsPerCentimeter: ts << "dpcm"_s; break;
    case CSSUnitType::Fraction: ts << "fr"_s; break;
    case CSSUnitType::QuarterMillimeter: ts << 'q'; break;
    case CSSUnitType::LineHeight: ts << "lh"_s; break;
    case CSSUnitType::RootLineHeight: ts << "rlh"_s; break;
    case CSSUnitType::ContainerQueryWidth: ts << "cqw"_s; break;
    case CSSUnitType::ContainerQueryHeight: ts << "cqh"_s; break;
    case CSSUnitType::ContainerQueryInline: ts << "cqi"_s; break;
    case CSSUnitType::ContainerQueryBlock: ts << "cqb"_s; break;
    case CSSUnitType::ContainerQueryMax: ts << "cqmax"_s; break;
    case CSSUnitType::ContainerQueryMin: ts << "cqmin"_s; break;
    case CSSUnitType::Turn: ts << "turn"_s; break;
    case CSSUnitType::RootCap: ts << "rcap"_s; break;
    case CSSUnitType::RootCh: ts << "rch"_s; break;
    case CSSUnitType::RootEm: ts << "rem"_s; break;
    case CSSUnitType::RootEx: ts << "rex"_s; break;
    case CSSUnitType::RootIc: ts << "ric"_s; break;
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
    case CSSUnitType::Pixel:
    case CSSUnitType::Degree:
    case CSSUnitType::Second:
    case CSSUnitType::Hertz:
    case CSSUnitType::DotsPerPixel:
        return 1.0;
    case CSSUnitType::Millisecond:
        return CSS::secondsPerMillisecond;
    case CSSUnitType::Centimeter:
        return CSS::pixelsPerCm;
    case CSSUnitType::DotsPerCentimeter:
        return CSS::dppxPerDpcm;
    case CSSUnitType::Millimeter:
        return CSS::pixelsPerMm;
    case CSSUnitType::QuarterMillimeter:
        return CSS::pixelsPerQ;
    case CSSUnitType::Inch:
        return CSS::pixelsPerInch;
    case CSSUnitType::DotsPerInch:
        return CSS::dppxPerDpi;
    case CSSUnitType::Point:
        return CSS::pixelsPerPt;
    case CSSUnitType::Pica:
        return CSS::pixelsPerPc;
    case CSSUnitType::Radian:
        return degreesPerRadianDouble;
    case CSSUnitType::Gradian:
        return degreesPerGradientDouble;
    case CSSUnitType::Turn:
        return degreesPerTurnDouble;
    case CSSUnitType::Kilohertz:
        return CSS::hertzPerKilohertz;
    default:
        return std::nullopt;
    }
}

bool conversionToCanonicalUnitRequiresConversionData(CSSUnitType unit)
{
    switch (unit) {
    case CSSUnitType::Centimeter:
    case CSSUnitType::Millimeter:
    case CSSUnitType::QuarterMillimeter:
    case CSSUnitType::Inch:
    case CSSUnitType::Point:
    case CSSUnitType::Pica:
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::LineHeight:
    case CSSUnitType::Cap:
    case CSSUnitType::Ch:
    case CSSUnitType::Ic:
    case CSSUnitType::RootCap:
    case CSSUnitType::RootCh:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIc:
    case CSSUnitType::RootLineHeight:
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportMin:
    case CSSUnitType::ViewportMax:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMin:
    case CSSUnitType::SmallViewportMax:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMin:
    case CSSUnitType::LargeViewportMax:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMin:
    case CSSUnitType::DynamicViewportMax:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMin:
    case CSSUnitType::ContainerQueryMax:
        return true;

    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Pixel:
    case CSSUnitType::Degree:
    case CSSUnitType::Radian:
    case CSSUnitType::Gradian:
    case CSSUnitType::Turn:
    case CSSUnitType::Second:
    case CSSUnitType::Millisecond:
    case CSSUnitType::Hertz:
    case CSSUnitType::Kilohertz:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::X:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::Fraction:
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
    case CSSUnitType::Centimeter: return "cm"_s;
    case CSSUnitType::ContainerQueryBlock: return "cqb"_s;
    case CSSUnitType::ContainerQueryHeight: return "cqh"_s;
    case CSSUnitType::ContainerQueryInline: return "cqi"_s;
    case CSSUnitType::ContainerQueryMax: return "cqmax"_s;
    case CSSUnitType::ContainerQueryMin: return "cqmin"_s;
    case CSSUnitType::ContainerQueryWidth: return "cqw"_s;
    case CSSUnitType::Degree: return "deg"_s;
    case CSSUnitType::DotsPerCentimeter: return "dpcm"_s;
    case CSSUnitType::DotsPerInch: return "dpi"_s;
    case CSSUnitType::DotsPerPixel: return "dppx"_s;
    case CSSUnitType::DynamicViewportBlock: return "dvb"_s;
    case CSSUnitType::DynamicViewportHeight: return "dvh"_s;
    case CSSUnitType::DynamicViewportInline: return "dvi"_s;
    case CSSUnitType::DynamicViewportMax: return "dvmax"_s;
    case CSSUnitType::DynamicViewportMin: return "dvmin"_s;
    case CSSUnitType::DynamicViewportWidth: return "dvw"_s;
    case CSSUnitType::Em: return "em"_s;
    case CSSUnitType::Ex: return "ex"_s;
    case CSSUnitType::Fraction: return "fr"_s;
    case CSSUnitType::Gradian: return "grad"_s;
    case CSSUnitType::Hertz: return "hz"_s;
    case CSSUnitType::Ic: return "ic"_s;
    case CSSUnitType::Inch: return "in"_s;
    case CSSUnitType::Kilohertz: return "khz"_s;
    case CSSUnitType::LineHeight: return "lh"_s;
    case CSSUnitType::LargeViewportBlock: return "lvb"_s;
    case CSSUnitType::LargeViewportHeight: return "lvh"_s;
    case CSSUnitType::LargeViewportInline: return "lvi"_s;
    case CSSUnitType::LargeViewportMax: return "lvmax"_s;
    case CSSUnitType::LargeViewportMin: return "lvmin"_s;
    case CSSUnitType::LargeViewportWidth: return "lvw"_s;
    case CSSUnitType::Millimeter: return "mm"_s;
    case CSSUnitType::Millisecond: return "ms"_s;
    case CSSUnitType::Pica: return "pc"_s;
    case CSSUnitType::Percentage: return "%"_s;
    case CSSUnitType::Point: return "pt"_s;
    case CSSUnitType::Pixel: return "px"_s;
    case CSSUnitType::QuarterMillimeter: return "q"_s;
    case CSSUnitType::Radian: return "rad"_s;
    case CSSUnitType::RootCap: return "rcap"_s;
    case CSSUnitType::RootCh: return "rch"_s;
    case CSSUnitType::RootEm: return "rem"_s;
    case CSSUnitType::RootEx: return "rex"_s;
    case CSSUnitType::RootIc: return "ric"_s;
    case CSSUnitType::RootLineHeight: return "rlh"_s;
    case CSSUnitType::Second: return "s"_s;
    case CSSUnitType::SmallViewportBlock: return "svb"_s;
    case CSSUnitType::SmallViewportHeight: return "svh"_s;
    case CSSUnitType::SmallViewportInline: return "svi"_s;
    case CSSUnitType::SmallViewportMax: return "svmax"_s;
    case CSSUnitType::SmallViewportMin: return "svmin"_s;
    case CSSUnitType::SmallViewportWidth: return "svw"_s;
    case CSSUnitType::Turn: return "turn"_s;
    case CSSUnitType::ViewportBlock: return "vb"_s;
    case CSSUnitType::ViewportHeight: return "vh"_s;
    case CSSUnitType::ViewportInline: return "vi"_s;
    case CSSUnitType::ViewportMax: return "vmax"_s;
    case CSSUnitType::ViewportMin: return "vmin"_s;
    case CSSUnitType::ViewportWidth: return "vw"_s;
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
