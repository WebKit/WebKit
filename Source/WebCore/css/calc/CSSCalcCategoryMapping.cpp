/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSCalcCategoryMapping.h"

#include "CSSUnits.h"
#include "CalculationCategory.h"

namespace WebCore {

CalculationCategory calcUnitCategory(CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
        return CalculationCategory::Number;
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Inch:
    case CSSUnitType::Point:
    case CSSUnitType::Pica:
    case CSSUnitType::Quarter:
    case CSSUnitType::LineHeight:
    case CSSUnitType::CapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::IdeographicCharacter:
    case CSSUnitType::RootCapHeight:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIdeographicCharacter:
    case CSSUnitType::RootLineHeight:
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportMinimum:
    case CSSUnitType::ViewportMaximum:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMinimum:
    case CSSUnitType::SmallViewportMaximum:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMinimum:
    case CSSUnitType::LargeViewportMaximum:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMinimum:
    case CSSUnitType::DynamicViewportMaximum:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMinimum:
    case CSSUnitType::ContainerQueryMaximum:
        return CalculationCategory::Length;
    case CSSUnitType::Percentage:
        return CalculationCategory::Percent;
    case CSSUnitType::Degree:
    case CSSUnitType::Radian:
    case CSSUnitType::Gradian:
    case CSSUnitType::Turn:
        return CalculationCategory::Angle;
    case CSSUnitType::Millisecond:
    case CSSUnitType::Second:
        return CalculationCategory::Time;
    case CSSUnitType::Hertz:
    case CSSUnitType::Kilohertz:
        return CalculationCategory::Frequency;
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::MultiplicationFactor:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerCentimeter:
        return CalculationCategory::Resolution;
    default:
        return CalculationCategory::Other;
    }
}

CalculationCategory calculationCategoryForCombination(CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
        return CalculationCategory::Number;
    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Inch:
    case CSSUnitType::Point:
    case CSSUnitType::Pica:
    case CSSUnitType::Quarter:
        return CalculationCategory::Length;
    case CSSUnitType::Percentage:
        return CalculationCategory::Percent;
    case CSSUnitType::Degree:
    case CSSUnitType::Radian:
    case CSSUnitType::Gradian:
    case CSSUnitType::Turn:
        return CalculationCategory::Angle;
    case CSSUnitType::Millisecond:
    case CSSUnitType::Second:
        return CalculationCategory::Time;
    case CSSUnitType::Hertz:
    case CSSUnitType::Kilohertz:
        return CalculationCategory::Frequency;
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::MultiplicationFactor:
        return CalculationCategory::Resolution;
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::LineHeight:
    case CSSUnitType::CapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::IdeographicCharacter:
    case CSSUnitType::RootCapHeight:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIdeographicCharacter:
    case CSSUnitType::RootLineHeight:
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportMinimum:
    case CSSUnitType::ViewportMaximum:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMinimum:
    case CSSUnitType::SmallViewportMaximum:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMinimum:
    case CSSUnitType::LargeViewportMaximum:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMinimum:
    case CSSUnitType::DynamicViewportMaximum:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMinimum:
    case CSSUnitType::ContainerQueryMaximum:
    default:
        return CalculationCategory::Other;
    }
}

CSSUnitType canonicalUnitTypeForCalculationCategory(CalculationCategory category)
{
    switch (category) {
    case CalculationCategory::Number: return CSSUnitType::Number;
    case CalculationCategory::Length: return CSSUnitType::Pixel;
    case CalculationCategory::Percent: return CSSUnitType::Percentage;
    case CalculationCategory::Angle: return CSSUnitType::Degree;
    case CalculationCategory::Time: return CSSUnitType::Second;
    case CalculationCategory::Frequency: return CSSUnitType::Hertz;
    case CalculationCategory::Resolution: return CSSUnitType::DotsPerPixel;
    case CalculationCategory::Other:
    case CalculationCategory::PercentNumber:
    case CalculationCategory::PercentLength:
        ASSERT_NOT_REACHED();
        break;
    }
    return CSSUnitType::Unknown;
}

bool hasDoubleValue(CSSUnitType type)
{
    switch (type) {
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Em:
    case CSSUnitType::Ex:
    case CSSUnitType::CapHeight:
    case CSSUnitType::CharacterWidth:
    case CSSUnitType::IdeographicCharacter:
    case CSSUnitType::Pixel:
    case CSSUnitType::Centimeter:
    case CSSUnitType::Millimeter:
    case CSSUnitType::Inch:
    case CSSUnitType::Point:
    case CSSUnitType::Pica:
    case CSSUnitType::Degree:
    case CSSUnitType::Radian:
    case CSSUnitType::Gradian:
    case CSSUnitType::Turn:
    case CSSUnitType::Millisecond:
    case CSSUnitType::Second:
    case CSSUnitType::Hertz:
    case CSSUnitType::Kilohertz:
    case CSSUnitType::Dimension:
    case CSSUnitType::ViewportWidth:
    case CSSUnitType::ViewportHeight:
    case CSSUnitType::ViewportMinimum:
    case CSSUnitType::ViewportMaximum:
    case CSSUnitType::ViewportBlock:
    case CSSUnitType::ViewportInline:
    case CSSUnitType::SmallViewportWidth:
    case CSSUnitType::SmallViewportHeight:
    case CSSUnitType::SmallViewportMinimum:
    case CSSUnitType::SmallViewportMaximum:
    case CSSUnitType::SmallViewportBlock:
    case CSSUnitType::SmallViewportInline:
    case CSSUnitType::LargeViewportWidth:
    case CSSUnitType::LargeViewportHeight:
    case CSSUnitType::LargeViewportMinimum:
    case CSSUnitType::LargeViewportMaximum:
    case CSSUnitType::LargeViewportBlock:
    case CSSUnitType::LargeViewportInline:
    case CSSUnitType::DynamicViewportWidth:
    case CSSUnitType::DynamicViewportHeight:
    case CSSUnitType::DynamicViewportMinimum:
    case CSSUnitType::DynamicViewportMaximum:
    case CSSUnitType::DynamicViewportBlock:
    case CSSUnitType::DynamicViewportInline:
    case CSSUnitType::DotsPerPixel:
    case CSSUnitType::MultiplicationFactor:
    case CSSUnitType::DotsPerInch:
    case CSSUnitType::DotsPerCentimeter:
    case CSSUnitType::Fraction:
    case CSSUnitType::Quarter:
    case CSSUnitType::LineHeight:
    case CSSUnitType::RootCapHeight:
    case CSSUnitType::RootCharacterWidth:
    case CSSUnitType::RootEm:
    case CSSUnitType::RootEx:
    case CSSUnitType::RootIdeographicCharacter:
    case CSSUnitType::RootLineHeight:
    case CSSUnitType::ContainerQueryWidth:
    case CSSUnitType::ContainerQueryHeight:
    case CSSUnitType::ContainerQueryInline:
    case CSSUnitType::ContainerQueryBlock:
    case CSSUnitType::ContainerQueryMinimum:
    case CSSUnitType::ContainerQueryMaximum:
        return true;
    case CSSUnitType::Unknown:
    case CSSUnitType::String:
    case CSSUnitType::FontFamily:
    case CSSUnitType::Uri:
    case CSSUnitType::Ident:
    case CSSUnitType::CustomIdent:
    case CSSUnitType::Attribute:
    case CSSUnitType::RgbColor:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithNumber:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::UnresolvedColor:
    case CSSUnitType::Anchor:
    case CSSUnitType::PropertyID:
    case CSSUnitType::ValueID:
        return false;
    };
    ASSERT_NOT_REACHED();
    return false;
}

}
