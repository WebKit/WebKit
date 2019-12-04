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
    CSS_UNKNOWN = 0,
    CSS_NUMBER = 1,
    CSS_PERCENTAGE = 2,
    CSS_EMS = 3,
    CSS_EXS = 4,
    CSS_PX = 5,
    CSS_CM = 6,
    CSS_MM = 7,
    CSS_IN = 8,
    CSS_PT = 9,
    CSS_PC = 10,
    CSS_DEG = 11,
    CSS_RAD = 12,
    CSS_GRAD = 13,
    CSS_MS = 14,
    CSS_S = 15,
    CSS_HZ = 16,
    CSS_KHZ = 17,
    CSS_DIMENSION = 18,
    CSS_STRING = 19,
    CSS_URI = 20,
    CSS_IDENT = 21,
    CSS_ATTR = 22,
    CSS_COUNTER = 23,
    CSS_RECT = 24,
    CSS_RGBCOLOR = 25,
    CSS_VW = 26,
    CSS_VH = 27,
    CSS_VMIN = 28,
    CSS_VMAX = 29,
    CSS_DPPX = 30,
    CSS_DPI = 31,
    CSS_DPCM = 32,
    CSS_FR = 33,
    CSS_Q = 34,
    CSS_PAIR = 100, // We envision this being exposed as a means of getting computed style values for pairs (border-spacing/radius, background-position, etc.)
    CSS_UNICODE_RANGE = 102,
    CSS_TURN = 107,
    CSS_REMS = 108,
    CSS_CHS = 109,

    // This is used internally for counter names (as opposed to counter values)
    CSS_COUNTER_NAME = 110,

    CSS_SHAPE = 111,

    // Used by border images.
    CSS_QUAD = 112,

    CSS_CALC = 113,
    CSS_CALC_PERCENTAGE_WITH_NUMBER = 114,
    CSS_CALC_PERCENTAGE_WITH_LENGTH = 115,

    CSS_FONT_FAMILY = 116,

    CSS_PROPERTY_ID = 117,
    CSS_VALUE_ID = 118,
    
    // This value is used to handle quirky margins in reflow roots (body, td, and th) like WinIE.
    // The basic idea is that a stylesheet can use the value __qem (for quirky em) instead of em.
    // When the quirky value is used, if you're in quirks mode, the margin will collapse away
    // inside a table cell. This quirk is specified in the HTML spec but our impl is different.
    CSS_QUIRKY_EMS = 120
    
    // Note that CSSValue allocates 7 bits for m_primitiveUnitType, so there can be no value here > 127.
};

enum class CSSUnitCategory : uint8_t {
    Number,
    Percent,
    Length,
    Angle,
    Time,
    Frequency,
    Resolution,
    Other
};

CSSUnitCategory unitCategory(CSSUnitType);
CSSUnitType canonicalUnitTypeForCategory(CSSUnitCategory);
CSSUnitType canonicalUnitType(CSSUnitType);

WTF::TextStream& operator<<(WTF::TextStream&, CSSUnitCategory);
WTF::TextStream& operator<<(WTF::TextStream&, CSSUnitType);

} // namespace WebCore

