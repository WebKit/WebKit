/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 */

#pragma once

#include "FloatRect.h"
#include "RenderReplaced.h"
#include "SVGRenderSupport.h"

namespace WebCore {

class AffineTransform;
class RenderSVGResourceContainer;
class SVGSVGElement;

class RenderSVGRoot final : public RenderReplaced {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGRoot);
public:
    RenderSVGRoot(SVGSVGElement&, RenderStyle&&);
    virtual ~RenderSVGRoot();

    SVGSVGElement& svgSVGElement() const;

    bool isEmbeddedThroughSVGImage() const;
    bool isEmbeddedThroughFrameContainingSVGDocument() const;

    void computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio) const override;

    bool isLayoutSizeChanged() const { return m_isLayoutSizeChanged; }
    void setNeedsBoundariesUpdate() override { m_needsBoundariesOrTransformUpdate = true; }
    bool needsBoundariesUpdate() override { return m_needsBoundariesOrTransformUpdate; }
    void setNeedsTransformUpdate() override { m_needsBoundariesOrTransformUpdate = true; }

    IntSize containerSize() const { return m_containerSize; }
    void setContainerSize(const IntSize& containerSize) { m_containerSize = containerSize; }

    bool hasRelativeDimensions() const override;

    // localToBorderBoxTransform maps local SVG viewport coordinates to local CSS box coordinates.  
    const AffineTransform& localToBorderBoxTransform() const { return m_localToBorderBoxTransform; }

    // The flag is cleared at the beginning of each layout() pass. Elements then call this
    // method during layout when they are invalidated by a filter.
    static void addResourceForClientInvalidation(RenderSVGResourceContainer*);

private:
    void element() const = delete;

    bool isSVGRoot() const override { return true; }
    const char* renderName() const override { return "RenderSVGRoot"; }

    LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred  = ComputeActual) const override;
    LayoutUnit computeReplacedLogicalHeight(std::optional<LayoutUnit> estimatedUsedWidth = std::nullopt) const override;
    void layout() override;
    void paintReplaced(PaintInfo&, const LayoutPoint&) override;

    void willBeDestroyed() override;

    void insertedIntoTree() override;
    void willBeRemovedFromTree() override;

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    const AffineTransform& localToParentTransform() const override;

    bool fillContains(const FloatPoint&) const;
    bool strokeContains(const FloatPoint&) const;

    FloatRect objectBoundingBox() const override { return m_objectBoundingBox; }
    FloatRect strokeBoundingBox() const override { return m_strokeBoundingBox; }
    FloatRect repaintRectInLocalCoordinates() const override { return m_repaintBoundingBox; }

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const override;
    std::optional<FloatRect> computeFloatVisibleRectInContainer(const FloatRect&, const RenderLayerModelObject* container, VisibleRectContext) const override;

    void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags, bool* wasFixed) const override;
    const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const override;

    bool canBeSelectionLeaf() const override { return false; }
    bool canHaveChildren() const override { return true; }

    bool shouldApplyViewportClip() const;
    void updateCachedBoundaries();
    void buildLocalToBorderBoxTransform();

    IntSize m_containerSize;
    FloatRect m_objectBoundingBox;
    bool m_objectBoundingBoxValid;
    FloatRect m_strokeBoundingBox;
    FloatRect m_repaintBoundingBox;
    FloatRect m_repaintBoundingBoxExcludingShadow;
    mutable AffineTransform m_localToParentTransform;
    AffineTransform m_localToBorderBoxTransform;
    HashSet<RenderSVGResourceContainer*> m_resourcesNeedingToInvalidateClients;
    bool m_isLayoutSizeChanged : 1;
    bool m_needsBoundariesOrTransformUpdate : 1;
    bool m_hasBoxDecorations : 1;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGRoot, isSVGRoot())
