/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <WebCore/StyleColor.h>
#include <wtf/Markable.h>

namespace WebCore {
namespace Style {

// <scrollbar-color-parts> = <color>{2}
struct ScrollbarColorParts {
    Color thumb;
    Color track;

    bool operator==(const ScrollbarColorParts&) const = default;
};
template<size_t I> const auto& get(const ScrollbarColorParts& value)
{
    if constexpr (!I)
        return value.thumb;
    else if constexpr (I == 1)
        return value.track;
}

struct ScrollbarColorMarkableTraits {
    static bool isEmptyValue(const ScrollbarColorParts& value)
    {
        return WTF::MarkableTraits<Color>::isEmptyValue(value.thumb);
    }
    static ScrollbarColorParts emptyValue()
    {
        return { WTF::MarkableTraits<Color>::emptyValue(), WTF::MarkableTraits<Color>::emptyValue() };
    }
};

// <'scrollbar-color'> = auto | <color>{2}
// https://www.w3.org/TR/css-scrollbars/#propdef-scrollbar-color
struct ScrollbarColor : ValueOrKeyword<ScrollbarColorParts, CSS::Keyword::Auto, ScrollbarColorMarkableTraits> {
    using Base::Base;
    using Parts = typename Base::Value;

    bool isAuto() const { return isKeyword(); }
    bool isParts() const { return isValue(); }
};

// MARK: - Conversion

template<> struct CSSValueConversion<ScrollbarColor> { auto operator()(BuilderState&, const CSSValue&) -> ScrollbarColor; };

// MARK: - Blending

template<> struct Blending<ScrollbarColor> {
    auto equals(const ScrollbarColor&, const ScrollbarColor&, const RenderStyle&, const RenderStyle&) -> bool;
    auto canBlend(const ScrollbarColor&, const ScrollbarColor&) -> bool;
    auto blend(const ScrollbarColor&, const ScrollbarColor&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> ScrollbarColor;
};

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::ScrollbarColorParts, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ScrollbarColor)
