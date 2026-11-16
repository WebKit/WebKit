/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSBorderImageOutset.h"
#include "CSSBorderImageRepeat.h"
#include "CSSBorderImageSlice.h"
#include "CSSBorderImageSource.h"
#include "CSSBorderImageWidth.h"

namespace WebCore {
namespace CSS {

// <'border-image'> = <'border-image-source'> || <'border-image-slice'> [ / <'border-image-width'> | / <'border-image-width'>? / <'border-image-outset'> ]? || <'border-image-repeat'>
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image
struct BorderImage {
    std::optional<BorderImageSource> borderImageSource;
    std::optional<BorderImageSlice> borderImageSlice;
    std::optional<BorderImageWidth> borderImageWidth;
    std::optional<BorderImageOutset> borderImageOutset;
    std::optional<BorderImageRepeat> borderImageRepeat;

    // Alias accessors for using in generic contexts with `MaskBorder`.
    const std::optional<BorderImageSource>& source() const { return borderImageSource; }
    const std::optional<BorderImageSlice>& slice() const { return borderImageSlice; }
    const std::optional<BorderImageWidth>& width() const { return borderImageWidth; }
    const std::optional<BorderImageOutset>& outset() const { return borderImageOutset; }
    const std::optional<BorderImageRepeat>& repeat() const { return borderImageRepeat; }

    bool operator==(const BorderImage&) const = default;
};
template<size_t I> const auto& get(const BorderImage& value)
{
    if constexpr (!I)
        return value.borderImageSource;
    else if constexpr (I == 1)
        return value.borderImageSlice;
    else if constexpr (I == 2)
        return value.borderImageWidth;
    else if constexpr (I == 3)
        return value.borderImageOutset;
    else if constexpr (I == 4)
        return value.borderImageRepeat;
}

// MARK: - Conversion

template<> struct CSSValueCreation<BorderImage> { auto operator()(CSSValuePool&, const BorderImage&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<BorderImage> { void operator()(StringBuilder&, const SerializationContext&, const BorderImage&); };

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::BorderImage, 5)
