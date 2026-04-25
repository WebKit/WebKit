/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/BorderValue.h>
#include <WebCore/RectCorners.h>
#include <WebCore/RectEdges.h>
#include <WebCore/StyleBorderImageData.h>
#include <WebCore/StyleBorderRadius.h>
#include <WebCore/StyleCornerShapeValue.h>
#include <wtf/DataRef.h>

namespace WebCore {

using namespace CSS::Literals;

struct BorderData {
    using Radii = Style::BorderRadius;

    BorderData();

    bool hasBorder() const
    {
        return edges.anyOf([](const auto& edge) { return edge.nonZero(); });
    }

    bool hasVisibleBorder() const
    {
        return edges.anyOf([](const auto& edge) { return edge.isVisible(); });
    }

    bool hasBorderImage() const
    {
        return !borderImage->borderImage.borderImageSource.isNone();
    }

    bool hasBorderRadius() const
    {
        return radii.anyOf([](auto& corner) { return !Style::isKnownEmpty(corner); });
    }

    bool hasVisibleBorderDecoration() const
    {
        return hasVisibleBorder() || hasBorderImage();
    }

    // `BorderEdgesView` provides a `RectEdges`-like interface for efficiently working with
    // the values stored in `BorderValue` by edge. This allows `Style::ComputedStyle` code
    // generation to work as if the `border-{edge}-*`properties were stored in a `RectEdges`,
    // while instead storing them grouped together by edge in `BorderValue``.
    template<bool isConst, template<BoxSide> typename Accessor, typename GetterType, typename SetterType = GetterType>
    using BorderEdgesView = RectEdgesView<isConst, BorderData, Accessor, GetterType, SetterType>;

    template<BoxSide side> struct WidthAccessor {
        static const Style::LineWidth& get(const BorderData& data) { return data.edges[side].width; }
        static void set(BorderData& data, Style::LineWidth&& width) { data.edges[side].width = WTF::move(width); }
    };
    template<bool isConst> using BorderWidthsView = BorderEdgesView<isConst, WidthAccessor, const Style::LineWidth&, Style::LineWidth&&>;
    BorderWidthsView<false> widths() { return { .data = *this }; }
    BorderWidthsView<true> widths() const { return { .data = *this }; }

    template<BoxSide side> struct ColorAccessor {
        static const Style::Color& get(const BorderData& data) { return data.edges[side].color; }
        static void set(BorderData& data, Style::Color&& color) { data.edges[side].color = WTF::move(color); }
    };
    template<bool isConst> using BorderColorsView = BorderEdgesView<isConst, ColorAccessor, const Style::Color&, Style::Color&&>;
    BorderColorsView<false> colors() { return { .data = *this }; }
    BorderColorsView<true> colors() const { return { .data = *this }; }

    template<BoxSide side> struct StyleAccessor {
        static unsigned get(const BorderData& data) { return data.edges[side].style; }
        static void set(BorderData& data, unsigned style) { data.edges[side].style = style; }
    };
    template<bool isConst> using BorderStylesView = BorderEdgesView<isConst, StyleAccessor, unsigned>;
    BorderStylesView<false> styles() { return { .data = *this }; }
    BorderStylesView<true> styles() const { return { .data = *this }; }

    BorderValue& left() LIFETIME_BOUND { return edges.left(); }
    BorderValue& right() LIFETIME_BOUND { return edges.right(); }
    BorderValue& top() LIFETIME_BOUND { return edges.top(); }
    BorderValue& bottom() LIFETIME_BOUND { return edges.bottom(); }

    const BorderValue& left() const LIFETIME_BOUND { return edges.left(); }
    const BorderValue& right() const LIFETIME_BOUND { return edges.right(); }
    const BorderValue& top() const LIFETIME_BOUND { return edges.top(); }
    const BorderValue& bottom() const LIFETIME_BOUND { return edges.bottom(); }

    Style::BorderRadiusValue& topLeftRadius() LIFETIME_BOUND { return radii.topLeft(); }
    Style::BorderRadiusValue& topRightRadius() LIFETIME_BOUND { return radii.topRight(); }
    Style::BorderRadiusValue& bottomLeftRadius() LIFETIME_BOUND { return radii.bottomLeft(); }
    Style::BorderRadiusValue& bottomRightRadius() LIFETIME_BOUND { return radii.bottomRight(); }

    const Style::BorderRadiusValue& topLeftRadius() const LIFETIME_BOUND { return radii.topLeft(); }
    const Style::BorderRadiusValue& topRightRadius() const LIFETIME_BOUND { return radii.topRight(); }
    const Style::BorderRadiusValue& bottomLeftRadius() const LIFETIME_BOUND { return radii.bottomLeft(); }
    const Style::BorderRadiusValue& bottomRightRadius() const LIFETIME_BOUND { return radii.bottomRight(); }

    const Style::CornerShapeValue& topLeftCornerShape() const LIFETIME_BOUND { return cornerShapes.topLeft(); }
    const Style::CornerShapeValue& topRightCornerShape() const LIFETIME_BOUND { return cornerShapes.topRight(); }
    const Style::CornerShapeValue& bottomLeftCornerShape() const LIFETIME_BOUND { return cornerShapes.bottomLeft(); }
    const Style::CornerShapeValue& bottomRightCornerShape() const LIFETIME_BOUND { return cornerShapes.bottomRight(); }

    bool containsCurrentColor() const;

    bool operator==(const BorderData&) const = default;

    void dump(TextStream&, DumpStyleValues = DumpStyleValues::All) const;
#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const BorderData&) const;
#endif

    RectEdges<BorderValue> edges;
    Style::BorderRadius radii { Style::BorderRadiusValue { 0_css_px, 0_css_px } };
    Style::CornerShape cornerShapes { Style::CornerShapeValue(CSS::Keyword::Round { }) };
    DataRef<Style::BorderImageData> borderImage;
};

WTF::TextStream& operator<<(WTF::TextStream&, const BorderData&);

} // namespace WebCore
