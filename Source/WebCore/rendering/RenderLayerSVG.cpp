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

#include "config.h"
#include "RenderLayerSVG.h"

#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "RenderDescendantIterator.h"
#include "RenderElementInlines.h"
#include "RenderLayerBacking.h"
#include "RenderLayerInlines.h"
#include "RenderLayerModelObject.h"
#include "RenderObjectInlines.h"
#include "RenderSVGContainer.h"
#include "RenderSVGForeignObject.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGInline.h"
#include "RenderSVGModelObject.h"
#include "RenderSVGResourceContainer.h"
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGRoot.h"
#include "RenderSVGText.h"
#include "RenderSVGViewportContainer.h"
#include "SVGRenderSupport.h"
#include "Settings.h"
#include "StyleTransformResolver.h"
#include "TransformPaintScope.h"
#include "TransformationMatrix.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_PREFERABLY_COMPACT_TZONE_ALLOCATED_IMPL(RenderLayerSVG);

RenderLayerSVG::RenderLayerSVG(RenderLayerModelObject& renderer)
    : RenderLayer(renderer, RenderLayerType::SVG)
{
}

RenderLayerSVG::~RenderLayerSVG() = default;

void RenderLayerSVG::dirtySVGChildrenInDOMOrder()
{
    if (m_svgChildrenInDOMOrder)
        m_svgChildrenInDOMOrder->clear();
    m_svgChildrenInDOMOrderDirty = true;
}

void RenderLayerSVG::updateSVGSpecificAncestorState()
{
    for (auto* ancestor = renderer().parent(); ancestor; ancestor = ancestor->parent()) {
        if (auto* container = dynamicDowncast<RenderSVGHiddenContainer>(ancestor)) {
            m_enclosingSVGHiddenOrResourceContainer = container;
            return;
        }
    }

    m_enclosingSVGHiddenOrResourceContainer = nullptr;
}

bool RenderLayerSVG::hasVisibleContentForPainting() const
{
    if (!hasVisibleContent())
        return false;

    if (!m_enclosingSVGHiddenOrResourceContainer)
        return true;

    // Hidden SVG containers (<defs> / <symbol> ...) and their children are never painted directly.
    CheckedPtr container = m_enclosingSVGHiddenOrResourceContainer.get();
    if (!is<RenderSVGResourceContainer>(container.get()))
        return false;

    // SVG resource layers and their children are only painted indirectly, via paintSVGResourceLayer().
    ASSERT(container->hasLayer());
    CheckedPtr containerLayer = container->layer();
    return containerLayer->isPaintingSVGResourceLayer();
}

void RenderLayerSVG::paintSVGResourceLayer(GraphicsContext& context, const AffineTransform& layerContentTransform)
{
    bool wasPaintingSVGResourceLayer = m_isPaintingSVGResourceLayer;
    m_isPaintingSVGResourceLayer = true;
    context.concatCTM(layerContentTransform);

    auto localPaintDirtyRect = LayoutRect::infiniteRect();

    CheckedPtr rootPaintingLayer = [&] () -> RenderLayer* {
        auto* curr = parent();
        while (curr && !(curr->renderer().isAnonymous() && is<RenderSVGViewportContainer>(curr->renderer())))
            curr = curr->parent();
        return curr;
    }();
    ASSERT(rootPaintingLayer);

    // If the viewport container has no layer, the walk ends at the SVG root.
    // Compensate for the content box offset (border + padding) so that resource
    // content paints at (0,0) in the ImageBuffer rather than at (borderLeft, borderTop).
    if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(rootPaintingLayer->renderer())) {
        auto contentOffset = svgRoot->contentBoxLocation();
        if (!contentOffset.isZero())
            context.translate(-FloatPoint(contentOffset));
    }

    LayerPaintingInfo paintingInfo(rootPaintingLayer, localPaintDirtyRect, PaintBehavior::Normal, LayoutSize());

    OptionSet<PaintLayerFlag> flags { PaintLayerFlag::TemporaryClipRects };
    if (!renderer().hasNonVisibleOverflow())
        flags.add({ PaintLayerFlag::PaintingOverflowContents, PaintLayerFlag::PaintingOverflowContentsRoot });

    paintLayer(context, paintingInfo, flags);

    m_isPaintingSVGResourceLayer = wasPaintingSVGResourceLayer;
}

void RenderLayerSVG::paintNegativeZOrderChildren(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags)
{
    // foreignObject uses HTML-style z-order painting.
    if (renderer().isRenderSVGForeignObject()) {
        RenderLayer::paintNegativeZOrderChildren(context, paintingInfo, paintFlags);
        return;
    }

    // SVG: no-op. Negative z-order children are handled in DOM-order painting
    // (paintSVGChildrenInDOMOrder handles all children including negative z-index).
}

void RenderLayerSVG::paintForegroundChildren(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, const LayerPaintingInfo& localPaintingInfo, OptionSet<PaintLayerFlag> paintFlags, const LayerFragments& layerFragments, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRoot)
{
    // foreignObject uses HTML-style z-order painting.
    if (renderer().isRenderSVGForeignObject()) {
        RenderLayer::paintForegroundChildren(context, paintingInfo, localPaintingInfo, paintFlags, layerFragments, paintBehavior, subtreePaintRoot);
        return;
    }

    bool hasSVGForegroundSplit = backing() && backing()->foregroundLayer() && !negativeZOrderLayers().size();
    if (hasSVGForegroundSplit) {
        // Paint non-composited children from the first composited child onwards into the foreground layer.
        paintSVGChildrenInDOMOrder(context, localPaintingInfo, paintFlags, layerFragments, paintBehavior, subtreePaintRoot, SVGChildPaintScope::FromFirstCompositedChild);
    } else
        paintSVGChildrenInDOMOrder(context, localPaintingInfo, paintFlags, layerFragments, paintBehavior, subtreePaintRoot);
}

void RenderLayerSVG::paintSVGForegroundSplitChildren(GraphicsContext& context, const LayerPaintingInfo& localPaintingInfo, OptionSet<PaintLayerFlag> paintFlags, const LayerFragments& layerFragments, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRoot)
{
    // foreignObject does not use SVG foreground split.
    if (renderer().isRenderSVGForeignObject())
        return;

    bool hasSVGForegroundSplit = backing() && backing()->foregroundLayer() && !negativeZOrderLayers().size();
    if (hasSVGForegroundSplit)
        paintSVGChildrenInDOMOrder(context, localPaintingInfo, paintFlags, layerFragments, paintBehavior, subtreePaintRoot, SVGChildPaintScope::BeforeFirstCompositedChild);
}

RenderLayer::HitLayer RenderLayerSVG::hitTestPositiveAndNormalFlowChildren(RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result, const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffsetForDescendants, bool depthSortDescendants, HitLayer& candidateLayer)
{
    // foreignObject uses HTML-style hit testing (its children are HTML content).
    if (renderer().isRenderSVGForeignObject())
        return RenderLayer::hitTestPositiveAndNormalFlowChildren(rootLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffsetForDescendants, depthSortDescendants, candidateLayer);

    // For SVGRoot, apply viewport clipping before testing children. The SVG root's
    // content box defines the visible area; points outside it should not hit SVG content.
    // This check was previously handled by RenderSVGRoot::nodeAtPoint(), but since we
    // now route SVGRoot through hitTestSVGChildrenInDOMOrder() to properly handle
    // non-layer transformed descendants, we need to apply the clip here.
    if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(renderer())) {
        auto contentBox = svgRoot->contentBoxRect();
        if (svgRoot->shouldApplyViewportClip() && !hitTestLocation.intersects(contentBox))
            return { };
    }

    // SVG hit tests children in DOM order, interleaving layer and non-layer children.
    // This handles non-layer transformed children that need inverse-transform hit testing.
    // SVGRoot also uses this path so that non-layer transformed descendants (e.g.,
    // <g transform="scale(200)">) are properly inverse-transform hit tested.
    return hitTestSVGChildrenInDOMOrder(rootLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffsetForDescendants);
}

RenderLayer::HitLayer RenderLayerSVG::hitTestNegativeZOrderChildren(RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result, const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffsetForDescendants, bool depthSortDescendants, HitLayer& candidateLayer)
{
    // foreignObject uses HTML-style hit testing.
    if (renderer().isRenderSVGForeignObject())
        return RenderLayer::hitTestNegativeZOrderChildren(rootLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffsetForDescendants, depthSortDescendants, candidateLayer);

    // SVG DOM-order hit testing already handled negative z-order children
    // in hitTestSVGChildrenInDOMOrder (called from hitTestPositiveAndNormalFlowChildren).
    return { };
}

void RenderLayerSVG::collectSVGChildrenInDOMOrder()
{
    m_svgChildrenInDOMOrderDirty = false;

    if (!m_svgChildrenInDOMOrder)
        m_svgChildrenInDOMOrder = makeUnique<Vector<SVGPaintOrderAwareChild>>();
    else
        m_svgChildrenInDOMOrder->clear();

    auto& allChildren = *m_svgChildrenInDOMOrder;

    // Recursively collect children, splitting non-layered containers that have
    // layered descendants to ensure proper DOM-order interleaving.
    auto collectChildren = [&allChildren](auto& self, RenderElement& parent, LayoutSize ancestorOffset) -> bool {
        bool foundLayeredChild = false;
        for (auto& child : childrenOfType<RenderElement>(parent)) {
            // Never directly paint children of <defs>, <linearGradient>, etc.
            if (child.isRenderSVGHiddenContainer())
                continue;

            if (child.hasSelfPaintingLayer()) {
                CheckedRef layerModelObject = downcast<RenderLayerModelObject>(child);
                CheckedPtr childLayer = layerModelObject->layer();
                allChildren.append({ &child, childLayer.get(), childLayer->zIndex(), false, { } });
                foundLayeredChild = true;
                continue;
            }

            // Transformed non-layer children are painted atomically: their transform
            // is applied and children painted recursively by paintSVGRendererByApplyingTransform().
            if (child.isTransformed()) {
                allChildren.append({ &child, nullptr, 0, false, ancestorOffset });
                // Transformed children require splitting the parent container, just like
                // layered children: they must remain in the collected list so that
                // paintSVGRendererByApplyingTransform() can apply their transform.
                // Without splitting, the parent would be painted atomically via
                // RenderSVGContainer::paint(), which does not apply SVG transforms.
                foundLayeredChild = true;
                continue;
            }

            // Leaf nodes (no children) are always painted atomically.
            if (!child.firstChild()) {
                allChildren.append({ &child, nullptr, 0, false, ancestorOffset });
                continue;
            }

            // Compute the offset that this child contributes to its descendants.
            LayoutSize childOffset = ancestorOffset;
            if (CheckedPtr svgModel = dynamicDowncast<RenderSVGModelObject>(child))
                childOffset += toLayoutSize(svgModel->currentSVGLayoutLocation());

            // Tentatively recurse into this non-layered container. If any layered
            // descendants are found, the container is split.
            size_t startIndex = allChildren.size();
            bool hasLayeredDescendants = self(self, child, childOffset);
            if (hasLayeredDescendants) {
                allChildren.append({ &child, nullptr, 0, true, ancestorOffset });
                foundLayeredChild = true;
            } else {
                allChildren.shrink(startIndex);
                allChildren.append({ &child, nullptr, 0, false, ancestorOffset });
            }
        }
        return foundLayeredChild;
    };
    collectChildren(collectChildren, renderer(), { });

    // Sort by z-index; for equal z-index, stable_sort preserves DOM order.
    std::stable_sort(allChildren.begin(), allChildren.end(),
        [](const SVGPaintOrderAwareChild& a, const SVGPaintOrderAwareChild& b) {
            return a.zIndex < b.zIndex;
        });

}

const Vector<SVGPaintOrderAwareChild>& RenderLayerSVG::svgChildrenInDOMOrder()
{
    if (m_svgChildrenInDOMOrderDirty)
        collectSVGChildrenInDOMOrder();

    ASSERT(m_svgChildrenInDOMOrder);
    return *m_svgChildrenInDOMOrder;
}

void RenderLayerSVG::paintNonLayerSVGChildForFragments(RenderElement& childRenderer, const LayoutSize& accumulatedAncestorOffset,
    PaintPhase phase, const LayerFragments& layerFragments, GraphicsContext& context, const LayerPaintingInfo& paintingInfo,
    OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRootForRenderer, const LayoutPoint& containerBaseOffset, bool isSVGRoot)
{
    for (const auto& fragment : layerFragments) {
        if (!fragment.shouldPaintContent || fragment.dirtyForegroundRect().isEmpty())
            continue;

        GraphicsContextStateSaver stateSaver(context, false);
        RegionContextStateSaver regionContextStateSaver(paintingInfo.regionContext);
        clipToRect(context, stateSaver, regionContextStateSaver, paintingInfo, paintBehavior, fragment.dirtyForegroundRect());

        PaintInfo paintInfo(context, fragment.dirtyForegroundRect().rect(),
            phase, paintBehavior, subtreePaintRootForRenderer,
            nullptr, nullptr, &paintingInfo.rootLayer->renderer(), this,
            paintingInfo.requireSecurityOriginAccessForWidgets);
        if (phase == PaintPhase::Foreground)
            paintInfo.overlapTestRequests = paintingInfo.overlapTestRequests;
        paintInfo.updateSubtreePaintRootForChildren(&renderer());

        auto containerPaintOffset = paintOffsetForRenderer(fragment, paintingInfo);
        auto childPaintOffset = containerBaseOffset.isZero() ? containerPaintOffset : containerPaintOffset + containerBaseOffset;
        if (isSVGRoot) {
            auto& svgRoot = downcast<RenderSVGRoot>(renderer());
            childPaintOffset.moveBy(LayoutPoint(-svgRoot.scrollPosition()));
        }

        auto finalOffset = childPaintOffset + accumulatedAncestorOffset;
        childRenderer.paint(paintInfo, finalOffset);
    }
}

void RenderLayerSVG::paintSVGChildrenInDOMOrder(GraphicsContext& context, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags,
    const LayerFragments& layerFragments, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRootForRenderer, SVGChildPaintScope paintScope)
{
    auto& allChildren = svgChildrenInDOMOrder();
    if (allChildren.isEmpty())
        return;

    bool isSVGRoot = is<RenderSVGRoot>(renderer());
    LayoutPoint containerBaseOffset;
    if (auto* svgModelObject = dynamicDowncast<RenderSVGModelObject>(renderer()))
        containerBaseOffset = svgModelObject->currentSVGLayoutLocation();
    else if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(renderer()))
        containerBaseOffset = svgRoot->location();

    size_t paintStart = 0;
    size_t paintEnd = allChildren.size();

    if (paintScope != SVGChildPaintScope::All) {
        size_t firstComposited = allChildren.size();
        for (size_t i = 0; i < allChildren.size(); ++i) {
            if (allChildren[i].layer && allChildren[i].layer->isComposited()) {
                firstComposited = i;
                break;
            }
        }

        if (paintScope == SVGChildPaintScope::BeforeFirstCompositedChild)
            paintEnd = firstComposited;
        else {
            ASSERT(paintScope == SVGChildPaintScope::FromFirstCompositedChild);
            paintStart = firstComposited;
        }
    }

    for (size_t i = paintStart; i < paintEnd; ++i) {
        auto& childToPaint = allChildren[i];
        if (CheckedPtr childLayer = childToPaint.layer) {
            if (paintScope != SVGChildPaintScope::All && childLayer->isComposited())
                continue;
            childLayer->paintLayer(context, paintingInfo, paintFlags);
            continue;
        }

        CheckedRef childRenderer = *childToPaint.renderer;

        // Transformed non-layer children: apply the transform and recursively paint the subtree.
        // Clip to the layer's foreground rect before entering the transform scope, so that
        // the parent layer's overflow clip (e.g., RenderSVGRoot viewport clip) is respected.
        if (childRenderer->isTransformed()) {
            for (const auto& fragment : layerFragments) {
                if (!fragment.shouldPaintContent || fragment.dirtyForegroundRect().isEmpty())
                    continue;

                GraphicsContextStateSaver stateSaver(context, false);
                RegionContextStateSaver regionContextStateSaver(paintingInfo.regionContext);
                clipToRect(context, stateSaver, regionContextStateSaver, paintingInfo, paintBehavior, fragment.dirtyForegroundRect());

                paintSVGRendererByApplyingTransform(context, childRenderer.get(),
                    childToPaint.accumulatedAncestorOffset, paintingInfo, paintFlags,
                    layerFragments, paintBehavior, subtreePaintRootForRenderer,
                    paintingInfo, AffineTransform());
            }
            continue;
        }

        if (childToPaint.selfOutlineOnly) {
            paintNonLayerSVGChildForFragments(childRenderer.get(), childToPaint.accumulatedAncestorOffset,
                PaintPhase::SelfOutline, layerFragments, context, paintingInfo, paintBehavior, subtreePaintRootForRenderer, containerBaseOffset, isSVGRoot);
            continue;
        }

        paintNonLayerSVGChildForFragments(childRenderer.get(), childToPaint.accumulatedAncestorOffset,
            PaintPhase::Foreground, layerFragments, context, paintingInfo, paintBehavior, subtreePaintRootForRenderer, containerBaseOffset, isSVGRoot);
        paintNonLayerSVGChildForFragments(childRenderer.get(), childToPaint.accumulatedAncestorOffset,
            PaintPhase::Outline, layerFragments, context, paintingInfo, paintBehavior, subtreePaintRootForRenderer, containerBaseOffset, isSVGRoot);
    }
}

std::optional<RenderLayerSVG::SVGRendererTransform> RenderLayerSVG::computeSVGRendererTransform(
    RenderElement& renderer, const LayoutSize& positionOffset) const
{
    CheckedRef layerModelObject = downcast<RenderLayerModelObject>(renderer);
    TransformationMatrix transform;
    auto& style = layerModelObject->style();
    auto referenceBoxRect = layerModelObject->transformReferenceBoxRect(style);

    // For non-layer renderers, undo the alignReferenceBox shift applied in transformReferenceBoxRect().
    // alignReferenceBox shifts the reference box by -nominalSVGLayoutLocation() to compensate for
    // offsetFromAncestor() in the layer path. Non-layer renderers don't use offsetFromAncestor(),
    // so the shift would incorrectly offset transform-origin.
    if (!renderer.hasSelfPaintingLayer()) {
        if (CheckedPtr svgModel = dynamicDowncast<RenderSVGModelObject>(&renderer))
            referenceBoxRect.moveBy(svgModel->nominalSVGLayoutLocation());
    }

    layerModelObject->applyTransform(transform, style, referenceBoxRect, Style::TransformResolver::allTransformOperations);

    // For the outermost viewport container (anonymous child of RenderSVGRoot), apply the
    // content-box origin offset (border+padding). Use translateRight (left-multiply) so
    // the offset is in CSS pixel space and NOT scaled by the viewBox transform.
    // This matches the layer painting path (paintLayerByApplyingTransform) which also uses
    // translateRight for positional offsets, and the screenCTM path (mapLocalToContainer)
    // which applies the offset via getTransformFromContainer as T(offset) * elementTransform.
    if (auto* viewportContainer = dynamicDowncast<RenderSVGViewportContainer>(renderer); viewportContainer && viewportContainer->isAnonymous()) {
        if (CheckedPtr svgRoot = dynamicDowncast<RenderSVGRoot>(viewportContainer->parent())) {
            auto contentBoxLocation = svgRoot->contentBoxLocation();
            if (!contentBoxLocation.isZero())
                transform.translateRight(contentBoxLocation.x(), contentBoxLocation.y());
        }
    }

    if (!transform.isInvertible())
        return std::nullopt;

    // Include position offset in the transform for renderers with layers, matching
    // paintLayerByApplyingTransform(). For non-layer renderers, do NOT add positionOffset
    // or currentSVGLayoutLocation: the SVG transform already encodes the correct position.
    bool isOutermostViewportContainer = is<RenderSVGViewportContainer>(renderer) && renderer.isAnonymous();
    LayoutSize containerOffset;
    if (renderer.hasSelfPaintingLayer() && !isOutermostViewportContainer) {
        containerOffset = positionOffset;
        if (auto* box = dynamicDowncast<RenderBox>(renderer))
            containerOffset += toLayoutSize(box->location());
        else if (auto* svgModel = dynamicDowncast<RenderSVGModelObject>(renderer))
            containerOffset += toLayoutSize(svgModel->currentSVGLayoutLocation());
    }

    transform.translateRight(containerOffset.width().toFloat(), containerOffset.height().toFloat());
    return SVGRendererTransform { transform, containerOffset };
}

void RenderLayerSVG::paintSVGRendererByApplyingTransform(GraphicsContext& context, CheckedRef<RenderElement> rendererToPaint,
    const LayoutSize& positionOffset, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags,
    const LayerFragments&, OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRoot,
    const LayerPaintingInfo& outerPaintingInfo, const AffineTransform& accumulatedTransform)
{
    auto result = computeSVGRendererTransform(rendererToPaint, positionOffset);
    if (!result)
        return;

    // Apply the transform and set up the transformed painting info via RAII scope.
    float deviceScaleFactor = rendererToPaint->document().deviceScaleFactor();

    // SVG renderers do not use pixel snapping — subpixelOffset must always be zero
    // within the SVG subtree (matching paintLayerByApplyingTransform() which explicitly
    // sets adjustedSubpixelOffset to zero for SVG via the rendererNeedsPixelSnapping() check).
    ASSERT(paintingInfo.subpixelOffset.isZero());
    LayoutSize adjustedSubpixelOffset;

    TransformPaintScope scope(context, paintingInfo, result->transform, deviceScaleFactor, adjustedSubpixelOffset);

    auto adjustedPaintFlags = paintFlags;
    adjustedPaintFlags.remove(PaintLayerFlag::PaintingOverflowContents);

    // Build up the accumulated non-layer transform for child layer painting.
    AffineTransform newAccumulatedTransform = accumulatedTransform;
    newAccumulatedTransform *= scope.appliedTransform();

    // Paint within the transformed coordinate system.
    if (rendererToPaint->hasSelfPaintingLayer()) {
        // The renderer has a layer (e.g., due to opacity, filters, etc.).
        // The combined transform (SVG transform + position offset) is already applied to the
        // context, and the dirty rect has been correctly inverse-mapped through it.
        // Use AppliedTransform so the layer skips its own transform+offset handling.
        CheckedRef layerModelObject = downcast<RenderLayerModelObject>(rendererToPaint.get());
        CheckedPtr childLayer = layerModelObject->layer();

        LayerPaintingInfo childPaintingInfo(childLayer.get(), scope.transformedPaintingInfo().paintDirtyRect, paintBehavior, LayoutSize());

        // Propagate accumulated non-layer ancestor transforms (e.g., scale(20) from a
        // non-layer parent <g>) so that paintLayerContents() can inverse-map clip rects
        // from the layer tree's coordinate space into the post-transform space. Without
        // this, ancestor overflow clips (e.g., SVG viewport) would be in untransformed
        // coordinates while the context is already scaled.
        if (!accumulatedTransform.isIdentity())
            childPaintingInfo.nonLayerSVGTransform = accumulatedTransform;

        childLayer->paintLayer(context, childPaintingInfo,
            adjustedPaintFlags | PaintLayerFlag::AppliedTransform);
    } else if (rendererToPaint->isRenderSVGContainer()) {
        // Container: recurse to paint children within the transform scope.
        // Pass the container's nominalSVGLayoutLocation as paintOffset so that children's
        // coordinateSystemOriginTranslation in paint() becomes zero. Without this, the
        // difference (currentSVGLayoutLocation - nominalSVGLayoutLocation = -parentNominal)
        // would shift all children by the parent's bounding box offset.
        //
        // For the outermost anonymous viewport container, use (0, 0) because the SVG
        // viewport coordinate system starts at the origin. The content box offset (border/
        // padding) is already encoded in the viewport container's transform.
        LayoutPoint containerPaintOffset;
        if (auto* viewportContainer = dynamicDowncast<RenderSVGViewportContainer>(rendererToPaint.get()); viewportContainer && viewportContainer->isAnonymous()) {
            // Outermost viewport container: coordinate system starts at (0, 0).
        } else if (auto* svgModel = dynamicDowncast<RenderSVGModelObject>(rendererToPaint.get()))
            containerPaintOffset = svgModel->nominalSVGLayoutLocation();
        paintSVGSubtreeWithinTransformScope(context, rendererToPaint.get(), containerPaintOffset, scope.transformedPaintingInfo(), adjustedPaintFlags, paintBehavior, subtreePaintRoot, outerPaintingInfo, newAccumulatedTransform);

        // Paint the container's own outline (e.g., for <g class="ring" transform="...">).
        // paintSVGSubtreeWithinTransformScope() only paints children, not the container itself.
        auto& transformedPaintingInfo = scope.transformedPaintingInfo();
        PaintInfo outlinePaintInfo(context, transformedPaintingInfo.paintDirtyRect, PaintPhase::SelfOutline, paintBehavior, subtreePaintRoot,
            nullptr, nullptr, &transformedPaintingInfo.rootLayer->renderer(), this,
            transformedPaintingInfo.requireSecurityOriginAccessForWidgets);
        rendererToPaint->paint(outlinePaintInfo, containerPaintOffset);
    } else {
        // Leaf renderer (rect, circle, path, text, etc.): paint it directly.
        // The transform is already applied to the graphics context.
        // Pass a paintOffset that compensates for the layout position offset so that
        // coordinateSystemOriginTranslation in paint() becomes zero.
        LayoutPoint leafPaintOffset;
        if (auto* svgModel = dynamicDowncast<RenderSVGModelObject>(rendererToPaint.get())) {
            leafPaintOffset = svgModel->nominalSVGLayoutLocation();
            leafPaintOffset.moveBy(-svgModel->currentSVGLayoutLocation());
        }
        auto& transformedPaintingInfo = scope.transformedPaintingInfo();
        LayoutRect dirtyRect = transformedPaintingInfo.paintDirtyRect;
        PaintInfo paintInfo(context, dirtyRect, PaintPhase::Foreground, paintBehavior, subtreePaintRoot,
            nullptr, nullptr, &transformedPaintingInfo.rootLayer->renderer(), this,
            transformedPaintingInfo.requireSecurityOriginAccessForWidgets);
        rendererToPaint->paint(paintInfo, leafPaintOffset);
        PaintInfo outlinePaintInfo(paintInfo);
        outlinePaintInfo.phase = PaintPhase::Outline;
        rendererToPaint->paint(outlinePaintInfo, leafPaintOffset);
    }
}

void RenderLayerSVG::paintSVGSubtreeWithinTransformScope(GraphicsContext& context, RenderElement& container,
    const LayoutPoint& paintOffset, const LayerPaintingInfo& paintingInfo, OptionSet<PaintLayerFlag> paintFlags,
    OptionSet<PaintBehavior> paintBehavior, RenderObject* subtreePaintRoot,
    const LayerPaintingInfo& outerPaintingInfo, const AffineTransform& accumulatedTransform)
{
    // Apply viewport clipping for nested <svg> elements without layers.
    GraphicsContextStateSaver clipSaver(context, false);
    if (CheckedPtr viewportContainer = dynamicDowncast<RenderSVGViewportContainer>(container)) {
        if (!viewportContainer->hasSelfPaintingLayer() && SVGRenderSupport::isOverflowHidden(*viewportContainer)) {
            clipSaver.save();
            // When the container's transform has been applied to the context (via
            // paintSVGRendererByApplyingTransform), the clip rect must be in post-transform
            // coordinates. Pass LayoutPoint() since the position is encoded in the transform.
            // For non-transformed containers (recursive call), use paintOffset for positioning.
            auto clipOffset = viewportContainer->isTransformed() ? LayoutPoint() : paintOffset;
            context.clip(FloatRect(static_cast<const RenderSVGModelObject&>(*viewportContainer).overflowClipRect(clipOffset)));
        }
    }

    // Collect children and sort by z-index so that z-indexed SVG elements within
    // non-layer transformed containers (e.g., viewport containers with viewBox) are
    // painted in stacking order rather than DOM order.
    Vector<SVGPaintOrderAwareChild> sortedChildren;
    for (auto& child : childrenOfType<RenderElement>(container)) {
        if (child.isRenderSVGHiddenContainer())
            continue;
        if (child.style().display() == Style::DisplayType::None)
            continue;

        int zIndex = 0;
        if (child.hasSelfPaintingLayer()) {
            CheckedRef layerModelObject = downcast<RenderLayerModelObject>(child);
            if (CheckedPtr childLayer = layerModelObject->layer())
                zIndex = childLayer->zIndex();
        }
        sortedChildren.append({ &child, nullptr, zIndex, false, { } });
    }

    std::stable_sort(sortedChildren.begin(), sortedChildren.end(),
        [](const SVGPaintOrderAwareChild& a, const SVGPaintOrderAwareChild& b) {
            return a.zIndex < b.zIndex;
        });

    for (auto& entry : sortedChildren) {
        auto& child = *entry.renderer;

        // Handle transformed children first (regardless of layer status).
        // paintSVGRendererByApplyingTransform handles both layer and non-layer children:
        // it applies the transform via concatCTM (same code path for both), then either
        // delegates to the layer with AppliedTransform, or paints directly.
        // This ensures paint() receives an identical paintOffset in both cases.
        if (child.isTransformed()) {
            paintSVGRendererByApplyingTransform(context, child, toLayoutSize(paintOffset), paintingInfo, paintFlags,
                { }, paintBehavior, subtreePaintRoot, outerPaintingInfo, accumulatedTransform);
            continue;
        }

        // Non-transformed child with a layer (e.g., due to opacity, clip-path).
        if (child.hasSelfPaintingLayer()) {
            CheckedRef layerModelObject = downcast<RenderLayerModelObject>(child);
            CheckedPtr childLayer = layerModelObject->layer();

            auto adjustedFlags = paintFlags;
            adjustedFlags.remove(PaintLayerFlag::PaintingOverflowContents);

            // Propagate the accumulated non-layer SVG transform so that clip rects
            // computed in paintLayerContents() can be mapped from rootLayer's
            // coordinate space to the current (post-non-layer-transform) space.
            RenderLayer::LayerPaintingInfo childPaintingInfo(paintingInfo);
            if (!accumulatedTransform.isIdentity()) {
                childPaintingInfo.nonLayerSVGTransform = accumulatedTransform;
                // FIXME: If the child is composited, the compositor doesn't know about
                // the non-layer SVG ancestor transform, that support is not complete yet.
            }
            childLayer->paintLayer(context, childPaintingInfo, adjustedFlags);
            continue;
        }

        // Non-layer, non-transformed child.
        auto adjustedPaintOffset = paintOffset;
        if (CheckedPtr childSvgModel = dynamicDowncast<RenderSVGModelObject>(child))
            adjustedPaintOffset.moveBy(childSvgModel->currentSVGLayoutLocation());

        if (child.isRenderSVGContainer()) {
            paintSVGSubtreeWithinTransformScope(context, child, adjustedPaintOffset, paintingInfo, paintFlags, paintBehavior, subtreePaintRoot, outerPaintingInfo, accumulatedTransform);

            // Paint the container's own outline after its children.
            PaintInfo outlinePaintInfo(context, paintingInfo.paintDirtyRect, PaintPhase::SelfOutline, paintBehavior, subtreePaintRoot,
                nullptr, nullptr, &paintingInfo.rootLayer->renderer(), this,
                paintingInfo.requireSecurityOriginAccessForWidgets);
            child.paint(outlinePaintInfo, paintOffset);
        } else {
            LayoutRect dirtyRect = paintingInfo.paintDirtyRect;
            PaintInfo paintInfo(context, dirtyRect, PaintPhase::Foreground, paintBehavior, subtreePaintRoot,
                nullptr, nullptr, &paintingInfo.rootLayer->renderer(), this,
                paintingInfo.requireSecurityOriginAccessForWidgets);
            child.paint(paintInfo, paintOffset);

            PaintInfo outlinePaintInfo(paintInfo);
            outlinePaintInfo.phase = PaintPhase::Outline;
            child.paint(outlinePaintInfo, paintOffset);
        }
    }
}

RenderLayer::HitLayer RenderLayerSVG::hitTestSVGChildrenInDOMOrder(RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffsetForDescendants)
{
    auto& allChildren = svgChildrenInDOMOrder();
    if (allChildren.isEmpty())
        return { };

    CheckedPtr svgModelObject = dynamicDowncast<RenderSVGModelObject>(renderer());

    // Hit test in reverse order (front-to-back).
    for (int i = allChildren.size() - 1; i >= 0; --i) {
        auto& childToPaint = allChildren[i];

        if (childToPaint.selfOutlineOnly)
            continue;

        if (CheckedPtr childLayer = childToPaint.layer) {
            auto hitLayer = childLayer->hitTestLayer(rootLayer, this, request, result, hitTestRect, hitTestLocation, false, transformState, zOffsetForDescendants);
            if (hitLayer.layer)
                return hitLayer;
            continue;
        }

        CheckedRef childRenderer = *childToPaint.renderer;

        // Transformed non-layer children: inverse-map the hit location and recursively hit test.
        if (childRenderer->isTransformed()) {
            auto hitLayer = hitTestSVGRendererByInversingTransform(childRenderer.get(), childToPaint.accumulatedAncestorOffset, rootLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffsetForDescendants, hitTestRect, hitTestLocation);
            if (hitLayer.layer)
                return hitLayer;
            continue;
        }

        // The hit test coordinates are layer-local (after transform inversion).
        // Pass nominalSVGLayoutLocation() as the accumulatedOffset so that children's
        // coordinateSystemOriginTranslation in nodeAtPoint() maps the layer-local point
        // into each child's local coordinate system. Do NOT shift the hit test location
        // itself, as the layer-local coordinates are already in the correct space.
        LayoutSize svgOffset;
        if (svgModelObject)
            svgOffset = toLayoutSize(svgModelObject->nominalSVGLayoutLocation());

        LayoutPoint accumulatedOffset(svgOffset);
        accumulatedOffset += childToPaint.accumulatedAncestorOffset;

        if (childRenderer->nodeAtPoint(request, result, hitTestLocation, accumulatedOffset, HitTestForeground))
            return { this, 0 };
    }

    return { };
}

RenderLayer::HitLayer RenderLayerSVG::hitTestSVGRendererByInversingTransform(RenderElement& rendererToTest,
    const LayoutSize& positionOffset, RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation,
    const HitTestingTransformState* transformState, double* zOffsetForDescendants,
    const LayoutRect& outerHitTestRect, const HitTestLocation& outerHitTestLocation)
{
    auto transformResult = computeSVGRendererTransform(rendererToTest, positionOffset);
    if (!transformResult)
        return { };

    // Inverse-map the hit test location through the transform.
    auto inverse = transformResult->transform.inverse();
    if (!inverse)
        return { };

    auto localPoint = inverse->mapPoint(hitTestLocation.point());
    auto localRect = enclosingLayoutRect(inverse->mapRect(FloatRect(hitTestRect)));

    HitTestLocation localLocation { LayoutPoint { localPoint } };

    // We've inverse-mapped through the full transform (renderer's transform + position).
    // For both layer and non-layer children, test content using the local coordinates.
    // For layer children, calling hitTestLayer() would double-inverse the transform,
    // so we test directly — matching the non-layer hit testing path exactly.
    //
    // The accumulatedOffset must be set so that children's coordinateSystemOriginTranslation
    // in nodeAtPoint() (= nominalSVGLayoutLocation - adjustedLocation) evaluates to zero,
    // since the inverse transform already maps the hit test point into the correct local
    // coordinate system. For containers, children need the container's nominalSVGLayoutLocation
    // as the base offset. For leaf renderers, the parent's nominalSVGLayoutLocation is needed.
    if (rendererToTest.isRenderSVGContainer()) {
        // For the outermost anonymous viewport container, use (0, 0) because the SVG
        // viewport coordinate system starts at the origin. The content box offset (border/
        // padding) is already encoded in the viewport container's transform.
        LayoutPoint accumulatedOffset;
        if (auto* viewportContainer = dynamicDowncast<RenderSVGViewportContainer>(rendererToTest); viewportContainer && viewportContainer->isAnonymous()) {
            // Outermost viewport container: coordinate system starts at (0, 0).
        } else if (auto* svgModel = dynamicDowncast<RenderSVGModelObject>(rendererToTest))
            accumulatedOffset = svgModel->nominalSVGLayoutLocation();
        return hitTestSVGSubtreeWithinTransformScope(rendererToTest, accumulatedOffset, rootLayer, request, result, localRect, localLocation, transformState, zOffsetForDescendants, outerHitTestRect, outerHitTestLocation);
    }

    // Leaf renderer (rect, circle, path, text, etc.): hit test it directly.
    LayoutPoint accumulatedOffset;
    if (CheckedPtr parentRenderer = rendererToTest.parent()) {
        if (CheckedPtr parentSvgModel = dynamicDowncast<RenderSVGModelObject>(*parentRenderer))
            accumulatedOffset = parentSvgModel->nominalSVGLayoutLocation();
    }
    if (rendererToTest.nodeAtPoint(request, result, localLocation, accumulatedOffset, HitTestForeground))
        return { this, 0 };
    return { };
}

RenderLayer::HitLayer RenderLayerSVG::hitTestSVGSubtreeWithinTransformScope(RenderElement& container,
    const LayoutPoint& accumulatedOffset, RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation,
    const HitTestingTransformState* transformState, double* zOffsetForDescendants,
    const LayoutRect& outerHitTestRect, const HitTestLocation& outerHitTestLocation)
{
    // Hit test in reverse DOM order (front-to-back).
    for (CheckedPtr child = container.lastChild(); child; child = child->previousSibling()) {
        CheckedPtr childElement = dynamicDowncast<RenderElement>(child.get());
        if (!childElement)
            continue;

        if (childElement->isRenderSVGHiddenContainer())
            continue;

        if (childElement->style().display() == Style::DisplayType::None)
            continue;

        // Handle transformed children first (regardless of layer status).
        // hitTestSVGRendererByInversingTransform handles both layer and non-layer children:
        // it inverse-maps the hit location through the transform, then either delegates to
        // the layer or tests the renderer directly.
        if (childElement->isTransformed()) {
            auto hitLayer = hitTestSVGRendererByInversingTransform(*childElement, toLayoutSize(accumulatedOffset), rootLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffsetForDescendants, outerHitTestRect, outerHitTestLocation);
            if (hitLayer.layer)
                return hitLayer;
            continue;
        }

        // Non-transformed child with a layer: test directly using nodeAtPoint(),
        // same as the non-layer path. Using hitTestLayer() would compute wrong
        // offsets via offsetFromAncestor() within a non-layer transform scope.
        if (childElement->hasSelfPaintingLayer()) {
            if (childElement->nodeAtPoint(request, result, hitTestLocation, accumulatedOffset, HitTestForeground))
                return { this, 0 };
            continue;
        }

        // Non-layer, non-transformed child.
        auto adjustedOffset = accumulatedOffset;
        if (CheckedPtr childSvgModel = dynamicDowncast<RenderSVGModelObject>(*childElement))
            adjustedOffset.moveBy(childSvgModel->currentSVGLayoutLocation());

        if (childElement->isRenderSVGContainer()) {
            // Check viewport clipping for nested <svg> elements without layers (mirrors the painting path).
            if (CheckedPtr viewportContainer = dynamicDowncast<RenderSVGViewportContainer>(*childElement)) {
                if (SVGRenderSupport::isOverflowHidden(*viewportContainer) && !viewportContainer->viewport().contains(hitTestLocation.point()))
                    continue;
            }
            auto hitLayer = hitTestSVGSubtreeWithinTransformScope(*childElement, adjustedOffset, rootLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffsetForDescendants, outerHitTestRect, outerHitTestLocation);
            if (hitLayer.layer)
                return hitLayer;
        } else {
            if (childElement->nodeAtPoint(request, result, hitTestLocation, accumulatedOffset, HitTestForeground))
                return { this, 0 };
        }
    }

    return { };
}

} // namespace WebCore
