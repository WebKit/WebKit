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
#include <wtf/DataRef.h>

namespace WebCore {
namespace Style {

// <'border-image'> = <'border-image-source'> || <'border-image-slice'> [ / <'border-image-width'> | / <'border-image-width'>? / <'border-image-outset'> ]? || <'border-image-repeat'>
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image
struct BorderImage {
    using Source = BorderImageSource;
    using Slice = BorderImageSlice;
    using Width = BorderImageWidth;
    using Outset = BorderImageOutset;
    using Repeat = BorderImageRepeat;

    BorderImage();
    BorderImage(BorderImageSource&&, BorderImageSlice&&, BorderImageWidth&&, BorderImageOutset&&, BorderImageRepeat&&);

    bool hasSource() const { return !m_data->source.isNone(); }
    const BorderImageSource& source() const { return m_data->source; }
    void setSource(BorderImageSource&& source) { m_data.access().source = WTFMove(source); }

    const BorderImageSlice& slice() const { return m_data->slice; }
    void setSlice(BorderImageSlice&& slice) { m_data.access().slice = WTFMove(slice); }

    const BorderImageWidth& width() const { return m_data->width; }
    void setWidth(BorderImageWidth&& width) { m_data.access().width = WTFMove(width); }

    const BorderImageOutset& outset() const { return m_data->outset; }
    void setOutset(BorderImageOutset&& outset) { m_data.access().outset = WTFMove(outset); }

    const BorderImageRepeat& repeat() const { return m_data->repeat; }
    void setRepeat(BorderImageRepeat&& repeat) { m_data.access().repeat = WTFMove(repeat); }

    void copySliceFrom(const BorderImage& other)
    {
        m_data.access().slice = other.m_data->slice;
    }

    void copyWidthFrom(const BorderImage& other)
    {
        m_data.access().width = other.m_data->width;
    }

    void copyOutsetFrom(const BorderImage& other)
    {
        m_data.access().outset = other.m_data->outset;
    }

    void copyRepeatFrom(const BorderImage& other)
    {
        m_data.access().repeat = other.m_data->repeat;
    }

    bool overridesBorderWidths() const { return width().legacyWebkitBorderImage; }

    bool operator==(const BorderImage&) const = default;

private:
    friend class WebCore::RenderStyle;

    struct Data : RefCounted<Data> {
        static Ref<Data> create();
        static Ref<Data> create(BorderImageSource&&, BorderImageSlice&&, BorderImageWidth&&, BorderImageOutset&&, BorderImageRepeat&&);
        Ref<Data> copy() const;

        bool operator==(const Data&) const;

        BorderImageSource source { CSS::Keyword::None { } };
        BorderImageSlice slice { .values = { BorderImageSliceValue::Percentage { 100 } }, .fill = { std::nullopt } };
        BorderImageWidth width { .values = { BorderImageWidthValue::Number { 1 } }, .legacyWebkitBorderImage = false };
        BorderImageOutset outset { .values = { BorderImageOutsetValue::Number { 0 } } };
        BorderImageRepeat repeat { .values { NinePieceImageRule::Stretch, NinePieceImageRule::Stretch } };

    private:
        Data();
        Data(BorderImageSource&&, BorderImageSlice&&, BorderImageWidth&&, BorderImageOutset&&, BorderImageRepeat&&);
        Data(const Data&);
    };

    static DataRef<Data>& defaultData();

    DataRef<Data> m_data;
};

// MARK: - Conversion

template<> struct CSSValueCreation<BorderImage> { auto operator()(CSSValuePool&, const RenderStyle&, const BorderImage&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<BorderImage> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const BorderImage&); };

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const BorderImage&);

} // namespace Style
} // namespace WebCore
