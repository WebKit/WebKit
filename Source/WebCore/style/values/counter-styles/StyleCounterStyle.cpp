/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleCounterStyle.h"

#include "CSSCounterStyle.h"
#include "CSSKeywordValue.h"
#include "CSSPropertyParserConsumer+CounterStyles.h"
#include "CSSValueKeywords.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto ToCSS<CounterStyle>::operator()(const CounterStyle& value, const RenderStyle& style) -> CSS::CounterStyle
{
    return { toCSS(value.identifier, style) };
}

auto ToStyle<CSS::CounterStyle>::operator()(const CSS::CounterStyle& value, const BuilderState& state) -> CounterStyle
{
    return WTF::switchOn(value.identifier,
        [&](CSSValueID predefinedKeyword) -> CounterStyle {
            return { CustomIdent { nameStringForSerialization(predefinedKeyword) } };
        },
        [&](const CSS::CustomIdent& customIdent) -> CounterStyle {
            return { toStyle(customIdent, state) };
        }
    );
}

auto CSSValueConversion<CounterStyle>::operator()(BuilderState& state, const CSSValue& value) -> CounterStyle
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        if (auto valueID = keywordValue->valueID(); CSSPropertyParserHelpers::isPredefinedCounterStyle(valueID))
            return { CustomIdent { nameStringForSerialization(valueID) } };

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return { CustomIdent { nullAtom() } };
    }

    return { toStyleFromCSSValue<CustomIdent>(state, value) };
}

} // namespace Style
} // namespace WebCore
