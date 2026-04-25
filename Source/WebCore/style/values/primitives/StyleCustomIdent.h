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

#include <WebCore/StyleValueTypes.h>

namespace WebCore {

namespace CSS {
struct CustomIdent;
}

class CSSCustomIdentValue;

namespace Style {

struct CustomIdent {
    AtomString value;

    bool operator==(const CustomIdent&) const = default;
    bool operator==(const AtomString& other) const { return value == other; }
};

// MARK: - Conversion

template<> struct ToCSS<CustomIdent> { auto operator()(const CustomIdent&, const RenderStyle&) -> CSS::CustomIdent; };
template<> struct ToStyle<CSS::CustomIdent> { auto operator()(const CSS::CustomIdent&, const BuilderState&) -> CustomIdent; };

template<> struct CSSValueCreation<CustomIdent> {
    Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const CustomIdent&);
};
template<> struct CSSValueConversion<CustomIdent> {
    auto operator()(BuilderState&, const CSSCustomIdentValue&) -> CustomIdent;
    auto operator()(BuilderState&, const CSSValue&) -> CustomIdent;
};

// MARK: - Serialization

template<> struct Serialize<CustomIdent> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const CustomIdent&); };

// MARK: - Logging

TextStream& operator<<(TextStream&, const CustomIdent&);

// MARK: - Hashing

void NODELETE add(Hasher&, const CustomIdent&);

} // namespace Style
} // namespace WebCore

namespace WTF {

template<>
struct MarkableTraits<WebCore::Style::CustomIdent> {
    static bool isEmptyValue(const WebCore::Style::CustomIdent& value) { return value.value.isNull(); }
    static WebCore::Style::CustomIdent emptyValue() { return WebCore::Style::CustomIdent { nullAtom() }; }
};

template<>
struct HashTraits<WebCore::Style::CustomIdent> : GenericHashTraits<WebCore::Style::CustomIdent> {
    using EmptyValueType = WebCore::Style::CustomIdent;
    static constexpr bool emptyValueIsZero = true;
    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const WebCore::Style::CustomIdent& value) { return value.value.isNull(); }
    static EmptyValueType emptyValue() { return WebCore::Style::CustomIdent { nullAtom() }; }

    static void constructDeletedValue(WebCore::Style::CustomIdent& value) { new (NotNull, std::addressof(value.value)) AtomString { HashTableDeletedValue }; }
    static bool isDeletedValue(const WebCore::Style::CustomIdent& value) { return value.value.isHashTableDeletedValue(); }
};

} // namespace WTF
