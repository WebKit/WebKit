/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

#include <WebCore/StyleMaskBorderOutset.h>
#include <WebCore/StyleMaskBorderRepeat.h>
#include <WebCore/StyleMaskBorderSlice.h>
#include <WebCore/StyleMaskBorderSource.h>
#include <WebCore/StyleMaskBorderWidth.h>

namespace WebCore {
namespace Style {

// <'mask-border'> = <'mask-border-source'> || <'mask-border-slice'> [ / <'mask-border-width'>? [ / <'mask-border-outset'> ]? ]? || <'mask-border-repeat'> || <'mask-border-mode'>
// FIXME: Add support for `mask-border-mode`.
// https://drafts.fxtf.org/css-masking-1/#propdef-mask-border
struct MaskBorder {
    MaskBorder();
    MaskBorder(MaskBorderSource&&, MaskBorderSlice&&, MaskBorderWidth&&, MaskBorderOutset&&, MaskBorderRepeat&&);

    MaskBorderSource maskBorderSource;
    MaskBorderSlice maskBorderSlice;
    MaskBorderWidth maskBorderWidth;
    MaskBorderOutset maskBorderOutset;
    MaskBorderRepeat maskBorderRepeat;

    // Alias accessors for using in generic contexts with `BorderImage`.
    const MaskBorderSource& source() const { return maskBorderSource; }
    const MaskBorderSlice& slice() const { return maskBorderSlice; }
    const MaskBorderWidth& width() const { return maskBorderWidth; }
    const MaskBorderOutset& outset() const { return maskBorderOutset; }
    const MaskBorderRepeat& repeat() const { return maskBorderRepeat; }

    bool operator==(const MaskBorder&) const = default;
};

// MARK: - Conversion

enum class MaskBorderSliceOverride : bool { None, AlwaysFill };

template<> struct CSSValueConversion<MaskBorder> { auto operator()(BuilderState&, const CSSValue&, MaskBorderSliceOverride = MaskBorderSliceOverride::None) -> MaskBorder; };
template<> struct CSSValueCreation<MaskBorder> { auto operator()(CSSValuePool&, const RenderStyle&, const MaskBorder&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<MaskBorder> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const MaskBorder&); };

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const MaskBorder&);

} // namespace Style
} // namespace WebCore
