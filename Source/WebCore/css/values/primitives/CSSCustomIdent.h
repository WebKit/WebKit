/*
 * Copyright (C) 2026 saku
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

#include <WebCore/CSSPrimitiveNumericTypes.h>
#include <WebCore/CSSString.h>
#include <WebCore/CSSValueTypes.h>
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace CSS {

// Non-keyword <ident> argument to the ident() function, contributing its codepoints to the resulting identifier.
struct IdentFunctionIdent {
    AtomString value;

    bool operator==(const IdentFunctionIdent&) const = default;
};

template<> struct Serialize<IdentFunctionIdent> { void operator()(StringBuilder&, const SerializationContext&, const IdentFunctionIdent&); };
template<> struct ComputedStyleDependenciesCollector<IdentFunctionIdent> { constexpr void operator()(ComputedStyleDependencies&, const IdentFunctionIdent&) { } };
template<> struct CSSValueChildrenVisitor<IdentFunctionIdent> { constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const IdentFunctionIdent&) { return IterationStatus::Continue; } };

// <ident-arg> = <string> | <integer> | <ident>
using IdentFunctionArg = Variant<IdentFunctionIdent, String, Integer<>>;

// <ident()> = ident( <ident-arg>+ )
// https://drafts.csswg.org/css-values-5/#ident
using IdentFunction = FunctionNotation<CSSValueIdent, SpaceSeparatedVector<IdentFunctionArg>>;

// https://drafts.csswg.org/css-values-4/#identifier-value
struct CustomIdent {
    Variant<AtomString, IdentFunction> value;

    bool isResolved() const { return WTF::holdsAlternative<AtomString>(value); }
    bool isNull() const;

    // Prefix test for parse-time validity checks, treating every <integer> component as "0".
    // See https://github.com/w3c/csswg-drafts/issues/12206#issuecomment-3998743769.
    bool startsWith(StringView prefix) const;

    bool NODELETE operator==(const CustomIdent&) const;
};

template<> struct Serialize<CustomIdent> { void operator()(StringBuilder&, const SerializationContext&, const CustomIdent&); };
template<> struct ComputedStyleDependenciesCollector<CustomIdent> { void operator()(ComputedStyleDependencies&, const CustomIdent&); };
template<> struct CSSValueChildrenVisitor<CustomIdent> { IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const CustomIdent&); };
template<> struct CSSValueCreation<CustomIdent> { Ref<CSSValue> operator()(CSSValuePool&, const CustomIdent&); };
template<> struct DeprecatedCSSOMValueCreation<CustomIdent> { Ref<DeprecatedCSSOMValue> operator()(CSSValuePool&, CSSStyleDeclaration&, const CustomIdent&); };

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const CustomIdent&);

// MARK: - Hashing

void NODELETE add(Hasher&, const CustomIdent&);

} // namespace CSS
} // namespace WebCore
