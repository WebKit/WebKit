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

// <'text-emphasis-position'> = [ over | under ] && [ right | left ]?@(default=right)
// https://drafts.csswg.org/css-text-decor-4/#propdef-text-emphasis-position

enum class TextEmphasisPositionValue : uint8_t {
    Over,
    Under,
    Left,
    Right,
};

using TextEmphasisPositionValueEnumSet = SpaceSeparatedEnumSet<TextEmphasisPositionValue>;

// FIXME: This could be packed into 2 bits if we didn't use an EnumSet.
struct TextEmphasisPosition {
    using EnumSet = TextEmphasisPositionValueEnumSet;
    using value_type = TextEmphasisPositionValueEnumSet::value_type;

    constexpr TextEmphasisPosition(EnumSet&& set) : m_value { WTF::move(set) } { }
    constexpr TextEmphasisPosition(value_type value) : TextEmphasisPosition { EnumSet { value } } { }
    constexpr TextEmphasisPosition(std::initializer_list<value_type> initializerList) : TextEmphasisPosition { EnumSet { initializerList } } { }

    static constexpr TextEmphasisPosition fromRaw(EnumSet::StorageType rawValue) { return EnumSet::fromRaw(rawValue); }
    constexpr EnumSet::StorageType toRaw() const { return m_value.toRaw(); }

    constexpr bool contains(TextEmphasisPositionValue e) const { return m_value.contains(e); }
    constexpr bool containsAny(EnumSet other) const { return m_value.containsAny(other.value); }
    constexpr bool containsAll(EnumSet other) const { return m_value.containsAll(other.value); }
    constexpr bool containsOnly(EnumSet other) const { return m_value.containsOnly(other.value); }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (contains(TextEmphasisPositionValue::Over)) {
            if (contains(TextEmphasisPositionValue::Left))
                return visitor(SpaceSeparatedTuple { CSS::Keyword::Over { }, CSS::Keyword::Left { } });
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Over { } });
        } else {
            if (contains(TextEmphasisPositionValue::Left))
                return visitor(SpaceSeparatedTuple { CSS::Keyword::Under { }, CSS::Keyword::Left { } });
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Under { } });
        }
    }

    constexpr bool operator==(const TextEmphasisPosition&) const = default;

private:
    EnumSet m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<TextEmphasisPosition> { auto operator()(BuilderState&, const CSSValue&) -> TextEmphasisPosition; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextEmphasisPosition)
