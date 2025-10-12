/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/CSSURL.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {

class ScriptExecutionContext;

namespace Style {

struct URL {
    WTF::URL resolved;
    CSS::URLModifiers modifiers;

    bool operator==(const URL&) const = default;
};

template<size_t I> const auto& get(const URL& value)
{
    if constexpr (!I)
        return value.resolved;
    if constexpr (I == 1)
        return value.modifiers;
}

// MARK: Conversion

// Special conversion function for use by filters and font-face code.
URL toStyleWithScriptExecutionContext(const CSS::URL&, const ScriptExecutionContext&);

template<> struct ToCSS<URL> { auto operator()(const URL&, const RenderStyle&) -> CSS::URL; };
template<> struct ToStyle<CSS::URL> { auto operator()(const CSS::URL&, const BuilderState&) -> URL; };

// `URL` is special-cased to return a `CSSURLValue`.
template<> struct CSSValueCreation<URL> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const URL&); };
template<> struct CSSValueConversion<URL> { auto operator()(BuilderState&, const CSSValue&) -> URL; };

// MARK: Serialization

template<> struct Serialize<URL> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const URL&); };

// MARK: Logging

TextStream& operator<<(TextStream&, const URL&);

} // namespace Style
} // namespace WebCore

namespace WTF {

template<>
struct MarkableTraits<WebCore::Style::URL> {
    static bool isEmptyValue(const WebCore::Style::URL& value) { return value.resolved.isNull(); }
    static WebCore::Style::URL emptyValue() { return { .resolved = { }, .modifiers = { } }; }
};

} // namespace WTF

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::URL, 2)
