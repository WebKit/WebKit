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

#include "RenderBoxModelObject.h"

namespace WebCore {

class GraphicsContext;
class FloatRoundedRect;

enum class ShadowStyle : uint8_t;

class FloatRoundedRect;

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

class BackgroundPainter {
public:
    BackgroundPainter(RenderBoxModelObject&, const PaintInfo&);

    void paintBackground(const LayoutRect&, BackgroundBleedAvoidance) const;

    void paintFillLayers(const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance, CompositeOperator, RenderElement* backgroundObject = nullptr) const;
    void paintFillLayer(const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance, const InlineIterator::InlineBoxIterator&, const LayoutRect& backgroundImageStrip = { }, CompositeOperator = CompositeOperator::SourceOver, RenderElement* backgroundObject = nullptr, BaseBackgroundColorUsage = BaseBackgroundColorUse) const;

    void paintBoxShadow(const LayoutRect&, const RenderStyle&, ShadowStyle, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;

    static bool paintsOwnBackground(const RenderBoxModelObject&);
    static BackgroundImageGeometry calculateBackgroundImageGeometry(const RenderBoxModelObject&, const RenderLayerModelObject* paintContainer, const FillLayer&, const LayoutPoint& paintOffset, const LayoutRect& borderBoxRect);
    static void clipRoundedInnerRect(GraphicsContext&, const FloatRoundedRect& clipRect);
    static bool boxShadowShouldBeAppliedToBackground(const RenderBoxModelObject&, const LayoutPoint& paintOffset, BackgroundBleedAvoidance, const InlineIterator::InlineBoxIterator&);

private:
    void paintRootBoxFillLayers() const;

    RoundedRect backgroundRoundedRectAdjustedForBleedAvoidance(const LayoutRect& borderRect, BackgroundBleedAvoidance, const InlineIterator::InlineBoxIterator&, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const;
    RoundedRect backgroundRoundedRect(const LayoutRect& borderRect, const InlineIterator::InlineBoxIterator&, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const;

    static LayoutSize calculateFillTileSize(const RenderBoxModelObject&, const FillLayer&, const LayoutSize& positioningAreaSize);

    const Document& document() const;
    const RenderView& view() const;

    RenderBoxModelObject& m_renderer;
    const PaintInfo& m_paintInfo;
};

}
