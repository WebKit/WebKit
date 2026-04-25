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
static_assert(std::to_underlying(AngleUnit::Deg) == std::to_underlying(AnglePercentageUnit::Deg));
static_assert(std::to_underlying(AngleUnit::Rad) == std::to_underlying(AnglePercentageUnit::Rad));
static_assert(std::to_underlying(AngleUnit::Grad) == std::to_underlying(AnglePercentageUnit::Grad));
static_assert(std::to_underlying(AngleUnit::Turn) == std::to_underlying(AnglePercentageUnit::Turn));

// Ensure the length units in `LengthUnit` and `LengthPercentageUnit` are all equal.
static_assert(std::to_underlying(LengthUnit::Px) == std::to_underlying(LengthPercentageUnit::Px));
static_assert(std::to_underlying(LengthUnit::Cm) == std::to_underlying(LengthPercentageUnit::Cm));
static_assert(std::to_underlying(LengthUnit::Mm) == std::to_underlying(LengthPercentageUnit::Mm));
static_assert(std::to_underlying(LengthUnit::Q) == std::to_underlying(LengthPercentageUnit::Q));
static_assert(std::to_underlying(LengthUnit::In) == std::to_underlying(LengthPercentageUnit::In));
static_assert(std::to_underlying(LengthUnit::Pt) == std::to_underlying(LengthPercentageUnit::Pt));
static_assert(std::to_underlying(LengthUnit::Pc) == std::to_underlying(LengthPercentageUnit::Pc));
static_assert(std::to_underlying(LengthUnit::Em) == std::to_underlying(LengthPercentageUnit::Em));
static_assert(std::to_underlying(LengthUnit::QuirkyEm) == std::to_underlying(LengthPercentageUnit::QuirkyEm));
static_assert(std::to_underlying(LengthUnit::Ex) == std::to_underlying(LengthPercentageUnit::Ex));
static_assert(std::to_underlying(LengthUnit::Lh) == std::to_underlying(LengthPercentageUnit::Lh));
static_assert(std::to_underlying(LengthUnit::Cap) == std::to_underlying(LengthPercentageUnit::Cap));
static_assert(std::to_underlying(LengthUnit::Ch) == std::to_underlying(LengthPercentageUnit::Ch));
static_assert(std::to_underlying(LengthUnit::Ic) == std::to_underlying(LengthPercentageUnit::Ic));
static_assert(std::to_underlying(LengthUnit::Rcap) == std::to_underlying(LengthPercentageUnit::Rcap));
static_assert(std::to_underlying(LengthUnit::Rch) == std::to_underlying(LengthPercentageUnit::Rch));
static_assert(std::to_underlying(LengthUnit::Rem) == std::to_underlying(LengthPercentageUnit::Rem));
static_assert(std::to_underlying(LengthUnit::Rex) == std::to_underlying(LengthPercentageUnit::Rex));
static_assert(std::to_underlying(LengthUnit::Ric) == std::to_underlying(LengthPercentageUnit::Ric));
static_assert(std::to_underlying(LengthUnit::Rlh) == std::to_underlying(LengthPercentageUnit::Rlh));
static_assert(std::to_underlying(LengthUnit::Vw) == std::to_underlying(LengthPercentageUnit::Vw));
static_assert(std::to_underlying(LengthUnit::Vh) == std::to_underlying(LengthPercentageUnit::Vh));
static_assert(std::to_underlying(LengthUnit::Vmin) == std::to_underlying(LengthPercentageUnit::Vmin));
static_assert(std::to_underlying(LengthUnit::Vmax) == std::to_underlying(LengthPercentageUnit::Vmax));
static_assert(std::to_underlying(LengthUnit::Vb) == std::to_underlying(LengthPercentageUnit::Vb));
static_assert(std::to_underlying(LengthUnit::Vi) == std::to_underlying(LengthPercentageUnit::Vi));
static_assert(std::to_underlying(LengthUnit::Svw) == std::to_underlying(LengthPercentageUnit::Svw));
static_assert(std::to_underlying(LengthUnit::Svh) == std::to_underlying(LengthPercentageUnit::Svh));
static_assert(std::to_underlying(LengthUnit::Svmin) == std::to_underlying(LengthPercentageUnit::Svmin));
static_assert(std::to_underlying(LengthUnit::Svmax) == std::to_underlying(LengthPercentageUnit::Svmax));
static_assert(std::to_underlying(LengthUnit::Svb) == std::to_underlying(LengthPercentageUnit::Svb));
static_assert(std::to_underlying(LengthUnit::Svi) == std::to_underlying(LengthPercentageUnit::Svi));
static_assert(std::to_underlying(LengthUnit::Lvw) == std::to_underlying(LengthPercentageUnit::Lvw));
static_assert(std::to_underlying(LengthUnit::Lvh) == std::to_underlying(LengthPercentageUnit::Lvh));
static_assert(std::to_underlying(LengthUnit::Lvmin) == std::to_underlying(LengthPercentageUnit::Lvmin));
static_assert(std::to_underlying(LengthUnit::Lvmax) == std::to_underlying(LengthPercentageUnit::Lvmax));
static_assert(std::to_underlying(LengthUnit::Lvb) == std::to_underlying(LengthPercentageUnit::Lvb));
static_assert(std::to_underlying(LengthUnit::Lvi) == std::to_underlying(LengthPercentageUnit::Lvi));
static_assert(std::to_underlying(LengthUnit::Dvw) == std::to_underlying(LengthPercentageUnit::Dvw));
static_assert(std::to_underlying(LengthUnit::Dvh) == std::to_underlying(LengthPercentageUnit::Dvh));
static_assert(std::to_underlying(LengthUnit::Dvmin) == std::to_underlying(LengthPercentageUnit::Dvmin));
static_assert(std::to_underlying(LengthUnit::Dvmax) == std::to_underlying(LengthPercentageUnit::Dvmax));
static_assert(std::to_underlying(LengthUnit::Dvb) == std::to_underlying(LengthPercentageUnit::Dvb));
static_assert(std::to_underlying(LengthUnit::Dvi) == std::to_underlying(LengthPercentageUnit::Dvi));
static_assert(std::to_underlying(LengthUnit::Cqw) == std::to_underlying(LengthPercentageUnit::Cqw));
static_assert(std::to_underlying(LengthUnit::Cqh) == std::to_underlying(LengthPercentageUnit::Cqh));
static_assert(std::to_underlying(LengthUnit::Cqi) == std::to_underlying(LengthPercentageUnit::Cqi));
static_assert(std::to_underlying(LengthUnit::Cqb) == std::to_underlying(LengthPercentageUnit::Cqb));
static_assert(std::to_underlying(LengthUnit::Cqmin) == std::to_underlying(LengthPercentageUnit::Cqmin));
static_assert(std::to_underlying(LengthUnit::Cqmax) == std::to_underlying(LengthPercentageUnit::Cqmax));

} // namespace CSS
} // namespace WebCore
