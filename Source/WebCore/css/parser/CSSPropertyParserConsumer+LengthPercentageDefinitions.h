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

#include "CSSParserToken.h"
#include "CSSPropertyParserConsumer+MetaConsumerDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+UnevaluatedCalc.h"
#include <optional>
#include <wtf/Brigand.h>

namespace WebCore {

class CSSCalcSymbolsAllowed;
class CSSParserTokenRange;

namespace CSSPropertyParserHelpers {

std::optional<LengthPercentageRaw> validatedRange(LengthPercentageRaw, CSSPropertyParserOptions);

struct LengthPercentageKnownTokenTypeFunctionConsumer {
    static constexpr CSSParserTokenType tokenType = FunctionToken;
    static std::optional<UnevaluatedCalc<LengthPercentageRaw>> consume(CSSParserTokenRange&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions);
};

struct LengthPercentageKnownTokenTypeDimensionConsumer {
    static constexpr CSSParserTokenType tokenType = DimensionToken;
    static std::optional<LengthPercentageRaw> consume(CSSParserTokenRange&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions);
};

struct LengthPercentageKnownTokenTypePercentConsumer {
    static constexpr CSSParserTokenType tokenType = PercentageToken;
    static std::optional<LengthPercentageRaw> consume(CSSParserTokenRange&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions);
};

struct LengthPercentageKnownTokenTypeNumberConsumer {
    static constexpr CSSParserTokenType tokenType = NumberToken;
    static std::optional<LengthPercentageRaw> consume(CSSParserTokenRange&, CSSCalcSymbolsAllowed, CSSPropertyParserOptions);
};

template<> struct ConsumerDefinition<LengthPercentageRaw> {
    using type = brigand::list<LengthPercentageRaw, UnevaluatedCalc<LengthPercentageRaw>>;

    using FunctionToken = LengthPercentageKnownTokenTypeFunctionConsumer;
    using DimensionToken = LengthPercentageKnownTokenTypeDimensionConsumer;
    using PercentageToken = LengthPercentageKnownTokenTypePercentConsumer;
    using NumberToken = LengthPercentageKnownTokenTypeNumberConsumer;
};

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
