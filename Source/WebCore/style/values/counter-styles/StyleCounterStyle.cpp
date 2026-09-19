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
#include "CSSRegisteredCounterStyle.h"
#include "CSSSymbolsFunctionValue.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

RefPtr<CSSRegisteredCounterStyle> CounterStyle::trySymbolsFunctionCounterStyle() const
{
    auto* symbolsFunction = std::get_if<SymbolsFunction>(&m_value);
    if (!symbolsFunction)
        return nullptr;
    return CSSRegisteredCounterStyle::createForSymbolsFunction(symbolsSystemFromKeyword((*symbolsFunction)->system), (*symbolsFunction)->symbols.value);
}

// MARK: - Conversion

static std::optional<CSS::SymbolsType> symbolsTypeFromKeywordID(CSSValueID keywordID)
{
    switch (keywordID) {
    case CSSValueCyclic:
        return CSS::SymbolsType { CSS::Keyword::Cyclic { } };
    case CSSValueNumeric:
        return CSS::SymbolsType { CSS::Keyword::Numeric { } };
    case CSSValueAlphabetic:
        return CSS::SymbolsType { CSS::Keyword::Alphabetic { } };
    case CSSValueSymbolic:
        return CSS::SymbolsType { CSS::Keyword::Symbolic { } };
    case CSSValueFixed:
        return CSS::SymbolsType { CSS::Keyword::Fixed { } };
    default:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
}

std::optional<CSS::SymbolsType> keywordFromSymbolsSystem(CSSCounterStyleDescriptors::System system)
{
    auto keywordID = symbolsTypeKeywordFromSystem(system);
    return keywordID ? symbolsTypeFromKeywordID(*keywordID) : std::nullopt;
}

CSSCounterStyleDescriptors::System symbolsSystemFromKeyword(std::optional<CSS::SymbolsType> keyword)
{
    if (!keyword)
        return CSSCounterStyleDescriptors::System::Symbolic;
    auto keywordID = WTF::switchOn(*keyword, [](auto keyword) { return decltype(keyword)::value; });
    return systemFromSymbolsTypeKeyword(keywordID).value_or(CSSCounterStyleDescriptors::System::Symbolic);
}

auto ToCSS<CounterStyle>::operator()(const CounterStyle& value, const Style::ComputedStyle& style) -> CSS::CounterStyle
{
    return WTF::switchOn(value,
        [&](const CustomIdent& customIdent) -> CSS::CounterStyle {
            return { toCSS(customIdent, style) };
        },
        [&](const CounterStyle::SymbolsFunction& symbolsFunction) -> CSS::CounterStyle {
            auto symbols = symbolsFunction->symbols.map([](auto& symbol) { return CSS::String { symbol }; });
            return { CSS::SymbolsFunction { CSS::SymbolsParameters { symbolsFunction->system, WTF::move(symbols) } } };
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
            auto symbols = symbolsFunction->symbols.map([](auto& symbol) { return symbol.value; });
            return { CounterStyle::SymbolsFunction { CounterStyle::SymbolsParameters { symbolsFunction->system, WTF::move(symbols) } } };
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

Ref<CSSValue> CSSValueCreation<CounterStyle::SymbolsFunction>::operator()(CSSValuePool&, const Style::ComputedStyle&, const CounterStyle::SymbolsFunction& value)
{
    auto symbols = value->symbols.map([](auto& symbol) { return CSS::String { symbol }; });
    return CSSSymbolsFunctionValue::create(CSS::SymbolsFunction { CSS::SymbolsParameters { value->system, WTF::move(symbols) } });
}

} // namespace Style
} // namespace WebCore
