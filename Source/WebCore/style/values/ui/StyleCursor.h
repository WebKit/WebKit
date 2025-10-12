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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/IntPoint.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleImage.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <cursor-image> = [ <url> | <url-set> ] <number>{2}?
// https://drafts.csswg.org/css-ui-4/#typedef-cursor-cursor-image
struct CursorImage {
    Ref<StyleImage> image;
    IntPoint hotSpot { -1, -1 };

    bool operator==(const CursorImage&) const = default;
};

using CursorImageList = CommaSeparatedRefCountedFixedVector<CursorImage>;

// <'cursor'> = <cursor-image>#? <cursor-predefined>
// https://drafts.csswg.org/css-ui-4/#propdef-cursor
struct Cursor {
    using Images = std::optional<CursorImageList>;

    Images images { };
    CursorType predefined { CursorType::Auto };

    Cursor(Images&& images, CursorType predefined)
        : images { WTFMove(images) }
        , predefined { predefined }
    {
    }

    Cursor(const Images& images, CursorType predefined)
        : images { images }
        , predefined { predefined }
    {
    }

    Cursor(CursorType predefined)
        : predefined { predefined }
    {
    }

    // Special constructor for use constructing initial 'auto' value.
    Cursor(CSS::Keyword::Auto)
        : predefined { CursorType::Auto }
    {
    }

    bool operator==(const Cursor&) const = default;
};

template<size_t I> const auto& get(const Cursor& value)
{
    if constexpr (!I)
        return value.images;
    else if constexpr (I == 1)
        return value.predefined;
}

// MARK: - Conversion

template<> struct CSSValueConversion<Cursor> { auto operator()(BuilderState&, const CSSValue&) -> Cursor; };

template<> struct CSSValueCreation<CursorImage> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const CursorImage&); };

// MARK: - Serialization

template<> struct Serialize<CursorImage> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const CursorImage&); };

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const CursorImage&);

} // namespace Style
} // namespace WebCore

DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::Cursor, 2)
