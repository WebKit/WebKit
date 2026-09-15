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

#pragma once

#include <WebCore/CSSCounterStyleDescriptors.h>
#include <WebCore/CSSValueKeywords.h>
#include <WebCore/StyleCustomIdent.h>
#include <WebCore/StyleValueTypes.h>
#include <wtf/Variant.h>

namespace WebCore {

class CSSRegisteredCounterStyle;

namespace CSS {
struct CounterStyle;
}

namespace Style {

// <counter-style> = <custom-ident excluding=none> | <symbols()>
// https://drafts.csswg.org/css-counter-styles-3/#typedef-counter-style
struct CounterStyle {
    // symbols() = symbols( <symbols-type>? [ <string> | <image> ]+ )
    // https://drafts.csswg.org/css-counter-styles-3/#funcdef-symbols
    struct SymbolsParameters {
        CSSCounterStyleDescriptors::System system { CSSCounterStyleDescriptors::System::Symbolic };
        SpaceSeparatedVector<WTF::String> symbols;

        bool operator==(const SymbolsParameters&) const = default;
    };
    using SymbolsFunction = FunctionNotation<CSSValueSymbols, SymbolsParameters>;

    CounterStyle(CustomIdent&& identifier)
        : m_value { WTF::move(identifier) }
    {
    }

    CounterStyle(SymbolsFunction&& symbolsFunction)
        : m_value { WTF::move(symbolsFunction) }
    {
    }

    bool isSymbolsFunction() const { return std::holds_alternative<SymbolsFunction>(m_value); }

    std::optional<CustomIdent> tryName() const
    {
        if (auto* identifier = std::get_if<CustomIdent>(&m_value))
            return *identifier;
        return std::nullopt;
    }

    std::optional<SymbolsFunction> trySymbolsFunction() const
    {
        if (auto* symbolsFunction = std::get_if<SymbolsFunction>(&m_value))
            return *symbolsFunction;
        return std::nullopt;
    }

    // Builds the anonymous runtime counter style used to render the `symbols()` function. Declared
    // here, but defined out-of-line since `CSSRegisteredCounterStyle` is only forward-declared here.
    WEBCORE_EXPORT RefPtr<CSSRegisteredCounterStyle> trySymbolsFunctionCounterStyle() const;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const CounterStyle&) const = default;
    bool operator==(const CustomIdent& other) const { auto* identifier = std::get_if<CustomIdent>(&m_value); return identifier && *identifier == other; }
    bool operator==(const AtomString& other) const { auto* identifier = std::get_if<CustomIdent>(&m_value); return identifier && identifier->value == other; }
    bool operator==(CSSValueID other) const { auto* identifier = std::get_if<CustomIdent>(&m_value); return identifier && identifier->value == nameString(other); }

private:
    Variant<CustomIdent, SymbolsFunction> m_value;
};

template<size_t I> const auto& get(const CounterStyle::SymbolsParameters& value)
{
    if constexpr (!I)
        return value.system;
    else
        return value.symbols;
}

// MARK: - Conversion

template<> struct ToCSS<CounterStyle> { auto operator()(const CounterStyle&, const Style::ComputedStyle&) -> CSS::CounterStyle; };
template<> struct ToStyle<CSS::CounterStyle> { auto operator()(const CSS::CounterStyle&, const BuilderState&) -> CounterStyle; };
template<> struct CSSValueConversion<CounterStyle> { auto operator()(BuilderState&, const CSSValue&) -> CounterStyle; };
template<> struct CSSValueCreation<CounterStyle::SymbolsFunction> { Ref<CSSValue> operator()(CSSValuePool&, const Style::ComputedStyle&, const CounterStyle::SymbolsFunction&); };

// MARK: - Serialization

template<> struct Serialize<CounterStyle::SymbolsFunction> { void operator()(StringBuilder&, const CSS::SerializationContext&, const Style::ComputedStyle&, const CounterStyle::SymbolsFunction&); };

// MARK: - Logging

TextStream& operator<<(TextStream&, const CounterStyle::SymbolsFunction&);

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::CounterStyle)
DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::CounterStyle::SymbolsParameters, 2)
