/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "RenderLayer.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class RenderSVGHiddenContainer;

struct SVGPaintOrderAwareChild {
    CheckedPtr<RenderElement> renderer;
    CheckedPtr<RenderLayer> layer; // null for non-layer children
    int zIndex;
    bool selfOutlineOnly { false };
    LayoutSize accumulatedAncestorOffset; // Precomputed offset from non-layered ancestors between child and layer's renderer.
};

enum class SVGChildPaintScope : uint8_t { All, BeforeFirstCompositedChild, FromFirstCompositedChild };

class RenderLayerSVG final : public RenderLayer {
    WTF_MAKE_PREFERABLY_COMPACT_TZONE_ALLOCATED_EXPORT(RenderLayerSVG, WEBCORE_EXPORT);
public:
    static UniquelyOwnedPtr<RenderLayer> create(CheckedRef<RenderLayerModelObject> renderer)
    {
        return adoptUniquelyOwned(static_cast<RenderLayer*>(new RenderLayerSVG(renderer.get())));
    }

    WEBCORE_EXPORT ~RenderLayerSVG() final;

    bool isPaintingSVGResourceLayer() const final { return m_isPaintingSVGResourceLayer; }
    RenderSVGHiddenContainer* enclosingSVGHiddenOrResourceContainer() const final { return m_enclosingSVGHiddenOrResourceContainer.get(); }
    void paintSVGResourceLayer(GraphicsContext&, const AffineTransform&) final;
    void dirtySVGChildrenInDOMOrder() final;

protected:
    void paintNegativeZOrderChildren(GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>) final;
    void paintForegroundChildren(GraphicsContext&, const LayerPaintingInfo&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>, const LayerFragments&, OptionSet<PaintBehavior>, RenderObject*) final;
    void paintSVGForegroundSplitChildren(GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>, const LayerFragments&, OptionSet<PaintBehavior>, RenderObject*) final;

    HitLayer hitTestPositiveAndNormalFlowChildren(RenderLayer* rootLayer, const HitTestRequest&, HitTestResult&, const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState*, double* zOffsetForDescendants, bool depthSortDescendants, HitLayer& candidateLayer) final;
    HitLayer hitTestNegativeZOrderChildren(RenderLayer* rootLayer, const HitTestRequest&, HitTestResult&, const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState*, double* zOffsetForDescendants, bool depthSortDescendants, HitLayer& candidateLayer) final;

    bool hasVisibleContentForPainting() const final;
    void updateSVGSpecificAncestorState() final;

    bool shouldFailedFilterProduceTransparentBlack() const final { return true; }

private:
    explicit RenderLayerSVG(RenderLayerModelObject&);

    void collectSVGChildrenInDOMOrder();
    const Vector<SVGPaintOrderAwareChild>& svgChildrenInDOMOrder();
    void paintSVGChildrenInDOMOrder(GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>, const LayerFragments&, OptionSet<PaintBehavior>, RenderObject*, SVGChildPaintScope = SVGChildPaintScope::All);
    void paintNonLayerSVGChildForFragments(RenderElement&, const LayoutSize& accumulatedAncestorOffset, PaintPhase, const LayerFragments&, GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintBehavior>, RenderObject*, const LayoutPoint& containerBaseOffset, bool isSVGRoot);

    void paintSVGRendererByApplyingTransform(GraphicsContext&, CheckedRef<RenderElement>, const LayoutSize& positionOffset, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>, const LayerFragments&, OptionSet<PaintBehavior>, RenderObject*, const LayerPaintingInfo& outerPaintingInfo, const AffineTransform& accumulatedTransform);
    void paintSVGSubtreeWithinTransformScope(GraphicsContext&, RenderElement& container, const LayoutPoint& paintOffset, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>, OptionSet<PaintBehavior>, RenderObject*, const LayerPaintingInfo& outerPaintingInfo, const AffineTransform& accumulatedTransform);

    HitLayer hitTestSVGChildrenInDOMOrder(RenderLayer* rootLayer, const HitTestRequest&, HitTestResult&, const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState*, double* zOffsetForDescendants);

    HitLayer hitTestSVGRendererByInversingTransform(RenderElement&, const LayoutSize& positionOffset, RenderLayer* rootLayer, const HitTestRequest&, HitTestResult&, const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState*, double* zOffsetForDescendants, const LayoutRect& outerHitTestRect, const HitTestLocation& outerHitTestLocation);
    HitLayer hitTestSVGSubtreeWithinTransformScope(RenderElement& container, const LayoutPoint& accumulatedOffset, RenderLayer* rootLayer, const HitTestRequest&, HitTestResult&, const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState*, double* zOffsetForDescendants, const LayoutRect& outerHitTestRect, const HitTestLocation& outerHitTestLocation);

    struct SVGRendererTransform {
        TransformationMatrix transform;
        LayoutSize containerOffset;
    };
    std::optional<SVGRendererTransform> computeSVGRendererTransform(RenderElement&, const LayoutSize& positionOffset) const;

    bool m_svgChildrenInDOMOrderDirty : 1 { true };
    bool m_isPaintingSVGResourceLayer : 1 { false };
    std::unique_ptr<Vector<SVGPaintOrderAwareChild>> m_svgChildrenInDOMOrder;
    SingleThreadWeakPtr<RenderSVGHiddenContainer> m_enclosingSVGHiddenOrResourceContainer;
};

} // namespace WebCore
