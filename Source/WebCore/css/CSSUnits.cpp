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
    case CSSUnitType::Quarter:
        return CSSUnitCategory::AbsoluteLength;
    // https://drafts.csswg.org/css-values-4/#font-relative-lengths
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::CapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::IdeographicCharacter:
    case CSSUnitType::LineHeight:
    case CSSUnitType::RootCapHeight:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIdeographicCharacter:
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
    case CSSUnitType::ViewportMinimum:
    case CSSUnitType::LargeViewportMinimum:
    case CSSUnitType::SmallViewportMinimum:
    case CSSUnitType::DynamicViewportMinimum:
    case CSSUnitType::ViewportMaximum:
    case CSSUnitType::SmallViewportMaximum:
    case CSSUnitType::LargeViewportMaximum:
    case CSSUnitType::DynamicViewportMaximum:
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
    case CSSUnitType::MultiplicationFactor:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerCentimeter:
        return CSSUnitCategory::Resolution;
    case CSSUnitType::Fraction:
        return CSSUnitCategory::Flex;
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMinimum:
    case CSSUnitType::ContainerQueryMaximum:
    case CSSUnitType::Anchor:
    case CSSUnitType::Attribute:
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::CalcPercentageWithNumber:
    case CSSUnitType::Dimension:
    case CSSUnitType::FontFamily:
    case CSSUnitType::Ident:
    case CSSUnitType::PropertyID:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::RgbColor:
    case CSSUnitType::String:
    case CSSUnitType::Unknown:
    case CSSUnitType::UnresolvedColor:
    case CSSUnitType::Uri:
    case CSSUnitType::ValueID:
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
    case CSSUnitType::Unknown: ts << "unknown"; break;
    case CSSUnitType::Number: ts << "number"; break;
    case CSSUnitType::Integer: ts << "integer"; break;
    case CSSUnitType::Percentage: ts << "percentage"; break;
    case CSSUnitType::Em: ts << "em"; break;
    case CSSUnitType::Ex: ts << "ex"; break;
    case CSSUnitType::Pixel: ts << "px"; break;
    case CSSUnitType::Centimeter: ts << "cm"; break;
    case CSSUnitType::Millimeter: ts << "mm"; break;
    case CSSUnitType::Inch: ts << "in"; break;
    case CSSUnitType::Point: ts << "pt"; break;
    case CSSUnitType::Pica: ts << "pc"; break;
    case CSSUnitType::Degree: ts << "deg"; break;
    case CSSUnitType::Radian: ts << "rad"; break;
    case CSSUnitType::Gradian: ts << "grad"; break;
    case CSSUnitType::Millisecond: ts << "ms"; break;
    case CSSUnitType::Second: ts << "s"; break;
    case CSSUnitType::Hertz: ts << "hz"; break;
    case CSSUnitType::Kilohertz: ts << "khz"; break;
    case CSSUnitType::Dimension: ts << "dimension"; break;
    case CSSUnitType::String: ts << "string"; break;
    case CSSUnitType::Uri: ts << "uri"; break;
    case CSSUnitType::Ident: ts << "ident"; break;
    case CSSUnitType::CustomIdent: ts << "custom-ident"; break;
    case CSSUnitType::Attribute: ts << "attr"; break;
    case CSSUnitType::RgbColor: ts << "rgbcolor"; break;
    case CSSUnitType::ViewportWidth: ts << "vw"; break;
    case CSSUnitType::ViewportHeight: ts << "vh"; break;
    case CSSUnitType::ViewportMinimum: ts << "vmin"; break;
    case CSSUnitType::ViewportMaximum: ts << "vmax"; break;
    case CSSUnitType::ViewportBlock: ts << "vb"; break;
    case CSSUnitType::ViewportInline: ts << "vi"; break;
    case CSSUnitType::SmallViewportWidth: ts << "svw"; break;
    case CSSUnitType::SmallViewportHeight: ts << "svh"; break;
    case CSSUnitType::SmallViewportMinimum: ts << "svmin"; break;
    case CSSUnitType::SmallViewportMaximum: ts << "svmax"; break;
    case CSSUnitType::SmallViewportBlock: ts << "svb"; break;
    case CSSUnitType::SmallViewportInline: ts << "svi"; break;
    case CSSUnitType::LargeViewportWidth: ts << "lvw"; break;
    case CSSUnitType::LargeViewportHeight: ts << "lvh"; break;
    case CSSUnitType::LargeViewportMinimum: ts << "lvmin"; break;
    case CSSUnitType::LargeViewportMaximum: ts << "lvmax"; break;
    case CSSUnitType::LargeViewportBlock: ts << "lvb"; break;
    case CSSUnitType::LargeViewportInline: ts << "lvi"; break;
    case CSSUnitType::DynamicViewportWidth: ts << "dvw"; break;
    case CSSUnitType::DynamicViewportHeight: ts << "dvh"; break;
    case CSSUnitType::DynamicViewportMinimum: ts << "dvmin"; break;
    case CSSUnitType::DynamicViewportMaximum: ts << "dvmax"; break;
    case CSSUnitType::DynamicViewportBlock: ts << "dvb"; break;
    case CSSUnitType::DynamicViewportInline: ts << "dvi"; break;
    case CSSUnitType::DotsPerPixel: ts << "dppx"; break;
    case CSSUnitType::MultiplicationFactor: ts << "x"; break;
    case CSSUnitType::DotsPerInch: ts << "dpi"; break;
    case CSSUnitType::DotsPerCentimeter: ts << "dpcm"; break;
    case CSSUnitType::Fraction: ts << "fr"; break;
    case CSSUnitType::Quarter: ts << "q"; break;
    case CSSUnitType::LineHeight: ts << "lh"; break;
    case CSSUnitType::RootLineHeight: ts << "rlh"; break;
    case CSSUnitType::ContainerQueryWidth: ts << "cqw"; break;
    case CSSUnitType::ContainerQueryHeight: ts << "cqh"; break;
    case CSSUnitType::ContainerQueryInline: ts << "cqi"; break;
    case CSSUnitType::ContainerQueryBlock: ts << "cqb"; break;
    case CSSUnitType::ContainerQueryMaximum: ts << "cqmax"; break;
    case CSSUnitType::ContainerQueryMinimum: ts << "cqmin"; break;
    case CSSUnitType::Turn: ts << "turn"; break;
    case CSSUnitType::RootCapHeight: ts << "rcap"; break;
    case CSSUnitType::RootCharacterWidth: ts << "rch"; break;
    case CSSUnitType::RootEm: ts << "rem"; break;
    case CSSUnitType::RootEx: ts << "rex"; break;
    case CSSUnitType::RootIdeographicCharacter: ts << "ric"; break;
    case CSSUnitType::CapHeight: ts << "cap"; break;
    case CSSUnitType::CharacterWidth: ts << "ch"; break;
    case CSSUnitType::IdeographicCharacter: ts << "ic"; break;
    case CSSUnitType::Calc: ts << "calc"; break;
    case CSSUnitType::CalcPercentageWithNumber: ts << "calc_percentage_with_number"; break;
    case CSSUnitType::CalcPercentageWithLength: ts << "calc_percentage_with_length"; break;
    case CSSUnitType::UnresolvedColor: ts << "unresolved_color"; break;
    case CSSUnitType::FontFamily: ts << "font_family"; break;
    case CSSUnitType::PropertyID: ts << "property_id"; break;
    case CSSUnitType::ValueID: ts << "value_id"; break;
    case CSSUnitType::QuirkyEm: ts << "quirky_em"; break;
    case CSSUnitType::Anchor: ts << "anchor"; break;
    }
    return ts;
}

} // namespace WebCore
