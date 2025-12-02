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

#include <WebCore/BorderValue.h>
#include <WebCore/RectCorners.h>
#include <WebCore/RectEdges.h>
#include <WebCore/StyleBorderImage.h>
#include <WebCore/StyleBorderRadius.h>
#include <WebCore/StyleCornerShapeValue.h>

namespace WebCore {

using namespace CSS::Literals;

class BorderData {
    friend class RenderStyle;
public:
    using Radii = Style::BorderRadius;

    bool hasBorder() const
    {
        return m_edges.anyOf([](const auto& edge) { return edge.nonZero(); });
    }

    bool hasVisibleBorder() const
    {
        return m_edges.anyOf([](const auto& edge) { return edge.isVisible(); });
    }

    bool hasBorderImage() const
    {
        return !m_image.source().isNone();
    }

    bool hasBorderRadius() const
    {
        return m_radii.anyOf([](auto& corner) { return !Style::isKnownEmpty(corner); });
    }

    template<BoxSide side>
    Style::LineWidth borderEdgeWidth() const
    {
        if (m_edges[side].style() == BorderStyle::None || m_edges[side].style() == BorderStyle::Hidden)
            return 0_css_px;
        if (m_image.overridesBorderWidths()) {
            if (auto fixedBorderWidthValue = m_image.width().values[side].tryFixed())
                return Style::LineWidth { *fixedBorderWidthValue };
        }
        return m_edges[side].width();
    }

    Style::LineWidth borderLeftWidth() const { return borderEdgeWidth<BoxSide::Left>(); }
    Style::LineWidth borderRightWidth() const { return borderEdgeWidth<BoxSide::Right>(); }
    Style::LineWidth borderTopWidth() const { return borderEdgeWidth<BoxSide::Top>(); }
    Style::LineWidth borderBottomWidth() const { return borderEdgeWidth<BoxSide::Bottom>(); }

    Style::LineWidthBox borderWidth() const
    {
        return { borderTopWidth(), borderRightWidth(), borderBottomWidth(), borderLeftWidth() };
    }

    // `BorderEdgesView` provides a RectEdges-like interface for efficiently working with
    // the values stored in BorderValue by edge. This allows `RenderStyle` code generation
    // to work as if the `border-{edge}-*`properties were stored in a RectEdges, while
    // instead storing them grouped together by edge in BorderValue.
    //
    // FIXME: Currently this is only implemented for `border-{edge}-color` and `border-{edge}-style`,
    // due to `border-{edge}-width` needing to return the computed value from borderEdgeWidth() from
    // its getter.
    template<bool isConst, template<BoxSide> typename Accessor, typename GetterType, typename SetterType = GetterType>
    struct BorderEdgesView {
        GetterType top() const { return Accessor<BoxSide::Top>::get(borderData); }
        GetterType right() const { return Accessor<BoxSide::Right>::get(borderData); }
        GetterType bottom() const { return Accessor<BoxSide::Bottom>::get(borderData); }
        GetterType left() const { return Accessor<BoxSide::Left>::get(borderData); }

        void setTop(SetterType value) requires (!isConst) { Accessor<BoxSide::Top>::set(borderData, std::forward<SetterType>(value)); }
        void setRight(SetterType value) requires (!isConst){ Accessor<BoxSide::Right>::set(borderData, std::forward<SetterType>(value)); }
        void setBottom(SetterType value) requires (!isConst){ Accessor<BoxSide::Bottom>::set(borderData, std::forward<SetterType>(value)); }
        void setLeft(SetterType value) requires (!isConst) { Accessor<BoxSide::Left>::set(borderData, std::forward<SetterType>(value)); }

        std::conditional_t<isConst, const BorderData&, BorderData&> borderData;
    };

    template<BoxSide side> struct ColorAccessor {
        static const Style::Color& get(const BorderData& data) { return data.edges()[side].m_color; }
        static void set(BorderData& data, Style::Color&& color) { data.edges()[side].m_color = WTFMove(color); }
    };
    using BorderColorsView = BorderEdgesView<false, ColorAccessor, const Style::Color&, Style::Color&&>;
    using BorderColorsConstView = BorderEdgesView<true, ColorAccessor, const Style::Color&, Style::Color&&>;
    BorderColorsView colors() { return { .borderData = *this }; }
    BorderColorsConstView colors() const { return { .borderData = *this }; }

    template<BoxSide side> struct StyleAccessor {
        static unsigned get(const BorderData& data) { return data.edges()[side].m_style; }
        static void set(BorderData& data, unsigned style) { data.edges()[side].m_style = style; }
    };
    using BorderStylesView = BorderEdgesView<false, StyleAccessor, unsigned>;
    using BorderStylesConstView = BorderEdgesView<true, StyleAccessor, unsigned>;
    BorderStylesView styles() { return { .borderData = *this }; }
    BorderStylesConstView styles() const { return { .borderData = *this }; }

    RectEdges<BorderValue>& edges() { return m_edges; }
    const RectEdges<BorderValue>& edges() const { return m_edges; }

    BorderValue& left() { return m_edges.left(); }
    BorderValue& right() { return m_edges.right(); }
    BorderValue& top() { return m_edges.top(); }
    BorderValue& bottom() { return m_edges.bottom(); }

    const BorderValue& left() const { return m_edges.left(); }
    const BorderValue& right() const { return m_edges.right(); }
    const BorderValue& top() const { return m_edges.top(); }
    const BorderValue& bottom() const { return m_edges.bottom(); }

    const Style::BorderImage& image() const { return m_image; }

    const Style::BorderRadiusValue& topLeftRadius() const { return m_radii.topLeft(); }
    const Style::BorderRadiusValue& topRightRadius() const { return m_radii.topRight(); }
    const Style::BorderRadiusValue& bottomLeftRadius() const { return m_radii.bottomLeft(); }
    const Style::BorderRadiusValue& bottomRightRadius() const { return m_radii.bottomRight(); }
    const Style::BorderRadius& radii() const { return m_radii; }

    const Style::CornerShapeValue& topLeftCornerShape() const { return m_cornerShapes.topLeft(); }
    const Style::CornerShapeValue& topRightCornerShape() const { return m_cornerShapes.topRight(); }
    const Style::CornerShapeValue& bottomLeftCornerShape() const { return m_cornerShapes.bottomLeft(); }
    const Style::CornerShapeValue& bottomRightCornerShape() const { return m_cornerShapes.bottomRight(); }

    bool isEquivalentForPainting(const BorderData& other, bool currentColorDiffers) const;

    void dump(TextStream&, DumpStyleValues = DumpStyleValues::All) const;

    bool operator==(const BorderData&) const = default;

private:
    bool containsCurrentColor() const;

    RectEdges<BorderValue> m_edges;
    Style::BorderImage m_image;
    Style::BorderRadius m_radii { Style::BorderRadiusValue { 0_css_px, 0_css_px } };
    Style::CornerShape m_cornerShapes { Style::CornerShapeValue::round() };
};

WTF::TextStream& operator<<(WTF::TextStream&, const BorderData&);

} // namespace WebCore
