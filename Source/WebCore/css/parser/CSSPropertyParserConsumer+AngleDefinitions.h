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

struct AngleValidator {
    static constexpr bool isValid(CSSUnitType unitType, CSSPropertyParserOptions)
    {
        switch (unitType) {
        case CSSUnitType::CSS_DEG:
        case CSSUnitType::CSS_RAD:
        case CSSUnitType::CSS_GRAD:
        case CSSUnitType::CSS_TURN:
            return true;

        default:
            return false;
        }
    }

    template<auto R> static bool isValid(CSS::AngleRaw<R> raw, CSSPropertyParserOptions)
    {
        return isValidDimensionValue(raw, [&] {
            auto canonicalValue = CSS::canonicalizeAngle(raw.value, raw.type);
            return canonicalValue >= raw.range.min && canonicalValue <= raw.range.max;
        });
    }
};

template<auto R> struct ConsumerDefinition<CSS::Angle<R>> {
    using FunctionToken = FunctionConsumerForCalcValues<CSS::Angle<R>>;
    using DimensionToken = DimensionConsumer<CSS::Angle<R>, AngleValidator>;
    using NumberToken = NumberConsumerForUnitlessValues<CSS::Angle<R>, AngleValidator, CSSUnitType::CSS_DEG>;
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
