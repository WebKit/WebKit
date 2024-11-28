/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CSSPropertyParserConsumer+MetaConsumerDefinitions.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

struct LengthValidator {
    static constexpr bool isValid(CSSUnitType unitType, CSSPropertyParserOptions options)
    {
        switch (unitType) {
        case CSSUnitType::CSS_QUIRKY_EM:
            if (!isUASheetBehavior(options.parserMode))
                return false;
            FALLTHROUGH;
        case CSSUnitType::CSS_EM:
        case CSSUnitType::CSS_REM:
        case CSSUnitType::CSS_LH:
        case CSSUnitType::CSS_RLH:
        case CSSUnitType::CSS_CAP:
        case CSSUnitType::CSS_RCAP:
        case CSSUnitType::CSS_CH:
        case CSSUnitType::CSS_RCH:
        case CSSUnitType::CSS_IC:
        case CSSUnitType::CSS_RIC:
        case CSSUnitType::CSS_EX:
        case CSSUnitType::CSS_REX:
        case CSSUnitType::CSS_PX:
        case CSSUnitType::CSS_CM:
        case CSSUnitType::CSS_MM:
        case CSSUnitType::CSS_IN:
        case CSSUnitType::CSS_PT:
        case CSSUnitType::CSS_PC:
        case CSSUnitType::CSS_VW:
        case CSSUnitType::CSS_VH:
        case CSSUnitType::CSS_VMIN:
        case CSSUnitType::CSS_VMAX:
        case CSSUnitType::CSS_VB:
        case CSSUnitType::CSS_VI:
        case CSSUnitType::CSS_SVW:
        case CSSUnitType::CSS_SVH:
        case CSSUnitType::CSS_SVMIN:
        case CSSUnitType::CSS_SVMAX:
        case CSSUnitType::CSS_SVB:
        case CSSUnitType::CSS_SVI:
        case CSSUnitType::CSS_LVW:
        case CSSUnitType::CSS_LVH:
        case CSSUnitType::CSS_LVMIN:
        case CSSUnitType::CSS_LVMAX:
        case CSSUnitType::CSS_LVB:
        case CSSUnitType::CSS_LVI:
        case CSSUnitType::CSS_DVW:
        case CSSUnitType::CSS_DVH:
        case CSSUnitType::CSS_DVMIN:
        case CSSUnitType::CSS_DVMAX:
        case CSSUnitType::CSS_DVB:
        case CSSUnitType::CSS_DVI:
        case CSSUnitType::CSS_Q:
        case CSSUnitType::CSS_CQW:
        case CSSUnitType::CSS_CQH:
        case CSSUnitType::CSS_CQI:
        case CSSUnitType::CSS_CQB:
        case CSSUnitType::CSS_CQMIN:
        case CSSUnitType::CSS_CQMAX:
            return true;

        default:
            return false;
        }
    }

    template<auto R> static bool isValid(CSS::LengthRaw<R> raw, CSSPropertyParserOptions)
    {
        // Values other than 0 and +/-∞ are not supported for <length> numeric ranges currently.
        return isValidNonCanonicalizableDimensionValue(raw);
    }
};

template<auto R> struct ConsumerDefinition<CSS::Length<R>> {
    using FunctionToken = FunctionConsumerForCalcValues<CSS::Length<R>>;
    using DimensionToken = DimensionConsumer<CSS::Length<R>, LengthValidator>;
    using NumberToken = NumberConsumerForUnitlessValues<CSS::Length<R>, LengthValidator, CSSUnitType::CSS_PX>;
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
