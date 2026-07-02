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

// <'margin-trim'> = none | [ block || inline ] | [ block-start || inline-start || block-end || inline-end ]
// https://drafts.csswg.org/css-box/#margin-trim

enum class MarginTrimSide : uint8_t {
    BlockStart,
    InlineStart,
    BlockEnd,
    InlineEnd,
};

using MarginTrimSideEnumSet = SpaceSeparatedEnumSet<MarginTrimSide>;

struct MarginTrim {
    using EnumSet = MarginTrimSideEnumSet;
    using value_type = MarginTrimSideEnumSet::value_type;

    constexpr MarginTrim(EnumSet&& set) : m_value { WTF::move(set) } { }
    constexpr MarginTrim(CSS::Keyword::None) : m_value { } { }
    constexpr MarginTrim(value_type value) : MarginTrim { EnumSet { value } } { }
    constexpr MarginTrim(std::initializer_list<value_type> initializerList) : MarginTrim { EnumSet { initializerList } } { }

    static constexpr MarginTrim fromRaw(EnumSet::StorageType rawValue)
    {
        if (!rawValue)
            return CSS::Keyword::None { };
        return EnumSet::fromRaw(rawValue);
    }
    constexpr EnumSet::StorageType toRaw() const { return m_value.toRaw(); }

    constexpr bool contains(MarginTrimSide e) const { return m_value.contains(e); }
    constexpr bool containsAny(EnumSet other) const { return m_value.containsAny(other.value); }
    constexpr bool containsAll(EnumSet other) const { return m_value.containsAll(other.value); }
    constexpr bool containsOnly(EnumSet other) const { return m_value.containsOnly(other.value); }

    constexpr bool isNone() const { return m_value.isEmpty(); }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });

        // Handle "block", "inline" and "block inline" shorthands
        if (m_value.containsAll({ MarginTrimSide::BlockStart, MarginTrimSide::BlockEnd }) && !m_value.containsAny({ MarginTrimSide::InlineStart, MarginTrimSide::InlineEnd }))
            return visitor(CSS::Keyword::Block { });
        if (m_value.containsAll({ MarginTrimSide::InlineStart, MarginTrimSide::InlineEnd }) && !m_value.containsAny({ MarginTrimSide::BlockStart, MarginTrimSide::BlockEnd }))
            return visitor(CSS::Keyword::Inline { });
        if (m_value.containsAll({ MarginTrimSide::BlockStart, MarginTrimSide::BlockEnd, MarginTrimSide::InlineStart, MarginTrimSide::InlineEnd }))
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Block { }, CSS::Keyword::Inline { } });

        return visitor(m_value);
    }

    constexpr bool operator==(const MarginTrim&) const = default;

private:
    EnumSet m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<MarginTrim> { auto operator()(BuilderState&, const CSSValue&) -> MarginTrim; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::MarginTrim)
