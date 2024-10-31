/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

// MARK: - Symbol replacement

// Replaces the symbol with a value from the symbol table. This is only relevant
// for Symbol, so a catchall overload that implements the identity function is
// provided to allow generic replacement.
Number<> replaceSymbol(Symbol, const CSSCalcSymbolTable&);

template<typename T> constexpr auto replaceSymbol(T value, const CSSCalcSymbolTable&) -> T
{
    return value;
}

template<typename... Ts> using TypesMinusSymbol = VariantOrSingle<TypeTransform::List::MinusSymbol<brigand::list<Ts...>>>;

template<typename... Ts> constexpr auto replaceSymbol(const std::variant<Ts...>& component, const CSSCalcSymbolTable& symbolTable) -> TypesMinusSymbol<Ts...>
{
    return WTF::switchOn(component, [&](auto part) -> TypesMinusSymbol<Ts...> { return replaceSymbol(part, symbolTable); });
}

template<typename T> constexpr decltype(auto) replaceSymbol(const std::optional<T>& component, const CSSCalcSymbolTable& symbolTable)
{
    return component ? std::make_optional(replaceSymbol(*component, symbolTable)) : std::nullopt;
}

} // namespace CSS
} // namespace WebCore
