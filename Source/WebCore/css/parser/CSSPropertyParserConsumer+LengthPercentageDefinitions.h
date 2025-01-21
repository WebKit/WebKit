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

#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumerDefinitions.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

struct LengthPercentageValidator {
    static constexpr std::optional<CSS::LengthPercentageUnit> validate(CSSUnitType unitType, CSSPropertyParserOptions options)
    {
        // NOTE: Percentages are handled explicitly by the PercentageValidator, so this only
        // needs to be concerned with the Length units.
        if (auto result = LengthValidator::validate(unitType, options))
            return static_cast<CSS::LengthPercentageUnit>(*result);
        return std::nullopt;
    }

    template<auto R, typename V> static bool isValid(CSS::LengthPercentageRaw<R, V> raw, CSSPropertyParserOptions)
    {
        // Values other than 0 and +/-∞ are not supported for <length-percentage> numeric ranges currently.
        return isValidNonCanonicalizableDimensionValue(raw);
    }
};

template<auto R, typename V> struct ConsumerDefinition<CSS::LengthPercentage<R, V>> {
    using FunctionToken = FunctionConsumerForCalcValues<CSS::LengthPercentage<R, V>>;
    using DimensionToken = DimensionConsumer<CSS::LengthPercentage<R, V>, LengthPercentageValidator>;
    using PercentageToken = PercentageConsumer<CSS::LengthPercentage<R, V>, LengthPercentageValidator>;
    using NumberToken = NumberConsumerForUnitlessValues<CSS::LengthPercentage<R, V>, LengthPercentageValidator, CSS::LengthUnit::Px>;
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
