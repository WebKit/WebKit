/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "CSSPrimitiveNumericUnits.h"

#include "CSSCalcSymbolTable.h"

namespace WebCore {
namespace CSS {

ASCIILiteral unitString(AngleUnit angleUnit)
{
    using enum AngleUnit;

    switch (angleUnit) {
    case Deg:    return "deg"_s;
    case Rad:    return "rad"_s;
    case Grad:   return "grad"_s;
    case Turn:   return "turn"_s;
    }

    WTF_UNREACHABLE();
}

ASCIILiteral unitString(LengthUnit lengthUnit)
{
    using enum LengthUnit;

    switch (lengthUnit) {
    case Px:        return "px"_s;
    case Cm:        return "cm"_s;
    case Mm:        return "mm"_s;
    case Q:         return "Q"_s;
    case In:        return "in"_s;
    case Pt:        return "pt"_s;
    case Pc:        return "pc"_s;
    case Em:        return "em"_s;
    case QuirkyEm:  return "em"_s;
    case Ex:        return "ex"_s;
    case Lh:        return "lh"_s;
    case Cap:       return "cap"_s;
    case Ch:        return "ch"_s;
    case Ic:        return "ic"_s;
    case Rcap:      return "rcap"_s;
    case Rch:       return "rch"_s;
    case Rem:       return "rem"_s;
    case Rex:       return "rex"_s;
    case Ric:       return "ric"_s;
    case Rlh:       return "rlh"_s;
    case Vw:        return "vw"_s;
    case Vh:        return "vh"_s;
    case Vmin:      return "vmin"_s;
    case Vmax:      return "vmax"_s;
    case Vb:        return "vb"_s;
    case Vi:        return "vi"_s;
    case Svw:       return "svw"_s;
    case Svh:       return "svh"_s;
    case Svmin:     return "svmin"_s;
    case Svmax:     return "svmax"_s;
    case Svb:       return "svb"_s;
    case Svi:       return "svi"_s;
    case Lvw:       return "lvw"_s;
    case Lvh:       return "lvh"_s;
    case Lvmin:     return "lvmin"_s;
    case Lvmax:     return "lvmax"_s;
    case Lvb:       return "lvb"_s;
    case Lvi:       return "lvi"_s;
    case Dvw:       return "dvw"_s;
    case Dvh:       return "dvh"_s;
    case Dvmin:     return "dvmin"_s;
    case Dvmax:     return "dvmax"_s;
    case Dvb:       return "dvb"_s;
    case Dvi:       return "dvi"_s;
    case Cqw:       return "cqw"_s;
    case Cqh:       return "cqh"_s;
    case Cqi:       return "cqi"_s;
    case Cqb:       return "cqb"_s;
    case Cqmin:     return "cqmin"_s;
    case Cqmax:     return "cqmax"_s;
    }

    WTF_UNREACHABLE();

}

ASCIILiteral unitString(TimeUnit timeUnit)
{
    using enum TimeUnit;

    switch (timeUnit) {
    case S:    return "s"_s;
    case Ms:   return "ms"_s;
    }

    WTF_UNREACHABLE();
}

ASCIILiteral unitString(FrequencyUnit frequencyUnit)
{
    using enum FrequencyUnit;

    switch (frequencyUnit) {
    case Hz:    return "hz"_s;
    case Khz:   return "khz"_s;;
    }

    WTF_UNREACHABLE();
}

ASCIILiteral unitString(ResolutionUnit resolutionUnit)
{
    using enum ResolutionUnit;

    switch (resolutionUnit) {
    case Dppx:   return "dppx"_s;
    case X:      return "x"_s;
    case Dpi:    return "dpi"_s;
    case Dpcm:   return "dpcm"_s;
    }

    WTF_UNREACHABLE();
}

// Ensure the angle units in `AngleUnit` and `AnglePercentageUnit` are all equal.
static_assert(enumToUnderlyingType(AngleUnit::Deg) == enumToUnderlyingType(AnglePercentageUnit::Deg));
static_assert(enumToUnderlyingType(AngleUnit::Rad) == enumToUnderlyingType(AnglePercentageUnit::Rad));
static_assert(enumToUnderlyingType(AngleUnit::Grad) == enumToUnderlyingType(AnglePercentageUnit::Grad));
static_assert(enumToUnderlyingType(AngleUnit::Turn) == enumToUnderlyingType(AnglePercentageUnit::Turn));

// Ensure the length units in `LengthUnit` and `LengthPercentageUnit` are all equal.
static_assert(enumToUnderlyingType(LengthUnit::Px) == enumToUnderlyingType(LengthPercentageUnit::Px));
static_assert(enumToUnderlyingType(LengthUnit::Cm) == enumToUnderlyingType(LengthPercentageUnit::Cm));
static_assert(enumToUnderlyingType(LengthUnit::Mm) == enumToUnderlyingType(LengthPercentageUnit::Mm));
static_assert(enumToUnderlyingType(LengthUnit::Q) == enumToUnderlyingType(LengthPercentageUnit::Q));
static_assert(enumToUnderlyingType(LengthUnit::In) == enumToUnderlyingType(LengthPercentageUnit::In));
static_assert(enumToUnderlyingType(LengthUnit::Pt) == enumToUnderlyingType(LengthPercentageUnit::Pt));
static_assert(enumToUnderlyingType(LengthUnit::Pc) == enumToUnderlyingType(LengthPercentageUnit::Pc));
static_assert(enumToUnderlyingType(LengthUnit::Em) == enumToUnderlyingType(LengthPercentageUnit::Em));
static_assert(enumToUnderlyingType(LengthUnit::QuirkyEm) == enumToUnderlyingType(LengthPercentageUnit::QuirkyEm));
static_assert(enumToUnderlyingType(LengthUnit::Ex) == enumToUnderlyingType(LengthPercentageUnit::Ex));
static_assert(enumToUnderlyingType(LengthUnit::Lh) == enumToUnderlyingType(LengthPercentageUnit::Lh));
static_assert(enumToUnderlyingType(LengthUnit::Cap) == enumToUnderlyingType(LengthPercentageUnit::Cap));
static_assert(enumToUnderlyingType(LengthUnit::Ch) == enumToUnderlyingType(LengthPercentageUnit::Ch));
static_assert(enumToUnderlyingType(LengthUnit::Ic) == enumToUnderlyingType(LengthPercentageUnit::Ic));
static_assert(enumToUnderlyingType(LengthUnit::Rcap) == enumToUnderlyingType(LengthPercentageUnit::Rcap));
static_assert(enumToUnderlyingType(LengthUnit::Rch) == enumToUnderlyingType(LengthPercentageUnit::Rch));
static_assert(enumToUnderlyingType(LengthUnit::Rem) == enumToUnderlyingType(LengthPercentageUnit::Rem));
static_assert(enumToUnderlyingType(LengthUnit::Rex) == enumToUnderlyingType(LengthPercentageUnit::Rex));
static_assert(enumToUnderlyingType(LengthUnit::Ric) == enumToUnderlyingType(LengthPercentageUnit::Ric));
static_assert(enumToUnderlyingType(LengthUnit::Rlh) == enumToUnderlyingType(LengthPercentageUnit::Rlh));
static_assert(enumToUnderlyingType(LengthUnit::Vw) == enumToUnderlyingType(LengthPercentageUnit::Vw));
static_assert(enumToUnderlyingType(LengthUnit::Vh) == enumToUnderlyingType(LengthPercentageUnit::Vh));
static_assert(enumToUnderlyingType(LengthUnit::Vmin) == enumToUnderlyingType(LengthPercentageUnit::Vmin));
static_assert(enumToUnderlyingType(LengthUnit::Vmax) == enumToUnderlyingType(LengthPercentageUnit::Vmax));
static_assert(enumToUnderlyingType(LengthUnit::Vb) == enumToUnderlyingType(LengthPercentageUnit::Vb));
static_assert(enumToUnderlyingType(LengthUnit::Vi) == enumToUnderlyingType(LengthPercentageUnit::Vi));
static_assert(enumToUnderlyingType(LengthUnit::Svw) == enumToUnderlyingType(LengthPercentageUnit::Svw));
static_assert(enumToUnderlyingType(LengthUnit::Svh) == enumToUnderlyingType(LengthPercentageUnit::Svh));
static_assert(enumToUnderlyingType(LengthUnit::Svmin) == enumToUnderlyingType(LengthPercentageUnit::Svmin));
static_assert(enumToUnderlyingType(LengthUnit::Svmax) == enumToUnderlyingType(LengthPercentageUnit::Svmax));
static_assert(enumToUnderlyingType(LengthUnit::Svb) == enumToUnderlyingType(LengthPercentageUnit::Svb));
static_assert(enumToUnderlyingType(LengthUnit::Svi) == enumToUnderlyingType(LengthPercentageUnit::Svi));
static_assert(enumToUnderlyingType(LengthUnit::Lvw) == enumToUnderlyingType(LengthPercentageUnit::Lvw));
static_assert(enumToUnderlyingType(LengthUnit::Lvh) == enumToUnderlyingType(LengthPercentageUnit::Lvh));
static_assert(enumToUnderlyingType(LengthUnit::Lvmin) == enumToUnderlyingType(LengthPercentageUnit::Lvmin));
static_assert(enumToUnderlyingType(LengthUnit::Lvmax) == enumToUnderlyingType(LengthPercentageUnit::Lvmax));
static_assert(enumToUnderlyingType(LengthUnit::Lvb) == enumToUnderlyingType(LengthPercentageUnit::Lvb));
static_assert(enumToUnderlyingType(LengthUnit::Lvi) == enumToUnderlyingType(LengthPercentageUnit::Lvi));
static_assert(enumToUnderlyingType(LengthUnit::Dvw) == enumToUnderlyingType(LengthPercentageUnit::Dvw));
static_assert(enumToUnderlyingType(LengthUnit::Dvh) == enumToUnderlyingType(LengthPercentageUnit::Dvh));
static_assert(enumToUnderlyingType(LengthUnit::Dvmin) == enumToUnderlyingType(LengthPercentageUnit::Dvmin));
static_assert(enumToUnderlyingType(LengthUnit::Dvmax) == enumToUnderlyingType(LengthPercentageUnit::Dvmax));
static_assert(enumToUnderlyingType(LengthUnit::Dvb) == enumToUnderlyingType(LengthPercentageUnit::Dvb));
static_assert(enumToUnderlyingType(LengthUnit::Dvi) == enumToUnderlyingType(LengthPercentageUnit::Dvi));
static_assert(enumToUnderlyingType(LengthUnit::Cqw) == enumToUnderlyingType(LengthPercentageUnit::Cqw));
static_assert(enumToUnderlyingType(LengthUnit::Cqh) == enumToUnderlyingType(LengthPercentageUnit::Cqh));
static_assert(enumToUnderlyingType(LengthUnit::Cqi) == enumToUnderlyingType(LengthPercentageUnit::Cqi));
static_assert(enumToUnderlyingType(LengthUnit::Cqb) == enumToUnderlyingType(LengthPercentageUnit::Cqb));
static_assert(enumToUnderlyingType(LengthUnit::Cqmin) == enumToUnderlyingType(LengthPercentageUnit::Cqmin));
static_assert(enumToUnderlyingType(LengthUnit::Cqmax) == enumToUnderlyingType(LengthPercentageUnit::Cqmax));

} // namespace CSS
} // namespace WebCore
