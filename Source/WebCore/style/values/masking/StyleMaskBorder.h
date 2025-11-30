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
#include <wtf/DataRef.h>

namespace WebCore {
namespace Style {

// <'mask-border'> = <'mask-border-source'> || <'mask-border-slice'> [ / <'mask-border-width'>? [ / <'mask-border-outset'> ]? ]? || <'mask-border-repeat'> || <'mask-border-mode'>
// FIXME: Add support for `mask-border-mode`.
// https://drafts.fxtf.org/css-masking-1/#propdef-mask-border
struct MaskBorder {
    using Source = MaskBorderSource;
    using Slice = MaskBorderSlice;
    using Width = MaskBorderWidth;
    using Outset = MaskBorderOutset;
    using Repeat = MaskBorderRepeat;

    MaskBorder();
    MaskBorder(MaskBorderSource&&, MaskBorderSlice&&, MaskBorderWidth&&, MaskBorderOutset&&, MaskBorderRepeat&&);

    bool hasSource() const { return !m_data->source.isNone(); }
    const MaskBorderSource& source() const { return m_data->source; }
    void setSource(MaskBorderSource&& source) { m_data.access().source = WTFMove(source); }

    const MaskBorderSlice& slice() const { return m_data->slice; }
    void setSlice(MaskBorderSlice&& slice) { m_data.access().slice = WTFMove(slice); }

    const MaskBorderWidth& width() const { return m_data->width; }
    void setWidth(MaskBorderWidth&& width) { m_data.access().width = WTFMove(width); }

    const MaskBorderOutset& outset() const { return m_data->outset; }
    void setOutset(MaskBorderOutset&& outset) { m_data.access().outset = WTFMove(outset); }

    const MaskBorderRepeat& repeat() const { return m_data->repeat; }
    void setRepeat(MaskBorderRepeat&& repeat) { m_data.access().repeat = WTFMove(repeat); }

    void copySliceFrom(const MaskBorder& other)
    {
        m_data.access().slice = other.m_data->slice;
    }

    void copyWidthFrom(const MaskBorder& other)
    {
        m_data.access().width = other.m_data->width;
    }

    void copyOutsetFrom(const MaskBorder& other)
    {
        m_data.access().outset = other.m_data->outset;
    }

    void copyRepeatFrom(const MaskBorder& other)
    {
        m_data.access().repeat = other.m_data->repeat;
    }

    bool operator==(const MaskBorder&) const = default;

private:
    friend class WebCore::RenderStyleProperties;

    struct Data : RefCounted<Data> {
        static Ref<Data> create();
        static Ref<Data> create(MaskBorderSource&&, MaskBorderSlice&&, MaskBorderWidth&&, MaskBorderOutset&&, MaskBorderRepeat&&);
        Ref<Data> copy() const;

        bool operator==(const Data&) const;

        MaskBorderSource source { CSS::Keyword::None { } };
        MaskBorderSlice slice { .values = { MaskBorderSliceValue::Number { 0 } }, .fill = { std::nullopt } };
        MaskBorderWidth width { .values = { CSS::Keyword::Auto { } } };
        MaskBorderOutset outset { .values = { MaskBorderOutsetValue::Number { 0 } } };
        MaskBorderRepeat repeat { .values { NinePieceImageRule::Stretch, NinePieceImageRule::Stretch } };

    private:
        Data();
        Data(MaskBorderSource&&, MaskBorderSlice&&, MaskBorderWidth&&, MaskBorderOutset&&, MaskBorderRepeat&&);
        Data(const Data&);
    };

    static DataRef<Data>& defaultData();

    DataRef<Data> m_data;
};

// MARK: - Conversion

template<> struct CSSValueConversion<MaskBorder> { auto operator()(BuilderState&, const CSSValue&) -> MaskBorder; };
template<> struct CSSValueCreation<MaskBorder> { auto operator()(CSSValuePool&, const RenderStyle&, const MaskBorder&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<MaskBorder> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const MaskBorder&); };

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const MaskBorder&);

} // namespace Style
} // namespace WebCore
