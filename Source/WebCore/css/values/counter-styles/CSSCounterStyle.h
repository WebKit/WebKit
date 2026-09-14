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
#include <wtf/Vector.h>

namespace WebCore {
namespace CSS {

// The anonymous counter style defined inline by the `symbols()` function.
// https://drafts.csswg.org/css-counter-styles-3/#funcdef-symbols
struct CounterStyleSymbolsFunction {
    // The `<symbols-type>` keyword, or `std::nullopt` for the default (`symbolic`).
    std::optional<Keyword> system;
    Vector<String> symbols;

    bool operator==(const CounterStyleSymbolsFunction&) const = default;
};

template<> struct Serialize<CounterStyleSymbolsFunction> { void operator()(StringBuilder&, const SerializationContext&, const CounterStyleSymbolsFunction&); };
template<> struct ComputedStyleDependenciesCollector<CounterStyleSymbolsFunction> { constexpr void operator()(ComputedStyleDependencies&, const CounterStyleSymbolsFunction&) { } };
template<> struct CSSValueChildrenVisitor<CounterStyleSymbolsFunction> { constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const CounterStyleSymbolsFunction&) { return IterationStatus::Continue; } };

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const CounterStyleSymbolsFunction&);

// MARK: - Hashing

void add(Hasher&, const CounterStyleSymbolsFunction&);

// <counter-style> = <custom-ident excluding=none> | <symbols()>
// https://drafts.csswg.org/css-counter-styles-3/#typedef-counter-style
struct CounterStyle {
    // Stores predefined style types using their Keyword representation.
    Variant<Keyword, CustomIdent, CounterStyleSymbolsFunction> identifier;

    bool operator==(const CounterStyle&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(CounterStyle, identifier);

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::CounterStyle)
