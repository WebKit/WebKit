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
    case CSSUnitType::CSS_CM:       return NumericIdentity::CM;
    case CSSUnitType::CSS_MM:       return NumericIdentity::MM;
    case CSSUnitType::CSS_Q:        return NumericIdentity::Q;
    case CSSUnitType::CSS_IN:       return NumericIdentity::INCH;
    case CSSUnitType::CSS_PT:       return NumericIdentity::PT;
    case CSSUnitType::CSS_PC:       return NumericIdentity::PC;
    case CSSUnitType::CSS_EM:       return NumericIdentity::EM;
    case CSSUnitType::CSS_EX:       return NumericIdentity::EX;
    case CSSUnitType::CSS_LH:       return NumericIdentity::LH;
    case CSSUnitType::CSS_CAP:      return NumericIdentity::CAP;
    case CSSUnitType::CSS_CH:       return NumericIdentity::CH;
    case CSSUnitType::CSS_IC:       return NumericIdentity::IC;
    case CSSUnitType::CSS_RCAP:     return NumericIdentity::RCAP;
    case CSSUnitType::CSS_RCH:      return NumericIdentity::RCH;
    case CSSUnitType::CSS_REM:      return NumericIdentity::REM;
    case CSSUnitType::CSS_REX:      return NumericIdentity::REX;
    case CSSUnitType::CSS_RIC:      return NumericIdentity::RIC;
    case CSSUnitType::CSS_RLH:      return NumericIdentity::RLH;
    case CSSUnitType::CSS_VW:       return NumericIdentity::VW;
    case CSSUnitType::CSS_VH:       return NumericIdentity::VH;
    case CSSUnitType::CSS_VMIN:     return NumericIdentity::VMIN;
    case CSSUnitType::CSS_VMAX:     return NumericIdentity::VMAX;
    case CSSUnitType::CSS_VB:       return NumericIdentity::VB;
    case CSSUnitType::CSS_VI:       return NumericIdentity::VI;
    case CSSUnitType::CSS_SVW:      return NumericIdentity::SVW;
    case CSSUnitType::CSS_SVH:      return NumericIdentity::SVH;
    case CSSUnitType::CSS_SVMIN:    return NumericIdentity::SVMIN;
    case CSSUnitType::CSS_SVMAX:    return NumericIdentity::SVMAX;
    case CSSUnitType::CSS_SVB:      return NumericIdentity::SVB;
    case CSSUnitType::CSS_SVI:      return NumericIdentity::SVI;
    case CSSUnitType::CSS_LVW:      return NumericIdentity::LVW;
    case CSSUnitType::CSS_LVH:      return NumericIdentity::LVH;
    case CSSUnitType::CSS_LVMIN:    return NumericIdentity::LVMIN;
    case CSSUnitType::CSS_LVMAX:    return NumericIdentity::LVMAX;
    case CSSUnitType::CSS_LVB:      return NumericIdentity::LVB;
    case CSSUnitType::CSS_LVI:      return NumericIdentity::LVI;
    case CSSUnitType::CSS_DVW:      return NumericIdentity::DVW;
    case CSSUnitType::CSS_DVH:      return NumericIdentity::DVH;
    case CSSUnitType::CSS_DVMIN:    return NumericIdentity::DVMIN;
    case CSSUnitType::CSS_DVMAX:    return NumericIdentity::DVMAX;
    case CSSUnitType::CSS_DVB:      return NumericIdentity::DVB;
    case CSSUnitType::CSS_DVI:      return NumericIdentity::DVI;
    case CSSUnitType::CSS_CQW:      return NumericIdentity::CQW;
    case CSSUnitType::CSS_CQH:      return NumericIdentity::CQH;
    case CSSUnitType::CSS_CQI:      return NumericIdentity::CQI;
    case CSSUnitType::CSS_CQB:      return NumericIdentity::CQB;
    case CSSUnitType::CSS_CQMIN:    return NumericIdentity::CQMIN;
    case CSSUnitType::CSS_CQMAX:    return NumericIdentity::CQMAX;
    case CSSUnitType::CSS_RAD:      return NumericIdentity::RAD;
    case CSSUnitType::CSS_GRAD:     return NumericIdentity::GRAD;
    case CSSUnitType::CSS_TURN:     return NumericIdentity::TURN;
    case CSSUnitType::CSS_MS:       return NumericIdentity::MS;
    case CSSUnitType::CSS_KHZ:      return NumericIdentity::KHZ;
    case CSSUnitType::CSS_X:        return NumericIdentity::X;
    case CSSUnitType::CSS_DPI:      return NumericIdentity::DPI;
    case CSSUnitType::CSS_DPCM:     return NumericIdentity::DPCM;

    // Invalid types for NonCanonicalDimension.
    case CSSUnitType::CSS_NUMBER:
    case CSSUnitType::CSS_INTEGER:
    case CSSUnitType::CSS_PERCENTAGE:
    case CSSUnitType::CSS_PX:
    case CSSUnitType::CSS_DEG:
    case CSSUnitType::CSS_S:
    case CSSUnitType::CSS_HZ:
    case CSSUnitType::CSS_DPPX:
    case CSSUnitType::CSS_FR:
    case CSSUnitType::CSS_ATTR:
    case CSSUnitType::CSS_CALC:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_ANGLE:
    case CSSUnitType::CSS_CALC_PERCENTAGE_WITH_LENGTH:
    case CSSUnitType::CSS_DIMENSION:
    case CSSUnitType::CSS_FONT_FAMILY:
    case CSSUnitType::CSS_IDENT:
    case CSSUnitType::CSS_PROPERTY_ID:
    case CSSUnitType::CSS_QUIRKY_EM:
    case CSSUnitType::CSS_STRING:
    case CSSUnitType::CSS_UNKNOWN:
    case CSSUnitType::CSS_URI:
    case CSSUnitType::CSS_VALUE_ID:
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
