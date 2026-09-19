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

#include <WebCore/CSSCustomIdent.h>
#include <WebCore/CSSKeyword.h>
#include <WebCore/CSSString.h>
#include <WebCore/CSSValueTypes.h>

namespace WebCore {
namespace CSS {

// <symbols-type> = cyclic | numeric | alphabetic | symbolic | fixed
// https://drafts.csswg.org/css-counter-styles-3/#typedef-symbols-type
using SymbolsType = Variant<Keyword::Cyclic, Keyword::Numeric, Keyword::Alphabetic, Keyword::Symbolic, Keyword::Fixed>;

// symbols() = symbols( <symbols-type>? [ <string> | <image> ]+ )
// https://drafts.csswg.org/css-counter-styles-3/#funcdef-symbols
// FIXME: Add support for <image> symbols.
struct SymbolsParameters {
    // The `<symbols-type>` keyword, or `std::nullopt` for the default (`symbolic`).
    std::optional<SymbolsType> system;
    SpaceSeparatedVector<String> symbols;

    bool operator==(const SymbolsParameters&) const = default;
};
using SymbolsFunction = FunctionNotation<CSSValueSymbols, SymbolsParameters>;

template<size_t I> const auto& get(const SymbolsParameters& value)
{
    if constexpr (!I)
        return value.system;
    else
        return value.symbols;
}

// <counter-style> = <custom-ident excluding=none> | <symbols()>
// https://drafts.csswg.org/css-counter-styles-3/#typedef-counter-style
struct CounterStyle {
    // Stores predefined style types using their Keyword representation.
    Variant<Keyword, CustomIdent, SymbolsFunction> identifier;

    bool operator==(const CounterStyle&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(CounterStyle, identifier);

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::CounterStyle)
DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::SymbolsParameters, 2)
