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

#include "CSSMaskBorderOutset.h"
#include "CSSMaskBorderRepeat.h"
#include "CSSMaskBorderSlice.h"
#include "CSSMaskBorderSource.h"
#include "CSSMaskBorderWidth.h"

namespace WebCore {
namespace CSS {

// <'mask-border'> = <'mask-border-source'> || <'mask-border-slice'> [ / <'mask-border-width'>? [ / <'mask-border-outset'> ]? ]? || <'mask-border-repeat'> || <'mask-border-mode'>
// FIXME: Add support for `mask-border-mode`.
// https://drafts.csswg.org/css-masking-1/#propdef-mask-border
struct MaskBorder {
    std::optional<MaskBorderSource> maskBorderSource;
    std::optional<MaskBorderSlice> maskBorderSlice;
    std::optional<MaskBorderWidth> maskBorderWidth;
    std::optional<MaskBorderOutset> maskBorderOutset;
    std::optional<MaskBorderRepeat> maskBorderRepeat;

    // Alias accessors for using in generic contexts with `BorderImage`.
    const std::optional<MaskBorderSource>& source() const { return maskBorderSource; }
    const std::optional<MaskBorderSlice>& slice() const { return maskBorderSlice; }
    const std::optional<MaskBorderWidth>& width() const { return maskBorderWidth; }
    const std::optional<MaskBorderOutset>& outset() const { return maskBorderOutset; }
    const std::optional<MaskBorderRepeat>& repeat() const { return maskBorderRepeat; }

    bool operator==(const MaskBorder&) const = default;
};
template<size_t I> const auto& get(const MaskBorder& value)
{
    if constexpr (!I)
        return value.maskBorderSource;
    else if constexpr (I == 1)
        return value.maskBorderSlice;
    else if constexpr (I == 2)
        return value.maskBorderWidth;
    else if constexpr (I == 3)
        return value.maskBorderOutset;
    else if constexpr (I == 4)
        return value.maskBorderRepeat;
}

// MARK: - Conversion

template<> struct CSSValueCreation<MaskBorder> { auto operator()(CSSValuePool&, const MaskBorder&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<MaskBorder> { void operator()(StringBuilder&, const SerializationContext&, const MaskBorder&); };

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::MaskBorder, 5)
