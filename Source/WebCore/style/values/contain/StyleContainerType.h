/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'container-type'> = normal | [ [ size | inline-size ] || scroll-state ]
// https://drafts.csswg.org/css-conditional-5/#container-type

// The combinable container-type flags. `normal` is intentionally not a value here:
// it is represented by the empty set (see ContainerType::isNormal()).
enum class ContainerTypeValue : uint8_t {
    Size,
    InlineSize,
    ScrollState,
};

using ContainerTypeValueEnumSet = SpaceSeparatedEnumSet<ContainerTypeValue>;

struct ContainerType {
    using EnumSet = ContainerTypeValueEnumSet;
    using value_type = ContainerTypeValueEnumSet::value_type;

    constexpr ContainerType(CSS::Keyword::Normal) : m_value { } { }
    constexpr ContainerType(CSS::Keyword::Size) : m_value { ContainerTypeValue::Size } { }
    constexpr ContainerType(CSS::Keyword::InlineSize) : m_value { ContainerTypeValue::InlineSize } { }
    constexpr ContainerType(CSS::Keyword::ScrollState) : m_value { ContainerTypeValue::ScrollState } { }
    constexpr ContainerType(EnumSet&& set) : m_value { WTF::move(set) } { }
    constexpr ContainerType(value_type value) : ContainerType { EnumSet { value } } { }
    constexpr ContainerType(std::initializer_list<value_type> initializerList) : ContainerType { EnumSet { initializerList } } { }

    constexpr bool isNormal() const { return m_value.isEmpty(); }
    constexpr bool hasSize() const { return m_value.contains(ContainerTypeValue::Size); }
    constexpr bool hasInlineSize() const { return m_value.contains(ContainerTypeValue::InlineSize); }
    constexpr bool hasScrollState() const { return m_value.contains(ContainerTypeValue::ScrollState); }
    constexpr bool hasSizeContainment() const { return hasSize() || hasInlineSize(); }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNormal())
            return visitor(CSS::Keyword::Normal { });
        return visitor(m_value);
    }

    constexpr bool operator==(const ContainerType&) const = default;

private:
    EnumSet m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<ContainerType> {
    auto operator()(BuilderState&, const CSSValue&) -> ContainerType;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ContainerType)
