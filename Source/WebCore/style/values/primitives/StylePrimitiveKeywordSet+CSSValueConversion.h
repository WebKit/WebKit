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

#include "CSSPrimitiveKeywordSet.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveKeyword+CSSValueConversion.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

template<CSS::PrimitiveKeywordSetDerived T> struct CSSValueConversion<T> {
    auto operator()(BuilderState& state, const CSSValue& value) -> T
    {
        if (RefPtr primitive = dynamicDowncast<CSSPrimitiveValue>(value)) {
            auto valueID = primitive->valueID();

            if (primitive->valueID() == T::emptyCase)
                return T::emptyCase;

            std::optional<T> result;
            auto processKeyword = [&](const auto keyword, CSSValueID valueID) -> bool {
                if (keyword.value == valueID) {
                    result = T { keyword };
                    return true;
                }
                return false;
            };
            WTF::apply([&](const auto& ...keyword) { (processKeyword(keyword, valueID) || ...); }, T::Keywords::tuple);
            if (result)
                return *result;

            state.setCurrentPropertyInvalidAtComputedValueTime();
            return { };
        }

        auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, value);
        if (!list)
            return { };

        T result;
        auto addKeyword = [&](const auto keyword, CSSValueID valueID) -> bool {
            if (keyword.value == valueID) {
                result.add(keyword);
                return true;
            }
            return false;
        };

        for (Ref primitive : *list) {
            auto valueID = primitive->valueID();

            bool success = WTF::apply([&](const auto& ...keyword) { return (addKeyword(keyword, valueID) || ...); }, T::Keywords::tuple);
            if (success)
                continue;

            state.setCurrentPropertyInvalidAtComputedValueTime();
            return { };
        }

        return result;
    }
};

} // namespace Style
} // namespace WebCore
