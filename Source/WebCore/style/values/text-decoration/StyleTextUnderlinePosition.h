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

// <'text-underline-position'> = auto | [ from-font | under ] || [ left | right ]
// https://drafts.csswg.org/css-text-decor-4/#propdef-text-underline-position

enum class TextUnderlinePositionValue : uint8_t {
    FromFont,
    Under,
    Left,
    Right,
};

using TextUnderlinePositionValueEnumSet = SpaceSeparatedEnumSet<TextUnderlinePositionValue>;

struct TextUnderlinePosition {
    using EnumSet = TextUnderlinePositionValueEnumSet;
    using value_type = TextUnderlinePositionValueEnumSet::value_type;

    enum class Side : uint8_t { NoPreference, Left, Right };

    constexpr TextUnderlinePosition(CSS::Keyword::Auto) : m_value { } { }
    constexpr TextUnderlinePosition(EnumSet&& set) : m_value { WTF::move(set) } { }
    constexpr TextUnderlinePosition(value_type value) : TextUnderlinePosition { EnumSet { value } } { }
    constexpr TextUnderlinePosition(std::initializer_list<value_type> initializerList) : TextUnderlinePosition { EnumSet { initializerList } } { }

    static constexpr TextUnderlinePosition fromRaw(EnumSet::StorageType rawValue) { return EnumSet::fromRaw(rawValue); }
    constexpr EnumSet::StorageType toRaw() const { return m_value.toRaw(); }

    constexpr bool contains(TextUnderlinePositionValue e) const { return m_value.contains(e); }
    constexpr bool containsAny(EnumSet other) const { return m_value.containsAny(other.value); }
    constexpr bool containsAll(EnumSet other) const { return m_value.containsAll(other.value); }
    constexpr bool containsOnly(EnumSet other) const { return m_value.containsOnly(other.value); }

    constexpr bool isAuto() const { return m_value.isEmpty(); }
    constexpr bool isFromFont() const { return contains(TextUnderlinePositionValue::FromFont); }
    constexpr bool isUnder() const { return contains(TextUnderlinePositionValue::Under); }

    constexpr Side verticalTypographySide() const
    {
        if (contains(TextUnderlinePositionValue::Left))
            return Side::Left;
        if (contains(TextUnderlinePositionValue::Right))
            return Side::Right;
        return Side::NoPreference;
    }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isAuto())
            return visitor(CSS::Keyword::Auto { });
        return visitor(m_value);
    }

    constexpr bool operator==(const TextUnderlinePosition&) const = default;

private:
    EnumSet m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<TextUnderlinePosition> { auto operator()(BuilderState&, const CSSValue&) -> TextUnderlinePosition; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextUnderlinePosition)
