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

#pragma once

#include "StylePrimitiveNumericOrKeyword+Conversions.h"
#include "StylePrimitiveNumericTypes+CSSValueConversions.h"

namespace WebCore {
namespace Style {

class BuilderState;

template<Numeric N, CSS::PrimitiveKeyword... Ks> struct CSSValueConversions<PrimitiveNumericOrKeyword<N, Ks...>> {
    using Result = PrimitiveNumericOrKeyword<N, Ks...>;
    using CSSType = typename ToCSSMapping<Result>::type;

    Result operator()(const CSSValue& value, const BuilderState& state)
    {
        if (auto result = tryIdent(value.valueID()))
            return *result;
        return { convertFromCSSValue<N>(value, state) };
    }

    std::optional<Result> tryIdent(CSSValueID ident)
    {
        auto process = [&]<auto Id>(Constant<Id> x, auto& result) {
            if (ident != Id)
                return false;
            result = Result { x };
            return true;
        };

        return WTF::apply([&](const auto& ...x) {
            std::optional<Result> result;
            (process(x, result) || ...);
            return result;
        }, Result::KeywordList::tuple);
    }
};

} // namespace Style
} // namespace WebCore
