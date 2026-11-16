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
#include "CSSSymbolsFunctionValue.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

std::optional<CSS::SymbolsType> keywordFromSymbolsSystemForSerialization(CSSCounterStyleDescriptors::System system)
{
    switch (system) {
    case CSSCounterStyleDescriptors::System::Cyclic:
        return CSS::SymbolsType { CSS::Keyword::Cyclic { } };
    case CSSCounterStyleDescriptors::System::Numeric:
        return CSS::SymbolsType { CSS::Keyword::Numeric { } };
    case CSSCounterStyleDescriptors::System::Alphabetic:
        return CSS::SymbolsType { CSS::Keyword::Alphabetic { } };
    case CSSCounterStyleDescriptors::System::Fixed:
        return CSS::SymbolsType { CSS::Keyword::Fixed { } };
    // `symbolic` is the default `<symbols-type>` and is omitted from serialization.
    default:
        return std::nullopt;
    }
}

CSSCounterStyleDescriptors::System symbolsSystemFromKeyword(std::optional<CSS::SymbolsType> keyword)
{
    if (!keyword)
        return CSSCounterStyleDescriptors::System::Symbolic;
    return WTF::switchOn(*keyword,
        [](CSS::Keyword::Cyclic) { return CSSCounterStyleDescriptors::System::Cyclic; },
        [](CSS::Keyword::Numeric) { return CSSCounterStyleDescriptors::System::Numeric; },
        [](CSS::Keyword::Alphabetic) { return CSSCounterStyleDescriptors::System::Alphabetic; },
        [](CSS::Keyword::Symbolic) { return CSSCounterStyleDescriptors::System::Symbolic; },
        [](CSS::Keyword::Fixed) { return CSSCounterStyleDescriptors::System::Fixed; }
    );
}

auto ToCSS<CounterStyle>::operator()(const CounterStyle& value, const Style::ComputedStyle& style) -> CSS::CounterStyle
{
    return WTF::switchOn(value,
        [&](const CustomIdent& customIdent) -> CSS::CounterStyle {
            return { toCSS(customIdent, style) };
        },
        [&](const SymbolsFunction& symbolsFunction) -> CSS::CounterStyle {
            return { toCSS(symbolsFunction, style) };
        }
    );
}

auto ToStyle<CSS::CounterStyle>::operator()(const CSS::CounterStyle& value, const BuilderState& state) -> CounterStyle
{
    return WTF::switchOn(value.identifier,
        [&](const CSS::Keyword& predefinedKeyword) -> CounterStyle {
            return { CustomIdent { nameStringForSerialization(predefinedKeyword.value) } };
        },
        [&](const CSS::CustomIdent& customIdent) -> CounterStyle {
            return { toStyle(customIdent, state) };
        },
        [&](const CSS::SymbolsFunction& symbolsFunction) -> CounterStyle {
            return { toStyle(symbolsFunction, state) };
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

Ref<CSSValue> CSSValueCreation<SymbolsFunction>::operator()(CSSValuePool&, const Style::ComputedStyle& style, const SymbolsFunction& value)
{
    return CSSSymbolsFunctionValue::create(toCSS(value, style));
}

} // namespace Style
} // namespace WebCore
