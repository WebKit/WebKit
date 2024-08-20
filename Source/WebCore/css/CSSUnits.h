/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2019 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
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

namespace WTF {
class TextStream;
}

namespace WebCore {

enum class CSSUnitType : uint8_t {
    Unknown,
    Number,
    Integer,
    Percentage,
    Em,
    Ex,
    Pixel,
    Centimeter,
    Millimeter,
    Inch,
    Point,
    Pica,
    Degree,
    Radian,
    Gradian,
    Millisecond,
    Second,
    Hertz,
    Kilohertz,
    Dimension,
    String,
    Uri,
    Ident,
    Attribute,
    RgbColor,

    ViewportWidth,
    ViewportHeight,
    ViewportMinimum,
    ViewportMaximum,
    ViewportBlock,
    ViewportInline,
    SmallViewportWidth,
    SmallViewportHeight,
    SmallViewportMinimum,
    SmallViewportMaximum,
    SmallViewportBlock,
    SmallViewportInline,
    LargeViewportWidth,
    LargeViewportHeight,
    LargeViewportMinimum,
    LargeViewportMaximum,
    LargeViewportBlock,
    LargeViewportInline,
    DynamicViewportWidth,
    DynamicViewportHeight,
    DynamicViewportMinimum,
    DynamicViewportMaximum,
    DynamicViewportBlock,
    DynamicViewportInline,
    FirstViewportCSSUnitType = ViewportWidth,
    LastViewportCSSUnitType = DynamicViewportInline,

    ContainerQueryWidth,
    ContainerQueryHeight,
    ContainerQueryInline,
    ContainerQueryBlock,
    ContainerQueryMinimum,
    ContainerQueryMaximum,

    DotsPerPixel,
    MultiplicationFactor,
    DotsPerInch,
    DotsPerCentimeter,
    Fraction,
    Quarter,
    LineHeight,
    RootLineHeight,

    CustomIdent,

    Turn,
    RootEm,
    RootEx,
    CapHeight,
    RootCapHeight,
    CharacterWidth,
    RootCharacterWidth,
    IdeographicCharacter,
    RootIdeographicCharacter,

    Calc,
    CalcPercentageWithNumber,
    CalcPercentageWithLength,

    Anchor,

    FontFamily,

    UnresolvedColor,

    PropertyID,
    ValueID,
    
    // This value is used to handle quirky margins in reflow roots (body, td, and th) like WinIE.
    // The basic idea is that a stylesheet can use the value __qem (for quirky em) instead of em.
    // When the quirky value is used, if you're in quirks mode, the margin will collapse away
    // inside a table cell. This quirk is specified in the HTML spec but our impl is different.
    QuirkyEm

    // Note that CSSValue allocates 7 bits for m_primitiveUnitType, so there can be no value here > 127.
};

enum class CSSUnitCategory : uint8_t {
    Number,
    Percent,
    AbsoluteLength,
    FontRelativeLength,
    ViewportPercentageLength,
    Angle,
    Time,
    Frequency,
    Resolution,
    Flex,
    Other
};

CSSUnitCategory unitCategory(CSSUnitType);
CSSUnitType canonicalUnitTypeForCategory(CSSUnitCategory);
CSSUnitType canonicalUnitTypeForUnitType(CSSUnitType);

WTF::TextStream& operator<<(WTF::TextStream&, CSSUnitCategory);
WTF::TextStream& operator<<(WTF::TextStream&, CSSUnitType);

} // namespace WebCore
