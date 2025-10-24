/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSCalcTree.h"
#include "CSSUnits.h"

namespace WebCore {
namespace CSSCalc {

// `NumericIdentity` is used to assign a densely packed unique identifier for each possible numeric type allowed in a `calc()` expression. This allows the construction of fixed size (specifically sized to `numberOfNumericIdentityTypes`) lookup tables needed in expression simplification.

enum class NumericIdentity : uint8_t {
    Number,
    Percentage,

    // Canonical dimension units
    PX,
    DEG,
    S,
    HZ,
    DPPX,
    FR,

    // Non-canonical dimension units
    CM,
    MM,
    Q,
    INCH, // NOTE: using "IN" here breaks under MSVC.
    PT,
    PC,
    EM,
    EX,
    LH,
    CAP,
    CH,
    IC,
    RCAP,
    RCH,
    REM,
    REX,
    RIC,
    RLH,
    VW,
    VH,
    VMIN,
    VMAX,
    VB,
    VI,
    SVW,
    SVH,
    SVMIN,
    SVMAX,
    SVB,
    SVI,
    LVW,
    LVH,
    LVMIN,
    LVMAX,
    LVB,
    LVI,
    DVW,
    DVH,
    DVMIN,
    DVMAX,
    DVB,
    DVI,
    CQW,
    CQH,
    CQI,
    CQB,
    CQMIN,
    CQMAX,
    RAD,
    GRAD,
    TURN,
    MS,
    KHZ,
    X,
    DPI,
    DPCM
};

constexpr uint8_t numberOfNumericIdentityTypes = static_cast<uint8_t>(NumericIdentity::DPCM) + 1;

constexpr NumericIdentity toNumericIdentity(const Number&)
{
    return NumericIdentity::Number;
}

constexpr NumericIdentity toNumericIdentity(const Percentage&)
{
    return NumericIdentity::Percentage;
}

constexpr NumericIdentity toNumericIdentity(const CanonicalDimension& dimension)
{
    switch (dimension.dimension) {
    case CanonicalDimension::Dimension::Length:      return NumericIdentity::PX;
    case CanonicalDimension::Dimension::Angle:       return NumericIdentity::DEG;
    case CanonicalDimension::Dimension::Time:        return NumericIdentity::S;
    case CanonicalDimension::Dimension::Frequency:   return NumericIdentity::HZ;
    case CanonicalDimension::Dimension::Resolution:  return NumericIdentity::DPPX;
    case CanonicalDimension::Dimension::Flex:        return NumericIdentity::FR;
    }

    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return NumericIdentity::Number;
}

constexpr NumericIdentity toNumericIdentity(const NonCanonicalDimension& dimension)
{
    switch (dimension.unit) {
    case CSSUnitType::Centimeter:       return NumericIdentity::CM;
    case CSSUnitType::Millimeter:       return NumericIdentity::MM;
    case CSSUnitType::QuarterMillimeter:        return NumericIdentity::Q;
    case CSSUnitType::Inch:       return NumericIdentity::INCH;
    case CSSUnitType::Point:       return NumericIdentity::PT;
    case CSSUnitType::Pica:       return NumericIdentity::PC;
    case CSSUnitType::Em:       return NumericIdentity::EM;
    case CSSUnitType::Ex:       return NumericIdentity::EX;
    case CSSUnitType::Lh:       return NumericIdentity::LH;
    case CSSUnitType::Cap:      return NumericIdentity::CAP;
    case CSSUnitType::Ch:       return NumericIdentity::CH;
    case CSSUnitType::Ic:       return NumericIdentity::IC;
    case CSSUnitType::Rcap:     return NumericIdentity::RCAP;
    case CSSUnitType::Rch:      return NumericIdentity::RCH;
    case CSSUnitType::Rem:      return NumericIdentity::REM;
    case CSSUnitType::Rex:      return NumericIdentity::REX;
    case CSSUnitType::Ric:      return NumericIdentity::RIC;
    case CSSUnitType::Rlh:      return NumericIdentity::RLH;
    case CSSUnitType::ViewportPercentageWidth:       return NumericIdentity::VW;
    case CSSUnitType::ViewportPercentageHeight:       return NumericIdentity::VH;
    case CSSUnitType::ViewportPercentageMin:     return NumericIdentity::VMIN;
    case CSSUnitType::ViewportPercentageMax:     return NumericIdentity::VMAX;
    case CSSUnitType::ViewportPercentageBlockSize:       return NumericIdentity::VB;
    case CSSUnitType::ViewportPercentageInlineSize:       return NumericIdentity::VI;
    case CSSUnitType::SmallViewportWidth:      return NumericIdentity::SVW;
    case CSSUnitType::SmallViewportHeight:      return NumericIdentity::SVH;
    case CSSUnitType::SmallViewportMin:    return NumericIdentity::SVMIN;
    case CSSUnitType::SmallViewportMax:    return NumericIdentity::SVMAX;
    case CSSUnitType::SmallViewportBlockSize:      return NumericIdentity::SVB;
    case CSSUnitType::SmallViewportInlineSize:      return NumericIdentity::SVI;
    case CSSUnitType::LargeViewportWidth:      return NumericIdentity::LVW;
    case CSSUnitType::LargeViewportHeight:      return NumericIdentity::LVH;
    case CSSUnitType::LargeViewportMin:    return NumericIdentity::LVMIN;
    case CSSUnitType::LargeViewportMax:    return NumericIdentity::LVMAX;
    case CSSUnitType::LargeViewportBlockSize:      return NumericIdentity::LVB;
    case CSSUnitType::LargeViewportInlineSize:      return NumericIdentity::LVI;
    case CSSUnitType::DynamicViewportWidth:      return NumericIdentity::DVW;
    case CSSUnitType::DynamicViewportHeight:      return NumericIdentity::DVH;
    case CSSUnitType::DynamicViewportMin:    return NumericIdentity::DVMIN;
    case CSSUnitType::DynamicViewportMax:    return NumericIdentity::DVMAX;
    case CSSUnitType::DynamicViewportBlockSize:      return NumericIdentity::DVB;
    case CSSUnitType::DynamicViewportInlineSize:      return NumericIdentity::DVI;
    case CSSUnitType::ContainerQueryWidth:      return NumericIdentity::CQW;
    case CSSUnitType::ContainerQueryHeight:      return NumericIdentity::CQH;
    case CSSUnitType::ContainerQueryInlineSize:      return NumericIdentity::CQI;
    case CSSUnitType::ContainerQueryBlockSize:      return NumericIdentity::CQB;
    case CSSUnitType::ContainerQueryMin:    return NumericIdentity::CQMIN;
    case CSSUnitType::ContainerQueryMax:    return NumericIdentity::CQMAX;
    case CSSUnitType::Radian:      return NumericIdentity::RAD;
    case CSSUnitType::Gradian:     return NumericIdentity::GRAD;
    case CSSUnitType::Turn:     return NumericIdentity::TURN;
    case CSSUnitType::Millisecond:       return NumericIdentity::MS;
    case CSSUnitType::Kilohertz:      return NumericIdentity::KHZ;
    case CSSUnitType::X:        return NumericIdentity::X;
    case CSSUnitType::DotsPerInch:      return NumericIdentity::DPI;
    case CSSUnitType::DotsPerCentimeter:     return NumericIdentity::DPCM;

    // Invalid types for NonCanonicalDimension.
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Pixel:
    case CSSUnitType::Degree:
    case CSSUnitType::Second:
    case CSSUnitType::Hertz:
    case CSSUnitType::DotsPerPixel:
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

    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return NumericIdentity::Number;
}

constexpr bool isLength(NumericIdentity id)
{
    switch (id) {
    case NumericIdentity::PX:
    case NumericIdentity::CM:
    case NumericIdentity::MM:
    case NumericIdentity::Q:
    case NumericIdentity::INCH:
    case NumericIdentity::PT:
    case NumericIdentity::PC:
    case NumericIdentity::EM:
    case NumericIdentity::EX:
    case NumericIdentity::LH:
    case NumericIdentity::CAP:
    case NumericIdentity::CH:
    case NumericIdentity::IC:
    case NumericIdentity::RCAP:
    case NumericIdentity::RCH:
    case NumericIdentity::REM:
    case NumericIdentity::REX:
    case NumericIdentity::RIC:
    case NumericIdentity::RLH:
    case NumericIdentity::VW:
    case NumericIdentity::VH:
    case NumericIdentity::VMIN:
    case NumericIdentity::VMAX:
    case NumericIdentity::VB:
    case NumericIdentity::VI:
    case NumericIdentity::SVW:
    case NumericIdentity::SVH:
    case NumericIdentity::SVMIN:
    case NumericIdentity::SVMAX:
    case NumericIdentity::SVB:
    case NumericIdentity::SVI:
    case NumericIdentity::LVW:
    case NumericIdentity::LVH:
    case NumericIdentity::LVMIN:
    case NumericIdentity::LVMAX:
    case NumericIdentity::LVB:
    case NumericIdentity::LVI:
    case NumericIdentity::DVW:
    case NumericIdentity::DVH:
    case NumericIdentity::DVMIN:
    case NumericIdentity::DVMAX:
    case NumericIdentity::DVB:
    case NumericIdentity::DVI:
    case NumericIdentity::CQW:
    case NumericIdentity::CQH:
    case NumericIdentity::CQI:
    case NumericIdentity::CQB:
    case NumericIdentity::CQMIN:
    case NumericIdentity::CQMAX:
        return true;

    case NumericIdentity::Number:
    case NumericIdentity::Percentage:
    case NumericIdentity::DEG:
    case NumericIdentity::S:
    case NumericIdentity::HZ:
    case NumericIdentity::DPPX:
    case NumericIdentity::FR:
    case NumericIdentity::RAD:
    case NumericIdentity::GRAD:
    case NumericIdentity::TURN:
    case NumericIdentity::MS:
    case NumericIdentity::KHZ:
    case NumericIdentity::X:
    case NumericIdentity::DPI:
    case NumericIdentity::DPCM:
        break;
    }

    return false;
}

} // namespace CSSCalc
} // namespace WebCore
