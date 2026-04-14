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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleCounterReset.h"

#include "CSSCustomIdentValue.h"
#include "CSSFunctionValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StyleValueTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Conversion

auto CSSValueConversion<CounterResetValue>::operator()(BuilderState& state, const CSSValue& value) -> CounterResetValue
{
    // Case 1: reversed(<counter-name>) without explicit value — standalone CSSFunctionValue.
    if (auto* function = dynamicDowncast<CSSFunctionValue>(value)) {
        if (function->name() == CSSValueReversed && function->length() == 1) {
            if (auto* identValue = dynamicDowncast<CSSCustomIdentValue>(*function->item(0))) {
                return {
                    toStyleFromCSSValue<CustomIdent>(state, *identValue),
                    0_css_integer,
                    true,
                    false,
                };
            }
        }
        return { CustomIdent { emptyAtom() }, 0_css_integer };
    }

    // Case 2: CSSValuePair — either normal or reversed with explicit value.
    auto* pair = dynamicDowncast<CSSValuePair>(value);
    if (!pair)
        return { CustomIdent { emptyAtom() }, 0_css_integer };

    // Case 2a: reversed(<counter-name>) <integer> — first is CSSFunctionValue.
    if (auto* function = dynamicDowncast<CSSFunctionValue>(pair->first())) {
        if (function->name() == CSSValueReversed && function->length() == 1) {
            if (auto* identValue = dynamicDowncast<CSSCustomIdentValue>(*function->item(0))) {
                return {
                    toStyleFromCSSValue<CustomIdent>(state, *identValue),
                    toStyleFromCSSValue<Integer<>>(state, pair->second()),
                    true,
                    true,
                };
            }
        }
        return { CustomIdent { emptyAtom() }, 0_css_integer };
    }

    // Case 2b: <counter-name> <integer> — normal counter (existing path).
    auto* identValue = dynamicDowncast<CSSCustomIdentValue>(pair->first());
    if (!identValue)
        return { CustomIdent { emptyAtom() }, 0_css_integer };

    return {
        toStyleFromCSSValue<CustomIdent>(state, *identValue),
        toStyleFromCSSValue<Integer<>>(state, pair->second()),
    };
}

} // namespace Style
} // namespace WebCore
