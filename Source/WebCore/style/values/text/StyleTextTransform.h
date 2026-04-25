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

#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'text-transform'> = none | [ capitalize | uppercase | lowercase ] || full-width || full-size-kana | math-auto
// https://drafts.csswg.org/css-text/#propdef-text-transform
// Additional value `math-auto` added by MathML
// https://w3c.github.io/mathml-core/#math-auto-transform

enum class TextTransformValue : uint8_t {
    Capitalize,
    Uppercase,
    Lowercase,
    FullWidth,
    FullSizeKana,
    MathAuto,
};
constexpr auto maxTextTransformValue = TextTransformValue::MathAuto;

using TextTransformValueEnumSet = SpaceSeparatedEnumSet<TextTransformValue>;

// FIXME: This could be packed in to 5 bits if we didn't use an EnumSet.
struct TextTransform {
    using EnumSet = TextTransformValueEnumSet;
    using value_type = TextTransformValueEnumSet::value_type;

    constexpr TextTransform(CSS::Keyword::None) : m_value { } { }
    constexpr TextTransform(EnumSet&& set) : m_value { WTF::move(set) } { }
    constexpr TextTransform(value_type value) : TextTransform { EnumSet { value } } { }
    constexpr TextTransform(std::initializer_list<value_type> initializerList) : TextTransform { EnumSet { initializerList } } { }

    static constexpr TextTransform fromRaw(EnumSet::StorageType rawValue) { return EnumSet::fromRaw(rawValue); }
    constexpr EnumSet::StorageType toRaw() const { return m_value.toRaw(); }

    constexpr bool contains(TextTransformValue e) const { return m_value.contains(e); }
    constexpr bool containsAny(EnumSet other) const { return m_value.containsAny(other.value); }
    constexpr bool containsAll(EnumSet other) const { return m_value.containsAll(other.value); }
    constexpr bool containsOnly(EnumSet other) const { return m_value.containsOnly(other.value); }

    constexpr bool isNone() const { return m_value.isEmpty(); }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });

        return visitor(m_value);
    }

    constexpr bool operator==(const TextTransform&) const = default;

private:
    EnumSet m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<TextTransform> { auto operator()(BuilderState&, const CSSValue&) -> TextTransform; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextTransform)
