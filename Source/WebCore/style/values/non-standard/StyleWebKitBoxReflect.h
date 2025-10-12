/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleLengthWrapper.h>
#include <WebCore/StyleMaskBorder.h>

namespace WebCore {
namespace Style {

struct WebkitBoxReflectionOffset : LengthWrapperBase<LengthPercentage<>> {
    using Base::Base;
};

struct WebkitBoxReflection {
    ReflectionDirection direction { ReflectionDirection::Below };
    WebkitBoxReflectionOffset offset;
    MaskBorder mask;

    bool operator==(const WebkitBoxReflection&) const = default;
};

template<size_t I> const auto& get(const WebkitBoxReflection& value)
{
    if constexpr (!I)
        return value.direction;
    else if constexpr (I == 1)
        return value.offset;
    else if constexpr (I == 2)
        return value.mask;
}

// <'-webkit-box-reflect'> = none | [ [ above | below | left | right ] <length-percentage>? <border-image>? ]
// NOTE: There is no standard associated with this property.
struct WebkitBoxReflect {
    WebkitBoxReflect(CSS::Keyword::None)
    {
    }

    WebkitBoxReflect(WebkitBoxReflection&& reflection)
        : m_reflection { WTFMove(reflection) }
    {
    }

    bool isNone() const { return !m_reflection; }
    bool isReflection() const { return !!m_reflection; }
    std::optional<WebkitBoxReflection> tryReflection() const { return m_reflection; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });
        return visitor(*m_reflection);
    }

    bool operator==(const WebkitBoxReflect&) const = default;

private:
    std::optional<WebkitBoxReflection> m_reflection { };
};

// MARK: - Conversion

template<> struct CSSValueConversion<WebkitBoxReflect> { auto operator()(BuilderState&, const CSSValue&) -> WebkitBoxReflect; };
template<> struct CSSValueCreation<WebkitBoxReflection> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const WebkitBoxReflection&); };

// MARK: - Serialization

template<> struct Serialize<WebkitBoxReflection> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const WebkitBoxReflection&); };

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::WebkitBoxReflection, 3)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::WebkitBoxReflectionOffset);
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::WebkitBoxReflect);
