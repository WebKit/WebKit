/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {
using TextDecorationInsetPair = MinimallySerializingSpaceSeparatedPair<Length<>>;
}
}

// text-decoration-inset stores 'auto' or a length pair, so ValueOrKeyword needs a Markable
// representation of the pair; the pair is empty exactly when its first length is empty.
namespace WTF {
template<> struct MarkableTraits<WebCore::Style::TextDecorationInsetPair> {
    using Pair = WebCore::Style::TextDecorationInsetPair;
    using Length = WebCore::Style::Length<>;
    static bool isEmptyValue(const Pair& value) { return MarkableTraits<Length>::isEmptyValue(value.first()); }
    static Pair emptyValue() { return { MarkableTraits<Length>::emptyValue(), MarkableTraits<Length>::emptyValue() }; }
};
}

namespace WebCore {
namespace Style {

// <'text-decoration-inset'> = auto | <length>{1,2}
// The first value applies to the start endpoint, the second to the end endpoint of the line
// decorations; a single value applies to both. Positive values move an endpoint inward (trimming
// the decoration), negative values move it outward (extending it). 'auto' lets the UA choose an
// inset so that adjacent identical underlined elements do not appear to share a single continuous
// underline. Note that 'auto' is distinct from '0'.
// https://drafts.csswg.org/css-text-decor-4/#text-decoration-inset-property
struct TextDecorationInset : ValueOrKeyword<TextDecorationInsetPair, CSS::Keyword::Auto> {
    using Base::Base;

    TextDecorationInset(CSS::ValueLiteral<CSS::LengthUnit::Px> literal)
        : Base(Value { Length<> { literal }, Length<> { literal } })
    {
    }

    bool isAuto() const { return isKeyword(); }

    // Resolves the start/end inset to used CSS pixels. For 'auto', returns the UA-chosen autoValue
    // supplied by the caller (which depends on the decorating box's font).
    float resolvedStart(const Style::ComputedStyle&, float autoValue) const;
    float resolvedEnd(const Style::ComputedStyle&, float autoValue) const;
};

// MARK: - Conversion

// Conversion of the 'auto' keyword is handled generically (CSSValueConversion<ValueOrKeywordDerived>);
// only the length-pair value needs the usual pair unpacking.
template<> struct CSSValueConversion<TextDecorationInsetPair> {
    auto operator()(BuilderState&, const CSSValue&) -> TextDecorationInsetPair;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextDecorationInset)
