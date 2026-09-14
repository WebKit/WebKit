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
#include "CSSFunctionValue.h"
#include "CSSKeywordValue.h"
#include "CSSMarkup.h"
#include "CSSPropertyParserConsumer+CounterStyles.h"
#include "CSSRegisteredCounterStyle.h"
#include "CSSStringValue.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "StyleBuilderChecking.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

RefPtr<CSSRegisteredCounterStyle> CounterStyle::trySymbolsFunctionCounterStyle() const
{
    auto* symbolsFunction = std::get_if<SymbolsFunction>(&m_value);
    if (!symbolsFunction)
        return nullptr;
    return CSSRegisteredCounterStyle::createForSymbolsFunction((*symbolsFunction)->system, (*symbolsFunction)->symbols.value);
}

// MARK: - Conversion

static std::optional<CSS::Keyword> keywordFromSystem(CSSCounterStyleDescriptors::System system)
{
    auto keywordID = symbolsTypeKeywordFromSystem(system);
    return keywordID ? std::make_optional(CSS::Keyword { *keywordID }) : std::nullopt;
}

auto ToCSS<CounterStyle>::operator()(const CounterStyle& value, const Style::ComputedStyle& style) -> CSS::CounterStyle
{
    return WTF::switchOn(value,
        [&](const CustomIdent& customIdent) -> CSS::CounterStyle {
            return { toCSS(customIdent, style) };
        },
        [&](const CounterStyle::SymbolsFunction& symbolsFunction) -> CSS::CounterStyle {
            auto symbols = symbolsFunction->symbols.map([](auto& symbol) { return CSS::String { symbol }; });
            return { CSS::CounterStyleSymbolsFunction { keywordFromSystem(symbolsFunction->system), WTF::move(symbols) } };
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
        [&](const CSS::CounterStyleSymbolsFunction& symbolsFunction) -> CounterStyle {
            auto system = symbolsFunction.system ? systemFromSymbolsTypeKeyword(symbolsFunction.system->value).value_or(CSSCounterStyleDescriptors::System::Symbolic) : CSSCounterStyleDescriptors::System::Symbolic;
            auto symbols = symbolsFunction.symbols.map([](auto& symbol) { return symbol.value; });
            return { CounterStyle::SymbolsFunction { CounterStyle::SymbolsParameters { system, WTF::move(symbols) } } };
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
    CSSValueListBuilder arguments;
    if (auto systemKeyword = keywordFromSystem(value->system))
        arguments.append(CSSKeywordValue::create(systemKeyword->value));
    for (auto& symbol : value->symbols)
        arguments.append(CSSStringValue::create(CSS::String { symbol }));
    return CSSFunctionValue::create(CSSValueSymbols, WTF::move(arguments), CSSValue::ValueSeparator::Space);
}

// MARK: - Serialization

void Serialize<CounterStyle::SymbolsFunction>::operator()(StringBuilder& builder, const CSS::SerializationContext&, const Style::ComputedStyle&, const CounterStyle::SymbolsFunction& value)
{
    builder.append("symbols("_s);
    bool needsSpace = false;
    if (auto systemKeyword = keywordFromSystem(value->system)) {
        builder.append(nameLiteralForSerialization(systemKeyword->value));
        needsSpace = true;
    }
    for (auto& symbol : value->symbols) {
        if (needsSpace)
            builder.append(' ');
        needsSpace = true;
        serializeString(builder, symbol);
    }
    builder.append(')');
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const CounterStyle::SymbolsFunction& value)
{
    ts << "symbols("_s;
    if (auto systemKeyword = keywordFromSystem(value->system))
        ts << nameLiteralForSerialization(systemKeyword->value) << ' ';
    for (auto& symbol : value->symbols)
        ts << symbol << ' ';
    ts << ')';
    return ts;
}

} // namespace Style
} // namespace WebCore
