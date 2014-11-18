/*
 * Copyright (C) 2006-2014 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
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
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "RenderLayer.h"

#include "AnimationController.h"
#include "BoxShape.h"
#include "CSSPropertyNames.h"
#include "Chrome.h"
#include "Document.h"
#include "DocumentEventQueue.h"
#include "Element.h"
#include "EventHandler.h"
#include "FloatConversion.h"
#include "FloatPoint3D.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "FlowThreadController.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameSelection.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "HitTestingTransformState.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InspectorInstrumentation.h"
#include "OverflowEvent.h"
#include "OverlapTestRequestClient.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "RenderFlowThread.h"
#include "RenderGeometryMap.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderMarquee.h"
#include "RenderMultiColumnFlowThread.h"
#include "RenderNamedFlowFragment.h"
#include "RenderNamedFlowThread.h"
#include "RenderRegion.h"
#include "RenderReplica.h"
#include "RenderSVGResourceClipper.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarPart.h"
#include "RenderTableCell.h"
#include "RenderTableRow.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "SVGNames.h"
#include "ScaleTransformOperation.h"
#include "ScrollAnimator.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SourceGraphic.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "TextStream.h"
#include "TransformationMatrix.h"
#include "TranslateTransformOperation.h"
#include <stdio.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

#if ENABLE(CSS_FILTERS)
#include "FEColorMatrix.h"
#include "FEMerge.h"
#include "FilterEffectRenderer.h"
#include "RenderLayerFilterInfo.h"
#endif

#define MIN_INTERSECT_FOR_REVEAL 32

namespace WebCore {

using namespace HTMLNames;

bool ClipRect::intersects(const HitTestLocation& hitTestLocation) const
{
    return hitTestLocation.intersects(m_rect);
}

void makeMatrixRenderable(TransformationMatrix& matrix, bool has3DRendering)
{
#if !ENABLE(3D_RENDERING)
    UNUSED_PARAM(has3DRendering);
    matrix.makeAffine();
#else
    if (!has3DRendering)
        matrix.makeAffine();
#endif
}

RenderLayer::RenderLayer(RenderLayerModelObject& rendererLayerModelObject)
    : m_inResizeMode(false)
    , m_scrollDimensionsDirty(true)
    , m_normalFlowListDirty(true)
    , m_hasSelfPaintingLayerDescendant(false)
    , m_hasSelfPaintingLayerDescendantDirty(false)
    , m_hasOutOfFlowPositionedDescendant(false)
    , m_hasOutOfFlowPositionedDescendantDirty(true)
    , m_needsCompositedScrolling(false)
    , m_descendantsAreContiguousInStackingOrder(false)
    , m_isRootLayer(rendererLayerModelObject.isRenderView())
    , m_usedTransparency(false)
    , m_paintingInsideReflection(false)
    , m_inOverflowRelayout(false)
    , m_repaintStatus(NeedsNormalRepaint)
    , m_visibleContentStatusDirty(true)
    , m_hasVisibleContent(false)
    , m_visibleDescendantStatusDirty(false)
    , m_hasVisibleDescendant(false)
    , m_3DTransformedDescendantStatusDirty(true)
    , m_has3DTransformedDescendant(false)
    , m_hasCompositingDescendant(false)
    , m_hasTransformedAncestor(false)
    , m_has3DTransformedAncestor(false)
    , m_indirectCompositingReason(static_cast<unsigned>(IndirectCompositingReason::None))
    , m_viewportConstrainedNotCompositedReason(NoNotCompositedReason)
#if PLATFORM(IOS)
    , m_adjustForIOSCaretWhenScrolling(false)
    , m_registeredAsTouchEventListenerForScrolling(false)
    , m_inUserScroll(false)
    , m_requiresScrollBoundsOriginUpdate(false)
#endif
    , m_containsDirtyOverlayScrollbars(false)
    , m_updatingMarqueePosition(false)
#if !ASSERT_DISABLED
    , m_layerListMutationAllowed(true)
#endif
#if ENABLE(CSS_FILTERS)
    , m_hasFilterInfo(false)
#endif
#if ENABLE(CSS_COMPOSITING)
    , m_blendMode(BlendModeNormal)
    , m_hasNotIsolatedCompositedBlendingDescendants(false)
    , m_hasNotIsolatedBlendingDescendants(false)
    , m_hasNotIsolatedBlendingDescendantsStatusDirty(false)
#endif
    , m_renderer(rendererLayerModelObject)
    , m_parent(0)
    , m_previous(0)
    , m_next(0)
    , m_first(0)
    , m_last(0)
    , m_staticInlinePosition(0)
    , m_staticBlockPosition(0)
    , m_enclosingPaginationLayer(0)
{
    m_isNormalFlowOnly = shouldBeNormalFlowOnly();
    m_isSelfPaintingLayer = shouldBeSelfPaintingLayer();

    // Non-stacking containers should have empty z-order lists. As this is already the case,
    // there is no need to dirty / recompute these lists.
    m_zOrderListsDirty = isStackingContainer();

    ScrollableArea::setConstrainsScrollingToContentEdge(false);

    if (!renderer().firstChild()) {
        m_visibleContentStatusDirty = false;
        m_hasVisibleContent = renderer().style().visibility() == VISIBLE;
    }

    if (Element* element = renderer().element()) {
        // We save and restore only the scrollOffset as the other scroll values are recalculated.
        m_scrollOffset = element->savedLayerScrollOffset();
        if (!m_scrollOffset.isZero())
            scrollAnimator()->setCurrentPosition(FloatPoint(m_scrollOffset.width(), m_scrollOffset.height()));
        element->setSavedLayerScrollOffset(IntSize());
    }
}

RenderLayer::~RenderLayer()
{
    if (inResizeMode() && !renderer().documentBeingDestroyed())
        renderer().frame().eventHandler().resizeLayerDestroyed();

    renderer().view().frameView().removeScrollableArea(this);

    if (!renderer().documentBeingDestroyed()) {
#if PLATFORM(IOS)
        unregisterAsTouchEventListenerForScrolling();
#endif
        if (Element* element = renderer().element())
            element->setSavedLayerScrollOffset(m_scrollOffset);
    }

    destroyScrollbar(HorizontalScrollbar);
    destroyScrollbar(VerticalScrollbar);

    if (renderer().frame().page()) {
        if (ScrollingCoordinator* scrollingCoordinator = renderer().frame().page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyScrollableArea(this);
    }

    if (m_reflection)
        removeReflection();
    
#if ENABLE(CSS_FILTERS)
    FilterInfo::remove(*this);
#endif

    // Child layers will be deleted by their corresponding render objects, so
    // we don't need to delete them ourselves.

    clearBacking(true);
}

String RenderLayer::name() const
{
    StringBuilder name;
    name.append(renderer().renderName());

    if (Element* element = renderer().element()) {
        name.append(' ');
        name.append(element->tagName());

        if (element->hasID()) {
            name.appendLiteral(" id=\'");
            name.append(element->getIdAttribute());
            name.append('\'');
        }

        if (element->hasClass()) {
            name.appendLiteral(" class=\'");
            for (size_t i = 0; i < element->classNames().size(); ++i) {
                if (i > 0)
                    name.append(' ');
                name.append(element->classNames()[i]);
            }
            name.append('\'');
        }
    }

    if (isReflection())
        name.appendLiteral(" (reflection)");

    return name.toString();
}

RenderLayerCompositor& RenderLayer::compositor() const
{
    return renderer().view().compositor();
}

void RenderLayer::contentChanged(ContentChangeType changeType)
{
    if ((changeType == CanvasChanged || changeType == VideoChanged || changeType == FullScreenChanged || changeType == ImageChanged) && compositor().updateLayerCompositingState(*this))
        compositor().setCompositingLayersNeedRebuild();

    if (m_backing)
        m_backing->contentChanged(changeType);
}

bool RenderLayer::canRender3DTransforms() const
{
    return compositor().canRender3DTransforms();
}

#if ENABLE(CSS_FILTERS)

bool RenderLayer::paintsWithFilters() const
{
    // FIXME: Eventually there will be cases where we paint with filters even without accelerated compositing,
    // and this whole function won't be inside the #if below.

    if (!renderer().hasFilter())
        return false;
        
    if (!isComposited())
        return true;

    if (!m_backing || !m_backing->canCompositeFilters())
        return true;

    return false;
}

bool RenderLayer::requiresFullLayerImageForFilters() const 
{
    if (!paintsWithFilters())
        return false;
    FilterEffectRenderer* renderer = filterRenderer();
    return renderer && renderer->hasFilterThatMovesPixels();
}

FilterEffectRenderer* RenderLayer::filterRenderer() const
{
    FilterInfo* filterInfo = FilterInfo::getIfExists(*this);
    return filterInfo ? filterInfo->renderer() : 0;
}

#endif

void RenderLayer::updateLayerPositionsAfterLayout(const RenderLayer* rootLayer, UpdateLayerPositionsFlags flags)
{
    RenderGeometryMap geometryMap(UseTransforms);
    if (this != rootLayer)
        geometryMap.pushMappingsToAncestor(parent(), 0);
    updateLayerPositions(&geometryMap, flags);
}

void RenderLayer::updateLayerPositions(RenderGeometryMap* geometryMap, UpdateLayerPositionsFlags flags)
{
    updateLayerPosition(); // For relpositioned layers or non-positioned layers,
                           // we need to keep in sync, since we may have shifted relative
                           // to our parent layer.
    if (geometryMap)
        geometryMap->pushMappingsToAncestor(this, parent());

    // Clear our cached clip rect information.
    clearClipRects();
    
    if (hasOverflowControls()) {
        LayoutSize offsetFromRoot;
        if (geometryMap)
            offsetFromRoot = LayoutSize(toFloatSize(geometryMap->absolutePoint(FloatPoint())));
        else {
            // FIXME: It looks suspicious to call convertToLayerCoords here
            // as canUseConvertToLayerCoords may be true for an ancestor layer.
            offsetFromRoot = offsetFromAncestor(root());
        }
        positionOverflowControls(roundedIntSize(offsetFromRoot));
    }

    updateDescendantDependentFlags();

    if (flags & UpdatePagination)
        updatePagination();
    else
        m_enclosingPaginationLayer = nullptr;
    
    if (m_hasVisibleContent) {
        // FIXME: LayoutState does not work with RenderLayers as there is not a 1-to-1
        // mapping between them and the RenderObjects. It would be neat to enable
        // LayoutState outside the layout() phase and use it here.
        ASSERT(!renderer().view().layoutStateEnabled());

        RenderLayerModelObject* repaintContainer = renderer().containerForRepaint();
        LayoutRect oldRepaintRect = m_repaintRect;
        LayoutRect oldOutlineBox = m_outlineBox;
        computeRepaintRects(repaintContainer, geometryMap);

        // FIXME: Should ASSERT that value calculated for m_outlineBox using the cached offset is the same
        // as the value not using the cached offset, but we can't due to https://bugs.webkit.org/show_bug.cgi?id=37048
        if (flags & CheckForRepaint) {
            if (!renderer().view().printing()) {
                bool didRepaint = false;
                if (m_repaintStatus & NeedsFullRepaint) {
                    renderer().repaintUsingContainer(repaintContainer, oldRepaintRect);
                    if (m_repaintRect != oldRepaintRect) {
                        renderer().repaintUsingContainer(repaintContainer, m_repaintRect);
                        didRepaint = true;
                    }
                } else if (shouldRepaintAfterLayout()) {
                    renderer().repaintAfterLayoutIfNeeded(repaintContainer, oldRepaintRect, oldOutlineBox, &m_repaintRect, &m_outlineBox);
                    didRepaint = true;
                }

                if (didRepaint && renderer().isRenderNamedFlowFragmentContainer()) {
                    // If we just repainted a region, we must also repaint the flow thread since it is the one
                    // doing the actual painting of the flowed content.
                    RenderNamedFlowFragment* region = toRenderBlockFlow(&renderer())->renderNamedFlowFragment();
                    if (region->isValid())
                        region->flowThread()->layer()->repaintIncludingDescendants();
                }
            }
        }
    } else
        clearRepaintRects();

    m_repaintStatus = NeedsNormalRepaint;
    m_hasTransformedAncestor = flags & SeenTransformedLayer;
    m_has3DTransformedAncestor = flags & Seen3DTransformedLayer;

    // Go ahead and update the reflection's position and size.
    if (m_reflection)
        m_reflection->layout();

    // Clear the IsCompositingUpdateRoot flag once we've found the first compositing layer in this update.
    bool isUpdateRoot = (flags & IsCompositingUpdateRoot);
    if (isComposited())
        flags &= ~IsCompositingUpdateRoot;

    if (renderer().isInFlowRenderFlowThread()) {
        updatePagination();
        flags |= UpdatePagination;
    }

    if (transform()) {
        flags |= SeenTransformedLayer;
        if (!transform()->isAffine())
            flags |= Seen3DTransformedLayer;
    }

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositions(geometryMap, flags);

    if ((flags & UpdateCompositingLayers) && isComposited()) {
        RenderLayerBacking::UpdateAfterLayoutFlags updateFlags = RenderLayerBacking::CompositingChildrenOnly;
        if (flags & NeedsFullRepaintInBacking)
            updateFlags |= RenderLayerBacking::NeedsFullRepaint;
        if (isUpdateRoot)
            updateFlags |= RenderLayerBacking::IsUpdateRoot;
        backing()->updateAfterLayout(updateFlags);
    }
        
    // With all our children positioned, now update our marquee if we need to.
    if (m_marquee) {
        // FIXME: would like to use TemporaryChange<> but it doesn't work with bitfields.
        bool oldUpdatingMarqueePosition = m_updatingMarqueePosition;
        m_updatingMarqueePosition = true;
        m_marquee->updateMarqueePosition();
        m_updatingMarqueePosition = oldUpdatingMarqueePosition;
    }

    if (geometryMap)
        geometryMap->popMappingsToAncestor(parent());
}

LayoutRect RenderLayer::repaintRectIncludingNonCompositingDescendants() const
{
    LayoutRect repaintRect = m_repaintRect;
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        // Don't include repaint rects for composited child layers; they will paint themselves and have a different origin.
        if (child->isComposited())
            continue;

        repaintRect.unite(child->repaintRectIncludingNonCompositingDescendants());
    }
    return repaintRect;
}

void RenderLayer::setAncestorChainHasSelfPaintingLayerDescendant()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (!layer->m_hasSelfPaintingLayerDescendantDirty && layer->hasSelfPaintingLayerDescendant())
            break;

        layer->m_hasSelfPaintingLayerDescendantDirty = false;
        layer->m_hasSelfPaintingLayerDescendant = true;
    }
}

void RenderLayer::dirtyAncestorChainHasSelfPaintingLayerDescendantStatus()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        layer->m_hasSelfPaintingLayerDescendantDirty = true;
        // If we have reached a self-painting layer, we know our parent should have a self-painting descendant
        // in this case, there is no need to dirty our ancestors further.
        if (layer->isSelfPaintingLayer()) {
            ASSERT(!parent() || parent()->m_hasSelfPaintingLayerDescendantDirty || parent()->hasSelfPaintingLayerDescendant());
            break;
        }
    }
}

bool RenderLayer::acceleratedCompositingForOverflowScrollEnabled() const
{
    return renderer().frame().settings().acceleratedCompositingForOverflowScrollEnabled();
}

// If we are a stacking container, then this function will determine if our
// descendants for a contiguous block in stacking order. This is required in
// order for an element to be safely promoted to a stacking container. It is safe
// to become a stacking container if this change would not alter the stacking
// order of layers on the page. That can only happen if a non-descendant appear
// between us and our descendants in stacking order. Here's an example:
//
//                                 this
//                                /  |  \.
//                               A   B   C
//                              /\   |   /\.
//                             0 -8  D  2  7
//                                   |
//                                   5
//
// I've labeled our normal flow descendants A, B, C, and D, our stacking
// container descendants with their z indices, and us with 'this' (we're a
// stacking container and our zIndex doesn't matter here). These nodes appear in
// three lists: posZOrder, negZOrder, and normal flow (keep in mind that normal
// flow layers don't overlap). So if we arrange these lists in order we get our
// stacking order:
//
//                     [-8], [A-D], [0, 2, 5, 7]--> pos z-order.
//                       |     |
//        Neg z-order. <-+     +--> Normal flow descendants.
//
// We can then assign new, 'stacking' order indices to these elements as follows:
//
//                     [-8], [A-D], [0, 2, 5, 7]
// 'Stacking' indices:  -1     0     1  2  3  4
//
// Note that the normal flow descendants can share an index because they don't
// stack/overlap. Now our problem becomes very simple: a layer can safely become
// a stacking container if the stacking-order indices of it and its descendants
// appear in a contiguous block in the list of stacking indices. This problem
// can be solved very efficiently by calculating the min/max stacking indices in
// the subtree, and the number stacking container descendants. Once we have this
// information, we know that the subtree's indices form a contiguous block if:
//
//           maxStackIndex - minStackIndex == numSCDescendants
//
// So for node A in the example above we would have:
//   maxStackIndex = 1
//   minStackIndex = -1
//   numSCDecendants = 2
//
// and so,
//       maxStackIndex - minStackIndex == numSCDescendants
//  ===>                      1 - (-1) == 2
//  ===>                             2 == 2
//
//  Since this is true, A can safely become a stacking container.
//  Now, for node C we have:
//
//   maxStackIndex = 4
//   minStackIndex = 0 <-- because C has stacking index 0.
//   numSCDecendants = 2
//
// and so,
//       maxStackIndex - minStackIndex == numSCDescendants
//  ===>                         4 - 0 == 2
//  ===>                             4 == 2
//
// Since this is false, C cannot be safely promoted to a stacking container. This
// happened because of the elements with z-index 5 and 0. Now if 5 had been a
// child of C rather than D, and A had no child with Z index 0, we would have had:
//
//   maxStackIndex = 3
//   minStackIndex = 0 <-- because C has stacking index 0.
//   numSCDecendants = 3
//
// and so,
//       maxStackIndex - minStackIndex == numSCDescendants
//  ===>                         3 - 0 == 3
//  ===>                             3 == 3
//
//  And we would conclude that C could be promoted.
void RenderLayer::updateDescendantsAreContiguousInStackingOrder()
{
    if (!isStackingContext() || !acceleratedCompositingForOverflowScrollEnabled())
        return;

    ASSERT(!m_normalFlowListDirty);
    ASSERT(!m_zOrderListsDirty);

    std::unique_ptr<Vector<RenderLayer*>> posZOrderList;
    std::unique_ptr<Vector<RenderLayer*>> negZOrderList;
    rebuildZOrderLists(StopAtStackingContexts, posZOrderList, negZOrderList);

    // Create a reverse lookup.
    HashMap<const RenderLayer*, int> lookup;

    if (negZOrderList) {
        int stackingOrderIndex = -1;
        size_t listSize = negZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* currentLayer = negZOrderList->at(listSize - i - 1);
            if (!currentLayer->isStackingContext())
                continue;
            lookup.set(currentLayer, stackingOrderIndex--);
        }
    }

    if (posZOrderList) {
        size_t listSize = posZOrderList->size();
        int stackingOrderIndex = 1;
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* currentLayer = posZOrderList->at(i);
            if (!currentLayer->isStackingContext())
                continue;
            lookup.set(currentLayer, stackingOrderIndex++);
        }
    }

    int minIndex = 0;
    int maxIndex = 0;
    int count = 0;
    bool firstIteration = true;
    updateDescendantsAreContiguousInStackingOrderRecursive(lookup, minIndex, maxIndex, count, firstIteration);
}

void RenderLayer::updateDescendantsAreContiguousInStackingOrderRecursive(const HashMap<const RenderLayer*, int>& lookup, int& minIndex, int& maxIndex, int& count, bool firstIteration)
{
    if (isStackingContext() && !firstIteration) {
        if (lookup.contains(this)) {
            minIndex = std::min(minIndex, lookup.get(this));
            maxIndex = std::max(maxIndex, lookup.get(this));
            count++;
        }
        return;
    }

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        int childMinIndex = 0;
        int childMaxIndex = 0;
        int childCount = 0;
        child->updateDescendantsAreContiguousInStackingOrderRecursive(lookup, childMinIndex, childMaxIndex, childCount, false);
        if (childCount) {
            count += childCount;
            minIndex = std::min(minIndex, childMinIndex);
            maxIndex = std::max(maxIndex, childMaxIndex);
        }
    }

    if (!isStackingContext()) {
        bool newValue = maxIndex - minIndex == count;
        bool didUpdate = newValue != m_descendantsAreContiguousInStackingOrder;
        m_descendantsAreContiguousInStackingOrder = newValue;
        if (didUpdate)
            updateNeedsCompositedScrolling();
    }
}

void RenderLayer::computeRepaintRects(const RenderLayerModelObject* repaintContainer, const RenderGeometryMap* geometryMap)
{
    ASSERT(!m_visibleContentStatusDirty);

    m_repaintRect = renderer().clippedOverflowRectForRepaint(repaintContainer);
    m_outlineBox = renderer().outlineBoundsForRepaint(repaintContainer, geometryMap);
}


void RenderLayer::computeRepaintRectsIncludingDescendants()
{
    // FIXME: computeRepaintRects() has to walk up the parent chain for every layer to compute the rects.
    // We should make this more efficient.
    // FIXME: it's wrong to call this when layout is not up-to-date, which we do.
    computeRepaintRects(renderer().containerForRepaint());

    for (RenderLayer* layer = firstChild(); layer; layer = layer->nextSibling())
        layer->computeRepaintRectsIncludingDescendants();
}

void RenderLayer::clearRepaintRects()
{
    ASSERT(!m_hasVisibleContent);
    ASSERT(!m_visibleContentStatusDirty);

    m_repaintRect = LayoutRect();
    m_outlineBox = LayoutRect();
}

void RenderLayer::updateLayerPositionsAfterDocumentScroll()
{
    ASSERT(this == renderer().view().layer());

    RenderGeometryMap geometryMap(UseTransforms);
    updateLayerPositionsAfterScroll(&geometryMap);
}

void RenderLayer::updateLayerPositionsAfterOverflowScroll()
{
    RenderGeometryMap geometryMap(UseTransforms);
    if (this != renderer().view().layer())
        geometryMap.pushMappingsToAncestor(parent(), 0);

    // FIXME: why is it OK to not check the ancestors of this layer in order to
    // initialize the HasSeenViewportConstrainedAncestor and HasSeenAncestorWithOverflowClip flags?
    updateLayerPositionsAfterScroll(&geometryMap, IsOverflowScroll);
}

void RenderLayer::updateLayerPositionsAfterScroll(RenderGeometryMap* geometryMap, UpdateLayerPositionsAfterScrollFlags flags)
{
    // FIXME: This shouldn't be needed, but there are some corner cases where
    // these flags are still dirty. Update so that the check below is valid.
    updateDescendantDependentFlags();

    // If we have no visible content and no visible descendants, there is no point recomputing
    // our rectangles as they will be empty. If our visibility changes, we are expected to
    // recompute all our positions anyway.
    if (!m_hasVisibleDescendant && !m_hasVisibleContent)
        return;

    bool positionChanged = updateLayerPosition();
    if (positionChanged)
        flags |= HasChangedAncestor;

    if (geometryMap)
        geometryMap->pushMappingsToAncestor(this, parent());

    if (flags & HasChangedAncestor || flags & HasSeenViewportConstrainedAncestor || flags & IsOverflowScroll)
        clearClipRects();

    if (renderer().style().hasViewportConstrainedPosition())
        flags |= HasSeenViewportConstrainedAncestor;

    if (renderer().hasOverflowClip())
        flags |= HasSeenAncestorWithOverflowClip;

    if (flags & HasSeenViewportConstrainedAncestor
        || (flags & IsOverflowScroll && flags & HasSeenAncestorWithOverflowClip)) {
        // FIXME: We could track the repaint container as we walk down the tree.
        computeRepaintRects(renderer().containerForRepaint(), geometryMap);
    } else {
        // Check that our cached rects are correct.
        ASSERT(m_repaintRect == renderer().clippedOverflowRectForRepaint(renderer().containerForRepaint()));
        ASSERT(m_outlineBox == renderer().outlineBoundsForRepaint(renderer().containerForRepaint(), geometryMap));
    }
    
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositionsAfterScroll(geometryMap, flags);

    // We don't update our reflection as scrolling is a translation which does not change the size()
    // of an object, thus RenderReplica will still repaint itself properly as the layer position was
    // updated above.

    if (m_marquee) {
        bool oldUpdatingMarqueePosition = m_updatingMarqueePosition;
        m_updatingMarqueePosition = true;
        m_marquee->updateMarqueePosition();
        m_updatingMarqueePosition = oldUpdatingMarqueePosition;
    }

    if (geometryMap)
        geometryMap->popMappingsToAncestor(parent());
}

void RenderLayer::positionNewlyCreatedOverflowControls()
{
    if (!backing()->hasUnpositionedOverflowControlsLayers())
        return;

    RenderGeometryMap geometryMap(UseTransforms);
    if (this != renderer().view().layer() && parent())
        geometryMap.pushMappingsToAncestor(parent(), 0);

    LayoutPoint offsetFromRoot = LayoutPoint(geometryMap.absolutePoint(FloatPoint()));
    positionOverflowControls(toIntSize(roundedIntPoint(offsetFromRoot)));
}

#if ENABLE(CSS_COMPOSITING)

void RenderLayer::updateBlendMode()
{
    bool hadBlendMode = m_blendMode != BlendModeNormal;
    if (parent() && hadBlendMode != hasBlendMode()) {
        if (hasBlendMode())
            parent()->updateAncestorChainHasBlendingDescendants();
        else
            parent()->dirtyAncestorChainHasBlendingDescendants();
    }

    BlendMode newBlendMode = renderer().style().blendMode();
    if (newBlendMode != m_blendMode)
        m_blendMode = newBlendMode;
}

void RenderLayer::updateAncestorChainHasBlendingDescendants()
{
    for (auto layer = this; layer; layer = layer->parent()) {
        if (!layer->hasNotIsolatedBlendingDescendantsStatusDirty() && layer->hasNotIsolatedBlendingDescendants())
            break;
        layer->m_hasNotIsolatedBlendingDescendants = true;
        layer->m_hasNotIsolatedBlendingDescendantsStatusDirty = false;

        layer->updateSelfPaintingLayer();

        if (layer->isStackingContext())
            break;
    }
}

void RenderLayer::dirtyAncestorChainHasBlendingDescendants()
{
    for (auto layer = this; layer; layer = layer->parent()) {
        if (layer->hasNotIsolatedBlendingDescendantsStatusDirty())
            break;
        
        layer->m_hasNotIsolatedBlendingDescendantsStatusDirty = true;

        if (layer->isStackingContext())
            break;
    }
}
#endif

void RenderLayer::updateTransform()
{
    // hasTransform() on the renderer is also true when there is transform-style: preserve-3d or perspective set,
    // so check style too.
    bool hasTransform = renderer().hasTransform() && renderer().style().hasTransform();
    bool had3DTransform = has3DTransform();

    bool hadTransform = !!m_transform;
    if (hasTransform != hadTransform) {
        if (hasTransform)
            m_transform = std::make_unique<TransformationMatrix>();
        else
            m_transform = nullptr;
        
        // Layers with transforms act as clip rects roots, so clear the cached clip rects here.
        clearClipRectsIncludingDescendants();
    }
    
    if (hasTransform) {
        RenderBox* box = renderBox();
        ASSERT(box);
        m_transform->makeIdentity();
        box->style().applyTransform(*m_transform, pixelSnappedForPainting(box->borderBoxRect(), box->document().deviceScaleFactor()), RenderStyle::IncludeTransformOrigin);
        makeMatrixRenderable(*m_transform, canRender3DTransforms());
    }

    if (had3DTransform != has3DTransform())
        dirty3DTransformedDescendantStatus();
}

TransformationMatrix RenderLayer::currentTransform(RenderStyle::ApplyTransformOrigin applyOrigin) const
{
    if (!m_transform)
        return TransformationMatrix();

    RenderBox* box = renderBox();
    ASSERT(box);
    FloatRect pixelSnappedBorderRect = pixelSnappedForPainting(box->borderBoxRect(), box->document().deviceScaleFactor());
    if (renderer().style().isRunningAcceleratedAnimation()) {
        TransformationMatrix currTransform;
        RefPtr<RenderStyle> style = renderer().animation().getAnimatedStyleForRenderer(&renderer());
        style->applyTransform(currTransform, pixelSnappedBorderRect, applyOrigin);
        makeMatrixRenderable(currTransform, canRender3DTransforms());
        return currTransform;
    }

    // m_transform includes transform-origin, so we need to recompute the transform here.
    if (applyOrigin == RenderStyle::ExcludeTransformOrigin) {
        TransformationMatrix currTransform;
        box->style().applyTransform(currTransform, pixelSnappedBorderRect, RenderStyle::ExcludeTransformOrigin);
        makeMatrixRenderable(currTransform, canRender3DTransforms());
        return currTransform;
    }

    return *m_transform;
}

TransformationMatrix RenderLayer::renderableTransform(PaintBehavior paintBehavior) const
{
    if (!m_transform)
        return TransformationMatrix();
    
    if (paintBehavior & PaintBehaviorFlattenCompositingLayers) {
        TransformationMatrix matrix = *m_transform;
        makeMatrixRenderable(matrix, false /* flatten 3d */);
        return matrix;
    }

    return *m_transform;
}

RenderLayer* RenderLayer::enclosingOverflowClipLayer(IncludeSelfOrNot includeSelf) const
{
    const RenderLayer* layer = (includeSelf == IncludeSelf) ? this : parent();
    while (layer) {
        if (layer->renderer().hasOverflowClip())
            return const_cast<RenderLayer*>(layer);

        layer = layer->parent();
    }
    return 0;
}

// FIXME: This is terrible. Bring back a cached bit for this someday. This crawl is going to slow down all
// painting of content inside paginated layers.
bool RenderLayer::hasCompositedLayerInEnclosingPaginationChain() const
{
    // No enclosing layer means no compositing in the chain.
    if (!m_enclosingPaginationLayer)
        return false;
    
    // If the enclosing layer is composited, we don't have to check anything in between us and that
    // layer.
    if (m_enclosingPaginationLayer->isComposited())
        return true;

    // If we are the enclosing pagination layer, then we can't be composited or we'd have passed the
    // previous check.
    if (m_enclosingPaginationLayer == this)
        return false;

    // The enclosing paginated layer is our ancestor and is not composited, so we have to check
    // intermediate layers between us and the enclosing pagination layer. Start with our own layer.
    if (isComposited())
        return true;
    
    // For normal flow layers, we can recur up the layer tree.
    if (isNormalFlowOnly())
        return parent()->hasCompositedLayerInEnclosingPaginationChain();
    
    // Otherwise we have to go up the containing block chain. Find the first enclosing
    // containing block layer ancestor, and check that.
    RenderView* renderView = &renderer().view();
    for (RenderBlock* containingBlock = renderer().containingBlock(); containingBlock && containingBlock != renderView; containingBlock = containingBlock->containingBlock()) {
        if (containingBlock->hasLayer())
            return containingBlock->layer()->hasCompositedLayerInEnclosingPaginationChain();
    }
    return false;
}

void RenderLayer::updatePagination()
{
    m_enclosingPaginationLayer = nullptr;
    
    if (!parent())
        return;
    
    // Each layer that is inside a multicolumn flow thread has to be checked individually and
    // genuinely know if it is going to have to split itself up when painting only its contents (and not any other descendant
    // layers). We track an enclosingPaginationLayer instead of using a simple bit, since we want to be able to get back
    // to that layer easily.
    if (renderer().isInFlowRenderFlowThread()) {
        m_enclosingPaginationLayer = this;
        return;
    }

    if (isNormalFlowOnly()) {
        // Content inside a transform is not considered to be paginated, since we simply
        // paint the transform multiple times in each column, so we don't have to use
        // fragments for the transformed content.
        if (parent()->hasTransform())
            m_enclosingPaginationLayer = nullptr;
        else
            m_enclosingPaginationLayer = parent()->enclosingPaginationLayer(IncludeCompositedPaginatedLayers);
        return;
    }

    // For the new columns code, we want to walk up our containing block chain looking for an enclosing layer. Once
    // we find one, then we just check its pagination status.
    RenderView* renderView = &renderer().view();
    RenderBlock* containingBlock;
    for (containingBlock = renderer().containingBlock(); containingBlock && containingBlock != renderView; containingBlock = containingBlock->containingBlock()) {
        if (containingBlock->hasLayer()) {
            // Content inside a transform is not considered to be paginated, since we simply
            // paint the transform multiple times in each column, so we don't have to use
            // fragments for the transformed content.
            if (containingBlock->layer()->hasTransform())
                m_enclosingPaginationLayer = nullptr;
            else
                m_enclosingPaginationLayer = containingBlock->layer()->enclosingPaginationLayer(IncludeCompositedPaginatedLayers);
            return;
        }
    }
}

bool RenderLayer::canBeStackingContainer() const
{
    if (isStackingContext() || !stackingContainer())
        return true;

    return m_descendantsAreContiguousInStackingOrder;
}

void RenderLayer::setHasVisibleContent()
{ 
    if (m_hasVisibleContent && !m_visibleContentStatusDirty) {
        ASSERT(!parent() || parent()->hasVisibleDescendant());
        return;
    }

    m_visibleContentStatusDirty = false; 
    m_hasVisibleContent = true;
    computeRepaintRects(renderer().containerForRepaint());
    if (!isNormalFlowOnly()) {
        // We don't collect invisible layers in z-order lists if we are not in compositing mode.
        // As we became visible, we need to dirty our stacking containers ancestors to be properly
        // collected. FIXME: When compositing, we could skip this dirtying phase.
        for (RenderLayer* sc = stackingContainer(); sc; sc = sc->stackingContainer()) {
            sc->dirtyZOrderLists();
            if (sc->hasVisibleContent())
                break;
        }
    }

    if (parent())
        parent()->setAncestorChainHasVisibleDescendant();
}

void RenderLayer::dirtyVisibleContentStatus() 
{ 
    m_visibleContentStatusDirty = true; 
    if (parent())
        parent()->dirtyAncestorChainVisibleDescendantStatus();
}

void RenderLayer::dirtyAncestorChainVisibleDescendantStatus()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (layer->m_visibleDescendantStatusDirty)
            break;

        layer->m_visibleDescendantStatusDirty = true;
    }
}

void RenderLayer::setAncestorChainHasVisibleDescendant()
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (!layer->m_visibleDescendantStatusDirty && layer->hasVisibleDescendant())
            break;

        layer->m_hasVisibleDescendant = true;
        layer->m_visibleDescendantStatusDirty = false;
    }
}

void RenderLayer::updateDescendantDependentFlags(HashSet<const RenderObject*>* outOfFlowDescendantContainingBlocks)
{
    if (m_visibleDescendantStatusDirty || m_hasSelfPaintingLayerDescendantDirty || m_hasOutOfFlowPositionedDescendantDirty || hasNotIsolatedBlendingDescendantsStatusDirty()) {
        bool hasVisibleDescendant = false;
        bool hasSelfPaintingLayerDescendant = false;
        bool hasOutOfFlowPositionedDescendant = false;
#if ENABLE(CSS_COMPOSITING)
        bool hasNotIsolatedBlendingDescendants = false;
#endif

        HashSet<const RenderObject*> childOutOfFlowDescendantContainingBlocks;
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            childOutOfFlowDescendantContainingBlocks.clear();
            child->updateDescendantDependentFlags(&childOutOfFlowDescendantContainingBlocks);

            bool childIsOutOfFlowPositioned = child->renderer().isOutOfFlowPositioned();
            if (childIsOutOfFlowPositioned)
                childOutOfFlowDescendantContainingBlocks.add(child->renderer().containingBlock());

            if (outOfFlowDescendantContainingBlocks) {
                HashSet<const RenderObject*>::const_iterator it = childOutOfFlowDescendantContainingBlocks.begin();
                for (; it != childOutOfFlowDescendantContainingBlocks.end(); ++it)
                    outOfFlowDescendantContainingBlocks->add(*it);
            }

            hasVisibleDescendant |= child->m_hasVisibleContent || child->m_hasVisibleDescendant;
            hasSelfPaintingLayerDescendant |= child->isSelfPaintingLayer() || child->hasSelfPaintingLayerDescendant();
            hasOutOfFlowPositionedDescendant |= !childOutOfFlowDescendantContainingBlocks.isEmpty();
#if ENABLE(CSS_COMPOSITING)
            hasNotIsolatedBlendingDescendants |= child->hasBlendMode() || (child->hasNotIsolatedBlendingDescendants() && !child->isolatesBlending());
#endif

            bool allFlagsSet = hasVisibleDescendant && hasSelfPaintingLayerDescendant && hasOutOfFlowPositionedDescendant;
#if ENABLE(CSS_COMPOSITING)
            allFlagsSet &= hasNotIsolatedBlendingDescendants;
#endif
            if (allFlagsSet)
                break;
        }

        if (outOfFlowDescendantContainingBlocks)
            outOfFlowDescendantContainingBlocks->remove(&renderer());

        m_hasVisibleDescendant = hasVisibleDescendant;
        m_visibleDescendantStatusDirty = false;
        m_hasSelfPaintingLayerDescendant = hasSelfPaintingLayerDescendant;
        m_hasSelfPaintingLayerDescendantDirty = false;

        m_hasOutOfFlowPositionedDescendant = hasOutOfFlowPositionedDescendant;
        if (m_hasOutOfFlowPositionedDescendantDirty)
            updateNeedsCompositedScrolling();

        m_hasOutOfFlowPositionedDescendantDirty = false;
#if ENABLE(CSS_COMPOSITING)
        m_hasNotIsolatedBlendingDescendants = hasNotIsolatedBlendingDescendants;
        if (m_hasNotIsolatedBlendingDescendantsStatusDirty) {
            m_hasNotIsolatedBlendingDescendantsStatusDirty = false;
            updateSelfPaintingLayer();
        }
#endif
    }

    if (m_visibleContentStatusDirty) {
        if (renderer().style().visibility() == VISIBLE)
            m_hasVisibleContent = true;
        else {
            // layer may be hidden but still have some visible content, check for this
            m_hasVisibleContent = false;
            RenderObject* r = renderer().firstChild();
            while (r) {
                if (r->style().visibility() == VISIBLE && !r->hasLayer()) {
                    m_hasVisibleContent = true;
                    break;
                }
                RenderObject* child = nullptr;
                if (!r->hasLayer() && (child = r->firstChildSlow()))
                    r = child;
                else if (r->nextSibling())
                    r = r->nextSibling();
                else {
                    do {
                        r = r->parent();
                        if (r == &renderer())
                            r = 0;
                    } while (r && !r->nextSibling());
                    if (r)
                        r = r->nextSibling();
                }
            }
        }    
        m_visibleContentStatusDirty = false; 
    }
}

// Return true if the new clipping behaviour requires layer update.
bool RenderLayer::checkIfDescendantClippingContextNeedsUpdate(bool isClipping)
{
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        RenderLayerBacking* backing = child->backing();
        // Layer subtree needs update when new clipping is added or existing clipping is removed.
        if (backing && (isClipping || backing->hasAncestorClippingLayer()))
            return true;

        if (child->checkIfDescendantClippingContextNeedsUpdate(isClipping))
            return true;
    }
    return false;
}

void RenderLayer::dirty3DTransformedDescendantStatus()
{
    RenderLayer* curr = stackingContainer();
    if (curr)
        curr->m_3DTransformedDescendantStatusDirty = true;
        
    // This propagates up through preserve-3d hierarchies to the enclosing flattening layer.
    // Note that preserves3D() creates stacking context, so we can just run up the stacking containers.
    while (curr && curr->preserves3D()) {
        curr->m_3DTransformedDescendantStatusDirty = true;
        curr = curr->stackingContainer();
    }
}

// Return true if this layer or any preserve-3d descendants have 3d.
bool RenderLayer::update3DTransformedDescendantStatus()
{
    if (m_3DTransformedDescendantStatusDirty) {
        m_has3DTransformedDescendant = false;

        updateZOrderLists();

        // Transformed or preserve-3d descendants can only be in the z-order lists, not
        // in the normal flow list, so we only need to check those.
        if (Vector<RenderLayer*>* positiveZOrderList = posZOrderList()) {
            for (unsigned i = 0; i < positiveZOrderList->size(); ++i)
                m_has3DTransformedDescendant |= positiveZOrderList->at(i)->update3DTransformedDescendantStatus();
        }

        // Now check our negative z-index children.
        if (Vector<RenderLayer*>* negativeZOrderList = negZOrderList()) {
            for (unsigned i = 0; i < negativeZOrderList->size(); ++i)
                m_has3DTransformedDescendant |= negativeZOrderList->at(i)->update3DTransformedDescendantStatus();
        }
        
        m_3DTransformedDescendantStatusDirty = false;
    }
    
    // If we live in a 3d hierarchy, then the layer at the root of that hierarchy needs
    // the m_has3DTransformedDescendant set.
    if (preserves3D())
        return has3DTransform() || m_has3DTransformedDescendant;

    return has3DTransform();
}

bool RenderLayer::updateLayerPosition()
{
    LayoutPoint localPoint;
    LayoutSize inlineBoundingBoxOffset; // We don't put this into the RenderLayer x/y for inlines, so we need to subtract it out when done.
    if (renderer().isInline() && renderer().isRenderInline()) {
        RenderInline& inlineFlow = toRenderInline(renderer());
        IntRect lineBox = inlineFlow.linesBoundingBox();
        setSize(lineBox.size());
        inlineBoundingBoxOffset = toLayoutSize(lineBox.location());
        localPoint += inlineBoundingBoxOffset;
    } else if (RenderBox* box = renderBox()) {
        // FIXME: Is snapping the size really needed here for the RenderBox case?
        setSize(pixelSnappedIntSize(box->size(), box->location()));
        localPoint += box->topLeftLocationOffset();
    }

    if (!renderer().isOutOfFlowPositioned() && renderer().parent()) {
        // We must adjust our position by walking up the render tree looking for the
        // nearest enclosing object with a layer.
        RenderElement* curr = renderer().parent();
        while (curr && !curr->hasLayer()) {
            if (curr->isBox() && !curr->isTableRow()) {
                // Rows and cells share the same coordinate space (that of the section).
                // Omit them when computing our xpos/ypos.
                localPoint += toRenderBox(curr)->topLeftLocationOffset();
            }
            curr = curr->parent();
        }
        if (curr->isBox() && curr->isTableRow()) {
            // Put ourselves into the row coordinate space.
            localPoint -= toRenderBox(curr)->topLeftLocationOffset();
        }
    }
    
    // Subtract our parent's scroll offset.
    if (renderer().isOutOfFlowPositioned() && enclosingPositionedAncestor()) {
        RenderLayer* positionedParent = enclosingPositionedAncestor();

        // For positioned layers, we subtract out the enclosing positioned layer's scroll offset.
        if (positionedParent->renderer().hasOverflowClip()) {
            LayoutSize offset = positionedParent->scrolledContentOffset();
            localPoint -= offset;
        }
        
        if (renderer().isOutOfFlowPositioned() && positionedParent->renderer().isInFlowPositioned() && positionedParent->renderer().isRenderInline()) {
            LayoutSize offset = toRenderInline(positionedParent->renderer()).offsetForInFlowPositionedInline(&toRenderBox(renderer()));
            localPoint += offset;
        }
    } else if (parent()) {
        if (parent()->renderer().hasOverflowClip()) {
            IntSize scrollOffset = parent()->scrolledContentOffset();
            localPoint -= scrollOffset;
        }
    }
    
    bool positionOrOffsetChanged = false;
    if (renderer().isInFlowPositioned()) {
        LayoutSize newOffset = toRenderBoxModelObject(renderer()).offsetForInFlowPosition();
        positionOrOffsetChanged = newOffset != m_offsetForInFlowPosition;
        m_offsetForInFlowPosition = newOffset;
        localPoint.move(m_offsetForInFlowPosition);
    } else {
        m_offsetForInFlowPosition = LayoutSize();
    }

    // FIXME: We'd really like to just get rid of the concept of a layer rectangle and rely on the renderers.
    localPoint -= inlineBoundingBoxOffset;
    
    positionOrOffsetChanged |= location() != localPoint;
    setLocation(localPoint);
    return positionOrOffsetChanged;
}

TransformationMatrix RenderLayer::perspectiveTransform() const
{
    if (!renderer().hasTransform())
        return TransformationMatrix();

    const RenderStyle& style = renderer().style();
    if (!style.hasPerspective())
        return TransformationMatrix();

    // Maybe fetch the perspective from the backing?
    const IntRect borderBox = toRenderBox(renderer()).pixelSnappedBorderBoxRect();
    const float boxWidth = borderBox.width();
    const float boxHeight = borderBox.height();

    float perspectiveOriginX = floatValueForLength(style.perspectiveOriginX(), boxWidth);
    float perspectiveOriginY = floatValueForLength(style.perspectiveOriginY(), boxHeight);

    // A perspective origin of 0,0 makes the vanishing point in the center of the element.
    // We want it to be in the top-left, so subtract half the height and width.
    perspectiveOriginX -= boxWidth / 2.0f;
    perspectiveOriginY -= boxHeight / 2.0f;
    
    TransformationMatrix t;
    t.translate(perspectiveOriginX, perspectiveOriginY);
    t.applyPerspective(style.perspective());
    t.translate(-perspectiveOriginX, -perspectiveOriginY);
    
    return t;
}

FloatPoint RenderLayer::perspectiveOrigin() const
{
    if (!renderer().hasTransform())
        return FloatPoint();

    const LayoutRect borderBox = toRenderBox(renderer()).borderBoxRect();
    const RenderStyle& style = renderer().style();

    return FloatPoint(floatValueForLength(style.perspectiveOriginX(), borderBox.width()),
                      floatValueForLength(style.perspectiveOriginY(), borderBox.height()));
}

RenderLayer* RenderLayer::stackingContainer() const
{
    RenderLayer* layer = parent();
    while (layer && !layer->isStackingContainer())
        layer = layer->parent();

    ASSERT(!layer || layer->isStackingContainer());
    return layer;
}

static inline bool isPositionedContainer(RenderLayer* layer)
{
    return layer->isRootLayer() || layer->renderer().isPositioned() || layer->hasTransform();
}

static inline bool isFixedPositionedContainer(RenderLayer* layer)
{
    return layer->isRootLayer() || layer->hasTransform();
}

RenderLayer* RenderLayer::enclosingPositionedAncestor() const
{
    RenderLayer* curr = parent();
    while (curr && !isPositionedContainer(curr))
        curr = curr->parent();

    return curr;
}

static RenderLayer* parentLayerCrossFrame(const RenderLayer* layer)
{
    ASSERT(layer);
    if (layer->parent())
        return layer->parent();

    HTMLFrameOwnerElement* ownerElement = layer->renderer().document().ownerElement();
    if (!ownerElement)
        return 0;

    RenderElement* ownerRenderer = ownerElement->renderer();
    if (!ownerRenderer)
        return 0;

    return ownerRenderer->enclosingLayer();
}

RenderLayer* RenderLayer::enclosingScrollableLayer() const
{
    for (RenderLayer* nextLayer = parentLayerCrossFrame(this); nextLayer; nextLayer = parentLayerCrossFrame(nextLayer)) {
        if (nextLayer->renderer().isBox() && toRenderBox(nextLayer->renderer()).canBeScrolledAndHasScrollableArea())
            return nextLayer;
    }

    return 0;
}

IntRect RenderLayer::scrollableAreaBoundingBox() const
{
    return renderer().absoluteBoundingBoxRect();
}

bool RenderLayer::forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const
{
    Page* page = renderer().frame().page();
    return page && page->settings().forceUpdateScrollbarsOnMainThreadForPerformanceTesting();
}

RenderLayer* RenderLayer::enclosingTransformedAncestor() const
{
    RenderLayer* curr = parent();
    while (curr && !curr->isRootLayer() && !curr->transform())
        curr = curr->parent();

    return curr;
}

static inline const RenderLayer* compositingContainer(const RenderLayer* layer)
{
    return layer->isNormalFlowOnly() ? layer->parent() : layer->stackingContainer();
}

inline bool RenderLayer::shouldRepaintAfterLayout() const
{
    if (m_repaintStatus == NeedsNormalRepaint)
        return true;

    // Composited layers that were moved during a positioned movement only
    // layout, don't need to be repainted. They just need to be recomposited.
    ASSERT(m_repaintStatus == NeedsFullRepaintForPositionedMovementLayout);
    return !isComposited() || backing()->paintsIntoCompositedAncestor();
}

static bool compositedWithOwnBackingStore(const RenderLayer* layer)
{
    return layer->isComposited() && !layer->backing()->paintsIntoCompositedAncestor();
}

RenderLayer* RenderLayer::enclosingCompositingLayer(IncludeSelfOrNot includeSelf) const
{
    if (includeSelf == IncludeSelf && isComposited())
        return const_cast<RenderLayer*>(this);

    for (const RenderLayer* curr = compositingContainer(this); curr; curr = compositingContainer(curr)) {
        if (curr->isComposited())
            return const_cast<RenderLayer*>(curr);
    }
         
    return 0;
}

RenderLayer* RenderLayer::enclosingCompositingLayerForRepaint(IncludeSelfOrNot includeSelf) const
{
    if (includeSelf == IncludeSelf && isComposited() && !backing()->paintsIntoCompositedAncestor())
        return const_cast<RenderLayer*>(this);

    for (const RenderLayer* curr = compositingContainer(this); curr; curr = compositingContainer(curr)) {
        if (compositedWithOwnBackingStore(curr))
            return const_cast<RenderLayer*>(curr);
    }
         
    return 0;
}

#if ENABLE(CSS_FILTERS)

RenderLayer* RenderLayer::enclosingFilterLayer(IncludeSelfOrNot includeSelf) const
{
    const RenderLayer* curr = (includeSelf == IncludeSelf) ? this : parent();
    for (; curr; curr = curr->parent()) {
        if (curr->requiresFullLayerImageForFilters())
            return const_cast<RenderLayer*>(curr);
    }
    
    return 0;
}

RenderLayer* RenderLayer::enclosingFilterRepaintLayer() const
{
    for (const RenderLayer* curr = this; curr; curr = curr->parent()) {
        if ((curr != this && curr->requiresFullLayerImageForFilters()) || compositedWithOwnBackingStore(curr) || curr->isRootLayer())
            return const_cast<RenderLayer*>(curr);
    }
    return 0;
}

void RenderLayer::setFilterBackendNeedsRepaintingInRect(const LayoutRect& rect)
{
    if (rect.isEmpty())
        return;
    
    LayoutRect rectForRepaint = rect;
    renderer().style().filterOutsets().expandRect(rectForRepaint);

    FilterInfo& filterInfo = FilterInfo::get(*this);
    filterInfo.expandDirtySourceRect(rectForRepaint);
    
    RenderLayer* parentLayer = enclosingFilterRepaintLayer();
    ASSERT(parentLayer);
    FloatQuad repaintQuad(rectForRepaint);
    LayoutRect parentLayerRect = renderer().localToContainerQuad(repaintQuad, &parentLayer->renderer()).enclosingBoundingBox();

    if (parentLayer->isComposited()) {
        if (!parentLayer->backing()->paintsIntoWindow()) {
            parentLayer->setBackingNeedsRepaintInRect(parentLayerRect);
            return;
        }
        // If the painting goes to window, redirect the painting to the parent RenderView.
        parentLayer = renderer().view().layer();
        parentLayerRect = renderer().localToContainerQuad(repaintQuad, &parentLayer->renderer()).enclosingBoundingBox();
    }

    if (parentLayer->paintsWithFilters()) {
        parentLayer->setFilterBackendNeedsRepaintingInRect(parentLayerRect);
        return;        
    }
    
    if (parentLayer->isRootLayer()) {
        toRenderView(parentLayer->renderer()).repaintViewRectangle(parentLayerRect);
        return;
    }
    
    ASSERT_NOT_REACHED();
}

bool RenderLayer::hasAncestorWithFilterOutsets() const
{
    for (const RenderLayer* curr = this; curr; curr = curr->parent()) {
        if (curr->renderer().style().hasFilterOutsets())
            return true;
    }
    return false;
}

#endif

RenderLayer* RenderLayer::clippingRootForPainting() const
{
    if (isComposited())
        return const_cast<RenderLayer*>(this);

    const RenderLayer* current = this;
    while (current) {
        if (current->isRootLayer())
            return const_cast<RenderLayer*>(current);

        current = compositingContainer(current);
        ASSERT(current);
        if (current->transform() || compositedWithOwnBackingStore(current))
            return const_cast<RenderLayer*>(current);
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutPoint RenderLayer::absoluteToContents(const LayoutPoint& absolutePoint) const
{
    // We don't use convertToLayerCoords because it doesn't know about transforms
    return roundedLayoutPoint(renderer().absoluteToLocal(absolutePoint, UseTransforms));
}

bool RenderLayer::cannotBlitToWindow() const
{
    if (isTransparent() || hasReflection() || hasTransform())
        return true;
    if (!parent())
        return false;
    return parent()->cannotBlitToWindow();
}

RenderLayer* RenderLayer::transparentPaintingAncestor()
{
    if (isComposited())
        return 0;

    for (RenderLayer* curr = parent(); curr; curr = curr->parent()) {
        if (curr->isComposited())
            return 0;
        if (curr->isTransparent())
            return curr;
    }
    return 0;
}

enum TransparencyClipBoxBehavior {
    PaintingTransparencyClipBox,
    HitTestingTransparencyClipBox
};

enum TransparencyClipBoxMode {
    DescendantsOfTransparencyClipBox,
    RootOfTransparencyClipBox
};

static LayoutRect transparencyClipBox(const RenderLayer&, const RenderLayer* rootLayer, TransparencyClipBoxBehavior, TransparencyClipBoxMode, PaintBehavior = 0);

static void expandClipRectForRegionAndReflection(LayoutRect& clipRect, const RenderLayer& layer, const RenderLayer* rootLayer,
    TransparencyClipBoxBehavior transparencyBehavior, PaintBehavior paintBehavior)
{
    // If this is a region, then the painting is actually done by its flow thread's layer.
    if (layer.renderer().isRenderNamedFlowFragmentContainer()) {
        RenderBlockFlow* regionContainer = toRenderBlockFlow(&layer.renderer());
        RenderNamedFlowFragment* region = regionContainer->renderNamedFlowFragment();
        RenderLayer* flowThreadLayer = region->flowThread()->layer();
        if (flowThreadLayer && (!layer.reflection() || layer.reflectionLayer() != flowThreadLayer)) {
            LayoutRect flowThreadClipRect = transparencyClipBox(*flowThreadLayer, rootLayer, transparencyBehavior, DescendantsOfTransparencyClipBox, paintBehavior);
            LayoutSize moveOffset = (regionContainer->contentBoxRect().location() + layer.offsetFromAncestor(flowThreadLayer)) - region->flowThreadPortionRect().location();
            flowThreadClipRect.move(moveOffset);
            
            clipRect.unite(flowThreadClipRect);
        }
    }
}

static void expandClipRectForDescendantsAndReflection(LayoutRect& clipRect, const RenderLayer& layer, const RenderLayer* rootLayer,
    TransparencyClipBoxBehavior transparencyBehavior, PaintBehavior paintBehavior)
{
    // If we have a mask, then the clip is limited to the border box area (and there is
    // no need to examine child layers).
    if (!layer.renderer().hasMask()) {
        // Note: we don't have to walk z-order lists since transparent elements always establish
        // a stacking container. This means we can just walk the layer tree directly.
        for (RenderLayer* curr = layer.firstChild(); curr; curr = curr->nextSibling()) {
            if (!layer.reflection() || layer.reflectionLayer() != curr)
                clipRect.unite(transparencyClipBox(*curr, rootLayer, transparencyBehavior, DescendantsOfTransparencyClipBox, paintBehavior));
        }
    }

    expandClipRectForRegionAndReflection(clipRect, layer, rootLayer, transparencyBehavior, paintBehavior);

    // If we have a reflection, then we need to account for that when we push the clip.  Reflect our entire
    // current transparencyClipBox to catch all child layers.
    // FIXME: Accelerated compositing will eventually want to do something smart here to avoid incorporating this
    // size into the parent layer.
    if (layer.renderer().hasReflection()) {
        LayoutSize delta = layer.offsetFromAncestor(rootLayer);
        clipRect.move(-delta);
        clipRect.unite(layer.renderBox()->reflectedRect(clipRect));
        clipRect.move(delta);
    }
}

static LayoutRect transparencyClipBox(const RenderLayer& layer, const RenderLayer* rootLayer, TransparencyClipBoxBehavior transparencyBehavior,
    TransparencyClipBoxMode transparencyMode, PaintBehavior paintBehavior)
{
    // FIXME: Although this function completely ignores CSS-imposed clipping, we did already intersect with the
    // paintDirtyRect, and that should cut down on the amount we have to paint.  Still it
    // would be better to respect clips.
    
    if (rootLayer != &layer && ((transparencyBehavior == PaintingTransparencyClipBox && layer.paintsWithTransform(paintBehavior))
        || (transparencyBehavior == HitTestingTransparencyClipBox && layer.hasTransform()))) {
        // The best we can do here is to use enclosed bounding boxes to establish a "fuzzy" enough clip to encompass
        // the transformed layer and all of its children.
        RenderLayer::PaginationInclusionMode mode = transparencyBehavior == HitTestingTransparencyClipBox ? RenderLayer::IncludeCompositedPaginatedLayers : RenderLayer::ExcludeCompositedPaginatedLayers;
        const RenderLayer* paginationLayer = transparencyMode == DescendantsOfTransparencyClipBox ? layer.enclosingPaginationLayer(mode) : 0;
        const RenderLayer* rootLayerForTransform = paginationLayer ? paginationLayer : rootLayer;
        LayoutSize delta = layer.offsetFromAncestor(rootLayerForTransform);

        TransformationMatrix transform;
        transform.translate(delta.width(), delta.height());
        transform = transform * *layer.transform();

        // We don't use fragment boxes when collecting a transformed layer's bounding box, since it always
        // paints unfragmented.
        LayoutRect clipRect = layer.boundingBox(&layer);
        expandClipRectForDescendantsAndReflection(clipRect, layer, &layer, transparencyBehavior, paintBehavior);
#if ENABLE(CSS_FILTERS)
        layer.renderer().style().filterOutsets().expandRect(clipRect);
#endif
        LayoutRect result = transform.mapRect(clipRect);
        if (!paginationLayer)
            return result;
        
        // We have to break up the transformed extent across our columns.
        // Split our box up into the actual fragment boxes that render in the columns/pages and unite those together to
        // get our true bounding box.
        RenderFlowThread& enclosingFlowThread = toRenderFlowThread(paginationLayer->renderer());
        result = enclosingFlowThread.fragmentsBoundingBox(result);
        result.move(paginationLayer->offsetFromAncestor(rootLayer));
        return result;
    }
    
    LayoutRect clipRect = layer.boundingBox(rootLayer, layer.offsetFromAncestor(rootLayer), transparencyBehavior == HitTestingTransparencyClipBox ? RenderLayer::UseFragmentBoxesIncludingCompositing : RenderLayer::UseFragmentBoxesExcludingCompositing);
    expandClipRectForDescendantsAndReflection(clipRect, layer, rootLayer, transparencyBehavior, paintBehavior);
#if ENABLE(CSS_FILTERS)
    layer.renderer().style().filterOutsets().expandRect(clipRect);
#endif
    return clipRect;
}

static LayoutRect paintingExtent(const RenderLayer& currentLayer, const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, PaintBehavior paintBehavior)
{
    return intersection(transparencyClipBox(currentLayer, rootLayer, PaintingTransparencyClipBox, RootOfTransparencyClipBox, paintBehavior), paintDirtyRect);
}

void RenderLayer::beginTransparencyLayers(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, const LayoutRect& dirtyRect)
{
    if (context->paintingDisabled() || (paintsWithTransparency(paintingInfo.paintBehavior) && m_usedTransparency))
        return;

    RenderLayer* ancestor = transparentPaintingAncestor();
    if (ancestor)
        ancestor->beginTransparencyLayers(context, paintingInfo, dirtyRect);
    
    if (paintsWithTransparency(paintingInfo.paintBehavior)) {
        m_usedTransparency = true;
        context->save();
        LayoutRect adjustedClipRect = paintingExtent(*this, paintingInfo.rootLayer, dirtyRect, paintingInfo.paintBehavior);
        adjustedClipRect.move(paintingInfo.subpixelAccumulation);
        FloatRect pixelSnappedClipRect = pixelSnappedForPainting(adjustedClipRect, renderer().document().deviceScaleFactor());
        context->clip(pixelSnappedClipRect);

#if ENABLE(CSS_COMPOSITING)
        if (hasBlendMode())
            context->setCompositeOperation(context->compositeOperation(), blendMode());
#endif

        context->beginTransparencyLayer(renderer().opacity());

#if ENABLE(CSS_COMPOSITING)
        if (hasBlendMode())
            context->setCompositeOperation(context->compositeOperation(), BlendModeNormal);
#endif

#ifdef REVEAL_TRANSPARENCY_LAYERS
        context->setFillColor(Color(0.0f, 0.0f, 0.5f, 0.2f), ColorSpaceDeviceRGB);
        context->fillRect(pixelSnappedClipRect);
#endif
    }
}

#if PLATFORM(IOS)
void RenderLayer::willBeDestroyed()
{
    if (RenderLayerBacking* layerBacking = backing())
        layerBacking->layerWillBeDestroyed();
}
#endif

void RenderLayer::addChild(RenderLayer* child, RenderLayer* beforeChild)
{
    RenderLayer* prevSibling = beforeChild ? beforeChild->previousSibling() : lastChild();
    if (prevSibling) {
        child->setPreviousSibling(prevSibling);
        prevSibling->setNextSibling(child);
        ASSERT(prevSibling != child);
    } else
        setFirstChild(child);

    if (beforeChild) {
        beforeChild->setPreviousSibling(child);
        child->setNextSibling(beforeChild);
        ASSERT(beforeChild != child);
    } else
        setLastChild(child);

    child->setParent(this);

    if (child->isNormalFlowOnly())
        dirtyNormalFlowList();

    if (!child->isNormalFlowOnly() || child->firstChild()) {
        // Dirty the z-order list in which we are contained. The stackingContainer() can be null in the
        // case where we're building up generated content layers. This is ok, since the lists will start
        // off dirty in that case anyway.
        child->dirtyStackingContainerZOrderLists();
    }

    child->updateDescendantDependentFlags();
    if (child->m_hasVisibleContent || child->m_hasVisibleDescendant)
        setAncestorChainHasVisibleDescendant();

    if (child->isSelfPaintingLayer() || child->hasSelfPaintingLayerDescendant())
        setAncestorChainHasSelfPaintingLayerDescendant();

    if (child->renderer().isOutOfFlowPositioned() || child->hasOutOfFlowPositionedDescendant())
        setAncestorChainHasOutOfFlowPositionedDescendant(child->renderer().containingBlock());

#if ENABLE(CSS_COMPOSITING)
    if (child->hasBlendMode() || (child->hasNotIsolatedBlendingDescendants() && !child->isolatesBlending()))
        updateAncestorChainHasBlendingDescendants();
#endif

    compositor().layerWasAdded(*this, *child);
}

RenderLayer* RenderLayer::removeChild(RenderLayer* oldChild)
{
    if (!renderer().documentBeingDestroyed())
        compositor().layerWillBeRemoved(*this, *oldChild);

    // remove the child
    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (m_first == oldChild)
        m_first = oldChild->nextSibling();
    if (m_last == oldChild)
        m_last = oldChild->previousSibling();

    if (oldChild->isNormalFlowOnly())
        dirtyNormalFlowList();
    if (!oldChild->isNormalFlowOnly() || oldChild->firstChild()) { 
        // Dirty the z-order list in which we are contained.  When called via the
        // reattachment process in removeOnlyThisLayer, the layer may already be disconnected
        // from the main layer tree, so we need to null-check the |stackingContainer| value.
        oldChild->dirtyStackingContainerZOrderLists();
    }

    if (oldChild->renderer().isOutOfFlowPositioned() || oldChild->hasOutOfFlowPositionedDescendant())
        dirtyAncestorChainHasOutOfFlowPositionedDescendantStatus();

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);
    
    oldChild->updateDescendantDependentFlags();
    if (oldChild->m_hasVisibleContent || oldChild->m_hasVisibleDescendant)
        dirtyAncestorChainVisibleDescendantStatus();

    if (oldChild->isSelfPaintingLayer() || oldChild->hasSelfPaintingLayerDescendant())
        dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

#if ENABLE(CSS_COMPOSITING)
    if (oldChild->hasBlendMode() || (oldChild->hasNotIsolatedBlendingDescendants() && !oldChild->isolatesBlending()))
        dirtyAncestorChainHasBlendingDescendants();
#endif

    return oldChild;
}

void RenderLayer::removeOnlyThisLayer()
{
    if (!m_parent)
        return;

    // Mark that we are about to lose our layer. This makes render tree
    // walks ignore this layer while we're removing it.
    renderer().setHasLayer(false);

    compositor().layerWillBeRemoved(*m_parent, *this);

    // Dirty the clip rects.
    clearClipRectsIncludingDescendants();

    RenderLayer* nextSib = nextSibling();

    // Remove the child reflection layer before moving other child layers.
    // The reflection layer should not be moved to the parent.
    if (reflection())
        removeChild(reflectionLayer());

    // Now walk our kids and reattach them to our parent.
    RenderLayer* current = m_first;
    while (current) {
        RenderLayer* next = current->nextSibling();
        removeChild(current);
        m_parent->addChild(current, nextSib);
        current->setRepaintStatus(NeedsFullRepaint);
        // updateLayerPositions depends on hasLayer() already being false for proper layout.
        ASSERT(!renderer().hasLayer());
        current->updateLayerPositions(0); // FIXME: use geometry map.
        current = next;
    }

    // Remove us from the parent.
    m_parent->removeChild(this);
    renderer().destroyLayer();
}

void RenderLayer::insertOnlyThisLayer()
{
    if (!m_parent && renderer().parent()) {
        // We need to connect ourselves when our renderer() has a parent.
        // Find our enclosingLayer and add ourselves.
        RenderLayer* parentLayer = renderer().parent()->enclosingLayer();
        ASSERT(parentLayer);
        RenderLayer* beforeChild = parentLayer->reflectionLayer() != this ? renderer().parent()->findNextLayer(parentLayer, &renderer()) : 0;
        parentLayer->addChild(this, beforeChild);
    }

    // Remove all descendant layers from the hierarchy and add them to the new position.
    for (auto& child : childrenOfType<RenderElement>(renderer()))
        child.moveLayers(m_parent, this);

    // Clear out all the clip rects.
    clearClipRectsIncludingDescendants();
}

void RenderLayer::convertToPixelSnappedLayerCoords(const RenderLayer* ancestorLayer, IntPoint& roundedLocation, ColumnOffsetAdjustment adjustForColumns) const
{
    LayoutPoint location = convertToLayerCoords(ancestorLayer, roundedLocation, adjustForColumns);
    roundedLocation = roundedIntPoint(location);
}

// Returns the layer reached on the walk up towards the ancestor.
static inline const RenderLayer* accumulateOffsetTowardsAncestor(const RenderLayer* layer, const RenderLayer* ancestorLayer, LayoutPoint& location, RenderLayer::ColumnOffsetAdjustment adjustForColumns)
{
    ASSERT(ancestorLayer != layer);

    const RenderLayerModelObject& renderer = layer->renderer();
    EPosition position = renderer.style().position();

    // FIXME: Special casing RenderFlowThread so much for fixed positioning here is not great.
    RenderFlowThread* fixedFlowThreadContainer = position == FixedPosition ? renderer.flowThreadContainingBlock() : 0;
    if (fixedFlowThreadContainer && !fixedFlowThreadContainer->isOutOfFlowPositioned())
        fixedFlowThreadContainer = 0;

    // FIXME: Positioning of out-of-flow(fixed, absolute) elements collected in a RenderFlowThread
    // may need to be revisited in a future patch.
    // If the fixed renderer is inside a RenderFlowThread, we should not compute location using localToAbsolute,
    // since localToAbsolute maps the coordinates from named flow to regions coordinates and regions can be
    // positioned in a completely different place in the viewport (RenderView).
    if (position == FixedPosition && !fixedFlowThreadContainer && (!ancestorLayer || ancestorLayer == renderer.view().layer())) {
        // If the fixed layer's container is the root, just add in the offset of the view. We can obtain this by calling
        // localToAbsolute() on the RenderView.
        FloatPoint absPos = renderer.localToAbsolute(FloatPoint(), IsFixed);
        location += LayoutSize(absPos.x(), absPos.y());
        return ancestorLayer;
    }

    // For the fixed positioned elements inside a render flow thread, we should also skip the code path below
    // Otherwise, for the case of ancestorLayer == rootLayer and fixed positioned element child of a transformed
    // element in render flow thread, we will hit the fixed positioned container before hitting the ancestor layer.
    if (position == FixedPosition && !fixedFlowThreadContainer) {
        // For a fixed layers, we need to walk up to the root to see if there's a fixed position container
        // (e.g. a transformed layer). It's an error to call offsetFromAncestor() across a layer with a transform,
        // so we should always find the ancestor at or before we find the fixed position container.
        RenderLayer* fixedPositionContainerLayer = 0;
        bool foundAncestor = false;
        for (RenderLayer* currLayer = layer->parent(); currLayer; currLayer = currLayer->parent()) {
            if (currLayer == ancestorLayer)
                foundAncestor = true;

            if (isFixedPositionedContainer(currLayer)) {
                fixedPositionContainerLayer = currLayer;
                ASSERT_UNUSED(foundAncestor, foundAncestor);
                break;
            }
        }
        
        ASSERT(fixedPositionContainerLayer); // We should have hit the RenderView's layer at least.

        if (fixedPositionContainerLayer != ancestorLayer) {
            LayoutSize fixedContainerCoords = layer->offsetFromAncestor(fixedPositionContainerLayer);
            LayoutSize ancestorCoords = ancestorLayer->offsetFromAncestor(fixedPositionContainerLayer);
            location += (fixedContainerCoords - ancestorCoords);
            return ancestorLayer;
        }
    }

    if (position == FixedPosition && fixedFlowThreadContainer) {
        ASSERT(ancestorLayer);
        if (ancestorLayer->isOutOfFlowRenderFlowThread()) {
            location += toLayoutSize(layer->location());
            return ancestorLayer;
        }

        if (ancestorLayer == renderer.view().layer()) {
            // Add location in flow thread coordinates.
            location += toLayoutSize(layer->location());

            // Add flow thread offset in view coordinates since the view may be scrolled.
            FloatPoint absPos = renderer.view().localToAbsolute(FloatPoint(), IsFixed);
            location += LayoutSize(absPos.x(), absPos.y());
            return ancestorLayer;
        }
    }

    RenderLayer* parentLayer;
    if (position == AbsolutePosition || position == FixedPosition) {
        // Do what enclosingPositionedAncestor() does, but check for ancestorLayer along the way.
        parentLayer = layer->parent();
        bool foundAncestorFirst = false;
        while (parentLayer) {
            // RenderFlowThread is a positioned container, child of RenderView, positioned at (0,0).
            // This implies that, for out-of-flow positioned elements inside a RenderFlowThread,
            // we are bailing out before reaching root layer.
            if (isPositionedContainer(parentLayer))
                break;

            if (parentLayer == ancestorLayer) {
                foundAncestorFirst = true;
                break;
            }

            parentLayer = parentLayer->parent();
        }

        // We should not reach RenderView layer past the RenderFlowThread layer for any
        // children of the RenderFlowThread.
        if (renderer.flowThreadContainingBlock() && !layer->isOutOfFlowRenderFlowThread())
            ASSERT(parentLayer != renderer.view().layer());

        if (foundAncestorFirst) {
            // Found ancestorLayer before the abs. positioned container, so compute offset of both relative
            // to enclosingPositionedAncestor and subtract.
            RenderLayer* positionedAncestor = parentLayer->enclosingPositionedAncestor();
            LayoutSize thisCoords = layer->offsetFromAncestor(positionedAncestor);
            LayoutSize ancestorCoords = ancestorLayer->offsetFromAncestor(positionedAncestor);
            location += (thisCoords - ancestorCoords);
            return ancestorLayer;
        }
    } else
        parentLayer = layer->parent();
    
    if (!parentLayer)
        return 0;

    location += toLayoutSize(layer->location());

    if (adjustForColumns == RenderLayer::AdjustForColumns) {
        if (RenderLayer* parentLayer = layer->parent()) {
            if (parentLayer->renderer().isRenderMultiColumnFlowThread()) {
                RenderRegion* region = toRenderMultiColumnFlowThread(parentLayer->renderer()).physicalTranslationFromFlowToRegion(location);
                if (region)
                    location.moveBy(region->topLeftLocation() + -parentLayer->renderBox()->topLeftLocation());
            }
        }
    }

    return parentLayer;
}

LayoutPoint RenderLayer::convertToLayerCoords(const RenderLayer* ancestorLayer, const LayoutPoint& location, ColumnOffsetAdjustment adjustForColumns) const
{
    if (ancestorLayer == this)
        return location;

    const RenderLayer* currLayer = this;
    LayoutPoint locationInLayerCoords = location;
    while (currLayer && currLayer != ancestorLayer)
        currLayer = accumulateOffsetTowardsAncestor(currLayer, ancestorLayer, locationInLayerCoords, adjustForColumns);
    return locationInLayerCoords;
}

LayoutSize RenderLayer::offsetFromAncestor(const RenderLayer* ancestorLayer) const
{
    return toLayoutSize(convertToLayerCoords(ancestorLayer, LayoutPoint()));
}

#if PLATFORM(IOS)
bool RenderLayer::hasAcceleratedTouchScrolling() const
{
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    if (!scrollsOverflow())
        return false;

    Settings* settings = renderer().document().settings();
    // FIXME: settings should not be null at this point. If you find a reliable way to hit this assertion, please file a bug.
    // See <rdar://problem/10266101>.
    ASSERT(settings);

    return renderer().style().useTouchOverflowScrolling() || (settings && settings->alwaysUseAcceleratedOverflowScroll());
#else
    return false;
#endif
}

bool RenderLayer::hasTouchScrollableOverflow() const
{
    return hasAcceleratedTouchScrolling() && (hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());
}

#if ENABLE(TOUCH_EVENTS)
bool RenderLayer::handleTouchEvent(const PlatformTouchEvent& touchEvent)
{
    // If we have accelerated scrolling, let the scrolling be handled outside of WebKit.
    if (hasTouchScrollableOverflow())
        return false;

    return ScrollableArea::handleTouchEvent(touchEvent);
}
#endif

void RenderLayer::registerAsTouchEventListenerForScrolling()
{
    if (!renderer().element() || m_registeredAsTouchEventListenerForScrolling)
        return;
    
    renderer().document().addTouchEventListener(renderer().element());
    m_registeredAsTouchEventListenerForScrolling = true;
}

void RenderLayer::unregisterAsTouchEventListenerForScrolling()
{
    if (!renderer().element() || !m_registeredAsTouchEventListenerForScrolling)
        return;

    renderer().document().removeTouchEventListener(renderer().element());
    m_registeredAsTouchEventListenerForScrolling = false;
}
#endif // PLATFORM(IOS)

bool RenderLayer::usesCompositedScrolling() const
{
    return isComposited() && backing()->scrollingLayer();
}

bool RenderLayer::needsCompositedScrolling() const
{
    return m_needsCompositedScrolling;
}

void RenderLayer::updateNeedsCompositedScrolling()
{
    bool oldNeedsCompositedScrolling = m_needsCompositedScrolling;

    if (!renderer().view().frameView().containsScrollableArea(this))
        m_needsCompositedScrolling = false;
    else {
        bool forceUseCompositedScrolling = acceleratedCompositingForOverflowScrollEnabled()
            && canBeStackingContainer()
            && !hasOutOfFlowPositionedDescendant();

#if !PLATFORM(IOS) && ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
        m_needsCompositedScrolling = forceUseCompositedScrolling || renderer().style().useTouchOverflowScrolling();
#else
        // On iOS we don't want to opt into accelerated composited scrolling, which creates scroll bar
        // layers in WebCore, because we use UIKit to composite our scroll bars.
        m_needsCompositedScrolling = forceUseCompositedScrolling;
#endif
    }

    if (oldNeedsCompositedScrolling != m_needsCompositedScrolling) {
        updateSelfPaintingLayer();
        if (isStackingContainer())
            dirtyZOrderLists();
        else
            clearZOrderLists();

        dirtyStackingContainerZOrderLists();

        compositor().setShouldReevaluateCompositingAfterLayout();
        compositor().setCompositingLayersNeedRebuild();
    }
}

static inline int adjustedScrollDelta(int beginningDelta) {
    // This implemention matches Firefox's.
    // http://mxr.mozilla.org/firefox/source/toolkit/content/widgets/browser.xml#856.
    const int speedReducer = 12;

    int adjustedDelta = beginningDelta / speedReducer;
    if (adjustedDelta > 1)
        adjustedDelta = static_cast<int>(adjustedDelta * sqrt(static_cast<double>(adjustedDelta))) - 1;
    else if (adjustedDelta < -1)
        adjustedDelta = static_cast<int>(adjustedDelta * sqrt(static_cast<double>(-adjustedDelta))) + 1;

    return adjustedDelta;
}

static inline IntSize adjustedScrollDelta(const IntSize& delta)
{
    return IntSize(adjustedScrollDelta(delta.width()), adjustedScrollDelta(delta.height()));
}

void RenderLayer::panScrollFromPoint(const IntPoint& sourcePoint)
{
    IntPoint lastKnownMousePosition = renderer().frame().eventHandler().lastKnownMousePosition();
    
    // We need to check if the last known mouse position is out of the window. When the mouse is out of the window, the position is incoherent
    static IntPoint previousMousePosition;
    if (lastKnownMousePosition.x() < 0 || lastKnownMousePosition.y() < 0)
        lastKnownMousePosition = previousMousePosition;
    else
        previousMousePosition = lastKnownMousePosition;

    IntSize delta = lastKnownMousePosition - sourcePoint;

    if (abs(delta.width()) <= ScrollView::noPanScrollRadius) // at the center we let the space for the icon
        delta.setWidth(0);
    if (abs(delta.height()) <= ScrollView::noPanScrollRadius)
        delta.setHeight(0);

    scrollByRecursively(adjustedScrollDelta(delta), ScrollOffsetClamped);
}

void RenderLayer::scrollByRecursively(const IntSize& delta, ScrollOffsetClamping clamp, ScrollableArea** scrolledArea)
{
    if (delta.isZero())
        return;

    bool restrictedByLineClamp = false;
    if (renderer().parent())
        restrictedByLineClamp = !renderer().parent()->style().lineClamp().isNone();

    if (renderer().hasOverflowClip() && !restrictedByLineClamp) {
        IntSize newScrollOffset = scrollOffset() + delta;
        scrollToOffset(newScrollOffset, clamp);
        if (scrolledArea)
            *scrolledArea = this;

        // If this layer can't do the scroll we ask the next layer up that can scroll to try
        IntSize remainingScrollOffset = newScrollOffset - scrollOffset();
        if (!remainingScrollOffset.isZero() && renderer().parent()) {
            if (RenderLayer* scrollableLayer = enclosingScrollableLayer())
                scrollableLayer->scrollByRecursively(remainingScrollOffset, clamp, scrolledArea);

            renderer().frame().eventHandler().updateAutoscrollRenderer();
        }
    } else {
        // If we are here, we were called on a renderer that can be programmatically scrolled, but doesn't
        // have an overflow clip. Which means that it is a document node that can be scrolled.
        renderer().view().frameView().scrollBy(delta);
        if (scrolledArea)
            *scrolledArea = &renderer().view().frameView();

        // FIXME: If we didn't scroll the whole way, do we want to try looking at the frames ownerElement? 
        // https://bugs.webkit.org/show_bug.cgi?id=28237
    }
}

IntSize RenderLayer::clampScrollOffset(const IntSize& scrollOffset) const
{
    RenderBox* box = renderBox();
    ASSERT(box);

    int maxX = scrollWidth() - box->pixelSnappedClientWidth();
    int maxY = scrollHeight() - box->pixelSnappedClientHeight();

    int x = std::max(std::min(scrollOffset.width(), maxX), 0);
    int y = std::max(std::min(scrollOffset.height(), maxY), 0);
    return IntSize(x, y);
}

void RenderLayer::scrollToOffset(const IntSize& scrollOffset, ScrollOffsetClamping clamp)
{
    IntSize newScrollOffset = clamp == ScrollOffsetClamped ? clampScrollOffset(scrollOffset) : scrollOffset;
    if (newScrollOffset != this->scrollOffset())
        scrollToOffsetWithoutAnimation(IntPoint(newScrollOffset));
}

void RenderLayer::scrollTo(int x, int y)
{
    RenderBox* box = renderBox();
    if (!box)
        return;

    if (box->style().overflowX() != OMARQUEE) {
        // Ensure that the dimensions will be computed if they need to be (for overflow:hidden blocks).
        if (m_scrollDimensionsDirty)
            computeScrollDimensions();
#if PLATFORM(IOS)
        if (adjustForIOSCaretWhenScrolling()) {
            int maxX = scrollWidth() - box->clientWidth();
            if (x > maxX - caretWidth) {
                x += caretWidth;
                if (x <= caretWidth)
                    x = 0;
            } else if (x < m_scrollOffset.width() - caretWidth)
                x -= caretWidth;
        }
#endif
    }
    
    // FIXME: Eventually, we will want to perform a blit.  For now never
    // blit, since the check for blitting is going to be very
    // complicated (since it will involve testing whether our layer
    // is either occluded by another layer or clipped by an enclosing
    // layer or contains fixed backgrounds, etc.).
    IntSize newScrollOffset = IntSize(x - scrollOrigin().x(), y - scrollOrigin().y());
    if (m_scrollOffset == newScrollOffset) {
#if PLATFORM(IOS)
        if (m_requiresScrollBoundsOriginUpdate)
            updateCompositingLayersAfterScroll();
#endif
        return;
    }
    
    IntPoint oldPosition = IntPoint(m_scrollOffset);
    m_scrollOffset = newScrollOffset;

    InspectorInstrumentation::willScrollLayer(&renderer().frame());

    RenderView& view = renderer().view();

    // Update the positions of our child layers (if needed as only fixed layers should be impacted by a scroll).
    // We don't update compositing layers, because we need to do a deep update from the compositing ancestor.
    bool inLayout = view.frameView().isInLayout();
    if (!inLayout) {
        // If we're in the middle of layout, we'll just update layers once layout has finished.
        updateLayerPositionsAfterOverflowScroll();
        // Update regions, scrolling may change the clip of a particular region.
#if ENABLE(DASHBOARD_SUPPORT)
        view.frameView().updateAnnotatedRegions();
#endif
        view.frameView().updateWidgetPositions();

        if (!m_updatingMarqueePosition) {
            // Avoid updating compositing layers if, higher on the stack, we're already updating layer
            // positions. Updating layer positions requires a full walk of up-to-date RenderLayers, and
            // in this case we're still updating their positions; we'll update compositing layers later
            // when that completes.
            updateCompositingLayersAfterScroll();
        }

#if PLATFORM(IOS) && ENABLE(TOUCH_EVENTS)
        renderer().document().dirtyTouchEventRects();
#endif
    }

    Frame& frame = renderer().frame();
    RenderLayerModelObject* repaintContainer = renderer().containerForRepaint();
    // The caret rect needs to be invalidated after scrolling
    frame.selection().setCaretRectNeedsUpdate();

    FloatQuad quadForFakeMouseMoveEvent = FloatQuad(m_repaintRect);
    if (repaintContainer)
        quadForFakeMouseMoveEvent = repaintContainer->localToAbsoluteQuad(quadForFakeMouseMoveEvent);
    frame.eventHandler().dispatchFakeMouseMoveEventSoonInQuad(quadForFakeMouseMoveEvent);

    bool requiresRepaint = true;
    if (compositor().inCompositingMode() && usesCompositedScrolling())
        requiresRepaint = false;

    // Just schedule a full repaint of our object.
    if (requiresRepaint)
        renderer().repaintUsingContainer(repaintContainer, m_repaintRect);

    // Schedule the scroll and scroll-related DOM events.
    if (Element* element = renderer().element()) {
        element->document().eventQueue().enqueueOrDispatchScrollEvent(*element);
        element->document().sendWillRevealEdgeEventsIfNeeded(oldPosition, IntPoint(newScrollOffset), visibleContentRect(), contentsSize(), element);
    }

    InspectorInstrumentation::didScrollLayer(&frame);
    if (scrollsOverflow())
        view.frameView().didChangeScrollOffset();

    view.frameView().resumeVisibleImageAnimationsIncludingSubframes();
}

static inline bool frameElementAndViewPermitScroll(HTMLFrameElementBase* frameElementBase, FrameView* frameView) 
{
    // If scrollbars aren't explicitly forbidden, permit scrolling.
    if (frameElementBase && frameElementBase->scrollingMode() != ScrollbarAlwaysOff)
        return true;

    // If scrollbars are forbidden, user initiated scrolls should obviously be ignored.
    if (frameView->wasScrolledByUser())
        return false;

    // Forbid autoscrolls when scrollbars are off, but permits other programmatic scrolls,
    // like navigation to an anchor.
    return !frameView->frame().eventHandler().autoscrollInProgress();
}

void RenderLayer::scrollRectToVisible(const LayoutRect& rect, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    RenderLayer* parentLayer = 0;
    LayoutRect newRect = rect;

    // We may end up propagating a scroll event. It is important that we suspend events until 
    // the end of the function since they could delete the layer or the layer's renderer().
    FrameView& frameView = renderer().view().frameView();

    bool restrictedByLineClamp = false;
    if (renderer().parent()) {
        parentLayer = renderer().parent()->enclosingLayer();
        restrictedByLineClamp = !renderer().parent()->style().lineClamp().isNone();
    }

    if (renderer().hasOverflowClip() && !restrictedByLineClamp) {
        // Don't scroll to reveal an overflow layer that is restricted by the -webkit-line-clamp property.
        // This will prevent us from revealing text hidden by the slider in Safari RSS.
        RenderBox* box = renderBox();
        ASSERT(box);
        LayoutRect localExposeRect(box->absoluteToLocalQuad(FloatQuad(FloatRect(rect)), UseTransforms).boundingBox());
        LayoutRect layerBounds(0, 0, box->clientWidth(), box->clientHeight());
        LayoutRect r = getRectToExpose(layerBounds, layerBounds, localExposeRect, alignX, alignY);

        IntSize clampedScrollOffset = clampScrollOffset(scrollOffset() + toIntSize(roundedIntRect(r).location()));
        if (clampedScrollOffset != scrollOffset()) {
            IntSize oldScrollOffset = scrollOffset();
            scrollToOffset(clampedScrollOffset);
            IntSize scrollOffsetDifference = scrollOffset() - oldScrollOffset;
            localExposeRect.move(-scrollOffsetDifference);
            newRect = LayoutRect(box->localToAbsoluteQuad(FloatQuad(FloatRect(localExposeRect)), UseTransforms).boundingBox());
        }
    } else if (!parentLayer && renderer().isBox() && renderBox()->canBeProgramaticallyScrolled()) {
        Element* ownerElement = renderer().document().ownerElement();

        if (ownerElement && ownerElement->renderer()) {
            HTMLFrameElementBase* frameElementBase = 0;

            if (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag))
                frameElementBase = toHTMLFrameElementBase(ownerElement);

            if (frameElementAndViewPermitScroll(frameElementBase, &frameView)) {
                LayoutRect viewRect = frameView.visibleContentRect(LegacyIOSDocumentVisibleRect);
                LayoutRect exposeRect = getRectToExpose(viewRect, viewRect, rect, alignX, alignY);

                int xOffset = roundToInt(exposeRect.x());
                int yOffset = roundToInt(exposeRect.y());
                // Adjust offsets if they're outside of the allowable range.
                xOffset = std::max(0, std::min(frameView.contentsWidth(), xOffset));
                yOffset = std::max(0, std::min(frameView.contentsHeight(), yOffset));

                frameView.setScrollPosition(IntPoint(xOffset, yOffset));
                if (frameView.safeToPropagateScrollToParent()) {
                    parentLayer = ownerElement->renderer()->enclosingLayer();
                    // FIXME: This doesn't correctly convert the rect to
                    // absolute coordinates in the parent.
                    newRect.setX(rect.x() - frameView.scrollX() + frameView.x());
                    newRect.setY(rect.y() - frameView.scrollY() + frameView.y());
                } else
                    parentLayer = 0;
            }
        } else {
#if !PLATFORM(IOS)
            LayoutRect viewRect = frameView.visibleContentRect();
            LayoutRect visibleRectRelativeToDocument = viewRect;
            IntSize documentScrollOffsetRelativeToScrollableAreaOrigin = frameView.documentScrollOffsetRelativeToScrollableAreaOrigin();
            visibleRectRelativeToDocument.setLocation(IntPoint(documentScrollOffsetRelativeToScrollableAreaOrigin.width(), documentScrollOffsetRelativeToScrollableAreaOrigin.height()));
#else
            LayoutRect viewRect = frameView.unobscuredContentRect();
            LayoutRect visibleRectRelativeToDocument = viewRect;
#endif

            LayoutRect r = getRectToExpose(viewRect, visibleRectRelativeToDocument, rect, alignX, alignY);
                
            frameView.setScrollPosition(roundedIntPoint(r.location()));

            // This is the outermost view of a web page, so after scrolling this view we
            // scroll its container by calling Page::scrollRectIntoView.
            // This only has an effect on the Mac platform in applications
            // that put web views into scrolling containers, such as Mac OS X Mail.
            // The canAutoscroll function in EventHandler also knows about this.
            if (Page* page = frameView.frame().page())
                page->chrome().scrollRectIntoView(pixelSnappedIntRect(rect));
        }
    }
    
    if (renderer().frame().eventHandler().autoscrollInProgress())
        parentLayer = enclosingScrollableLayer();

    if (parentLayer)
        parentLayer->scrollRectToVisible(newRect, alignX, alignY);
}

void RenderLayer::updateCompositingLayersAfterScroll()
{
    if (compositor().inCompositingMode()) {
        // Our stacking container is guaranteed to contain all of our descendants that may need
        // repositioning, so update compositing layers from there.
        if (RenderLayer* compositingAncestor = stackingContainer()->enclosingCompositingLayer()) {
            if (usesCompositedScrolling() && !hasOutOfFlowPositionedDescendant())
                compositor().updateCompositingLayers(CompositingUpdateOnCompositedScroll, compositingAncestor);
            else
                compositor().updateCompositingLayers(CompositingUpdateOnScroll, compositingAncestor);
        }
    }
}

LayoutRect RenderLayer::getRectToExpose(const LayoutRect &visibleRect, const LayoutRect &visibleRectRelativeToDocument, const LayoutRect &exposeRect, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    // Determine the appropriate X behavior.
    ScrollBehavior scrollX;
    LayoutRect exposeRectX(exposeRect.x(), visibleRect.y(), exposeRect.width(), visibleRect.height());
    LayoutUnit intersectWidth = intersection(visibleRect, exposeRectX).width();
    if (intersectWidth == exposeRect.width() || intersectWidth >= MIN_INTERSECT_FOR_REVEAL)
        // If the rectangle is fully visible, use the specified visible behavior.
        // If the rectangle is partially visible, but over a certain threshold,
        // then treat it as fully visible to avoid unnecessary horizontal scrolling
        scrollX = ScrollAlignment::getVisibleBehavior(alignX);
    else if (intersectWidth == visibleRect.width()) {
        // If the rect is bigger than the visible area, don't bother trying to center. Other alignments will work.
        scrollX = ScrollAlignment::getVisibleBehavior(alignX);
        if (scrollX == alignCenter)
            scrollX = noScroll;
    } else if (intersectWidth > 0)
        // If the rectangle is partially visible, but not above the minimum threshold, use the specified partial behavior
        scrollX = ScrollAlignment::getPartialBehavior(alignX);
    else
        scrollX = ScrollAlignment::getHiddenBehavior(alignX);
    // If we're trying to align to the closest edge, and the exposeRect is further right
    // than the visibleRect, and not bigger than the visible area, then align with the right.
    if (scrollX == alignToClosestEdge && exposeRect.maxX() > visibleRect.maxX() && exposeRect.width() < visibleRect.width())
        scrollX = alignRight;

    // Given the X behavior, compute the X coordinate.
    LayoutUnit x;
    if (scrollX == noScroll) 
        x = visibleRect.x();
    else if (scrollX == alignRight)
        x = exposeRect.maxX() - visibleRect.width();
    else if (scrollX == alignCenter)
        x = exposeRect.x() + (exposeRect.width() - visibleRect.width()) / 2;
    else
        x = exposeRect.x();

    // Determine the appropriate Y behavior.
    ScrollBehavior scrollY;
    LayoutRect exposeRectY(visibleRect.x(), exposeRect.y(), visibleRect.width(), exposeRect.height());
    LayoutUnit intersectHeight = intersection(visibleRectRelativeToDocument, exposeRectY).height();
    if (intersectHeight == exposeRect.height())
        // If the rectangle is fully visible, use the specified visible behavior.
        scrollY = ScrollAlignment::getVisibleBehavior(alignY);
    else if (intersectHeight == visibleRect.height()) {
        // If the rect is bigger than the visible area, don't bother trying to center. Other alignments will work.
        scrollY = ScrollAlignment::getVisibleBehavior(alignY);
        if (scrollY == alignCenter)
            scrollY = noScroll;
    } else if (intersectHeight > 0)
        // If the rectangle is partially visible, use the specified partial behavior
        scrollY = ScrollAlignment::getPartialBehavior(alignY);
    else
        scrollY = ScrollAlignment::getHiddenBehavior(alignY);
    // If we're trying to align to the closest edge, and the exposeRect is further down
    // than the visibleRect, and not bigger than the visible area, then align with the bottom.
    if (scrollY == alignToClosestEdge && exposeRect.maxY() > visibleRect.maxY() && exposeRect.height() < visibleRect.height())
        scrollY = alignBottom;

    // Given the Y behavior, compute the Y coordinate.
    LayoutUnit y;
    if (scrollY == noScroll) 
        y = visibleRect.y();
    else if (scrollY == alignBottom)
        y = exposeRect.maxY() - visibleRect.height();
    else if (scrollY == alignCenter)
        y = exposeRect.y() + (exposeRect.height() - visibleRect.height()) / 2;
    else
        y = exposeRect.y();

    return LayoutRect(LayoutPoint(x, y), visibleRect.size());
}

void RenderLayer::autoscroll(const IntPoint& position)
{
    IntPoint currentDocumentPosition = renderer().view().frameView().windowToContents(position);
    scrollRectToVisible(LayoutRect(currentDocumentPosition, LayoutSize(1, 1)), ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded);
}

bool RenderLayer::canResize() const
{
    // We need a special case for <iframe> because they never have
    // hasOverflowClip(). However, they do "implicitly" clip their contents, so
    // we want to allow resizing them also.
    return (renderer().hasOverflowClip() || renderer().isRenderIFrame()) && renderer().style().resize() != RESIZE_NONE;
}

void RenderLayer::resize(const PlatformMouseEvent& evt, const LayoutSize& oldOffset)
{
    // FIXME: This should be possible on generated content but is not right now.
    if (!inResizeMode() || !canResize() || !renderer().element())
        return;

    // FIXME: The only case where renderer->element()->renderer() != renderer is with continuations. Do they matter here?
    // If they do it would still be better to deal with them explicitly.
    Element* element = renderer().element();
    RenderBox* renderer = toRenderBox(element->renderer());

    Document& document = element->document();
    if (!document.frame()->eventHandler().mousePressed())
        return;

    float zoomFactor = renderer->style().effectiveZoom();

    LayoutSize newOffset = offsetFromResizeCorner(document.view()->windowToContents(evt.position()));
    newOffset.setWidth(newOffset.width() / zoomFactor);
    newOffset.setHeight(newOffset.height() / zoomFactor);
    
    LayoutSize currentSize = LayoutSize(renderer->width() / zoomFactor, renderer->height() / zoomFactor);
    LayoutSize minimumSize = element->minimumSizeForResizing().shrunkTo(currentSize);
    element->setMinimumSizeForResizing(minimumSize);
    
    LayoutSize adjustedOldOffset = LayoutSize(oldOffset.width() / zoomFactor, oldOffset.height() / zoomFactor);
    if (renderer->style().shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        newOffset.setWidth(-newOffset.width());
        adjustedOldOffset.setWidth(-adjustedOldOffset.width());
    }
    
    LayoutSize difference = (currentSize + newOffset - adjustedOldOffset).expandedTo(minimumSize) - currentSize;

    StyledElement* styledElement = toStyledElement(element);
    bool isBoxSizingBorder = renderer->style().boxSizing() == BORDER_BOX;

    EResize resize = renderer->style().resize();
    if (resize != RESIZE_VERTICAL && difference.width()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            styledElement->setInlineStyleProperty(CSSPropertyMarginLeft, renderer->marginLeft() / zoomFactor, CSSPrimitiveValue::CSS_PX);
            styledElement->setInlineStyleProperty(CSSPropertyMarginRight, renderer->marginRight() / zoomFactor, CSSPrimitiveValue::CSS_PX);
        }
        LayoutUnit baseWidth = renderer->width() - (isBoxSizingBorder ? LayoutUnit() : renderer->horizontalBorderAndPaddingExtent());
        baseWidth = baseWidth / zoomFactor;
        styledElement->setInlineStyleProperty(CSSPropertyWidth, roundToInt(baseWidth + difference.width()), CSSPrimitiveValue::CSS_PX);
    }

    if (resize != RESIZE_HORIZONTAL && difference.height()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            styledElement->setInlineStyleProperty(CSSPropertyMarginTop, renderer->marginTop() / zoomFactor, CSSPrimitiveValue::CSS_PX);
            styledElement->setInlineStyleProperty(CSSPropertyMarginBottom, renderer->marginBottom() / zoomFactor, CSSPrimitiveValue::CSS_PX);
        }
        LayoutUnit baseHeight = renderer->height() - (isBoxSizingBorder ? LayoutUnit() : renderer->verticalBorderAndPaddingExtent());
        baseHeight = baseHeight / zoomFactor;
        styledElement->setInlineStyleProperty(CSSPropertyHeight, roundToInt(baseHeight + difference.height()), CSSPrimitiveValue::CSS_PX);
    }

    document.updateLayout();

    // FIXME (Radar 4118564): We should also autoscroll the window as necessary to keep the point under the cursor in view.
}

int RenderLayer::scrollSize(ScrollbarOrientation orientation) const
{
    Scrollbar* scrollbar = ((orientation == HorizontalScrollbar) ? m_hBar : m_vBar).get();
    return scrollbar ? (scrollbar->totalSize() - scrollbar->visibleSize()) : 0;
}

void RenderLayer::setScrollOffset(const IntPoint& offset)
{
    scrollTo(offset.x(), offset.y());
}

int RenderLayer::scrollPosition(Scrollbar* scrollbar) const
{
    if (scrollbar->orientation() == HorizontalScrollbar)
        return scrollXOffset();
    if (scrollbar->orientation() == VerticalScrollbar)
        return scrollYOffset();
    return 0;
}

IntPoint RenderLayer::scrollPosition() const
{
    return IntPoint(m_scrollOffset);
}

IntPoint RenderLayer::minimumScrollPosition() const
{
    return -scrollOrigin();
}

IntPoint RenderLayer::maximumScrollPosition() const
{
    // FIXME: m_scrollSize may not be up-to-date if m_scrollDimensionsDirty is true.
    return -scrollOrigin() + roundedIntSize(m_scrollSize) - visibleContentRectIncludingScrollbars(ContentsVisibleRect).size();
}

IntRect RenderLayer::visibleContentRectInternal(VisibleContentRectIncludesScrollbars scrollbarInclusion, VisibleContentRectBehavior) const
{
    int verticalScrollbarWidth = 0;
    int horizontalScrollbarHeight = 0;
    if (showsOverflowControls() && scrollbarInclusion == IncludeScrollbars) {
        verticalScrollbarWidth = (verticalScrollbar() && !verticalScrollbar()->isOverlayScrollbar()) ? verticalScrollbar()->width() : 0;
        horizontalScrollbarHeight = (horizontalScrollbar() && !horizontalScrollbar()->isOverlayScrollbar()) ? horizontalScrollbar()->height() : 0;
    }
    
    return IntRect(IntPoint(scrollXOffset(), scrollYOffset()),
                   IntSize(std::max(0, m_layerSize.width() - verticalScrollbarWidth),
                           std::max(0, m_layerSize.height() - horizontalScrollbarHeight)));
}

IntSize RenderLayer::overhangAmount() const
{
    return IntSize();
}

bool RenderLayer::isActive() const
{
    Page* page = renderer().frame().page();
    return page && page->focusController().isActive();
}

static int cornerStart(const RenderLayer* layer, int minX, int maxX, int thickness)
{
    if (layer->renderer().style().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + layer->renderer().style().borderLeftWidth();
    return maxX - thickness - layer->renderer().style().borderRightWidth();
}

static LayoutRect cornerRect(const RenderLayer* layer, const LayoutRect& bounds)
{
    int horizontalThickness;
    int verticalThickness;
    if (!layer->verticalScrollbar() && !layer->horizontalScrollbar()) {
        // FIXME: This isn't right.  We need to know the thickness of custom scrollbars
        // even when they don't exist in order to set the resizer square size properly.
        horizontalThickness = ScrollbarTheme::theme()->scrollbarThickness();
        verticalThickness = horizontalThickness;
    } else if (layer->verticalScrollbar() && !layer->horizontalScrollbar()) {
        horizontalThickness = layer->verticalScrollbar()->width();
        verticalThickness = horizontalThickness;
    } else if (layer->horizontalScrollbar() && !layer->verticalScrollbar()) {
        verticalThickness = layer->horizontalScrollbar()->height();
        horizontalThickness = verticalThickness;
    } else {
        horizontalThickness = layer->verticalScrollbar()->width();
        verticalThickness = layer->horizontalScrollbar()->height();
    }
    return LayoutRect(cornerStart(layer, bounds.x(), bounds.maxX(), horizontalThickness),
        bounds.maxY() - verticalThickness - layer->renderer().style().borderBottomWidth(),
        horizontalThickness, verticalThickness);
}

IntRect RenderLayer::scrollCornerRect() const
{
    // We have a scrollbar corner when a scrollbar is visible and not filling the entire length of the box.
    // This happens when:
    // (a) A resizer is present and at least one scrollbar is present
    // (b) Both scrollbars are present.
    bool hasHorizontalBar = horizontalScrollbar();
    bool hasVerticalBar = verticalScrollbar();
    bool hasResizer = renderer().style().resize() != RESIZE_NONE;
    if ((hasHorizontalBar && hasVerticalBar) || (hasResizer && (hasHorizontalBar || hasVerticalBar)))
        return pixelSnappedIntRect(cornerRect(this, renderBox()->borderBoxRect()));
    return IntRect();
}

static LayoutRect resizerCornerRect(const RenderLayer* layer, const LayoutRect& bounds)
{
    ASSERT(layer->renderer().isBox());
    if (layer->renderer().style().resize() == RESIZE_NONE)
        return LayoutRect();
    return cornerRect(layer, bounds);
}

LayoutRect RenderLayer::scrollCornerAndResizerRect() const
{
    RenderBox* box = renderBox();
    if (!box)
        return LayoutRect();
    LayoutRect scrollCornerAndResizer = scrollCornerRect();
    if (scrollCornerAndResizer.isEmpty())
        scrollCornerAndResizer = resizerCornerRect(this, box->borderBoxRect());
    return scrollCornerAndResizer;
}

bool RenderLayer::isScrollCornerVisible() const
{
    ASSERT(renderer().isBox());
    return !scrollCornerRect().isEmpty();
}

IntRect RenderLayer::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& scrollbarRect) const
{
    IntRect rect = scrollbarRect;
    rect.move(scrollbarOffset(scrollbar));

    return renderer().view().frameView().convertFromRendererToContainingView(&renderer(), rect);
}

IntRect RenderLayer::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& parentRect) const
{
    IntRect rect = renderer().view().frameView().convertFromContainingViewToRenderer(&renderer(), parentRect);
    rect.move(-scrollbarOffset(scrollbar));
    return rect;
}

IntPoint RenderLayer::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& scrollbarPoint) const
{
    IntPoint point = scrollbarPoint;
    point.move(scrollbarOffset(scrollbar));
    return renderer().view().frameView().convertFromRendererToContainingView(&renderer(), point);
}

IntPoint RenderLayer::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& parentPoint) const
{
    IntPoint point = renderer().view().frameView().convertFromContainingViewToRenderer(&renderer(), parentPoint);
    point.move(-scrollbarOffset(scrollbar));
    return point;
}

IntSize RenderLayer::visibleSize() const
{
    if (!renderer().isBox())
        return IntSize();

    return IntSize(renderBox()->pixelSnappedClientWidth(), renderBox()->pixelSnappedClientHeight());
}

IntSize RenderLayer::contentsSize() const
{
    return IntSize(scrollWidth(), scrollHeight());
}

IntSize RenderLayer::scrollableContentsSize() const
{
    IntSize contentsSize = this->contentsSize();

    if (!hasScrollableHorizontalOverflow())
        contentsSize.setWidth(std::min(contentsSize.width(), visibleSize().width()));

    if (!hasScrollableVerticalOverflow())
        contentsSize.setHeight(std::min(contentsSize.height(), visibleSize().height()));

    return contentsSize;
}

bool RenderLayer::shouldSuspendScrollAnimations() const
{
    return renderer().view().frameView().shouldSuspendScrollAnimations();
}

#if PLATFORM(IOS)
void RenderLayer::didStartScroll()
{
    if (Page* page = renderer().frame().page())
        page->chrome().client().didStartOverflowScroll();
}

void RenderLayer::didEndScroll()
{
    if (Page* page = renderer().frame().page())
        page->chrome().client().didEndOverflowScroll();
}
    
void RenderLayer::didUpdateScroll()
{
    // Send this notification when we scroll, since this is how we keep selection updated.
    if (Page* page = renderer().frame().page())
        page->chrome().client().didLayout(ChromeClient::Scroll);
}
#endif

IntPoint RenderLayer::lastKnownMousePosition() const
{
    return renderer().frame().eventHandler().lastKnownMousePosition();
}

bool RenderLayer::isHandlingWheelEvent() const
{
    return renderer().frame().eventHandler().isHandlingWheelEvent();
}

IntRect RenderLayer::rectForHorizontalScrollbar(const IntRect& borderBoxRect) const
{
    if (!m_hBar)
        return IntRect();

    const RenderBox* box = renderBox();
    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(horizontalScrollbarStart(borderBoxRect.x()),
        borderBoxRect.maxY() - box->borderBottom() - m_hBar->height(),
        borderBoxRect.width() - (box->borderLeft() + box->borderRight()) - scrollCorner.width(),
        m_hBar->height());
}

IntRect RenderLayer::rectForVerticalScrollbar(const IntRect& borderBoxRect) const
{
    if (!m_vBar)
        return IntRect();

    const RenderBox* box = renderBox();
    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(verticalScrollbarStart(borderBoxRect.x(), borderBoxRect.maxX()),
        borderBoxRect.y() + box->borderTop(),
        m_vBar->width(),
        borderBoxRect.height() - (box->borderTop() + box->borderBottom()) - scrollCorner.height());
}

LayoutUnit RenderLayer::verticalScrollbarStart(int minX, int maxX) const
{
    const RenderBox* box = renderBox();
    if (renderer().style().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + box->borderLeft();
    return maxX - box->borderRight() - m_vBar->width();
}

LayoutUnit RenderLayer::horizontalScrollbarStart(int minX) const
{
    const RenderBox* box = renderBox();
    int x = minX + box->borderLeft();
    if (renderer().style().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        x += m_vBar ? m_vBar->width() : roundToInt(resizerCornerRect(this, box->borderBoxRect()).width());
    return x;
}

IntSize RenderLayer::scrollbarOffset(const Scrollbar* scrollbar) const
{
    RenderBox* box = renderBox();

    if (scrollbar == m_vBar.get())
        return IntSize(verticalScrollbarStart(0, box->width()), box->borderTop());

    if (scrollbar == m_hBar.get())
        return IntSize(horizontalScrollbarStart(0), box->height() - box->borderBottom() - scrollbar->height());
    
    ASSERT_NOT_REACHED();
    return IntSize();
}

void RenderLayer::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    if (!showsOverflowControls())
        return;

    if (scrollbar == m_vBar.get()) {
        if (GraphicsLayer* layer = layerForVerticalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
    } else {
        if (GraphicsLayer* layer = layerForHorizontalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
    }

    IntRect scrollRect = rect;
    RenderBox* box = renderBox();
    ASSERT(box);
    // If we are not yet inserted into the tree, there is no need to repaint.
    if (!box->parent())
        return;

    if (scrollbar == m_vBar.get())
        scrollRect.move(verticalScrollbarStart(0, box->width()), box->borderTop());
    else
        scrollRect.move(horizontalScrollbarStart(0), box->height() - box->borderBottom() - scrollbar->height());
    LayoutRect repaintRect = scrollRect;
    renderBox()->flipForWritingMode(repaintRect);
    renderer().repaintRectangle(repaintRect);
}

void RenderLayer::invalidateScrollCornerRect(const IntRect& rect)
{
    if (!showsOverflowControls())
        return;

    if (GraphicsLayer* layer = layerForScrollCorner()) {
        layer->setNeedsDisplayInRect(rect);
        return;
    }

    if (m_scrollCorner)
        m_scrollCorner->repaintRectangle(rect);
    if (m_resizer)
        m_resizer->repaintRectangle(rect);
}

static inline RenderElement* rendererForScrollbar(RenderLayerModelObject& renderer)
{
    if (Element* element = renderer.element()) {
        if (ShadowRoot* shadowRoot = element->containingShadowRoot()) {
            if (shadowRoot->type() == ShadowRoot::UserAgentShadowRoot)
                return shadowRoot->hostElement()->renderer();
        }
    }

    return &renderer;
}

PassRefPtr<Scrollbar> RenderLayer::createScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar> widget;
    RenderElement* actualRenderer = rendererForScrollbar(renderer());
    bool hasCustomScrollbarStyle = actualRenderer->isBox() && actualRenderer->style().hasPseudoStyle(SCROLLBAR);
    if (hasCustomScrollbarStyle)
        widget = RenderScrollbar::createCustomScrollbar(this, orientation, actualRenderer->element());
    else {
        widget = Scrollbar::createNativeScrollbar(this, orientation, RegularScrollbar);
        didAddScrollbar(widget.get(), orientation);
    }
    renderer().view().frameView().addChild(widget.get());
    return widget.release();
}

void RenderLayer::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_hBar : m_vBar;
    if (!scrollbar)
        return;

    if (!scrollbar->isCustomScrollbar())
        willRemoveScrollbar(scrollbar.get(), orientation);

    scrollbar->removeFromParent();
    scrollbar->disconnectFromScrollableArea();
    scrollbar = 0;
}

bool RenderLayer::scrollsOverflow() const
{
    if (!renderer().isBox())
        return false;

    return toRenderBox(renderer()).scrollsOverflow();
}

void RenderLayer::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasHorizontalScrollbar())
        return;

    if (hasScrollbar)
        m_hBar = createScrollbar(HorizontalScrollbar);
    else
        destroyScrollbar(HorizontalScrollbar);

    // Destroying or creating one bar can cause our scrollbar corner to come and go.  We need to update the opposite scrollbar's style.
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    // Force an update since we know the scrollbars have changed things.
#if ENABLE(DASHBOARD_SUPPORT)
    if (renderer().document().hasAnnotatedRegions())
        renderer().document().setAnnotatedRegionsDirty(true);
#endif
}

void RenderLayer::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasVerticalScrollbar())
        return;

    if (hasScrollbar)
        m_vBar = createScrollbar(VerticalScrollbar);
    else
        destroyScrollbar(VerticalScrollbar);

     // Destroying or creating one bar can cause our scrollbar corner to come and go.  We need to update the opposite scrollbar's style.
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    // Force an update since we know the scrollbars have changed things.
#if ENABLE(DASHBOARD_SUPPORT)
    if (renderer().document().hasAnnotatedRegions())
        renderer().document().setAnnotatedRegionsDirty(true);
#endif
}

ScrollableArea* RenderLayer::enclosingScrollableArea() const
{
    if (RenderLayer* scrollableLayer = enclosingScrollableLayer())
        return scrollableLayer;

    // FIXME: We should return the frame view here (or possibly an ancestor frame view,
    // if the frame view isn't scrollable.
    return 0;
}

int RenderLayer::verticalScrollbarWidth(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_vBar
        || !showsOverflowControls()
        || (m_vBar->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !m_vBar->shouldParticipateInHitTesting())))
        return 0;

    return m_vBar->width();
}

int RenderLayer::horizontalScrollbarHeight(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_hBar
        || !showsOverflowControls()
        || (m_hBar->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !m_hBar->shouldParticipateInHitTesting())))
        return 0;

    return m_hBar->height();
}

IntSize RenderLayer::offsetFromResizeCorner(const IntPoint& absolutePoint) const
{
    // Currently the resize corner is either the bottom right corner or the bottom left corner.
    // FIXME: This assumes the location is 0, 0. Is this guaranteed to always be the case?
    IntSize elementSize = size();
    if (renderer().style().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        elementSize.setWidth(0);
    IntPoint resizerPoint = IntPoint(elementSize);
    IntPoint localPoint = roundedIntPoint(absoluteToContents(absolutePoint));
    return localPoint - resizerPoint;
}

bool RenderLayer::hasOverflowControls() const
{
    return m_hBar || m_vBar || m_scrollCorner || renderer().style().resize() != RESIZE_NONE;
}

void RenderLayer::positionOverflowControls(const IntSize& offsetFromRoot)
{
    if (!m_hBar && !m_vBar && !canResize())
        return;
    
    RenderBox* box = renderBox();
    if (!box)
        return;

    const IntRect borderBox = box->pixelSnappedBorderBoxRect();
    const IntRect& scrollCorner = scrollCornerRect();
    IntRect absBounds(borderBox.location() + offsetFromRoot, borderBox.size());
    if (m_vBar) {
        IntRect vBarRect = rectForVerticalScrollbar(borderBox);
        vBarRect.move(offsetFromRoot);
        m_vBar->setFrameRect(vBarRect);
    }
    
    if (m_hBar) {
        IntRect hBarRect = rectForHorizontalScrollbar(borderBox);
        hBarRect.move(offsetFromRoot);
        m_hBar->setFrameRect(hBarRect);
    }
    
    if (m_scrollCorner)
        m_scrollCorner->setFrameRect(scrollCorner);
    if (m_resizer)
        m_resizer->setFrameRect(resizerCornerRect(this, borderBox));

    if (isComposited())
        backing()->positionOverflowControlsLayers();
}

int RenderLayer::scrollWidth() const
{
    ASSERT(renderBox());
    if (m_scrollDimensionsDirty)
        const_cast<RenderLayer*>(this)->computeScrollDimensions();
    return snapSizeToPixel(m_scrollSize.width(), renderBox()->clientLeft() + renderBox()->x());
}

int RenderLayer::scrollHeight() const
{
    ASSERT(renderBox());
    if (m_scrollDimensionsDirty)
        const_cast<RenderLayer*>(this)->computeScrollDimensions();
    return snapSizeToPixel(m_scrollSize.height(), renderBox()->clientTop() + renderBox()->y());
}

LayoutUnit RenderLayer::overflowTop() const
{
    RenderBox* box = renderBox();
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return overflowRect.y();
}

LayoutUnit RenderLayer::overflowBottom() const
{
    RenderBox* box = renderBox();
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return overflowRect.maxY();
}

LayoutUnit RenderLayer::overflowLeft() const
{
    RenderBox* box = renderBox();
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return overflowRect.x();
}

LayoutUnit RenderLayer::overflowRight() const
{
    RenderBox* box = renderBox();
    LayoutRect overflowRect(box->layoutOverflowRect());
    box->flipForWritingMode(overflowRect);
    return overflowRect.maxX();
}

void RenderLayer::computeScrollDimensions()
{
    RenderBox* box = renderBox();
    ASSERT(box);

    m_scrollDimensionsDirty = false;

    m_scrollSize.setWidth(overflowRight() - overflowLeft());
    m_scrollSize.setHeight(overflowBottom() - overflowTop());

    int scrollableLeftOverflow = overflowLeft() - box->borderLeft();
    int scrollableTopOverflow = overflowTop() - box->borderTop();
    setScrollOrigin(IntPoint(-scrollableLeftOverflow, -scrollableTopOverflow));
}

bool RenderLayer::hasScrollableHorizontalOverflow() const
{
    return hasHorizontalOverflow() && renderBox()->scrollsOverflowX();
}

bool RenderLayer::hasScrollableVerticalOverflow() const
{
    return hasVerticalOverflow() && renderBox()->scrollsOverflowY();
}

bool RenderLayer::hasHorizontalOverflow() const
{
    ASSERT(!m_scrollDimensionsDirty);

    return scrollWidth() > renderBox()->pixelSnappedClientWidth();
}

bool RenderLayer::hasVerticalOverflow() const
{
    ASSERT(!m_scrollDimensionsDirty);

    return scrollHeight() > renderBox()->pixelSnappedClientHeight();
}

void RenderLayer::updateScrollbarsAfterLayout()
{
    RenderBox* box = renderBox();
    ASSERT(box);

    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (box->style().appearance() == ListboxPart)
        return;

    bool hasHorizontalOverflow = this->hasHorizontalOverflow();
    bool hasVerticalOverflow = this->hasVerticalOverflow();

    // overflow:scroll should just enable/disable.
    if (renderer().style().overflowX() == OSCROLL)
        m_hBar->setEnabled(hasHorizontalOverflow);
    if (renderer().style().overflowY() == OSCROLL)
        m_vBar->setEnabled(hasVerticalOverflow);

    // overflow:auto may need to lay out again if scrollbars got added/removed.
    bool autoHorizontalScrollBarChanged = box->hasAutoHorizontalScrollbar() && (hasHorizontalScrollbar() != hasHorizontalOverflow);
    bool autoVerticalScrollBarChanged = box->hasAutoVerticalScrollbar() && (hasVerticalScrollbar() != hasVerticalOverflow);

    if (autoHorizontalScrollBarChanged || autoVerticalScrollBarChanged) {
        if (box->hasAutoHorizontalScrollbar())
            setHasHorizontalScrollbar(hasHorizontalOverflow);
        if (box->hasAutoVerticalScrollbar())
            setHasVerticalScrollbar(hasVerticalOverflow);

        updateSelfPaintingLayer();

        // Force an update since we know the scrollbars have changed things.
#if ENABLE(DASHBOARD_SUPPORT)
        if (renderer().document().hasAnnotatedRegions())
            renderer().document().setAnnotatedRegionsDirty(true);
#endif

        renderer().repaint();

        if (renderer().style().overflowX() == OAUTO || renderer().style().overflowY() == OAUTO) {
            if (!m_inOverflowRelayout) {
                // Our proprietary overflow: overlay value doesn't trigger a layout.
                m_inOverflowRelayout = true;
                renderer().setNeedsLayout(MarkOnlyThis);
                if (renderer().isRenderBlock()) {
                    RenderBlock& block = toRenderBlock(renderer());
                    block.scrollbarsChanged(autoHorizontalScrollBarChanged, autoVerticalScrollBarChanged);
                    block.layoutBlock(true);
                } else
                    renderer().layout();
                m_inOverflowRelayout = false;
            }
        }
    }

    // Set up the range (and page step/line step).
    if (m_hBar) {
        int clientWidth = box->pixelSnappedClientWidth();
        int pageStep = std::max(std::max<int>(clientWidth * Scrollbar::minFractionToStepWhenPaging(), clientWidth - Scrollbar::maxOverlapBetweenPages()), 1);
        m_hBar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
        m_hBar->setProportion(clientWidth, m_scrollSize.width().round());
    }
    if (m_vBar) {
        int clientHeight = box->pixelSnappedClientHeight();
        int pageStep = std::max(std::max<int>(clientHeight * Scrollbar::minFractionToStepWhenPaging(), clientHeight - Scrollbar::maxOverlapBetweenPages()), 1);
        m_vBar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
        m_vBar->setProportion(clientHeight, m_scrollSize.height().round());
    }

    updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());
}

void RenderLayer::updateScrollInfoAfterLayout()
{
    RenderBox* box = renderBox();
    if (!box)
        return;

    m_scrollDimensionsDirty = true;
    IntSize originalScrollOffset = scrollOffset();

    computeScrollDimensions();

    if (box->style().overflowX() != OMARQUEE) {
        // Layout may cause us to be at an invalid scroll position. In this case we need
        // to pull our scroll offsets back to the max (or push them up to the min).
        IntSize clampedScrollOffset = clampScrollOffset(scrollOffset());
#if PLATFORM(IOS)
        // FIXME: This looks wrong. The caret adjust mode should only be enabled on editing related entry points.
        // This code was added to fix an issue where the text insertion point would always be drawn on the right edge
        // of a text field whose content overflowed its bounds. See <rdar://problem/15579797> for more details.
        setAdjustForIOSCaretWhenScrolling(true);
#endif
        if (clampedScrollOffset != scrollOffset())
            scrollToOffset(clampedScrollOffset);

#if PLATFORM(IOS)
        setAdjustForIOSCaretWhenScrolling(false);
#endif
    }

    updateScrollbarsAfterLayout();

    if (originalScrollOffset != scrollOffset())
        scrollToOffsetWithoutAnimation(IntPoint(scrollOffset()));

    // Composited scrolling may need to be enabled or disabled if the amount of overflow changed.
    if (compositor().updateLayerCompositingState(*this))
        compositor().setCompositingLayersNeedRebuild();
}

bool RenderLayer::overflowControlsIntersectRect(const IntRect& localRect) const
{
    const IntRect borderBox = renderBox()->pixelSnappedBorderBoxRect();

    if (rectForHorizontalScrollbar(borderBox).intersects(localRect))
        return true;

    if (rectForVerticalScrollbar(borderBox).intersects(localRect))
        return true;

    if (scrollCornerRect().intersects(localRect))
        return true;
    
    if (resizerCornerRect(this, borderBox).intersects(localRect))
        return true;

    return false;
}

bool RenderLayer::showsOverflowControls() const
{
#if PLATFORM(IOS)
    // Don't render (custom) scrollbars if we have accelerated scrolling.
    if (hasAcceleratedTouchScrolling())
        return false;
#endif

    return true;
}

void RenderLayer::paintOverflowControls(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect, bool paintingOverlayControls)
{
    // Don't do anything if we have no overflow.
    if (!renderer().hasOverflowClip())
        return;

    if (!showsOverflowControls())
        return;

    // Overlay scrollbars paint in a second pass through the layer tree so that they will paint
    // on top of everything else. If this is the normal painting pass, paintingOverlayControls
    // will be false, and we should just tell the root layer that there are overlay scrollbars
    // that need to be painted. That will cause the second pass through the layer tree to run,
    // and we'll paint the scrollbars then. In the meantime, cache tx and ty so that the 
    // second pass doesn't need to re-enter the RenderTree to get it right.
    if (hasOverlayScrollbars() && !paintingOverlayControls) {
        m_cachedOverlayScrollbarOffset = paintOffset;

        // It's not necessary to do the second pass if the scrollbars paint into layers.
        if ((m_hBar && layerForHorizontalScrollbar()) || (m_vBar && layerForVerticalScrollbar()))
            return;
        IntRect localDamgeRect = damageRect;
        localDamgeRect.moveBy(-paintOffset);
        if (!overflowControlsIntersectRect(localDamgeRect))
            return;

        RenderLayer* paintingRoot = enclosingCompositingLayer();
        if (!paintingRoot)
            paintingRoot = renderer().view().layer();

        paintingRoot->setContainsDirtyOverlayScrollbars(true);
        return;
    }

    // This check is required to avoid painting custom CSS scrollbars twice.
    if (paintingOverlayControls && !hasOverlayScrollbars())
        return;

    IntPoint adjustedPaintOffset = paintOffset;
    if (paintingOverlayControls)
        adjustedPaintOffset = m_cachedOverlayScrollbarOffset;

    // Move the scrollbar widgets if necessary.  We normally move and resize widgets during layout, but sometimes
    // widgets can move without layout occurring (most notably when you scroll a document that
    // contains fixed positioned elements).
    positionOverflowControls(toIntSize(adjustedPaintOffset));

    // Now that we're sure the scrollbars are in the right place, paint them.
    if (m_hBar && !layerForHorizontalScrollbar())
        m_hBar->paint(context, damageRect);
    if (m_vBar && !layerForVerticalScrollbar())
        m_vBar->paint(context, damageRect);

    if (layerForScrollCorner())
        return;

    // We fill our scroll corner with white if we have a scrollbar that doesn't run all the way up to the
    // edge of the box.
    paintScrollCorner(context, adjustedPaintOffset, damageRect);
    
    // Paint our resizer last, since it sits on top of the scroll corner.
    paintResizer(context, adjustedPaintOffset, damageRect);
}

void RenderLayer::paintScrollCorner(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    RenderBox* box = renderBox();
    ASSERT(box);

    IntRect absRect = scrollCornerRect();
    absRect.moveBy(paintOffset);
    if (!absRect.intersects(damageRect))
        return;

    if (context->updatingControlTints()) {
        updateScrollCornerStyle();
        return;
    }

    if (m_scrollCorner) {
        m_scrollCorner->paintIntoRect(context, paintOffset, absRect);
        return;
    }

    // We don't want to paint white if we have overlay scrollbars, since we need
    // to see what is behind it.
    if (!hasOverlayScrollbars())
        context->fillRect(absRect, Color::white, box->style().colorSpace());
}

void RenderLayer::drawPlatformResizerImage(GraphicsContext* context, const LayoutRect& resizerCornerRect)
{
    RefPtr<Image> resizeCornerImage;
    FloatSize cornerResizerSize;
    if (renderer().document().deviceScaleFactor() >= 2) {
        DEPRECATED_DEFINE_STATIC_LOCAL(Image*, resizeCornerImageHiRes, (Image::loadPlatformResource("textAreaResizeCorner@2x").leakRef()));
        resizeCornerImage = resizeCornerImageHiRes;
        cornerResizerSize = resizeCornerImage->size();
        cornerResizerSize.scale(0.5f);
    } else {
        DEPRECATED_DEFINE_STATIC_LOCAL(Image*, resizeCornerImageLoRes, (Image::loadPlatformResource("textAreaResizeCorner").leakRef()));
        resizeCornerImage = resizeCornerImageLoRes;
        cornerResizerSize = resizeCornerImage->size();
    }

    if (renderer().style().shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        context->save();
        context->translate(resizerCornerRect.x() + cornerResizerSize.width(), resizerCornerRect.y() + resizerCornerRect.height() - cornerResizerSize.height());
        context->scale(FloatSize(-1.0, 1.0));
        context->drawImage(resizeCornerImage.get(), renderer().style().colorSpace(), FloatRect(FloatPoint(), cornerResizerSize));
        context->restore();
        return;
    }
    FloatRect imageRect = pixelSnappedForPainting(LayoutRect(resizerCornerRect.maxXMaxYCorner() - cornerResizerSize, cornerResizerSize), renderer().document().deviceScaleFactor());
    context->drawImage(resizeCornerImage.get(), renderer().style().colorSpace(), imageRect);
}

void RenderLayer::paintResizer(GraphicsContext* context, const LayoutPoint& paintOffset, const LayoutRect& damageRect)
{
    if (renderer().style().resize() == RESIZE_NONE)
        return;

    RenderBox* box = renderBox();
    ASSERT(box);

    LayoutRect absRect = resizerCornerRect(this, box->borderBoxRect());
    absRect.moveBy(paintOffset);
    if (!absRect.intersects(damageRect))
        return;

    if (context->updatingControlTints()) {
        updateResizerStyle();
        return;
    }
    
    if (m_resizer) {
        m_resizer->paintIntoRect(context, paintOffset, absRect);
        return;
    }

    drawPlatformResizerImage(context, absRect);

    // Draw a frame around the resizer (1px grey line) if there are any scrollbars present.
    // Clipping will exclude the right and bottom edges of this frame.
    if (!hasOverlayScrollbars() && (m_vBar || m_hBar)) {
        GraphicsContextStateSaver stateSaver(*context);
        context->clip(absRect);
        LayoutRect largerCorner = absRect;
        largerCorner.setSize(LayoutSize(largerCorner.width() + LayoutUnit::fromPixel(1), largerCorner.height() + LayoutUnit::fromPixel(1)));
        context->setStrokeColor(Color(makeRGB(217, 217, 217)), ColorSpaceDeviceRGB);
        context->setStrokeThickness(1.0f);
        context->setFillColor(Color::transparent, ColorSpaceDeviceRGB);
        context->drawRect(pixelSnappedIntRect(largerCorner));
    }
}

bool RenderLayer::isPointInResizeControl(const IntPoint& absolutePoint) const
{
    if (!canResize())
        return false;
    
    RenderBox* box = renderBox();
    ASSERT(box);

    IntPoint localPoint = roundedIntPoint(absoluteToContents(absolutePoint));

    IntRect localBounds(0, 0, box->pixelSnappedWidth(), box->pixelSnappedHeight());
    return resizerCornerRect(this, localBounds).contains(localPoint);
}

bool RenderLayer::hitTestOverflowControls(HitTestResult& result, const IntPoint& localPoint)
{
    if (!m_hBar && !m_vBar && !canResize())
        return false;

    RenderBox* box = renderBox();
    ASSERT(box);
    
    IntRect resizeControlRect;
    if (renderer().style().resize() != RESIZE_NONE) {
        resizeControlRect = pixelSnappedIntRect(resizerCornerRect(this, box->borderBoxRect()));
        if (resizeControlRect.contains(localPoint))
            return true;
    }

    int resizeControlSize = std::max(resizeControlRect.height(), 0);

    // FIXME: We should hit test the m_scrollCorner and pass it back through the result.

    if (m_vBar && m_vBar->shouldParticipateInHitTesting()) {
        LayoutRect vBarRect(verticalScrollbarStart(0, box->width()),
                            box->borderTop(),
                            m_vBar->width(),
                            box->height() - (box->borderTop() + box->borderBottom()) - (m_hBar ? m_hBar->height() : resizeControlSize));
        if (vBarRect.contains(localPoint)) {
            result.setScrollbar(m_vBar.get());
            return true;
        }
    }

    resizeControlSize = std::max(resizeControlRect.width(), 0);
    if (m_hBar && m_hBar->shouldParticipateInHitTesting()) {
        LayoutRect hBarRect(horizontalScrollbarStart(0),
                            box->height() - box->borderBottom() - m_hBar->height(),
                            box->width() - (box->borderLeft() + box->borderRight()) - (m_vBar ? m_vBar->width() : resizeControlSize),
                            m_hBar->height());
        if (hBarRect.contains(localPoint)) {
            result.setScrollbar(m_hBar.get());
            return true;
        }
    }

    return false;
}

bool RenderLayer::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    return ScrollableArea::scroll(direction, granularity, multiplier);
}

void RenderLayer::paint(GraphicsContext* context, const LayoutRect& damageRect, PaintBehavior paintBehavior, RenderObject* subtreePaintRoot, PaintLayerFlags paintFlags)
{
    OverlapTestRequestMap overlapTestRequests;

    LayerPaintingInfo paintingInfo(this, enclosingIntRect(damageRect), paintBehavior, LayoutSize(), subtreePaintRoot, &overlapTestRequests);
    paintLayer(context, paintingInfo, paintFlags);

    OverlapTestRequestMap::iterator end = overlapTestRequests.end();
    for (OverlapTestRequestMap::iterator it = overlapTestRequests.begin(); it != end; ++it)
        it->key->setOverlapTestResult(false);
}

void RenderLayer::paintOverlayScrollbars(GraphicsContext* context, const LayoutRect& damageRect, PaintBehavior paintBehavior, RenderObject* subtreePaintRoot)
{
    if (!m_containsDirtyOverlayScrollbars)
        return;

    LayerPaintingInfo paintingInfo(this, enclosingIntRect(damageRect), paintBehavior, LayoutSize(), subtreePaintRoot);
    paintLayer(context, paintingInfo, PaintLayerPaintingOverlayScrollbars);

    m_containsDirtyOverlayScrollbars = false;
}

static bool inContainingBlockChain(RenderLayer* startLayer, RenderLayer* endLayer)
{
    if (startLayer == endLayer)
        return true;
    
    RenderView* view = &startLayer->renderer().view();
    for (RenderBlock* currentBlock = startLayer->renderer().containingBlock(); currentBlock && currentBlock != view; currentBlock = currentBlock->containingBlock()) {
        if (currentBlock->layer() == endLayer)
            return true;
    }
    
    return false;
}

void RenderLayer::clipToRect(const LayerPaintingInfo& paintingInfo, GraphicsContext* context, const ClipRect& clipRect, BorderRadiusClippingRule rule)
{
    float deviceScaleFactor = renderer().document().deviceScaleFactor();
    if (clipRect.rect() != paintingInfo.paintDirtyRect || clipRect.hasRadius()) {
        context->save();
        LayoutRect adjustedClipRect = clipRect.rect();
        adjustedClipRect.move(paintingInfo.subpixelAccumulation);
        context->clip(pixelSnappedForPainting(adjustedClipRect, deviceScaleFactor));
    }

    if (!clipRect.hasRadius())
        return;

    // If the clip rect has been tainted by a border radius, then we have to walk up our layer chain applying the clips from
    // any layers with overflow. The condition for being able to apply these clips is that the overflow object be in our
    // containing block chain so we check that also.
    for (RenderLayer* layer = rule == IncludeSelfForBorderRadius ? this : parent(); layer; layer = layer->parent()) {
        if (layer->renderer().hasOverflowClip() && layer->renderer().style().hasBorderRadius() && inContainingBlockChain(this, layer)) {
            LayoutRect adjustedClipRect = LayoutRect(toLayoutPoint(layer->offsetFromAncestor(paintingInfo.rootLayer)), layer->size());
            adjustedClipRect.move(paintingInfo.subpixelAccumulation);
            context->clipRoundedRect(layer->renderer().style().getRoundedInnerBorderFor(adjustedClipRect).pixelSnappedRoundedRectForPainting(deviceScaleFactor));
        }

        if (layer == paintingInfo.rootLayer)
            break;
    }
}

void RenderLayer::restoreClip(GraphicsContext* context, const LayoutRect& paintDirtyRect, const ClipRect& clipRect)
{
    if (clipRect.rect() == paintDirtyRect && !clipRect.hasRadius())
        return;
    context->restore();
}

static void performOverlapTests(OverlapTestRequestMap& overlapTestRequests, const RenderLayer* rootLayer, const RenderLayer* layer)
{
    Vector<OverlapTestRequestClient*> overlappedRequestClients;
    OverlapTestRequestMap::iterator end = overlapTestRequests.end();
    LayoutRect boundingBox = layer->boundingBox(rootLayer, layer->offsetFromAncestor(rootLayer));
    for (OverlapTestRequestMap::iterator it = overlapTestRequests.begin(); it != end; ++it) {
        if (!boundingBox.intersects(it->value))
            continue;

        it->key->setOverlapTestResult(true);
        overlappedRequestClients.append(it->key);
    }
    for (size_t i = 0; i < overlappedRequestClients.size(); ++i)
        overlapTestRequests.remove(overlappedRequestClients[i]);
}

static inline bool shouldDoSoftwarePaint(const RenderLayer* layer, bool paintingReflection)
{
    return paintingReflection && !layer->has3DTransform();
}
    
static inline bool shouldSuppressPaintingLayer(RenderLayer* layer)
{
    // Avoid painting descendants of the root layer when stylesheets haven't loaded. This eliminates FOUC.
    // It's ok not to draw, because later on, when all the stylesheets do load, updateStyleSelector on the Document
    // will do a full repaint().
    if (layer->renderer().document().didLayoutWithPendingStylesheets() && !layer->isRootLayer() && !layer->renderer().isRoot())
        return true;

    // Avoid painting all layers if the document is in a state where visual updates aren't allowed.
    // A full repaint will occur in Document::implicitClose() if painting is suppressed here.
    if (!layer->renderer().document().visualUpdatesAllowed())
        return true;

    return false;
}

static inline bool paintForFixedRootBackground(const RenderLayer* layer, RenderLayer::PaintLayerFlags paintFlags)
{
    return layer->renderer().isRoot() && (paintFlags & RenderLayer::PaintLayerPaintingRootBackgroundOnly);
}

void RenderLayer::paintLayer(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    if (isComposited()) {
        // The updatingControlTints() painting pass goes through compositing layers,
        // but we need to ensure that we don't cache clip rects computed with the wrong root in this case.
        if (context->updatingControlTints() || (paintingInfo.paintBehavior & PaintBehaviorFlattenCompositingLayers))
            paintFlags |= PaintLayerTemporaryClipRects;
        else if (!backing()->paintsIntoWindow()
            && !backing()->paintsIntoCompositedAncestor()
            && !shouldDoSoftwarePaint(this, paintFlags & PaintLayerPaintingReflection)
            && !paintForFixedRootBackground(this, paintFlags)) {
            // If this RenderLayer should paint into its backing, that will be done via RenderLayerBacking::paintIntoLayer().
            return;
        }
    } else if (viewportConstrainedNotCompositedReason() == NotCompositedForBoundsOutOfView) {
        // Don't paint out-of-view viewport constrained layers (when doing prepainting) because they will never be visible
        // unless their position or viewport size is changed.
        ASSERT(renderer().style().position() == FixedPosition);
        return;
    }

    // Non self-painting leaf layers don't need to be painted as their renderer() should properly paint itself.
    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return;

    if (shouldSuppressPaintingLayer(this))
        return;
    
    // If this layer is totally invisible then there is nothing to paint.
    if (!renderer().opacity())
        return;

    // Don't paint the layer if the renderer doesn't belong to this region.
    // This is true as long as we clamp the range of a box to its containing block range.

    // Disable named flow region information for in flow threads such as multi-col.
    std::unique_ptr<CurrentRenderFlowThreadDisabler> flowThreadDisabler;
    if (enclosingPaginationLayer(ExcludeCompositedPaginatedLayers))
        flowThreadDisabler = std::make_unique<CurrentRenderFlowThreadDisabler>(&renderer().view());

    RenderNamedFlowFragment* namedFlowFragment = currentRenderNamedFlowFragment();
    if (namedFlowFragment) {
        ASSERT(namedFlowFragment->isValid());
        if (!namedFlowFragment->flowThread()->objectShouldFragmentInFlowRegion(&renderer(), namedFlowFragment))
            return;
    }

    if (paintsWithTransparency(paintingInfo.paintBehavior))
        paintFlags |= PaintLayerHaveTransparency;

    // PaintLayerAppliedTransform is used in RenderReplica, to avoid applying the transform twice.
    if (paintsWithTransform(paintingInfo.paintBehavior) && !(paintFlags & PaintLayerAppliedTransform)) {
        TransformationMatrix layerTransform = renderableTransform(paintingInfo.paintBehavior);
        // If the transform can't be inverted, then don't paint anything.
        if (!layerTransform.isInvertible())
            return;

        // If we have a transparency layer enclosing us and we are the root of a transform, then we need to establish the transparency
        // layer from the parent now, assuming there is a parent
        if (paintFlags & PaintLayerHaveTransparency) {
            if (parent())
                parent()->beginTransparencyLayers(context, paintingInfo, paintingInfo.paintDirtyRect);
            else
                beginTransparencyLayers(context, paintingInfo, paintingInfo.paintDirtyRect);
        }

        if (enclosingPaginationLayer(ExcludeCompositedPaginatedLayers)) {
            paintTransformedLayerIntoFragments(context, paintingInfo, paintFlags);
            return;
        }

        // Make sure the parent's clip rects have been calculated.
        ClipRect clipRect = paintingInfo.paintDirtyRect;
        if (parent()) {
            ClipRectsContext clipRectsContext(paintingInfo.rootLayer, (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects,
                IgnoreOverlayScrollbarSize, (paintFlags & PaintLayerPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip);
            clipRect = backgroundClipRect(clipRectsContext);
            clipRect.intersect(paintingInfo.paintDirtyRect);
        
            // Push the parent coordinate space's clip.
            parent()->clipToRect(paintingInfo, context, clipRect);
        }

        paintLayerByApplyingTransform(context, paintingInfo, paintFlags);

        // Restore the clip.
        if (parent())
            parent()->restoreClip(context, paintingInfo.paintDirtyRect, clipRect);

        return;
    }
    
    paintLayerContentsAndReflection(context, paintingInfo, paintFlags);
}

void RenderLayer::paintLayerContentsAndReflection(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    PaintLayerFlags localPaintFlags = paintFlags & ~(PaintLayerAppliedTransform);

    // Paint the reflection first if we have one.
    if (m_reflection && !m_paintingInsideReflection) {
        // Mark that we are now inside replica painting.
        m_paintingInsideReflection = true;
        reflectionLayer()->paintLayer(context, paintingInfo, localPaintFlags | PaintLayerPaintingReflection);
        m_paintingInsideReflection = false;
    }

    localPaintFlags |= PaintLayerPaintingCompositingAllPhases;
    paintLayerContents(context, paintingInfo, localPaintFlags);
}

bool RenderLayer::setupFontSubpixelQuantization(GraphicsContext* context, bool& didQuantizeFonts)
{
    if (context->paintingDisabled())
        return false;

    bool scrollingOnMainThread = true;
#if ENABLE(ASYNC_SCROLLING)
    if (Page* page = renderer().frame().page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            scrollingOnMainThread = scrollingCoordinator->shouldUpdateScrollLayerPositionSynchronously();
    }
#endif

    // FIXME: We shouldn't have to disable subpixel quantization for overflow clips or subframes once we scroll those
    // things on the scrolling thread.
    bool contentsScrollByPainting = (renderer().hasOverflowClip() && !usesCompositedScrolling()) || (renderer().frame().ownerElement());
    bool isZooming = false;
    if (Page* page = renderer().frame().page())
        isZooming = !page->chrome().client().hasStablePageScaleFactor();
    if (scrollingOnMainThread || contentsScrollByPainting || isZooming) {
        didQuantizeFonts = context->shouldSubpixelQuantizeFonts();
        context->setShouldSubpixelQuantizeFonts(false);
        return true;
    }
    return false;
}

template <class ReferenceBoxClipPathOperation>
static inline LayoutRect computeReferenceBox(const RenderObject& renderer, const ReferenceBoxClipPathOperation& clippingPath, const LayoutSize& offsetFromRoot, const LayoutRect& rootRelativeBounds)
{
    // FIXME: Support different reference boxes for inline content.
    // https://bugs.webkit.org/show_bug.cgi?id=129047
    if (!renderer.isBox())
        return rootRelativeBounds;

    LayoutRect referenceBox;
    const RenderBox& box = toRenderBox(renderer);
    switch (clippingPath.referenceBox()) {
    case ContentBox:
        referenceBox = box.contentBoxRect();
        referenceBox.move(offsetFromRoot);
        break;
    case PaddingBox:
        referenceBox = box.paddingBoxRect();
        referenceBox.move(offsetFromRoot);
        break;
    // FIXME: Support margin-box. Use bounding client rect for now.
    // https://bugs.webkit.org/show_bug.cgi?id=127984
    case MarginBox:
    // fill, stroke, view-box compute to border-box for HTML elements.
    case Fill:
    case Stroke:
    case ViewBox:
    case BorderBox:
    case BoxMissing:
        referenceBox = box.borderBoxRect();
        referenceBox.move(offsetFromRoot);
        break;
    }

    return referenceBox;
}

bool RenderLayer::setupClipPath(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, const LayoutSize& offsetFromRoot, LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed)
{
    if (!renderer().hasClipPath() || context->paintingDisabled())
        return false;

    if (!rootRelativeBoundsComputed) {
        rootRelativeBounds = calculateLayerBounds(paintingInfo.rootLayer, offsetFromRoot, 0);
        rootRelativeBoundsComputed = true;
    }

    RenderStyle& style = renderer().style();
    ASSERT(style.clipPath());
    if (style.clipPath()->type() == ClipPathOperation::Shape) {
        ShapeClipPathOperation& clippingPath = toShapeClipPathOperation(*(style.clipPath()));

        LayoutRect referenceBox = computeReferenceBox(renderer(), clippingPath, offsetFromRoot, rootRelativeBounds);
        context->save();
        context->clipPath(clippingPath.pathForReferenceRect(referenceBox), clippingPath.windRule());
        return true;
    }

    if (style.clipPath()->type() == ClipPathOperation::Box && renderer().isBox()) {
        BoxClipPathOperation& clippingPath = toBoxClipPathOperation(*(style.clipPath()));

        RoundedRect shapeRect = computeRoundedRectForBoxShape(clippingPath.referenceBox(), toRenderBox(renderer()));
        shapeRect.move(offsetFromRoot);

        context->save();
        context->clipPath(clippingPath.pathForReferenceRect(shapeRect), RULE_NONZERO);
        return true;
    }

    if (style.clipPath()->type() == ClipPathOperation::Reference) {
        ReferenceClipPathOperation* referenceClipPathOperation = static_cast<ReferenceClipPathOperation*>(style.clipPath());
        Element* element = renderer().document().getElementById(referenceClipPathOperation->fragment());
        if (element && element->hasTagName(SVGNames::clipPathTag) && element->renderer()) {
            // FIXME: This should use a safer cast such as toRenderSVGResourceContainer().
            // FIXME: Should this do a context->save() and return true so we restore the context?
            static_cast<RenderSVGResourceClipper*>(element->renderer())->applyClippingToContext(renderer(), rootRelativeBounds, paintingInfo.paintDirtyRect, context);
        }
    }

    return false;
}

#if ENABLE(CSS_FILTERS)

std::unique_ptr<FilterEffectRendererHelper> RenderLayer::setupFilters(GraphicsContext* context, LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags, const LayoutSize& offsetFromRoot, LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed)
{
    if (context->paintingDisabled())
        return nullptr;

    if (paintFlags & PaintLayerPaintingOverlayScrollbars)
        return nullptr;

    FilterInfo* filterInfo = FilterInfo::getIfExists(*this);
    bool hasPaintedFilter = filterInfo && filterInfo->renderer() && paintsWithFilters();
    if (!hasPaintedFilter)
        return nullptr;

    auto filterPainter = std::make_unique<FilterEffectRendererHelper>(hasPaintedFilter);
    if (!filterPainter->haveFilterEffect())
        return nullptr;
    
    LayoutRect filterRepaintRect = filterInfo->dirtySourceRect();
    filterRepaintRect.move(offsetFromRoot);

    if (!rootRelativeBoundsComputed) {
        rootRelativeBounds = calculateLayerBounds(paintingInfo.rootLayer, offsetFromRoot, 0);
        rootRelativeBoundsComputed = true;
    }

    if (filterPainter->prepareFilterEffect(this, enclosingIntRect(rootRelativeBounds), enclosingIntRect(paintingInfo.paintDirtyRect), enclosingIntRect(filterRepaintRect))) {
        // Now we know for sure, that the source image will be updated, so we can revert our tracking repaint rect back to zero.
        filterInfo->resetDirtySourceRect();

        if (!filterPainter->beginFilterEffect())
            return nullptr;

        // Check that we didn't fail to allocate the graphics context for the offscreen buffer.
        ASSERT(filterPainter->hasStartedFilterEffect());

        paintingInfo.paintDirtyRect = filterPainter->repaintRect();
        // If the filter needs the full source image, we need to avoid using the clip rectangles.
        // Otherwise, if for example this layer has overflow:hidden, a drop shadow will not compute correctly.
        // Note that we will still apply the clipping on the final rendering of the filter.
        paintingInfo.clipToDirtyRect = !filterInfo->renderer()->hasFilterThatMovesPixels();
        return filterPainter;
    }
    return nullptr;
}

GraphicsContext* RenderLayer::applyFilters(FilterEffectRendererHelper* filterPainter, GraphicsContext* originalContext, LayerPaintingInfo& paintingInfo, LayerFragments& layerFragments)
{
    ASSERT(filterPainter->hasStartedFilterEffect());
    // Apply the correct clipping (ie. overflow: hidden).
    // FIXME: It is incorrect to just clip to the damageRect here once multiple fragments are involved.
    ClipRect backgroundRect = layerFragments.isEmpty() ? ClipRect() : layerFragments[0].backgroundRect;
    clipToRect(paintingInfo, originalContext, backgroundRect);
    filterPainter->applyFilterEffect(originalContext);
    restoreClip(originalContext, paintingInfo.paintDirtyRect, backgroundRect);
    return originalContext;
}

#endif

// Helper for the sorting of layers by z-index.
static inline bool compareZIndex(RenderLayer* first, RenderLayer* second)
{
    return first->zIndex() < second->zIndex();
}

// Paint the layers associated with the fixed positioned elements in named flows.
// These layers should not be painted in a similar way as the other elements collected in
// named flows - regions -> named flows - since we do not want them to be clipped to the
// regions viewport.
void RenderLayer::paintFixedLayersInNamedFlows(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    if (!isRootLayer())
        return;

    // Get the named flows for the view
    if (!renderer().view().hasRenderNamedFlowThreads())
        return;

    // Ensure the flow threads hierarchy is up-to-date before using it.
    renderer().view().flowThreadController().updateNamedFlowsLayerListsIfNeeded();

    // Collect the fixed layers in a list to be painted
    Vector<RenderLayer*> fixedLayers;
    renderer().view().flowThreadController().collectFixedPositionedLayers(fixedLayers);

    // Paint the layers
    for (size_t i = 0; i < fixedLayers.size(); ++i) {
        RenderLayer* fixedLayer = fixedLayers.at(i);
        fixedLayer->paintLayer(context, paintingInfo, paintFlags);
    }
}

void RenderLayer::paintLayerContents(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    PaintLayerFlags localPaintFlags = paintFlags & ~(PaintLayerAppliedTransform);
    bool haveTransparency = localPaintFlags & PaintLayerHaveTransparency;
    bool isSelfPaintingLayer = this->isSelfPaintingLayer();
    bool isPaintingOverlayScrollbars = paintFlags & PaintLayerPaintingOverlayScrollbars;
    bool isPaintingScrollingContent = paintFlags & PaintLayerPaintingCompositingScrollingPhase;
    bool isPaintingCompositedForeground = paintFlags & PaintLayerPaintingCompositingForegroundPhase;
    bool isPaintingCompositedBackground = paintFlags & PaintLayerPaintingCompositingBackgroundPhase;
    bool isPaintingOverflowContents = paintFlags & PaintLayerPaintingOverflowContents;
    // Outline always needs to be painted even if we have no visible content. Also,
    // the outline is painted in the background phase during composited scrolling.
    // If it were painted in the foreground phase, it would move with the scrolled
    // content. When not composited scrolling, the outline is painted in the
    // foreground phase. Since scrolled contents are moved by repainting in this
    // case, the outline won't get 'dragged along'.
    bool shouldPaintOutline = isSelfPaintingLayer && !isPaintingOverlayScrollbars
        && ((isPaintingScrollingContent && isPaintingCompositedBackground)
        || (!isPaintingScrollingContent && isPaintingCompositedForeground));
    bool shouldPaintContent = m_hasVisibleContent && isSelfPaintingLayer && !isPaintingOverlayScrollbars;

    if (localPaintFlags & PaintLayerPaintingRootBackgroundOnly && !renderer().isRenderView() && !renderer().isRoot())
        return;

    // Ensure our lists are up-to-date.
    updateLayerListsIfNeeded();

    // Do not paint the fixed positioned layers if the paint root is the named flow,
    // if the paint originates at region level.
    if (paintingInfo.rootLayer->isOutOfFlowRenderFlowThread()
        && renderer().fixedPositionedWithNamedFlowContainingBlock())
        return;

    LayoutSize offsetFromRoot = offsetFromAncestor(paintingInfo.rootLayer);
    LayoutRect rootRelativeBounds;
    bool rootRelativeBoundsComputed = false;

    // FIXME: We shouldn't have to disable subpixel quantization for overflow clips or subframes once we scroll those
    // things on the scrolling thread.
    bool didQuantizeFonts = true;
    bool needToAdjustSubpixelQuantization = setupFontSubpixelQuantization(context, didQuantizeFonts);

    // Apply clip-path to context.
    bool hasClipPath = setupClipPath(context, paintingInfo, offsetFromRoot, rootRelativeBounds, rootRelativeBoundsComputed);

    LayerPaintingInfo localPaintingInfo(paintingInfo);

    GraphicsContext* transparencyLayerContext = context;
#if ENABLE(CSS_FILTERS)
    std::unique_ptr<FilterEffectRendererHelper> filterPainter = setupFilters(context, localPaintingInfo, paintFlags, offsetFromRoot, rootRelativeBounds, rootRelativeBoundsComputed);
    if (filterPainter) {
        context = filterPainter->filterContext();
        if (context != transparencyLayerContext && haveTransparency) {
            // If we have a filter and transparency, we have to eagerly start a transparency layer here, rather than risk a child layer lazily starts one with the wrong context.
            beginTransparencyLayers(transparencyLayerContext, localPaintingInfo, paintingInfo.paintDirtyRect);
        }
    }
#endif

    // If this layer's renderer is a child of the subtreePaintRoot, we render unconditionally, which
    // is done by passing a nil subtreePaintRoot down to our renderer (as if no subtreePaintRoot was ever set).
    // Otherwise, our renderer tree may or may not contain the subtreePaintRoot root, so we pass that root along
    // so it will be tested against as we descend through the renderers.
    RenderObject* subtreePaintRootForRenderer = 0;
    if (localPaintingInfo.subtreePaintRoot && !renderer().isDescendantOf(localPaintingInfo.subtreePaintRoot))
        subtreePaintRootForRenderer = localPaintingInfo.subtreePaintRoot;

    if (localPaintingInfo.overlapTestRequests && isSelfPaintingLayer)
        performOverlapTests(*localPaintingInfo.overlapTestRequests, localPaintingInfo.rootLayer, this);

    bool forceBlackText = localPaintingInfo.paintBehavior & PaintBehaviorForceBlackText;
    bool selectionOnly  = localPaintingInfo.paintBehavior & PaintBehaviorSelectionOnly;
    
    PaintBehavior paintBehavior = PaintBehaviorNormal;
    if (localPaintFlags & PaintLayerPaintingSkipRootBackground)
        paintBehavior |= PaintBehaviorSkipRootBackground;
    else if (localPaintFlags & PaintLayerPaintingRootBackgroundOnly)
        paintBehavior |= PaintBehaviorRootBackgroundOnly;

    LayerFragments layerFragments;
    LayoutRect paintDirtyRect = localPaintingInfo.paintDirtyRect;
    if (shouldPaintContent || shouldPaintOutline || isPaintingOverlayScrollbars) {
        // Collect the fragments. This will compute the clip rectangles and paint offsets for each layer fragment, as well as whether or not the content of each
        // fragment should paint. If the parent's filter dictates full repaint to ensure proper filter effect,
        // use the overflow clip as dirty rect, instead of no clipping. It maintains proper clipping for overflow::scroll.
        if (!paintingInfo.clipToDirtyRect && renderer().hasOverflowClip()) {
            // We can turn clipping back by requesting full repaint for the overflow area.
            localPaintingInfo.clipToDirtyRect = true;
            paintDirtyRect = selfClipRect();
        }
        collectFragments(layerFragments, localPaintingInfo.rootLayer, paintDirtyRect, ExcludeCompositedPaginatedLayers,
            (localPaintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, IgnoreOverlayScrollbarSize,
            (isPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip, offsetFromRoot);
        updatePaintingInfoForFragments(layerFragments, localPaintingInfo, localPaintFlags, shouldPaintContent, offsetFromRoot);
    }
    
    if (isPaintingCompositedBackground) {
        // Paint only the backgrounds for all of the fragments of the layer.
        if (shouldPaintContent && !selectionOnly)
            paintBackgroundForFragments(layerFragments, context, transparencyLayerContext, paintingInfo.paintDirtyRect, haveTransparency,
                localPaintingInfo, paintBehavior, subtreePaintRootForRenderer);
    }

    // Now walk the sorted list of children with negative z-indices.
    if ((isPaintingScrollingContent && isPaintingOverflowContents) || (!isPaintingScrollingContent && isPaintingCompositedBackground))
        paintList(negZOrderList(), context, localPaintingInfo, localPaintFlags);
    
    if (isPaintingCompositedForeground) {
        if (shouldPaintContent)
            paintForegroundForFragments(layerFragments, context, transparencyLayerContext, paintingInfo.paintDirtyRect, haveTransparency,
                localPaintingInfo, paintBehavior, subtreePaintRootForRenderer, selectionOnly, forceBlackText);
    }

    if (shouldPaintOutline)
        paintOutlineForFragments(layerFragments, context, localPaintingInfo, paintBehavior, subtreePaintRootForRenderer);

    if (isPaintingCompositedForeground) {
        // Paint any child layers that have overflow.
        paintList(m_normalFlowList.get(), context, localPaintingInfo, localPaintFlags);
    
        // Now walk the sorted list of children with positive z-indices.
        paintList(posZOrderList(), context, localPaintingInfo, localPaintFlags);

        // Paint the fixed elements from flow threads.
        paintFixedLayersInNamedFlows(context, localPaintingInfo, localPaintFlags);
        
        // If this is a region, paint its contents via the flow thread's layer.
        if (shouldPaintContent)
            paintFlowThreadIfRegionForFragments(layerFragments, context, localPaintingInfo, localPaintFlags);
    }

    if (isPaintingOverlayScrollbars)
        paintOverflowControlsForFragments(layerFragments, context, localPaintingInfo);

#if ENABLE(CSS_FILTERS)
    if (filterPainter) {
        context = applyFilters(filterPainter.get(), transparencyLayerContext, localPaintingInfo, layerFragments);
        filterPainter = nullptr;
    }
#endif
    
    // Make sure that we now use the original transparency context.
    ASSERT(transparencyLayerContext == context);

    if ((localPaintFlags & PaintLayerPaintingCompositingMaskPhase) && shouldPaintContent && renderer().hasMask() && !selectionOnly) {
        // Paint the mask for the fragments.
        paintMaskForFragments(layerFragments, context, localPaintingInfo, subtreePaintRootForRenderer);
    }

    // End our transparency layer
    if (haveTransparency && m_usedTransparency && !m_paintingInsideReflection) {
        context->endTransparencyLayer();
        context->restore();
        m_usedTransparency = false;
    }

    // Re-set this to whatever it was before we painted the layer.
    if (needToAdjustSubpixelQuantization)
        context->setShouldSubpixelQuantizeFonts(didQuantizeFonts);

    if (hasClipPath)
        context->restore();
}

void RenderLayer::paintLayerByApplyingTransform(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags, const LayoutSize& translationOffset)
{
    // This involves subtracting out the position of the layer in our current coordinate space, but preserving
    // the accumulated error for sub-pixel layout.
    float deviceScaleFactor = renderer().document().deviceScaleFactor();
    LayoutSize offsetFromParent = offsetFromAncestor(paintingInfo.rootLayer);
    offsetFromParent += translationOffset;
    TransformationMatrix transform(renderableTransform(paintingInfo.paintBehavior));
    // Add the subpixel accumulation to the current layer's offset so that we can always snap the translateRight value to where the renderer() is supposed to be painting.
    LayoutSize offsetForThisLayer = offsetFromParent + paintingInfo.subpixelAccumulation;
    FloatSize devicePixelSnappedOffsetForThisLayer = toFloatSize(roundedForPainting(toLayoutPoint(offsetForThisLayer), deviceScaleFactor));
    // We handle accumulated subpixels through nested layers here. Since the context gets translated to device pixels,
    // all we need to do is add the delta to the accumulated pixels coming from ancestor layers.
    // Translate the graphics context to the snapping position to avoid off-device-pixel positing.
    transform.translateRight(devicePixelSnappedOffsetForThisLayer.width(), devicePixelSnappedOffsetForThisLayer.height());
    // Apply the transform.
    GraphicsContextStateSaver stateSaver(*context);
    context->concatCTM(transform.toAffineTransform());

    // Now do a paint with the root layer shifted to be us.
    LayoutSize adjustedSubpixelAccumulation = offsetForThisLayer - LayoutSize(devicePixelSnappedOffsetForThisLayer);
    LayerPaintingInfo transformedPaintingInfo(this, LayoutRect(enclosingRectForPainting(transform.inverse().mapRect(paintingInfo.paintDirtyRect), deviceScaleFactor)),
        paintingInfo.paintBehavior, adjustedSubpixelAccumulation, paintingInfo.subtreePaintRoot, paintingInfo.overlapTestRequests);
    paintLayerContentsAndReflection(context, transformedPaintingInfo, paintFlags);
}

void RenderLayer::paintList(Vector<RenderLayer*>* list, GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    if (!list)
        return;

    if (!hasSelfPaintingLayerDescendant())
        return;

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(this);
#endif

    for (size_t i = 0; i < list->size(); ++i) {
        RenderLayer* childLayer = list->at(i);
        if (childLayer->isFlowThreadCollectingGraphicsLayersUnderRegions())
            continue;
        childLayer->paintLayer(context, paintingInfo, paintFlags);
    }
}

RenderLayer* RenderLayer::enclosingPaginationLayerInSubtree(const RenderLayer* rootLayer, PaginationInclusionMode mode) const
{
    // If we don't have an enclosing layer, or if the root layer is the same as the enclosing layer,
    // then just return the enclosing pagination layer (it will be 0 in the former case and the rootLayer in the latter case).
    RenderLayer* paginationLayer = enclosingPaginationLayer(mode);
    if (!paginationLayer || rootLayer == paginationLayer)
        return paginationLayer;
    
    // Walk up the layer tree and see which layer we hit first. If it's the root, then the enclosing pagination
    // layer isn't in our subtree and we return 0. If we hit the enclosing pagination layer first, then
    // we can return it.
    for (const RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (layer == rootLayer)
            return 0;
        if (layer == paginationLayer)
            return paginationLayer;
    }
    
    // This should never be reached, since an enclosing layer should always either be the rootLayer or be
    // our enclosing pagination layer.
    ASSERT_NOT_REACHED();
    return 0;
}

void RenderLayer::collectFragments(LayerFragments& fragments, const RenderLayer* rootLayer, const LayoutRect& dirtyRect, PaginationInclusionMode inclusionMode,
    ClipRectsType clipRectsType, OverlayScrollbarSizeRelevancy inOverlayScrollbarSizeRelevancy, ShouldRespectOverflowClip respectOverflowClip, const LayoutSize& offsetFromRoot,
    const LayoutRect* layerBoundingBox, ShouldApplyRootOffsetToFragments applyRootOffsetToFragments)
{
    RenderLayer* paginationLayer = enclosingPaginationLayerInSubtree(rootLayer, inclusionMode);
    if (!paginationLayer || hasTransform()) {
        // For unpaginated layers, there is only one fragment.
        LayerFragment fragment;
        ClipRectsContext clipRectsContext(rootLayer, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip);
        calculateRects(clipRectsContext, dirtyRect, fragment.layerBounds, fragment.backgroundRect, fragment.foregroundRect, fragment.outlineRect, offsetFromRoot);
        fragments.append(fragment);
        return;
    }
    
    // Compute our offset within the enclosing pagination layer.
    LayoutSize offsetWithinPaginatedLayer = offsetFromAncestor(paginationLayer);
    
    // Calculate clip rects relative to the enclosingPaginationLayer. The purpose of this call is to determine our bounds clipped to intermediate
    // layers between us and the pagination context. It's important to minimize the number of fragments we need to create and this helps with that.
    ClipRectsContext paginationClipRectsContext(paginationLayer, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip);
    LayoutRect layerBoundsInFlowThread;
    ClipRect backgroundRectInFlowThread;
    ClipRect foregroundRectInFlowThread;
    ClipRect outlineRectInFlowThread;
    calculateRects(paginationClipRectsContext, LayoutRect::infiniteRect(), layerBoundsInFlowThread, backgroundRectInFlowThread, foregroundRectInFlowThread,
        outlineRectInFlowThread, offsetWithinPaginatedLayer);
    
    // Take our bounding box within the flow thread and clip it.
    LayoutRect layerBoundingBoxInFlowThread = layerBoundingBox ? *layerBoundingBox : boundingBox(paginationLayer, offsetWithinPaginatedLayer);
    layerBoundingBoxInFlowThread.intersect(backgroundRectInFlowThread.rect());
    
    RenderFlowThread& enclosingFlowThread = toRenderFlowThread(paginationLayer->renderer());
    RenderLayer* parentPaginationLayer = paginationLayer->parent()->enclosingPaginationLayerInSubtree(rootLayer, inclusionMode);
    LayerFragments ancestorFragments;
    if (parentPaginationLayer) {
        // Compute a bounding box accounting for fragments.
        LayoutRect layerFragmentBoundingBoxInParentPaginationLayer = enclosingFlowThread.fragmentsBoundingBox(layerBoundingBoxInFlowThread);
        
        // Convert to be in the ancestor pagination context's coordinate space.
        LayoutSize offsetWithinParentPaginatedLayer = paginationLayer->offsetFromAncestor(parentPaginationLayer);
        layerFragmentBoundingBoxInParentPaginationLayer.move(offsetWithinParentPaginatedLayer);
        
        // Now collect ancestor fragments.
        parentPaginationLayer->collectFragments(ancestorFragments, rootLayer, dirtyRect, inclusionMode, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip,
            offsetFromAncestor(rootLayer), &layerFragmentBoundingBoxInParentPaginationLayer, ApplyRootOffsetToFragments);
        
        if (ancestorFragments.isEmpty())
            return;
        
        for (auto& ancestorFragment : ancestorFragments) {
            // Shift the dirty rect into flow thread coordinates.
            LayoutRect dirtyRectInFlowThread(dirtyRect);
            dirtyRectInFlowThread.move(-offsetWithinParentPaginatedLayer - ancestorFragment.paginationOffset);
            
            size_t oldSize = fragments.size();
            
            // Tell the flow thread to collect the fragments. We pass enough information to create a minimal number of fragments based off the pages/columns
            // that intersect the actual dirtyRect as well as the pages/columns that intersect our layer's bounding box.
            enclosingFlowThread.collectLayerFragments(fragments, layerBoundingBoxInFlowThread, dirtyRectInFlowThread);
            
            size_t newSize = fragments.size();
            
            if (oldSize == newSize)
                continue;

            for (size_t i = oldSize; i < newSize; ++i) {
                LayerFragment& fragment = fragments.at(i);
                
                // Set our four rects with all clipping applied that was internal to the flow thread.
                fragment.setRects(layerBoundsInFlowThread, backgroundRectInFlowThread, foregroundRectInFlowThread, outlineRectInFlowThread, &layerBoundingBoxInFlowThread);
                
                // Shift to the root-relative physical position used when painting the flow thread in this fragment.
                fragment.moveBy(toLayoutPoint(ancestorFragment.paginationOffset + fragment.paginationOffset + offsetWithinParentPaginatedLayer));

                // Intersect the fragment with our ancestor's background clip so that e.g., columns in an overflow:hidden block are
                // properly clipped by the overflow.
                fragment.intersect(ancestorFragment.paginationClip);
                
                // Now intersect with our pagination clip. This will typically mean we're just intersecting the dirty rect with the column
                // clip, so the column clip ends up being all we apply.
                fragment.intersect(fragment.paginationClip);
                
                if (applyRootOffsetToFragments == ApplyRootOffsetToFragments)
                    fragment.paginationOffset = fragment.paginationOffset + offsetWithinParentPaginatedLayer;
            }
        }
        
        return;
    }
    
    // Shift the dirty rect into flow thread coordinates.
    LayoutSize offsetOfPaginationLayerFromRoot = enclosingPaginationLayer(inclusionMode)->offsetFromAncestor(rootLayer);
    LayoutRect dirtyRectInFlowThread(dirtyRect);
    dirtyRectInFlowThread.move(-offsetOfPaginationLayerFromRoot);

    // Tell the flow thread to collect the fragments. We pass enough information to create a minimal number of fragments based off the pages/columns
    // that intersect the actual dirtyRect as well as the pages/columns that intersect our layer's bounding box.
    enclosingFlowThread.collectLayerFragments(fragments, layerBoundingBoxInFlowThread, dirtyRectInFlowThread);
    
    if (fragments.isEmpty())
        return;
    
    // Get the parent clip rects of the pagination layer, since we need to intersect with that when painting column contents.
    ClipRect ancestorClipRect = dirtyRect;
    if (paginationLayer->parent()) {
        ClipRectsContext clipRectsContext(rootLayer, clipRectsType, inOverlayScrollbarSizeRelevancy, respectOverflowClip);
        ancestorClipRect = paginationLayer->backgroundClipRect(clipRectsContext);
        ancestorClipRect.intersect(dirtyRect);
    }

    for (size_t i = 0; i < fragments.size(); ++i) {
        LayerFragment& fragment = fragments.at(i);
        
        // Set our four rects with all clipping applied that was internal to the flow thread.
        fragment.setRects(layerBoundsInFlowThread, backgroundRectInFlowThread, foregroundRectInFlowThread, outlineRectInFlowThread, &layerBoundingBoxInFlowThread);
        
        // Shift to the root-relative physical position used when painting the flow thread in this fragment.
        fragment.moveBy(toLayoutPoint(fragment.paginationOffset + offsetOfPaginationLayerFromRoot));

        // Intersect the fragment with our ancestor's background clip so that e.g., columns in an overflow:hidden block are
        // properly clipped by the overflow.
        fragment.intersect(ancestorClipRect.rect());
        
        // If the ancestor clip rect has border-radius, make sure to apply it to the fragments.
        if (ancestorClipRect.hasRadius()) {
            fragment.foregroundRect.setHasRadius(true);
            fragment.backgroundRect.setHasRadius(true);
            fragment.outlineRect.setHasRadius(true);
        }

        // Now intersect with our pagination clip. This will typically mean we're just intersecting the dirty rect with the column
        // clip, so the column clip ends up being all we apply.
        fragment.intersect(fragment.paginationClip);
        
        if (applyRootOffsetToFragments == ApplyRootOffsetToFragments)
            fragment.paginationOffset = fragment.paginationOffset + offsetOfPaginationLayerFromRoot;
    }
}

void RenderLayer::updatePaintingInfoForFragments(LayerFragments& fragments, const LayerPaintingInfo& localPaintingInfo, PaintLayerFlags localPaintFlags,
    bool shouldPaintContent, const LayoutSize& offsetFromRoot)
{
    for (size_t i = 0; i < fragments.size(); ++i) {
        LayerFragment& fragment = fragments.at(i);
        fragment.shouldPaintContent = shouldPaintContent;
        if (this != localPaintingInfo.rootLayer || !(localPaintFlags & PaintLayerPaintingOverflowContents)) {
            LayoutSize newOffsetFromRoot = offsetFromRoot + fragment.paginationOffset;
            fragment.shouldPaintContent &= intersectsDamageRect(fragment.layerBounds, fragment.backgroundRect.rect(), localPaintingInfo.rootLayer, newOffsetFromRoot, fragment.hasBoundingBox ? &fragment.boundingBox : 0);
        }
    }
}

void RenderLayer::paintTransformedLayerIntoFragments(GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    LayerFragments enclosingPaginationFragments;
    LayoutSize offsetOfPaginationLayerFromRoot;
    RenderLayer* paginatedLayer = enclosingPaginationLayer(ExcludeCompositedPaginatedLayers);
    LayoutRect transformedExtent = transparencyClipBox(*this, paginatedLayer, PaintingTransparencyClipBox, RootOfTransparencyClipBox, paintingInfo.paintBehavior);
    paginatedLayer->collectFragments(enclosingPaginationFragments, paintingInfo.rootLayer, paintingInfo.paintDirtyRect, ExcludeCompositedPaginatedLayers,
        (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, IgnoreOverlayScrollbarSize,
        (paintFlags & PaintLayerPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip, offsetOfPaginationLayerFromRoot, &transformedExtent);
    
    for (size_t i = 0; i < enclosingPaginationFragments.size(); ++i) {
        const LayerFragment& fragment = enclosingPaginationFragments.at(i);
        
        // Apply the page/column clip for this fragment, as well as any clips established by layers in between us and
        // the enclosing pagination layer.
        LayoutRect clipRect = fragment.backgroundRect.rect();
        
        // Now compute the clips within a given fragment
        if (parent() != paginatedLayer) {
            offsetOfPaginationLayerFromRoot = toLayoutSize(paginatedLayer->convertToLayerCoords(paintingInfo.rootLayer, toLayoutPoint(offsetOfPaginationLayerFromRoot)));
    
            ClipRectsContext clipRectsContext(paginatedLayer, (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects,
                IgnoreOverlayScrollbarSize, (paintFlags & PaintLayerPaintingOverflowContents) ? IgnoreOverflowClip : RespectOverflowClip);
            LayoutRect parentClipRect = backgroundClipRect(clipRectsContext).rect();
            parentClipRect.move(fragment.paginationOffset + offsetOfPaginationLayerFromRoot);
            clipRect.intersect(parentClipRect);
        }

        parent()->clipToRect(paintingInfo, context, clipRect);
        paintLayerByApplyingTransform(context, paintingInfo, paintFlags, fragment.paginationOffset);
        parent()->restoreClip(context, paintingInfo.paintDirtyRect, clipRect);
    }
}

void RenderLayer::paintBackgroundForFragments(const LayerFragments& layerFragments, GraphicsContext* context, GraphicsContext* transparencyLayerContext,
    const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo& localPaintingInfo, PaintBehavior paintBehavior,
    RenderObject* subtreePaintRootForRenderer)
{
    for (size_t i = 0; i < layerFragments.size(); ++i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (!fragment.shouldPaintContent)
            continue;

        // Begin transparency layers lazily now that we know we have to paint something.
        if (haveTransparency)
            beginTransparencyLayers(transparencyLayerContext, localPaintingInfo, transparencyPaintDirtyRect);
    
        if (localPaintingInfo.clipToDirtyRect) {
            // Paint our background first, before painting any child layers.
            // Establish the clip used to paint our background.
            clipToRect(localPaintingInfo, context, fragment.backgroundRect, DoNotIncludeSelfForBorderRadius); // Background painting will handle clipping to self.
        }
        
        // Paint the background.
        // FIXME: Eventually we will collect the region from the fragment itself instead of just from the paint info.
        PaintInfo paintInfo(context, fragment.backgroundRect.rect(), PaintPhaseBlockBackground, paintBehavior, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer());
        renderer().paint(paintInfo, toLayoutPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subpixelAccumulation));

        if (localPaintingInfo.clipToDirtyRect)
            restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.backgroundRect);
    }
}

void RenderLayer::paintForegroundForFragments(const LayerFragments& layerFragments, GraphicsContext* context, GraphicsContext* transparencyLayerContext,
    const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo& localPaintingInfo, PaintBehavior paintBehavior,
    RenderObject* subtreePaintRootForRenderer, bool selectionOnly, bool forceBlackText)
{
    // Begin transparency if we have something to paint.
    if (haveTransparency) {
        for (size_t i = 0; i < layerFragments.size(); ++i) {
            const LayerFragment& fragment = layerFragments.at(i);
            if (fragment.shouldPaintContent && !fragment.foregroundRect.isEmpty()) {
                beginTransparencyLayers(transparencyLayerContext, localPaintingInfo, transparencyPaintDirtyRect);
                break;
            }
        }
    }
    
    PaintBehavior localPaintBehavior = forceBlackText ? (PaintBehavior)PaintBehaviorForceBlackText : paintBehavior;

    // Optimize clipping for the single fragment case.
    bool shouldClip = localPaintingInfo.clipToDirtyRect && layerFragments.size() == 1 && layerFragments[0].shouldPaintContent && !layerFragments[0].foregroundRect.isEmpty();
    ClipRect clippedRect;
    if (shouldClip) {
        clippedRect = layerFragments[0].foregroundRect;
        clipToRect(localPaintingInfo, context, clippedRect);
    }
    
    // We have to loop through every fragment multiple times, since we have to repaint in each specific phase in order for
    // interleaving of the fragments to work properly.
    paintForegroundForFragmentsWithPhase(selectionOnly ? PaintPhaseSelection : PaintPhaseChildBlockBackgrounds, layerFragments,
        context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
    
    if (!selectionOnly) {
        paintForegroundForFragmentsWithPhase(PaintPhaseFloat, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
        paintForegroundForFragmentsWithPhase(PaintPhaseForeground, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);

        // Switch the clipping rectangle to the outline version.
        if (shouldClip && clippedRect != layerFragments[0].outlineRect) {
            restoreClip(context, localPaintingInfo.paintDirtyRect, clippedRect);
            
            if (!layerFragments[0].outlineRect.isEmpty()) {
                clippedRect = layerFragments[0].outlineRect;
                clipToRect(localPaintingInfo, context, clippedRect);
            } else
                shouldClip = false;
        }

        paintForegroundForFragmentsWithPhase(PaintPhaseChildOutlines, layerFragments, context, localPaintingInfo, localPaintBehavior, subtreePaintRootForRenderer);
    }
    
    if (shouldClip)
        restoreClip(context, localPaintingInfo.paintDirtyRect, clippedRect);
}

void RenderLayer::paintForegroundForFragmentsWithPhase(PaintPhase phase, const LayerFragments& layerFragments, GraphicsContext* context,
    const LayerPaintingInfo& localPaintingInfo, PaintBehavior paintBehavior, RenderObject* subtreePaintRootForRenderer)
{
    bool shouldClip = localPaintingInfo.clipToDirtyRect && layerFragments.size() > 1;

    for (size_t i = 0; i < layerFragments.size(); ++i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (!fragment.shouldPaintContent || fragment.foregroundRect.isEmpty())
            continue;
        
        if (shouldClip)
            clipToRect(localPaintingInfo, context, fragment.foregroundRect);
    
        PaintInfo paintInfo(context, fragment.foregroundRect.rect(), phase, paintBehavior, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer());
        if (phase == PaintPhaseForeground)
            paintInfo.overlapTestRequests = localPaintingInfo.overlapTestRequests;
        renderer().paint(paintInfo, toLayoutPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subpixelAccumulation));
        
        if (shouldClip)
            restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.foregroundRect);
    }
}

void RenderLayer::paintOutlineForFragments(const LayerFragments& layerFragments, GraphicsContext* context, const LayerPaintingInfo& localPaintingInfo,
    PaintBehavior paintBehavior, RenderObject* subtreePaintRootForRenderer)
{
    for (size_t i = 0; i < layerFragments.size(); ++i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (fragment.outlineRect.isEmpty())
            continue;
    
        // Paint our own outline
        PaintInfo paintInfo(context, fragment.outlineRect.rect(), PaintPhaseSelfOutline, paintBehavior, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer());
        clipToRect(localPaintingInfo, context, fragment.outlineRect, DoNotIncludeSelfForBorderRadius);
        renderer().paint(paintInfo, toLayoutPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subpixelAccumulation));
        restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.outlineRect);
    }
}

void RenderLayer::paintMaskForFragments(const LayerFragments& layerFragments, GraphicsContext* context, const LayerPaintingInfo& localPaintingInfo,
    RenderObject* subtreePaintRootForRenderer)
{
    for (size_t i = 0; i < layerFragments.size(); ++i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (!fragment.shouldPaintContent)
            continue;

        if (localPaintingInfo.clipToDirtyRect)
            clipToRect(localPaintingInfo, context, fragment.backgroundRect, DoNotIncludeSelfForBorderRadius); // Mask painting will handle clipping to self.
        
        // Paint the mask.
        // FIXME: Eventually we will collect the region from the fragment itself instead of just from the paint info.
        PaintInfo paintInfo(context, fragment.backgroundRect.rect(), PaintPhaseMask, PaintBehaviorNormal, subtreePaintRootForRenderer, nullptr, nullptr, &localPaintingInfo.rootLayer->renderer());
        renderer().paint(paintInfo, toLayoutPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subpixelAccumulation));
        
        if (localPaintingInfo.clipToDirtyRect)
            restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.backgroundRect);
    }
}

void RenderLayer::paintOverflowControlsForFragments(const LayerFragments& layerFragments, GraphicsContext* context, const LayerPaintingInfo& localPaintingInfo)
{
    for (size_t i = 0; i < layerFragments.size(); ++i) {
        const LayerFragment& fragment = layerFragments.at(i);
        clipToRect(localPaintingInfo, context, fragment.backgroundRect);
        paintOverflowControls(context, roundedIntPoint(toLayoutPoint(fragment.layerBounds.location() - renderBoxLocation() + localPaintingInfo.subpixelAccumulation)),
            pixelSnappedIntRect(fragment.backgroundRect.rect()), true);
        restoreClip(context, localPaintingInfo.paintDirtyRect, fragment.backgroundRect);
    }
}

bool RenderLayer::hitTest(const HitTestRequest& request, HitTestResult& result)
{
    return hitTest(request, result.hitTestLocation(), result);
}

bool RenderLayer::hitTest(const HitTestRequest& request, const HitTestLocation& hitTestLocation, HitTestResult& result)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    renderer().document().updateLayout();

    LayoutRect hitTestArea = isOutOfFlowRenderFlowThread() ? toRenderFlowThread(&renderer())->visualOverflowRect() : renderer().view().documentRect();
    if (!request.ignoreClipping())
        hitTestArea.intersect(renderer().view().frameView().visibleContentRect(LegacyIOSDocumentVisibleRect));

    RenderLayer* insideLayer = hitTestLayer(this, nullptr, request, result, hitTestArea, hitTestLocation, false);
    if (!insideLayer) {
        // We didn't hit any layer. If we are the root layer and the mouse is -- or just was -- down, 
        // return ourselves. We do this so mouse events continue getting delivered after a drag has 
        // exited the WebView, and so hit testing over a scrollbar hits the content document.
        if (!request.isChildFrameHitTest() && (request.active() || request.release()) && isRootLayer()) {
            renderer().updateHitTestResult(result, toRenderView(renderer()).flipForWritingMode(hitTestLocation.point()));
            insideLayer = this;
        }
    }

    // Now determine if the result is inside an anchor - if the urlElement isn't already set.
    Node* node = result.innerNode();
    if (node && !result.URLElement())
        result.setURLElement(node->enclosingLinkEventParentOrSelf());

    // Now return whether we were inside this layer (this will always be true for the root
    // layer).
    return insideLayer;
}

Element* RenderLayer::enclosingElement() const
{
    for (RenderElement* r = &renderer(); r; r = r->parent()) {
        if (Element* e = r->element())
            return e;
    }
    return 0;
}

RenderLayer* RenderLayer::enclosingFlowThreadAncestor() const
{
    RenderLayer* curr = parent();
    for (; curr && !curr->isRenderFlowThread(); curr = curr->parent()) {
        if (curr->isStackingContainer() && curr->isComposited()) {
            // We only adjust the position of the first level of layers.
            return 0;
        }
    }
    return curr;
}

bool RenderLayer::isFlowThreadCollectingGraphicsLayersUnderRegions() const
{
    return renderer().isRenderFlowThread() && toRenderFlowThread(renderer()).collectsGraphicsLayersUnderRegions();
}

// Compute the z-offset of the point in the transformState.
// This is effectively projecting a ray normal to the plane of ancestor, finding where that
// ray intersects target, and computing the z delta between those two points.
static double computeZOffset(const HitTestingTransformState& transformState)
{
    // We got an affine transform, so no z-offset
    if (transformState.m_accumulatedTransform.isAffine())
        return 0;

    // Flatten the point into the target plane
    FloatPoint targetPoint = transformState.mappedPoint();
    
    // Now map the point back through the transform, which computes Z.
    FloatPoint3D backmappedPoint = transformState.m_accumulatedTransform.mapPoint(FloatPoint3D(targetPoint));
    return backmappedPoint.z();
}

PassRefPtr<HitTestingTransformState> RenderLayer::createLocalTransformState(RenderLayer* rootLayer, RenderLayer* containerLayer,
                                        const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation,
                                        const HitTestingTransformState* containerTransformState,
                                        const LayoutSize& translationOffset) const
{
    RefPtr<HitTestingTransformState> transformState;
    LayoutSize offset;
    if (containerTransformState) {
        // If we're already computing transform state, then it's relative to the container (which we know is non-null).
        transformState = HitTestingTransformState::create(*containerTransformState);
        offset = offsetFromAncestor(containerLayer);
    } else {
        // If this is the first time we need to make transform state, then base it off of hitTestLocation,
        // which is relative to rootLayer.
        transformState = HitTestingTransformState::create(hitTestLocation.transformedPoint(), hitTestLocation.transformedRect(), FloatQuad(hitTestRect));
        offset = offsetFromAncestor(rootLayer);
    }
    offset += translationOffset;

    RenderObject* containerRenderer = containerLayer ? &containerLayer->renderer() : 0;
    if (renderer().shouldUseTransformFromContainer(containerRenderer)) {
        TransformationMatrix containerTransform;
        renderer().getTransformFromContainer(containerRenderer, offset, containerTransform);
        transformState->applyTransform(containerTransform, HitTestingTransformState::AccumulateTransform);
    } else {
        transformState->translate(offset.width(), offset.height(), HitTestingTransformState::AccumulateTransform);
    }
    
    return transformState;
}


static bool isHitCandidate(const RenderLayer* hitLayer, bool canDepthSort, double* zOffset, const HitTestingTransformState* transformState)
{
    if (!hitLayer)
        return false;
    
    // RenderNamedFlowFragments are not hit candidates. The hit test algorithm will pick the parent
    // layer, the one of the region.
    if (hitLayer->renderer().isRenderNamedFlowFragment())
        return false;

    // The hit layer is depth-sorting with other layers, so just say that it was hit.
    if (canDepthSort)
        return true;
    
    // We need to look at z-depth to decide if this layer was hit.
    if (zOffset) {
        ASSERT(transformState);
        // This is actually computing our z, but that's OK because the hitLayer is coplanar with us.
        double childZOffset = computeZOffset(*transformState);
        if (childZOffset > *zOffset) {
            *zOffset = childZOffset;
            return true;
        }
        return false;
    }

    return true;
}

RenderLayer* RenderLayer::hitTestFixedLayersInNamedFlows(RenderLayer* /*rootLayer*/,
    const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation,
    const HitTestingTransformState* transformState, 
    double* zOffsetForDescendants, double* zOffset,
    const HitTestingTransformState* unflattenedTransformState,
    bool depthSortDescendants)
{
    if (!isRootLayer())
        return 0;

    // Get the named flows for the view
    if (!renderer().view().hasRenderNamedFlowThreads())
        return 0;

    Vector<RenderLayer*> fixedLayers;
    renderer().view().flowThreadController().collectFixedPositionedLayers(fixedLayers);

    // Hit test the layers
    RenderLayer* resultLayer = 0;
    for (int i = fixedLayers.size() - 1; i >= 0; --i) {
        RenderLayer* fixedLayer = fixedLayers.at(i);

        HitTestResult tempResult(result.hitTestLocation());
        RenderLayer* hitLayer = fixedLayer->hitTestLayer(fixedLayer->renderer().flowThreadContainingBlock()->layer(), nullptr, request, tempResult,
            hitTestRect, hitTestLocation, false, transformState, zOffsetForDescendants);

        // If it a rect-based test, we can safely append the temporary result since it might had hit
        // nodes but not necesserily had hitLayer set.
        if (result.isRectBasedTest())
            result.append(tempResult);

        if (isHitCandidate(hitLayer, depthSortDescendants, zOffset, unflattenedTransformState)) {
            resultLayer = hitLayer;
            if (!result.isRectBasedTest())
                result = tempResult;
            if (!depthSortDescendants)
                break;
        }
    }

    return resultLayer;
}

// hitTestLocation and hitTestRect are relative to rootLayer.
// A 'flattening' layer is one preserves3D() == false.
// transformState.m_accumulatedTransform holds the transform from the containing flattening layer.
// transformState.m_lastPlanarPoint is the hitTestLocation in the plane of the containing flattening layer.
// transformState.m_lastPlanarQuad is the hitTestRect as a quad in the plane of the containing flattening layer.
// 
// If zOffset is non-null (which indicates that the caller wants z offset information), 
//  *zOffset on return is the z offset of the hit point relative to the containing flattening layer.
RenderLayer* RenderLayer::hitTestLayer(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
                                       const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, bool appliedTransform,
                                       const HitTestingTransformState* transformState, double* zOffset)
{
    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return 0;

    RenderNamedFlowFragment* namedFlowFragment = currentRenderNamedFlowFragment();

    // Prevent hitting the fixed layers inside the flow thread when hitting through regions.
    if (renderer().fixedPositionedWithNamedFlowContainingBlock() && namedFlowFragment)
        return 0;

    // Don't hit-test the layer if the renderer doesn't belong to this region.
    // This is true as long as we clamp the range of a box to its containing block range.
    // FIXME: Fix hit testing with in-flow threads included in out-of-flow threads.
    if (namedFlowFragment) {
        ASSERT(namedFlowFragment->isValid());
        RenderFlowThread* flowThread = namedFlowFragment->flowThread();
        if (!flowThread->objectShouldFragmentInFlowRegion(&renderer(), namedFlowFragment))
            return 0;
    }

    // The natural thing would be to keep HitTestingTransformState on the stack, but it's big, so we heap-allocate.

    // Apply a transform if we have one.
    if (transform() && !appliedTransform) {
        if (enclosingPaginationLayer(IncludeCompositedPaginatedLayers))
            return hitTestTransformedLayerInFragments(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffset);

        // Make sure the parent's clip rects have been calculated.
        if (parent()) {
            ClipRectsContext clipRectsContext(rootLayer, RootRelativeClipRects, IncludeOverlayScrollbarSize);
            ClipRect clipRect = backgroundClipRect(clipRectsContext);
            // Go ahead and test the enclosing clip now.
            if (!clipRect.intersects(hitTestLocation))
                return 0;
        }

        return hitTestLayerByApplyingTransform(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation, transformState, zOffset);
    }

    // Ensure our lists and 3d status are up-to-date.
    updateCompositingAndLayerListsIfNeeded();
    update3DTransformedDescendantStatus();

    RefPtr<HitTestingTransformState> localTransformState;
    if (appliedTransform) {
        // We computed the correct state in the caller (above code), so just reference it.
        ASSERT(transformState);
        localTransformState = const_cast<HitTestingTransformState*>(transformState);
    } else if (transformState || has3DTransformedDescendant() || preserves3D()) {
        // We need transform state for the first time, or to offset the container state, so create it here.
        localTransformState = createLocalTransformState(rootLayer, containerLayer, hitTestRect, hitTestLocation, transformState);
    }

    // Check for hit test on backface if backface-visibility is 'hidden'
    if (localTransformState && renderer().style().backfaceVisibility() == BackfaceVisibilityHidden) {
        TransformationMatrix invertedMatrix = localTransformState->m_accumulatedTransform.inverse();
        // If the z-vector of the matrix is negative, the back is facing towards the viewer.
        if (invertedMatrix.m33() < 0)
            return 0;
    }

    RefPtr<HitTestingTransformState> unflattenedTransformState = localTransformState;
    if (localTransformState && !preserves3D()) {
        // Keep a copy of the pre-flattening state, for computing z-offsets for the container
        unflattenedTransformState = HitTestingTransformState::create(*localTransformState);
        // This layer is flattening, so flatten the state passed to descendants.
        localTransformState->flatten();
    }

    // The following are used for keeping track of the z-depth of the hit point of 3d-transformed
    // descendants.
    double localZOffset = -std::numeric_limits<double>::infinity();
    double* zOffsetForDescendantsPtr = 0;
    double* zOffsetForContentsPtr = 0;
    
    bool depthSortDescendants = false;
    if (preserves3D()) {
        depthSortDescendants = true;
        // Our layers can depth-test with our container, so share the z depth pointer with the container, if it passed one down.
        zOffsetForDescendantsPtr = zOffset ? zOffset : &localZOffset;
        zOffsetForContentsPtr = zOffset ? zOffset : &localZOffset;
    } else if (zOffset) {
        zOffsetForDescendantsPtr = 0;
        // Container needs us to give back a z offset for the hit layer.
        zOffsetForContentsPtr = zOffset;
    }

    // This variable tracks which layer the mouse ends up being inside.
    RenderLayer* candidateLayer = 0;

    // Check the fixed positioned layers within flow threads that are positioned by the view.
    RenderLayer* hitLayer = hitTestFixedLayersInNamedFlows(rootLayer, request, result, hitTestRect, hitTestLocation,
        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Begin by walking our list of positive layers from highest z-index down to the lowest z-index.
    hitLayer = hitTestList(posZOrderList(), rootLayer, request, result, hitTestRect, hitTestLocation,
                                        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Now check our overflow objects.
    hitLayer = hitTestList(m_normalFlowList.get(), rootLayer, request, result, hitTestRect, hitTestLocation,
                           localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Collect the fragments. This will compute the clip rectangles for each layer fragment.
    LayerFragments layerFragments;
    collectFragments(layerFragments, rootLayer, hitTestRect, IncludeCompositedPaginatedLayers, RootRelativeClipRects, IncludeOverlayScrollbarSize, RespectOverflowClip,
        offsetFromAncestor(rootLayer));

    if (canResize() && hitTestResizerInFragments(layerFragments, hitTestLocation)) {
        renderer().updateHitTestResult(result, hitTestLocation.point());
        return this;
    }

    hitLayer = hitTestFlowThreadIfRegionForFragments(layerFragments, rootLayer, request, result, hitTestRect, hitTestLocation,
        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Next we want to see if the mouse pos is inside the child RenderObjects of the layer. Check
    // every fragment in reverse order.
    if (isSelfPaintingLayer()) {
        // Hit test with a temporary HitTestResult, because we only want to commit to 'result' if we know we're frontmost.
        HitTestResult tempResult(result.hitTestLocation());
        bool insideFragmentForegroundRect = false;
        if (hitTestContentsForFragments(layerFragments, request, tempResult, hitTestLocation, HitTestDescendants, insideFragmentForegroundRect)
            && isHitCandidate(this, false, zOffsetForContentsPtr, unflattenedTransformState.get())) {
            if (result.isRectBasedTest())
                result.append(tempResult);
            else
                result = tempResult;
            if (!depthSortDescendants)
                return this;
            // Foreground can depth-sort with descendant layers, so keep this as a candidate.
            candidateLayer = this;
        } else if (insideFragmentForegroundRect && result.isRectBasedTest())
            result.append(tempResult);
    }

    // Now check our negative z-index children.
    hitLayer = hitTestList(negZOrderList(), rootLayer, request, result, hitTestRect, hitTestLocation,
        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // If we found a layer, return. Child layers, and foreground always render in front of background.
    if (candidateLayer)
        return candidateLayer;

    if (isSelfPaintingLayer()) {
        HitTestResult tempResult(result.hitTestLocation());
        bool insideFragmentBackgroundRect = false;
        if (hitTestContentsForFragments(layerFragments, request, tempResult, hitTestLocation, HitTestSelf, insideFragmentBackgroundRect)
            && isHitCandidate(this, false, zOffsetForContentsPtr, unflattenedTransformState.get())) {
            if (result.isRectBasedTest())
                result.append(tempResult);
            else
                result = tempResult;
            return this;
        }
        if (insideFragmentBackgroundRect && result.isRectBasedTest())
            result.append(tempResult);
    }

    return 0;
}

bool RenderLayer::hitTestContentsForFragments(const LayerFragments& layerFragments, const HitTestRequest& request, HitTestResult& result,
    const HitTestLocation& hitTestLocation, HitTestFilter hitTestFilter, bool& insideClipRect) const
{
    if (layerFragments.isEmpty())
        return false;

    for (int i = layerFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if ((hitTestFilter == HitTestSelf && !fragment.backgroundRect.intersects(hitTestLocation))
            || (hitTestFilter == HitTestDescendants && !fragment.foregroundRect.intersects(hitTestLocation)))
            continue;
        insideClipRect = true;
        if (hitTestContents(request, result, fragment.layerBounds, hitTestLocation, hitTestFilter))
            return true;
    }
    
    return false;
}

bool RenderLayer::hitTestResizerInFragments(const LayerFragments& layerFragments, const HitTestLocation& hitTestLocation) const
{
    if (layerFragments.isEmpty())
        return false;

    for (int i = layerFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (fragment.backgroundRect.intersects(hitTestLocation) && resizerCornerRect(this, pixelSnappedIntRect(fragment.layerBounds)).contains(hitTestLocation.roundedPoint()))
            return true;
    }
    
    return false;
}

RenderLayer* RenderLayer::hitTestTransformedLayerInFragments(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset)
{
    LayerFragments enclosingPaginationFragments;
    LayoutSize offsetOfPaginationLayerFromRoot;
    RenderLayer* paginatedLayer = enclosingPaginationLayer(IncludeCompositedPaginatedLayers);
    LayoutRect transformedExtent = transparencyClipBox(*this, paginatedLayer, HitTestingTransparencyClipBox, RootOfTransparencyClipBox);
    paginatedLayer->collectFragments(enclosingPaginationFragments, rootLayer, hitTestRect, IncludeCompositedPaginatedLayers,
        RootRelativeClipRects, IncludeOverlayScrollbarSize, RespectOverflowClip, offsetOfPaginationLayerFromRoot, &transformedExtent);

    for (int i = enclosingPaginationFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = enclosingPaginationFragments.at(i);
        
        // Apply the page/column clip for this fragment, as well as any clips established by layers in between us and
        // the enclosing pagination layer.
        LayoutRect clipRect = fragment.backgroundRect.rect();
        
        // Now compute the clips within a given fragment
        if (parent() != paginatedLayer) {
            offsetOfPaginationLayerFromRoot = toLayoutSize(paginatedLayer->convertToLayerCoords(rootLayer, toLayoutPoint(offsetOfPaginationLayerFromRoot)));
    
            ClipRectsContext clipRectsContext(paginatedLayer, RootRelativeClipRects, IncludeOverlayScrollbarSize);
            LayoutRect parentClipRect = backgroundClipRect(clipRectsContext).rect();
            parentClipRect.move(fragment.paginationOffset + offsetOfPaginationLayerFromRoot);
            clipRect.intersect(parentClipRect);
        }
        
        if (!hitTestLocation.intersects(clipRect))
            continue;

        RenderLayer* hitLayer = hitTestLayerByApplyingTransform(rootLayer, containerLayer, request, result, hitTestRect, hitTestLocation,
            transformState, zOffset, fragment.paginationOffset);
        if (hitLayer)
            return hitLayer;
    }
    
    return 0;
}

RenderLayer* RenderLayer::hitTestLayerByApplyingTransform(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
    const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation, const HitTestingTransformState* transformState, double* zOffset,
    const LayoutSize& translationOffset)
{
    // Create a transform state to accumulate this transform.
    RefPtr<HitTestingTransformState> newTransformState = createLocalTransformState(rootLayer, containerLayer, hitTestRect, hitTestLocation, transformState, translationOffset);

    // If the transform can't be inverted, then don't hit test this layer at all.
    if (!newTransformState->m_accumulatedTransform.isInvertible())
        return 0;

    // Compute the point and the hit test rect in the coords of this layer by using the values
    // from the transformState, which store the point and quad in the coords of the last flattened
    // layer, and the accumulated transform which lets up map through preserve-3d layers.
    //
    // We can't just map hitTestLocation and hitTestRect because they may have been flattened (losing z)
    // by our container.
    FloatPoint localPoint = newTransformState->mappedPoint();
    FloatQuad localPointQuad = newTransformState->mappedQuad();
    LayoutRect localHitTestRect = newTransformState->boundsOfMappedArea();
    HitTestLocation newHitTestLocation;
    if (hitTestLocation.isRectBasedTest())
        newHitTestLocation = HitTestLocation(localPoint, localPointQuad);
    else
        newHitTestLocation = HitTestLocation(localPoint);

    // Now do a hit test with the root layer shifted to be us.
    return hitTestLayer(this, containerLayer, request, result, localHitTestRect, newHitTestLocation, true, newTransformState.get(), zOffset);
}

bool RenderLayer::hitTestContents(const HitTestRequest& request, HitTestResult& result, const LayoutRect& layerBounds, const HitTestLocation& hitTestLocation, HitTestFilter hitTestFilter) const
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    if (!renderer().hitTest(request, result, hitTestLocation, toLayoutPoint(layerBounds.location() - renderBoxLocation()), hitTestFilter)) {
        // It's wrong to set innerNode, but then claim that you didn't hit anything, unless it is
        // a rect-based test.
        ASSERT(!result.innerNode() || (result.isRectBasedTest() && result.rectBasedTestResult().size()));
        return false;
    }

    // For positioned generated content, we might still not have a
    // node by the time we get to the layer level, since none of
    // the content in the layer has an element. So just walk up
    // the tree.
    if (!result.innerNode() || !result.innerNonSharedNode()) {
        if (isOutOfFlowRenderFlowThread()) {
            // The flowthread doesn't have an enclosing element, so when hitting the layer of the
            // flowthread (e.g. the descent area of the RootInlineBox for the image flowed alone
            // inside the flow thread) we're letting the hit testing continue so it will hit the region.
            return false;
        }

        Element* e = enclosingElement();
        if (!result.innerNode())
            result.setInnerNode(e);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(e);
    }
        
    return true;
}

RenderLayer* RenderLayer::hitTestList(Vector<RenderLayer*>* list, RenderLayer* rootLayer,
                                      const HitTestRequest& request, HitTestResult& result,
                                      const LayoutRect& hitTestRect, const HitTestLocation& hitTestLocation,
                                      const HitTestingTransformState* transformState, 
                                      double* zOffsetForDescendants, double* zOffset,
                                      const HitTestingTransformState* unflattenedTransformState,
                                      bool depthSortDescendants)
{
    if (!list)
        return 0;

    if (!hasSelfPaintingLayerDescendant())
        return 0;

    RenderLayer* resultLayer = 0;
    for (int i = list->size() - 1; i >= 0; --i) {
        RenderLayer* childLayer = list->at(i);
        if (childLayer->isFlowThreadCollectingGraphicsLayersUnderRegions())
            continue;
        RenderLayer* hitLayer = 0;
        HitTestResult tempResult(result.hitTestLocation());
        hitLayer = childLayer->hitTestLayer(rootLayer, this, request, tempResult, hitTestRect, hitTestLocation, false, transformState, zOffsetForDescendants);

        // If it a rect-based test, we can safely append the temporary result since it might had hit
        // nodes but not necesserily had hitLayer set.
        if (result.isRectBasedTest())
            result.append(tempResult);

        if (isHitCandidate(hitLayer, depthSortDescendants, zOffset, unflattenedTransformState)) {
            resultLayer = hitLayer;
            if (!result.isRectBasedTest())
                result = tempResult;
            if (!depthSortDescendants)
                break;
        }
    }

    return resultLayer;
}

void RenderLayer::updateClipRects(const ClipRectsContext& clipRectsContext)
{
    ClipRectsType clipRectsType = clipRectsContext.clipRectsType;
    ASSERT(clipRectsType < NumCachedClipRectsTypes);
    if (m_clipRectsCache && m_clipRectsCache->getClipRects(clipRectsType, clipRectsContext.respectOverflowClip)) {
        ASSERT(clipRectsContext.rootLayer == m_clipRectsCache->m_clipRectsRoot[clipRectsType]);
        ASSERT(m_clipRectsCache->m_scrollbarRelevancy[clipRectsType] == clipRectsContext.overlayScrollbarSizeRelevancy);
        
#ifdef CHECK_CACHED_CLIP_RECTS
        // This code is useful to check cached clip rects, but is too expensive to leave enabled in debug builds by default.
        ClipRectsContext tempContext(clipRectsContext);
        tempContext.clipRectsType = TemporaryClipRects;
        ClipRects clipRects;
        calculateClipRects(tempContext, clipRects);
        ASSERT(clipRects == *m_clipRectsCache->getClipRects(clipRectsType, clipRectsContext.respectOverflowClip).get());
#endif
        return; // We have the correct cached value.
    }
    
    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent.  We want to cache clip rects with us as the root.
    RenderLayer* parentLayer = clipRectsContext.rootLayer != this ? parent() : 0;
    if (parentLayer)
        parentLayer->updateClipRects(clipRectsContext);

    ClipRects clipRects;
    calculateClipRects(clipRectsContext, clipRects);

    if (!m_clipRectsCache)
        m_clipRectsCache = std::make_unique<ClipRectsCache>();

    if (parentLayer && parentLayer->clipRects(clipRectsContext) && clipRects == *parentLayer->clipRects(clipRectsContext))
        m_clipRectsCache->setClipRects(clipRectsType, clipRectsContext.respectOverflowClip, parentLayer->clipRects(clipRectsContext));
    else
        m_clipRectsCache->setClipRects(clipRectsType, clipRectsContext.respectOverflowClip, ClipRects::create(clipRects));

#ifndef NDEBUG
    m_clipRectsCache->m_clipRectsRoot[clipRectsType] = clipRectsContext.rootLayer;
    m_clipRectsCache->m_scrollbarRelevancy[clipRectsType] = clipRectsContext.overlayScrollbarSizeRelevancy;
#endif
}

bool RenderLayer::mapLayerClipRectsToFragmentationLayer(ClipRects& clipRects) const
{
    RenderNamedFlowFragment* namedFlowFragment = currentRenderNamedFlowFragment();
    if (!namedFlowFragment)
        return false;

    ASSERT(namedFlowFragment->parent() && namedFlowFragment->parent()->isRenderNamedFlowFragmentContainer());
    
    CurrentRenderFlowThreadDisabler flowThreadDisabler(&renderer().view());
    ClipRectsContext targetClipRectsContext(&namedFlowFragment->fragmentContainerLayer(), TemporaryClipRects);
    namedFlowFragment->fragmentContainerLayer().calculateClipRects(targetClipRectsContext, clipRects);

    LayoutRect flowThreadPortionRect = namedFlowFragment->flowThreadPortionRect();

    LayoutPoint portionLocation = flowThreadPortionRect.location();
    LayoutRect regionContentBox = namedFlowFragment->fragmentContainer().contentBoxRect();
    LayoutSize moveOffset = portionLocation - regionContentBox.location() + namedFlowFragment->fragmentContainer().scrolledContentOffset();

    ClipRect newOverflowClipRect = clipRects.overflowClipRect();
    newOverflowClipRect.move(moveOffset);
    clipRects.setOverflowClipRect(newOverflowClipRect);

    ClipRect newFixedClipRect = clipRects.fixedClipRect();
    newFixedClipRect.move(moveOffset);
    clipRects.setFixedClipRect(newFixedClipRect);

    ClipRect newPosClipRect = clipRects.posClipRect();
    newPosClipRect.move(moveOffset);
    clipRects.setPosClipRect(newPosClipRect);

    return true;
}

void RenderLayer::calculateClipRects(const ClipRectsContext& clipRectsContext, ClipRects& clipRects) const
{
    if (!parent()) {
        // The root layer's clip rect is always infinite.
        clipRects.reset(LayoutRect::infiniteRect());
        return;
    }
    
    ClipRectsType clipRectsType = clipRectsContext.clipRectsType;
    bool useCached = clipRectsType != TemporaryClipRects;

    if (renderer().isRenderNamedFlowThread() && mapLayerClipRectsToFragmentationLayer(clipRects))
        return;

    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent. We want to cache clip rects with us as the root.
    RenderLayer* parentLayer = clipRectsContext.rootLayer != this ? parent() : 0;
    
    // Ensure that our parent's clip has been calculated so that we can examine the values.
    if (parentLayer) {
        if (useCached && parentLayer->clipRects(clipRectsContext))
            clipRects = *parentLayer->clipRects(clipRectsContext);
        else {
            ClipRectsContext parentContext(clipRectsContext);
            parentContext.overlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize; // FIXME: why?
            parentLayer->calculateClipRects(parentContext, clipRects);
        }
    } else
        clipRects.reset(LayoutRect::infiniteRect());

    // A fixed object is essentially the root of its containing block hierarchy, so when
    // we encounter such an object, we reset our clip rects to the fixedClipRect.
    if (renderer().style().position() == FixedPosition) {
        clipRects.setPosClipRect(clipRects.fixedClipRect());
        clipRects.setOverflowClipRect(clipRects.fixedClipRect());
        clipRects.setFixed(true);
    } else if (renderer().style().hasInFlowPosition())
        clipRects.setPosClipRect(clipRects.overflowClipRect());
    else if (renderer().style().position() == AbsolutePosition)
        clipRects.setOverflowClipRect(clipRects.posClipRect());
    
    // Update the clip rects that will be passed to child layers.
#if PLATFORM(IOS)
    if (renderer().hasClipOrOverflowClip() && (clipRectsContext.respectOverflowClip == RespectOverflowClip || this != clipRectsContext.rootLayer)) {
#else
    if ((renderer().hasOverflowClip() && (clipRectsContext.respectOverflowClip == RespectOverflowClip || this != clipRectsContext.rootLayer)) || renderer().hasClip()) {
#endif
        // This layer establishes a clip of some kind.

        // This offset cannot use convertToLayerCoords, because sometimes our rootLayer may be across
        // some transformed layer boundary, for example, in the RenderLayerCompositor overlapMap, where
        // clipRects are needed in view space.
        LayoutPoint offset;
        offset = roundedLayoutPoint(renderer().localToContainerPoint(FloatPoint(), &clipRectsContext.rootLayer->renderer()));
        if (clipRects.fixed() && &clipRectsContext.rootLayer->renderer() == &renderer().view())
            offset -= renderer().view().frameView().scrollOffsetForFixedPosition();
        
        if (renderer().hasOverflowClip()) {
            ClipRect newOverflowClip = toRenderBox(renderer()).overflowClipRectForChildLayers(offset, currentRenderNamedFlowFragment(), clipRectsContext.overlayScrollbarSizeRelevancy);
            newOverflowClip.setHasRadius(renderer().style().hasBorderRadius());
            clipRects.setOverflowClipRect(intersection(newOverflowClip, clipRects.overflowClipRect()));
            if (renderer().isPositioned())
                clipRects.setPosClipRect(intersection(newOverflowClip, clipRects.posClipRect()));
        }
        if (renderer().hasClip()) {
            LayoutRect newPosClip = toRenderBox(renderer()).clipRect(offset, currentRenderNamedFlowFragment());
            clipRects.setPosClipRect(intersection(newPosClip, clipRects.posClipRect()));
            clipRects.setOverflowClipRect(intersection(newPosClip, clipRects.overflowClipRect()));
            clipRects.setFixedClipRect(intersection(newPosClip, clipRects.fixedClipRect()));
        }
    }
}

void RenderLayer::parentClipRects(const ClipRectsContext& clipRectsContext, ClipRects& clipRects) const
{
    ASSERT(parent());
    if (renderer().isRenderNamedFlowThread() && mapLayerClipRectsToFragmentationLayer(clipRects))
        return;

    if (clipRectsContext.clipRectsType == TemporaryClipRects) {
        parent()->calculateClipRects(clipRectsContext, clipRects);
        return;
    }

    parent()->updateClipRects(clipRectsContext);
    clipRects = *parent()->clipRects(clipRectsContext);
}

static inline ClipRect backgroundClipRectForPosition(const ClipRects& parentRects, EPosition position)
{
    if (position == FixedPosition)
        return parentRects.fixedClipRect();

    if (position == AbsolutePosition)
        return parentRects.posClipRect();

    return parentRects.overflowClipRect();
}

ClipRect RenderLayer::backgroundClipRect(const ClipRectsContext& clipRectsContext) const
{
    ASSERT(parent());
    
    ClipRects parentRects;

    // If we cross into a different pagination context, then we can't rely on the cache.
    // Just switch over to using TemporaryClipRects.
    if (clipRectsContext.clipRectsType != TemporaryClipRects && parent()->enclosingPaginationLayer(IncludeCompositedPaginatedLayers) != enclosingPaginationLayer(IncludeCompositedPaginatedLayers)) {
        ClipRectsContext tempContext(clipRectsContext);
        tempContext.clipRectsType = TemporaryClipRects;
        parentClipRects(tempContext, parentRects);
    } else
        parentClipRects(clipRectsContext, parentRects);
    
    ClipRect backgroundClipRect = backgroundClipRectForPosition(parentRects, renderer().style().position());
    RenderView& view = renderer().view();

    // Note: infinite clipRects should not be scrolled here, otherwise they will accidentally no longer be considered infinite.
    if (parentRects.fixed() && &clipRectsContext.rootLayer->renderer() == &view && backgroundClipRect != LayoutRect::infiniteRect())
        backgroundClipRect.move(view.frameView().scrollOffsetForFixedPosition());

    return backgroundClipRect;
}

void RenderLayer::calculateRects(const ClipRectsContext& clipRectsContext, const LayoutRect& paintDirtyRect, LayoutRect& layerBounds,
    ClipRect& backgroundRect, ClipRect& foregroundRect, ClipRect& outlineRect, const LayoutSize& offsetFromRoot) const
{
    if (clipRectsContext.rootLayer != this && parent()) {
        backgroundRect = backgroundClipRect(clipRectsContext);
        backgroundRect.intersect(paintDirtyRect);
    } else
        backgroundRect = paintDirtyRect;

    LayoutSize offsetFromRootLocal = offsetFromRoot;
    RenderNamedFlowFragment* namedFlowFragment = currentRenderNamedFlowFragment();
    // If the view is scrolled, the flow thread is not scrolled with it and we should
    // take the scroll offset into account.
    if (clipRectsContext.rootLayer->isOutOfFlowRenderFlowThread() && !namedFlowFragment) {
        LayoutPoint absPos = LayoutPoint(renderer().view().localToAbsolute(FloatPoint(), IsFixed));
        offsetFromRootLocal += toLayoutSize(absPos);
    }

    layerBounds = LayoutRect(toLayoutPoint(offsetFromRootLocal), size());

    foregroundRect = backgroundRect;
    outlineRect = backgroundRect;

    RenderFlowThread* flowThread = namedFlowFragment ? namedFlowFragment->flowThread() : 0;
    if (isSelfPaintingLayer() && flowThread && !renderer().isInFlowRenderFlowThread()) {
        ASSERT(namedFlowFragment->isValid());
        const RenderBoxModelObject& boxModelObject = toRenderBoxModelObject(renderer());
        LayoutRect layerBoundsWithVisualOverflow = namedFlowFragment->visualOverflowRectForBox(&boxModelObject);

        // Layers are in physical coordinates so the origin must be moved to the physical top-left of the flowthread.
        if (&boxModelObject == flowThread && flowThread->style().isFlippedBlocksWritingMode()) {
            if (flowThread->style().isHorizontalWritingMode())
                layerBoundsWithVisualOverflow.moveBy(LayoutPoint(0, flowThread->height()));
            else
                layerBoundsWithVisualOverflow.moveBy(LayoutPoint(flowThread->width(), 0));
        } else {
            RenderBlock* rendererContainingBlock = boxModelObject.enclosingBox().isRenderBlock() ? toRenderBlock(&boxModelObject.enclosingBox()) : 0;
            if (rendererContainingBlock)
                rendererContainingBlock->flipForWritingMode(layerBoundsWithVisualOverflow);
        }

        layerBoundsWithVisualOverflow.move(offsetFromRootLocal);
        backgroundRect.intersect(layerBoundsWithVisualOverflow);

        foregroundRect = backgroundRect;
        outlineRect = backgroundRect;

        // If the region does not clip its overflow, inflate the outline rect.
        if (namedFlowFragment) {
            if (!(namedFlowFragment->parent()->hasOverflowClip() && (&namedFlowFragment->fragmentContainerLayer() != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip == RespectOverflowClip)))
                outlineRect.inflate(renderer().maximalOutlineSize(PaintPhaseOutline));
        }
    }

    // Update the clip rects that will be passed to child layers.
    if (renderer().hasClipOrOverflowClip()) {
        // This layer establishes a clip of some kind.
        if (renderer().hasOverflowClip() && (this != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip == RespectOverflowClip)) {
            foregroundRect.intersect(toRenderBox(renderer()).overflowClipRect(toLayoutPoint(offsetFromRootLocal), namedFlowFragment, clipRectsContext.overlayScrollbarSizeRelevancy));
            foregroundRect.setHasRadius(renderer().style().hasBorderRadius());
        }

        if (renderer().hasClip()) {
            // Clip applies to *us* as well, so go ahead and update the damageRect.
            LayoutRect newPosClip = toRenderBox(renderer()).clipRect(toLayoutPoint(offsetFromRootLocal), namedFlowFragment);
            backgroundRect.intersect(newPosClip);
            foregroundRect.intersect(newPosClip);
            outlineRect.intersect(newPosClip);
        }

        // If we establish a clip at all, then go ahead and make sure our background
        // rect is intersected with our layer's bounds including our visual overflow,
        // since any visual overflow like box-shadow or border-outset is not clipped by overflow:auto/hidden.
        if (renderBox()->hasVisualOverflow()) {
            // FIXME: Does not do the right thing with CSS regions yet, since we don't yet factor in the
            // individual region boxes as overflow.
            LayoutRect layerBoundsWithVisualOverflow = namedFlowFragment ? namedFlowFragment->visualOverflowRectForBox(renderBox()) : renderBox()->visualOverflowRect();
            renderBox()->flipForWritingMode(layerBoundsWithVisualOverflow); // Layers are in physical coordinates, so the overflow has to be flipped.
            layerBoundsWithVisualOverflow.move(offsetFromRootLocal);
            if (this != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip == RespectOverflowClip)
                backgroundRect.intersect(layerBoundsWithVisualOverflow);
        } else {
            // Shift the bounds to be for our region only.
            LayoutRect bounds = renderBox()->borderBoxRectInRegion(namedFlowFragment);
            if (namedFlowFragment)
                bounds = namedFlowFragment->rectFlowPortionForBox(renderBox(), bounds);

            bounds.move(offsetFromRootLocal);
            if (this != clipRectsContext.rootLayer || clipRectsContext.respectOverflowClip == RespectOverflowClip)
                backgroundRect.intersect(bounds);
            
            // Boxes inside flow threads don't have their logical left computed to avoid
            // floats. Instead, that information is kept in their RenderBoxRegionInfo structure.
            // As such, the layer bounds must be enlarged to encompass their background rect
            // to ensure intersecting them won't result in an empty rect, which would eventually
            // cause paint rejection.
            if (flowThread && flowThread->isRenderNamedFlowThread()) {
                if (flowThread->style().isHorizontalWritingMode())
                    layerBounds.shiftMaxXEdgeTo(std::max(layerBounds.maxX(), backgroundRect.rect().maxX()));
                else
                    layerBounds.shiftMaxYEdgeTo(std::max(layerBounds.maxY(), backgroundRect.rect().maxY()));
            }
        }
    }
}

LayoutRect RenderLayer::childrenClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    ClipRectsContext clipRectsContext(clippingRootLayer, TemporaryClipRects);
    // Need to use temporary clip rects, because the value of 'dontClipToOverflow' may be different from the painting path (<rdar://problem/11844909>).
    calculateRects(clipRectsContext, renderer().view().unscaledDocumentRect(), layerBounds, backgroundRect, foregroundRect, outlineRect, offsetFromAncestor(clipRectsContext.rootLayer));
    return clippingRootLayer->renderer().localToAbsoluteQuad(FloatQuad(foregroundRect.rect())).enclosingBoundingBox();
}

LayoutRect RenderLayer::selfClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    ClipRectsContext clipRectsContext(clippingRootLayer, PaintingClipRects);
    calculateRects(clipRectsContext, renderer().view().documentRect(), layerBounds, backgroundRect, foregroundRect, outlineRect, offsetFromAncestor(clippingRootLayer));
    return clippingRootLayer->renderer().localToAbsoluteQuad(FloatQuad(backgroundRect.rect())).enclosingBoundingBox();
}

LayoutRect RenderLayer::localClipRect(bool& clipExceedsBounds) const
{
    clipExceedsBounds = false;
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutSize offsetFromRoot = offsetFromAncestor(clippingRootLayer);

    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    ClipRectsContext clipRectsContext(clippingRootLayer, PaintingClipRects);
    calculateRects(clipRectsContext, LayoutRect::infiniteRect(), layerBounds, backgroundRect, foregroundRect, outlineRect, offsetFromRoot);

    LayoutRect clipRect = backgroundRect.rect();
    if (clipRect == LayoutRect::infiniteRect())
        return clipRect;

    if (renderer().hasClip()) {
        // CSS clip may be larger than our border box.
        LayoutRect cssClipRect = toRenderBox(renderer()).clipRect(toLayoutPoint(offsetFromRoot), currentRenderNamedFlowFragment());
        clipExceedsBounds = !clipRect.contains(cssClipRect);
    }

    clipRect.move(-offsetFromRoot);
    return clipRect;
}

void RenderLayer::addBlockSelectionGapsBounds(const LayoutRect& bounds)
{
    m_blockSelectionGapsBounds.unite(enclosingIntRect(bounds));
}

void RenderLayer::clearBlockSelectionGapsBounds()
{
    m_blockSelectionGapsBounds = IntRect();
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->clearBlockSelectionGapsBounds();
}

void RenderLayer::repaintBlockSelectionGaps()
{
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->repaintBlockSelectionGaps();

    if (m_blockSelectionGapsBounds.isEmpty())
        return;

    LayoutRect rect = m_blockSelectionGapsBounds;
    rect.move(-scrolledContentOffset());
    if (renderer().hasOverflowClip() && !usesCompositedScrolling())
        rect.intersect(toRenderBox(renderer()).overflowClipRect(LayoutPoint(), nullptr)); // FIXME: Regions not accounted for.
    if (renderer().hasClip())
        rect.intersect(toRenderBox(renderer()).clipRect(LayoutPoint(), nullptr)); // FIXME: Regions not accounted for.
    if (!rect.isEmpty())
        renderer().repaintRectangle(rect);
}

bool RenderLayer::intersectsDamageRect(const LayoutRect& layerBounds, const LayoutRect& damageRect, const RenderLayer* rootLayer, const LayoutSize& offsetFromRoot, const LayoutRect* cachedBoundingBox) const
{
    // Always examine the canvas and the root.
    // FIXME: Could eliminate the isRoot() check if we fix background painting so that the RenderView
    // paints the root's background.
    if (isRootLayer() || renderer().isRoot())
        return true;

    // If we aren't an inline flow, and our layer bounds do intersect the damage rect, then we 
    // can go ahead and return true.
    if (!renderer().isRenderInline()) {
        LayoutRect b = layerBounds;
        b.inflate(renderer().view().maximalOutlineSize());
        if (b.intersects(damageRect))
            return true;
    }

    RenderNamedFlowFragment* namedFlowFragment = currentRenderNamedFlowFragment();
    // When using regions, some boxes might have their frame rect relative to the flow thread, which could
    // cause the paint rejection algorithm to prevent them from painting when using different width regions.
    // e.g. an absolutely positioned box with bottom:0px and right:0px would have it's frameRect.x relative
    // to the flow thread, not the last region (in which it will end up because of bottom:0px)
    if (namedFlowFragment && renderer().flowThreadContainingBlock()) {
        LayoutRect b = layerBounds;
        b.moveBy(namedFlowFragment->visualOverflowRectForBox(toRenderBoxModelObject(&renderer())).location());
        b.inflate(renderer().view().maximalOutlineSize());
        if (b.intersects(damageRect))
            return true;
    }
    
    // Otherwise we need to compute the bounding box of this single layer and see if it intersects
    // the damage rect. It's possible the fragment computed the bounding box already, in which case we
    // can use the cached value.
    if (cachedBoundingBox)
        return cachedBoundingBox->intersects(damageRect);
    
    return boundingBox(rootLayer, offsetFromRoot).intersects(damageRect);
}

LayoutRect RenderLayer::localBoundingBox(CalculateLayerBoundsFlags flags) const
{
    // There are three special cases we need to consider.
    // (1) Inline Flows.  For inline flows we will create a bounding box that fully encompasses all of the lines occupied by the
    // inline.  In other words, if some <span> wraps to three lines, we'll create a bounding box that fully encloses the
    // line boxes of all three lines (including overflow on those lines).
    // (2) Left/Top Overflow.  The width/height of layers already includes right/bottom overflow.  However, in the case of left/top
    // overflow, we have to create a bounding box that will extend to include this overflow.
    // (3) Floats.  When a layer has overhanging floats that it paints, we need to make sure to include these overhanging floats
    // as part of our bounding box.  We do this because we are the responsible layer for both hit testing and painting those
    // floats.
    LayoutRect result;
    if (renderer().isInline() && renderer().isRenderInline())
        result = toRenderInline(renderer()).linesVisualOverflowBoundingBox();
    else if (renderer().isTableRow()) {
        RenderTableRow& tableRow = toRenderTableRow(renderer());
        // Our bounding box is just the union of all of our cells' border/overflow rects.
        for (RenderTableCell* cell = tableRow.firstCell(); cell; cell = cell->nextCell()) {
            LayoutRect bbox = cell->borderBoxRect();
            result.unite(bbox);
            LayoutRect overflowRect = tableRow.visualOverflowRect();
            if (bbox != overflowRect)
                result.unite(overflowRect);
        }
    } else {
        RenderBox* box = renderBox();
        ASSERT(box);
        if (!(flags & DontConstrainForMask) && box->hasMask()) {
            result = box->maskClipRect();
            box->flipForWritingMode(result); // The mask clip rect is in physical coordinates, so we have to flip, since localBoundingBox is not.
        } else {
            LayoutRect bbox = box->borderBoxRect();
            result = bbox;
            LayoutRect overflowRect = box->visualOverflowRect();
            if (bbox != overflowRect)
                result.unite(overflowRect);
        }
    }

    result.inflate(renderer().view().maximalOutlineSize()); // Used to apply a fudge factor to dirty-rect checks on blocks/tables.
    return result;
}

LayoutRect RenderLayer::boundingBox(const RenderLayer* ancestorLayer, const LayoutSize& offsetFromRoot, CalculateLayerBoundsFlags flags) const
{    
    LayoutRect result = localBoundingBox(flags);
    if (renderer().isBox())
        renderBox()->flipForWritingMode(result);
    else
        renderer().containingBlock()->flipForWritingMode(result);
    
    PaginationInclusionMode inclusionMode = ExcludeCompositedPaginatedLayers;
    if (flags & UseFragmentBoxesIncludingCompositing)
        inclusionMode = IncludeCompositedPaginatedLayers;
    const RenderLayer* paginationLayer = nullptr;
    if (flags & UseFragmentBoxesExcludingCompositing || flags & UseFragmentBoxesIncludingCompositing)
        paginationLayer = enclosingPaginationLayerInSubtree(ancestorLayer, inclusionMode);
    
    const RenderLayer* childLayer = this;
    bool isPaginated = paginationLayer;
    while (paginationLayer) {
        // Split our box up into the actual fragment boxes that render in the columns/pages and unite those together to
        // get our true bounding box.
        result.move(childLayer->offsetFromAncestor(paginationLayer));

        RenderFlowThread& enclosingFlowThread = toRenderFlowThread(paginationLayer->renderer());
        result = enclosingFlowThread.fragmentsBoundingBox(result);
        
        childLayer = paginationLayer;
        paginationLayer = paginationLayer->parent()->enclosingPaginationLayerInSubtree(ancestorLayer, inclusionMode);
    }

    if (isPaginated) {
        result.move(childLayer->offsetFromAncestor(ancestorLayer));
        return result;
    }
    
    result.move(offsetFromRoot);
    return result;
}

IntRect RenderLayer::absoluteBoundingBox() const
{
    const RenderLayer* rootLayer = root();
    return pixelSnappedIntRect(boundingBox(rootLayer, offsetFromAncestor(rootLayer)));
}

FloatRect RenderLayer::absoluteBoundingBoxForPainting() const
{
    const RenderLayer* rootLayer = root();
    return pixelSnappedForPainting(boundingBox(rootLayer, offsetFromAncestor(rootLayer)), renderer().document().deviceScaleFactor());
}

LayoutRect RenderLayer::calculateLayerBounds(const RenderLayer* ancestorLayer, const LayoutSize& offsetFromRoot, CalculateLayerBoundsFlags flags) const
{
    if (!isSelfPaintingLayer())
        return LayoutRect();

    // FIXME: This could be improved to do a check like hasVisibleNonCompositingDescendantLayers() (bug 92580).
    if ((flags & ExcludeHiddenDescendants) && this != ancestorLayer && !hasVisibleContent() && !hasVisibleDescendant())
        return LayoutRect();

    if (isRootLayer()) {
        // The root layer is always just the size of the document.
        return renderer().view().unscaledDocumentRect();
    }

    LayoutRect boundingBoxRect = localBoundingBox(flags);

    if (renderer().isBox())
        toRenderBox(renderer()).flipForWritingMode(boundingBoxRect);
    else
        renderer().containingBlock()->flipForWritingMode(boundingBoxRect);

    if (renderer().isRoot()) {
        // If the root layer becomes composited (e.g. because some descendant with negative z-index is composited),
        // then it has to be big enough to cover the viewport in order to display the background. This is akin
        // to the code in RenderBox::paintRootBoxFillLayers().
        const FrameView& frameView = renderer().view().frameView();
        boundingBoxRect.setWidth(std::max(boundingBoxRect.width(), frameView.contentsWidth() - boundingBoxRect.x()));
        boundingBoxRect.setHeight(std::max(boundingBoxRect.height(), frameView.contentsHeight() - boundingBoxRect.y()));
    }

    LayoutRect unionBounds = boundingBoxRect;

    if (flags & UseLocalClipRectIfPossible) {
        bool clipExceedsBounds = false;
        LayoutRect localClipRect = this->localClipRect(clipExceedsBounds);
        if (localClipRect != LayoutRect::infiniteRect() && !clipExceedsBounds) {
            if ((flags & IncludeSelfTransform) && paintsWithTransform(PaintBehaviorNormal))
                localClipRect = transform()->mapRect(localClipRect);

            localClipRect.move(offsetFromAncestor(ancestorLayer));
            return localClipRect;
        }
    }

    // FIXME: should probably just pass 'flags' down to descendants.
    CalculateLayerBoundsFlags descendantFlags = DefaultCalculateLayerBoundsFlags | (flags & ExcludeHiddenDescendants) | (flags & IncludeCompositedDescendants);

    const_cast<RenderLayer*>(this)->updateLayerListsIfNeeded();

    if (RenderLayer* reflection = reflectionLayer()) {
        if (!reflection->isComposited()) {
            LayoutRect childUnionBounds = reflection->calculateLayerBounds(this, reflection->offsetFromAncestor(this), descendantFlags);
            unionBounds.unite(childUnionBounds);
        }
    }
    
    ASSERT(isStackingContainer() || (!posZOrderList() || !posZOrderList()->size()));

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(const_cast<RenderLayer*>(this));
#endif

    if (Vector<RenderLayer*>* negZOrderList = this->negZOrderList()) {
        size_t listSize = negZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = negZOrderList->at(i);
            if (flags & IncludeCompositedDescendants || !curLayer->isComposited()) {
                LayoutRect childUnionBounds = curLayer->calculateLayerBounds(this, curLayer->offsetFromAncestor(this), descendantFlags);
                unionBounds.unite(childUnionBounds);
            }
        }
    }

    if (Vector<RenderLayer*>* posZOrderList = this->posZOrderList()) {
        size_t listSize = posZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = posZOrderList->at(i);
            // The RenderNamedFlowThread is ignored when we calculate the bounds of the RenderView.
            if ((flags & IncludeCompositedDescendants || !curLayer->isComposited()) && !curLayer->isFlowThreadCollectingGraphicsLayersUnderRegions()) {
                LayoutRect childUnionBounds = curLayer->calculateLayerBounds(this, curLayer->offsetFromAncestor(this), descendantFlags);
                unionBounds.unite(childUnionBounds);
            }
        }
    }

    if (Vector<RenderLayer*>* normalFlowList = this->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            // RenderView will always return the size of the document, before reaching this point,
            // so there's no way we could hit a RenderNamedFlowThread here.
            ASSERT(!curLayer->isFlowThreadCollectingGraphicsLayersUnderRegions());
            if (flags & IncludeCompositedDescendants || !curLayer->isComposited()) {
                LayoutRect curAbsBounds = curLayer->calculateLayerBounds(this, curLayer->offsetFromAncestor(this), descendantFlags);
                unionBounds.unite(curAbsBounds);
            }
        }
    }
    
#if ENABLE(CSS_FILTERS)
    // FIXME: We can optimize the size of the composited layers, by not enlarging
    // filtered areas with the outsets if we know that the filter is going to render in hardware.
    // https://bugs.webkit.org/show_bug.cgi?id=81239
    if (flags & IncludeLayerFilterOutsets)
        renderer().style().filterOutsets().expandRect(unionBounds);
#endif

    if ((flags & IncludeSelfTransform) && paintsWithTransform(PaintBehaviorNormal)) {
        TransformationMatrix* affineTrans = transform();
        boundingBoxRect = affineTrans->mapRect(boundingBoxRect);
        unionBounds = affineTrans->mapRect(unionBounds);
    }
    unionBounds.move(offsetFromRoot);
    return unionBounds;
}

void RenderLayer::clearClipRectsIncludingDescendants(ClipRectsType typeToClear)
{
    // FIXME: it's not clear how this layer not having clip rects guarantees that no descendants have any.
    if (!m_clipRectsCache)
        return;

    clearClipRects(typeToClear);
    
    for (RenderLayer* l = firstChild(); l; l = l->nextSibling())
        l->clearClipRectsIncludingDescendants(typeToClear);
}

void RenderLayer::clearClipRects(ClipRectsType typeToClear)
{
    if (typeToClear == AllClipRectTypes)
        m_clipRectsCache = nullptr;
    else {
        ASSERT(typeToClear < NumCachedClipRectsTypes);
        RefPtr<ClipRects> dummy;
        m_clipRectsCache->setClipRects(typeToClear, RespectOverflowClip, dummy);
        m_clipRectsCache->setClipRects(typeToClear, IgnoreOverflowClip, dummy);
    }
}

RenderLayerBacking* RenderLayer::ensureBacking()
{
    if (!m_backing) {
        m_backing = std::make_unique<RenderLayerBacking>(*this);
        compositor().layerBecameComposited(*this);

#if ENABLE(CSS_FILTERS)
        updateOrRemoveFilterEffectRenderer();
#endif
    }
    return m_backing.get();
}

void RenderLayer::clearBacking(bool layerBeingDestroyed)
{
    if (m_backing && !renderer().documentBeingDestroyed())
        compositor().layerBecameNonComposited(*this);
    m_backing = nullptr;

#if ENABLE(CSS_FILTERS)
    if (!layerBeingDestroyed)
        updateOrRemoveFilterEffectRenderer();
#else
    UNUSED_PARAM(layerBeingDestroyed);
#endif
}

bool RenderLayer::hasCompositedMask() const
{
    return m_backing && m_backing->hasMaskLayer();
}

GraphicsLayer* RenderLayer::layerForScrolling() const
{
    return m_backing ? m_backing->scrollingContentsLayer() : 0;
}

GraphicsLayer* RenderLayer::layerForHorizontalScrollbar() const
{
    return m_backing ? m_backing->layerForHorizontalScrollbar() : 0;
}

GraphicsLayer* RenderLayer::layerForVerticalScrollbar() const
{
    return m_backing ? m_backing->layerForVerticalScrollbar() : 0;
}

GraphicsLayer* RenderLayer::layerForScrollCorner() const
{
    return m_backing ? m_backing->layerForScrollCorner() : 0;
}

bool RenderLayer::paintsWithTransform(PaintBehavior paintBehavior) const
{
    bool paintsToWindow = !isComposited() || backing()->paintsIntoWindow();
    return transform() && ((paintBehavior & PaintBehaviorFlattenCompositingLayers) || paintsToWindow);
}

bool RenderLayer::backgroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect) const
{
    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return false;

    if (paintsWithTransparency(PaintBehaviorNormal))
        return false;

    // We can't use hasVisibleContent(), because that will be true if our renderer is hidden, but some child
    // is visible and that child doesn't cover the entire rect.
    if (renderer().style().visibility() != VISIBLE)
        return false;

#if ENABLE(CSS_FILTERS)
    if (paintsWithFilters() && renderer().style().filter().hasFilterThatAffectsOpacity())
        return false;
#endif

    // FIXME: Handle simple transforms.
    if (paintsWithTransform(PaintBehaviorNormal))
        return false;

    // FIXME: Remove this check.
    // This function should not be called when layer-lists are dirty.
    // It is somehow getting triggered during style update.
    if (m_zOrderListsDirty || m_normalFlowListDirty)
        return false;

    // FIXME: We currently only check the immediate renderer,
    // which will miss many cases.
    if (renderer().backgroundIsKnownToBeOpaqueInRect(localRect))
        return true;
    
    // We can't consult child layers if we clip, since they might cover
    // parts of the rect that are clipped out.
    if (renderer().hasOverflowClip())
        return false;
    
    return listBackgroundIsKnownToBeOpaqueInRect(posZOrderList(), localRect)
        || listBackgroundIsKnownToBeOpaqueInRect(negZOrderList(), localRect)
        || listBackgroundIsKnownToBeOpaqueInRect(normalFlowList(), localRect);
}

bool RenderLayer::listBackgroundIsKnownToBeOpaqueInRect(const Vector<RenderLayer*>* list, const LayoutRect& localRect) const
{
    if (!list || list->isEmpty())
        return false;

    for (Vector<RenderLayer*>::const_reverse_iterator iter = list->rbegin(); iter != list->rend(); ++iter) {
        const RenderLayer* childLayer = *iter;
        if (childLayer->isComposited())
            continue;

        if (!childLayer->canUseConvertToLayerCoords())
            continue;

        LayoutRect childLocalRect(localRect);
        childLocalRect.move(-childLayer->offsetFromAncestor(this));

        if (childLayer->backgroundIsKnownToBeOpaqueInRect(childLocalRect))
            return true;
    }
    return false;
}

void RenderLayer::setParent(RenderLayer* parent)
{
    if (parent == m_parent)
        return;

    if (m_parent && !renderer().documentBeingDestroyed())
        compositor().layerWillBeRemoved(*m_parent, *this);
    
    m_parent = parent;

    if (m_parent && !renderer().documentBeingDestroyed())
        compositor().layerWasAdded(*m_parent, *this);
}

void RenderLayer::dirtyZOrderLists()
{
    ASSERT(m_layerListMutationAllowed);
    ASSERT(isStackingContainer());

    if (m_posZOrderList)
        m_posZOrderList->clear();
    if (m_negZOrderList)
        m_negZOrderList->clear();
    m_zOrderListsDirty = true;

    if (!renderer().documentBeingDestroyed()) {
        if (isFlowThreadCollectingGraphicsLayersUnderRegions())
            toRenderFlowThread(renderer()).setNeedsLayerToRegionMappingsUpdate();
        compositor().setCompositingLayersNeedRebuild();
        if (acceleratedCompositingForOverflowScrollEnabled())
            compositor().setShouldReevaluateCompositingAfterLayout();
    }
}

void RenderLayer::dirtyStackingContainerZOrderLists()
{
    RenderLayer* sc = stackingContainer();
    if (sc)
        sc->dirtyZOrderLists();
}

void RenderLayer::dirtyNormalFlowList()
{
    ASSERT(m_layerListMutationAllowed);

    if (m_normalFlowList)
        m_normalFlowList->clear();
    m_normalFlowListDirty = true;

    if (!renderer().documentBeingDestroyed()) {
        if (isFlowThreadCollectingGraphicsLayersUnderRegions())
            toRenderFlowThread(renderer()).setNeedsLayerToRegionMappingsUpdate();
        compositor().setCompositingLayersNeedRebuild();
        if (acceleratedCompositingForOverflowScrollEnabled())
            compositor().setShouldReevaluateCompositingAfterLayout();
    }
}

void RenderLayer::rebuildZOrderLists()
{
    ASSERT(m_layerListMutationAllowed);
    ASSERT(isDirtyStackingContainer());
    rebuildZOrderLists(StopAtStackingContainers, m_posZOrderList, m_negZOrderList);
    m_zOrderListsDirty = false;
}

void RenderLayer::rebuildZOrderLists(CollectLayersBehavior behavior, std::unique_ptr<Vector<RenderLayer*>>& posZOrderList, std::unique_ptr<Vector<RenderLayer*>>& negZOrderList)
{
    bool includeHiddenLayers = compositor().inCompositingMode();
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        if (!m_reflection || reflectionLayer() != child)
            child->collectLayers(includeHiddenLayers, behavior, posZOrderList, negZOrderList);

    // Sort the two lists.
    if (posZOrderList)
        std::stable_sort(posZOrderList->begin(), posZOrderList->end(), compareZIndex);

    if (negZOrderList)
        std::stable_sort(negZOrderList->begin(), negZOrderList->end(), compareZIndex);
}

void RenderLayer::updateNormalFlowList()
{
    if (!m_normalFlowListDirty)
        return;

    ASSERT(m_layerListMutationAllowed);

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        // Ignore non-overflow layers and reflections.
        if (child->isNormalFlowOnly() && (!m_reflection || reflectionLayer() != child)) {
            if (!m_normalFlowList)
                m_normalFlowList = std::make_unique<Vector<RenderLayer*>>();
            m_normalFlowList->append(child);
        }
    }
    
    m_normalFlowListDirty = false;
}

void RenderLayer::collectLayers(bool includeHiddenLayers, CollectLayersBehavior behavior, std::unique_ptr<Vector<RenderLayer*>>& posBuffer, std::unique_ptr<Vector<RenderLayer*>>& negBuffer)
{
    updateDescendantDependentFlags();

    bool isStacking = behavior == StopAtStackingContexts ? isStackingContext() : isStackingContainer();
    // Overflow layers are just painted by their enclosing layers, so they don't get put in zorder lists.
    bool includeHiddenLayer = includeHiddenLayers || (m_hasVisibleContent || (m_hasVisibleDescendant && isStacking));
    if (includeHiddenLayer && !isNormalFlowOnly()) {
        // Determine which buffer the child should be in.
        std::unique_ptr<Vector<RenderLayer*>>& buffer = (zIndex() >= 0) ? posBuffer : negBuffer;

        // Create the buffer if it doesn't exist yet.
        if (!buffer)
            buffer = std::make_unique<Vector<RenderLayer*>>();
        
        // Append ourselves at the end of the appropriate buffer.
        buffer->append(this);
    }

    // Recur into our children to collect more layers, but only if we don't establish
    // a stacking context/container.
    if ((includeHiddenLayers || m_hasVisibleDescendant) && !isStacking) {
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            // Ignore reflections.
            if (!m_reflection || reflectionLayer() != child)
                child->collectLayers(includeHiddenLayers, behavior, posBuffer, negBuffer);
        }
    }
}

void RenderLayer::updateLayerListsIfNeeded()
{
    bool shouldUpdateDescendantsAreContiguousInStackingOrder = isStackingContext() && (m_zOrderListsDirty || m_normalFlowListDirty);
    updateZOrderLists();
    updateNormalFlowList();

    if (RenderLayer* reflectionLayer = this->reflectionLayer()) {
        reflectionLayer->updateZOrderLists();
        reflectionLayer->updateNormalFlowList();
    }

    if (shouldUpdateDescendantsAreContiguousInStackingOrder) {
        updateDescendantsAreContiguousInStackingOrder();
        // The above function can cause us to update m_needsCompositedScrolling
        // and dirty our layer lists. Refresh them if necessary.
        updateZOrderLists();
        updateNormalFlowList();
    }
}

void RenderLayer::updateDescendantsLayerListsIfNeeded(bool recursive)
{
    Vector<RenderLayer*> layersToUpdate;
    
    if (isStackingContainer()) {
        if (Vector<RenderLayer*>* list = negZOrderList()) {
            size_t listSize = list->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* childLayer = list->at(i);
                layersToUpdate.append(childLayer);
            }
        }
    }
    
    if (Vector<RenderLayer*>* list = normalFlowList()) {
        size_t listSize = list->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* childLayer = list->at(i);
            layersToUpdate.append(childLayer);
        }
    }
    
    if (isStackingContainer()) {
        if (Vector<RenderLayer*>* list = posZOrderList()) {
            size_t listSize = list->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* childLayer = list->at(i);
                layersToUpdate.append(childLayer);
            }
        }
    }
    
    size_t listSize = layersToUpdate.size();
    for (size_t i = 0; i < listSize; ++i) {
        RenderLayer* childLayer = layersToUpdate.at(i);
        childLayer->updateLayerListsIfNeeded();
        
        if (recursive)
            childLayer->updateDescendantsLayerListsIfNeeded(true);
    }
}

void RenderLayer::updateCompositingAndLayerListsIfNeeded()
{
    if (compositor().inCompositingMode()) {
        if (isDirtyStackingContainer() || m_normalFlowListDirty)
            compositor().updateCompositingLayers(CompositingUpdateOnHitTest, this);
        return;
    }

    updateLayerListsIfNeeded();
}

void RenderLayer::repaintIncludingDescendants()
{
    renderer().repaint();
    for (RenderLayer* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->repaintIncludingDescendants();

    // If this is a region, we must also repaint the flow thread's layer since it is the one
    // doing the actual painting of the flowed content, but only if the region is valid.
    if (renderer().isRenderNamedFlowFragmentContainer()) {
        RenderNamedFlowFragment* region = toRenderBlockFlow(renderer()).renderNamedFlowFragment();
        if (region->isValid())
            region->flowThread()->layer()->repaintIncludingDescendants();
    }
}

void RenderLayer::setBackingNeedsRepaint(GraphicsLayer::ShouldClipToLayer shouldClip)
{
    ASSERT(isComposited());
    if (backing()->paintsIntoWindow()) {
        // If we're trying to repaint the placeholder document layer, propagate the
        // repaint to the native view system.
        renderer().view().repaintViewRectangle(absoluteBoundingBox());
    } else
        backing()->setContentsNeedDisplay(shouldClip);
}

void RenderLayer::setBackingNeedsRepaintInRect(const LayoutRect& r, GraphicsLayer::ShouldClipToLayer shouldClip)
{
    // https://bugs.webkit.org/show_bug.cgi?id=61159 describes an unreproducible crash here,
    // so assert but check that the layer is composited.
    ASSERT(isComposited());
    if (!isComposited() || backing()->paintsIntoWindow()) {
        // If we're trying to repaint the placeholder document layer, propagate the
        // repaint to the native view system.
        LayoutRect absRect(r);
        absRect.move(offsetFromAncestor(root()));

        renderer().view().repaintViewRectangle(absRect);
    } else
        backing()->setContentsNeedDisplayInRect(r, shouldClip);
}

// Since we're only painting non-composited layers, we know that they all share the same repaintContainer.
void RenderLayer::repaintIncludingNonCompositingDescendants(RenderLayerModelObject* repaintContainer)
{
    renderer().repaintUsingContainer(repaintContainer, renderer().clippedOverflowRectForRepaint(repaintContainer));

    for (RenderLayer* curr = firstChild(); curr; curr = curr->nextSibling()) {
        if (!curr->isComposited())
            curr->repaintIncludingNonCompositingDescendants(repaintContainer);
    }
}

bool RenderLayer::shouldBeNormalFlowOnly() const
{
    return (renderer().hasOverflowClip()
        || renderer().hasReflection()
        || renderer().hasMask()
        || renderer().isCanvas()
        || renderer().isVideo()
        || renderer().isEmbeddedObject()
        || renderer().isRenderIFrame()
        || (renderer().style().specifiesColumns() && !isRootLayer())
        || renderer().isInFlowRenderFlowThread())
        && !renderer().isPositioned()
        && !renderer().hasTransform()
        && !renderer().hasClipPath()
#if ENABLE(CSS_FILTERS)
        && !renderer().hasFilter()
#endif
#if PLATFORM(IOS)
        && !hasAcceleratedTouchScrolling()
#endif
#if ENABLE(CSS_COMPOSITING)
        && !renderer().hasBlendMode()
#endif
        && !isTransparent()
        && !needsCompositedScrolling()
        && !renderer().style().hasFlowFrom();
}

bool RenderLayer::shouldBeSelfPaintingLayer() const
{
    return !isNormalFlowOnly()
        || hasOverlayScrollbars()
        || needsCompositedScrolling()
        || isolatesBlending()
        || renderer().hasReflection()
        || renderer().hasMask()
        || renderer().isTableRow()
        || renderer().isCanvas()
        || renderer().isVideo()
        || renderer().isEmbeddedObject()
        || renderer().isRenderIFrame()
        || renderer().isInFlowRenderFlowThread();
}

void RenderLayer::updateSelfPaintingLayer()
{
    bool isSelfPaintingLayer = shouldBeSelfPaintingLayer();
    if (m_isSelfPaintingLayer == isSelfPaintingLayer)
        return;

    m_isSelfPaintingLayer = isSelfPaintingLayer;
    if (!parent())
        return;
    if (isSelfPaintingLayer)
        parent()->setAncestorChainHasSelfPaintingLayerDescendant();
    else
        parent()->dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();
}

// FIXME: use RenderObject::hasBoxDecorations(). And why hasBorderRadius() and filter?
static bool hasBoxDecorations(const RenderStyle& style)
{
    return style.hasBorder() || style.hasBorderRadius() || style.hasOutline() || style.hasAppearance() || style.boxShadow() || style.hasFilter();
}

static bool hasBoxDecorationsOrBackground(const RenderElement& renderer)
{
    return hasBoxDecorations(renderer.style()) || renderer.hasBackground();
}

// Constrain the depth and breadth of the search for performance.
static const int maxDescendentDepth = 3;
static const int maxSiblingCount = 20;

static bool hasPaintingNonLayerDescendants(const RenderElement& renderer, int depth)
{
    if (depth > maxDescendentDepth)
        return true;
    
    int siblingCount = 0;
    for (const auto& child : childrenOfType<RenderObject>(renderer)) {
        if (++siblingCount > maxSiblingCount)
            return true;
        
        if (child.isText()) {
            bool isSelectable = renderer.style().userSelect() != SELECT_NONE;
            if (isSelectable || !toRenderText(child).isAllCollapsibleWhitespace())
                return true;
        }
        
        if (!child.isRenderElement())
            continue;
        
        const RenderElement& renderElementChild = toRenderElement(child);

        if (renderElementChild.isRenderLayerModelObject() && toRenderLayerModelObject(renderElementChild).hasSelfPaintingLayer())
            continue;

        if (hasBoxDecorationsOrBackground(renderElementChild))
            return true;
        
        if (renderElementChild.isRenderReplaced())
            return true;

        if (hasPaintingNonLayerDescendants(renderElementChild, depth + 1))
            return true;
    }

    return false;
}

bool RenderLayer::hasNonEmptyChildRenderers() const
{
    return hasPaintingNonLayerDescendants(renderer(), 0);
}

bool RenderLayer::hasBoxDecorationsOrBackground() const
{
    return WebCore::hasBoxDecorationsOrBackground(renderer());
}

bool RenderLayer::hasVisibleBoxDecorations() const
{
    if (!hasVisibleContent())
        return false;

    return hasBoxDecorationsOrBackground() || hasOverflowControls();
}

bool RenderLayer::isVisuallyNonEmpty() const
{
    ASSERT(!m_visibleDescendantStatusDirty);

    if (!hasVisibleContent())
        return false;

    if (renderer().isRenderReplaced() || hasOverflowControls())
        return true;

    if (hasBoxDecorationsOrBackground())
        return true;
    
    if (hasNonEmptyChildRenderers())
        return true;

    return false;
}

void RenderLayer::updateStackingContextsAfterStyleChange(const RenderStyle* oldStyle)
{
    if (!oldStyle)
        return;

    bool wasStackingContext = isStackingContext(oldStyle);
    bool isStackingContext = this->isStackingContext();
    if (isStackingContext != wasStackingContext) {
        dirtyStackingContainerZOrderLists();
        if (isStackingContext)
            dirtyZOrderLists();
        else
            clearZOrderLists();

#if ENABLE(CSS_COMPOSITING)
        if (parent()) {
            if (isStackingContext) {
                if (!hasNotIsolatedBlendingDescendantsStatusDirty() && hasNotIsolatedBlendingDescendants())
                    parent()->dirtyAncestorChainHasBlendingDescendants();
            } else {
                if (hasNotIsolatedBlendingDescendantsStatusDirty())
                    parent()->dirtyAncestorChainHasBlendingDescendants();
                else if (hasNotIsolatedBlendingDescendants())
                    parent()->updateAncestorChainHasBlendingDescendants();
            }
        }
#endif

        return;
    }

    // FIXME: RenderLayer already handles visibility changes through our visiblity dirty bits. This logic could
    // likely be folded along with the rest.
    if (oldStyle->zIndex() != renderer().style().zIndex() || oldStyle->visibility() != renderer().style().visibility()) {
        dirtyStackingContainerZOrderLists();
        if (isStackingContext)
            dirtyZOrderLists();
    }
}

static bool overflowRequiresScrollbar(EOverflow overflow)
{
    return overflow == OSCROLL;
}

static bool overflowDefinesAutomaticScrollbar(EOverflow overflow)
{
    return overflow == OAUTO || overflow == OOVERLAY;
}

void RenderLayer::updateScrollbarsAfterStyleChange(const RenderStyle* oldStyle)
{
    // Overflow are a box concept.
    RenderBox* box = renderBox();
    if (!box)
        return;

    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (box->style().appearance() == ListboxPart)
        return;

    EOverflow overflowX = box->style().overflowX();
    EOverflow overflowY = box->style().overflowY();

    // To avoid doing a relayout in updateScrollbarsAfterLayout, we try to keep any automatic scrollbar that was already present.
    bool needsHorizontalScrollbar = (hasHorizontalScrollbar() && overflowDefinesAutomaticScrollbar(overflowX)) || overflowRequiresScrollbar(overflowX);
    bool needsVerticalScrollbar = (hasVerticalScrollbar() && overflowDefinesAutomaticScrollbar(overflowY)) || overflowRequiresScrollbar(overflowY);
    setHasHorizontalScrollbar(needsHorizontalScrollbar);
    setHasVerticalScrollbar(needsVerticalScrollbar);

    // With overflow: scroll, scrollbars are always visible but may be disabled.
    // When switching to another value, we need to re-enable them (see bug 11985).
    if (needsHorizontalScrollbar && oldStyle && oldStyle->overflowX() == OSCROLL && overflowX != OSCROLL) {
        ASSERT(hasHorizontalScrollbar());
        m_hBar->setEnabled(true);
    }

    if (needsVerticalScrollbar && oldStyle && oldStyle->overflowY() == OSCROLL && overflowY != OSCROLL) {
        ASSERT(hasVerticalScrollbar());
        m_vBar->setEnabled(true);
    }

    if (!m_scrollDimensionsDirty)
        updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());
}

void RenderLayer::setAncestorChainHasOutOfFlowPositionedDescendant(RenderBlock* containingBlock)
{
    for (RenderLayer* layer = this; layer; layer = layer->parent()) {
        if (!layer->m_hasOutOfFlowPositionedDescendantDirty && layer->hasOutOfFlowPositionedDescendant())
            break;

        layer->m_hasOutOfFlowPositionedDescendantDirty = false;
        layer->m_hasOutOfFlowPositionedDescendant = true;
        layer->updateNeedsCompositedScrolling();

        if (&layer->renderer() == containingBlock)
            break;
    }
}

void RenderLayer::dirtyAncestorChainHasOutOfFlowPositionedDescendantStatus()
{
    m_hasOutOfFlowPositionedDescendantDirty = true;
    if (parent())
        parent()->dirtyAncestorChainHasOutOfFlowPositionedDescendantStatus();
}

void RenderLayer::updateOutOfFlowPositioned(const RenderStyle* oldStyle)
{
    bool wasOutOfFlowPositioned = oldStyle && (oldStyle->position() == AbsolutePosition || oldStyle->position() == FixedPosition);
    if (parent() && (renderer().isOutOfFlowPositioned() != wasOutOfFlowPositioned)) {
        parent()->dirtyAncestorChainHasOutOfFlowPositionedDescendantStatus();
        if (!renderer().documentBeingDestroyed() && acceleratedCompositingForOverflowScrollEnabled())
            compositor().setShouldReevaluateCompositingAfterLayout();
    }
}

inline bool RenderLayer::needsCompositingLayersRebuiltForClip(const RenderStyle* oldStyle, const RenderStyle* newStyle) const
{
    ASSERT(newStyle);
    return oldStyle && (oldStyle->clip() != newStyle->clip() || oldStyle->hasClip() != newStyle->hasClip());
}

inline bool RenderLayer::needsCompositingLayersRebuiltForOverflow(const RenderStyle* oldStyle, const RenderStyle* newStyle) const
{
    ASSERT(newStyle);
    return !isComposited() && oldStyle && (oldStyle->overflowX() != newStyle->overflowX()) && stackingContainer()->hasCompositingDescendant();
}

void RenderLayer::styleChanged(StyleDifference diff, const RenderStyle* oldStyle)
{
    bool isNormalFlowOnly = shouldBeNormalFlowOnly();
    if (isNormalFlowOnly != m_isNormalFlowOnly) {
        m_isNormalFlowOnly = isNormalFlowOnly;
        RenderLayer* p = parent();
        if (p)
            p->dirtyNormalFlowList();
        dirtyStackingContainerZOrderLists();
    }

    if (renderer().style().overflowX() == OMARQUEE && renderer().style().marqueeBehavior() != MNONE && renderer().isBox()) {
        if (!m_marquee)
            m_marquee = std::make_unique<RenderMarquee>(this);
        m_marquee->updateMarqueeStyle();
    }
    else if (m_marquee) {
        m_marquee = nullptr;
    }

    updateScrollbarsAfterStyleChange(oldStyle);
    updateStackingContextsAfterStyleChange(oldStyle);
    // Overlay scrollbars can make this layer self-painting so we need
    // to recompute the bit once scrollbars have been updated.
    updateSelfPaintingLayer();
    updateOutOfFlowPositioned(oldStyle);

    if (!hasReflection() && m_reflection)
        removeReflection();
    else if (hasReflection()) {
        if (!m_reflection)
            createReflection();
        else
            m_reflection->setStyle(createReflectionStyle());
    }
    
    // FIXME: Need to detect a swap from custom to native scrollbars (and vice versa).
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();
    
    updateScrollCornerStyle();
    updateResizerStyle();

    updateDescendantDependentFlags();
    updateTransform();
#if ENABLE(CSS_COMPOSITING)
    updateBlendMode();
#endif
#if ENABLE(CSS_FILTERS)
    updateOrRemoveFilterClients();
#endif

    updateNeedsCompositedScrolling();

    const RenderStyle& newStyle = renderer().style();
    if (compositor().updateLayerCompositingState(*this)
        || needsCompositingLayersRebuiltForClip(oldStyle, &newStyle)
        || needsCompositingLayersRebuiltForOverflow(oldStyle, &newStyle))
        compositor().setCompositingLayersNeedRebuild();
    else if (isComposited()) {
        // FIXME: updating geometry here is potentially harmful, because layout is not up-to-date.
        backing()->updateGeometry();
        backing()->updateAfterDescendents();
    }

    if (oldStyle) {
        // Compositing layers keep track of whether they are clipped by any of the ancestors.
        // When the current layer's clipping behaviour changes, we need to propagate it to the descendants.
        const RenderStyle& style = renderer().style();
        bool wasClipping = oldStyle->hasClip() || oldStyle->overflowX() != OVISIBLE || oldStyle->overflowY() != OVISIBLE;
        bool isClipping = style.hasClip() || style.overflowX() != OVISIBLE || style.overflowY() != OVISIBLE;
        if (isClipping != wasClipping) {
            if (checkIfDescendantClippingContextNeedsUpdate(isClipping))
                compositor().setCompositingLayersNeedRebuild();
        }
    }

#if PLATFORM(IOS) && ENABLE(TOUCH_EVENTS)
    if (diff == StyleDifferenceRecompositeLayer || diff >= StyleDifferenceLayoutPositionedMovementOnly)
        renderer().document().dirtyTouchEventRects();
#else
    UNUSED_PARAM(diff);
#endif

#if ENABLE(CSS_FILTERS)
    updateOrRemoveFilterEffectRenderer();
    bool backingDidCompositeLayers = isComposited() && backing()->canCompositeFilters();
    if (isComposited() && backingDidCompositeLayers && !backing()->canCompositeFilters()) {
        // The filters used to be drawn by platform code, but now the platform cannot draw them anymore.
        // Fallback to drawing them in software.
        setBackingNeedsRepaint();
    }
#endif
}

void RenderLayer::updateScrollableAreaSet(bool hasOverflow)
{
    FrameView& frameView = renderer().view().frameView();

    bool isVisibleToHitTest = renderer().visibleToHitTesting();
    if (HTMLFrameOwnerElement* owner = frameView.frame().ownerElement())
        isVisibleToHitTest &= owner->renderer() && owner->renderer()->visibleToHitTesting();

    bool isScrollable = hasOverflow && isVisibleToHitTest;
    bool addedOrRemoved = false;
    if (isScrollable)
        addedOrRemoved = frameView.addScrollableArea(this);
    else
        addedOrRemoved = frameView.removeScrollableArea(this);
    
    if (addedOrRemoved)
        updateNeedsCompositedScrolling();

#if PLATFORM(IOS)
    if (addedOrRemoved) {
        if (isScrollable && !hasAcceleratedTouchScrolling())
            registerAsTouchEventListenerForScrolling();
        else {
            // We only need the touch listener for unaccelerated overflow scrolling, so if we became
            // accelerated, remove ourselves as a touch event listener.
            unregisterAsTouchEventListenerForScrolling();
        }
    }
#endif
}

void RenderLayer::updateScrollCornerStyle()
{
    RenderElement* actualRenderer = rendererForScrollbar(renderer());
    RefPtr<RenderStyle> corner = renderer().hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), &actualRenderer->style()) : PassRefPtr<RenderStyle>(0);

    if (!corner) {
        m_scrollCorner = nullptr;
        return;
    }

    if (!m_scrollCorner) {
        m_scrollCorner = createRenderer<RenderScrollbarPart>(renderer().document(), corner.releaseNonNull());
        m_scrollCorner->setParent(&renderer());
        m_scrollCorner->initializeStyle();
    } else
        m_scrollCorner->setStyle(corner.releaseNonNull());
}

void RenderLayer::updateResizerStyle()
{
    RenderElement* actualRenderer = rendererForScrollbar(renderer());
    RefPtr<RenderStyle> resizer = renderer().hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(PseudoStyleRequest(RESIZER), &actualRenderer->style()) : PassRefPtr<RenderStyle>(0);

    if (!resizer) {
        m_resizer = nullptr;
        return;
    }

    if (!m_resizer) {
        m_resizer = createRenderer<RenderScrollbarPart>(renderer().document(), resizer.releaseNonNull());
        m_resizer->setParent(&renderer());
        m_resizer->initializeStyle();
    } else
        m_resizer->setStyle(resizer.releaseNonNull());
}

RenderLayer* RenderLayer::reflectionLayer() const
{
    return m_reflection ? m_reflection->layer() : 0;
}

void RenderLayer::createReflection()
{
    ASSERT(!m_reflection);
    m_reflection = createRenderer<RenderReplica>(renderer().document(), createReflectionStyle());
    m_reflection->setParent(&renderer()); // We create a 1-way connection.
    m_reflection->initializeStyle();
}

void RenderLayer::removeReflection()
{
    if (!m_reflection->documentBeingDestroyed())
        m_reflection->removeLayers(this);

    m_reflection->setParent(0);
    m_reflection = nullptr;
}

PassRef<RenderStyle> RenderLayer::createReflectionStyle()
{
    auto newStyle = RenderStyle::create();
    newStyle.get().inheritFrom(&renderer().style());
    
    // Map in our transform.
    TransformOperations transform;
    switch (renderer().style().boxReflect()->direction()) {
        case ReflectionBelow:
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), Length(100., Percent), TransformOperation::TRANSLATE));
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), renderer().style().boxReflect()->offset(), TransformOperation::TRANSLATE));
            transform.operations().append(ScaleTransformOperation::create(1.0, -1.0, ScaleTransformOperation::SCALE));
            break;
        case ReflectionAbove:
            transform.operations().append(ScaleTransformOperation::create(1.0, -1.0, ScaleTransformOperation::SCALE));
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), Length(100., Percent), TransformOperation::TRANSLATE));
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), renderer().style().boxReflect()->offset(), TransformOperation::TRANSLATE));
            break;
        case ReflectionRight:
            transform.operations().append(TranslateTransformOperation::create(Length(100., Percent), Length(0, Fixed), TransformOperation::TRANSLATE));
            transform.operations().append(TranslateTransformOperation::create(renderer().style().boxReflect()->offset(), Length(0, Fixed), TransformOperation::TRANSLATE));
            transform.operations().append(ScaleTransformOperation::create(-1.0, 1.0, ScaleTransformOperation::SCALE));
            break;
        case ReflectionLeft:
            transform.operations().append(ScaleTransformOperation::create(-1.0, 1.0, ScaleTransformOperation::SCALE));
            transform.operations().append(TranslateTransformOperation::create(Length(100., Percent), Length(0, Fixed), TransformOperation::TRANSLATE));
            transform.operations().append(TranslateTransformOperation::create(renderer().style().boxReflect()->offset(), Length(0, Fixed), TransformOperation::TRANSLATE));
            break;
    }
    newStyle.get().setTransform(transform);

    // Map in our mask.
    newStyle.get().setMaskBoxImage(renderer().style().boxReflect()->mask());

    return newStyle;
}

#if ENABLE(CSS_FILTERS)

void RenderLayer::updateOrRemoveFilterClients()
{
    if (!hasFilter()) {
        FilterInfo::remove(*this);
        return;
    }

    if (renderer().style().filter().hasReferenceFilter())
        FilterInfo::get(*this).updateReferenceFilterClients(renderer().style().filter());
    else if (FilterInfo* filterInfo = FilterInfo::getIfExists(*this))
        filterInfo->removeReferenceFilterClients();
}

void RenderLayer::updateOrRemoveFilterEffectRenderer()
{
    // FilterEffectRenderer is only used to render the filters in software mode,
    // so we always need to run updateOrRemoveFilterEffectRenderer after the composited
    // mode might have changed for this layer.
    if (!paintsWithFilters()) {
        // Don't delete the whole filter info here, because we might use it
        // for loading SVG reference filter files.
        if (FilterInfo* filterInfo = FilterInfo::getIfExists(*this))
            filterInfo->setRenderer(0);

        // Early-return only if we *don't* have reference filters.
        // For reference filters, we still want the FilterEffect graph built
        // for us, even if we're composited.
        if (!renderer().style().filter().hasReferenceFilter())
            return;
    }
    
    FilterInfo& filterInfo = FilterInfo::get(*this);
    if (!filterInfo.renderer()) {
        RefPtr<FilterEffectRenderer> filterRenderer = FilterEffectRenderer::create();
        filterRenderer->setFilterScale(renderer().frame().page()->deviceScaleFactor());
        RenderingMode renderingMode = renderer().frame().settings().acceleratedFiltersEnabled() ? Accelerated : Unaccelerated;
        filterRenderer->setRenderingMode(renderingMode);
        filterInfo.setRenderer(filterRenderer.release());
        
        // We can optimize away code paths in other places if we know that there are no software filters.
        renderer().view().setHasSoftwareFilters(true);
    } else if (filterInfo.renderer()->filterScale() != renderer().frame().page()->deviceScaleFactor()) {
        filterInfo.renderer()->setFilterScale(renderer().frame().page()->deviceScaleFactor());
        filterInfo.renderer()->clearIntermediateResults();
    }

    // If the filter fails to build, remove it from the layer. It will still attempt to
    // go through regular processing (e.g. compositing), but never apply anything.
    if (!filterInfo.renderer()->build(&renderer(), renderer().style().filter(), FilterProperty))
        filterInfo.setRenderer(0);
}

void RenderLayer::filterNeedsRepaint()
{
    // We use the enclosing element so that we recalculate style for the ancestor of an anonymous object.
    if (Element* element = enclosingElement())
        element->setNeedsStyleRecalc(SyntheticStyleChange);
    renderer().repaint();
}

#endif

void RenderLayer::paintNamedFlowThreadInsideRegion(GraphicsContext* context, RenderNamedFlowFragment* region, LayoutRect paintDirtyRect, LayoutPoint paintOffset, PaintBehavior paintBehavior, PaintLayerFlags paintFlags)
{
    LayoutRect regionContentBox = toRenderBox(region->layerOwner()).contentBoxRect();
    LayoutSize moveOffset = region->flowThreadPortionLocation() - (paintOffset + regionContentBox.location()) + region->fragmentContainer().scrolledContentOffset();

    FloatPoint adjustedPaintOffset = roundedForPainting(LayoutPoint(-moveOffset.width(), -moveOffset.height()), renderer().document().deviceScaleFactor());
    paintDirtyRect.move(moveOffset);

    context->save();
    context->translate(adjustedPaintOffset.x(), adjustedPaintOffset.y());

    CurrentRenderFlowThreadMaintainer flowThreadMaintainer(toRenderFlowThread(&renderer()));
    CurrentRenderRegionMaintainer regionMaintainer(*region);

    region->setRegionObjectsRegionStyle();
    paint(context, paintDirtyRect, paintBehavior, nullptr, paintFlags | PaintLayerTemporaryClipRects);
    region->restoreRegionObjectsOriginalStyle();

    context->restore();
}

void RenderLayer::paintFlowThreadIfRegionForFragments(const LayerFragments& fragments, GraphicsContext* context, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    if (!renderer().isRenderNamedFlowFragmentContainer())
        return;
    
    RenderBlockFlow* renderNamedFlowFragmentContainer = toRenderBlockFlow(&renderer());
    RenderNamedFlowFragment* flowFragment = renderNamedFlowFragmentContainer->renderNamedFlowFragment();
    if (!flowFragment->isValid())
        return;

    RenderNamedFlowThread* flowThread = flowFragment->namedFlowThread();
    RenderLayer* flowThreadLayer = flowThread->layer();

    LayoutRect regionClipRect = LayoutRect::infiniteRect();
    if (flowFragment->shouldClipFlowThreadContent()) {
        regionClipRect = renderNamedFlowFragmentContainer->paddingBoxRect();

        // When the layer of the flow fragment's container is composited, the flow fragment container receives a
        // GraphicsLayer of its own so the clipping coordinates (caused by overflow:hidden) must be relative to the
        // GraphicsLayer coordinates in which the fragment gets painted. So what is computed so far is enough.
        // If the layer of the flowFragment is not composited, then we change the coordinates to be relative to the flow
        // thread's layer.
        if (!isComposited())
            regionClipRect.move(offsetFromAncestor(paintingInfo.rootLayer));
    }
    
    for (size_t i = 0; i < fragments.size(); ++i) {
        const LayerFragment& fragment = fragments.at(i);

        ClipRect clipRect = fragment.foregroundRect;
        if (flowFragment->shouldClipFlowThreadContent())
            clipRect.intersect(regionClipRect);

        bool shouldClip = (clipRect != LayoutRect::infiniteRect());
        // Optimize clipping for the single fragment case.
        if (shouldClip)
            clipToRect(paintingInfo, context, clipRect);

        flowThreadLayer->paintNamedFlowThreadInsideRegion(context, flowFragment, paintingInfo.paintDirtyRect, fragment.layerBounds.location() + paintingInfo.subpixelAccumulation,
            paintingInfo.paintBehavior, paintFlags);

        if (shouldClip)
            restoreClip(context, paintingInfo.paintDirtyRect, clipRect);
    }
}

RenderLayer* RenderLayer::hitTestFlowThreadIfRegionForFragments(const LayerFragments& fragments, RenderLayer*, const HitTestRequest& request, HitTestResult& result, const LayoutRect& hitTestRect,
    const HitTestLocation& hitTestLocation,
    const HitTestingTransformState* transformState, 
    double* zOffsetForDescendants, double* zOffset,
    const HitTestingTransformState* unflattenedTransformState, bool depthSortDescendants)
{
    if (!renderer().isRenderNamedFlowFragmentContainer())
        return 0;

    RenderNamedFlowFragment* region = toRenderBlockFlow(&renderer())->renderNamedFlowFragment();
    if (!region->isValid())
        return 0;

    RenderFlowThread* flowThread = region->flowThread();
    LayoutPoint portionLocation = region->flowThreadPortionRect().location();
    if (flowThread->style().isFlippedBlocksWritingMode()) {
        // The portion location coordinate must be translated into physical coordinates.
        if (flowThread->style().isHorizontalWritingMode())
            portionLocation.setY(flowThread->height() - (portionLocation.y() + region->contentHeight()));
        else
            portionLocation.setX(flowThread->width() - (portionLocation.x() + region->contentWidth()));
    }

    LayoutRect regionContentBox = toRenderBlockFlow(&renderer())->contentBoxRect();

    RenderLayer* resultLayer = 0;
    for (int i = fragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = fragments.at(i);

        if (!fragment.backgroundRect.intersects(hitTestLocation))
            continue;

        LayoutSize hitTestOffset = portionLocation - (fragment.layerBounds.location() + regionContentBox.location()) + region->fragmentContainer().scrolledContentOffset();

        // Always ignore clipping, since the RenderFlowThread has nothing to do with the bounds of the FrameView.
        HitTestRequest newRequest(request.type() | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowShadowContent);

        // Make a new temporary HitTestLocation in the new region.
        HitTestLocation newHitTestLocation(hitTestLocation, hitTestOffset);

        // Expand the hit-test rect to the flow thread's coordinate system.
        LayoutRect hitTestRectInFlowThread = hitTestRect;
        hitTestRectInFlowThread.move(hitTestOffset);
        hitTestRectInFlowThread.expand(LayoutSize(fabs((double)hitTestOffset.width()), fabs((double)hitTestOffset.height())));

        CurrentRenderFlowThreadMaintainer flowThreadMaintainer(flowThread);
        CurrentRenderRegionMaintainer regionMaintainer(*region);

        HitTestResult tempResult(result.hitTestLocation());
        RenderLayer* hitLayer = flowThread->layer()->hitTestLayer(flowThread->layer(), 0, newRequest, tempResult, hitTestRectInFlowThread, newHitTestLocation, false, transformState, zOffsetForDescendants);
        if (result.isRectBasedTest())
            result.append(tempResult);
        if (isHitCandidate(hitLayer, depthSortDescendants, zOffset, unflattenedTransformState)) {
            resultLayer = hitLayer;
            if (!result.isRectBasedTest())
                result = tempResult;
            if (!depthSortDescendants)
                break;
        }
    }

    return resultLayer;
}

RenderNamedFlowFragment* RenderLayer::currentRenderNamedFlowFragment() const
{
    return renderer().currentRenderNamedFlowFragment();
}

} // namespace WebCore

#ifndef NDEBUG

void showLayerTree(const WebCore::RenderLayer* layer)
{
    if (!layer)
        return;

    WTF::String output = externalRepresentation(&layer->renderer().frame(), WebCore::RenderAsTextShowAllLayers | WebCore::RenderAsTextShowLayerNesting | WebCore::RenderAsTextShowCompositedLayers | WebCore::RenderAsTextShowAddresses | WebCore::RenderAsTextShowIDAndClass | WebCore::RenderAsTextDontUpdateLayout | WebCore::RenderAsTextShowLayoutState | WebCore::RenderAsTextShowOverflow);
    fprintf(stderr, "%s\n", output.utf8().data());
}

void showLayerTree(const WebCore::RenderObject* renderer)
{
    if (!renderer)
        return;
    showLayerTree(renderer->enclosingLayer());
}

#endif
