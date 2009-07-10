/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "CString.h"
#include "CSSPropertyNames.h"
#include "CSSStyleDeclaration.h"
#include "CSSStyleSelector.h"
#include "Document.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatPoint3D.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "OverflowEvent.h"
#include "OverlapTestRequestClient.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "RenderArena.h"
#include "RenderInline.h"
#include "RenderMarquee.h"
#include "RenderReplica.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarPart.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "ScaleTransformOperation.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "SelectionController.h"
#include "TransformationMatrix.h"
#include "TransformState.h"
#include "TranslateTransformOperation.h"
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#endif

#if ENABLE(SVG)
#include "SVGNames.h"
#endif

#define MIN_INTERSECT_FOR_REVEAL 32

using namespace std;

namespace WebCore {

using namespace HTMLNames;

const int MinimumWidthWhileResizing = 100;
const int MinimumHeightWhileResizing = 40;

void* ClipRects::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void ClipRects::operator delete(void* ptr, size_t sz)
{
    // Stash size where destroy can find it.
    *(size_t *)ptr = sz;
}

void ClipRects::destroy(RenderArena* renderArena)
{
    delete this;
    
    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

RenderLayer::RenderLayer(RenderBoxModelObject* renderer)
    : m_renderer(renderer)
    , m_parent(0)
    , m_previous(0)
    , m_next(0)
    , m_first(0)
    , m_last(0)
    , m_relX(0)
    , m_relY(0)
    , m_x(0)
    , m_y(0)
    , m_width(0)
    , m_height(0)
    , m_scrollX(0)
    , m_scrollY(0)
    , m_scrollOriginX(0)
    , m_scrollLeftOverflow(0)
    , m_scrollWidth(0)
    , m_scrollHeight(0)
    , m_inResizeMode(false)
    , m_posZOrderList(0)
    , m_negZOrderList(0)
    , m_normalFlowList(0)
    , m_clipRects(0) 
#ifndef NDEBUG    
    , m_clipRectsRoot(0)
#endif
    , m_scrollDimensionsDirty(true)
    , m_zOrderListsDirty(true)
    , m_normalFlowListDirty(true)
    , m_isNormalFlowOnly(shouldBeNormalFlowOnly())
    , m_usedTransparency(false)
    , m_paintingInsideReflection(false)
    , m_inOverflowRelayout(false)
    , m_needsFullRepaint(false)
    , m_overflowStatusDirty(true)
    , m_visibleContentStatusDirty(true)
    , m_hasVisibleContent(false)
    , m_visibleDescendantStatusDirty(false)
    , m_hasVisibleDescendant(false)
    , m_3DTransformedDescendantStatusDirty(true)
    , m_has3DTransformedDescendant(false)
#if USE(ACCELERATED_COMPOSITING)
    , m_hasCompositingDescendant(false)
    , m_mustOverlapCompositedLayers(false)
#endif
    , m_marquee(0)
    , m_staticX(0)
    , m_staticY(0)
    , m_reflection(0)
    , m_scrollCorner(0)
    , m_resizer(0)
{
    if (!renderer->firstChild() && renderer->style()) {
        m_visibleContentStatusDirty = false;
        m_hasVisibleContent = renderer->style()->visibility() == VISIBLE;
    }
}

RenderLayer::~RenderLayer()
{
    if (inResizeMode() && !renderer()->documentBeingDestroyed()) {
        if (Frame* frame = renderer()->document()->frame())
            frame->eventHandler()->resizeLayerDestroyed();
    }

    destroyScrollbar(HorizontalScrollbar);
    destroyScrollbar(VerticalScrollbar);

    // Child layers will be deleted by their corresponding render objects, so
    // we don't need to delete them ourselves.

    delete m_posZOrderList;
    delete m_negZOrderList;
    delete m_normalFlowList;
    delete m_marquee;

#if USE(ACCELERATED_COMPOSITING)
    clearBacking();
#endif
    
    // Make sure we have no lingering clip rects.
    ASSERT(!m_clipRects);
    
    if (m_reflection) {
        if (!m_reflection->documentBeingDestroyed())
            m_reflection->removeLayers(this);
        m_reflection->setParent(0);
        m_reflection->destroy();
    }
    
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

void RenderLayer::rendererContentChanged()
{
    // This can get called when video becomes accelerated, so the layers may change.
    if (compositor()->updateLayerCompositingState(this))
        compositor()->setCompositingLayersNeedRebuild();

    if (m_backing)
        m_backing->rendererContentChanged();
}
#endif // USE(ACCELERATED_COMPOSITING)

bool RenderLayer::hasAcceleratedCompositing() const
{
#if USE(ACCELERATED_COMPOSITING)
    return compositor()->hasAcceleratedCompositing();
#else
    return false;
#endif
}

void RenderLayer::setStaticY(int staticY)
{
    if (m_staticY == staticY)
        return;
    m_staticY = staticY;
    renderer()->setChildNeedsLayout(true, false);
}

void RenderLayer::updateLayerPositions(UpdateLayerPositionsFlags flags)
{
    if (flags & DoFullRepaint) {
        renderer()->repaint();
#if USE(ACCELERATED_COMPOSITING)
        flags &= ~CheckForRepaint;
        // We need the full repaint to propagate to child layers if we are hardware compositing.
        if (!compositor()->inCompositingMode())
            flags &= ~DoFullRepaint;
#else
        flags &= ~(CheckForRepaint | DoFullRepaint);
#endif
    }
    
    updateLayerPosition(); // For relpositioned layers or non-positioned layers,
                           // we need to keep in sync, since we may have shifted relative
                           // to our parent layer.

    int x = 0;
    int y = 0;
    convertToLayerCoords(root(), x, y);
    positionOverflowControls(x, y);

    updateVisibilityStatus();

    updateTransform();
     
    if (m_hasVisibleContent) {
        RenderView* view = renderer()->view();
        ASSERT(view);
        // FIXME: Optimize using LayoutState and remove the disableLayoutState() call
        // from updateScrollInfoAfterLayout().
        ASSERT(!view->layoutStateEnabled());

        RenderBoxModelObject* repaintContainer = renderer()->containerForRepaint();
        IntRect newRect = renderer()->clippedOverflowRectForRepaint(repaintContainer);
        IntRect newOutlineBox = renderer()->outlineBoundsForRepaint(repaintContainer);
        if (flags & CheckForRepaint) {
            if (view && !view->printing()) {
                if (m_needsFullRepaint) {
                    renderer()->repaintUsingContainer(repaintContainer, m_repaintRect);
                    if (newRect != m_repaintRect)
                        renderer()->repaintUsingContainer(repaintContainer, newRect);
                } else
                    renderer()->repaintAfterLayoutIfNeeded(repaintContainer, m_repaintRect, m_outlineBox);
            }
        }
        m_repaintRect = newRect;
        m_outlineBox = newOutlineBox;
    } else {
        m_repaintRect = IntRect();
        m_outlineBox = IntRect();
    }

    m_needsFullRepaint = false;

    // Go ahead and update the reflection's position and size.
    if (m_reflection)
        m_reflection->layout();

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositions(flags);

#if USE(ACCELERATED_COMPOSITING)
    if ((flags & UpdateCompositingLayers) && isComposited())
        backing()->updateAfterLayout(RenderLayerBacking::CompositingChildren);
#endif
        
    // With all our children positioned, now update our marquee if we need to.
    if (m_marquee)
        m_marquee->updateMarqueePosition();
}

void RenderLayer::computeRepaintRects()
{
    RenderBoxModelObject* repaintContainer = renderer()->containerForRepaint();
    m_repaintRect = renderer()->clippedOverflowRectForRepaint(repaintContainer);
    m_outlineBox = renderer()->outlineBoundsForRepaint(repaintContainer);
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
            m_transform.set(new TransformationMatrix);
        else
            m_transform.clear();
    }
    
    if (hasTransform) {
        RenderBox* box = renderBox();
        ASSERT(box);
        m_transform->makeIdentity();
        box->style()->applyTransform(*m_transform, box->borderBoxRect().size(), RenderStyle::IncludeTransformOrigin);
        makeMatrixRenderable(*m_transform, hasAcceleratedCompositing());
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
        makeMatrixRenderable(currTransform, hasAcceleratedCompositing());
        return currTransform;
    }
#endif

    return *m_transform;
}

void RenderLayer::setHasVisibleContent(bool b)
{ 
    if (m_hasVisibleContent == b && !m_visibleContentStatusDirty)
        return;
    m_visibleContentStatusDirty = false; 
    m_hasVisibleContent = b;
    if (m_hasVisibleContent) {
        RenderBoxModelObject* repaintContainer = renderer()->containerForRepaint();
        m_repaintRect = renderer()->clippedOverflowRectForRepaint(repaintContainer);
        m_outlineBox = renderer()->outlineBoundsForRepaint(repaintContainer);
        if (!isNormalFlowOnly())
            dirtyStackingContextZOrderLists();
    }
    if (parent())
        parent()->childVisibilityChanged(m_hasVisibleContent);
}

void RenderLayer::dirtyVisibleContentStatus() 
{ 
    m_visibleContentStatusDirty = true; 
    if (parent())
        parent()->dirtyVisibleDescendantStatus();
}

void RenderLayer::childVisibilityChanged(bool newVisibility) 
{ 
    if (m_hasVisibleDescendant == newVisibility || m_visibleDescendantStatusDirty)
        return;
    if (newVisibility) {
        RenderLayer* l = this;
        while (l && !l->m_visibleDescendantStatusDirty && !l->m_hasVisibleDescendant) {
            l->m_hasVisibleDescendant = true;
            l = l->parent();
        }
    } else 
        dirtyVisibleDescendantStatus();
}

void RenderLayer::dirtyVisibleDescendantStatus()
{
    RenderLayer* l = this;
    while (l && !l->m_visibleDescendantStatusDirty) {
        l->m_visibleDescendantStatusDirty = true;
        l = l->parent();
    }
}

void RenderLayer::updateVisibilityStatus()
{
    if (m_visibleDescendantStatusDirty) {
        m_hasVisibleDescendant = false;
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            child->updateVisibilityStatus();        
            if (child->m_hasVisibleContent || child->m_hasVisibleDescendant) {
                m_hasVisibleDescendant = true;
                break;
            }
        }
        m_visibleDescendantStatusDirty = false;
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

        // Transformed or preserve-3d descendants can only be in the z-order lists, not
        // in the normal flow list, so we only need to check those.
        if (m_posZOrderList) {
            for (unsigned i = 0; i < m_posZOrderList->size(); ++i)
                m_has3DTransformedDescendant |= m_posZOrderList->at(i)->update3DTransformedDescendantStatus();
        }

        // Now check our negative z-index children.
        if (m_negZOrderList) {
            for (unsigned i = 0; i < m_negZOrderList->size(); ++i)
                m_has3DTransformedDescendant |= m_negZOrderList->at(i)->update3DTransformedDescendantStatus();
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
    // Clear our cached clip rect information.
    clearClipRects();

    RenderBox* rendererBox = renderBox();
    
    int x = rendererBox ? rendererBox->x() : 0;
    int y = rendererBox ? rendererBox->y() : 0;

    if (!renderer()->isPositioned() && renderer()->parent()) {
        // We must adjust our position by walking up the render tree looking for the
        // nearest enclosing object with a layer.
        RenderObject* curr = renderer()->parent();
        while (curr && !curr->hasLayer()) {
            if (curr->isBox() && !curr->isTableRow()) {
                // Rows and cells share the same coordinate space (that of the section).
                // Omit them when computing our xpos/ypos.
                RenderBox* currBox = toRenderBox(curr);
                x += currBox->x();
                y += currBox->y();
            }
            curr = curr->parent();
        }
        if (curr->isBox() && curr->isTableRow()) {
            // Put ourselves into the row coordinate space.
            RenderBox* currBox = toRenderBox(curr);
            x -= currBox->x();
            y -= currBox->y();
        }
    }

    m_relX = m_relY = 0;
    if (renderer()->isRelPositioned()) {
        m_relX = renderer()->relativePositionOffsetX();
        m_relY = renderer()->relativePositionOffsetY();
        x += m_relX; y += m_relY;
    }
    
    // Subtract our parent's scroll offset.
    if (renderer()->isPositioned() && enclosingPositionedAncestor()) {
        RenderLayer* positionedParent = enclosingPositionedAncestor();

        // For positioned layers, we subtract out the enclosing positioned layer's scroll offset.
        positionedParent->subtractScrolledContentOffset(x, y);
        
        if (renderer()->isPositioned() && positionedParent->renderer()->isRelPositioned() && positionedParent->renderer()->isRenderInline()) {
            IntSize offset = toRenderInline(positionedParent->renderer())->relativePositionedInlineOffset(toRenderBox(renderer()));
            x += offset.width();
            y += offset.height();
        }
    } else if (parent())
        parent()->subtractScrolledContentOffset(x, y);
    
    // FIXME: We'd really like to just get rid of the concept of a layer rectangle and rely on the renderers.

    setLocation(x, y);

    if (renderer()->isRenderInline()) {
        RenderInline* inlineFlow = toRenderInline(renderer());
        IntRect lineBox = inlineFlow->linesBoundingBox();
        setWidth(lineBox.width());
        setHeight(lineBox.height());
    } else if (RenderBox* box = renderBox()) {
        setWidth(box->width());
        setHeight(box->height());

        if (!box->hasOverflowClip()) {
            if (box->overflowWidth() > box->width())
                setWidth(box->overflowWidth());
            if (box->overflowHeight() > box->height())
                setHeight(box->overflowHeight());
        }
    }
}

TransformationMatrix RenderLayer::perspectiveTransform() const
{
    if (!renderer()->hasTransform())
        return TransformationMatrix();

    RenderStyle* style = renderer()->style();
    if (!style->hasPerspective())
        return TransformationMatrix();

    // Maybe fetch the perspective from the backing?
    const IntRect borderBox = toRenderBox(renderer())->borderBoxRect();
    const float boxWidth = borderBox.width();
    const float boxHeight = borderBox.height();

    float perspectiveOriginX = style->perspectiveOriginX().calcFloatValue(boxWidth);
    float perspectiveOriginY = style->perspectiveOriginY().calcFloatValue(boxHeight);

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

    const IntRect borderBox = toRenderBox(renderer())->borderBoxRect();
    RenderStyle* style = renderer()->style();

    return FloatPoint(style->perspectiveOriginX().calcFloatValue(borderBox.width()),
                      style->perspectiveOriginY().calcFloatValue(borderBox.height()));
}

RenderLayer* RenderLayer::stackingContext() const
{
    RenderLayer* layer = parent();
    while (layer && !layer->renderer()->isRenderView() && !layer->renderer()->isRoot() && layer->renderer()->style()->hasAutoZIndex())
        layer = layer->parent();
    return layer;
}

RenderLayer* RenderLayer::enclosingPositionedAncestor() const
{
    RenderLayer* curr = parent();
    for ( ; curr && !curr->renderer()->isRenderView() && !curr->renderer()->isPositioned() && !curr->renderer()->isRelPositioned() && !curr->hasTransform();
         curr = curr->parent()) { }
    return curr;
}

RenderLayer* RenderLayer::enclosingTransformedAncestor() const
{
    RenderLayer* curr = parent();
    for ( ; curr && !curr->renderer()->isRenderView() && !curr->transform(); curr = curr->parent())
        { }
    return curr;
}

#if USE(ACCELERATED_COMPOSITING)
RenderLayer* RenderLayer::enclosingCompositingLayer(bool includeSelf) const
{
    if (includeSelf && isComposited())
        return const_cast<RenderLayer*>(this);

    // Compositing layers are parented according to stacking order and overflow list,
    // so we have to check whether the parent is a stacking context, or whether 
    // the child is overflow-only.
    bool inNormalFlowList = isNormalFlowOnly();
    for (RenderLayer* curr = parent(); curr; curr = curr->parent()) {
        if (curr->isComposited() && (inNormalFlowList || curr->isStackingContext()))
            return curr;
        
        inNormalFlowList = curr->isNormalFlowOnly();
    }
         
    return 0;
}
#endif

IntPoint RenderLayer::absoluteToContents(const IntPoint& absolutePoint) const
{
    // We don't use convertToLayerCoords because it doesn't know about transforms
    return roundedIntPoint(renderer()->absoluteToLocal(absolutePoint, false, true));
}

bool RenderLayer::requiresSlowRepaints() const
{
    if (isTransparent() || hasReflection() || hasTransform())
        return true;
    if (!parent())
        return false;
    return parent()->requiresSlowRepaints();
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

static IntRect transparencyClipBox(const TransformationMatrix& enclosingTransform, const RenderLayer* l, const RenderLayer* rootLayer)
{
    // FIXME: Although this function completely ignores CSS-imposed clipping, we did already intersect with the
    // paintDirtyRect, and that should cut down on the amount we have to paint.  Still it
    // would be better to respect clips.
    
    if (rootLayer != l && l->paintsWithTransform()) {
        // The best we can do here is to use enclosed bounding boxes to establish a "fuzzy" enough clip to encompass
        // the transformed layer and all of its children.
        int x = 0;
        int y = 0;
        l->convertToLayerCoords(rootLayer, x, y);

        TransformationMatrix transform;
        transform.translate(x, y);
        transform = *l->transform() * transform;
        transform = transform * enclosingTransform;

        // We now have a transform that will produce a rectangle in our view's space.
        IntRect clipRect = transform.mapRect(l->boundingBox(l));
        
        // Now shift the root layer to be us and pass down the new enclosing transform.
        for (RenderLayer* curr = l->firstChild(); curr; curr = curr->nextSibling()) {
            if (!l->reflection() || l->reflectionLayer() != curr)
                clipRect.unite(transparencyClipBox(transform, curr, l));
        }
            
        return clipRect;
    }
    
    // Note: we don't have to walk z-order lists since transparent elements always establish
    // a stacking context.  This means we can just walk the layer tree directly.
    IntRect clipRect = l->boundingBox(rootLayer);
    
    // If we have a mask, then the clip is limited to the border box area (and there is
    // no need to examine child layers).
    if (!l->renderer()->hasMask()) {
        for (RenderLayer* curr = l->firstChild(); curr; curr = curr->nextSibling()) {
            if (!l->reflection() || l->reflectionLayer() != curr)
                clipRect.unite(transparencyClipBox(enclosingTransform, curr, rootLayer));
        }
    }

    // Now map the clipRect via the enclosing transform
    return enclosingTransform.mapRect(clipRect);
}

void RenderLayer::beginTransparencyLayers(GraphicsContext* p, const RenderLayer* rootLayer)
{
    if (p->paintingDisabled() || (paintsWithTransparency() && m_usedTransparency))
        return;
    
    RenderLayer* ancestor = transparentPaintingAncestor();
    if (ancestor)
        ancestor->beginTransparencyLayers(p, rootLayer);
    
    if (paintsWithTransparency()) {
        m_usedTransparency = true;
        p->save();
        p->clip(transparencyClipBox(TransformationMatrix(), this, rootLayer));
        p->beginTransparencyLayer(renderer()->opacity());
    }
}

void* RenderLayer::operator new(size_t sz, RenderArena* renderArena) throw()
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
    } else
        setFirstChild(child);

    if (beforeChild) {
        beforeChild->setPreviousSibling(child);
        child->setNextSibling(beforeChild);
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

    child->updateVisibilityStatus();
    if (child->m_hasVisibleContent || child->m_hasVisibleDescendant)
        childVisibilityChanged(true);
    
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
    
    oldChild->updateVisibilityStatus();
    if (oldChild->m_hasVisibleContent || oldChild->m_hasVisibleDescendant)
        childVisibilityChanged(false);
    
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

    // Remove us from the parent.
    RenderLayer* parent = m_parent;
    RenderLayer* nextSib = nextSibling();
    parent->removeChild(this);
    
    if (reflection())
        removeChild(reflectionLayer());

    // Now walk our kids and reattach them to our parent.
    RenderLayer* current = m_first;
    while (current) {
        RenderLayer* next = current->nextSibling();
        removeChild(current);
        parent->addChild(current, nextSib);
        current->updateLayerPositions(); // Depends on hasLayer() already being false for proper layout.
        current = next;
    }

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

void 
RenderLayer::convertToLayerCoords(const RenderLayer* ancestorLayer, int& xPos, int& yPos) const
{
    if (ancestorLayer == this)
        return;
        
    if (renderer()->style()->position() == FixedPosition) {
        // Add in the offset of the view.  We can obtain this by calling
        // localToAbsolute() on the RenderView.
        FloatPoint absPos = renderer()->localToAbsolute(FloatPoint(), true);
        xPos += absPos.x();
        yPos += absPos.y();
        return;
    }
 
    RenderLayer* parentLayer;
    if (renderer()->style()->position() == AbsolutePosition)
        parentLayer = enclosingPositionedAncestor();
    else
        parentLayer = parent();
    
    if (!parentLayer) return;
    
    parentLayer->convertToLayerCoords(ancestorLayer, xPos, yPos);

    xPos += x();
    yPos += y();
}

void RenderLayer::panScrollFromPoint(const IntPoint& sourcePoint) 
{
    // We want to reduce the speed if we're close from the original point to improve the handleability of the scroll
    const int shortDistanceLimit = 100;  // We delimit a 200 pixels long square enclosing the original point
    const int speedReducer = 2;          // Within this square we divide the scrolling speed by 2
    
    Frame* frame = renderer()->document()->frame();
    if (!frame)
        return;
    
    IntPoint currentMousePosition = frame->eventHandler()->currentMousePosition();
    
    // We need to check if the current mouse position is out of the window. When the mouse is out of the window, the position is incoherent
    static IntPoint previousMousePosition;
    if (currentMousePosition.x() < 0 || currentMousePosition.y() < 0)
        currentMousePosition = previousMousePosition;
    else
        previousMousePosition = currentMousePosition;

    int xDelta = currentMousePosition.x() - sourcePoint.x();
    int yDelta = currentMousePosition.y() - sourcePoint.y();

    if (abs(xDelta) < ScrollView::noPanScrollRadius) // at the center we let the space for the icon
        xDelta = 0;
    if (abs(yDelta) < ScrollView::noPanScrollRadius)
        yDelta = 0;

    // Let's attenuate the speed for the short distances
    if (abs(xDelta) < shortDistanceLimit)
        xDelta /= speedReducer;
    if (abs(yDelta) < shortDistanceLimit)
        yDelta /= speedReducer;

    scrollByRecursively(xDelta, yDelta);
}

void RenderLayer::scrollByRecursively(int xDelta, int yDelta)
{
    bool restrictedByLineClamp = false;
    if (renderer()->parent())
        restrictedByLineClamp = renderer()->parent()->style()->lineClamp() >= 0;

    if (renderer()->hasOverflowClip() && !restrictedByLineClamp) {
        int newOffsetX = scrollXOffset() + xDelta;
        int newOffsetY = scrollYOffset() + yDelta;
        scrollToOffset(newOffsetX, newOffsetY);

        // If this layer can't do the scroll we ask its parent
        int leftToScrollX = newOffsetX - scrollXOffset();
        int leftToScrollY = newOffsetY - scrollYOffset();
        if ((leftToScrollX || leftToScrollY) && renderer()->parent()) {
            renderer()->parent()->enclosingLayer()->scrollByRecursively(leftToScrollX, leftToScrollY);
            Frame* frame = renderer()->document()->frame();
            if (frame)
                frame->eventHandler()->updateAutoscrollRenderer();
        }
    } else if (renderer()->view()->frameView())
        renderer()->view()->frameView()->scrollBy(IntSize(xDelta, yDelta));
}


void
RenderLayer::addScrolledContentOffset(int& x, int& y) const
{
    x += scrollXOffset() + m_scrollLeftOverflow;
    y += scrollYOffset();
}

void
RenderLayer::subtractScrolledContentOffset(int& x, int& y) const
{
    x -= scrollXOffset() + m_scrollLeftOverflow;
    y -= scrollYOffset();
}

void RenderLayer::scrollToOffset(int x, int y, bool updateScrollbars, bool repaint)
{
    RenderBox* box = renderBox();
    if (!box)
        return;

    if (box->style()->overflowX() != OMARQUEE) {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
    
        // Call the scrollWidth/Height functions so that the dimensions will be computed if they need
        // to be (for overflow:hidden blocks).
        int maxX = scrollWidth() - box->clientWidth();
        int maxY = scrollHeight() - box->clientHeight();
        
        if (x > maxX) x = maxX;
        if (y > maxY) y = maxY;
    }
    
    // FIXME: Eventually, we will want to perform a blit.  For now never
    // blit, since the check for blitting is going to be very
    // complicated (since it will involve testing whether our layer
    // is either occluded by another layer or clipped by an enclosing
    // layer or contains fixed backgrounds, etc.).
    int newScrollX = x - m_scrollOriginX;
    if (m_scrollY == y && m_scrollX == newScrollX)
        return;
    m_scrollX = newScrollX;
    m_scrollY = y;

    // Update the positions of our child layers. Don't have updateLayerPositions() update
    // compositing layers, because we need to do a deep update from the compositing ancestor.
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositions(0);

#if USE(ACCELERATED_COMPOSITING)
    if (compositor()->inCompositingMode()) {
        if (RenderLayer* compositingAncestor = ancestorCompositingLayer())
            compositingAncestor->backing()->updateAfterLayout(RenderLayerBacking::AllDescendants);
    }
#endif
    
    RenderView* view = renderer()->view();
    
    // We should have a RenderView if we're trying to scroll.
    ASSERT(view);
    if (view) {
#if ENABLE(DASHBOARD_SUPPORT)
        // Update dashboard regions, scrolling may change the clip of a
        // particular region.
        view->frameView()->updateDashboardRegions();
#endif

        view->updateWidgetPositions();
    }

    // The caret rect needs to be invalidated after scrolling
    Frame* frame = renderer()->document()->frame();
    if (frame)
        frame->invalidateSelection();

    // Just schedule a full repaint of our object.
    if (repaint)
        renderer()->repaint();
    
    if (updateScrollbars) {
        if (m_hBar)
            m_hBar->setValue(scrollXOffset());
        if (m_vBar)
            m_vBar->setValue(m_scrollY);
    }

    // Schedule the scroll DOM event.
    if (view) {
        if (FrameView* frameView = view->frameView())
            frameView->scheduleEvent(Event::create(eventNames().scrollEvent, false, false), renderer()->node());
    }
}

void RenderLayer::scrollRectToVisible(const IntRect &rect, bool scrollToAnchor, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    RenderLayer* parentLayer = 0;
    IntRect newRect = rect;
    int xOffset = 0, yOffset = 0;

    // We may end up propagating a scroll event. It is important that we suspend events until 
    // the end of the function since they could delete the layer or the layer's renderer().
    FrameView* frameView = renderer()->document()->view();
    if (frameView)
        frameView->pauseScheduledEvents();

    bool restrictedByLineClamp = false;
    if (renderer()->parent()) {
        parentLayer = renderer()->parent()->enclosingLayer();
        restrictedByLineClamp = renderer()->parent()->style()->lineClamp() >= 0;
    }

    if (renderer()->hasOverflowClip() && !restrictedByLineClamp) {
        // Don't scroll to reveal an overflow layer that is restricted by the -webkit-line-clamp property.
        // This will prevent us from revealing text hidden by the slider in Safari RSS.
        RenderBox* box = renderBox();
        ASSERT(box);
        FloatPoint absPos = box->localToAbsolute();
        absPos.move(box->borderLeft(), box->borderTop());

        IntRect layerBounds = IntRect(absPos.x() + scrollXOffset(), absPos.y() + scrollYOffset(), box->clientWidth(), box->clientHeight());
        IntRect exposeRect = IntRect(rect.x() + scrollXOffset(), rect.y() + scrollYOffset(), rect.width(), rect.height());
        IntRect r = getRectToExpose(layerBounds, exposeRect, alignX, alignY);
        
        xOffset = r.x() - absPos.x();
        yOffset = r.y() - absPos.y();
        // Adjust offsets if they're outside of the allowable range.
        xOffset = max(0, min(scrollWidth() - layerBounds.width(), xOffset));
        yOffset = max(0, min(scrollHeight() - layerBounds.height(), yOffset));
        
        if (xOffset != scrollXOffset() || yOffset != scrollYOffset()) {
            int diffX = scrollXOffset();
            int diffY = scrollYOffset();
            scrollToOffset(xOffset, yOffset);
            diffX = scrollXOffset() - diffX;
            diffY = scrollYOffset() - diffY;
            newRect.setX(rect.x() - diffX);
            newRect.setY(rect.y() - diffY);
        }
    } else if (!parentLayer && renderer()->isBox() && renderBox()->canBeProgramaticallyScrolled(scrollToAnchor)) {
        if (frameView) {
            if (renderer()->document() && renderer()->document()->ownerElement() && renderer()->document()->ownerElement()->renderer()) {
                IntRect viewRect = frameView->visibleContentRect();
                IntRect r = getRectToExpose(viewRect, rect, alignX, alignY);
                
                xOffset = r.x();
                yOffset = r.y();
                // Adjust offsets if they're outside of the allowable range.
                xOffset = max(0, min(frameView->contentsWidth(), xOffset));
                yOffset = max(0, min(frameView->contentsHeight(), yOffset));

                frameView->setScrollPosition(IntPoint(xOffset, yOffset));
                parentLayer = renderer()->document()->ownerElement()->renderer()->enclosingLayer();
                newRect.setX(rect.x() - frameView->scrollX() + frameView->x());
                newRect.setY(rect.y() - frameView->scrollY() + frameView->y());
            } else {
                IntRect viewRect = frameView->visibleContentRect(true);
                IntRect r = getRectToExpose(viewRect, rect, alignX, alignY);
                
                // If this is the outermost view that RenderLayer needs to scroll, then we should scroll the view recursively
                // Other apps, like Mail, rely on this feature.
                frameView->scrollRectIntoViewRecursively(r);
            }
        }
    }
    
    if (parentLayer)
        parentLayer->scrollRectToVisible(newRect, scrollToAnchor, alignX, alignY);

    if (frameView)
        frameView->resumeScheduledEvents();
}

IntRect RenderLayer::getRectToExpose(const IntRect &visibleRect, const IntRect &exposeRect, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    // Determine the appropriate X behavior.
    ScrollBehavior scrollX;
    IntRect exposeRectX(exposeRect.x(), visibleRect.y(), exposeRect.width(), visibleRect.height());
    int intersectWidth = intersection(visibleRect, exposeRectX).width();
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
    if (scrollX == alignToClosestEdge && exposeRect.right() > visibleRect.right() && exposeRect.width() < visibleRect.width())
        scrollX = alignRight;

    // Given the X behavior, compute the X coordinate.
    int x;
    if (scrollX == noScroll) 
        x = visibleRect.x();
    else if (scrollX == alignRight)
        x = exposeRect.right() - visibleRect.width();
    else if (scrollX == alignCenter)
        x = exposeRect.x() + (exposeRect.width() - visibleRect.width()) / 2;
    else
        x = exposeRect.x();

    // Determine the appropriate Y behavior.
    ScrollBehavior scrollY;
    IntRect exposeRectY(visibleRect.x(), exposeRect.y(), visibleRect.width(), exposeRect.height());
    int intersectHeight = intersection(visibleRect, exposeRectY).height();
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
    if (scrollY == alignToClosestEdge && exposeRect.bottom() > visibleRect.bottom() && exposeRect.height() < visibleRect.height())
        scrollY = alignBottom;

    // Given the Y behavior, compute the Y coordinate.
    int y;
    if (scrollY == noScroll) 
        y = visibleRect.y();
    else if (scrollY == alignBottom)
        y = exposeRect.bottom() - visibleRect.height();
    else if (scrollY == alignCenter)
        y = exposeRect.y() + (exposeRect.height() - visibleRect.height()) / 2;
    else
        y = exposeRect.y();

    return IntRect(IntPoint(x, y), visibleRect.size());
}

void RenderLayer::autoscroll()
{
    Frame* frame = renderer()->document()->frame();
    if (!frame)
        return;

    FrameView* frameView = frame->view();
    if (!frameView)
        return;

    frame->eventHandler()->updateSelectionForMouseDrag();

    IntPoint currentDocumentPosition = frameView->windowToContents(frame->eventHandler()->currentMousePosition());
    scrollRectToVisible(IntRect(currentDocumentPosition, IntSize(1, 1)), false, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded);
}

void RenderLayer::resize(const PlatformMouseEvent& evt, const IntSize& oldOffset)
{
    // FIXME: This should be possible on generated content but is not right now.
    if (!inResizeMode() || !renderer()->hasOverflowClip() || !renderer()->node())
        return;

    // Set the width and height of the shadow ancestor node if there is one.
    // This is necessary for textarea elements since the resizable layer is in the shadow content.
    Element* element = static_cast<Element*>(renderer()->node()->shadowAncestorNode());
    RenderBox* renderer = toRenderBox(element->renderer());

    EResize resize = renderer->style()->resize();
    if (resize == RESIZE_NONE)
        return;

    Document* document = element->document();
    if (!document->frame()->eventHandler()->mousePressed())
        return;

    float zoomFactor = renderer->style()->effectiveZoom();

    IntSize newOffset = offsetFromResizeCorner(document->view()->windowToContents(evt.pos()));
    newOffset.setWidth(newOffset.width() / zoomFactor);
    newOffset.setHeight(newOffset.height() / zoomFactor);
    
    IntSize currentSize = IntSize(renderer->width() / zoomFactor, renderer->height() / zoomFactor);
    IntSize minimumSize = element->minimumSizeForResizing().shrunkTo(currentSize);
    element->setMinimumSizeForResizing(minimumSize);
    
    IntSize adjustedOldOffset = IntSize(oldOffset.width() / zoomFactor, oldOffset.height() / zoomFactor);
    
    IntSize difference = (currentSize + newOffset - adjustedOldOffset).expandedTo(minimumSize) - currentSize;

    CSSStyleDeclaration* style = element->style();
    bool isBoxSizingBorder = renderer->style()->boxSizing() == BORDER_BOX;

    ExceptionCode ec;

    if (resize != RESIZE_VERTICAL && difference.width()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            style->setProperty(CSSPropertyMarginLeft, String::number(renderer->marginLeft() / zoomFactor) + "px", false, ec);
            style->setProperty(CSSPropertyMarginRight, String::number(renderer->marginRight() / zoomFactor) + "px", false, ec);
        }
        int baseWidth = renderer->width() - (isBoxSizingBorder ? 0
            : renderer->borderLeft() + renderer->paddingLeft() + renderer->borderRight() + renderer->paddingRight());
        baseWidth = baseWidth / zoomFactor;
        style->setProperty(CSSPropertyWidth, String::number(baseWidth + difference.width()) + "px", false, ec);
    }

    if (resize != RESIZE_HORIZONTAL && difference.height()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            style->setProperty(CSSPropertyMarginTop, String::number(renderer->marginTop() / zoomFactor) + "px", false, ec);
            style->setProperty(CSSPropertyMarginBottom, String::number(renderer->marginBottom() / zoomFactor) + "px", false, ec);
        }
        int baseHeight = renderer->height() - (isBoxSizingBorder ? 0
            : renderer->borderTop() + renderer->paddingTop() + renderer->borderBottom() + renderer->paddingBottom());
        baseHeight = baseHeight / zoomFactor;
        style->setProperty(CSSPropertyHeight, String::number(baseHeight + difference.height()) + "px", false, ec);
    }

    document->updateLayout();

    // FIXME (Radar 4118564): We should also autoscroll the window as necessary to keep the point under the cursor in view.
}

void RenderLayer::valueChanged(Scrollbar*)
{
    // Update scroll position from scrollbars.

    bool needUpdate = false;
    int newX = scrollXOffset();
    int newY = m_scrollY;
    
    if (m_hBar) {
        newX = m_hBar->value();
        if (newX != scrollXOffset())
           needUpdate = true;
    }

    if (m_vBar) {
        newY = m_vBar->value();
        if (newY != m_scrollY)
           needUpdate = true;
    }

    if (needUpdate)
        scrollToOffset(newX, newY, false);
}

bool RenderLayer::isActive() const
{
    Page* page = renderer()->document()->frame()->page();
    return page && page->focusController()->isActive();
}


static IntRect cornerRect(const RenderLayer* layer, const IntRect& bounds)
{
    int horizontalThickness;
    int verticalThickness;
    if (!layer->verticalScrollbar() && !layer->horizontalScrollbar()) {
        // FIXME: This isn't right.  We need to know the thickness of custom scrollbars
        // even when they don't exist in order to set the resizer square size properly.
        horizontalThickness = ScrollbarTheme::nativeTheme()->scrollbarThickness();
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
    return IntRect(bounds.right() - horizontalThickness - layer->renderer()->style()->borderRightWidth(), 
                   bounds.bottom() - verticalThickness - layer->renderer()->style()->borderBottomWidth(),
                   horizontalThickness, verticalThickness);
}

static IntRect scrollCornerRect(const RenderLayer* layer, const IntRect& bounds)
{
    // We have a scrollbar corner when a scrollbar is visible and not filling the entire length of the box.
    // This happens when:
    // (a) A resizer is present and at least one scrollbar is present
    // (b) Both scrollbars are present.
    bool hasHorizontalBar = layer->horizontalScrollbar();
    bool hasVerticalBar = layer->verticalScrollbar();
    bool hasResizer = layer->renderer()->style()->resize() != RESIZE_NONE;
    if ((hasHorizontalBar && hasVerticalBar) || (hasResizer && (hasHorizontalBar || hasVerticalBar)))
        return cornerRect(layer, bounds);
    return IntRect();
}

static IntRect resizerCornerRect(const RenderLayer* layer, const IntRect& bounds)
{
    ASSERT(layer->renderer()->isBox());
    if (layer->renderer()->style()->resize() == RESIZE_NONE)
        return IntRect();
    return cornerRect(layer, bounds);
}

bool RenderLayer::scrollbarCornerPresent() const
{
    ASSERT(renderer()->isBox());
    return !scrollCornerRect(this, renderBox()->borderBoxRect()).isEmpty();
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

IntSize RenderLayer::scrollbarOffset(const Scrollbar* scrollbar) const
{
    RenderBox* box = renderBox();

    if (scrollbar == m_vBar.get())
        return IntSize(box->width() - box->borderRight() - scrollbar->width(), box->borderTop());

    if (scrollbar == m_hBar.get())
        return IntSize(box->borderLeft(), box->height() - box->borderBottom() - scrollbar->height());
    
    ASSERT_NOT_REACHED();
    return IntSize();
}

void RenderLayer::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    IntRect scrollRect = rect;
    RenderBox* box = renderBox();
    ASSERT(box);
    if (scrollbar == m_vBar.get())
        scrollRect.move(box->width() - box->borderRight() - scrollbar->width(), box->borderTop());
    else
        scrollRect.move(box->borderLeft(), box->height() - box->borderBottom() - scrollbar->height());
    renderer()->repaintRectangle(scrollRect);
}

PassRefPtr<Scrollbar> RenderLayer::createScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar> widget;
    RenderObject* actualRenderer = renderer()->node() ? renderer()->node()->shadowAncestorNode()->renderer() : renderer();
    bool hasCustomScrollbarStyle = actualRenderer->isBox() && actualRenderer->style()->hasPseudoStyle(SCROLLBAR);
    if (hasCustomScrollbarStyle)
        widget = RenderScrollbar::createCustomScrollbar(this, orientation, toRenderBox(actualRenderer));
    else
        widget = Scrollbar::createNativeScrollbar(this, orientation, RegularScrollbar);
    renderer()->document()->view()->addChild(widget.get());        
    return widget.release();
}

void RenderLayer::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_hBar : m_vBar;
    if (scrollbar) {
        scrollbar->removeFromParent();
        scrollbar->setClient(0);
        scrollbar = 0;
    }
}

void RenderLayer::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == (m_hBar != 0))
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

#if ENABLE(DASHBOARD_SUPPORT)
    // Force an update since we know the scrollbars have changed things.
    if (renderer()->document()->hasDashboardRegions())
        renderer()->document()->setDashboardRegionsDirty(true);
#endif
}

void RenderLayer::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == (m_vBar != 0))
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

#if ENABLE(DASHBOARD_SUPPORT)
    // Force an update since we know the scrollbars have changed things.
    if (renderer()->document()->hasDashboardRegions())
        renderer()->document()->setDashboardRegionsDirty(true);
#endif
}

int RenderLayer::verticalScrollbarWidth() const
{
    if (!m_vBar)
        return 0;
    return m_vBar->width();
}

int RenderLayer::horizontalScrollbarHeight() const
{
    if (!m_hBar)
        return 0;
    return m_hBar->height();
}

IntSize RenderLayer::offsetFromResizeCorner(const IntPoint& absolutePoint) const
{
    // Currently the resize corner is always the bottom right corner
    IntPoint bottomRight(width(), height());
    IntPoint localPoint = absoluteToContents(absolutePoint);
    return localPoint - bottomRight;
}

void RenderLayer::positionOverflowControls(int tx, int ty)
{
    if (!m_hBar && !m_vBar && (!renderer()->hasOverflowClip() || renderer()->style()->resize() == RESIZE_NONE))
        return;
    
    RenderBox* box = renderBox();
    if (!box)
        return;

    IntRect borderBox = box->borderBoxRect();
    IntRect scrollCorner(scrollCornerRect(this, borderBox));
    IntRect absBounds(borderBox.x() + tx, borderBox.y() + ty, borderBox.width(), borderBox.height());
    if (m_vBar)
        m_vBar->setFrameRect(IntRect(absBounds.right() - box->borderRight() - m_vBar->width(),
                                     absBounds.y() + box->borderTop(),
                                     m_vBar->width(),
                                     absBounds.height() - (box->borderTop() + box->borderBottom()) - scrollCorner.height()));

    if (m_hBar)
        m_hBar->setFrameRect(IntRect(absBounds.x() + box->borderLeft(),
                                     absBounds.bottom() - box->borderBottom() - m_hBar->height(),
                                     absBounds.width() - (box->borderLeft() + box->borderRight()) - scrollCorner.width(),
                                     m_hBar->height()));
    
    if (m_scrollCorner)
        m_scrollCorner->setFrameRect(scrollCorner);
    if (m_resizer)
        m_resizer->setFrameRect(resizerCornerRect(this, borderBox));
}

int RenderLayer::scrollWidth()
{
    if (m_scrollDimensionsDirty)
        computeScrollDimensions();
    return m_scrollWidth;
}

int RenderLayer::scrollHeight()
{
    if (m_scrollDimensionsDirty)
        computeScrollDimensions();
    return m_scrollHeight;
}

void RenderLayer::computeScrollDimensions(bool* needHBar, bool* needVBar)
{
    RenderBox* box = renderBox();
    ASSERT(box);
    
    m_scrollDimensionsDirty = false;
    
    bool ltr = renderer()->style()->direction() == LTR;

    int clientWidth = box->clientWidth();
    int clientHeight = box->clientHeight();

    m_scrollLeftOverflow = ltr ? 0 : min(0, box->leftmostPosition(true, false) - box->borderLeft());

    int rightPos = ltr ?
                    box->rightmostPosition(true, false) - box->borderLeft() :
                    clientWidth - m_scrollLeftOverflow;
    int bottomPos = box->lowestPosition(true, false) - box->borderTop();

    m_scrollWidth = max(rightPos, clientWidth);
    m_scrollHeight = max(bottomPos, clientHeight);
    
    m_scrollOriginX = ltr ? 0 : m_scrollWidth - clientWidth;

    if (needHBar)
        *needHBar = rightPos > clientWidth;
    if (needVBar)
        *needVBar = bottomPos > clientHeight;
}

void RenderLayer::updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow)
{
    if (m_overflowStatusDirty) {
        m_horizontalOverflow = horizontalOverflow;
        m_verticalOverflow = verticalOverflow;
        m_overflowStatusDirty = false;
        
        return;
    }
    
    bool horizontalOverflowChanged = (m_horizontalOverflow != horizontalOverflow);
    bool verticalOverflowChanged = (m_verticalOverflow != verticalOverflow);
    
    if (horizontalOverflowChanged || verticalOverflowChanged) {
        m_horizontalOverflow = horizontalOverflow;
        m_verticalOverflow = verticalOverflow;
        
        if (FrameView* frameView = renderer()->document()->view()) {
            frameView->scheduleEvent(OverflowEvent::create(horizontalOverflowChanged, horizontalOverflow, verticalOverflowChanged, verticalOverflow),
                renderer()->node());
        }
    }
}

void
RenderLayer::updateScrollInfoAfterLayout()
{
    RenderBox* box = renderBox();
    if (!box)
        return;

    m_scrollDimensionsDirty = true;

    bool horizontalOverflow, verticalOverflow;
    computeScrollDimensions(&horizontalOverflow, &verticalOverflow);

    if (box->style()->overflowX() != OMARQUEE) {
        // Layout may cause us to be in an invalid scroll position.  In this case we need
        // to pull our scroll offsets back to the max (or push them up to the min).
        int newX = max(0, min(scrollXOffset(), scrollWidth() - box->clientWidth()));
        int newY = max(0, min(m_scrollY, scrollHeight() - box->clientHeight()));
        if (newX != scrollXOffset() || newY != m_scrollY) {
            RenderView* view = renderer()->view();
            ASSERT(view);
            // scrollToOffset() may call updateLayerPositions(), which doesn't work
            // with LayoutState.
            // FIXME: Remove the disableLayoutState/enableLayoutState if the above changes.
            if (view)
                view->disableLayoutState();
            scrollToOffset(newX, newY);
            if (view)
                view->enableLayoutState();
        }
    }

    bool haveHorizontalBar = m_hBar;
    bool haveVerticalBar = m_vBar;
    
    // overflow:scroll should just enable/disable.
    if (renderer()->style()->overflowX() == OSCROLL)
        m_hBar->setEnabled(horizontalOverflow);
    if (renderer()->style()->overflowY() == OSCROLL)
        m_vBar->setEnabled(verticalOverflow);

    // A dynamic change from a scrolling overflow to overflow:hidden means we need to get rid of any
    // scrollbars that may be present.
    if (renderer()->style()->overflowX() == OHIDDEN && haveHorizontalBar)
        setHasHorizontalScrollbar(false);
    if (renderer()->style()->overflowY() == OHIDDEN && haveVerticalBar)
        setHasVerticalScrollbar(false);
    
    // overflow:auto may need to lay out again if scrollbars got added/removed.
    bool scrollbarsChanged = (box->hasAutoHorizontalScrollbar() && haveHorizontalBar != horizontalOverflow) || 
                             (box->hasAutoVerticalScrollbar() && haveVerticalBar != verticalOverflow);    
    if (scrollbarsChanged) {
        if (box->hasAutoHorizontalScrollbar())
            setHasHorizontalScrollbar(horizontalOverflow);
        if (box->hasAutoVerticalScrollbar())
            setHasVerticalScrollbar(verticalOverflow);

#if ENABLE(DASHBOARD_SUPPORT)
        // Force an update since we know the scrollbars have changed things.
        if (renderer()->document()->hasDashboardRegions())
            renderer()->document()->setDashboardRegionsDirty(true);
#endif

        renderer()->repaint();

        if (renderer()->style()->overflowX() == OAUTO || renderer()->style()->overflowY() == OAUTO) {
            if (!m_inOverflowRelayout) {
                // Our proprietary overflow: overlay value doesn't trigger a layout.
                m_inOverflowRelayout = true;
                renderer()->setNeedsLayout(true, false);
                if (renderer()->isRenderBlock())
                    toRenderBlock(renderer())->layoutBlock(true);
                else
                    renderer()->layout();
                m_inOverflowRelayout = false;
            }
        }
    }
    
    // If overflow:scroll is turned into overflow:auto a bar might still be disabled (Bug 11985).
    if (m_hBar && box->hasAutoHorizontalScrollbar())
        m_hBar->setEnabled(true);
    if (m_vBar && box->hasAutoVerticalScrollbar())
        m_vBar->setEnabled(true);

    // Set up the range (and page step/line step).
    if (m_hBar) {
        int clientWidth = box->clientWidth();
        int pageStep = (clientWidth - cAmountToKeepWhenPaging);
        if (pageStep < 0) pageStep = clientWidth;
        m_hBar->setSteps(cScrollbarPixelsPerLineStep, pageStep);
        m_hBar->setProportion(clientWidth, m_scrollWidth);
        m_hBar->setValue(scrollXOffset());
    }
    if (m_vBar) {
        int clientHeight = box->clientHeight();
        int pageStep = (clientHeight - cAmountToKeepWhenPaging);
        if (pageStep < 0) pageStep = clientHeight;
        m_vBar->setSteps(cScrollbarPixelsPerLineStep, pageStep);
        m_vBar->setProportion(clientHeight, m_scrollHeight);
    }
 
    if (renderer()->node() && renderer()->document()->hasListenerType(Document::OVERFLOWCHANGED_LISTENER))
        updateOverflowStatus(horizontalOverflow, verticalOverflow);
}

void RenderLayer::paintOverflowControls(GraphicsContext* context, int tx, int ty, const IntRect& damageRect)
{
    // Don't do anything if we have no overflow.
    if (!renderer()->hasOverflowClip())
        return;
    
    // Move the scrollbar widgets if necessary.  We normally move and resize widgets during layout, but sometimes
    // widgets can move without layout occurring (most notably when you scroll a document that
    // contains fixed positioned elements).
    positionOverflowControls(tx, ty);

    // Now that we're sure the scrollbars are in the right place, paint them.
    if (m_hBar)
        m_hBar->paint(context, damageRect);
    if (m_vBar)
        m_vBar->paint(context, damageRect);

    // We fill our scroll corner with white if we have a scrollbar that doesn't run all the way up to the
    // edge of the box.
    paintScrollCorner(context, tx, ty, damageRect);
    
    // Paint our resizer last, since it sits on top of the scroll corner.
    paintResizer(context, tx, ty, damageRect);
}

void RenderLayer::paintScrollCorner(GraphicsContext* context, int tx, int ty, const IntRect& damageRect)
{
    RenderBox* box = renderBox();
    ASSERT(box);

    IntRect cornerRect = scrollCornerRect(this, box->borderBoxRect());
    IntRect absRect = IntRect(cornerRect.x() + tx, cornerRect.y() + ty, cornerRect.width(), cornerRect.height());
    if (!absRect.intersects(damageRect))
        return;

    if (context->updatingControlTints()) {
        updateScrollCornerStyle();
        return;
    }

    if (m_scrollCorner) {
        m_scrollCorner->paintIntoRect(context, tx, ty, absRect);
        return;
    }
    
    context->fillRect(absRect, Color::white);
}

void RenderLayer::paintResizer(GraphicsContext* context, int tx, int ty, const IntRect& damageRect)
{
    if (renderer()->style()->resize() == RESIZE_NONE)
        return;

    RenderBox* box = renderBox();
    ASSERT(box);

    IntRect cornerRect = resizerCornerRect(this, box->borderBoxRect());
    IntRect absRect = IntRect(cornerRect.x() + tx, cornerRect.y() + ty, cornerRect.width(), cornerRect.height());
    if (!absRect.intersects(damageRect))
        return;

    if (context->updatingControlTints()) {
        updateResizerStyle();
        return;
    }
    
    if (m_resizer) {
        m_resizer->paintIntoRect(context, tx, ty, absRect);
        return;
    }

    // Paint the resizer control.
    DEFINE_STATIC_LOCAL(RefPtr<Image>, resizeCornerImage, (Image::loadPlatformResource("textAreaResizeCorner")));
    IntPoint imagePoint(absRect.right() - resizeCornerImage->width(), absRect.bottom() - resizeCornerImage->height());
    context->drawImage(resizeCornerImage.get(), imagePoint);

    // Draw a frame around the resizer (1px grey line) if there are any scrollbars present.
    // Clipping will exclude the right and bottom edges of this frame.
    if (m_hBar || m_vBar) {
        context->save();
        context->clip(absRect);
        IntRect largerCorner = absRect;
        largerCorner.setSize(IntSize(largerCorner.width() + 1, largerCorner.height() + 1));
        context->setStrokeColor(Color(makeRGB(217, 217, 217)));
        context->setStrokeThickness(1.0f);
        context->setFillColor(Color::transparent);
        context->drawRect(largerCorner);
        context->restore();
    }
}

bool RenderLayer::isPointInResizeControl(const IntPoint& absolutePoint) const
{
    if (!renderer()->hasOverflowClip() || renderer()->style()->resize() == RESIZE_NONE)
        return false;
    
    RenderBox* box = renderBox();
    ASSERT(box);

    IntPoint localPoint = absoluteToContents(absolutePoint);

    IntRect localBounds(0, 0, box->width(), box->height());
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
        resizeControlRect = resizerCornerRect(this, box->borderBoxRect());
        if (resizeControlRect.contains(localPoint))
            return true;
    }

    int resizeControlSize = max(resizeControlRect.height(), 0);

    if (m_vBar) {
        IntRect vBarRect(box->width() - box->borderRight() - m_vBar->width(), 
                         box->borderTop(),
                         m_vBar->width(),
                         box->height() - (box->borderTop() + box->borderBottom()) - (m_hBar ? m_hBar->height() : resizeControlSize));
        if (vBarRect.contains(localPoint)) {
            result.setScrollbar(m_vBar.get());
            return true;
        }
    }

    resizeControlSize = max(resizeControlRect.width(), 0);
    if (m_hBar) {
        IntRect hBarRect(box->borderLeft(),
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
    bool didHorizontalScroll = false;
    bool didVerticalScroll = false;
    
    if (m_hBar) {
        if (granularity == ScrollByDocument) {
            // Special-case for the ScrollByDocument granularity. A document scroll can only be up 
            // or down and in both cases the horizontal bar goes all the way to the left.
            didHorizontalScroll = m_hBar->scroll(ScrollLeft, ScrollByDocument, multiplier);
        } else
            didHorizontalScroll = m_hBar->scroll(direction, granularity, multiplier);
    }

    if (m_vBar)
        didVerticalScroll = m_vBar->scroll(direction, granularity, multiplier);

    return (didHorizontalScroll || didVerticalScroll);
}

void RenderLayer::paint(GraphicsContext* p, const IntRect& damageRect, PaintRestriction paintRestriction, RenderObject *paintingRoot)
{
    RenderObject::OverlapTestRequestMap overlapTestRequests;
    paintLayer(this, p, damageRect, paintRestriction, paintingRoot, &overlapTestRequests);
    RenderObject::OverlapTestRequestMap::iterator end = overlapTestRequests.end();
    for (RenderObject::OverlapTestRequestMap::iterator it = overlapTestRequests.begin(); it != end; ++it)
        it->first->setOverlapTestResult(false);
}

static void setClip(GraphicsContext* p, const IntRect& paintDirtyRect, const IntRect& clipRect)
{
    if (paintDirtyRect == clipRect)
        return;
    p->save();
    p->clip(clipRect);
}

static void restoreClip(GraphicsContext* p, const IntRect& paintDirtyRect, const IntRect& clipRect)
{
    if (paintDirtyRect == clipRect)
        return;
    p->restore();
}

static void performOverlapTests(RenderObject::OverlapTestRequestMap& overlapTestRequests, const IntRect& layerBounds)
{
    Vector<OverlapTestRequestClient*> overlappedRequestClients;
    RenderObject::OverlapTestRequestMap::iterator end = overlapTestRequests.end();
    for (RenderObject::OverlapTestRequestMap::iterator it = overlapTestRequests.begin(); it != end; ++it) {
        if (!layerBounds.intersects(it->second))
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

void RenderLayer::paintLayer(RenderLayer* rootLayer, GraphicsContext* p,
                        const IntRect& paintDirtyRect, PaintRestriction paintRestriction,
                        RenderObject* paintingRoot, RenderObject::OverlapTestRequestMap* overlapTestRequests,
                        PaintLayerFlags paintFlags)
{
#if USE(ACCELERATED_COMPOSITING)
    if (isComposited()) {
        // The updatingControlTints() painting pass goes through compositing layers,
        // but we need to ensure that we don't cache clip rects computed with the wrong root in this case.
        if (p->updatingControlTints())
            paintFlags |= PaintLayerTemporaryClipRects;
        else if (!backing()->paintingGoesToWindow() && !shouldDoSoftwarePaint(this, paintFlags & PaintLayerPaintingReflection)) {
            // If this RenderLayer should paint into its backing, that will be done via RenderLayerBacking::paintIntoLayer().
            return;
        }
    }
#endif

    // Avoid painting layers when stylesheets haven't loaded.  This eliminates FOUC.
    // It's ok not to draw, because later on, when all the stylesheets do load, updateStyleSelector on the Document
    // will do a full repaint().
    if (renderer()->document()->didLayoutWithPendingStylesheets() && !renderer()->isRenderView() && !renderer()->isRoot())
        return;
    
    // If this layer is totally invisible then there is nothing to paint.
    if (!renderer()->opacity())
        return;

    if (paintsWithTransparency())
        paintFlags |= PaintLayerHaveTransparency;

    // Apply a transform if we have one.  A reflection is considered to be a transform, since it is a flip and a translate.
    if (paintsWithTransform() && !(paintFlags & PaintLayerAppliedTransform)) {
        // If the transform can't be inverted, then don't paint anything.
        if (!m_transform->isInvertible())
            return;

        // If we have a transparency layer enclosing us and we are the root of a transform, then we need to establish the transparency
        // layer from the parent now.
        if (paintFlags & PaintLayerHaveTransparency)
            parent()->beginTransparencyLayers(p, rootLayer);
  
        // Make sure the parent's clip rects have been calculated.
        IntRect clipRect = paintDirtyRect;
        if (parent()) {
            ClipRects parentRects;
            parentClipRects(rootLayer, parentRects, paintFlags & PaintLayerTemporaryClipRects);
            clipRect = parentRects.overflowClipRect();
            clipRect.intersect(paintDirtyRect);
        }
        
        // Push the parent coordinate space's clip.
        setClip(p, paintDirtyRect, clipRect);

        // Adjust the transform such that the renderer's upper left corner will paint at (0,0) in user space.
        // This involves subtracting out the position of the layer in our current coordinate space.
        int x = 0;
        int y = 0;
        convertToLayerCoords(rootLayer, x, y);
        TransformationMatrix transform;
        transform.translate(x, y);
        transform = *m_transform * transform;
        
        // Apply the transform.
        p->save();
        p->concatCTM(transform);

        // Now do a paint with the root layer shifted to be us.
        paintLayer(this, p, transform.inverse().mapRect(paintDirtyRect), paintRestriction, paintingRoot, overlapTestRequests, paintFlags | PaintLayerAppliedTransform);

        p->restore();
        
        // Restore the clip.
        restoreClip(p, paintDirtyRect, clipRect);
        
        return;
    }

    PaintLayerFlags localPaintFlags = paintFlags & ~PaintLayerAppliedTransform;
    bool haveTransparency = localPaintFlags & PaintLayerHaveTransparency;

    // Paint the reflection first if we have one.
    if (m_reflection && !m_paintingInsideReflection) {
        // Mark that we are now inside replica painting.
        m_paintingInsideReflection = true;
        reflectionLayer()->paintLayer(rootLayer, p, paintDirtyRect, paintRestriction, paintingRoot, overlapTestRequests, localPaintFlags | PaintLayerPaintingReflection);
        m_paintingInsideReflection = false;
    }

    // Calculate the clip rects we should use.
    IntRect layerBounds, damageRect, clipRectToApply, outlineRect;
    calculateRects(rootLayer, paintDirtyRect, layerBounds, damageRect, clipRectToApply, outlineRect, localPaintFlags & PaintLayerTemporaryClipRects);
    int x = layerBounds.x();
    int y = layerBounds.y();
    int tx = x - renderBoxX();
    int ty = y - renderBoxY();
                             
    // Ensure our lists are up-to-date.
    updateCompositingAndLayerListsIfNeeded();

    bool selectionOnly = paintRestriction == PaintRestrictionSelectionOnly || paintRestriction == PaintRestrictionSelectionOnlyBlackText;
    bool forceBlackText = paintRestriction == PaintRestrictionSelectionOnlyBlackText;
    
    // If this layer's renderer is a child of the paintingRoot, we render unconditionally, which
    // is done by passing a nil paintingRoot down to our renderer (as if no paintingRoot was ever set).
    // Else, our renderer tree may or may not contain the painting root, so we pass that root along
    // so it will be tested against as we decend through the renderers.
    RenderObject* paintingRootForRenderer = 0;
    if (paintingRoot && !renderer()->isDescendantOf(paintingRoot))
        paintingRootForRenderer = paintingRoot;

    if (overlapTestRequests)
        performOverlapTests(*overlapTestRequests, layerBounds);

    // We want to paint our layer, but only if we intersect the damage rect.
    bool shouldPaint = intersectsDamageRect(layerBounds, damageRect, rootLayer) && m_hasVisibleContent && isSelfPaintingLayer();
    if (shouldPaint && !selectionOnly && !damageRect.isEmpty()) {
        // Begin transparency layers lazily now that we know we have to paint something.
        if (haveTransparency)
            beginTransparencyLayers(p, rootLayer);
        
        // Paint our background first, before painting any child layers.
        // Establish the clip used to paint our background.
        setClip(p, paintDirtyRect, damageRect);

        // Paint the background.
        RenderObject::PaintInfo paintInfo(p, damageRect, PaintPhaseBlockBackground, false, paintingRootForRenderer, 0);
        renderer()->paint(paintInfo, tx, ty);

        // Restore the clip.
        restoreClip(p, paintDirtyRect, damageRect);
    }

    // Now walk the sorted list of children with negative z-indices.
    if (m_negZOrderList)
        for (Vector<RenderLayer*>::iterator it = m_negZOrderList->begin(); it != m_negZOrderList->end(); ++it)
            it[0]->paintLayer(rootLayer, p, paintDirtyRect, paintRestriction, paintingRoot, overlapTestRequests, localPaintFlags);
    
    // Now establish the appropriate clip and paint our child RenderObjects.
    if (shouldPaint && !clipRectToApply.isEmpty()) {
        // Begin transparency layers lazily now that we know we have to paint something.
        if (haveTransparency)
            beginTransparencyLayers(p, rootLayer);

        // Set up the clip used when painting our children.
        setClip(p, paintDirtyRect, clipRectToApply);
        RenderObject::PaintInfo paintInfo(p, clipRectToApply, 
                                          selectionOnly ? PaintPhaseSelection : PaintPhaseChildBlockBackgrounds,
                                          forceBlackText, paintingRootForRenderer, 0);
        renderer()->paint(paintInfo, tx, ty);
        if (!selectionOnly) {
            paintInfo.phase = PaintPhaseFloat;
            renderer()->paint(paintInfo, tx, ty);
            paintInfo.phase = PaintPhaseForeground;
            paintInfo.overlapTestRequests = overlapTestRequests;
            renderer()->paint(paintInfo, tx, ty);
            paintInfo.phase = PaintPhaseChildOutlines;
            renderer()->paint(paintInfo, tx, ty);
        }

        // Now restore our clip.
        restoreClip(p, paintDirtyRect, clipRectToApply);
    }
    
    if (!outlineRect.isEmpty() && isSelfPaintingLayer()) {
        // Paint our own outline
        RenderObject::PaintInfo paintInfo(p, outlineRect, PaintPhaseSelfOutline, false, paintingRootForRenderer, 0);
        setClip(p, paintDirtyRect, outlineRect);
        renderer()->paint(paintInfo, tx, ty);
        restoreClip(p, paintDirtyRect, outlineRect);
    }
    
    // Paint any child layers that have overflow.
    if (m_normalFlowList)
        for (Vector<RenderLayer*>::iterator it = m_normalFlowList->begin(); it != m_normalFlowList->end(); ++it)
            it[0]->paintLayer(rootLayer, p, paintDirtyRect, paintRestriction, paintingRoot, overlapTestRequests, localPaintFlags);

    // Now walk the sorted list of children with positive z-indices.
    if (m_posZOrderList)
        for (Vector<RenderLayer*>::iterator it = m_posZOrderList->begin(); it != m_posZOrderList->end(); ++it)
            it[0]->paintLayer(rootLayer, p, paintDirtyRect, paintRestriction, paintingRoot, overlapTestRequests, localPaintFlags);
    
    if (renderer()->hasMask() && shouldPaint && !selectionOnly && !damageRect.isEmpty()) {
        setClip(p, paintDirtyRect, damageRect);

        // Paint the mask.
        RenderObject::PaintInfo paintInfo(p, damageRect, PaintPhaseMask, false, paintingRootForRenderer, 0);
        renderer()->paint(paintInfo, tx, ty);
        
        // Restore the clip.
        restoreClip(p, paintDirtyRect, damageRect);
    }

    // End our transparency layer
    if (haveTransparency && m_usedTransparency && !m_paintingInsideReflection) {
        p->endTransparencyLayer();
        p->restore();
        m_usedTransparency = false;
    }
}

static inline IntRect frameVisibleRect(RenderObject* renderer)
{
    FrameView* frameView = renderer->document()->view();
    if (!frameView)
        return IntRect();

    return frameView->visibleContentRect();
}

bool RenderLayer::hitTest(const HitTestRequest& request, HitTestResult& result)
{
    renderer()->document()->updateLayout();
    
    IntRect boundsRect(m_x, m_y, width(), height());
    if (!request.ignoreClipping())
        boundsRect.intersect(frameVisibleRect(renderer()));

    RenderLayer* insideLayer = hitTestLayer(this, 0, request, result, boundsRect, result.point(), false);
    if (!insideLayer) {
        // We didn't hit any layer. If we are the root layer and the mouse is -- or just was -- down, 
        // return ourselves. We do this so mouse events continue getting delivered after a drag has 
        // exited the WebView, and so hit testing over a scrollbar hits the content document.
        if ((request.active() || request.mouseUp()) && renderer()->isRenderView()) {
            renderer()->updateHitTestResult(result, result.point());
            insideLayer = this;
        }
    }

    // Now determine if the result is inside an anchor; make sure an image map wins if
    // it already set URLElement and only use the innermost.
    Node* node = result.innerNode();
    while (node) {
        // for imagemaps, URLElement is the associated area element not the image itself
        if (node->isLink() && !result.URLElement() && !node->hasTagName(imgTag))
            result.setURLElement(static_cast<Element*>(node));
        node = node->eventParentNode();
    }

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
                                        const IntRect& hitTestRect, const IntPoint& hitTestPoint,
                                        const HitTestingTransformState* containerTransformState) const
{
    RefPtr<HitTestingTransformState> transformState;
    int offsetX = 0;
    int offsetY = 0;
    if (containerTransformState) {
        // If we're already computing transform state, then it's relative to the container (which we know is non-null).
        transformState = HitTestingTransformState::create(*containerTransformState);
        convertToLayerCoords(containerLayer, offsetX, offsetY);
    } else {
        // If this is the first time we need to make transform state, then base it off of hitTestPoint,
        // which is relative to rootLayer.
        transformState = HitTestingTransformState::create(hitTestPoint, FloatQuad(hitTestRect));
        convertToLayerCoords(rootLayer, offsetX, offsetY);
    }
    
    RenderObject* containerRenderer = containerLayer ? containerLayer->renderer() : 0;
    if (renderer()->shouldUseTransformFromContainer(containerRenderer)) {
        TransformationMatrix containerTransform;
        renderer()->getTransformFromContainer(containerRenderer, IntSize(offsetX, offsetY), containerTransform);
        transformState->applyTransform(containerTransform, HitTestingTransformState::AccumulateTransform);
    } else {
        transformState->translate(offsetX, offsetY, HitTestingTransformState::AccumulateTransform);
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
                                                const IntRect& hitTestRect, const IntPoint& hitTestPoint, bool appliedTransform,
                                                const HitTestingTransformState* transformState, double* zOffset)
{
    // The natural thing would be to keep HitTestingTransformState on the stack, but it's big, so we heap-allocate.

    bool useTemporaryClipRects = false;
#if USE(ACCELERATED_COMPOSITING)
    useTemporaryClipRects = compositor()->inCompositingMode();
#endif
    
    // Apply a transform if we have one.
    if (transform() && !appliedTransform) {
        // Make sure the parent's clip rects have been calculated.
        if (parent()) {
            ClipRects parentRects;
            parentClipRects(rootLayer, parentRects, useTemporaryClipRects);
            IntRect clipRect = parentRects.overflowClipRect();
            // Go ahead and test the enclosing clip now.
            if (!clipRect.contains(hitTestPoint))
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
        IntPoint localPoint = roundedIntPoint(newTransformState->mappedPoint());
        IntRect localHitTestRect;
#if USE(ACCELERATED_COMPOSITING)
        if (isComposited()) {
            // It doesn't make sense to project hitTestRect into the plane of this layer, so use the same bounds we use for painting.
            localHitTestRect = backing()->compositedBounds();
        } else
#endif
            localHitTestRect = newTransformState->mappedQuad().enclosingBoundingBox();

        // Now do a hit test with the root layer shifted to be us.
        return hitTestLayer(this, containerLayer, request, result, localHitTestRect, localPoint, true, newTransformState.get(), zOffset);
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
    IntRect layerBounds;
    IntRect bgRect;
    IntRect fgRect;
    IntRect outlineRect;
    calculateRects(rootLayer, hitTestRect, layerBounds, bgRect, fgRect, outlineRect, useTemporaryClipRects);
    
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
    if (m_posZOrderList) {
        for (int i = m_posZOrderList->size() - 1; i >= 0; --i) {
            HitTestResult tempResult(result.point());
            RenderLayer* hitLayer = m_posZOrderList->at(i)->hitTestLayer(rootLayer, this, request, tempResult, hitTestRect, hitTestPoint, false, localTransformState.get(), zOffsetForDescendantsPtr);
            if (isHitCandidate(hitLayer, depthSortDescendants, zOffset, unflattenedTransformState.get())) {
                result = tempResult;
                if (!depthSortDescendants)
                    return hitLayer;

                candidateLayer = hitLayer;
            }
        }
    }

    // Now check our overflow objects.
    if (m_normalFlowList) {
        for (int i = m_normalFlowList->size() - 1; i >= 0; --i) {
            RenderLayer* currLayer = m_normalFlowList->at(i);
            HitTestResult tempResult(result.point());
            RenderLayer* hitLayer = currLayer->hitTestLayer(rootLayer, this, request, tempResult, hitTestRect, hitTestPoint, false, localTransformState.get(), zOffsetForDescendantsPtr);
            if (isHitCandidate(hitLayer, depthSortDescendants, zOffset, unflattenedTransformState.get())) {
                result = tempResult;
                if (!depthSortDescendants)
                    return hitLayer;

                candidateLayer = hitLayer;
            }
        }
    }

    // Next we want to see if the mouse pos is inside the child RenderObjects of the layer.
    if (fgRect.contains(hitTestPoint) && isSelfPaintingLayer()) {
        // Hit test with a temporary HitTestResult, because we onlyl want to commit to 'result' if we know we're frontmost.
        HitTestResult tempResult(result.point());
        if (hitTestContents(request, tempResult, layerBounds, hitTestPoint, HitTestDescendants) &&
            isHitCandidate(this, false, zOffsetForContentsPtr, unflattenedTransformState.get())) {
            result = tempResult;
            if (!depthSortDescendants)
                return this;
            // Foreground can depth-sort with descendant layers, so keep this as a candidate.
            candidateLayer = this;
        }
    }

    // Now check our negative z-index children.
    if (m_negZOrderList) {
        for (int i = m_negZOrderList->size() - 1; i >= 0; --i) {
            HitTestResult tempResult(result.point());
            RenderLayer* hitLayer = m_negZOrderList->at(i)->hitTestLayer(rootLayer, this, request, tempResult, hitTestRect, hitTestPoint, false, localTransformState.get(), zOffsetForDescendantsPtr);
            if (isHitCandidate(hitLayer, depthSortDescendants, zOffset, unflattenedTransformState.get())) {
                result = tempResult;
                if (!depthSortDescendants)
                    return hitLayer;

                candidateLayer = hitLayer;
            }
        }
    }
    
    // If we found a layer, return. Child layers, and foreground always render in front of background.
    if (candidateLayer)
        return candidateLayer;

    if (bgRect.contains(hitTestPoint) && isSelfPaintingLayer()) {
        HitTestResult tempResult(result.point());
        if (hitTestContents(request, tempResult, layerBounds, hitTestPoint, HitTestSelf) &&
            isHitCandidate(this, false, zOffsetForContentsPtr, unflattenedTransformState.get())) {
            result = tempResult;
            return this;
        }
    }
    
    return 0;
}

bool RenderLayer::hitTestContents(const HitTestRequest& request, HitTestResult& result, const IntRect& layerBounds, const IntPoint& hitTestPoint, HitTestFilter hitTestFilter) const
{
    if (!renderer()->hitTest(request, result, hitTestPoint,
                            layerBounds.x() - renderBoxX(),
                            layerBounds.y() - renderBoxY(), 
                            hitTestFilter)) {
        // It's wrong to set innerNode, but then claim that you didn't hit anything.
        ASSERT(!result.innerNode());
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

void RenderLayer::updateClipRects(const RenderLayer* rootLayer)
{
    if (m_clipRects) {
        ASSERT(rootLayer == m_clipRectsRoot);
        return; // We have the correct cached value.
    }
    
    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent.  We want to cache clip rects with us as the root.
    RenderLayer* parentLayer = rootLayer != this ? parent() : 0;
    if (parentLayer)
        parentLayer->updateClipRects(rootLayer);

    ClipRects clipRects;
    calculateClipRects(rootLayer, clipRects, true);

    if (parentLayer && parentLayer->clipRects() && clipRects == *parentLayer->clipRects())
        m_clipRects = parentLayer->clipRects();
    else
        m_clipRects = new (renderer()->renderArena()) ClipRects(clipRects);
    m_clipRects->ref();
#ifndef NDEBUG
    m_clipRectsRoot = rootLayer;
#endif
}

void RenderLayer::calculateClipRects(const RenderLayer* rootLayer, ClipRects& clipRects, bool useCached) const
{
    if (!parent()) {
        // The root layer's clip rect is always infinite.
        clipRects.reset(ClipRects::infiniteRect());
        return;
    }

    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent.  We want to cache clip rects with us as the root.
    RenderLayer* parentLayer = rootLayer != this ? parent() : 0;
    
    // Ensure that our parent's clip has been calculated so that we can examine the values.
    if (parentLayer) {
        if (useCached && parentLayer->clipRects())
            clipRects = *parentLayer->clipRects();
        else
            parentLayer->calculateClipRects(rootLayer, clipRects);
    }
    else
        clipRects.reset(ClipRects::infiniteRect());

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
    if (renderer()->hasOverflowClip() || renderer()->hasClip()) {
        // This layer establishes a clip of some kind.
        int x = 0;
        int y = 0;
        convertToLayerCoords(rootLayer, x, y);
        RenderView* view = renderer()->view();
        ASSERT(view);
        if (view && clipRects.fixed() && rootLayer->renderer() == view) {
            x -= view->frameView()->scrollX();
            y -= view->frameView()->scrollY();
        }
        
        if (renderer()->hasOverflowClip()) {
            IntRect newOverflowClip = toRenderBox(renderer())->overflowClipRect(x, y);
            clipRects.setOverflowClipRect(intersection(newOverflowClip, clipRects.overflowClipRect()));
            if (renderer()->isPositioned() || renderer()->isRelPositioned())
                clipRects.setPosClipRect(intersection(newOverflowClip, clipRects.posClipRect()));
        }
        if (renderer()->hasClip()) {
            IntRect newPosClip = toRenderBox(renderer())->clipRect(x, y);
            clipRects.setPosClipRect(intersection(newPosClip, clipRects.posClipRect()));
            clipRects.setOverflowClipRect(intersection(newPosClip, clipRects.overflowClipRect()));
            clipRects.setFixedClipRect(intersection(newPosClip, clipRects.fixedClipRect()));
        }
    }
}

void RenderLayer::parentClipRects(const RenderLayer* rootLayer, ClipRects& clipRects, bool temporaryClipRects) const
{
    ASSERT(parent());
    if (temporaryClipRects) {
        parent()->calculateClipRects(rootLayer, clipRects);
        return;
    }

    parent()->updateClipRects(rootLayer);
    clipRects = *parent()->clipRects();
}

void RenderLayer::calculateRects(const RenderLayer* rootLayer, const IntRect& paintDirtyRect, IntRect& layerBounds,
                                 IntRect& backgroundRect, IntRect& foregroundRect, IntRect& outlineRect, bool temporaryClipRects) const
{
    if (rootLayer != this && parent()) {
        ClipRects parentRects;
        parentClipRects(rootLayer, parentRects, temporaryClipRects);
        backgroundRect = renderer()->style()->position() == FixedPosition ? parentRects.fixedClipRect() :
                         (renderer()->isPositioned() ? parentRects.posClipRect() : 
                                                       parentRects.overflowClipRect());
        RenderView* view = renderer()->view();
        ASSERT(view);
        if (view && parentRects.fixed() && rootLayer->renderer() == view)
            backgroundRect.move(view->frameView()->scrollX(), view->frameView()->scrollY());

        backgroundRect.intersect(paintDirtyRect);
    } else
        backgroundRect = paintDirtyRect;

    foregroundRect = backgroundRect;
    outlineRect = backgroundRect;
    
    int x = 0;
    int y = 0;
    convertToLayerCoords(rootLayer, x, y);
    layerBounds = IntRect(x, y, width(), height());
    
    // Update the clip rects that will be passed to child layers.
    if (renderer()->hasOverflowClip() || renderer()->hasClip()) {
        // This layer establishes a clip of some kind.
        if (renderer()->hasOverflowClip())
            foregroundRect.intersect(toRenderBox(renderer())->overflowClipRect(x, y));
        if (renderer()->hasClip()) {
            // Clip applies to *us* as well, so go ahead and update the damageRect.
            IntRect newPosClip = toRenderBox(renderer())->clipRect(x, y);
            backgroundRect.intersect(newPosClip);
            foregroundRect.intersect(newPosClip);
            outlineRect.intersect(newPosClip);
        }

        // If we establish a clip at all, then go ahead and make sure our background
        // rect is intersected with our layer's bounds.
        if (ShadowData* boxShadow = renderer()->style()->boxShadow()) {
            IntRect overflow = layerBounds;
            do {
                IntRect shadowRect = layerBounds;
                shadowRect.move(boxShadow->x, boxShadow->y);
                shadowRect.inflate(boxShadow->blur);
                overflow.unite(shadowRect);
                boxShadow = boxShadow->next;
            } while (boxShadow);
            backgroundRect.intersect(overflow);
        } else
            backgroundRect.intersect(layerBounds);
    }
}

IntRect RenderLayer::childrenClipRect() const
{
    RenderLayer* rootLayer = renderer()->view()->layer();
    IntRect layerBounds, backgroundRect, foregroundRect, outlineRect;
    calculateRects(rootLayer, rootLayer->boundingBox(rootLayer), layerBounds, backgroundRect, foregroundRect, outlineRect);
    return foregroundRect;
}

IntRect RenderLayer::selfClipRect() const
{
    RenderLayer* rootLayer = renderer()->view()->layer();
    IntRect layerBounds, backgroundRect, foregroundRect, outlineRect;
    calculateRects(rootLayer, rootLayer->boundingBox(rootLayer), layerBounds, backgroundRect, foregroundRect, outlineRect);
    return backgroundRect;
}

bool RenderLayer::intersectsDamageRect(const IntRect& layerBounds, const IntRect& damageRect, const RenderLayer* rootLayer) const
{
    // Always examine the canvas and the root.
    // FIXME: Could eliminate the isRoot() check if we fix background painting so that the RenderView
    // paints the root's background.
    if (renderer()->isRenderView() || renderer()->isRoot())
        return true;

    // If we aren't an inline flow, and our layer bounds do intersect the damage rect, then we 
    // can go ahead and return true.
    RenderView* view = renderer()->view();
    ASSERT(view);
    if (view && !renderer()->isRenderInline()) {
        IntRect b = layerBounds;
        b.inflate(view->maximalOutlineSize());
        if (b.intersects(damageRect))
            return true;
    }
        
    // Otherwise we need to compute the bounding box of this single layer and see if it intersects
    // the damage rect.
    return boundingBox(rootLayer).intersects(damageRect);
}

IntRect RenderLayer::localBoundingBox() const
{
    // There are three special cases we need to consider.
    // (1) Inline Flows.  For inline flows we will create a bounding box that fully encompasses all of the lines occupied by the
    // inline.  In other words, if some <span> wraps to three lines, we'll create a bounding box that fully encloses the root
    // line boxes of all three lines (including overflow on those lines).
    // (2) Left/Top Overflow.  The width/height of layers already includes right/bottom overflow.  However, in the case of left/top
    // overflow, we have to create a bounding box that will extend to include this overflow.
    // (3) Floats.  When a layer has overhanging floats that it paints, we need to make sure to include these overhanging floats
    // as part of our bounding box.  We do this because we are the responsible layer for both hit testing and painting those
    // floats.
    IntRect result;
    if (renderer()->isRenderInline()) {
        // Go from our first line box to our last line box.
        RenderInline* inlineFlow = toRenderInline(renderer());
        InlineFlowBox* firstBox = inlineFlow->firstLineBox();
        if (!firstBox)
            return result;
        int top = firstBox->root()->topOverflow();
        int bottom = inlineFlow->lastLineBox()->root()->bottomOverflow();
        int left = firstBox->x();
        for (InlineRunBox* curr = firstBox->nextLineBox(); curr; curr = curr->nextLineBox())
            left = min(left, curr->x());
        result = IntRect(left, top, width(), bottom - top);
    } else if (renderer()->isTableRow()) {
        // Our bounding box is just the union of all of our cells' border/overflow rects.
        for (RenderObject* child = renderer()->firstChild(); child; child = child->nextSibling()) {
            if (child->isTableCell()) {
                IntRect bbox = toRenderBox(child)->borderBoxRect();
                result.unite(bbox);
                IntRect overflowRect = renderBox()->overflowRect(false);
                if (bbox != overflowRect)
                    result.unite(overflowRect);
            }
        }
    } else {
        RenderBox* box = renderBox();
        ASSERT(box);
        if (box->hasMask())
            result = box->maskClipRect();
        else {
            IntRect bbox = box->borderBoxRect();
            result = bbox;
            IntRect overflowRect = box->overflowRect(false);
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

IntRect RenderLayer::boundingBox(const RenderLayer* ancestorLayer) const
{    
    IntRect result = localBoundingBox();

    int deltaX = 0, deltaY = 0;
    convertToLayerCoords(ancestorLayer, deltaX, deltaY);
    result.move(deltaX, deltaY);
    return result;
}

IntRect RenderLayer::absoluteBoundingBox() const
{
    return boundingBox(root());
}

void RenderLayer::clearClipRectsIncludingDescendants()
{
    if (!m_clipRects)
        return;

    clearClipRects();
    
    for (RenderLayer* l = firstChild(); l; l = l->nextSibling())
        l->clearClipRectsIncludingDescendants();
}

void RenderLayer::clearClipRects()
{
    if (m_clipRects) {
        m_clipRects->deref(renderer()->renderArena());
        m_clipRects = 0;
#ifndef NDEBUG
        m_clipRectsRoot = 0;
#endif    
    }
}

#if USE(ACCELERATED_COMPOSITING)
RenderLayerBacking* RenderLayer::ensureBacking()
{
    if (!m_backing)
        m_backing.set(new RenderLayerBacking(this));
    return m_backing.get();
}

void RenderLayer::clearBacking()
{
    m_backing.clear();
}
#endif

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
            if (curr->node() && !curr->isText())
                curr->node()->setInActiveChain(false);
        }
        doc->setActiveNode(0);
    } else {
        Node* newActiveNode = result.innerNode();
        if (!activeNode && newActiveNode && request.active()) {
            // We are setting the :active chain and freezing it. If future moves happen, they
            // will need to reference this chain.
            for (RenderObject* curr = newActiveNode->renderer(); curr; curr = curr->parent()) {
                if (curr->node() && !curr->isText()) {
                    curr->node()->setInActiveChain(true);
                }
            }
            doc->setActiveNode(newActiveNode);
        }
    }

    // If the mouse is down and if this is a mouse move event, we want to restrict changes in 
    // :hover/:active to only apply to elements that are in the :active chain that we froze
    // at the time the mouse went down.
    bool mustBeInActiveChain = request.active() && request.mouseMove();

    // Check to see if the hovered node has changed.  If not, then we don't need to
    // do anything.  
    RefPtr<Node> oldHoverNode = doc->hoverNode();
    Node* newHoverNode = result.innerNode();

    // Update our current hover node.
    doc->setHoverNode(newHoverNode);

    // We have two different objects.  Fetch their renderers.
    RenderObject* oldHoverObj = oldHoverNode ? oldHoverNode->renderer() : 0;
    RenderObject* newHoverObj = newHoverNode ? newHoverNode->renderer() : 0;
    
    // Locate the common ancestor render object for the two renderers.
    RenderObject* ancestor = commonAncestor(oldHoverObj, newHoverObj);

    if (oldHoverObj != newHoverObj) {
        // The old hover path only needs to be cleared up to (and not including) the common ancestor;
        for (RenderObject* curr = oldHoverObj; curr && curr != ancestor; curr = curr->hoverAncestor()) {
            if (curr->node() && !curr->isText() && (!mustBeInActiveChain || curr->node()->inActiveChain())) {
                curr->node()->setActive(false);
                curr->node()->setHovered(false);
            }
        }
    }

    // Now set the hover state for our new object up to the root.
    for (RenderObject* curr = newHoverObj; curr; curr = curr->hoverAncestor()) {
        if (curr->node() && !curr->isText() && (!mustBeInActiveChain || curr->node()->inActiveChain())) {
            curr->node()->setActive(request.active());
            curr->node()->setHovered(true);
        }
    }
}

// Helper for the sorting of layers by z-index.
static inline bool compareZIndex(RenderLayer* first, RenderLayer* second)
{
    return first->zIndex() < second->zIndex();
}

void RenderLayer::dirtyZOrderLists()
{
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
    if (m_normalFlowList)
        m_normalFlowList->clear();
    m_normalFlowListDirty = true;

#if USE(ACCELERATED_COMPOSITING)
    if (!renderer()->documentBeingDestroyed())
        compositor()->setCompositingLayersNeedRebuild();
#endif
}

void RenderLayer::updateZOrderLists()
{
    if (!isStackingContext() || !m_zOrderListsDirty)
        return;

    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        if (!m_reflection || reflectionLayer() != child)
            child->collectLayers(m_posZOrderList, m_negZOrderList);

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

void RenderLayer::collectLayers(Vector<RenderLayer*>*& posBuffer, Vector<RenderLayer*>*& negBuffer)
{
    updateVisibilityStatus();

    // Overflow layers are just painted by their enclosing layers, so they don't get put in zorder lists.
    if ((m_hasVisibleContent || (m_hasVisibleDescendant && isStackingContext())) && !isNormalFlowOnly()) {
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
    if (m_hasVisibleDescendant && !isStackingContext()) {
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            // Ignore reflections.
            if (!m_reflection || reflectionLayer() != child)
                child->collectLayers(posBuffer, negBuffer);
        }
    }
}

void RenderLayer::updateLayerListsIfNeeded()
{
    updateZOrderLists();
    updateNormalFlowList();
}

void RenderLayer::updateCompositingAndLayerListsIfNeeded()
{
#if USE(ACCELERATED_COMPOSITING)
    if (compositor()->inCompositingMode()) {
        if ((isStackingContext() && m_zOrderListsDirty) || m_normalFlowListDirty)
            compositor()->updateCompositingLayers(this);
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
    if (backing()->paintingGoesToWindow()) {
        // If we're trying to repaint the placeholder document layer, propagate the
        // repaint to the native view system.
        RenderView* view = renderer()->view();
        if (view)
            view->repaintViewRectangle(absoluteBoundingBox());
    } else
        backing()->setContentsNeedDisplay();
}

void RenderLayer::setBackingNeedsRepaintInRect(const IntRect& r)
{
    ASSERT(isComposited());
    if (backing()->paintingGoesToWindow()) {
        // If we're trying to repaint the placeholder document layer, propagate the
        // repaint to the native view system.
        IntRect absRect(r);
        int x = 0;
        int y = 0;
        convertToLayerCoords(root(), x, y);
        absRect.move(x, y);

        RenderView* view = renderer()->view();
        if (view)
            view->repaintViewRectangle(absRect);
    } else
        backing()->setContentsNeedDisplayInRect(r);
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
    return (renderer()->hasOverflowClip() || renderer()->hasReflection() || renderer()->hasMask() || renderer()->isVideo()) &&
           !renderer()->isPositioned() &&
           !renderer()->isRelPositioned() &&
           !renderer()->hasTransform() &&
           !isTransparent();
}

bool RenderLayer::isSelfPaintingLayer() const
{
    return !isNormalFlowOnly() || renderer()->hasReflection() || renderer()->hasMask() || renderer()->isTableRow() || renderer()->isVideo();
}

void RenderLayer::styleChanged(StyleDifference diff, const RenderStyle*)
{
    bool isNormalFlowOnly = shouldBeNormalFlowOnly();
    if (isNormalFlowOnly != m_isNormalFlowOnly) {
        m_isNormalFlowOnly = isNormalFlowOnly;
        RenderLayer* p = parent();
        if (p)
            p->dirtyNormalFlowList();
        dirtyStackingContextZOrderLists();
    }

    if (renderer()->style()->overflowX() == OMARQUEE && renderer()->style()->marqueeBehavior() != MNONE) {
        if (!m_marquee)
            m_marquee = new RenderMarquee(this);
        m_marquee->updateMarqueeStyle();
    }
    else if (m_marquee) {
        delete m_marquee;
        m_marquee = 0;
    }
    
    if (!hasReflection() && m_reflection) {
        m_reflection->destroy();
        m_reflection = 0;
    } else if (hasReflection()) {
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

#if USE(ACCELERATED_COMPOSITING)
    updateTransform();

    if (compositor()->updateLayerCompositingState(this))
        compositor()->setCompositingLayersNeedRebuild();
    else if (m_backing)
        m_backing->updateGraphicsLayerGeometry();

    if (m_backing && diff >= StyleDifferenceRepaint)
        m_backing->setContentsNeedDisplay();
#else
    UNUSED_PARAM(diff);
#endif
}

void RenderLayer::updateScrollCornerStyle()
{
    RenderObject* actualRenderer = renderer()->node() ? renderer()->node()->shadowAncestorNode()->renderer() : renderer();
    RefPtr<RenderStyle> corner = renderer()->hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(SCROLLBAR_CORNER, actualRenderer->style()) : 0;
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
    RefPtr<RenderStyle> resizer = renderer()->hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(RESIZER, actualRenderer->style()) : 0;
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

} // namespace WebCore
