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

#include <WebCore/StyleValueTypes.h>

namespace WebCore {

namespace CSS {
struct String;
}

class CSSStringValue;

namespace Style {

struct String {
    WTF::String value;

    bool operator==(const String&) const = default;
    bool operator==(const WTF::String& other) const { return value == other; }
};

// MARK: - Conversion

template<> struct ToCSS<String> { auto operator()(const String&, const RenderStyle&) -> CSS::String; };
template<> struct ToStyle<CSS::String> { auto operator()(const CSS::String&, const BuilderState&) -> String; };

template<> struct CSSValueCreation<String> {
    Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const String&);
};
template<> struct CSSValueConversion<String> {
    auto operator()(BuilderState&, const CSSStringValue&) -> String;
    auto operator()(BuilderState&, const CSSValue&) -> String;
};

// MARK: - Serialization

template<> struct Serialize<String> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const String&); };

// MARK: - Logging

TextStream& operator<<(TextStream&, const String&);

// MARK: - Hashing

void NODELETE add(Hasher&, const String&);

} // namespace Style
} // namespace WebCore

namespace WTF {

template<>
struct MarkableTraits<WebCore::Style::String> {
    static bool isEmptyValue(const WebCore::Style::String& value) { return value.value.isNull(); }
    static WebCore::Style::String emptyValue() { return WebCore::Style::String { nullString() }; }
};

template<>
struct HashTraits<WebCore::Style::String> : GenericHashTraits<WebCore::Style::String> {
    using EmptyValueType = WebCore::Style::String;
    static constexpr bool emptyValueIsZero = true;
    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const WebCore::Style::String& value) { return value.value.isNull(); }
    static EmptyValueType emptyValue() { return WebCore::Style::String { nullString() }; }

    static void constructDeletedValue(WebCore::Style::String& value) { new (NotNull, std::addressof(value.value)) String { HashTableDeletedValue }; }
    static bool isDeletedValue(const WebCore::Style::String& value) { return value.value.isHashTableDeletedValue(); }
};

} // namespace WTF
