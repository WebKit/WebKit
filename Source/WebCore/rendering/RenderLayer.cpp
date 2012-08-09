/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "ColumnInfo.h"
#include "CSSPropertyNames.h"
#include "Chrome.h"
#include "Document.h"
#include "DocumentEventQueue.h"
#include "EventHandler.h"
#if ENABLE(CSS_FILTERS)
#include "FEColorMatrix.h"
#include "FEMerge.h"
#include "FilterEffectRenderer.h"
#endif
#include "FloatConversion.h"
#include "FloatPoint3D.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
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
#include "OverflowEvent.h"
#include "OverlapTestRequestClient.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "RenderArena.h"
#include "RenderFlowThread.h"
#include "RenderInline.h"
#include "RenderMarquee.h"
#include "RenderReplica.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarPart.h"
#include "RenderTheme.h"
#include "RenderTreeAsText.h"
#include "RenderView.h"
#include "ScaleTransformOperation.h"
#include "ScrollAnimator.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "Settings.h"
#include "SourceGraphic.h"
#include "StylePropertySet.h"
#include "StyleResolver.h"
#include "TextStream.h"
#include "TransformationMatrix.h"
#include "TranslateTransformOperation.h"
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#endif

#if ENABLE(SVG)
#include "SVGNames.h"
#endif

#if PLATFORM(CHROMIUM) || PLATFORM(BLACKBERRY)
// FIXME: border radius clipping triggers too-slow path on Chromium
// https://bugs.webkit.org/show_bug.cgi?id=69866
#define DISABLE_ROUNDED_CORNER_CLIPPING
#endif

#define MIN_INTERSECT_FOR_REVEAL 32

using namespace std;

namespace WebCore {

using namespace HTMLNames;

const int MinimumWidthWhileResizing = 100;
const int MinimumHeightWhileResizing = 40;

bool ClipRect::intersects(const HitTestPoint& hitTestPoint)
{
    return hitTestPoint.intersects(m_rect);
}

RenderLayer::RenderLayer(RenderBoxModelObject* renderer)
    : m_inResizeMode(false)
    , m_scrollDimensionsDirty(true)
    , m_normalFlowListDirty(true)
    , m_hasSelfPaintingLayerDescendant(false)
    , m_hasSelfPaintingLayerDescendantDirty(false)
    , m_isRootLayer(renderer->isRenderView())
    , m_usedTransparency(false)
    , m_paintingInsideReflection(false)
    , m_inOverflowRelayout(false)
    , m_repaintStatus(NeedsNormalRepaint)
    , m_visibleContentStatusDirty(true)
    , m_hasVisibleContent(false)
    , m_visibleDescendantStatusDirty(false)
    , m_hasVisibleDescendant(false)
    , m_isPaginated(false)
    , m_3DTransformedDescendantStatusDirty(true)
    , m_has3DTransformedDescendant(false)
#if USE(ACCELERATED_COMPOSITING)
    , m_hasCompositingDescendant(false)
    , m_indirectCompositingReason(NoIndirectCompositingReason)
#endif
    , m_containsDirtyOverlayScrollbars(false)
#if !ASSERT_DISABLED
    , m_layerListMutationAllowed(true)
#endif
    , m_canSkipRepaintRectsUpdateOnScroll(renderer->isTableCell())
#if ENABLE(CSS_FILTERS)
    , m_hasFilterInfo(false)
#endif
    , m_renderer(renderer)
    , m_parent(0)
    , m_previous(0)
    , m_next(0)
    , m_first(0)
    , m_last(0)
    , m_posZOrderList(0)
    , m_negZOrderList(0)
    , m_normalFlowList(0)
    , m_marquee(0)
    , m_staticInlinePosition(0)
    , m_staticBlockPosition(0)
    , m_reflection(0)
    , m_scrollCorner(0)
    , m_resizer(0)
{
    m_isNormalFlowOnly = shouldBeNormalFlowOnly();
    m_isSelfPaintingLayer = shouldBeSelfPaintingLayer();

    // Non-stacking contexts should have empty z-order lists. As this is already the case,
    // there is no need to dirty / recompute these lists.
    m_zOrderListsDirty = isStackingContext();

    ScrollableArea::setConstrainsScrollingToContentEdge(false);

    if (!renderer->firstChild() && renderer->style()) {
        m_visibleContentStatusDirty = false;
        m_hasVisibleContent = renderer->style()->visibility() == VISIBLE;
    }

    Node* node = renderer->node();
    if (node && node->isElementNode()) {
        // We save and restore only the scrollOffset as the other scroll values are recalculated.
        Element* element = toElement(node);
        m_scrollOffset = element->savedLayerScrollOffset();
        if (!m_scrollOffset.isZero())
            scrollAnimator()->setCurrentPosition(FloatPoint(m_scrollOffset.width(), m_scrollOffset.height()));
        element->setSavedLayerScrollOffset(IntSize());
    }
}

RenderLayer::~RenderLayer()
{
    if (inResizeMode() && !renderer()->documentBeingDestroyed()) {
        if (Frame* frame = renderer()->frame())
            frame->eventHandler()->resizeLayerDestroyed();
    }

    if (Frame* frame = renderer()->frame()) {
        if (FrameView* frameView = frame->view())
            frameView->removeScrollableArea(this);
    }

    if (!m_renderer->documentBeingDestroyed()) {
        Node* node = m_renderer->node();
        if (node && node->isElementNode())
            toElement(node)->setSavedLayerScrollOffset(m_scrollOffset);
    }

    destroyScrollbar(HorizontalScrollbar);
    destroyScrollbar(VerticalScrollbar);

    if (m_reflection)
        removeReflection();
    
#if ENABLE(CSS_FILTERS)
    removeFilterInfoIfNeeded();
#endif

    // Child layers will be deleted by their corresponding render objects, so
    // we don't need to delete them ourselves.

    delete m_posZOrderList;
    delete m_negZOrderList;
    delete m_normalFlowList;
    delete m_marquee;

#if USE(ACCELERATED_COMPOSITING)
    clearBacking(true);
#endif
    
    if (m_scrollCorner)
        m_scrollCorner->destroy();
    if (m_resizer)
        m_resizer->destroy();
}

#if USE(ACCELERATED_COMPOSITING)
RenderLayerCompositor* RenderLayer::compositor() const
{
    ASSERT(renderer()->view());
    return renderer()->view()->compositor();
}

void RenderLayer::contentChanged(ContentChangeType changeType)
{
    // This can get called when video becomes accelerated, so the layers may change.
    if ((changeType == CanvasChanged || changeType == VideoChanged || changeType == FullScreenChanged) && compositor()->updateLayerCompositingState(this))
        compositor()->setCompositingLayersNeedRebuild();

    if (m_backing)
        m_backing->contentChanged(changeType);
}
#endif // USE(ACCELERATED_COMPOSITING)

bool RenderLayer::canRender3DTransforms() const
{
#if USE(ACCELERATED_COMPOSITING)
    return compositor()->canRender3DTransforms();
#else
    return false;
#endif
}

#if ENABLE(CSS_FILTERS)
bool RenderLayer::paintsWithFilters() const
{
    // FIXME: Eventually there will be more factors than isComposited() to decide whether or not to render the filter
    if (!renderer()->hasFilter())
        return false;
        
#if USE(ACCELERATED_COMPOSITING)
    if (!isComposited())
        return true;

    if (!m_backing || !m_backing->canCompositeFilters())
        return true;
#endif

    return false;
}
    
bool RenderLayer::requiresFullLayerImageForFilters() const 
{
    if (!paintsWithFilters())
        return false;
    FilterEffectRenderer* filter = filterRenderer();
    return filter ? filter->hasFilterThatMovesPixels() : false;
}
#endif

LayoutPoint RenderLayer::computeOffsetFromRoot(bool& hasLayerOffset) const
{
    hasLayerOffset = true;

    if (!parent())
        return LayoutPoint();

    // This is similar to root() but we check if an ancestor layer would
    // prevent the optimization from working.
    const RenderLayer* rootLayer = 0;
    for (const RenderLayer* parentLayer = parent(); parentLayer; rootLayer = parentLayer, parentLayer = parentLayer->parent()) {
        hasLayerOffset = parentLayer->canUseConvertToLayerCoords();
        if (!hasLayerOffset)
            return LayoutPoint();
    }
    ASSERT(rootLayer == root());

    LayoutPoint offset;
    parent()->convertToLayerCoords(rootLayer, offset);
    return offset;
}

void RenderLayer::updateLayerPositions(LayoutPoint* offsetFromRoot, UpdateLayerPositionsFlags flags)
{
#if !ASSERT_DISABLED
    if (offsetFromRoot) {
        bool hasLayerOffset;
        LayoutPoint computedOffsetFromRoot = computeOffsetFromRoot(hasLayerOffset);
        ASSERT(hasLayerOffset);
        ASSERT(*offsetFromRoot == computedOffsetFromRoot);
    }
#endif

    updateLayerPosition(); // For relpositioned layers or non-positioned layers,
                           // we need to keep in sync, since we may have shifted relative
                           // to our parent layer.
    LayoutPoint oldOffsetFromRoot;
    if (offsetFromRoot) {
        // We can't cache our offset to the repaint container if the mapping is anything more complex than a simple translation
        if (!canUseConvertToLayerCoords())
            offsetFromRoot = 0; // If our cached offset is invalid make sure it's not passed to any of our children
        else {
            oldOffsetFromRoot = *offsetFromRoot;
            // Frequently our parent layer's renderer will be the same as our renderer's containing block.  In that case,
            // we just update the cache using our offset to our parent (which is m_topLeft). Otherwise, regenerated cached
            // offsets to the root from the render tree.
            if (!m_parent || m_parent->renderer() == renderer()->containingBlock())
                offsetFromRoot->move(m_topLeft.x(), m_topLeft.y()); // Fast case
            else {
                LayoutPoint offset;
                convertToLayerCoords(root(), offset);
                *offsetFromRoot = offset;
            }
        }
    }

    LayoutPoint offset;
    if (offsetFromRoot) {
        offset = *offsetFromRoot;
#ifndef NDEBUG
        LayoutPoint computedOffsetFromRoot;
        convertToLayerCoords(root(), computedOffsetFromRoot);
        ASSERT(offset == computedOffsetFromRoot);
#endif
    } else {
        // FIXME: It looks suspicious to call convertToLayerCoords here
        // as canUseConvertToLayerCoords may be true for an ancestor layer.
        convertToLayerCoords(root(), offset);
    }
    positionOverflowControls(toSize(roundedIntPoint(offset)));

    updateDescendantDependentFlags();

    if (flags & UpdatePagination)
        updatePagination();
    else
        m_isPaginated = false;

    if (m_hasVisibleContent) {
        RenderView* view = renderer()->view();
        ASSERT(view);
        // FIXME: LayoutState does not work with RenderLayers as there is not a 1-to-1
        // mapping between them and the RenderObjects. It would be neat to enable
        // LayoutState outside the layout() phase and use it here.
        ASSERT(!view->layoutStateEnabled());

        RenderBoxModelObject* repaintContainer = renderer()->containerForRepaint();
        LayoutRect oldRepaintRect = m_repaintRect;
        LayoutRect oldOutlineBox = m_outlineBox;
        computeRepaintRects(offsetFromRoot);
        // FIXME: Should ASSERT that value calculated for m_outlineBox using the cached offset is the same
        // as the value not using the cached offset, but we can't due to https://bugs.webkit.org/show_bug.cgi?id=37048
        if (flags & CheckForRepaint) {
            if (view && !view->printing()) {
                if (m_repaintStatus & NeedsFullRepaint) {
                    renderer()->repaintUsingContainer(repaintContainer, oldRepaintRect);
                    if (m_repaintRect != oldRepaintRect)
                        renderer()->repaintUsingContainer(repaintContainer, m_repaintRect);
                } else if (shouldRepaintAfterLayout())
                    renderer()->repaintAfterLayoutIfNeeded(repaintContainer, oldRepaintRect, oldOutlineBox, &m_repaintRect, &m_outlineBox);
            }
        }
    } else
        clearRepaintRects();

    m_repaintStatus = NeedsNormalRepaint;

    // Go ahead and update the reflection's position and size.
    if (m_reflection)
        m_reflection->layout();

#if USE(ACCELERATED_COMPOSITING)
    // Clear the IsCompositingUpdateRoot flag once we've found the first compositing layer in this update.
    bool isUpdateRoot = (flags & IsCompositingUpdateRoot);
    if (isComposited())
        flags &= ~IsCompositingUpdateRoot;
#endif

    if (renderer()->hasColumns())
        flags |= UpdatePagination;

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositions(offsetFromRoot, flags);

#if USE(ACCELERATED_COMPOSITING)
    if ((flags & UpdateCompositingLayers) && isComposited())
        backing()->updateAfterLayout(RenderLayerBacking::CompositingChildren, isUpdateRoot);
#endif
        
    // With all our children positioned, now update our marquee if we need to.
    if (m_marquee)
        m_marquee->updateMarqueePosition();

    if (offsetFromRoot)
        *offsetFromRoot = oldOffsetFromRoot;
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

void RenderLayer::computeRepaintRects(LayoutPoint* offsetFromRoot)
{
    ASSERT(!m_visibleContentStatusDirty);

    RenderBoxModelObject* repaintContainer = renderer()->containerForRepaint();
    m_repaintRect = renderer()->clippedOverflowRectForRepaint(repaintContainer);
    m_outlineBox = renderer()->outlineBoundsForRepaint(repaintContainer, offsetFromRoot);
}


void RenderLayer::computeRepaintRectsIncludingDescendants()
{
    // FIXME: computeRepaintRects() has to walk up the parent chain for every layer to compute the rects.
    // We should make this more efficient.
    computeRepaintRects();

    for (RenderLayer* layer = firstChild(); layer; layer = layer->nextSibling())
        layer->computeRepaintRectsIncludingDescendants();
}

void RenderLayer::clearRepaintRects()
{
    ASSERT(!m_hasVisibleContent);
    ASSERT(!m_visibleContentStatusDirty);

    m_repaintRect = IntRect();
    m_outlineBox = IntRect();
}

void RenderLayer::updateLayerPositionsAfterScroll(UpdateLayerPositionsAfterScrollFlags flags)
{
    // FIXME: This shouldn't be needed, but there are some corner cases where
    // these flags are still dirty. Update so that the check below is valid.
    updateDescendantDependentFlags();

    // If we have no visible content and no visible descendants, there is no point recomputing
    // our rectangles as they will be empty. If our visibility changes, we are expected to
    // recompute all our positions anyway.
    if (!m_hasVisibleDescendant && !m_hasVisibleContent)
        return;

    updateLayerPosition();

    if ((flags & HasSeenFixedPositionedAncestor) || renderer()->style()->position() == FixedPosition) {
        // FIXME: Is it worth passing the offsetFromRoot around like in updateLayerPositions?
        computeRepaintRects();
        flags |= HasSeenFixedPositionedAncestor;
    } else if ((flags & HasSeenAncestorWithOverflowClip) && !m_canSkipRepaintRectsUpdateOnScroll) {
        // If we have seen an overflow clip, we should update our repaint rects as clippedOverflowRectForRepaint
        // intersects it with our ancestor overflow clip that may have moved.
        computeRepaintRects();
    }

    if (renderer()->hasOverflowClip())
        flags |= HasSeenAncestorWithOverflowClip;

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositionsAfterScroll(flags);

    // We don't update our reflection as scrolling is a translation which does not change the size()
    // of an object, thus RenderReplica will still repaint itself properly as the layer position was
    // updated above.

    if (m_marquee)
        m_marquee->updateMarqueePosition();
}

void RenderLayer::updateTransform()
{
    // hasTransform() on the renderer is also true when there is transform-style: preserve-3d or perspective set,
    // so check style too.
    bool hasTransform = renderer()->hasTransform() && renderer()->style()->hasTransform();
    bool had3DTransform = has3DTransform();

    bool hadTransform = m_transform;
    if (hasTransform != hadTransform) {
        if (hasTransform)
            m_transform = adoptPtr(new TransformationMatrix);
        else
            m_transform.clear();
        
        // Layers with transforms act as clip rects roots, so clear the cached clip rects here.
        clearClipRectsIncludingDescendants();
    }
    
    if (hasTransform) {
        RenderBox* box = renderBox();
        ASSERT(box);
        m_transform->makeIdentity();
        box->style()->applyTransform(*m_transform, box->borderBoxRect().size(), RenderStyle::IncludeTransformOrigin);
        makeMatrixRenderable(*m_transform, canRender3DTransforms());
    }

    if (had3DTransform != has3DTransform())
        dirty3DTransformedDescendantStatus();
}

TransformationMatrix RenderLayer::currentTransform() const
{
    if (!m_transform)
        return TransformationMatrix();

#if USE(ACCELERATED_COMPOSITING)
    if (renderer()->style()->isRunningAcceleratedAnimation()) {
        TransformationMatrix currTransform;
        RefPtr<RenderStyle> style = renderer()->animation()->getAnimatedStyleForRenderer(renderer());
        style->applyTransform(currTransform, renderBox()->borderBoxRect().size(), RenderStyle::IncludeTransformOrigin);
        makeMatrixRenderable(currTransform, canRender3DTransforms());
        return currTransform;
    }
#endif

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

static bool checkContainingBlockChainForPagination(RenderBoxModelObject* renderer, RenderBox* ancestorColumnsRenderer)
{
    RenderView* view = renderer->view();
    RenderBoxModelObject* prevBlock = renderer;
    RenderBlock* containingBlock;
    for (containingBlock = renderer->containingBlock();
         containingBlock && containingBlock != view && containingBlock != ancestorColumnsRenderer;
         containingBlock = containingBlock->containingBlock())
        prevBlock = containingBlock;
    
    // If the columns block wasn't in our containing block chain, then we aren't paginated by it.
    if (containingBlock != ancestorColumnsRenderer)
        return false;
        
    // If the previous block is absolutely positioned, then we can't be paginated by the columns block.
    if (prevBlock->isOutOfFlowPositioned())
        return false;
        
    // Otherwise we are paginated by the columns block.
    return true;
}

void RenderLayer::updatePagination()
{
    m_isPaginated = false;
    if (isComposited() || !parent())
        return; // FIXME: We will have to deal with paginated compositing layers someday.
                // FIXME: For now the RenderView can't be paginated.  Eventually printing will move to a model where it is though.
    
    if (isNormalFlowOnly()) {
        m_isPaginated = parent()->renderer()->hasColumns();
        return;
    }

    // If we're not normal flow, then we need to look for a multi-column object between us and our stacking context.
    RenderLayer* ancestorStackingContext = stackingContext();
    for (RenderLayer* curr = parent(); curr; curr = curr->parent()) {
        if (curr->renderer()->hasColumns()) {
            m_isPaginated = checkContainingBlockChainForPagination(renderer(), curr->renderBox());
            return;
        }
        if (curr == ancestorStackingContext)
            return;
    }
}

void RenderLayer::setHasVisibleContent()
{ 
    if (m_hasVisibleContent && !m_visibleContentStatusDirty) {
        ASSERT(!parent() || parent()->hasVisibleDescendant());
        return;
    }

    m_visibleContentStatusDirty = false; 
    m_hasVisibleContent = true;
    computeRepaintRects();
    if (!isNormalFlowOnly()) {
        // We don't collect invisible layers in z-order lists if we are not in compositing mode.
        // As we became visible, we need to dirty our stacking contexts ancestors to be properly
        // collected. FIXME: When compositing, we could skip this dirtying phase.
        for (RenderLayer* sc = stackingContext(); sc; sc = sc->stackingContext()) {
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

void RenderLayer::updateDescendantDependentFlags()
{
    if (m_visibleDescendantStatusDirty || m_hasSelfPaintingLayerDescendantDirty) {
        m_hasVisibleDescendant = false;
        m_hasSelfPaintingLayerDescendant = false;
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            child->updateDescendantDependentFlags();

            bool hasVisibleDescendant = child->m_hasVisibleContent || child->m_hasVisibleDescendant;
            bool hasSelfPaintingLayerDescendant = child->isSelfPaintingLayer() || child->hasSelfPaintingLayerDescendant();

            m_hasVisibleDescendant |= hasVisibleDescendant;
            m_hasSelfPaintingLayerDescendant |= hasSelfPaintingLayerDescendant;

            if (m_hasVisibleDescendant && m_hasSelfPaintingLayerDescendant)
                break;
        }
        m_visibleDescendantStatusDirty = false;
        m_hasSelfPaintingLayerDescendantDirty = false;
    }

    if (m_visibleContentStatusDirty) {
        if (renderer()->style()->visibility() == VISIBLE)
            m_hasVisibleContent = true;
        else {
            // layer may be hidden but still have some visible content, check for this
            m_hasVisibleContent = false;
            RenderObject* r = renderer()->firstChild();
            while (r) {
                if (r->style()->visibility() == VISIBLE && !r->hasLayer()) {
                    m_hasVisibleContent = true;
                    break;
                }
                if (r->firstChild() && !r->hasLayer())
                    r = r->firstChild();
                else if (r->nextSibling())
                    r = r->nextSibling();
                else {
                    do {
                        r = r->parent();
                        if (r == renderer())
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

void RenderLayer::dirty3DTransformedDescendantStatus()
{
    RenderLayer* curr = stackingContext();
    if (curr)
        curr->m_3DTransformedDescendantStatusDirty = true;
        
    // This propagates up through preserve-3d hierarchies to the enclosing flattening layer.
    // Note that preserves3D() creates stacking context, so we can just run up the stacking contexts.
    while (curr && curr->preserves3D()) {
        curr->m_3DTransformedDescendantStatusDirty = true;
        curr = curr->stackingContext();
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

void RenderLayer::updateLayerPosition()
{
    LayoutPoint localPoint;
    LayoutSize inlineBoundingBoxOffset; // We don't put this into the RenderLayer x/y for inlines, so we need to subtract it out when done.
    if (renderer()->isRenderInline()) {
        RenderInline* inlineFlow = toRenderInline(renderer());
        IntRect lineBox = inlineFlow->linesBoundingBox();
        setSize(lineBox.size());
        inlineBoundingBoxOffset = toSize(lineBox.location());
        localPoint += inlineBoundingBoxOffset;
    } else if (RenderBox* box = renderBox()) {
        // FIXME: Is snapping the size really needed here for the RenderBox case?
        setSize(pixelSnappedIntSize(box->size(), box->location()));
        localPoint += box->topLeftLocationOffset();
    }

    // Clear our cached clip rect information.
    clearClipRects();
 
    if (!renderer()->isOutOfFlowPositioned() && renderer()->parent()) {
        // We must adjust our position by walking up the render tree looking for the
        // nearest enclosing object with a layer.
        RenderObject* curr = renderer()->parent();
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
    if (renderer()->isOutOfFlowPositioned() && enclosingPositionedAncestor()) {
        RenderLayer* positionedParent = enclosingPositionedAncestor();

        // For positioned layers, we subtract out the enclosing positioned layer's scroll offset.
        LayoutSize offset = positionedParent->scrolledContentOffset();
        localPoint -= offset;
        
        if (renderer()->isOutOfFlowPositioned() && positionedParent->renderer()->isRelPositioned() && positionedParent->renderer()->isRenderInline()) {
            LayoutSize offset = toRenderInline(positionedParent->renderer())->relativePositionedInlineOffset(toRenderBox(renderer()));
            localPoint += offset;
        }
    } else if (parent()) {
        if (isComposited()) {
            // FIXME: Composited layers ignore pagination, so about the best we can do is make sure they're offset into the appropriate column.
            // They won't split across columns properly.
            LayoutSize columnOffset;
            if (!parent()->renderer()->hasColumns() && parent()->renderer()->isRoot() && renderer()->view()->hasColumns())
                renderer()->view()->adjustForColumns(columnOffset, localPoint);
            else
                parent()->renderer()->adjustForColumns(columnOffset, localPoint);

            localPoint += columnOffset;
        }

        IntSize scrollOffset = parent()->scrolledContentOffset();
        localPoint -= scrollOffset;
    }
        
    if (renderer()->isRelPositioned()) {
        m_relativeOffset = renderer()->relativePositionOffset();
        localPoint.move(m_relativeOffset);
    } else {
        m_relativeOffset = LayoutSize();
    }

    // FIXME: We'd really like to just get rid of the concept of a layer rectangle and rely on the renderers.
    localPoint -= inlineBoundingBoxOffset;
    setLocation(localPoint.x(), localPoint.y());
}

TransformationMatrix RenderLayer::perspectiveTransform() const
{
    if (!renderer()->hasTransform())
        return TransformationMatrix();

    RenderStyle* style = renderer()->style();
    if (!style->hasPerspective())
        return TransformationMatrix();

    // Maybe fetch the perspective from the backing?
    const LayoutRect borderBox = toRenderBox(renderer())->borderBoxRect();
    const float boxWidth = borderBox.width();
    const float boxHeight = borderBox.height();

    float perspectiveOriginX = floatValueForLength(style->perspectiveOriginX(), boxWidth);
    float perspectiveOriginY = floatValueForLength(style->perspectiveOriginY(), boxHeight);

    // A perspective origin of 0,0 makes the vanishing point in the center of the element.
    // We want it to be in the top-left, so subtract half the height and width.
    perspectiveOriginX -= boxWidth / 2.0f;
    perspectiveOriginY -= boxHeight / 2.0f;
    
    TransformationMatrix t;
    t.translate(perspectiveOriginX, perspectiveOriginY);
    t.applyPerspective(style->perspective());
    t.translate(-perspectiveOriginX, -perspectiveOriginY);
    
    return t;
}

FloatPoint RenderLayer::perspectiveOrigin() const
{
    if (!renderer()->hasTransform())
        return FloatPoint();

    const LayoutRect borderBox = toRenderBox(renderer())->borderBoxRect();
    RenderStyle* style = renderer()->style();

    return FloatPoint(floatValueForLength(style->perspectiveOriginX(), borderBox.width()),
                      floatValueForLength(style->perspectiveOriginY(), borderBox.height()));
}

RenderLayer* RenderLayer::stackingContext() const
{
    RenderLayer* layer = parent();
    while (layer && !layer->isRootLayer() && !layer->renderer()->isRoot() && layer->renderer()->style()->hasAutoZIndex())
        layer = layer->parent();
    return layer;
}

static inline bool isPositionedContainer(RenderLayer* layer)
{
    RenderBoxModelObject* layerRenderer = layer->renderer();
    return layer->isRootLayer() || layerRenderer->isOutOfFlowPositioned() || layerRenderer->isRelPositioned() || layer->hasTransform();
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

RenderLayer* RenderLayer::enclosingScrollableLayer() const
{
    for (RenderObject* nextRenderer = renderer()->parent(); nextRenderer; nextRenderer = nextRenderer->parent()) {
        if (nextRenderer->isBox() && toRenderBox(nextRenderer)->canBeScrolledAndHasScrollableArea())
            return nextRenderer->enclosingLayer();
    }

    return 0;
}

IntRect RenderLayer::scrollableAreaBoundingBox() const
{
    return renderer()->absoluteBoundingBoxRect();
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
    return layer->isNormalFlowOnly() ? layer->parent() : layer->stackingContext();
}

inline bool RenderLayer::shouldRepaintAfterLayout() const
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_repaintStatus == NeedsNormalRepaint)
        return true;

    // Composited layers that were moved during a positioned movement only
    // layout, don't need to be repainted. They just need to be recomposited.
    ASSERT(m_repaintStatus == NeedsFullRepaintForPositionedMovementLayout);
    return !isComposited();
#else
    return true;
#endif
}

#if USE(ACCELERATED_COMPOSITING)
RenderLayer* RenderLayer::enclosingCompositingLayer(bool includeSelf) const
{
    if (includeSelf && isComposited())
        return const_cast<RenderLayer*>(this);

    for (const RenderLayer* curr = compositingContainer(this); curr; curr = compositingContainer(curr)) {
        if (curr->isComposited())
            return const_cast<RenderLayer*>(curr);
    }
         
    return 0;
}

RenderLayer* RenderLayer::enclosingCompositingLayerForRepaint(bool includeSelf) const
{
    if (includeSelf && isComposited() && !backing()->paintsIntoCompositedAncestor())
        return const_cast<RenderLayer*>(this);

    for (const RenderLayer* curr = compositingContainer(this); curr; curr = compositingContainer(curr)) {
        if (curr->isComposited() && !curr->backing()->paintsIntoCompositedAncestor())
            return const_cast<RenderLayer*>(curr);
    }
         
    return 0;
}
#endif

#if ENABLE(CSS_FILTERS)
RenderLayer* RenderLayer::enclosingFilterLayer(bool includeSelf) const
{
    const RenderLayer* curr = includeSelf ? this : parent();
    for (; curr; curr = curr->parent()) {
        if (curr->requiresFullLayerImageForFilters())
            return const_cast<RenderLayer*>(curr);
    }
    
    return 0;
}

RenderLayer* RenderLayer::enclosingFilterRepaintLayer() const
{
    for (const RenderLayer* curr = this; curr; curr = curr->parent()) {
        if ((curr != this && curr->requiresFullLayerImageForFilters()) || curr->isComposited() || curr->isRootLayer())
            return const_cast<RenderLayer*>(curr);
    }
    return 0;
}

void RenderLayer::setFilterBackendNeedsRepaintingInRect(const LayoutRect& rect, bool immediate)
{
    if (rect.isEmpty())
        return;
    
    LayoutRect rectForRepaint = rect;
    
#if ENABLE(CSS_FILTERS)
    if (renderer()->style()->hasFilterOutsets()) {
        int topOutset;
        int rightOutset;
        int bottomOutset;
        int leftOutset;
        renderer()->style()->getFilterOutsets(topOutset, rightOutset, bottomOutset, leftOutset);
        rectForRepaint.move(-leftOutset, -topOutset);
        rectForRepaint.expand(leftOutset + rightOutset, topOutset + bottomOutset);
    }
#endif

    RenderLayerFilterInfo* filterInfo = this->filterInfo();
    ASSERT(filterInfo);
    filterInfo->expandDirtySourceRect(rectForRepaint);
    
#if ENABLE(CSS_SHADERS)
    ASSERT(filterInfo->renderer());
    if (filterInfo->renderer()->hasCustomShaderFilter()) {
        // If we have at least one custom shader, we need to update the whole bounding box of the layer, because the
        // shader can address any ouput pixel.
        // Note: This is only for output rect, so there's no need to expand the dirty source rect.
        rectForRepaint.unite(calculateLayerBounds(this, this));
    }
#endif
    
    RenderLayer* parentLayer = enclosingFilterRepaintLayer();
    ASSERT(parentLayer);
    FloatQuad repaintQuad(rectForRepaint);
    LayoutRect parentLayerRect = renderer()->localToContainerQuad(repaintQuad, parentLayer->renderer()).enclosingBoundingBox();
    
#if USE(ACCELERATED_COMPOSITING)
    if (parentLayer->isComposited()) {
        if (!parentLayer->backing()->paintsIntoWindow()) {
            parentLayer->setBackingNeedsRepaintInRect(parentLayerRect);
            return;
        }
        // If the painting goes to window, redirect the painting to the parent RenderView.
        parentLayer = renderer()->view()->layer();
        parentLayerRect = renderer()->localToContainerQuad(repaintQuad, parentLayer->renderer()).enclosingBoundingBox();
    }
#endif

    if (parentLayer->paintsWithFilters()) {
        parentLayer->setFilterBackendNeedsRepaintingInRect(parentLayerRect, immediate);
        return;        
    }
    
    if (parentLayer->isRootLayer()) {
        RenderView* view = toRenderView(parentLayer->renderer());
        view->repaintViewRectangle(parentLayerRect, immediate);
        return;
    }
    
    ASSERT_NOT_REACHED();
}

bool RenderLayer::hasAncestorWithFilterOutsets() const
{
    for (const RenderLayer* curr = this; curr; curr = curr->parent()) {
        RenderBoxModelObject* renderer = curr->renderer();
        if (renderer->style()->hasFilterOutsets())
            return true;
    }
    return false;
}
#endif
    
RenderLayer* RenderLayer::clippingRootForPainting() const
{
#if USE(ACCELERATED_COMPOSITING)
    if (isComposited())
        return const_cast<RenderLayer*>(this);
#endif

    const RenderLayer* current = this;
    while (current) {
        if (current->isRootLayer())
            return const_cast<RenderLayer*>(current);

        current = compositingContainer(current);
        ASSERT(current);
        if (current->transform()
#if USE(ACCELERATED_COMPOSITING)
            || (current->isComposited() && !current->backing()->paintsIntoCompositedAncestor())
#endif
        )
            return const_cast<RenderLayer*>(current);
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutPoint RenderLayer::absoluteToContents(const LayoutPoint& absolutePoint) const
{
    // We don't use convertToLayerCoords because it doesn't know about transforms
    return roundedLayoutPoint(renderer()->absoluteToLocal(absolutePoint, false, true));
}

bool RenderLayer::cannotBlitToWindow() const
{
    if (isTransparent() || hasReflection() || hasTransform())
        return true;
    if (!parent())
        return false;
    return parent()->cannotBlitToWindow();
}

bool RenderLayer::isTransparent() const
{
#if ENABLE(SVG)
    if (renderer()->node() && renderer()->node()->namespaceURI() == SVGNames::svgNamespaceURI)
        return false;
#endif
    return renderer()->isTransparent() || renderer()->hasMask();
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

static LayoutRect transparencyClipBox(const RenderLayer*, const RenderLayer* rootLayer, PaintBehavior);

static void expandClipRectForDescendantsAndReflection(LayoutRect& clipRect, const RenderLayer* layer, const RenderLayer* rootLayer, PaintBehavior paintBehavior)
{
    // If we have a mask, then the clip is limited to the border box area (and there is
    // no need to examine child layers).
    if (!layer->renderer()->hasMask()) {
        // Note: we don't have to walk z-order lists since transparent elements always establish
        // a stacking context.  This means we can just walk the layer tree directly.
        for (RenderLayer* curr = layer->firstChild(); curr; curr = curr->nextSibling()) {
            if (!layer->reflection() || layer->reflectionLayer() != curr)
                clipRect.unite(transparencyClipBox(curr, rootLayer, paintBehavior));
        }
    }

    // If we have a reflection, then we need to account for that when we push the clip.  Reflect our entire
    // current transparencyClipBox to catch all child layers.
    // FIXME: Accelerated compositing will eventually want to do something smart here to avoid incorporating this
    // size into the parent layer.
    if (layer->renderer()->hasReflection()) {
        LayoutPoint delta;
        layer->convertToLayerCoords(rootLayer, delta);
        clipRect.move(-delta.x(), -delta.y());
        clipRect.unite(layer->renderBox()->reflectedRect(clipRect));
        clipRect.moveBy(delta);
    }
}

static LayoutRect transparencyClipBox(const RenderLayer* layer, const RenderLayer* rootLayer, PaintBehavior paintBehavior)
{
    // FIXME: Although this function completely ignores CSS-imposed clipping, we did already intersect with the
    // paintDirtyRect, and that should cut down on the amount we have to paint.  Still it
    // would be better to respect clips.
    
    if (rootLayer != layer && layer->paintsWithTransform(paintBehavior)) {
        // The best we can do here is to use enclosed bounding boxes to establish a "fuzzy" enough clip to encompass
        // the transformed layer and all of its children.
        LayoutPoint delta;
        layer->convertToLayerCoords(rootLayer, delta);

        TransformationMatrix transform;
        transform.translate(delta.x(), delta.y());
        transform = transform * *layer->transform();

        LayoutRect clipRect = layer->boundingBox(layer);
        expandClipRectForDescendantsAndReflection(clipRect, layer, layer, paintBehavior);
        return transform.mapRect(clipRect);
    }
    
    LayoutRect clipRect = layer->boundingBox(rootLayer);
    expandClipRectForDescendantsAndReflection(clipRect, layer, rootLayer, paintBehavior);
    return clipRect;
}

LayoutRect RenderLayer::paintingExtent(const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, PaintBehavior paintBehavior)
{
    return intersection(transparencyClipBox(this, rootLayer, paintBehavior), paintDirtyRect);
}

void RenderLayer::beginTransparencyLayers(GraphicsContext* context, const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, PaintBehavior paintBehavior)
{
    if (context->paintingDisabled() || (paintsWithTransparency(paintBehavior) && m_usedTransparency))
        return;
    
    RenderLayer* ancestor = transparentPaintingAncestor();
    if (ancestor)
        ancestor->beginTransparencyLayers(context, rootLayer, paintDirtyRect, paintBehavior);
    
    if (paintsWithTransparency(paintBehavior)) {
        m_usedTransparency = true;
        context->save();
        LayoutRect clipRect = paintingExtent(rootLayer, paintDirtyRect, paintBehavior);
        context->clip(clipRect);
        context->beginTransparencyLayer(renderer()->opacity());
#ifdef REVEAL_TRANSPARENCY_LAYERS
        context->setFillColor(Color(0.0f, 0.0f, 0.5f, 0.2f), ColorSpaceDeviceRGB);
        context->fillRect(clipRect);
#endif
    }
}

void* RenderLayer::operator new(size_t sz, RenderArena* renderArena)
{
    return renderArena->allocate(sz);
}

void RenderLayer::operator delete(void* ptr, size_t sz)
{
    // Stash size where destroy can find it.
    *(size_t *)ptr = sz;
}

void RenderLayer::destroy(RenderArena* renderArena)
{
    delete this;

    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

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
        // Dirty the z-order list in which we are contained.  The stackingContext() can be null in the
        // case where we're building up generated content layers.  This is ok, since the lists will start
        // off dirty in that case anyway.
        child->dirtyStackingContextZOrderLists();
    }

    child->updateDescendantDependentFlags();
    if (child->m_hasVisibleContent || child->m_hasVisibleDescendant)
        setAncestorChainHasVisibleDescendant();

    if (child->isSelfPaintingLayer() || child->hasSelfPaintingLayerDescendant())
        setAncestorChainHasSelfPaintingLayerDescendant();

#if USE(ACCELERATED_COMPOSITING)
    compositor()->layerWasAdded(this, child);
#endif
}

RenderLayer* RenderLayer::removeChild(RenderLayer* oldChild)
{
#if USE(ACCELERATED_COMPOSITING)
    if (!renderer()->documentBeingDestroyed())
        compositor()->layerWillBeRemoved(this, oldChild);
#endif

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
        // from the main layer tree, so we need to null-check the |stackingContext| value.
        oldChild->dirtyStackingContextZOrderLists();
    }

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);
    
    oldChild->updateDescendantDependentFlags();
    if (oldChild->m_hasVisibleContent || oldChild->m_hasVisibleDescendant)
        dirtyAncestorChainVisibleDescendantStatus();

    if (oldChild->isSelfPaintingLayer() || oldChild->hasSelfPaintingLayerDescendant())
        dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

    return oldChild;
}

void RenderLayer::removeOnlyThisLayer()
{
    if (!m_parent)
        return;

    // Mark that we are about to lose our layer. This makes render tree
    // walks ignore this layer while we're removing it.
    m_renderer->setHasLayer(false);

#if USE(ACCELERATED_COMPOSITING)
    compositor()->layerWillBeRemoved(m_parent, this);
#endif

    // Dirty the clip rects.
    clearClipRectsIncludingDescendants();

    RenderLayer* nextSib = nextSibling();
    bool hasLayerOffset;
    const LayoutPoint offsetFromRootBeforeMove = computeOffsetFromRoot(hasLayerOffset);

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
        LayoutPoint offsetFromRoot = offsetFromRootBeforeMove;
        // updateLayerPositions depends on hasLayer() already being false for proper layout.
        ASSERT(!renderer()->hasLayer());
        current->updateLayerPositions(hasLayerOffset ? &offsetFromRoot : 0);
        current = next;
    }

    // Remove us from the parent.
    m_parent->removeChild(this);
    m_renderer->destroyLayer();
}

void RenderLayer::insertOnlyThisLayer()
{
    if (!m_parent && renderer()->parent()) {
        // We need to connect ourselves when our renderer() has a parent.
        // Find our enclosingLayer and add ourselves.
        RenderLayer* parentLayer = renderer()->parent()->enclosingLayer();
        ASSERT(parentLayer);
        RenderLayer* beforeChild = parentLayer->reflectionLayer() != this ? renderer()->parent()->findNextLayer(parentLayer, renderer()) : 0;
        parentLayer->addChild(this, beforeChild);
    }

    // Remove all descendant layers from the hierarchy and add them to the new position.
    for (RenderObject* curr = renderer()->firstChild(); curr; curr = curr->nextSibling())
        curr->moveLayers(m_parent, this);

    // Clear out all the clip rects.
    clearClipRectsIncludingDescendants();
}

void RenderLayer::convertToPixelSnappedLayerCoords(const RenderLayer* ancestorLayer, IntPoint& roundedLocation) const
{
    LayoutPoint location = roundedLocation;
    convertToLayerCoords(ancestorLayer, location);
    roundedLocation = roundedIntPoint(location);
}

void RenderLayer::convertToPixelSnappedLayerCoords(const RenderLayer* ancestorLayer, IntRect& roundedRect) const
{
    LayoutRect rect = roundedRect;
    convertToLayerCoords(ancestorLayer, rect);
    roundedRect = pixelSnappedIntRect(rect);
}

void RenderLayer::convertToLayerCoords(const RenderLayer* ancestorLayer, LayoutPoint& location) const
{
    if (ancestorLayer == this)
        return;

    EPosition position = renderer()->style()->position();

    // FIXME: Positioning of out-of-flow(fixed, absolute) elements collected in a RenderFlowThread
    // may need to be revisited in a future patch.
    // If the fixed renderer is inside a RenderFlowThread, we should not compute location using localToAbsolute,
    // since localToAbsolute maps the coordinates from named flow to regions coordinates and regions can be
    // positioned in a completely different place in the viewport (RenderView).
    if (position == FixedPosition && !renderer()->inRenderFlowThread() && (!ancestorLayer || ancestorLayer == renderer()->view()->layer())) {
        // If the fixed layer's container is the root, just add in the offset of the view. We can obtain this by calling
        // localToAbsolute() on the RenderView.
        FloatPoint absPos = renderer()->localToAbsolute(FloatPoint(), true);
        location += LayoutSize(absPos.x(), absPos.y());
        return;
    }

    // For the fixed positioned elements inside a render flow thread, we should also skip the code path below
    // Otherwise, for the case of ancestorLayer == rootLayer and fixed positioned element child of a transformed
    // element in render flow thread, we will hit the fixed positioned container before hitting the ancestor layer.
    if (position == FixedPosition && !renderer()->inRenderFlowThread()) {
        // For a fixed layers, we need to walk up to the root to see if there's a fixed position container
        // (e.g. a transformed layer). It's an error to call convertToLayerCoords() across a layer with a transform,
        // so we should always find the ancestor at or before we find the fixed position container.
        RenderLayer* fixedPositionContainerLayer = 0;
        bool foundAncestor = false;
        for (RenderLayer* currLayer = parent(); currLayer; currLayer = currLayer->parent()) {
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
            LayoutPoint fixedContainerCoords;
            convertToLayerCoords(fixedPositionContainerLayer, fixedContainerCoords);

            LayoutPoint ancestorCoords;
            ancestorLayer->convertToLayerCoords(fixedPositionContainerLayer, ancestorCoords);

            location += (fixedContainerCoords - ancestorCoords);
            return;
        }
    }
    
    RenderLayer* parentLayer;
    if (position == AbsolutePosition || position == FixedPosition) {
        // Do what enclosingPositionedAncestor() does, but check for ancestorLayer along the way.
        parentLayer = parent();
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
        if (renderer()->inRenderFlowThread() && !renderer()->isRenderFlowThread())
            ASSERT(parentLayer != renderer()->view()->layer());

        if (foundAncestorFirst) {
            // Found ancestorLayer before the abs. positioned container, so compute offset of both relative
            // to enclosingPositionedAncestor and subtract.
            RenderLayer* positionedAncestor = parentLayer->enclosingPositionedAncestor();

            LayoutPoint thisCoords;
            convertToLayerCoords(positionedAncestor, thisCoords);
            
            LayoutPoint ancestorCoords;
            ancestorLayer->convertToLayerCoords(positionedAncestor, ancestorCoords);

            location += (thisCoords - ancestorCoords);
            return;
        }
    } else
        parentLayer = parent();
    
    if (!parentLayer)
        return;

    parentLayer->convertToLayerCoords(ancestorLayer, location);

    location += toSize(m_topLeft);
}

void RenderLayer::convertToLayerCoords(const RenderLayer* ancestorLayer, LayoutRect& rect) const
{
    LayoutPoint delta;
    convertToLayerCoords(ancestorLayer, delta);
    rect.move(-delta.x(), -delta.y());
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
    Frame* frame = renderer()->frame();
    if (!frame)
        return;
    
    IntPoint currentMousePosition = frame->eventHandler()->currentMousePosition();
    
    // We need to check if the current mouse position is out of the window. When the mouse is out of the window, the position is incoherent
    static IntPoint previousMousePosition;
    if (currentMousePosition.x() < 0 || currentMousePosition.y() < 0)
        currentMousePosition = previousMousePosition;
    else
        previousMousePosition = currentMousePosition;

    IntSize delta = currentMousePosition - sourcePoint;

    if (abs(delta.width()) <= ScrollView::noPanScrollRadius) // at the center we let the space for the icon
        delta.setWidth(0);
    if (abs(delta.height()) <= ScrollView::noPanScrollRadius)
        delta.setHeight(0);

    scrollByRecursively(adjustedScrollDelta(delta), ScrollOffsetClamped);
}

void RenderLayer::scrollByRecursively(const IntSize& delta, ScrollOffsetClamping clamp)
{
    if (delta.isZero())
        return;

    bool restrictedByLineClamp = false;
    if (renderer()->parent())
        restrictedByLineClamp = !renderer()->parent()->style()->lineClamp().isNone();

    if (renderer()->hasOverflowClip() && !restrictedByLineClamp) {
        IntSize newScrollOffset = scrollOffset() + delta;
        scrollToOffset(newScrollOffset, clamp);

        // If this layer can't do the scroll we ask the next layer up that can scroll to try
        IntSize remainingScrollOffset = newScrollOffset - scrollOffset();
        if (!remainingScrollOffset.isZero() && renderer()->parent()) {
            if (RenderLayer* scrollableLayer = enclosingScrollableLayer())
                scrollableLayer->scrollByRecursively(remainingScrollOffset);

            Frame* frame = renderer()->frame();
            if (frame)
                frame->eventHandler()->updateAutoscrollRenderer();
        }
    } else if (renderer()->view()->frameView()) {
        // If we are here, we were called on a renderer that can be programmatically scrolled, but doesn't
        // have an overflow clip. Which means that it is a document node that can be scrolled.
        renderer()->view()->frameView()->scrollBy(delta);
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

    int x = min(max(scrollOffset.width(), 0), maxX);
    int y = min(max(scrollOffset.height(), 0), maxY);
    return IntSize(x, y);
}

void RenderLayer::scrollToOffset(const IntSize& scrollOffset, ScrollOffsetClamping clamp)
{
    IntSize newScrollOffset = clamp == ScrollOffsetClamped ? clampScrollOffset(scrollOffset) : scrollOffset;
    if (newScrollOffset != this->scrollOffset())
        scrollToOffsetWithoutAnimation(toPoint(newScrollOffset));
}

void RenderLayer::scrollTo(int x, int y)
{
    RenderBox* box = renderBox();
    if (!box)
        return;

    if (box->style()->overflowX() != OMARQUEE) {
        // Ensure that the dimensions will be computed if they need to be (for overflow:hidden blocks).
        if (m_scrollDimensionsDirty)
            computeScrollDimensions();
    }
    
    // FIXME: Eventually, we will want to perform a blit.  For now never
    // blit, since the check for blitting is going to be very
    // complicated (since it will involve testing whether our layer
    // is either occluded by another layer or clipped by an enclosing
    // layer or contains fixed backgrounds, etc.).
    IntSize newScrollOffset = IntSize(x - scrollOrigin().x(), y - scrollOrigin().y());
    if (m_scrollOffset == newScrollOffset)
        return;
    m_scrollOffset = newScrollOffset;

    // Update the positions of our child layers (if needed as only fixed layers should be impacted by a scroll).
    // We don't update compositing layers, because we need to do a deep update from the compositing ancestor.
    updateLayerPositionsAfterScroll();

    RenderView* view = renderer()->view();
    
    // We should have a RenderView if we're trying to scroll.
    ASSERT(view);
    if (view) {
#if ENABLE(DASHBOARD_SUPPORT) || ENABLE(WIDGET_REGION)
        // Update dashboard regions, scrolling may change the clip of a
        // particular region.
        view->frameView()->updateDashboardRegions();
#endif

        view->updateWidgetPositions();
    }

    updateCompositingLayersAfterScroll();

    RenderBoxModelObject* repaintContainer = renderer()->containerForRepaint();
    Frame* frame = renderer()->frame();
    if (frame) {
        // The caret rect needs to be invalidated after scrolling
        frame->selection()->setCaretRectNeedsUpdate();

        FloatQuad quadForFakeMouseMoveEvent = FloatQuad(m_repaintRect);
        if (repaintContainer)
            quadForFakeMouseMoveEvent = repaintContainer->localToAbsoluteQuad(quadForFakeMouseMoveEvent);
        frame->eventHandler()->dispatchFakeMouseMoveEventSoonInQuad(quadForFakeMouseMoveEvent);
    }

    // Just schedule a full repaint of our object.
    if (view)
        renderer()->repaintUsingContainer(repaintContainer, m_repaintRect);

    // Schedule the scroll DOM event.
    if (renderer()->node())
        renderer()->node()->document()->eventQueue()->enqueueOrDispatchScrollEvent(renderer()->node(), DocumentEventQueue::ScrollEventElementTarget);
}

void RenderLayer::scrollRectToVisible(const LayoutRect& rect, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    RenderLayer* parentLayer = 0;
    LayoutRect newRect = rect;

    // We may end up propagating a scroll event. It is important that we suspend events until 
    // the end of the function since they could delete the layer or the layer's renderer().
    FrameView* frameView = renderer()->document()->view();
    if (frameView)
        frameView->pauseScheduledEvents();

    bool restrictedByLineClamp = false;
    if (renderer()->parent()) {
        parentLayer = renderer()->parent()->enclosingLayer();
        restrictedByLineClamp = !renderer()->parent()->style()->lineClamp().isNone();
    }

    if (renderer()->hasOverflowClip() && !restrictedByLineClamp) {
        // Don't scroll to reveal an overflow layer that is restricted by the -webkit-line-clamp property.
        // This will prevent us from revealing text hidden by the slider in Safari RSS.
        RenderBox* box = renderBox();
        ASSERT(box);
        FloatPoint absPos = box->localToAbsolute();
        absPos.move(box->borderLeft(), box->borderTop());

        LayoutRect layerBounds = LayoutRect(absPos.x() + scrollXOffset(), absPos.y() + scrollYOffset(), box->clientWidth(), box->clientHeight());
        LayoutRect exposeRect = LayoutRect(rect.x() + scrollXOffset(), rect.y() + scrollYOffset(), rect.width(), rect.height());
        LayoutRect r = getRectToExpose(layerBounds, exposeRect, alignX, alignY);
        
        int roundedAdjustedX = roundToInt(r.x() - absPos.x());
        int roundedAdjustedY = roundToInt(r.y() - absPos.y());
        IntSize clampedScrollOffset = clampScrollOffset(IntSize(roundedAdjustedX, roundedAdjustedY));
        if (clampedScrollOffset != scrollOffset()) {
            IntSize oldScrollOffset = scrollOffset();
            scrollToOffset(clampedScrollOffset);
            IntSize scrollOffsetDifference = scrollOffset() - oldScrollOffset;
            newRect.move(-scrollOffsetDifference);
        }
    } else if (!parentLayer && renderer()->isBox() && renderBox()->canBeProgramaticallyScrolled()) {
        if (frameView) {
            Element* ownerElement = 0;
            if (renderer()->document())
                ownerElement = renderer()->document()->ownerElement();

            if (ownerElement && ownerElement->renderer()) {
                HTMLFrameElement* frameElement = 0;

                if (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag))
                    frameElement = static_cast<HTMLFrameElement*>(ownerElement);

                if (frameElement && frameElement->scrollingMode() != ScrollbarAlwaysOff) {
                    LayoutRect viewRect = frameView->visibleContentRect();
                    LayoutRect exposeRect = getRectToExpose(viewRect, rect, alignX, alignY);

                    int xOffset = roundToInt(exposeRect.x());
                    int yOffset = roundToInt(exposeRect.y());
                    // Adjust offsets if they're outside of the allowable range.
                    xOffset = max(0, min(frameView->contentsWidth(), xOffset));
                    yOffset = max(0, min(frameView->contentsHeight(), yOffset));

                    frameView->setScrollPosition(IntPoint(xOffset, yOffset));
                    if (frameView->safeToPropagateScrollToParent()) {
                        parentLayer = ownerElement->renderer()->enclosingLayer();
                        newRect.setX(rect.x() - frameView->scrollX() + frameView->x());
                        newRect.setY(rect.y() - frameView->scrollY() + frameView->y());
                    } else
                        parentLayer = 0;
                }
            } else {
                LayoutRect viewRect = frameView->visibleContentRect();
                LayoutRect r = getRectToExpose(viewRect, rect, alignX, alignY);
                
                frameView->setScrollPosition(roundedIntPoint(r.location()));

                // This is the outermost view of a web page, so after scrolling this view we
                // scroll its container by calling Page::scrollRectIntoView.
                // This only has an effect on the Mac platform in applications
                // that put web views into scrolling containers, such as Mac OS X Mail.
                // The canAutoscroll function in EventHandler also knows about this.
                if (Frame* frame = frameView->frame()) {
                    if (Page* page = frame->page())
                        page->chrome()->scrollRectIntoView(pixelSnappedIntRect(rect));
                }
            }
        }
    }
    
    if (parentLayer)
        parentLayer->scrollRectToVisible(newRect, alignX, alignY);

    if (frameView)
        frameView->resumeScheduledEvents();
}

void RenderLayer::updateCompositingLayersAfterScroll()
{
#if USE(ACCELERATED_COMPOSITING)
    if (compositor()->inCompositingMode()) {
        // Our stacking context is guaranteed to contain all of our descendants that may need
        // repositioning, so update compositing layers from there.
        if (RenderLayer* compositingAncestor = stackingContext()->enclosingCompositingLayer()) {
            if (compositor()->compositingConsultsOverlap())
                compositor()->updateCompositingLayers(CompositingUpdateOnScroll, compositingAncestor);
            else {
                bool isUpdateRoot = true;
                compositingAncestor->backing()->updateAfterLayout(RenderLayerBacking::AllDescendants, isUpdateRoot);
            }
        }
    }
#endif
}

LayoutRect RenderLayer::getRectToExpose(const LayoutRect &visibleRect, const LayoutRect &exposeRect, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
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
    LayoutUnit intersectHeight = intersection(visibleRect, exposeRectY).height();
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

void RenderLayer::autoscroll()
{
    Frame* frame = renderer()->frame();
    if (!frame)
        return;

    FrameView* frameView = frame->view();
    if (!frameView)
        return;

#if ENABLE(DRAG_SUPPORT)
    frame->eventHandler()->updateSelectionForMouseDrag();
#endif

    IntPoint currentDocumentPosition = frameView->windowToContents(frame->eventHandler()->currentMousePosition());
    scrollRectToVisible(LayoutRect(currentDocumentPosition, LayoutSize(1, 1)), ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded);
}

void RenderLayer::resize(const PlatformMouseEvent& evt, const LayoutSize& oldOffset)
{
    // FIXME: This should be possible on generated content but is not right now.
    if (!inResizeMode() || !renderer()->hasOverflowClip() || !renderer()->node())
        return;

    ASSERT(renderer()->node()->isElementNode());
    Element* element = static_cast<Element*>(renderer()->node());
    RenderBox* renderer = toRenderBox(element->renderer());

    EResize resize = renderer->style()->resize();
    if (resize == RESIZE_NONE)
        return;

    Document* document = element->document();
    if (!document->frame()->eventHandler()->mousePressed())
        return;

    float zoomFactor = renderer->style()->effectiveZoom();

    LayoutSize newOffset = offsetFromResizeCorner(document->view()->windowToContents(evt.position()));
    newOffset.setWidth(newOffset.width() / zoomFactor);
    newOffset.setHeight(newOffset.height() / zoomFactor);
    
    LayoutSize currentSize = LayoutSize(renderer->width() / zoomFactor, renderer->height() / zoomFactor);
    LayoutSize minimumSize = element->minimumSizeForResizing().shrunkTo(currentSize);
    element->setMinimumSizeForResizing(minimumSize);
    
    LayoutSize adjustedOldOffset = LayoutSize(oldOffset.width() / zoomFactor, oldOffset.height() / zoomFactor);
    if (renderer->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        newOffset.setWidth(-newOffset.width());
        adjustedOldOffset.setWidth(-adjustedOldOffset.width());
    }
    
    LayoutSize difference = (currentSize + newOffset - adjustedOldOffset).expandedTo(minimumSize) - currentSize;

    ASSERT(element->isStyledElement());
    StyledElement* styledElement = static_cast<StyledElement*>(element);
    bool isBoxSizingBorder = renderer->style()->boxSizing() == BORDER_BOX;

    if (resize != RESIZE_VERTICAL && difference.width()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            styledElement->setInlineStyleProperty(CSSPropertyMarginLeft, String::number(renderer->marginLeft() / zoomFactor) + "px", false);
            styledElement->setInlineStyleProperty(CSSPropertyMarginRight, String::number(renderer->marginRight() / zoomFactor) + "px", false);
        }
        LayoutUnit baseWidth = renderer->width() - (isBoxSizingBorder ? ZERO_LAYOUT_UNIT : renderer->borderAndPaddingWidth());
        baseWidth = baseWidth / zoomFactor;
        styledElement->setInlineStyleProperty(CSSPropertyWidth, String::number(roundToInt(baseWidth + difference.width())) + "px", false);
    }

    if (resize != RESIZE_HORIZONTAL && difference.height()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            styledElement->setInlineStyleProperty(CSSPropertyMarginTop, String::number(renderer->marginTop() / zoomFactor) + "px", false);
            styledElement->setInlineStyleProperty(CSSPropertyMarginBottom, String::number(renderer->marginBottom() / zoomFactor) + "px", false);
        }
        LayoutUnit baseHeight = renderer->height() - (isBoxSizingBorder ? ZERO_LAYOUT_UNIT : renderer->borderAndPaddingHeight());
        baseHeight = baseHeight / zoomFactor;
        styledElement->setInlineStyleProperty(CSSPropertyHeight, String::number(roundToInt(baseHeight + difference.height())) + "px", false);
    }

    document->updateLayout();

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
    return scrollOrigin() + m_scrollOffset;
}

IntPoint RenderLayer::minimumScrollPosition() const
{
    return scrollOrigin();
}

IntPoint RenderLayer::maximumScrollPosition() const
{
    // FIXME: m_scrollSize may not be up-to-date if m_scrollDimensionsDirty is true.
    return scrollOrigin() + roundedIntSize(m_scrollSize) - visibleContentRect(true).size();
}

IntRect RenderLayer::visibleContentRect(bool includeScrollbars) const
{
    int verticalScrollbarWidth = 0;
    int horizontalScrollbarHeight = 0;
    if (includeScrollbars) {
        verticalScrollbarWidth = (verticalScrollbar() && !verticalScrollbar()->isOverlayScrollbar()) ? verticalScrollbar()->width() : 0;
        horizontalScrollbarHeight = (horizontalScrollbar() && !horizontalScrollbar()->isOverlayScrollbar()) ? horizontalScrollbar()->height() : 0;
    }
    
    return IntRect(IntPoint(scrollXOffset(), scrollYOffset()),
                   IntSize(max(0, m_layerSize.width() - verticalScrollbarWidth), 
                           max(0, m_layerSize.height() - horizontalScrollbarHeight)));
}

IntSize RenderLayer::overhangAmount() const
{
    return IntSize();
}

bool RenderLayer::isActive() const
{
    Page* page = renderer()->frame()->page();
    return page && page->focusController()->isActive();
}

static int cornerStart(const RenderLayer* layer, int minX, int maxX, int thickness)
{
    if (layer->renderer()->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + layer->renderer()->style()->borderLeftWidth();
    return maxX - thickness - layer->renderer()->style()->borderRightWidth();
}

static IntRect cornerRect(const RenderLayer* layer, const IntRect& bounds)
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
    return IntRect(cornerStart(layer, bounds.x(), bounds.maxX(), horizontalThickness),
                   bounds.maxY() - verticalThickness - layer->renderer()->style()->borderBottomWidth(),
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
    bool hasResizer = renderer()->style()->resize() != RESIZE_NONE;
    if ((hasHorizontalBar && hasVerticalBar) || (hasResizer && (hasHorizontalBar || hasVerticalBar)))
        return cornerRect(this, renderBox()->pixelSnappedBorderBoxRect());
    return IntRect();
}

static IntRect resizerCornerRect(const RenderLayer* layer, const IntRect& bounds)
{
    ASSERT(layer->renderer()->isBox());
    if (layer->renderer()->style()->resize() == RESIZE_NONE)
        return IntRect();
    return cornerRect(layer, bounds);
}

IntRect RenderLayer::scrollCornerAndResizerRect() const
{
    RenderBox* box = renderBox();
    if (!box)
        return IntRect();
    IntRect scrollCornerAndResizer = scrollCornerRect();
    if (scrollCornerAndResizer.isEmpty())
        scrollCornerAndResizer = resizerCornerRect(this, box->pixelSnappedBorderBoxRect());
    return scrollCornerAndResizer;
}

bool RenderLayer::isScrollCornerVisible() const
{
    ASSERT(renderer()->isBox());
    return !scrollCornerRect().isEmpty();
}

IntRect RenderLayer::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& scrollbarRect) const
{
    RenderView* view = renderer()->view();
    if (!view)
        return scrollbarRect;

    IntRect rect = scrollbarRect;
    rect.move(scrollbarOffset(scrollbar));

    return view->frameView()->convertFromRenderer(renderer(), rect);
}

IntRect RenderLayer::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& parentRect) const
{
    RenderView* view = renderer()->view();
    if (!view)
        return parentRect;

    IntRect rect = view->frameView()->convertToRenderer(renderer(), parentRect);
    rect.move(-scrollbarOffset(scrollbar));
    return rect;
}

IntPoint RenderLayer::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& scrollbarPoint) const
{
    RenderView* view = renderer()->view();
    if (!view)
        return scrollbarPoint;

    IntPoint point = scrollbarPoint;
    point.move(scrollbarOffset(scrollbar));
    return view->frameView()->convertFromRenderer(renderer(), point);
}

IntPoint RenderLayer::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& parentPoint) const
{
    RenderView* view = renderer()->view();
    if (!view)
        return parentPoint;

    IntPoint point = view->frameView()->convertToRenderer(renderer(), parentPoint);

    point.move(-scrollbarOffset(scrollbar));
    return point;
}

IntSize RenderLayer::contentsSize() const
{
    return IntSize(scrollWidth(), scrollHeight());
}

int RenderLayer::visibleHeight() const
{
    return m_layerSize.height();
}

int RenderLayer::visibleWidth() const
{
    return m_layerSize.width();
}

bool RenderLayer::shouldSuspendScrollAnimations() const
{
    RenderView* view = renderer()->view();
    if (!view)
        return true;
    return view->frameView()->shouldSuspendScrollAnimations();
}

bool RenderLayer::isOnActivePage() const
{
    return !m_renderer->document()->inPageCache();
}

IntPoint RenderLayer::currentMousePosition() const
{
    return renderer()->frame() ? renderer()->frame()->eventHandler()->currentMousePosition() : IntPoint();
}

LayoutUnit RenderLayer::verticalScrollbarStart(int minX, int maxX) const
{
    const RenderBox* box = renderBox();
    if (renderer()->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + box->borderLeft();
    return maxX - box->borderRight() - m_vBar->width();
}

LayoutUnit RenderLayer::horizontalScrollbarStart(int minX) const
{
    const RenderBox* box = renderBox();
    int x = minX + box->borderLeft();
    if (renderer()->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        x += m_vBar ? m_vBar->width() : resizerCornerRect(this, box->pixelSnappedBorderBoxRect()).width();
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
#if USE(ACCELERATED_COMPOSITING)
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
#endif
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
    renderer()->repaintRectangle(scrollRect);
}

void RenderLayer::invalidateScrollCornerRect(const IntRect& rect)
{
#if USE(ACCELERATED_COMPOSITING)
    if (GraphicsLayer* layer = layerForScrollCorner()) {
        layer->setNeedsDisplayInRect(rect);
        return;
    }
#endif
    if (m_scrollCorner)
        m_scrollCorner->repaintRectangle(rect);
    if (m_resizer)
        m_resizer->repaintRectangle(rect);
}

PassRefPtr<Scrollbar> RenderLayer::createScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar> widget;
    RenderObject* actualRenderer = renderer()->node() ? renderer()->node()->shadowAncestorNode()->renderer() : renderer();
    bool hasCustomScrollbarStyle = actualRenderer->isBox() && actualRenderer->style()->hasPseudoStyle(SCROLLBAR);
    if (hasCustomScrollbarStyle)
        widget = RenderScrollbar::createCustomScrollbar(this, orientation, actualRenderer->node());
    else {
        widget = Scrollbar::createNativeScrollbar(this, orientation, RegularScrollbar);
        if (orientation == HorizontalScrollbar)
            didAddHorizontalScrollbar(widget.get());
        else 
            didAddVerticalScrollbar(widget.get());
    }
    renderer()->document()->view()->addChild(widget.get());        
    return widget.release();
}

void RenderLayer::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_hBar : m_vBar;
    if (!scrollbar)
        return;

    if (!scrollbar->isCustomScrollbar()) {
        if (orientation == HorizontalScrollbar)
            willRemoveHorizontalScrollbar(scrollbar.get());
        else
            willRemoveVerticalScrollbar(scrollbar.get());
    }

    scrollbar->removeFromParent();
    scrollbar->disconnectFromScrollableArea();
    scrollbar = 0;
}

bool RenderLayer::scrollsOverflow() const
{
    if (!renderer()->isBox())
        return false;

    return toRenderBox(renderer())->scrollsOverflow();
}

bool RenderLayer::allowsScrolling() const
{
    return (m_hBar && m_hBar->enabled()) || (m_vBar && m_vBar->enabled());
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

#if ENABLE(DASHBOARD_SUPPORT) || ENABLE(WIDGET_REGION)
    // Force an update since we know the scrollbars have changed things.
    if (renderer()->document()->hasDashboardRegions())
        renderer()->document()->setDashboardRegionsDirty(true);
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

#if ENABLE(DASHBOARD_SUPPORT) || ENABLE(WIDGET_REGION)
    // Force an update since we know the scrollbars have changed things.
    if (renderer()->document()->hasDashboardRegions())
        renderer()->document()->setDashboardRegionsDirty(true);
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
    if (!m_vBar || (m_vBar->isOverlayScrollbar() && relevancy == IgnoreOverlayScrollbarSize))
        return 0;
    return m_vBar->width();
}

int RenderLayer::horizontalScrollbarHeight(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_hBar || (m_hBar->isOverlayScrollbar() && relevancy == IgnoreOverlayScrollbarSize))
        return 0;
    return m_hBar->height();
}

IntSize RenderLayer::offsetFromResizeCorner(const IntPoint& absolutePoint) const
{
    // Currently the resize corner is either the bottom right corner or the bottom left corner.
    // FIXME: This assumes the location is 0, 0. Is this guaranteed to always be the case?
    IntSize elementSize = size();
    if (renderer()->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        elementSize.setWidth(0);
    IntPoint resizerPoint = toPoint(elementSize);
    IntPoint localPoint = roundedIntPoint(absoluteToContents(absolutePoint));
    return localPoint - resizerPoint;
}

bool RenderLayer::hasOverflowControls() const
{
    return m_hBar || m_vBar || m_scrollCorner || renderer()->style()->resize() != RESIZE_NONE;
}

void RenderLayer::positionOverflowControls(const IntSize& offsetFromLayer)
{
    if (!m_hBar && !m_vBar && (!renderer()->hasOverflowClip() || renderer()->style()->resize() == RESIZE_NONE))
        return;
    
    RenderBox* box = renderBox();
    if (!box)
        return;

    const IntRect borderBox = box->pixelSnappedBorderBoxRect();
    const IntRect& scrollCorner = scrollCornerRect();
    IntRect absBounds(borderBox.location() + offsetFromLayer, borderBox.size());
    if (m_vBar)
        m_vBar->setFrameRect(IntRect(verticalScrollbarStart(absBounds.x(), absBounds.maxX()),
                                     absBounds.y() + box->borderTop(),
                                     m_vBar->width(),
                                     absBounds.height() - (box->borderTop() + box->borderBottom()) - scrollCorner.height()));

    if (m_hBar)
        m_hBar->setFrameRect(IntRect(horizontalScrollbarStart(absBounds.x()),
                                     absBounds.maxY() - box->borderBottom() - m_hBar->height(),
                                     absBounds.width() - (box->borderLeft() + box->borderRight()) - scrollCorner.width(),
                                     m_hBar->height()));

#if USE(ACCELERATED_COMPOSITING)
    if (GraphicsLayer* layer = layerForHorizontalScrollbar()) {
        if (m_hBar) {
            layer->setPosition(m_hBar->frameRect().location() - offsetFromLayer);
            layer->setSize(m_hBar->frameRect().size());
        }
        layer->setDrawsContent(m_hBar);
    }
    if (GraphicsLayer* layer = layerForVerticalScrollbar()) {
        if (m_vBar) {
            layer->setPosition(m_vBar->frameRect().location() - offsetFromLayer);
            layer->setSize(m_vBar->frameRect().size());
        }
        layer->setDrawsContent(m_vBar);
    }

    if (GraphicsLayer* layer = layerForScrollCorner()) {
        const LayoutRect& scrollCornerAndResizer = scrollCornerAndResizerRect();
        layer->setPosition(scrollCornerAndResizer.location());
        layer->setSize(scrollCornerAndResizer.size());
        layer->setDrawsContent(!scrollCornerAndResizer.isEmpty());
    }
#endif

    if (m_scrollCorner)
        m_scrollCorner->setFrameRect(scrollCorner);
    if (m_resizer)
        m_resizer->setFrameRect(resizerCornerRect(this, borderBox));
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
    if (box->style()->appearance() == ListboxPart)
        return;

    bool hasHorizontalOverflow = this->hasHorizontalOverflow();
    bool hasVerticalOverflow = this->hasVerticalOverflow();

    // overflow:scroll should just enable/disable.
    if (renderer()->style()->overflowX() == OSCROLL)
        m_hBar->setEnabled(hasHorizontalOverflow);
    if (renderer()->style()->overflowY() == OSCROLL)
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

#if ENABLE(DASHBOARD_SUPPORT) || ENABLE(WIDGET_REGION)
        // Force an update since we know the scrollbars have changed things.
        if (renderer()->document()->hasDashboardRegions())
            renderer()->document()->setDashboardRegionsDirty(true);
#endif

        renderer()->repaint();

        if (renderer()->style()->overflowX() == OAUTO || renderer()->style()->overflowY() == OAUTO) {
            if (!m_inOverflowRelayout) {
                // Our proprietary overflow: overlay value doesn't trigger a layout.
                m_inOverflowRelayout = true;
                renderer()->setNeedsLayout(true, MarkOnlyThis);
                if (renderer()->isRenderBlock()) {
                    RenderBlock* block = toRenderBlock(renderer());
                    block->scrollbarsChanged(autoHorizontalScrollBarChanged, autoVerticalScrollBarChanged);
                    block->layoutBlock(true);
                } else
                    renderer()->layout();
                m_inOverflowRelayout = false;
            }
        }
    }

    // Set up the range (and page step/line step).
    if (m_hBar) {
        int clientWidth = box->pixelSnappedClientWidth();
        int pageStep = max(max<int>(clientWidth * Scrollbar::minFractionToStepWhenPaging(), clientWidth - Scrollbar::maxOverlapBetweenPages()), 1);
        m_hBar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
        m_hBar->setProportion(clientWidth, m_scrollSize.width());
    }
    if (m_vBar) {
        int clientHeight = box->pixelSnappedClientHeight();
        int pageStep = max(max<int>(clientHeight * Scrollbar::minFractionToStepWhenPaging(), clientHeight - Scrollbar::maxOverlapBetweenPages()), 1);
        m_vBar->setSteps(Scrollbar::pixelsPerLineStep(), pageStep);
        m_vBar->setProportion(clientHeight, m_scrollSize.height());
    }

    updateScrollableAreaSet((hasHorizontalOverflow || hasVerticalOverflow) && scrollsOverflow());
}

void RenderLayer::updateScrollInfoAfterLayout()
{
    RenderBox* box = renderBox();
    if (!box)
        return;

    m_scrollDimensionsDirty = true;
    IntSize originalScrollOffset = scrollOffset();

    computeScrollDimensions();

    if (box->style()->overflowX() != OMARQUEE) {
        // Layout may cause us to be at an invalid scroll position. In this case we need
        // to pull our scroll offsets back to the max (or push them up to the min).
        IntSize clampedScrollOffset = clampScrollOffset(scrollOffset());
        if (clampedScrollOffset != scrollOffset())
            scrollToOffset(clampedScrollOffset);
    }

    updateScrollbarsAfterLayout();

    if (originalScrollOffset != scrollOffset())
        scrollToOffsetWithoutAnimation(toPoint(scrollOffset()));
}

void RenderLayer::paintOverflowControls(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect, bool paintingOverlayControls)
{
    // Don't do anything if we have no overflow.
    if (!renderer()->hasOverflowClip())
        return;

    // Overlay scrollbars paint in a second pass through the layer tree so that they will paint
    // on top of everything else. If this is the normal painting pass, paintingOverlayControls
    // will be false, and we should just tell the root layer that there are overlay scrollbars
    // that need to be painted. That will cause the second pass through the layer tree to run,
    // and we'll paint the scrollbars then. In the meantime, cache tx and ty so that the 
    // second pass doesn't need to re-enter the RenderTree to get it right.
    if (hasOverlayScrollbars() && !paintingOverlayControls) {
        m_cachedOverlayScrollbarOffset = paintOffset;
#if USE(ACCELERATED_COMPOSITING)
        // It's not necessary to do the second pass if the scrollbars paint into layers.
        if ((m_hBar && layerForHorizontalScrollbar()) || (m_vBar && layerForVerticalScrollbar()))
            return;
#endif
        RenderView* renderView = renderer()->view();
        renderView->layer()->setContainsDirtyOverlayScrollbars(true);
        renderView->frameView()->setContainsScrollableAreaWithOverlayScrollbars(true);
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
    positionOverflowControls(toSize(adjustedPaintOffset));

    // Now that we're sure the scrollbars are in the right place, paint them.
    if (m_hBar
#if USE(ACCELERATED_COMPOSITING)
        && !layerForHorizontalScrollbar()
#endif
              )
        m_hBar->paint(context, damageRect);
    if (m_vBar
#if USE(ACCELERATED_COMPOSITING)
        && !layerForVerticalScrollbar()
#endif
              )
        m_vBar->paint(context, damageRect);

#if USE(ACCELERATED_COMPOSITING)
    if (layerForScrollCorner())
        return;
#endif

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
        context->fillRect(absRect, Color::white, box->style()->colorSpace());
}

void RenderLayer::drawPlatformResizerImage(GraphicsContext* context, IntRect resizerCornerRect)
{
    float deviceScaleFactor = WebCore::deviceScaleFactor(renderer()->frame());

    RefPtr<Image> resizeCornerImage;
    IntSize cornerResizerSize;
    if (deviceScaleFactor >= 2) {
        DEFINE_STATIC_LOCAL(Image*, resizeCornerImageHiRes, (Image::loadPlatformResource("textAreaResizeCorner@2x").leakRef()));
        resizeCornerImage = resizeCornerImageHiRes;
        cornerResizerSize = resizeCornerImage->size();
        cornerResizerSize.scale(0.5f);
    } else {
        DEFINE_STATIC_LOCAL(Image*, resizeCornerImageLoRes, (Image::loadPlatformResource("textAreaResizeCorner").leakRef()));
        resizeCornerImage = resizeCornerImageLoRes;
        cornerResizerSize = resizeCornerImage->size();
    }

    if (renderer()->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        context->save();
        context->translate(resizerCornerRect.x() + cornerResizerSize.width(), resizerCornerRect.y() + resizerCornerRect.height() - cornerResizerSize.height());
        context->scale(FloatSize(-1.0, 1.0));
        context->drawImage(resizeCornerImage.get(), renderer()->style()->colorSpace(), IntRect(IntPoint(), cornerResizerSize));
        context->restore();
        return;
    }
    IntRect imageRect(resizerCornerRect.maxXMaxYCorner() - cornerResizerSize, cornerResizerSize);
    context->drawImage(resizeCornerImage.get(), renderer()->style()->colorSpace(), imageRect);
}

void RenderLayer::paintResizer(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    if (renderer()->style()->resize() == RESIZE_NONE)
        return;

    RenderBox* box = renderBox();
    ASSERT(box);

    IntRect absRect = resizerCornerRect(this, box->pixelSnappedBorderBoxRect());
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
        IntRect largerCorner = absRect;
        largerCorner.setSize(IntSize(largerCorner.width() + 1, largerCorner.height() + 1));
        context->setStrokeColor(Color(makeRGB(217, 217, 217)), ColorSpaceDeviceRGB);
        context->setStrokeThickness(1.0f);
        context->setFillColor(Color::transparent, ColorSpaceDeviceRGB);
        context->drawRect(largerCorner);
    }
}

bool RenderLayer::isPointInResizeControl(const IntPoint& absolutePoint) const
{
    if (!renderer()->hasOverflowClip() || renderer()->style()->resize() == RESIZE_NONE)
        return false;
    
    RenderBox* box = renderBox();
    ASSERT(box);

    IntPoint localPoint = roundedIntPoint(absoluteToContents(absolutePoint));

    IntRect localBounds(0, 0, box->pixelSnappedWidth(), box->pixelSnappedHeight());
    return resizerCornerRect(this, localBounds).contains(localPoint);
}
    
bool RenderLayer::hitTestOverflowControls(HitTestResult& result, const IntPoint& localPoint)
{
    if (!m_hBar && !m_vBar && (!renderer()->hasOverflowClip() || renderer()->style()->resize() == RESIZE_NONE))
        return false;

    RenderBox* box = renderBox();
    ASSERT(box);
    
    IntRect resizeControlRect;
    if (renderer()->style()->resize() != RESIZE_NONE) {
        resizeControlRect = resizerCornerRect(this, box->pixelSnappedBorderBoxRect());
        if (resizeControlRect.contains(localPoint))
            return true;
    }

    int resizeControlSize = max(resizeControlRect.height(), 0);

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

    resizeControlSize = max(resizeControlRect.width(), 0);
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

void RenderLayer::paint(GraphicsContext* context, const LayoutRect& damageRect, PaintBehavior paintBehavior, RenderObject* paintingRoot, RenderRegion* region, PaintLayerFlags paintFlags)
{
    OverlapTestRequestMap overlapTestRequests;
    paintLayer(this, context, damageRect, paintBehavior, paintingRoot, region, &overlapTestRequests, paintFlags);
    OverlapTestRequestMap::iterator end = overlapTestRequests.end();
    for (OverlapTestRequestMap::iterator it = overlapTestRequests.begin(); it != end; ++it)
        it->first->setOverlapTestResult(false);
}

void RenderLayer::paintOverlayScrollbars(GraphicsContext* context, const LayoutRect& damageRect, PaintBehavior paintBehavior, RenderObject* paintingRoot)
{
    if (!m_containsDirtyOverlayScrollbars)
        return;
    paintLayer(this, context, damageRect, paintBehavior, paintingRoot, 0, 0, PaintLayerHaveTransparency | PaintLayerTemporaryClipRects 
               | PaintLayerPaintingOverlayScrollbars);
    m_containsDirtyOverlayScrollbars = false;
}

#ifndef DISABLE_ROUNDED_CORNER_CLIPPING
static bool inContainingBlockChain(RenderLayer* startLayer, RenderLayer* endLayer)
{
    if (startLayer == endLayer)
        return true;
    
    RenderView* view = startLayer->renderer()->view();
    for (RenderBlock* currentBlock = startLayer->renderer()->containingBlock(); currentBlock && currentBlock != view; currentBlock = currentBlock->containingBlock()) {
        if (currentBlock->layer() == endLayer)
            return true;
    }
    
    return false;
}
#endif

void RenderLayer::clipToRect(RenderLayer* rootLayer, GraphicsContext* context, const LayoutRect& paintDirtyRect, const ClipRect& clipRect,
                             BorderRadiusClippingRule rule)
{
    if (clipRect.rect() == paintDirtyRect)
        return;
    context->save();
    context->clip(pixelSnappedIntRect(clipRect.rect()));
    
    if (!clipRect.hasRadius())
        return;

#ifndef DISABLE_ROUNDED_CORNER_CLIPPING
    // If the clip rect has been tainted by a border radius, then we have to walk up our layer chain applying the clips from
    // any layers with overflow. The condition for being able to apply these clips is that the overflow object be in our
    // containing block chain so we check that also.
    for (RenderLayer* layer = rule == IncludeSelfForBorderRadius ? this : parent(); layer; layer = layer->parent()) {
        if (layer->renderer()->hasOverflowClip() && layer->renderer()->style()->hasBorderRadius() && inContainingBlockChain(this, layer)) {
                LayoutPoint delta;
                layer->convertToLayerCoords(rootLayer, delta);
                context->addRoundedRectClip(layer->renderer()->style()->getRoundedInnerBorderFor(LayoutRect(delta, layer->size())));
        }

        if (layer == rootLayer)
            break;
    }
#endif
}

void RenderLayer::restoreClip(GraphicsContext* context, const LayoutRect& paintDirtyRect, const ClipRect& clipRect)
{
    if (clipRect.rect() == paintDirtyRect)
        return;
    context->restore();
}

static void performOverlapTests(OverlapTestRequestMap& overlapTestRequests, const RenderLayer* rootLayer, const RenderLayer* layer)
{
    Vector<OverlapTestRequestClient*> overlappedRequestClients;
    OverlapTestRequestMap::iterator end = overlapTestRequests.end();
    LayoutRect boundingBox = layer->boundingBox(rootLayer);
    for (OverlapTestRequestMap::iterator it = overlapTestRequests.begin(); it != end; ++it) {
        if (!boundingBox.intersects(it->second))
            continue;

        it->first->setOverlapTestResult(true);
        overlappedRequestClients.append(it->first);
    }
    for (size_t i = 0; i < overlappedRequestClients.size(); ++i)
        overlapTestRequests.remove(overlappedRequestClients[i]);
}

#if USE(ACCELERATED_COMPOSITING)
static bool shouldDoSoftwarePaint(const RenderLayer* layer, bool paintingReflection)
{
    return paintingReflection && !layer->has3DTransform();
}
#endif
    
static inline bool shouldSuppressPaintingLayer(RenderLayer* layer)
{
    // Avoid painting descendants of the root layer when stylesheets haven't loaded. This eliminates FOUC.
    // It's ok not to draw, because later on, when all the stylesheets do load, updateStyleSelector on the Document
    // will do a full repaint().
    if (layer->renderer()->document()->didLayoutWithPendingStylesheets() && !layer->isRootLayer() && !layer->renderer()->isRoot())
        return true;

    // Avoid painting all layers if the document is in a state where visual updates aren't allowed.
    // A full repaint will occur in Document::implicitClose() if painting is suppressed here.
    if (!layer->renderer()->document()->visualUpdatesAllowed())
        return true;

    return false;
}


void RenderLayer::paintLayer(RenderLayer* rootLayer, GraphicsContext* context,
                        const LayoutRect& paintDirtyRect, PaintBehavior paintBehavior,
                        RenderObject* paintingRoot, RenderRegion* region, OverlapTestRequestMap* overlapTestRequests,
                        PaintLayerFlags paintFlags)
{
#if USE(ACCELERATED_COMPOSITING)
    if (isComposited()) {
        // The updatingControlTints() painting pass goes through compositing layers,
        // but we need to ensure that we don't cache clip rects computed with the wrong root in this case.
        if (context->updatingControlTints() || (paintBehavior & PaintBehaviorFlattenCompositingLayers))
            paintFlags |= PaintLayerTemporaryClipRects;
        else if (!backing()->paintsIntoWindow() && !backing()->paintsIntoCompositedAncestor() && !shouldDoSoftwarePaint(this, paintFlags & PaintLayerPaintingReflection) && !(rootLayer->containsDirtyOverlayScrollbars() && (paintFlags & PaintLayerPaintingOverlayScrollbars))) {
            // If this RenderLayer should paint into its backing, that will be done via RenderLayerBacking::paintIntoLayer().
            return;
        }
    }
#endif

    // Non self-painting leaf layers don't need to be painted as their renderer() should properly paint itself.
    if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
        return;

    if (shouldSuppressPaintingLayer(this))
        return;
    
    // If this layer is totally invisible then there is nothing to paint.
    if (!renderer()->opacity())
        return;

    if (paintsWithTransparency(paintBehavior))
        paintFlags |= PaintLayerHaveTransparency;

    // PaintLayerAppliedTransform is used in RenderReplica, to avoid applying the transform twice.
    if (paintsWithTransform(paintBehavior) && !(paintFlags & PaintLayerAppliedTransform)) {
        TransformationMatrix layerTransform = renderableTransform(paintBehavior);
        // If the transform can't be inverted, then don't paint anything.
        if (!layerTransform.isInvertible())
            return;

        // If we have a transparency layer enclosing us and we are the root of a transform, then we need to establish the transparency
        // layer from the parent now, assuming there is a parent
        if (paintFlags & PaintLayerHaveTransparency) {
            if (parent())
                parent()->beginTransparencyLayers(context, rootLayer, paintDirtyRect, paintBehavior);
            else
                beginTransparencyLayers(context, rootLayer, paintDirtyRect, paintBehavior);
        }

        // Make sure the parent's clip rects have been calculated.
        ClipRect clipRect = paintDirtyRect;
        if (parent()) {
            clipRect = backgroundClipRect(rootLayer, region, (paintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects);
            clipRect.intersect(paintDirtyRect);
        
            // Push the parent coordinate space's clip.
            parent()->clipToRect(rootLayer, context, paintDirtyRect, clipRect);
        }

        // Adjust the transform such that the renderer's upper left corner will paint at (0,0) in user space.
        // This involves subtracting out the position of the layer in our current coordinate space.
        LayoutPoint delta;
        convertToLayerCoords(rootLayer, delta);
        TransformationMatrix transform(layerTransform);
        transform.translateRight(delta.x(), delta.y());
        
        // Apply the transform.
        {
            GraphicsContextStateSaver stateSaver(*context);
            context->concatCTM(transform.toAffineTransform());

            // Now do a paint with the root layer shifted to be us.
            paintLayerContentsAndReflection(this, context, transform.inverse().mapRect(paintDirtyRect), paintBehavior, paintingRoot, region, overlapTestRequests, paintFlags);
        }        

        // Restore the clip.
        if (parent())
            parent()->restoreClip(context, paintDirtyRect, clipRect);

        return;
    }
    
    paintLayerContentsAndReflection(rootLayer, context, paintDirtyRect, paintBehavior, paintingRoot, region, overlapTestRequests, paintFlags);
}

void RenderLayer::paintLayerContentsAndReflection(RenderLayer* rootLayer, GraphicsContext* context,
                        const LayoutRect& paintDirtyRect, PaintBehavior paintBehavior,
                        RenderObject* paintingRoot, RenderRegion* region, OverlapTestRequestMap* overlapTestRequests,
                        PaintLayerFlags paintFlags)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    PaintLayerFlags localPaintFlags = paintFlags & ~(PaintLayerAppliedTransform);

    // Paint the reflection first if we have one.
    if (m_reflection && !m_paintingInsideReflection) {
        // Mark that we are now inside replica painting.
        m_paintingInsideReflection = true;
        reflectionLayer()->paintLayer(rootLayer, context, paintDirtyRect, paintBehavior, paintingRoot, region, overlapTestRequests, localPaintFlags | PaintLayerPaintingReflection);
        m_paintingInsideReflection = false;
    }

    localPaintFlags |= PaintLayerPaintingCompositingAllPhases;
    paintLayerContents(rootLayer, context, paintDirtyRect, paintBehavior, paintingRoot, region, overlapTestRequests, localPaintFlags);
}

void RenderLayer::paintLayerContents(RenderLayer* rootLayer, GraphicsContext* context, 
                        const LayoutRect& parentPaintDirtyRect, PaintBehavior paintBehavior,
                        RenderObject* paintingRoot, RenderRegion* region, OverlapTestRequestMap* overlapTestRequests,
                        PaintLayerFlags paintFlags)
{
    ASSERT(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

    PaintLayerFlags localPaintFlags = paintFlags & ~(PaintLayerAppliedTransform);
    bool haveTransparency = localPaintFlags & PaintLayerHaveTransparency;
    bool isSelfPaintingLayer = this->isSelfPaintingLayer();
    bool isPaintingOverlayScrollbars = paintFlags & PaintLayerPaintingOverlayScrollbars;
    // Outline always needs to be painted even if we have no visible content.
    bool shouldPaintOutline = isSelfPaintingLayer && !isPaintingOverlayScrollbars;
    bool shouldPaintContent = m_hasVisibleContent && isSelfPaintingLayer && !isPaintingOverlayScrollbars;

    // Calculate the clip rects we should use only when we need them.
    LayoutRect layerBounds;
    ClipRect damageRect, clipRectToApply, outlineRect;
    LayoutPoint paintOffset;
    LayoutRect paintDirtyRect = parentPaintDirtyRect;
    
    bool useClipRect = true;
    GraphicsContext* transparencyLayerContext = context;
    
    // Ensure our lists are up-to-date.
    updateLayerListsIfNeeded();

#if ENABLE(CSS_FILTERS)
    FilterEffectRendererHelper filterPainter(filterRenderer() && paintsWithFilters());
    if (filterPainter.haveFilterEffect() && !context->paintingDisabled()) {
        LayoutPoint rootLayerOffset;
        convertToLayerCoords(rootLayer, rootLayerOffset);
        RenderLayerFilterInfo* filterInfo = this->filterInfo();
        ASSERT(filterInfo);
        LayoutRect filterRepaintRect = filterInfo->dirtySourceRect();
        filterRepaintRect.move(rootLayerOffset.x(), rootLayerOffset.y());
        if (filterPainter.prepareFilterEffect(this, calculateLayerBounds(this, rootLayer, 0), parentPaintDirtyRect, filterRepaintRect)) {
            // Now we know for sure, that the source image will be updated, so we can revert our tracking repaint rect back to zero.
            filterInfo->resetDirtySourceRect();

            // Rewire the old context to a memory buffer, so that we can capture the contents of the layer.
            // NOTE: We saved the old context in the "transparencyLayerContext" local variable, to be able to start a transparency layer
            // on the original context and avoid duplicating "beginFilterEffect" after each transpareny layer call. Also, note that 
            // beginTransparencyLayers will only create a single lazy transparency layer, even though it is called twice in this method.
            context = filterPainter.beginFilterEffect(context);

            // Check that we didn't fail to allocate the graphics context for the offscreen buffer.
            if (filterPainter.hasStartedFilterEffect()) {
                paintDirtyRect = filterPainter.repaintRect();
                // If the filter needs the full source image, we need to avoid using the clip rectangles.
                // Otherwise, if for example this layer has overflow:hidden, a drop shadow will not compute correctly.
                // Note that we will still apply the clipping on the final rendering of the filter.
                useClipRect = !filterRenderer()->hasFilterThatMovesPixels();
            }
        }
    }
#endif
    
    if (shouldPaintContent || shouldPaintOutline || isPaintingOverlayScrollbars) {
        calculateRects(rootLayer, region, (localPaintFlags & PaintLayerTemporaryClipRects) ? TemporaryClipRects : PaintingClipRects, paintDirtyRect, layerBounds, damageRect, clipRectToApply, outlineRect);
        paintOffset = toPoint(layerBounds.location() - renderBoxLocation());
    }

    bool forceBlackText = paintBehavior & PaintBehaviorForceBlackText;
    bool selectionOnly  = paintBehavior & PaintBehaviorSelectionOnly;
    
    // If this layer's renderer is a child of the paintingRoot, we render unconditionally, which
    // is done by passing a nil paintingRoot down to our renderer (as if no paintingRoot was ever set).
    // Else, our renderer tree may or may not contain the painting root, so we pass that root along
    // so it will be tested against as we descend through the renderers.
    RenderObject* paintingRootForRenderer = 0;
    if (paintingRoot && !renderer()->isDescendantOf(paintingRoot))
        paintingRootForRenderer = paintingRoot;

    if (overlapTestRequests && isSelfPaintingLayer)
        performOverlapTests(*overlapTestRequests, rootLayer, this);

    // We want to paint our layer, but only if we intersect the damage rect.
    shouldPaintContent &= intersectsDamageRect(layerBounds, damageRect.rect(), rootLayer);
    
    if (localPaintFlags & PaintLayerPaintingCompositingBackgroundPhase) {
        if (shouldPaintContent && !selectionOnly) {
            // Begin transparency layers lazily now that we know we have to paint something.
            if (haveTransparency)
                beginTransparencyLayers(transparencyLayerContext, rootLayer, paintDirtyRect, paintBehavior);
        
            if (useClipRect) {
                // Paint our background first, before painting any child layers.
                // Establish the clip used to paint our background.
                clipToRect(rootLayer, context, paintDirtyRect, damageRect, DoNotIncludeSelfForBorderRadius); // Background painting will handle clipping to self.
            }
            
            // Paint the background.
            PaintInfo paintInfo(context, pixelSnappedIntRect(damageRect.rect()), PaintPhaseBlockBackground, false, paintingRootForRenderer, region, 0);
            renderer()->paint(paintInfo, paintOffset);

            if (useClipRect) {
                // Restore the clip.
                restoreClip(context, paintDirtyRect, damageRect);
            }
        }

        // Now walk the sorted list of children with negative z-indices.
        paintList(negZOrderList(), rootLayer, context, paintDirtyRect, paintBehavior, paintingRoot, region, overlapTestRequests, localPaintFlags);
    }
    
    if (localPaintFlags & PaintLayerPaintingCompositingForegroundPhase) {
        // Now establish the appropriate clip and paint our child RenderObjects.
        if (shouldPaintContent && !clipRectToApply.isEmpty()) {
            // Begin transparency layers lazily now that we know we have to paint something.
            if (haveTransparency)
                beginTransparencyLayers(transparencyLayerContext, rootLayer, paintDirtyRect, paintBehavior);

            if (useClipRect) {
                // Set up the clip used when painting our children.
                clipToRect(rootLayer, context, paintDirtyRect, clipRectToApply);
            }
            
            PaintInfo paintInfo(context, pixelSnappedIntRect(clipRectToApply.rect()),
                                selectionOnly ? PaintPhaseSelection : PaintPhaseChildBlockBackgrounds,
                                forceBlackText, paintingRootForRenderer, region, 0);
            renderer()->paint(paintInfo, paintOffset);
            if (!selectionOnly) {
                paintInfo.phase = PaintPhaseFloat;
                renderer()->paint(paintInfo, paintOffset);
                paintInfo.phase = PaintPhaseForeground;
                paintInfo.overlapTestRequests = overlapTestRequests;
                renderer()->paint(paintInfo, paintOffset);
                paintInfo.phase = PaintPhaseChildOutlines;
                renderer()->paint(paintInfo, paintOffset);
            }

            if (useClipRect) {
                // Now restore our clip.
                restoreClip(context, paintDirtyRect, clipRectToApply);
            }
        }

        if (shouldPaintOutline && !outlineRect.isEmpty()) {
            // Paint our own outline
            PaintInfo paintInfo(context, pixelSnappedIntRect(outlineRect.rect()), PaintPhaseSelfOutline, false, paintingRootForRenderer, region, 0);
            clipToRect(rootLayer, context, paintDirtyRect, outlineRect, DoNotIncludeSelfForBorderRadius);
            renderer()->paint(paintInfo, paintOffset);
            restoreClip(context, paintDirtyRect, outlineRect);
        }
    
        // Paint any child layers that have overflow.
        paintList(m_normalFlowList, rootLayer, context, paintDirtyRect, paintBehavior, paintingRoot, region, overlapTestRequests, localPaintFlags);
    
        // Now walk the sorted list of children with positive z-indices.
        paintList(posZOrderList(), rootLayer, context, paintDirtyRect, paintBehavior, paintingRoot, region, overlapTestRequests, localPaintFlags);
    }
    
    if ((localPaintFlags & PaintLayerPaintingCompositingMaskPhase) && shouldPaintContent && renderer()->hasMask() && !selectionOnly) {
        if (useClipRect)
            clipToRect(rootLayer, context, paintDirtyRect, damageRect, DoNotIncludeSelfForBorderRadius); // Mask painting will handle clipping to self.

        // Paint the mask.
        PaintInfo paintInfo(context, pixelSnappedIntRect(damageRect.rect()), PaintPhaseMask, false, paintingRootForRenderer, region, 0);
        renderer()->paint(paintInfo, paintOffset);
        
        if (useClipRect) {
            // Restore the clip.
            restoreClip(context, paintDirtyRect, damageRect);
        }
    }

    if (isPaintingOverlayScrollbars) {
        clipToRect(rootLayer, context, paintDirtyRect, damageRect);
        paintOverflowControls(context, roundedIntPoint(paintOffset), pixelSnappedIntRect(damageRect.rect()), true);
        restoreClip(context, paintDirtyRect, damageRect);
    }

#if ENABLE(CSS_FILTERS)
    if (filterPainter.hasStartedFilterEffect()) {
        // Apply the correct clipping (ie. overflow: hidden).
        clipToRect(rootLayer, transparencyLayerContext, paintDirtyRect, damageRect);
        context = filterPainter.applyFilterEffect();
        restoreClip(transparencyLayerContext, paintDirtyRect, damageRect);
    }
#endif

    // Make sure that we now use the original transparency context.
    ASSERT(transparencyLayerContext == context);
    
    // End our transparency layer
    if (haveTransparency && m_usedTransparency && !m_paintingInsideReflection) {
        context->endTransparencyLayer();
        context->restore();
        m_usedTransparency = false;
    }
}

void RenderLayer::paintList(Vector<RenderLayer*>* list, RenderLayer* rootLayer, GraphicsContext* context,
                            const LayoutRect& paintDirtyRect, PaintBehavior paintBehavior,
                            RenderObject* paintingRoot, RenderRegion* region, OverlapTestRequestMap* overlapTestRequests,
                            PaintLayerFlags paintFlags)
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
        if (!childLayer->isPaginated())
            childLayer->paintLayer(rootLayer, context, paintDirtyRect, paintBehavior, paintingRoot, region, overlapTestRequests, paintFlags);
        else
            paintPaginatedChildLayer(childLayer, rootLayer, context, paintDirtyRect, paintBehavior, paintingRoot, region, overlapTestRequests, paintFlags);
    }
}

void RenderLayer::paintPaginatedChildLayer(RenderLayer* childLayer, RenderLayer* rootLayer, GraphicsContext* context,
                                           const LayoutRect& paintDirtyRect, PaintBehavior paintBehavior,
                                           RenderObject* paintingRoot, RenderRegion* region, OverlapTestRequestMap* overlapTestRequests,
                                           PaintLayerFlags paintFlags)
{
    // We need to do multiple passes, breaking up our child layer into strips.
    Vector<RenderLayer*> columnLayers;
    RenderLayer* ancestorLayer = isNormalFlowOnly() ? parent() : stackingContext();
    for (RenderLayer* curr = childLayer->parent(); curr; curr = curr->parent()) {
        if (curr->renderer()->hasColumns() && checkContainingBlockChainForPagination(childLayer->renderer(), curr->renderBox()))
            columnLayers.append(curr);
        if (curr == ancestorLayer)
            break;
    }

    // It is possible for paintLayer() to be called after the child layer ceases to be paginated but before
    // updateLayerPositions() is called and resets the isPaginated() flag, see <rdar://problem/10098679>.
    // If this is the case, just bail out, since the upcoming call to updateLayerPositions() will repaint the layer.
    if (!columnLayers.size())
        return;

    paintChildLayerIntoColumns(childLayer, rootLayer, context, paintDirtyRect, paintBehavior, paintingRoot, region, overlapTestRequests, paintFlags, columnLayers, columnLayers.size() - 1);
}

void RenderLayer::paintChildLayerIntoColumns(RenderLayer* childLayer, RenderLayer* rootLayer, GraphicsContext* context,
                                             const LayoutRect& paintDirtyRect, PaintBehavior paintBehavior,
                                             RenderObject* paintingRoot, RenderRegion* region, OverlapTestRequestMap* overlapTestRequests,
                                             PaintLayerFlags paintFlags, const Vector<RenderLayer*>& columnLayers, size_t colIndex)
{
    RenderBlock* columnBlock = toRenderBlock(columnLayers[colIndex]->renderer());

    ASSERT(columnBlock && columnBlock->hasColumns());
    if (!columnBlock || !columnBlock->hasColumns())
        return;
    
    LayoutPoint layerOffset;
    // FIXME: It looks suspicious to call convertToLayerCoords here
    // as canUseConvertToLayerCoords is true for this layer.
    columnBlock->layer()->convertToLayerCoords(rootLayer, layerOffset);
    
    bool isHorizontal = columnBlock->style()->isHorizontalWritingMode();

    ColumnInfo* colInfo = columnBlock->columnInfo();
    unsigned colCount = columnBlock->columnCount(colInfo);
    LayoutUnit currLogicalTopOffset = 0;
    for (unsigned i = 0; i < colCount; i++) {
        // For each rect, we clip to the rect, and then we adjust our coords.
        LayoutRect colRect = columnBlock->columnRectAt(colInfo, i);
        columnBlock->flipForWritingMode(colRect);
        LayoutUnit logicalLeftOffset = (isHorizontal ? colRect.x() : colRect.y()) - columnBlock->logicalLeftOffsetForContent();
        LayoutSize offset;
        if (isHorizontal) {
            if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                offset = LayoutSize(logicalLeftOffset, currLogicalTopOffset);
            else
                offset = LayoutSize(0, colRect.y() + currLogicalTopOffset - columnBlock->borderTop() - columnBlock->paddingTop());
        } else {
            if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                offset = LayoutSize(currLogicalTopOffset, logicalLeftOffset);
            else
                offset = LayoutSize(colRect.x() + currLogicalTopOffset - columnBlock->borderLeft() - columnBlock->paddingLeft(), 0);
        }

        colRect.moveBy(layerOffset);

        LayoutRect localDirtyRect(paintDirtyRect);
        localDirtyRect.intersect(colRect);
        
        if (!localDirtyRect.isEmpty()) {
            GraphicsContextStateSaver stateSaver(*context);
            
            // Each strip pushes a clip, since column boxes are specified as being
            // like overflow:hidden.
            context->clip(pixelSnappedIntRect(colRect));

            if (!colIndex) {
                // Apply a translation transform to change where the layer paints.
                TransformationMatrix oldTransform;
                bool oldHasTransform = childLayer->transform();
                if (oldHasTransform)
                    oldTransform = *childLayer->transform();
                TransformationMatrix newTransform(oldTransform);
                newTransform.translateRight(roundToInt(offset.width()), roundToInt(offset.height()));
                
                childLayer->m_transform = adoptPtr(new TransformationMatrix(newTransform));
                childLayer->paintLayer(rootLayer, context, localDirtyRect, paintBehavior, paintingRoot, region, overlapTestRequests, paintFlags);
                if (oldHasTransform)
                    childLayer->m_transform = adoptPtr(new TransformationMatrix(oldTransform));
                else
                    childLayer->m_transform.clear();
            } else {
                // Adjust the transform such that the renderer's upper left corner will paint at (0,0) in user space.
                // This involves subtracting out the position of the layer in our current coordinate space.
                LayoutPoint childOffset;
                columnLayers[colIndex - 1]->convertToLayerCoords(rootLayer, childOffset);
                TransformationMatrix transform;
                transform.translateRight(roundToInt(childOffset.x() + offset.width()), roundToInt(childOffset.y() + offset.height()));
                
                // Apply the transform.
                context->concatCTM(transform.toAffineTransform());

                // Now do a paint with the root layer shifted to be the next multicol block.
                paintChildLayerIntoColumns(childLayer, columnLayers[colIndex - 1], context, transform.inverse().mapRect(localDirtyRect), paintBehavior, 
                                           paintingRoot, region, overlapTestRequests, paintFlags, 
                                           columnLayers, colIndex - 1);
            }
        }

        // Move to the next position.
        LayoutUnit blockDelta = isHorizontal ? colRect.height() : colRect.width();
        if (columnBlock->style()->isFlippedBlocksWritingMode())
            currLogicalTopOffset += blockDelta;
        else
            currLogicalTopOffset -= blockDelta;
    }
}

static inline LayoutRect frameVisibleRect(RenderObject* renderer)
{
    FrameView* frameView = renderer->document()->view();
    if (!frameView)
        return LayoutRect();

    return frameView->visibleContentRect();
}

bool RenderLayer::hitTest(const HitTestRequest& request, HitTestResult& result)
{
    return hitTest(request, result.hitTestPoint(), result);
}

bool RenderLayer::hitTest(const HitTestRequest& request, const HitTestPoint& hitTestPoint, HitTestResult& result)
{
    renderer()->document()->updateLayout();
    
    LayoutRect hitTestArea = renderer()->isRenderFlowThread() ? toRenderFlowThread(renderer())->borderBoxRect() : renderer()->view()->documentRect();
    if (!request.ignoreClipping())
        hitTestArea.intersect(frameVisibleRect(renderer()));

    RenderLayer* insideLayer = hitTestLayer(this, 0, request, result, hitTestArea, hitTestPoint, false);
    if (!insideLayer) {
        // We didn't hit any layer. If we are the root layer and the mouse is -- or just was -- down, 
        // return ourselves. We do this so mouse events continue getting delivered after a drag has 
        // exited the WebView, and so hit testing over a scrollbar hits the content document.
        if ((request.active() || request.release()) && isRootLayer()) {
            renderer()->updateHitTestResult(result, toRenderView(renderer())->flipForWritingMode(result.point()));
            insideLayer = this;
        }
    }

    // Now determine if the result is inside an anchor - if the urlElement isn't already set.
    Node* node = result.innerNode();
    if (node && !result.URLElement())
        result.setURLElement(static_cast<Element*>(node->enclosingLinkEventParentOrSelf()));

    // Next set up the correct :hover/:active state along the new chain.
    updateHoverActiveState(request, result);
    
    // Now return whether we were inside this layer (this will always be true for the root
    // layer).
    return insideLayer;
}

Node* RenderLayer::enclosingElement() const
{
    for (RenderObject* r = renderer(); r; r = r->parent()) {
        if (Node* e = r->node())
            return e;
    }
    ASSERT_NOT_REACHED();
    return 0;
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
                                        const LayoutRect& hitTestRect, const HitTestPoint& hitTestPoint,
                                        const HitTestingTransformState* containerTransformState) const
{
    RefPtr<HitTestingTransformState> transformState;
    LayoutPoint offset;
    if (containerTransformState) {
        // If we're already computing transform state, then it's relative to the container (which we know is non-null).
        transformState = HitTestingTransformState::create(*containerTransformState);
        convertToLayerCoords(containerLayer, offset);
    } else {
        // If this is the first time we need to make transform state, then base it off of hitTestPoint,
        // which is relative to rootLayer.
        transformState = HitTestingTransformState::create(hitTestPoint.transformedPoint(), hitTestPoint.transformedRect(), FloatQuad(hitTestRect));
        convertToLayerCoords(rootLayer, offset);
    }
    
    RenderObject* containerRenderer = containerLayer ? containerLayer->renderer() : 0;
    if (renderer()->shouldUseTransformFromContainer(containerRenderer)) {
        TransformationMatrix containerTransform;
        renderer()->getTransformFromContainer(containerRenderer, toLayoutSize(offset), containerTransform);
        transformState->applyTransform(containerTransform, HitTestingTransformState::AccumulateTransform);
    } else {
        transformState->translate(offset.x(), offset.y(), HitTestingTransformState::AccumulateTransform);
    }
    
    return transformState;
}


static bool isHitCandidate(const RenderLayer* hitLayer, bool canDepthSort, double* zOffset, const HitTestingTransformState* transformState)
{
    if (!hitLayer)
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

// hitTestPoint and hitTestRect are relative to rootLayer.
// A 'flattening' layer is one preserves3D() == false.
// transformState.m_accumulatedTransform holds the transform from the containing flattening layer.
// transformState.m_lastPlanarPoint is the hitTestPoint in the plane of the containing flattening layer.
// transformState.m_lastPlanarQuad is the hitTestRect as a quad in the plane of the containing flattening layer.
// 
// If zOffset is non-null (which indicates that the caller wants z offset information), 
//  *zOffset on return is the z offset of the hit point relative to the containing flattening layer.
RenderLayer* RenderLayer::hitTestLayer(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
                                       const LayoutRect& hitTestRect, const HitTestPoint& hitTestPoint, bool appliedTransform,
                                       const HitTestingTransformState* transformState, double* zOffset)
{
    // The natural thing would be to keep HitTestingTransformState on the stack, but it's big, so we heap-allocate.

    bool useTemporaryClipRects = renderer()->view()->frameView()->containsScrollableAreaWithOverlayScrollbars();

    // Apply a transform if we have one.
    if (transform() && !appliedTransform) {
        // Make sure the parent's clip rects have been calculated.
        if (parent()) {
            ClipRect clipRect = backgroundClipRect(rootLayer, hitTestPoint.region(), useTemporaryClipRects ? TemporaryClipRects : RootRelativeClipRects, IncludeOverlayScrollbarSize);
            // Go ahead and test the enclosing clip now.
            if (!clipRect.intersects(hitTestPoint))
                return 0;
        }

        // Create a transform state to accumulate this transform.
        RefPtr<HitTestingTransformState> newTransformState = createLocalTransformState(rootLayer, containerLayer, hitTestRect, hitTestPoint, transformState);

        // If the transform can't be inverted, then don't hit test this layer at all.
        if (!newTransformState->m_accumulatedTransform.isInvertible())
            return 0;

        // Compute the point and the hit test rect in the coords of this layer by using the values
        // from the transformState, which store the point and quad in the coords of the last flattened
        // layer, and the accumulated transform which lets up map through preserve-3d layers.
        //
        // We can't just map hitTestPoint and hitTestRect because they may have been flattened (losing z)
        // by our container.
        FloatPoint localPoint = newTransformState->mappedPoint();
        FloatQuad localPointQuad = newTransformState->mappedQuad();
        LayoutRect localHitTestRect = newTransformState->boundsOfMappedArea();
        HitTestPoint newHitTestPoint;
        if (hitTestPoint.isRectBasedTest())
            newHitTestPoint = HitTestPoint(localPoint, localPointQuad);
        else
            newHitTestPoint = HitTestPoint(localPoint);

        // Now do a hit test with the root layer shifted to be us.
        return hitTestLayer(this, containerLayer, request, result, localHitTestRect, newHitTestPoint, true, newTransformState.get(), zOffset);
    }

    // Ensure our lists and 3d status are up-to-date.
    updateCompositingAndLayerListsIfNeeded();
    update3DTransformedDescendantStatus();

    RefPtr<HitTestingTransformState> localTransformState;
    if (appliedTransform) {
        // We computed the correct state in the caller (above code), so just reference it.
        ASSERT(transformState);
        localTransformState = const_cast<HitTestingTransformState*>(transformState);
    } else if (transformState || m_has3DTransformedDescendant || preserves3D()) {
        // We need transform state for the first time, or to offset the container state, so create it here.
        localTransformState = createLocalTransformState(rootLayer, containerLayer, hitTestRect, hitTestPoint, transformState);
    }

    // Check for hit test on backface if backface-visibility is 'hidden'
    if (localTransformState && renderer()->style()->backfaceVisibility() == BackfaceVisibilityHidden) {
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
    
    // Calculate the clip rects we should use.
    LayoutRect layerBounds;
    ClipRect bgRect;
    ClipRect fgRect;
    ClipRect outlineRect;
    calculateRects(rootLayer, hitTestPoint.region(), useTemporaryClipRects ? TemporaryClipRects : RootRelativeClipRects, hitTestRect, layerBounds, bgRect, fgRect, outlineRect, IncludeOverlayScrollbarSize);
    
    // The following are used for keeping track of the z-depth of the hit point of 3d-transformed
    // descendants.
    double localZOffset = -numeric_limits<double>::infinity();
    double* zOffsetForDescendantsPtr = 0;
    double* zOffsetForContentsPtr = 0;
    
    bool depthSortDescendants = false;
    if (preserves3D()) {
        depthSortDescendants = true;
        // Our layers can depth-test with our container, so share the z depth pointer with the container, if it passed one down.
        zOffsetForDescendantsPtr = zOffset ? zOffset : &localZOffset;
        zOffsetForContentsPtr = zOffset ? zOffset : &localZOffset;
    } else if (m_has3DTransformedDescendant) {
        // Flattening layer with 3d children; use a local zOffset pointer to depth-test children and foreground.
        depthSortDescendants = true;
        zOffsetForDescendantsPtr = zOffset ? zOffset : &localZOffset;
        zOffsetForContentsPtr = zOffset ? zOffset : &localZOffset;
    } else if (zOffset) {
        zOffsetForDescendantsPtr = 0;
        // Container needs us to give back a z offset for the hit layer.
        zOffsetForContentsPtr = zOffset;
    }

    // This variable tracks which layer the mouse ends up being inside.
    RenderLayer* candidateLayer = 0;

    // Begin by walking our list of positive layers from highest z-index down to the lowest z-index.
    RenderLayer* hitLayer = hitTestList(posZOrderList(), rootLayer, request, result, hitTestRect, hitTestPoint,
                                        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Now check our overflow objects.
    hitLayer = hitTestList(m_normalFlowList, rootLayer, request, result, hitTestRect, hitTestPoint,
                           localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // Next we want to see if the mouse pos is inside the child RenderObjects of the layer.
    if (fgRect.intersects(hitTestPoint) && isSelfPaintingLayer()) {
        // Hit test with a temporary HitTestResult, because we only want to commit to 'result' if we know we're frontmost.
        HitTestResult tempResult(result.hitTestPoint(), result.shadowContentFilterPolicy());
        if (hitTestContents(request, tempResult, layerBounds, hitTestPoint, HitTestDescendants) &&
            isHitCandidate(this, false, zOffsetForContentsPtr, unflattenedTransformState.get())) {
            if (result.isRectBasedTest())
                result.append(tempResult);
            else
                result = tempResult;
            if (!depthSortDescendants)
                return this;
            // Foreground can depth-sort with descendant layers, so keep this as a candidate.
            candidateLayer = this;
        } else if (result.isRectBasedTest())
            result.append(tempResult);
    }

    // Now check our negative z-index children.
    hitLayer = hitTestList(negZOrderList(), rootLayer, request, result, hitTestRect, hitTestPoint,
                                        localTransformState.get(), zOffsetForDescendantsPtr, zOffset, unflattenedTransformState.get(), depthSortDescendants);
    if (hitLayer) {
        if (!depthSortDescendants)
            return hitLayer;
        candidateLayer = hitLayer;
    }

    // If we found a layer, return. Child layers, and foreground always render in front of background.
    if (candidateLayer)
        return candidateLayer;

    if (bgRect.intersects(hitTestPoint) && isSelfPaintingLayer()) {
        HitTestResult tempResult(result.hitTestPoint(), result.shadowContentFilterPolicy());
        if (hitTestContents(request, tempResult, layerBounds, hitTestPoint, HitTestSelf) &&
            isHitCandidate(this, false, zOffsetForContentsPtr, unflattenedTransformState.get())) {
            if (result.isRectBasedTest())
                result.append(tempResult);
            else
                result = tempResult;
            return this;
        } else if (result.isRectBasedTest())
            result.append(tempResult);
    }

    return 0;
}

bool RenderLayer::hitTestContents(const HitTestRequest& request, HitTestResult& result, const LayoutRect& layerBounds, const HitTestPoint& hitTestPoint, HitTestFilter hitTestFilter) const
{
    if (!renderer()->hitTest(request, result, hitTestPoint,
                            toLayoutPoint(layerBounds.location() - renderBoxLocation()),
                            hitTestFilter)) {
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
        Node* e = enclosingElement();
        if (!result.innerNode())
            result.setInnerNode(e);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(e);
    }
        
    return true;
}

RenderLayer* RenderLayer::hitTestList(Vector<RenderLayer*>* list, RenderLayer* rootLayer,
                                      const HitTestRequest& request, HitTestResult& result,
                                      const LayoutRect& hitTestRect, const HitTestPoint& hitTestPoint,
                                      const HitTestingTransformState* transformState, 
                                      double* zOffsetForDescendants, double* zOffset,
                                      const HitTestingTransformState* unflattenedTransformState,
                                      bool depthSortDescendants)
{
    if (!list)
        return 0;
    
    RenderLayer* resultLayer = 0;
    for (int i = list->size() - 1; i >= 0; --i) {
        RenderLayer* childLayer = list->at(i);
        RenderLayer* hitLayer = 0;
        HitTestResult tempResult(result.hitTestPoint(), result.shadowContentFilterPolicy());
        if (childLayer->isPaginated())
            hitLayer = hitTestPaginatedChildLayer(childLayer, rootLayer, request, tempResult, hitTestRect, hitTestPoint, transformState, zOffsetForDescendants);
        else
            hitLayer = childLayer->hitTestLayer(rootLayer, this, request, tempResult, hitTestRect, hitTestPoint, false, transformState, zOffsetForDescendants);

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

RenderLayer* RenderLayer::hitTestPaginatedChildLayer(RenderLayer* childLayer, RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result,
                                                     const LayoutRect& hitTestRect, const HitTestPoint& hitTestPoint, const HitTestingTransformState* transformState, double* zOffset)
{
    Vector<RenderLayer*> columnLayers;
    RenderLayer* ancestorLayer = isNormalFlowOnly() ? parent() : stackingContext();
    for (RenderLayer* curr = childLayer->parent(); curr; curr = curr->parent()) {
        if (curr->renderer()->hasColumns() && checkContainingBlockChainForPagination(childLayer->renderer(), curr->renderBox()))
            columnLayers.append(curr);
        if (curr == ancestorLayer)
            break;
    }

    ASSERT(columnLayers.size());
    return hitTestChildLayerColumns(childLayer, rootLayer, request, result, hitTestRect, hitTestPoint, transformState, zOffset,
                                    columnLayers, columnLayers.size() - 1);
}

RenderLayer* RenderLayer::hitTestChildLayerColumns(RenderLayer* childLayer, RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result,
                                                   const LayoutRect& hitTestRect, const HitTestPoint& hitTestPoint, const HitTestingTransformState* transformState, double* zOffset,
                                                   const Vector<RenderLayer*>& columnLayers, size_t columnIndex)
{
    RenderBlock* columnBlock = toRenderBlock(columnLayers[columnIndex]->renderer());

    ASSERT(columnBlock && columnBlock->hasColumns());
    if (!columnBlock || !columnBlock->hasColumns())
        return 0;

    LayoutPoint layerOffset;
    columnBlock->layer()->convertToLayerCoords(rootLayer, layerOffset);

    ColumnInfo* colInfo = columnBlock->columnInfo();
    int colCount = columnBlock->columnCount(colInfo);

    // We have to go backwards from the last column to the first.
    bool isHorizontal = columnBlock->style()->isHorizontalWritingMode();
    LayoutUnit logicalLeft = columnBlock->logicalLeftOffsetForContent();
    LayoutUnit currLogicalTopOffset = 0;
    int i;
    for (i = 0; i < colCount; i++) {
        LayoutRect colRect = columnBlock->columnRectAt(colInfo, i);
        LayoutUnit blockDelta =  (isHorizontal ? colRect.height() : colRect.width());
        if (columnBlock->style()->isFlippedBlocksWritingMode())
            currLogicalTopOffset += blockDelta;
        else
            currLogicalTopOffset -= blockDelta;
    }
    for (i = colCount - 1; i >= 0; i--) {
        // For each rect, we clip to the rect, and then we adjust our coords.
        LayoutRect colRect = columnBlock->columnRectAt(colInfo, i);
        columnBlock->flipForWritingMode(colRect);
        LayoutUnit currLogicalLeftOffset = (isHorizontal ? colRect.x() : colRect.y()) - logicalLeft;
        LayoutUnit blockDelta =  (isHorizontal ? colRect.height() : colRect.width());
        if (columnBlock->style()->isFlippedBlocksWritingMode())
            currLogicalTopOffset -= blockDelta;
        else
            currLogicalTopOffset += blockDelta;

        LayoutSize offset;
        if (isHorizontal) {
            if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                offset = LayoutSize(currLogicalLeftOffset, currLogicalTopOffset);
            else
                offset = LayoutSize(0, colRect.y() + currLogicalTopOffset - columnBlock->borderTop() - columnBlock->paddingTop());
        } else {
            if (colInfo->progressionAxis() == ColumnInfo::InlineAxis)
                offset = LayoutSize(currLogicalTopOffset, currLogicalLeftOffset);
            else
                offset = LayoutSize(colRect.x() + currLogicalTopOffset - columnBlock->borderLeft() - columnBlock->paddingLeft(), 0);
        }

        colRect.moveBy(layerOffset);

        LayoutRect localClipRect(hitTestRect);
        localClipRect.intersect(colRect);

        if (!localClipRect.isEmpty() && hitTestPoint.intersects(localClipRect)) {
            RenderLayer* hitLayer = 0;
            if (!columnIndex) {
                // Apply a translation transform to change where the layer paints.
                TransformationMatrix oldTransform;
                bool oldHasTransform = childLayer->transform();
                if (oldHasTransform)
                    oldTransform = *childLayer->transform();
                TransformationMatrix newTransform(oldTransform);
                newTransform.translateRight(offset.width(), offset.height());

                childLayer->m_transform = adoptPtr(new TransformationMatrix(newTransform));
                hitLayer = childLayer->hitTestLayer(rootLayer, columnLayers[0], request, result, localClipRect, hitTestPoint, false, transformState, zOffset);
                if (oldHasTransform)
                    childLayer->m_transform = adoptPtr(new TransformationMatrix(oldTransform));
                else
                    childLayer->m_transform.clear();
            } else {
                // Adjust the transform such that the renderer's upper left corner will be at (0,0) in user space.
                // This involves subtracting out the position of the layer in our current coordinate space.
                RenderLayer* nextLayer = columnLayers[columnIndex - 1];
                RefPtr<HitTestingTransformState> newTransformState = nextLayer->createLocalTransformState(rootLayer, nextLayer, localClipRect, hitTestPoint, transformState);
                newTransformState->translate(offset.width(), offset.height(), HitTestingTransformState::AccumulateTransform);
                FloatPoint localPoint = newTransformState->mappedPoint();
                FloatQuad localPointQuad = newTransformState->mappedQuad();
                LayoutRect localHitTestRect = newTransformState->mappedArea().enclosingBoundingBox();
                HitTestPoint newHitTestPoint;
                if (hitTestPoint.isRectBasedTest())
                    newHitTestPoint = HitTestPoint(localPoint, localPointQuad);
                else
                    newHitTestPoint = HitTestPoint(localPoint);
                newTransformState->flatten();

                hitLayer = hitTestChildLayerColumns(childLayer, columnLayers[columnIndex - 1], request, result, localHitTestRect, newHitTestPoint,
                                                    newTransformState.get(), zOffset, columnLayers, columnIndex - 1);
            }

            if (hitLayer)
                return hitLayer;
        }
    }

    return 0;
}

void RenderLayer::updateClipRects(const RenderLayer* rootLayer, RenderRegion* region, ClipRectsType clipRectsType, OverlayScrollbarSizeRelevancy relevancy)
{
    ASSERT(clipRectsType < NumCachedClipRectsTypes);
    if (m_clipRectsCache && m_clipRectsCache->m_clipRects[clipRectsType]) {
        ASSERT(rootLayer == m_clipRectsCache->m_clipRectsRoot[clipRectsType]);
        return; // We have the correct cached value.
    }
    
    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent.  We want to cache clip rects with us as the root.
    RenderLayer* parentLayer = rootLayer != this ? parent() : 0;
    if (parentLayer)
        parentLayer->updateClipRects(rootLayer, region, clipRectsType, relevancy);

    ClipRects clipRects;
    calculateClipRects(rootLayer, region, clipRectsType, clipRects, relevancy);

    if (!m_clipRectsCache)
        m_clipRectsCache = adoptPtr(new ClipRectsCache);

    if (parentLayer && parentLayer->clipRects(clipRectsType) && clipRects == *parentLayer->clipRects(clipRectsType))
        m_clipRectsCache->m_clipRects[clipRectsType] = parentLayer->clipRects(clipRectsType);
    else
        m_clipRectsCache->m_clipRects[clipRectsType] = ClipRects::create(clipRects);

#ifndef NDEBUG
    m_clipRectsCache->m_clipRectsRoot[clipRectsType] = rootLayer;
#endif
}

void RenderLayer::calculateClipRects(const RenderLayer* rootLayer, RenderRegion* region, ClipRectsType clipRectsType, ClipRects& clipRects, OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!parent()) {
        // The root layer's clip rect is always infinite.
        clipRects.reset(PaintInfo::infiniteRect());
        return;
    }
    
    bool useCached = clipRectsType != TemporaryClipRects;

    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent.  We want to cache clip rects with us as the root.
    RenderLayer* parentLayer = rootLayer != this ? parent() : 0;
    
    // Ensure that our parent's clip has been calculated so that we can examine the values.
    if (parentLayer) {
        if (useCached && parentLayer->clipRects(clipRectsType))
            clipRects = *parentLayer->clipRects(clipRectsType);
        else
            parentLayer->calculateClipRects(rootLayer, region, clipRectsType, clipRects);
    } else
        clipRects.reset(PaintInfo::infiniteRect());

    // A fixed object is essentially the root of its containing block hierarchy, so when
    // we encounter such an object, we reset our clip rects to the fixedClipRect.
    if (renderer()->style()->position() == FixedPosition) {
        clipRects.setPosClipRect(clipRects.fixedClipRect());
        clipRects.setOverflowClipRect(clipRects.fixedClipRect());
        clipRects.setFixed(true);
    }
    else if (renderer()->style()->position() == RelativePosition)
        clipRects.setPosClipRect(clipRects.overflowClipRect());
    else if (renderer()->style()->position() == AbsolutePosition)
        clipRects.setOverflowClipRect(clipRects.posClipRect());
    
    // Update the clip rects that will be passed to child layers.
    if (renderer()->hasClipOrOverflowClip()) {
        // This layer establishes a clip of some kind.

        // This offset cannot use convertToLayerCoords, because sometimes our rootLayer may be across
        // some transformed layer boundary, for example, in the RenderLayerCompositor overlapMap, where
        // clipRects are needed in view space.
        LayoutPoint offset;
        offset = roundedIntPoint(renderer()->localToContainerPoint(FloatPoint(), rootLayer->renderer()));
        RenderView* view = renderer()->view();
        ASSERT(view);
        if (view && clipRects.fixed() && rootLayer->renderer() == view) {
            offset -= view->frameView()->scrollOffsetForFixedPosition();
        }
        
        if (renderer()->hasOverflowClip()) {
            ClipRect newOverflowClip = toRenderBox(renderer())->overflowClipRect(offset, region, relevancy);
            if (renderer()->style()->hasBorderRadius())
                newOverflowClip.setHasRadius(true);
            clipRects.setOverflowClipRect(intersection(newOverflowClip, clipRects.overflowClipRect()));
            if (renderer()->isOutOfFlowPositioned() || renderer()->isRelPositioned())
                clipRects.setPosClipRect(intersection(newOverflowClip, clipRects.posClipRect()));
        }
        if (renderer()->hasClip()) {
            LayoutRect newPosClip = toRenderBox(renderer())->clipRect(offset, region);
            clipRects.setPosClipRect(intersection(newPosClip, clipRects.posClipRect()));
            clipRects.setOverflowClipRect(intersection(newPosClip, clipRects.overflowClipRect()));
            clipRects.setFixedClipRect(intersection(newPosClip, clipRects.fixedClipRect()));
        }
    }
}

void RenderLayer::parentClipRects(const RenderLayer* rootLayer, RenderRegion* region, ClipRectsType clipRectsType, ClipRects& clipRects, OverlayScrollbarSizeRelevancy relevancy) const
{
    ASSERT(parent());
    if (clipRectsType == TemporaryClipRects) {
        parent()->calculateClipRects(rootLayer, region, clipRectsType, clipRects, relevancy);
        return;
    }

    parent()->updateClipRects(rootLayer, region, clipRectsType, relevancy);
    clipRects = *parent()->clipRects(clipRectsType);
}

static inline ClipRect backgroundClipRectForPosition(const ClipRects& parentRects, EPosition position)
{
    if (position == FixedPosition)
        return parentRects.fixedClipRect();

    if (position == AbsolutePosition)
        return parentRects.posClipRect();

    return parentRects.overflowClipRect();
}

ClipRect RenderLayer::backgroundClipRect(const RenderLayer* rootLayer, RenderRegion* region, ClipRectsType clipRectsType, OverlayScrollbarSizeRelevancy relevancy) const
{
    ASSERT(parent());
    ClipRects parentRects;
    parentClipRects(rootLayer, region, clipRectsType, parentRects, relevancy);
    ClipRect backgroundClipRect = backgroundClipRectForPosition(parentRects, renderer()->style()->position());
    RenderView* view = renderer()->view();
    ASSERT(view);

    // Note: infinite clipRects should not be scrolled here, otherwise they will accidentally no longer be considered infinite.
    if (parentRects.fixed() && rootLayer->renderer() == view && backgroundClipRect != PaintInfo::infiniteRect())
        backgroundClipRect.move(view->frameView()->scrollOffsetForFixedPosition());

    return backgroundClipRect;
}

void RenderLayer::calculateRects(const RenderLayer* rootLayer, RenderRegion* region, ClipRectsType clipRectsType, const LayoutRect& paintDirtyRect, LayoutRect& layerBounds,
                                 ClipRect& backgroundRect, ClipRect& foregroundRect, ClipRect& outlineRect, OverlayScrollbarSizeRelevancy relevancy) const
{
    if (rootLayer != this && parent()) {
        backgroundRect = backgroundClipRect(rootLayer, region, clipRectsType, relevancy);
        backgroundRect.intersect(paintDirtyRect);
    } else
        backgroundRect = paintDirtyRect;

    foregroundRect = backgroundRect;
    outlineRect = backgroundRect;
    
    LayoutPoint offset;
    convertToLayerCoords(rootLayer, offset);
    layerBounds = LayoutRect(offset, size());

    // Update the clip rects that will be passed to child layers.
    if (renderer()->hasClipOrOverflowClip()) {
        // This layer establishes a clip of some kind.
        if (renderer()->hasOverflowClip()) {
            foregroundRect.intersect(toRenderBox(renderer())->overflowClipRect(offset, region, relevancy));
            if (renderer()->style()->hasBorderRadius())
                foregroundRect.setHasRadius(true);
        }

        if (renderer()->hasClip()) {
            // Clip applies to *us* as well, so go ahead and update the damageRect.
            LayoutRect newPosClip = toRenderBox(renderer())->clipRect(offset, region);
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
            LayoutRect layerBoundsWithVisualOverflow = renderBox()->visualOverflowRect();
            renderBox()->flipForWritingMode(layerBoundsWithVisualOverflow); // Layers are in physical coordinates, so the overflow has to be flipped.
            layerBoundsWithVisualOverflow.moveBy(offset);
            backgroundRect.intersect(layerBoundsWithVisualOverflow);
        } else {
            // Shift the bounds to be for our region only.
            LayoutRect bounds = pixelSnappedIntRect(renderBox()->borderBoxRectInRegion(region));
            bounds.moveBy(offset);
            backgroundRect.intersect(bounds);
        }
    }
}

LayoutRect RenderLayer::childrenClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderView* renderView = renderer()->view();
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    calculateRects(clippingRootLayer, 0, PaintingClipRects, renderView->unscaledDocumentRect(), layerBounds, backgroundRect, foregroundRect, outlineRect);
    return clippingRootLayer->renderer()->localToAbsoluteQuad(FloatQuad(foregroundRect.rect())).enclosingBoundingBox();
}

LayoutRect RenderLayer::selfClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderView* renderView = renderer()->view();
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    calculateRects(clippingRootLayer, 0, PaintingClipRects, renderView->documentRect(), layerBounds, backgroundRect, foregroundRect, outlineRect);
    return clippingRootLayer->renderer()->localToAbsoluteQuad(FloatQuad(backgroundRect.rect())).enclosingBoundingBox();
}

LayoutRect RenderLayer::localClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    RenderLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    calculateRects(clippingRootLayer, 0, PaintingClipRects, PaintInfo::infiniteRect(), layerBounds, backgroundRect, foregroundRect, outlineRect);

    LayoutRect clipRect = backgroundRect.rect();
    if (clipRect == PaintInfo::infiniteRect())
        return clipRect;

    LayoutPoint clippingRootOffset;
    convertToLayerCoords(clippingRootLayer, clippingRootOffset);
    clipRect.moveBy(-clippingRootOffset);

    return clipRect;
}

void RenderLayer::addBlockSelectionGapsBounds(const LayoutRect& bounds)
{
    m_blockSelectionGapsBounds.unite(bounds);
}

void RenderLayer::clearBlockSelectionGapsBounds()
{
    m_blockSelectionGapsBounds = LayoutRect();
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
    if (renderer()->hasOverflowClip())
        rect.intersect(toRenderBox(renderer())->overflowClipRect(LayoutPoint(), 0)); // FIXME: Regions not accounted for.
    if (renderer()->hasClip())
        rect.intersect(toRenderBox(renderer())->clipRect(LayoutPoint(), 0)); // FIXME: Regions not accounted for.
    if (!rect.isEmpty())
        renderer()->repaintRectangle(rect);
}

bool RenderLayer::intersectsDamageRect(const LayoutRect& layerBounds, const LayoutRect& damageRect, const RenderLayer* rootLayer) const
{
    // Always examine the canvas and the root.
    // FIXME: Could eliminate the isRoot() check if we fix background painting so that the RenderView
    // paints the root's background.
    if (isRootLayer() || renderer()->isRoot())
        return true;

    // If we aren't an inline flow, and our layer bounds do intersect the damage rect, then we 
    // can go ahead and return true.
    RenderView* view = renderer()->view();
    ASSERT(view);
    if (view && !renderer()->isRenderInline()) {
        LayoutRect b = layerBounds;
        b.inflate(view->maximalOutlineSize());
        if (b.intersects(damageRect))
            return true;
    }
        
    // Otherwise we need to compute the bounding box of this single layer and see if it intersects
    // the damage rect.
    return boundingBox(rootLayer).intersects(damageRect);
}

LayoutRect RenderLayer::localBoundingBox() const
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
    if (renderer()->isRenderInline())
        result = toRenderInline(renderer())->linesVisualOverflowBoundingBox();
    else if (renderer()->isTableRow()) {
        // Our bounding box is just the union of all of our cells' border/overflow rects.
        for (RenderObject* child = renderer()->firstChild(); child; child = child->nextSibling()) {
            if (child->isTableCell()) {
                LayoutRect bbox = toRenderBox(child)->borderBoxRect();
                result.unite(bbox);
                LayoutRect overflowRect = renderBox()->visualOverflowRect();
                if (bbox != overflowRect)
                    result.unite(overflowRect);
            }
        }
    } else {
        RenderBox* box = renderBox();
        ASSERT(box);
        if (box->hasMask()) {
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

    RenderView* view = renderer()->view();
    ASSERT(view);
    if (view)
        result.inflate(view->maximalOutlineSize()); // Used to apply a fudge factor to dirty-rect checks on blocks/tables.

    return result;
}

LayoutRect RenderLayer::boundingBox(const RenderLayer* ancestorLayer) const
{    
    LayoutRect result = localBoundingBox();
    if (renderer()->isBox())
        renderBox()->flipForWritingMode(result);
    else
        renderer()->containingBlock()->flipForWritingMode(result);
    LayoutPoint delta;
    convertToLayerCoords(ancestorLayer, delta);
    result.moveBy(delta);
    return result;
}

IntRect RenderLayer::absoluteBoundingBox() const
{
    return pixelSnappedIntRect(boundingBox(root()));
}

IntRect RenderLayer::calculateLayerBounds(const RenderLayer* layer, const RenderLayer* ancestorLayer, CalculateLayerBoundsFlags flags)
{
    if (!layer->isSelfPaintingLayer())
        return IntRect();

    // FIXME: This could be improved to do a check like hasVisibleNonCompositingDescendantLayers() (bug 92580).
    if ((flags & ExcludeHiddenDescendants) && layer != ancestorLayer && !layer->hasVisibleContent() && !layer->hasVisibleDescendant())
        return IntRect();

    LayoutRect boundingBoxRect = layer->localBoundingBox();
    if (layer->renderer()->isBox())
        layer->renderBox()->flipForWritingMode(boundingBoxRect);
    else
        layer->renderer()->containingBlock()->flipForWritingMode(boundingBoxRect);

    if (layer->renderer()->isRoot()) {
        // If the root layer becomes composited (e.g. because some descendant with negative z-index is composited),
        // then it has to be big enough to cover the viewport in order to display the background. This is akin
        // to the code in RenderBox::paintRootBoxFillLayers().
        if (FrameView* frameView = layer->renderer()->view()->frameView()) {
            LayoutUnit contentsWidth = frameView->contentsWidth();
            LayoutUnit contentsHeight = frameView->contentsHeight();
        
            boundingBoxRect.setWidth(max(boundingBoxRect.width(), contentsWidth - boundingBoxRect.x()));
            boundingBoxRect.setHeight(max(boundingBoxRect.height(), contentsHeight - boundingBoxRect.y()));
        }
    }

    LayoutRect unionBounds = boundingBoxRect;

    if (flags & UseLocalClipRectIfPossible) {
        LayoutRect localClipRect = layer->localClipRect();
        if (localClipRect != PaintInfo::infiniteRect()) {
            if ((flags & IncludeSelfTransform) && layer->paintsWithTransform(PaintBehaviorNormal))
                localClipRect = layer->transform()->mapRect(localClipRect);

            LayoutPoint ancestorRelOffset;
            layer->convertToLayerCoords(ancestorLayer, ancestorRelOffset);
            localClipRect.moveBy(ancestorRelOffset);
            return pixelSnappedIntRect(localClipRect);
        }
    }

    // FIXME: should probably just pass 'flags' down to descendants.
    CalculateLayerBoundsFlags descendantFlags = DefaultCalculateLayerBoundsFlags | (flags & ExcludeHiddenDescendants);

    const_cast<RenderLayer*>(layer)->updateLayerListsIfNeeded();

    if (RenderLayer* reflection = layer->reflectionLayer()) {
        if (!reflection->isComposited()) {
            IntRect childUnionBounds = calculateLayerBounds(reflection, layer, descendantFlags);
            unionBounds.unite(childUnionBounds);
        }
    }
    
    ASSERT(layer->isStackingContext() || (!layer->posZOrderList() || !layer->posZOrderList()->size()));

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(const_cast<RenderLayer*>(layer));
#endif

    if (Vector<RenderLayer*>* negZOrderList = layer->negZOrderList()) {
        size_t listSize = negZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = negZOrderList->at(i);
            if (!curLayer->isComposited()) {
                IntRect childUnionBounds = calculateLayerBounds(curLayer, layer, descendantFlags);
                unionBounds.unite(childUnionBounds);
            }
        }
    }

    if (Vector<RenderLayer*>* posZOrderList = layer->posZOrderList()) {
        size_t listSize = posZOrderList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = posZOrderList->at(i);
            if (!curLayer->isComposited()) {
                IntRect childUnionBounds = calculateLayerBounds(curLayer, layer, descendantFlags);
                unionBounds.unite(childUnionBounds);
            }
        }
    }

    if (Vector<RenderLayer*>* normalFlowList = layer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            if (!curLayer->isComposited()) {
                IntRect curAbsBounds = calculateLayerBounds(curLayer, layer, descendantFlags);
                unionBounds.unite(curAbsBounds);
            }
        }
    }
    
#if ENABLE(CSS_FILTERS)
    // FIXME: We can optimize the size of the composited layers, by not enlarging
    // filtered areas with the outsets if we know that the filter is going to render in hardware.
    // https://bugs.webkit.org/show_bug.cgi?id=81239
    if ((flags & IncludeLayerFilterOutsets) && layer->renderer()->style()->hasFilterOutsets()) {
        int topOutset;
        int rightOutset;
        int bottomOutset;
        int leftOutset;
        layer->renderer()->style()->getFilterOutsets(topOutset, rightOutset, bottomOutset, leftOutset);
        unionBounds.move(-leftOutset, -topOutset);
        unionBounds.expand(leftOutset + rightOutset, topOutset + bottomOutset);
    }
#endif

    if ((flags & IncludeSelfTransform) && layer->paintsWithTransform(PaintBehaviorNormal)) {
        TransformationMatrix* affineTrans = layer->transform();
        boundingBoxRect = affineTrans->mapRect(boundingBoxRect);
        unionBounds = affineTrans->mapRect(unionBounds);
    }

    LayoutPoint ancestorRelOffset;
    layer->convertToLayerCoords(ancestorLayer, ancestorRelOffset);
    unionBounds.moveBy(ancestorRelOffset);
    
    return pixelSnappedIntRect(unionBounds);
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
        m_clipRectsCache->m_clipRects[typeToClear] = nullptr;
    }
}

#if USE(ACCELERATED_COMPOSITING)
RenderLayerBacking* RenderLayer::ensureBacking()
{
    if (!m_backing) {
        m_backing = adoptPtr(new RenderLayerBacking(this));
        compositor()->layerBecameComposited(this);

#if ENABLE(CSS_FILTERS)
        updateOrRemoveFilterEffect();
#endif
    }
    return m_backing.get();
}

void RenderLayer::clearBacking(bool layerBeingDestroyed)
{
    if (m_backing && !renderer()->documentBeingDestroyed())
        compositor()->layerBecameNonComposited(this);
    m_backing.clear();

#if ENABLE(CSS_FILTERS)
    if (!layerBeingDestroyed)
        updateOrRemoveFilterEffect();
#else
    UNUSED_PARAM(layerBeingDestroyed);
#endif
}

bool RenderLayer::hasCompositedMask() const
{
    return m_backing && m_backing->hasMaskLayer();
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
#endif

bool RenderLayer::paintsWithTransform(PaintBehavior paintBehavior) const
{
#if USE(ACCELERATED_COMPOSITING)
    bool paintsToWindow = !isComposited() || backing()->paintsIntoWindow();
#else
    bool paintsToWindow = true;
#endif    
    return transform() && ((paintBehavior & PaintBehaviorFlattenCompositingLayers) || paintsToWindow);
}

void RenderLayer::setParent(RenderLayer* parent)
{
    if (parent == m_parent)
        return;

#if USE(ACCELERATED_COMPOSITING)
    if (m_parent && !renderer()->documentBeingDestroyed())
        compositor()->layerWillBeRemoved(m_parent, this);
#endif
    
    m_parent = parent;
    
#if USE(ACCELERATED_COMPOSITING)
    if (m_parent && !renderer()->documentBeingDestroyed())
        compositor()->layerWasAdded(m_parent, this);
#endif
}

static RenderObject* commonAncestor(RenderObject* obj1, RenderObject* obj2)
{
    if (!obj1 || !obj2)
        return 0;

    for (RenderObject* currObj1 = obj1; currObj1; currObj1 = currObj1->hoverAncestor())
        for (RenderObject* currObj2 = obj2; currObj2; currObj2 = currObj2->hoverAncestor())
            if (currObj1 == currObj2)
                return currObj1;

    return 0;
}

void RenderLayer::updateHoverActiveState(const HitTestRequest& request, HitTestResult& result)
{
    // We don't update :hover/:active state when the result is marked as readOnly.
    if (request.readOnly())
        return;

    Document* doc = renderer()->document();

    Node* activeNode = doc->activeNode();
    if (activeNode && !request.active()) {
        // We are clearing the :active chain because the mouse has been released.
        for (RenderObject* curr = activeNode->renderer(); curr; curr = curr->parent()) {
            if (curr->node() && !curr->isText()) {
                curr->node()->setActive(false);
                curr->node()->clearInActiveChain();
            }
        }
        doc->setActiveNode(0);
    } else {
        Node* newActiveNode = result.innerNode();
        if (!activeNode && newActiveNode && request.active() && !request.touchMove()) {
            // We are setting the :active chain and freezing it. If future moves happen, they
            // will need to reference this chain.
            for (RenderObject* curr = newActiveNode->renderer(); curr; curr = curr->parent()) {
                if (curr->node() && !curr->isText())
                    curr->node()->setInActiveChain();
            }
            doc->setActiveNode(newActiveNode);
        }
    }
    // If the mouse has just been pressed, set :active on the chain. Those (and only those)
    // nodes should remain :active until the mouse is released.
    bool allowActiveChanges = !activeNode && doc->activeNode();

    // If the mouse is down and if this is a mouse move event, we want to restrict changes in 
    // :hover/:active to only apply to elements that are in the :active chain that we froze
    // at the time the mouse went down.
    bool mustBeInActiveChain = request.active() && request.move();

    RefPtr<Node> oldHoverNode = doc->hoverNode();
    // Clear the :hover chain when the touch gesture is over.
    if (request.touchRelease()) {
        if (oldHoverNode) {
            for (RenderObject* curr = oldHoverNode->renderer(); curr; curr = curr->parent()) {
                if (curr->node() && !curr->isText())
                    curr->node()->setHovered(false);
            }
            doc->setHoverNode(0);
        }
        // A touch release can not set new hover or active target.
        return;
    }

    // Check to see if the hovered node has changed.
    // If it hasn't, we do not need to do anything.
    Node* newHoverNode = result.innerNode();
    if (newHoverNode && !newHoverNode->renderer())
        newHoverNode = result.innerNonSharedNode();

    // Update our current hover node.
    doc->setHoverNode(newHoverNode);

    // We have two different objects.  Fetch their renderers.
    RenderObject* oldHoverObj = oldHoverNode ? oldHoverNode->renderer() : 0;
    RenderObject* newHoverObj = newHoverNode ? newHoverNode->renderer() : 0;
    
    // Locate the common ancestor render object for the two renderers.
    RenderObject* ancestor = commonAncestor(oldHoverObj, newHoverObj);

    Vector<RefPtr<Node>, 32> nodesToRemoveFromChain;
    Vector<RefPtr<Node>, 32> nodesToAddToChain;

    if (oldHoverObj != newHoverObj) {
        // The old hover path only needs to be cleared up to (and not including) the common ancestor;
        for (RenderObject* curr = oldHoverObj; curr && curr != ancestor; curr = curr->hoverAncestor()) {
            if (curr->node() && !curr->isText() && (!mustBeInActiveChain || curr->node()->inActiveChain()))
                nodesToRemoveFromChain.append(curr->node());
        }
    }

    // Now set the hover state for our new object up to the root.
    for (RenderObject* curr = newHoverObj; curr; curr = curr->hoverAncestor()) {
        if (curr->node() && !curr->isText() && (!mustBeInActiveChain || curr->node()->inActiveChain()))
            nodesToAddToChain.append(curr->node());
    }

    size_t removeCount = nodesToRemoveFromChain.size();
    for (size_t i = 0; i < removeCount; ++i) {
        nodesToRemoveFromChain[i]->setHovered(false);
    }

    size_t addCount = nodesToAddToChain.size();
    for (size_t i = 0; i < addCount; ++i) {
        if (allowActiveChanges)
            nodesToAddToChain[i]->setActive(true);
        nodesToAddToChain[i]->setHovered(true);
    }
}

// Helper for the sorting of layers by z-index.
static inline bool compareZIndex(RenderLayer* first, RenderLayer* second)
{
    return first->zIndex() < second->zIndex();
}

void RenderLayer::dirtyZOrderLists()
{
    ASSERT(m_layerListMutationAllowed);
    ASSERT(isStackingContext());

    if (m_posZOrderList)
        m_posZOrderList->clear();
    if (m_negZOrderList)
        m_negZOrderList->clear();
    m_zOrderListsDirty = true;

#if USE(ACCELERATED_COMPOSITING)
    if (!renderer()->documentBeingDestroyed())
        compositor()->setCompositingLayersNeedRebuild();
#endif
}

void RenderLayer::dirtyStackingContextZOrderLists()
{
    RenderLayer* sc = stackingContext();
    if (sc)
        sc->dirtyZOrderLists();
}

void RenderLayer::dirtyNormalFlowList()
{
    ASSERT(m_layerListMutationAllowed);

    if (m_normalFlowList)
        m_normalFlowList->clear();
    m_normalFlowListDirty = true;

#if USE(ACCELERATED_COMPOSITING)
    if (!renderer()->documentBeingDestroyed())
        compositor()->setCompositingLayersNeedRebuild();
#endif
}

void RenderLayer::rebuildZOrderLists()
{
    ASSERT(m_layerListMutationAllowed);
    ASSERT(isDirtyStackingContext());

#if USE(ACCELERATED_COMPOSITING)
    bool includeHiddenLayers = compositor()->inCompositingMode();
#else
    bool includeHiddenLayers = false;
#endif
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        if (!m_reflection || reflectionLayer() != child)
            child->collectLayers(includeHiddenLayers, m_posZOrderList, m_negZOrderList);

    // Sort the two lists.
    if (m_posZOrderList)
        std::stable_sort(m_posZOrderList->begin(), m_posZOrderList->end(), compareZIndex);

    if (m_negZOrderList)
        std::stable_sort(m_negZOrderList->begin(), m_negZOrderList->end(), compareZIndex);

    m_zOrderListsDirty = false;
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
                m_normalFlowList = new Vector<RenderLayer*>;
            m_normalFlowList->append(child);
        }
    }
    
    m_normalFlowListDirty = false;
}

void RenderLayer::collectLayers(bool includeHiddenLayers, Vector<RenderLayer*>*& posBuffer, Vector<RenderLayer*>*& negBuffer)
{
    updateDescendantDependentFlags();

    // Overflow layers are just painted by their enclosing layers, so they don't get put in zorder lists.
    bool includeHiddenLayer = includeHiddenLayers || (m_hasVisibleContent || (m_hasVisibleDescendant && isStackingContext()));
    if (includeHiddenLayer && !isNormalFlowOnly() && !renderer()->isRenderFlowThread()) {
        // Determine which buffer the child should be in.
        Vector<RenderLayer*>*& buffer = (zIndex() >= 0) ? posBuffer : negBuffer;

        // Create the buffer if it doesn't exist yet.
        if (!buffer)
            buffer = new Vector<RenderLayer*>;
        
        // Append ourselves at the end of the appropriate buffer.
        buffer->append(this);
    }

    // Recur into our children to collect more layers, but only if we don't establish
    // a stacking context.
    if ((includeHiddenLayers || m_hasVisibleDescendant) && !isStackingContext()) {
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            // Ignore reflections.
            if (!m_reflection || reflectionLayer() != child)
                child->collectLayers(includeHiddenLayers, posBuffer, negBuffer);
        }
    }
}

void RenderLayer::updateLayerListsIfNeeded()
{
    updateZOrderLists();
    updateNormalFlowList();

    if (RenderLayer* reflectionLayer = this->reflectionLayer()) {
        reflectionLayer->updateZOrderLists();
        reflectionLayer->updateNormalFlowList();
    }
}

void RenderLayer::updateCompositingAndLayerListsIfNeeded()
{
#if USE(ACCELERATED_COMPOSITING)
    if (compositor()->inCompositingMode()) {
        if (isDirtyStackingContext() || m_normalFlowListDirty)
            compositor()->updateCompositingLayers(CompositingUpdateOnHitTest, this);
        return;
    }
#endif
    updateLayerListsIfNeeded();
}

void RenderLayer::repaintIncludingDescendants()
{
    renderer()->repaint();
    for (RenderLayer* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->repaintIncludingDescendants();
}

#if USE(ACCELERATED_COMPOSITING)
void RenderLayer::setBackingNeedsRepaint()
{
    ASSERT(isComposited());
    if (backing()->paintsIntoWindow()) {
        // If we're trying to repaint the placeholder document layer, propagate the
        // repaint to the native view system.
        RenderView* view = renderer()->view();
        if (view)
            view->repaintViewRectangle(absoluteBoundingBox());
    } else
        backing()->setContentsNeedDisplay();
}

void RenderLayer::setBackingNeedsRepaintInRect(const LayoutRect& r)
{
    // https://bugs.webkit.org/show_bug.cgi?id=61159 describes an unreproducible crash here,
    // so assert but check that the layer is composited.
    ASSERT(isComposited());
    if (!isComposited() || backing()->paintsIntoWindow()) {
        // If we're trying to repaint the placeholder document layer, propagate the
        // repaint to the native view system.
        LayoutRect absRect(r);
        LayoutPoint delta;
        convertToLayerCoords(root(), delta);
        absRect.moveBy(delta);

        RenderView* view = renderer()->view();
        if (view)
            view->repaintViewRectangle(absRect);
    } else
        backing()->setContentsNeedDisplayInRect(pixelSnappedIntRect(r));
}

// Since we're only painting non-composited layers, we know that they all share the same repaintContainer.
void RenderLayer::repaintIncludingNonCompositingDescendants(RenderBoxModelObject* repaintContainer)
{
    renderer()->repaintUsingContainer(repaintContainer, renderer()->clippedOverflowRectForRepaint(repaintContainer));

    for (RenderLayer* curr = firstChild(); curr; curr = curr->nextSibling()) {
        if (!curr->isComposited())
            curr->repaintIncludingNonCompositingDescendants(repaintContainer);
    }
}
#endif

bool RenderLayer::shouldBeNormalFlowOnly() const
{
    return (renderer()->hasOverflowClip()
                || renderer()->hasReflection()
                || renderer()->hasMask()
                || renderer()->isCanvas()
                || renderer()->isVideo()
                || renderer()->isEmbeddedObject()
                || renderer()->isRenderIFrame()
                || renderer()->style()->specifiesColumns())
            && !renderer()->isOutOfFlowPositioned()
            && !renderer()->isRelPositioned()
            && !renderer()->hasTransform()
#if ENABLE(CSS_FILTERS)
            && !renderer()->hasFilter()
#endif
            && !isTransparent();
}

bool RenderLayer::shouldBeSelfPaintingLayer() const
{
    return !isNormalFlowOnly()
        || hasOverlayScrollbars()
        || renderer()->hasReflection()
        || renderer()->hasMask()
        || renderer()->isTableRow()
        || renderer()->isCanvas()
        || renderer()->isVideo()
        || renderer()->isEmbeddedObject()
        || renderer()->isRenderIFrame();
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

void RenderLayer::updateStackingContextsAfterStyleChange(const RenderStyle* oldStyle)
{
    if (!oldStyle)
        return;

    bool wasStackingContext = isStackingContext(oldStyle);
    bool isStackingContext = this->isStackingContext();
    if (isStackingContext != wasStackingContext) {
        dirtyStackingContextZOrderLists();
        if (isStackingContext)
            dirtyZOrderLists();
        else
            clearZOrderLists();
        return;
    }

    // FIXME: RenderLayer already handles visibility changes through our visiblity dirty bits. This logic could
    // likely be folded along with the rest.
    if (oldStyle->zIndex() != renderer()->style()->zIndex() || oldStyle->visibility() != renderer()->style()->visibility()) {
        dirtyStackingContextZOrderLists();
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
    if (box->style()->appearance() == ListboxPart)
        return;

    EOverflow overflowX = box->style()->overflowX();
    EOverflow overflowY = box->style()->overflowY();

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
        updateScrollableAreaSet((hasHorizontalOverflow() || hasVerticalOverflow()) && scrollsOverflow());
}

void RenderLayer::styleChanged(StyleDifference, const RenderStyle* oldStyle)
{
    bool isNormalFlowOnly = shouldBeNormalFlowOnly();
    if (isNormalFlowOnly != m_isNormalFlowOnly) {
        m_isNormalFlowOnly = isNormalFlowOnly;
        RenderLayer* p = parent();
        if (p)
            p->dirtyNormalFlowList();
        dirtyStackingContextZOrderLists();
    }

    if (renderer()->style()->overflowX() == OMARQUEE && renderer()->style()->marqueeBehavior() != MNONE && renderer()->isBox()) {
        if (!m_marquee)
            m_marquee = new RenderMarquee(this);
        m_marquee->updateMarqueeStyle();
    }
    else if (m_marquee) {
        delete m_marquee;
        m_marquee = 0;
    }

    updateStackingContextsAfterStyleChange(oldStyle);
    updateScrollbarsAfterStyleChange(oldStyle);
    // Overlay scrollbars can make this layer self-painting so we need
    // to recompute the bit once scrollbars have been updated.
    updateSelfPaintingLayer();

    if (!hasReflection() && m_reflection)
        removeReflection();
    else if (hasReflection()) {
        if (!m_reflection)
            createReflection();
        updateReflectionStyle();
    }
    
    // FIXME: Need to detect a swap from custom to native scrollbars (and vice versa).
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();
    
    updateScrollCornerStyle();
    updateResizerStyle();

#if ENABLE(CSS_FILTERS)
    bool backingDidCompositeLayers = isComposited() && backing()->canCompositeFilters();
#endif

    updateDescendantDependentFlags();
    updateTransform();

#if USE(ACCELERATED_COMPOSITING)
    if (compositor()->updateLayerCompositingState(this))
        compositor()->setCompositingLayersNeedRebuild();
    else if (oldStyle && (oldStyle->clip() != renderer()->style()->clip() || oldStyle->hasClip() != renderer()->style()->hasClip()))
        compositor()->setCompositingLayersNeedRebuild();
    else if (m_backing)
        m_backing->updateGraphicsLayerGeometry();
    else if (oldStyle && oldStyle->overflowX() != renderer()->style()->overflowX()) {
        if (stackingContext()->hasCompositingDescendant())
            compositor()->setCompositingLayersNeedRebuild();
    }
#endif

#if ENABLE(CSS_FILTERS)
    updateOrRemoveFilterEffect();
    if (isComposited() && backingDidCompositeLayers && !backing()->canCompositeFilters()) {
        // The filters used to be drawn by platform code, but now the platform cannot draw them anymore.
        // Fallback to drawing them in software.
        setBackingNeedsRepaint();
    }
#endif
}

void RenderLayer::updateScrollableAreaSet(bool hasOverflow)
{
    Frame* frame = renderer()->frame();
    if (!frame)
        return;

    FrameView* frameView = frame->view();
    if (!frameView)
        return;

    bool isVisibleToHitTest = renderer()->visibleToHitTesting();
    if (HTMLFrameOwnerElement* owner = frame->ownerElement())
        isVisibleToHitTest &= owner->renderer() && owner->renderer()->visibleToHitTesting();

    if (hasOverflow && isVisibleToHitTest)
        frameView->addScrollableArea(this);
    else
        frameView->removeScrollableArea(this);
}

void RenderLayer::updateScrollCornerStyle()
{
    RenderObject* actualRenderer = renderer()->node() ? renderer()->node()->shadowAncestorNode()->renderer() : renderer();
    RefPtr<RenderStyle> corner = renderer()->hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(SCROLLBAR_CORNER, actualRenderer->style()) : PassRefPtr<RenderStyle>(0);
    if (corner) {
        if (!m_scrollCorner) {
            m_scrollCorner = new (renderer()->renderArena()) RenderScrollbarPart(renderer()->document());
            m_scrollCorner->setParent(renderer());
        }
        m_scrollCorner->setStyle(corner.release());
    } else if (m_scrollCorner) {
        m_scrollCorner->destroy();
        m_scrollCorner = 0;
    }
}

void RenderLayer::updateResizerStyle()
{
    RenderObject* actualRenderer = renderer()->node() ? renderer()->node()->shadowAncestorNode()->renderer() : renderer();
    RefPtr<RenderStyle> resizer = renderer()->hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(RESIZER, actualRenderer->style()) : PassRefPtr<RenderStyle>(0);
    if (resizer) {
        if (!m_resizer) {
            m_resizer = new (renderer()->renderArena()) RenderScrollbarPart(renderer()->document());
            m_resizer->setParent(renderer());
        }
        m_resizer->setStyle(resizer.release());
    } else if (m_resizer) {
        m_resizer->destroy();
        m_resizer = 0;
    }
}

RenderLayer* RenderLayer::reflectionLayer() const
{
    return m_reflection ? m_reflection->layer() : 0;
}

void RenderLayer::createReflection()
{
    ASSERT(!m_reflection);
    m_reflection = new (renderer()->renderArena()) RenderReplica(renderer()->document());
    m_reflection->setParent(renderer()); // We create a 1-way connection.
}

void RenderLayer::removeReflection()
{
    if (!m_reflection->documentBeingDestroyed())
        m_reflection->removeLayers(this);

    m_reflection->setParent(0);
    m_reflection->destroy();
    m_reflection = 0;
}

void RenderLayer::updateReflectionStyle()
{
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    newStyle->inheritFrom(renderer()->style());
    
    // Map in our transform.
    TransformOperations transform;
    switch (renderer()->style()->boxReflect()->direction()) {
        case ReflectionBelow:
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), Length(100., Percent), TransformOperation::TRANSLATE));
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), renderer()->style()->boxReflect()->offset(), TransformOperation::TRANSLATE));
            transform.operations().append(ScaleTransformOperation::create(1.0, -1.0, ScaleTransformOperation::SCALE));
            break;
        case ReflectionAbove:
            transform.operations().append(ScaleTransformOperation::create(1.0, -1.0, ScaleTransformOperation::SCALE));
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), Length(100., Percent), TransformOperation::TRANSLATE));
            transform.operations().append(TranslateTransformOperation::create(Length(0, Fixed), renderer()->style()->boxReflect()->offset(), TransformOperation::TRANSLATE));
            break;
        case ReflectionRight:
            transform.operations().append(TranslateTransformOperation::create(Length(100., Percent), Length(0, Fixed), TransformOperation::TRANSLATE));
            transform.operations().append(TranslateTransformOperation::create(renderer()->style()->boxReflect()->offset(), Length(0, Fixed), TransformOperation::TRANSLATE));
            transform.operations().append(ScaleTransformOperation::create(-1.0, 1.0, ScaleTransformOperation::SCALE));
            break;
        case ReflectionLeft:
            transform.operations().append(ScaleTransformOperation::create(-1.0, 1.0, ScaleTransformOperation::SCALE));
            transform.operations().append(TranslateTransformOperation::create(Length(100., Percent), Length(0, Fixed), TransformOperation::TRANSLATE));
            transform.operations().append(TranslateTransformOperation::create(renderer()->style()->boxReflect()->offset(), Length(0, Fixed), TransformOperation::TRANSLATE));
            break;
    }
    newStyle->setTransform(transform);

    // Map in our mask.
    newStyle->setMaskBoxImage(renderer()->style()->boxReflect()->mask());
    
    m_reflection->setStyle(newStyle.release());
}

#if ENABLE(CSS_FILTERS)
void RenderLayer::updateOrRemoveFilterEffect()
{
    if (!hasFilter()) {
        removeFilterInfoIfNeeded();
        return;
    }

#if ENABLE(CSS_SHADERS)
    if (renderer()->style()->filter().hasCustomFilter())
        ensureFilterInfo()->updateCustomFilterClients(renderer()->style()->filter());
    else if (hasFilterInfo())
        filterInfo()->removeCustomFilterClients();
#endif

#if ENABLE(SVG)
    if (renderer()->style()->filter().hasReferenceFilter())
        ensureFilterInfo()->updateReferenceFilterClients(renderer()->style()->filter());
    else if (hasFilterInfo())
        filterInfo()->removeReferenceFilterClients();
#endif

    if (!paintsWithFilters()) {
        // Don't delete the whole filter info here, because we might use it
        // for loading CSS shader files.
        if (RenderLayerFilterInfo* filterInfo = this->filterInfo())
            filterInfo->setRenderer(0);
        return;
    }
    
    RenderLayerFilterInfo* filterInfo = ensureFilterInfo();
    if (!filterInfo->renderer()) {
        RefPtr<FilterEffectRenderer> filterRenderer = FilterEffectRenderer::create();
        RenderingMode renderingMode = renderer()->frame()->page()->settings()->acceleratedFiltersEnabled() ? Accelerated : Unaccelerated;
        filterRenderer->setRenderingMode(renderingMode);
        filterInfo->setRenderer(filterRenderer.release());
    }

    // If the filter fails to build, remove it from the layer. It will still attempt to
    // go through regular processing (e.g. compositing), but never apply anything.
    if (!filterInfo->renderer()->build(renderer()->document(), renderer()->style()->filter()))
        filterInfo->setRenderer(0);
}

void RenderLayer::filterNeedsRepaint()
{
    renderer()->node()->setNeedsStyleRecalc(SyntheticStyleChange);
    if (renderer()->view())
        renderer()->repaint();
}
#endif

} // namespace WebCore

#ifndef NDEBUG
void showLayerTree(const WebCore::RenderLayer* layer)
{
    if (!layer)
        return;

    if (WebCore::Frame* frame = layer->renderer()->frame()) {
        WTF::String output = externalRepresentation(frame, WebCore::RenderAsTextShowAllLayers | WebCore::RenderAsTextShowLayerNesting | WebCore::RenderAsTextShowCompositedLayers | WebCore::RenderAsTextShowAddresses | WebCore::RenderAsTextShowIDAndClass | WebCore::RenderAsTextDontUpdateLayout | WebCore::RenderAsTextShowLayoutState);
        fprintf(stderr, "%s\n", output.utf8().data());
    }
}

void showLayerTree(const WebCore::RenderObject* renderer)
{
    if (!renderer)
        return;
    showLayerTree(renderer->enclosingLayer());
}
#endif
