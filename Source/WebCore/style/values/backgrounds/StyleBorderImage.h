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

#include <WebCore/StyleBorderImageOutset.h>
#include <WebCore/StyleBorderImageRepeat.h>
#include <WebCore/StyleBorderImageSlice.h>
#include <WebCore/StyleBorderImageSource.h>
#include <WebCore/StyleBorderImageWidth.h>

namespace WebCore {
namespace Style {

// <'border-image'> = <'border-image-source'> || <'border-image-slice'> [ / <'border-image-width'> | / <'border-image-width'>? / <'border-image-outset'> ]? || <'border-image-repeat'>
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image
struct BorderImage {
    BorderImage();
    BorderImage(BorderImageSource&&, BorderImageSlice&&, BorderImageWidth&&, BorderImageOutset&&, BorderImageRepeat&&);

    BorderImageSource borderImageSource;
    BorderImageSlice borderImageSlice;
    BorderImageWidth borderImageWidth;
    BorderImageOutset borderImageOutset;
    BorderImageRepeat borderImageRepeat;

    // Alias accessors for using in generic contexts with `MaskBorder`.
    const BorderImageSource& source() const { return borderImageSource; }
    const BorderImageSlice& slice() const { return borderImageSlice; }
    const BorderImageWidth& width() const { return borderImageWidth; }
    const BorderImageOutset& outset() const { return borderImageOutset; }
    const BorderImageRepeat& repeat() const { return borderImageRepeat; }

    bool operator==(const BorderImage&) const = default;
};

// MARK: - Conversion

template<> struct CSSValueCreation<BorderImage> { auto operator()(CSSValuePool&, const RenderStyle&, const BorderImage&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<BorderImage> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const BorderImage&); };

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const BorderImage&);

} // namespace Style
} // namespace WebCore
