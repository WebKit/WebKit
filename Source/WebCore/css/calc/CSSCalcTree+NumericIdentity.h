/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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
    case CSSUnitType::Cm:       return NumericIdentity::CM;
    case CSSUnitType::Mm:       return NumericIdentity::MM;
    case CSSUnitType::Q:        return NumericIdentity::Q;
    case CSSUnitType::In:       return NumericIdentity::INCH;
    case CSSUnitType::Pt:       return NumericIdentity::PT;
    case CSSUnitType::Pc:       return NumericIdentity::PC;
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
    case CSSUnitType::Vw:       return NumericIdentity::VW;
    case CSSUnitType::Vh:       return NumericIdentity::VH;
    case CSSUnitType::Vmin:     return NumericIdentity::VMIN;
    case CSSUnitType::Vmax:     return NumericIdentity::VMAX;
    case CSSUnitType::Vb:       return NumericIdentity::VB;
    case CSSUnitType::Vi:       return NumericIdentity::VI;
    case CSSUnitType::Svw:      return NumericIdentity::SVW;
    case CSSUnitType::Svh:      return NumericIdentity::SVH;
    case CSSUnitType::Svmin:    return NumericIdentity::SVMIN;
    case CSSUnitType::Svmax:    return NumericIdentity::SVMAX;
    case CSSUnitType::Svb:      return NumericIdentity::SVB;
    case CSSUnitType::Svi:      return NumericIdentity::SVI;
    case CSSUnitType::Lvw:      return NumericIdentity::LVW;
    case CSSUnitType::Lvh:      return NumericIdentity::LVH;
    case CSSUnitType::Lvmin:    return NumericIdentity::LVMIN;
    case CSSUnitType::Lvmax:    return NumericIdentity::LVMAX;
    case CSSUnitType::Lvb:      return NumericIdentity::LVB;
    case CSSUnitType::Lvi:      return NumericIdentity::LVI;
    case CSSUnitType::Dvw:      return NumericIdentity::DVW;
    case CSSUnitType::Dvh:      return NumericIdentity::DVH;
    case CSSUnitType::Dvmin:    return NumericIdentity::DVMIN;
    case CSSUnitType::Dvmax:    return NumericIdentity::DVMAX;
    case CSSUnitType::Dvb:      return NumericIdentity::DVB;
    case CSSUnitType::Dvi:      return NumericIdentity::DVI;
    case CSSUnitType::Cqw:      return NumericIdentity::CQW;
    case CSSUnitType::Cqh:      return NumericIdentity::CQH;
    case CSSUnitType::Cqi:      return NumericIdentity::CQI;
    case CSSUnitType::Cqb:      return NumericIdentity::CQB;
    case CSSUnitType::Cqmin:    return NumericIdentity::CQMIN;
    case CSSUnitType::Cqmax:    return NumericIdentity::CQMAX;
    case CSSUnitType::Rad:      return NumericIdentity::RAD;
    case CSSUnitType::Grad:     return NumericIdentity::GRAD;
    case CSSUnitType::Turn:     return NumericIdentity::TURN;
    case CSSUnitType::Ms:       return NumericIdentity::MS;
    case CSSUnitType::Khz:      return NumericIdentity::KHZ;
    case CSSUnitType::X:        return NumericIdentity::X;
    case CSSUnitType::Dpi:      return NumericIdentity::DPI;
    case CSSUnitType::Dpcm:     return NumericIdentity::DPCM;

    // Invalid types for NonCanonicalDimension.
    case CSSUnitType::Number:
    case CSSUnitType::Integer:
    case CSSUnitType::Percentage:
    case CSSUnitType::Px:
    case CSSUnitType::Deg:
    case CSSUnitType::S:
    case CSSUnitType::Hz:
    case CSSUnitType::Dppx:
    case CSSUnitType::Fr:
    case CSSUnitType::Calc:
    case CSSUnitType::CalcPercentageWithAngle:
    case CSSUnitType::CalcPercentageWithLength:
    case CSSUnitType::QuirkyEm:
    case CSSUnitType::Unknown:
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
