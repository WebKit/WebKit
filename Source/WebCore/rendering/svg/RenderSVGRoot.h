/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2020, 2021, 2022 Igalia S.L.
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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "FloatRect.h"
#include "RenderReplaced.h"
#include "RenderSVGViewportContainer.h"
#include "SVGBoundingBoxComputation.h"

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
    FloatSize computeViewportSize() const;

    bool isEmbeddedThroughSVGImage() const;
    bool isEmbeddedThroughFrameContainingSVGDocument() const;

    void computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio) const final;

    bool isLayoutSizeChanged() const { return m_isLayoutSizeChanged; }
    bool didTransformToRootUpdate() const { return m_didTransformToRootUpdate; }
    bool isInLayout() const { return m_inLayout; }

    IntSize containerSize() const { return m_containerSize; }
    void setContainerSize(const IntSize& containerSize) { m_containerSize = containerSize; }

    bool hasRelativeDimensions() const final;

    // The flag is cleared at the beginning of each layout() pass. Elements then call this
    // method during layout when they are invalidated by a filter.
    static void addResourceForClientInvalidation(RenderSVGResourceContainer*);

    bool shouldApplyViewportClip() const;

    FloatRect objectBoundingBox() const final { return m_objectBoundingBox; }
    FloatRect objectBoundingBoxWithoutTransformations() const final { return m_objectBoundingBoxWithoutTransformations; }
    FloatRect strokeBoundingBox() const final { return m_strokeBoundingBox; }
    FloatRect repaintRectInLocalCoordinates() const final { return SVGBoundingBoxComputation::computeRepaintBoundingBox(*this); }

    LayoutRect visualOverflowRectEquivalent() const { return SVGBoundingBoxComputation::computeVisualOverflowRect(*this); }

    RenderSVGViewportContainer* viewportContainer() const { return m_viewportContainer.get(); }
    void setViewportContainer(RenderSVGViewportContainer&);

private:
    void element() const = delete;

    bool isSVGRoot() const final { return true; }
    ASCIILiteral renderName() const final { return "RenderSVGRoot"_s; }
    bool requiresLayer() const final { return true; }

    bool updateLayoutSizeIfNeeded();
    bool paintingAffectedByExternalOffset() const;

    // To prevent certain legacy code paths to hit assertions in debug builds, when switching off LBSE (during the teardown of the LBSE tree).
    std::optional<FloatRect> computeFloatVisibleRectInContainer(const FloatRect&, const RenderLayerModelObject*, VisibleRectContext) const final { return std::nullopt; }

    LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred  = ComputeActual) const final;
    LayoutUnit computeReplacedLogicalHeight(std::optional<LayoutUnit> estimatedUsedWidth = std::nullopt) const final;
    void layout() final;
    void layoutChildren();
    void paint(PaintInfo&, const LayoutPoint&) final;
    void paintObject(PaintInfo&, const LayoutPoint&) final;
    void paintContents(PaintInfo&, const LayoutPoint&);

    void willBeDestroyed() final;

    void insertedIntoTree(IsInternalMove) final;
    void willBeRemovedFromTree(IsInternalMove) final;

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;
    void updateFromStyle() final;
    void updateFromElement() final;
    void updateLayerTransform() final;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;

    LayoutRect overflowClipRect(const LayoutPoint& location, RenderFragmentContainer* = nullptr, OverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize, PaintPhase = PaintPhase::BlockBackground) const final;
    LayoutRect clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext) const final;

    void mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState&, OptionSet<MapCoordinatesMode>, bool* wasFixed) const final;

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const final;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const final;

    bool canBeSelectionLeaf() const final { return false; }
    bool canHaveChildren() const final { return true; }

    bool m_inLayout { false };
    bool m_didTransformToRootUpdate { false };
    bool m_isLayoutSizeChanged { false };

    IntSize m_containerSize;
    FloatRect m_objectBoundingBox;
    FloatRect m_objectBoundingBoxWithoutTransformations;
    FloatRect m_strokeBoundingBox;
    HashSet<RenderSVGResourceContainer*> m_resourcesNeedingToInvalidateClients;
    WeakPtr<RenderSVGViewportContainer> m_viewportContainer;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGRoot, isSVGRoot())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
