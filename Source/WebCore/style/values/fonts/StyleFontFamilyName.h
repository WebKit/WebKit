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
struct FontFamilyName;
}

class CSSFontFamilyNameValue;

namespace Style {

// <family-name> = <string> | <custom-ident>+
// NOTE: Per spec, "If a sequence of identifiers is given as a <family-name>, the computed value is the name converted to a string by joining all the identifiers in the sequence by single spaces."
// https://drafts.csswg.org/css-fonts-4/#family-name-syntax
struct FontFamilyName {
    AtomString value;

    bool operator==(const FontFamilyName&) const = default;
};

// MARK: - Conversion

template<> struct ToCSS<FontFamilyName> { auto operator()(const FontFamilyName&, const RenderStyle&) -> CSS::FontFamilyName; };
template<> struct ToStyle<CSS::FontFamilyName> { auto operator()(const CSS::FontFamilyName&, const BuilderState&) -> FontFamilyName; };

template<> struct CSSValueCreation<FontFamilyName> {
    auto operator()(CSSValuePool&, const RenderStyle&, const FontFamilyName&) -> Ref<CSSValue>;
};
template<> struct CSSValueConversion<FontFamilyName> {
    auto operator()(BuilderState&, const CSSFontFamilyNameValue&) -> FontFamilyName;
    auto operator()(BuilderState&, const CSSValue&) -> FontFamilyName;
};

// MARK: - Serialization

template<> struct Serialize<FontFamilyName> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const FontFamilyName&); };

// MARK: - Logging

TextStream& operator<<(TextStream&, const FontFamilyName&);

} // namespace Style
} // namespace WebCore
