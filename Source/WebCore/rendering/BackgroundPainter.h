/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "GraphicsTypes.h"
#include "RenderBoxModelObject.h"

namespace WebCore {

class GraphicsContext;
class FloatRoundedRect;

namespace Style {
enum class ShadowStyle : bool;
}

enum BaseBackgroundColorUsage {
    BaseBackgroundColorUse,
    BaseBackgroundColorOnly,
    BaseBackgroundColorSkip
};

struct BackgroundImageGeometry {
    BackgroundImageGeometry(const LayoutRect& destinationRect, const LayoutSize& tileSizeWithoutPixelSnapping, const LayoutSize& tileSize, const LayoutSize& phase, const LayoutSize& spaceSize, bool fixedAttachment);

    LayoutSize relativePhase() const
    {
        LayoutSize relativePhase = phase;
        relativePhase += destinationRect.location() - destinationOrigin;
        return relativePhase;
    }

    void clip(const LayoutRect& clipRect) { destinationRect.intersect(clipRect); }

    LayoutRect destinationRect;
    LayoutPoint destinationOrigin;
    LayoutSize tileSizeWithoutPixelSnapping;
    LayoutSize tileSize;
    LayoutSize phase;
    LayoutSize spaceSize;
    bool hasNonLocalGeometry; // Has background-attachment: fixed. Implies that we can't always cheaply compute destRect.
};

template<typename Layer>
struct FillLayerToPaint {
    const Layer& layer;
    bool isLast;
};

class BackgroundPainter {
public:
    BackgroundPainter(RenderBoxModelObject&, const PaintInfo&);

    void setOverrideClip(FillBox overrideClip) { m_overrideClip = overrideClip; }
    void setOverrideOrigin(FillBox overrideOrigin) { m_overrideOrigin = overrideOrigin; }

    void paintBackground(const LayoutRect&, BleedAvoidance) const;

    void paintFillLayers(const Color&, const Style::BackgroundLayers&, const LayoutRect&, BleedAvoidance, CompositeOperator, RenderElement* backgroundObject = nullptr) const;
    void paintFillLayer(const Color&, const FillLayerToPaint<Style::BackgroundLayer>&, const LayoutRect&, BleedAvoidance, const InlineIterator::InlineBoxIterator&, const LayoutRect& backgroundImageStrip = { }, CompositeOperator = CompositeOperator::SourceOver, RenderElement* backgroundObject = nullptr, BaseBackgroundColorUsage = BaseBackgroundColorUse) const;

    void paintFillLayers(const Color&, const Style::MaskLayers&, const LayoutRect&, BleedAvoidance, CompositeOperator, RenderElement* backgroundObject = nullptr) const;
    void paintFillLayer(const Color&, const FillLayerToPaint<Style::MaskLayer>&, const LayoutRect&, BleedAvoidance, const InlineIterator::InlineBoxIterator&, const LayoutRect& backgroundImageStrip = { }, CompositeOperator = CompositeOperator::SourceOver, RenderElement* backgroundObject = nullptr, BaseBackgroundColorUsage = BaseBackgroundColorUse) const;

    void paintBoxShadow(const LayoutRect&, const RenderStyle&, Style::ShadowStyle, RectEdges<bool> closedEdges = { true, true, true, true }) const;

    static bool paintsOwnBackground(const RenderBoxModelObject&);
    static BackgroundImageGeometry calculateFillLayerImageGeometry(const RenderBoxModelObject&, const RenderLayerModelObject* paintContainer, const Style::BackgroundLayer&, const LayoutPoint& paintOffset, const LayoutRect& borderBoxRect, std::optional<FillBox> overrideOrigin = std::nullopt);
    static BackgroundImageGeometry calculateFillLayerImageGeometry(const RenderBoxModelObject&, const RenderLayerModelObject* paintContainer, const Style::MaskLayer&, const LayoutPoint& paintOffset, const LayoutRect& borderBoxRect, std::optional<FillBox> overrideOrigin = std::nullopt);
    static void clipRoundedInnerRect(GraphicsContext&, const FloatRoundedRect& clipRect);
    static bool boxShadowShouldBeAppliedToBackground(const RenderBoxModelObject&, const LayoutPoint& paintOffset, BleedAvoidance, const InlineIterator::InlineBoxIterator&);

private:
    void paintRootBoxFillLayers() const;

    template<typename LayerList> void paintFillLayersImpl(const Color&, const LayerList&, const LayoutRect&, BleedAvoidance, CompositeOperator, RenderElement* backgroundObject = nullptr) const;
    template<typename Layer> void paintFillLayerImpl(const Color&, const FillLayerToPaint<Layer>&, const LayoutRect&, BleedAvoidance, const InlineIterator::InlineBoxIterator&, const LayoutRect& backgroundImageStrip = { }, CompositeOperator = CompositeOperator::SourceOver, RenderElement* backgroundObject = nullptr, BaseBackgroundColorUsage = BaseBackgroundColorUse) const;
    template<typename Layer> static BackgroundImageGeometry calculateFillLayerImageGeometryImpl(const RenderBoxModelObject&, const RenderLayerModelObject* paintContainer, const Layer&, const LayoutPoint& paintOffset, const LayoutRect& borderBoxRect, std::optional<FillBox> overrideOrigin = std::nullopt);
    template<typename Layer> static LayoutSize calculateFillTileSize(const RenderBoxModelObject&, const Layer&, const LayoutSize& positioningAreaSize);

    const Document& document() const;
    const RenderView& view() const;

    RenderBoxModelObject& m_renderer;
    const PaintInfo& m_paintInfo;
    std::optional<FillBox> m_overrideClip;
    std::optional<FillBox> m_overrideOrigin;
};

}
