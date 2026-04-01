/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2026 saku <saku@email.sakupi01.com>
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

#include <wtf/Compiler.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/IterationStatus.h>
#include <wtf/Markable.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class CSSValue;
struct ComputedStyleDependencies;

namespace CSS {

struct SerializationContext;

// Helper type used to represent an arbitrary constant identifier.
struct CustomIdent {
    AtomString value;

    bool operator==(const CustomIdent&) const = default;
    bool operator==(const AtomString& other) const { return value == other; }
};

// MARK: - Serialization

void serializationForCSSCustomIdentifier(StringBuilder&, const SerializationContext&, const CustomIdent&);

template<typename CSSType> struct Serialize;

// Specialization for `CustomIdent`.
template<> struct Serialize<CustomIdent> {
    template<typename... Rest> void operator()(StringBuilder& builder, const SerializationContext& context, const CustomIdent& value, Rest&&...)
    {
        serializationForCSSCustomIdentifier(builder, context, value);
    }
};

// MARK: - Computed Style Dependencies

template<typename CSSType> struct ComputedStyleDependenciesCollector;

// Specialization for `CustomIdent`.
template<> struct ComputedStyleDependenciesCollector<CustomIdent> {
    constexpr void operator()(ComputedStyleDependencies&, const CustomIdent&)
    {
        // Nothing to do.
    }
};

// MARK: - CSSValue Visitation

template<typename CSSType> struct CSSValueChildrenVisitor;

// Specialization for `CustomIdent`.
template<> struct CSSValueChildrenVisitor<CustomIdent> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const CustomIdent&)
    {
        return IterationStatus::Continue;
    }
};

// MARK: - CSSValue Creation

Ref<CSSValue> makePrimitiveCSSValue(const CustomIdent&);

template<typename CSSType> struct CSSValueCreation;

// Specialization for `CustomIdent`.
template<> struct CSSValueCreation<CustomIdent> {
    template<typename CSSValuePool, typename... Rest> Ref<CSSValue> operator()(CSSValuePool&, const CustomIdent& customIdentifier, Rest&&...)
    {
        return makePrimitiveCSSValue(customIdentifier);
    }
};

} // namespace CSS

// TODO: Transitional alias while CSS/Style custom-ident representation is being split.
using CustomIdentifier = CSS::CustomIdent;

WTF::TextStream& operator<<(WTF::TextStream&, const CSS::CustomIdent&);
void NODELETE add(Hasher&, const CSS::CustomIdent&);

} // namespace WebCore

namespace WTF {

template<>
struct MarkableTraits<WebCore::CSS::CustomIdent> {
    static bool isEmptyValue(const WebCore::CSS::CustomIdent& value) { return value.value.isNull(); }
    static WebCore::CSS::CustomIdent emptyValue() { return WebCore::CSS::CustomIdent { nullAtom() }; }
};

} // namespace WTF
