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
    case CSSUnitType::Lh:
    case CSSUnitType::Rcap:
    case CSSUnitType::Rch:
    case CSSUnitType::Rem:
    case CSSUnitType::Rex:
    case CSSUnitType::Ric:
    case CSSUnitType::Rlh:
        return CSSUnitCategory::FontRelativeLength;
    // https://drafts.csswg.org/css-values-4/#viewport-relative-lengths
    case CSSUnitType::ViewportPercentageWidth:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::ViewportPercentageHeight:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::ViewportPercentageInlineSize:
    case CSSUnitType::SmallViewportInlineSize:
    case CSSUnitType::LargeViewportInlineSize:
    case CSSUnitType::DynamicViewportInlineSize:
    case CSSUnitType::ViewportPercentageBlockSize:
    case CSSUnitType::SmallViewportBlockSize:
    case CSSUnitType::LargeViewportBlockSize:
    case CSSUnitType::DynamicViewportBlockSize:
    case CSSUnitType::ViewportPercentageMin:
    case CSSUnitType::LargeViewportMin:
    case CSSUnitType::SmallViewportMin:
    case CSSUnitType::DynamicViewportMin:
    case CSSUnitType::ViewportPercentageMax:
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
    case CSSUnitType::Fr:
        return CSSUnitCategory::Flex;
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInlineSize:
    case CSSUnitType::ContainerQueryBlockSize:
    case CSSUnitType::ContainerQueryMin:
    case CSSUnitType::ContainerQueryMax:
    case CSSUnitType::Attr:
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::Dimension:
    case CSSUnitType::FontFamily:
    case CSSUnitType::Ident:
    case CSSUnitType::PropertyId:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::String:
    case CSSUnitType::Unknown:
    case CSSUnitType::ValueId:
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
    case CSSUnitType::Dimension: ts << "dimension"_s; break;
    case CSSUnitType::String: ts << "string"_s; break;
    case CSSUnitType::Ident: ts << "ident"_s; break;
    case CSSUnitType::CustomIdent: ts << "custom-ident"_s; break;
    case CSSUnitType::Attr: ts << "attr"_s; break;
    case CSSUnitType::ViewportPercentageWidth: ts << "vw"_s; break;
    case CSSUnitType::ViewportPercentageHeight: ts << "vh"_s; break;
    case CSSUnitType::ViewportPercentageMin: ts << "vmin"_s; break;
    case CSSUnitType::ViewportPercentageMax: ts << "vmax"_s; break;
    case CSSUnitType::ViewportPercentageBlockSize: ts << "vb"_s; break;
    case CSSUnitType::ViewportPercentageInlineSize: ts << "vi"_s; break;
    case CSSUnitType::SmallViewportWidth: ts << "svw"_s; break;
    case CSSUnitType::SmallViewportHeight: ts << "svh"_s; break;
    case CSSUnitType::SmallViewportMin: ts << "svmin"_s; break;
    case CSSUnitType::SmallViewportMax: ts << "svmax"_s; break;
    case CSSUnitType::SmallViewportBlockSize: ts << "svb"_s; break;
    case CSSUnitType::SmallViewportInlineSize: ts << "svi"_s; break;
    case CSSUnitType::LargeViewportWidth: ts << "lvw"_s; break;
    case CSSUnitType::LargeViewportHeight: ts << "lvh"_s; break;
    case CSSUnitType::LargeViewportMin: ts << "lvmin"_s; break;
    case CSSUnitType::LargeViewportMax: ts << "lvmax"_s; break;
    case CSSUnitType::LargeViewportBlockSize: ts << "lvb"_s; break;
    case CSSUnitType::LargeViewportInlineSize: ts << "lvi"_s; break;
    case CSSUnitType::DynamicViewportWidth: ts << "dvw"_s; break;
    case CSSUnitType::DynamicViewportHeight: ts << "dvh"_s; break;
    case CSSUnitType::DynamicViewportMin: ts << "dvmin"_s; break;
    case CSSUnitType::DynamicViewportMax: ts << "dvmax"_s; break;
    case CSSUnitType::DynamicViewportBlockSize: ts << "dvb"_s; break;
    case CSSUnitType::DynamicViewportInlineSize: ts << "dvi"_s; break;
    case CSSUnitType::DotsPerPixel: ts << "dppx"_s; break;
    case CSSUnitType::X: ts << 'x'; break;
    case CSSUnitType::DotsPerInch: ts << "dpi"_s; break;
    case CSSUnitType::DotsPerCentimeter: ts << "dpcm"_s; break;
    case CSSUnitType::Fr: ts << "fr"_s; break;
    case CSSUnitType::QuarterMillimeter: ts << 'q'; break;
    case CSSUnitType::Lh: ts << "lh"_s; break;
    case CSSUnitType::Rlh: ts << "rlh"_s; break;
    case CSSUnitType::ContainerQueryWidth: ts << "cqw"_s; break;
    case CSSUnitType::ContainerQueryHeight: ts << "cqh"_s; break;
    case CSSUnitType::ContainerQueryInlineSize: ts << "cqi"_s; break;
    case CSSUnitType::ContainerQueryBlockSize: ts << "cqb"_s; break;
    case CSSUnitType::ContainerQueryMax: ts << "cqmax"_s; break;
    case CSSUnitType::ContainerQueryMin: ts << "cqmin"_s; break;
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
    case CSSUnitType::FontFamily: ts << "font_family"_s; break;
    case CSSUnitType::PropertyId: ts << "property_id"_s; break;
    case CSSUnitType::ValueId: ts << "value_id"_s; break;
    case CSSUnitType::QuirkyEm: ts << "quirky_em"_s; break;
    }
    return ts;
}

double conversionToCanonicalUnitsScaleFactor(CSSUnitType unit)
{
    switch (unit) {
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
        return 1.0;
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
    case CSSUnitType::ViewportPercentageWidth:
    case CSSUnitType::ViewportPercentageHeight:
    case CSSUnitType::ViewportPercentageMin:
    case CSSUnitType::ViewportPercentageMax:
    case CSSUnitType::ViewportPercentageBlockSize:
    case CSSUnitType::ViewportPercentageInlineSize:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMin:
    case CSSUnitType::SmallViewportMax:
    case CSSUnitType::SmallViewportBlockSize:
    case CSSUnitType::SmallViewportInlineSize:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMin:
    case CSSUnitType::LargeViewportMax:
    case CSSUnitType::LargeViewportBlockSize:
    case CSSUnitType::LargeViewportInlineSize:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMin:
    case CSSUnitType::DynamicViewportMax:
    case CSSUnitType::DynamicViewportBlockSize:
    case CSSUnitType::DynamicViewportInlineSize:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInlineSize:
    case CSSUnitType::ContainerQueryBlockSize:
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
    case CSSUnitType::Fr:
    case CSSUnitType::Attr:
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::Dimension:
    case CSSUnitType::FontFamily:
    case CSSUnitType::Ident:
    case CSSUnitType::PropertyId:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::String:
    case CSSUnitType::Unknown:
    case CSSUnitType::ValueId:
    case CSSUnitType::CustomIdent:
        break;
    }

    return false;
}

} // namespace WebCore
