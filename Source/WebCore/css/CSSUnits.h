/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2019 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
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

#pragma once

namespace WTF {
class TextStream;
}

namespace WebCore {

namespace CSS {

// We always assume 96 CSS pixels in a CSS inch. This is the cold hard truth of the Web.
// At high DPI, we may scale a CSS pixel, but the ratio of the CSS pixel to the so-called
// "absolute" CSS length units like inch and pt is always fixed and never changes.
constexpr double pixelsPerInch = 96;

constexpr double pointsPerInch = 72;
constexpr double picasPerInch = 6;
constexpr double mmPerInch = 25.4;
constexpr double cmPerInch = 2.54;
constexpr double QPerInch = 25.4 * 4.0;

constexpr double pixelsPerCm = pixelsPerInch / cmPerInch;
constexpr double pixelsPerMm = pixelsPerInch / mmPerInch;
constexpr double pixelsPerQ = pixelsPerInch / QPerInch;
constexpr double pixelsPerPt = pixelsPerInch / pointsPerInch;
constexpr double pixelsPerPc = pixelsPerInch / picasPerInch;
constexpr double dppxPerX = 1.0;
constexpr double dppxPerDpi = 1.0 / pixelsPerInch;
constexpr double dppxPerDpcm = cmPerInch / pixelsPerInch;
constexpr double secondsPerMillisecond = 1.0 / 1000.0;
constexpr double hertzPerKilohertz = 1000.0;

}

enum class CSSUnitType : uint8_t {
    Unknown,
    Number,
    Integer,
    Percentage,
    Em,
    Ex,
    Px,
    Cm,
    Mm,
    In,
    Pt,
    Pc,
    Deg,
    Rad,
    Grad,
    Ms,
    S,
    Hz,
    Khz,

    Vw,
    Vh,
    Vmin,
    Vmax,
    Vb,
    Vi,
    Svw,
    Svh,
    Svmin,
    Svmax,
    Svb,
    Svi,
    Lvw,
    Lvh,
    Lvmin,
    Lvmax,
    Lvb,
    Lvi,
    Dvw,
    Dvh,
    Dvmin,
    Dvmax,
    Dvb,
    Dvi,
    FirstViewportCSSUnitType = Vw,
    LastViewportCSSUnitType = Dvi,

    Cqw,
    Cqh,
    Cqi,
    Cqb,
    Cqmin,
    Cqmax,

    Dppx,
    X,
    Dpi,
    Dpcm,
    Fr,
    Q,
    Lh,
    Rlh,

    Turn,
    Rem,
    Rex,
    Cap,
    Rcap,
    Ch,
    Rch,
    Ic,
    Ric,

    Calc,
    CalcPercentageWithAngle,
    CalcPercentageWithLength,

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

CSSUnitCategory NODELETE unitCategory(CSSUnitType);
CSSUnitType NODELETE canonicalUnitTypeForCategory(CSSUnitCategory);
CSSUnitType NODELETE canonicalUnitTypeForUnitType(CSSUnitType);
std::optional<double> NODELETE conversionToCanonicalUnitsScaleFactor(CSSUnitType);
bool NODELETE conversionToCanonicalUnitRequiresConversionData(CSSUnitType);
ASCIILiteral unitTypeString(CSSUnitType);

WTF::TextStream& operator<<(WTF::TextStream&, CSSUnitCategory);
WTF::TextStream& operator<<(WTF::TextStream&, CSSUnitType);

} // namespace WebCore
