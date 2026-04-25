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

// <'contain'> = none | strict@(aliased-to=[size layout paint style]) | content@(aliased-to=[layout paint style]) | [ [size | inline-size] || layout || style || paint ]
// https://drafts.csswg.org/css-contain-2/#contain-property

enum class ContainValue : uint8_t {
    Size,
    InlineSize,
    Layout,
    Style,
    Paint,
};

using ContainValueEnumSet = SpaceSeparatedEnumSet<ContainValue>;

struct Contain {
    using EnumSet = ContainValueEnumSet;
    using value_type = ContainValueEnumSet::value_type;

    static constexpr ContainValueEnumSet strict { ContainValue::Size, ContainValue::Layout, ContainValue::Style, ContainValue::Paint };
    static constexpr ContainValueEnumSet content { ContainValue::Layout, ContainValue::Style, ContainValue::Paint };

    constexpr Contain(CSS::Keyword::None) : m_value { } { }
    constexpr Contain(CSS::Keyword::Strict) : m_value { strict } { }
    constexpr Contain(CSS::Keyword::Content) : m_value { content } { }
    constexpr Contain(EnumSet&& set) : m_value { WTF::move(set) } { }
    constexpr Contain(value_type value) : Contain { EnumSet { value } } { }
    constexpr Contain(std::initializer_list<value_type> initializerList) : Contain { EnumSet { initializerList } } { }

    static constexpr Contain fromRaw(EnumSet::StorageType rawValue)
    {
        if (!rawValue)
            return CSS::Keyword::None { };
        return EnumSet::fromRaw(rawValue);
    }
    constexpr EnumSet::StorageType toRaw() const { return m_value.toRaw(); }

    constexpr bool contains(value_type e) const { return m_value.contains(e); }
    constexpr bool containsAny(EnumSet other) const { return m_value.containsAny(other.value); }
    constexpr bool containsAll(EnumSet other) const { return m_value.containsAll(other.value); }
    constexpr bool containsOnly(EnumSet other) const { return m_value.containsOnly(other.value); }
    constexpr void add(EnumSet other) { m_value.value.add(other.value); }

    constexpr bool isNone() const { return m_value.isEmpty(); }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });

        // Handle "strict" and "content" shorthands
        if (m_value == strict)
            return visitor(CSS::Keyword::Strict { });
        if (m_value == content)
            return visitor(CSS::Keyword::Content { });

        return visitor(m_value);
    }

    constexpr bool operator==(const Contain&) const = default;

private:
    EnumSet m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<Contain> { auto operator()(BuilderState&, const CSSValue&) -> Contain; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Contain)
