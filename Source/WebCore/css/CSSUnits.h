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

// FIXME: No need to use all capitals and a CSS prefix on all these names. Should fix that.
enum class CSSUnitType : uint8_t {
    CSS_UNKNOWN,
    CSS_NUMBER,
    CSS_INTEGER,
    CSS_PERCENTAGE,
    CSS_EMS,
    CSS_EXS,
    CSS_PX,
    CSS_CM,
    CSS_MM,
    CSS_IN,
    CSS_PT,
    CSS_PC,
    CSS_DEG,
    CSS_RAD,
    CSS_GRAD,
    CSS_MS,
    CSS_S,
    CSS_HZ,
    CSS_KHZ,
    CSS_DIMENSION,
    CSS_STRING,
    CSS_URI,
    CSS_IDENT,
    CSS_ATTR,
    CSS_COUNTER,
    CSS_RECT,
    CSS_RGBCOLOR,

    CSS_VW,
    CSS_VH,
    CSS_VMIN,
    CSS_VMAX,
    CSS_VB,
    CSS_VI,
    CSS_SVW,
    CSS_SVH,
    CSS_SVMIN,
    CSS_SVMAX,
    CSS_SVB,
    CSS_SVI,
    CSS_LVW,
    CSS_LVH,
    CSS_LVMIN,
    CSS_LVMAX,
    CSS_LVB,
    CSS_LVI,
    CSS_DVW,
    CSS_DVH,
    CSS_DVMIN,
    CSS_DVMAX,
    CSS_DVB,
    CSS_DVI,
    FirstViewportCSSUnitType = CSS_VW,
    LastViewporCSSUnitType = CSS_DVI,

    CSS_CQW,
    CSS_CQH,
    CSS_CQI,
    CSS_CQB,
    CSS_CQMIN,
    CSS_CQMAX,

    CSS_DPPX,
    CSS_X,
    CSS_DPI,
    CSS_DPCM,
    CSS_FR,
    CSS_Q,
    CSS_LHS,
    CSS_RLHS,

    CustomIdent,

    CSS_PAIR,
    CSS_UNICODE_RANGE,
    CSS_TURN,
    CSS_REMS,
    CSS_CHS,
    CSS_IC,

    CSS_COUNTER_NAME,

    CSS_SHAPE,

    CSS_QUAD,

    CSS_CALC,
    CSS_CALC_PERCENTAGE_WITH_NUMBER,
    CSS_CALC_PERCENTAGE_WITH_LENGTH,

    CSS_FONT_FAMILY,

    CSS_PROPERTY_ID,
    CSS_VALUE_ID,
    
    // This value is used to handle quirky margins in reflow roots (body, td, and th) like WinIE.
    // The basic idea is that a stylesheet can use the value __qem (for quirky em) instead of em.
    // When the quirky value is used, if you're in quirks mode, the margin will collapse away
    // inside a table cell. This quirk is specified in the HTML spec but our impl is different.
    CSS_QUIRKY_EMS

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
