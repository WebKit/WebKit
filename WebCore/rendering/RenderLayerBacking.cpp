/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AnimationController.h"
#include "CanvasRenderingContext3D.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "HTMLCanvasElement.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "RenderBox.h"
#include "RenderImage.h"
#include "RenderLayerCompositor.h"
#include "RenderVideo.h"
#include "RenderView.h"

#include "RenderLayerBacking.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

static bool hasBorderOutlineOrShadow(const RenderStyle*);
static bool hasBoxDecorations(const RenderStyle*);
static bool hasBoxDecorationsWithBackgroundImage(const RenderStyle*);

RenderLayerBacking::RenderLayerBacking(RenderLayer* layer)
    : m_owningLayer(layer)
    , m_hasDirectlyCompositedContent(false)
    , m_artificiallyInflatedBounds(false)
{
    createGraphicsLayer();
}

RenderLayerBacking::~RenderLayerBacking()
{
    updateClippingLayers(false, false);
    updateForegroundLayer(false);
    updateMaskLayer(false);
    destroyGraphicsLayer();
}

void RenderLayerBacking::createGraphicsLayer()
{
    m_graphicsLayer = GraphicsLayer::create(this);
    
#ifndef NDEBUG
    if (renderer()->node()) {
        if (renderer()->node()->isDocumentNode())
            m_graphicsLayer->setName("Document Node");
        else {
            if (renderer()->node()->isHTMLElement() && renderer()->node()->hasID())
                m_graphicsLayer->setName(renderer()->renderName() + String(" ") + static_cast<HTMLElement*>(renderer()->node())->getAttribute(idAttr));
            else
                m_graphicsLayer->setName(renderer()->renderName());
        }
    } else
        m_graphicsLayer->setName("Anonymous Node");
#endif  // NDEBUG

    updateLayerOpacity();
    updateLayerTransform();
}

void RenderLayerBacking::destroyGraphicsLayer()
{
    if (m_graphicsLayer)
        m_graphicsLayer->removeFromParent();

    m_graphicsLayer = 0;
    m_foregroundLayer = 0;
    m_clippingLayer = 0;
    m_maskLayer = 0;
}

void RenderLayerBacking::updateLayerOpacity()
{
    m_graphicsLayer->setOpacity(compositingOpacity(renderer()->opacity()));
}

void RenderLayerBacking::updateLayerTransform()
{
    RenderStyle* style = renderer()->style();

    // FIXME: This could use m_owningLayer->transform(), but that currently has transform-origin
    // baked into it, and we don't want that.
    TransformationMatrix t;
    if (m_owningLayer->hasTransform()) {
        style->applyTransform(t, toRenderBox(renderer())->borderBoxRect().size(), RenderStyle::ExcludeTransformOrigin);
        makeMatrixRenderable(t, compositor()->hasAcceleratedCompositing());
    }
    
    m_graphicsLayer->setTransform(t);
}

static bool hasNonZeroTransformOrigin(const RenderObject* renderer)
{
    RenderStyle* style = renderer->style();
    return (style->transformOriginX().type() == Fixed && style->transformOriginX().value())
        || (style->transformOriginY().type() == Fixed && style->transformOriginY().value());
}

void RenderLayerBacking::updateCompositedBounds()
{
    IntRect layerBounds = compositor()->calculateCompositedBounds(m_owningLayer, m_owningLayer);

    // If the element has a transform-origin that has fixed lengths, and the renderer has zero size,
    // then we need to ensure that the compositing layer has non-zero size so that we can apply
    // the transform-origin via the GraphicsLayer anchorPoint (which is expressed as a fractional value).
    if (layerBounds.isEmpty() && hasNonZeroTransformOrigin(renderer())) {
        layerBounds.setWidth(1);
        layerBounds.setHeight(1);
        m_artificiallyInflatedBounds = true;
    } else
        m_artificiallyInflatedBounds = false;

    setCompositedBounds(layerBounds);
}

void RenderLayerBacking::updateAfterLayout(UpdateDepth updateDepth)
{
    RenderLayerCompositor* layerCompositor = compositor();
    if (!layerCompositor->compositingLayersNeedRebuild()) {
        // Calling updateGraphicsLayerGeometry() here gives incorrect results, because the
        // position of this layer's GraphicsLayer depends on the position of our compositing
        // ancestor's GraphicsLayer. That cannot be determined until all the descendant 
        // RenderLayers of that ancestor have been processed via updateLayerPositions().
        //
        // The solution is to update compositing children of this layer here,
        // via updateCompositingChildrenGeometry().
        updateCompositedBounds();
        layerCompositor->updateCompositingDescendantGeometry(m_owningLayer, m_owningLayer, updateDepth);
        
        if (!m_owningLayer->parent()) {
            updateGraphicsLayerGeometry();
            layerCompositor->updateRootLayerPosition();
        }
    }
}

bool RenderLayerBacking::updateGraphicsLayerConfiguration()
{
    RenderLayerCompositor* compositor = this->compositor();

    bool layerConfigChanged = false;
    if (updateForegroundLayer(compositor->needsContentsCompositingLayer(m_owningLayer)))
        layerConfigChanged = true;
    
    if (updateClippingLayers(compositor->clippedByAncestor(m_owningLayer), compositor->clipsCompositingDescendants(m_owningLayer)))
        layerConfigChanged = true;

    if (updateMaskLayer(m_owningLayer->renderer()->hasMask()))
        m_graphicsLayer->setMaskLayer(m_maskLayer.get());

    m_hasDirectlyCompositedContent = false;
    if (canUseDirectCompositing()) {
        if (renderer()->isImage()) {
            updateImageContents();
            m_hasDirectlyCompositedContent = true;
            m_graphicsLayer->setDrawsContent(false);
        }
#if ENABLE(3D_CANVAS)    
        else if (renderer()->isCanvas()) {
            HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(renderer()->node());
            if (canvas->is3D()) {
                CanvasRenderingContext3D* context = static_cast<CanvasRenderingContext3D*>(canvas->renderingContext());
                if (context->graphicsContext3D()->platformGraphicsContext3D())
                    m_graphicsLayer->setContentsToGraphicsContext3D(context->graphicsContext3D());
            }
        }
#endif

        if (rendererHasBackground())
            m_graphicsLayer->setBackgroundColor(rendererBackgroundColor());
        else
            m_graphicsLayer->clearBackgroundColor();
    }

    return layerConfigChanged;
}

void RenderLayerBacking::updateGraphicsLayerGeometry()
{
    // If we haven't built z-order lists yet, wait until later.
    if (m_owningLayer->isStackingContext() && m_owningLayer->m_zOrderListsDirty)
        return;

    // Set transform property, if it is not animating. We have to do this here because the transform
    // is affected by the layer dimensions.
    if (!renderer()->animation()->isAnimatingPropertyOnRenderer(renderer(), CSSPropertyWebkitTransform))
        updateLayerTransform();

    // Set opacity, if it is not animating.
    if (!renderer()->animation()->isAnimatingPropertyOnRenderer(renderer(), CSSPropertyOpacity))
        updateLayerOpacity();
    
    RenderStyle* style = renderer()->style();
    m_graphicsLayer->setPreserves3D(style->transformStyle3D() == TransformStyle3DPreserve3D);
    m_graphicsLayer->setBackfaceVisibility(style->backfaceVisibility() == BackfaceVisibilityVisible);

    RenderLayer* compAncestor = m_owningLayer->ancestorCompositingLayer();
    
    // We compute everything relative to the enclosing compositing layer.
    IntRect ancestorCompositingBounds;
    if (compAncestor) {
        ASSERT(compAncestor->backing());
        ancestorCompositingBounds = compAncestor->backing()->compositedBounds();
    }

    IntRect localCompositingBounds = compositedBounds();

    IntRect relativeCompositingBounds(localCompositingBounds);
    int deltaX = 0, deltaY = 0;
    m_owningLayer->convertToLayerCoords(compAncestor, deltaX, deltaY);
    relativeCompositingBounds.move(deltaX, deltaY);

    IntPoint graphicsLayerParentLocation;
    if (compAncestor && compAncestor->backing()->hasClippingLayer()) {
        // If the compositing ancestor has a layer to clip children, we parent in that, and therefore
        // position relative to it.
        graphicsLayerParentLocation = toRenderBox(compAncestor->renderer())->overflowClipRect(0, 0).location();
    } else
        graphicsLayerParentLocation = ancestorCompositingBounds.location();
    
    if (compAncestor && m_ancestorClippingLayer) {
        // Call calculateRects to get the backgroundRect which is what is used to clip the contents of this
        // layer. Note that we call it with temporaryClipRects = true because normally when computing clip rects
        // for a compositing layer, rootLayer is the layer itself.
        IntRect parentClipRect = m_owningLayer->backgroundClipRect(compAncestor, true);
        m_ancestorClippingLayer->setPosition(FloatPoint() + (parentClipRect.location() - graphicsLayerParentLocation));
        m_ancestorClippingLayer->setSize(parentClipRect.size());

        // backgroundRect is relative to compAncestor, so subtract deltaX/deltaY to get back to local coords.
        IntSize rendererOffset(parentClipRect.location().x() - deltaX, parentClipRect.location().y() - deltaY);
        m_ancestorClippingLayer->setOffsetFromRenderer(rendererOffset);

        // The primary layer is then parented in, and positioned relative to this clipping layer.
        graphicsLayerParentLocation = parentClipRect.location();
    }

    m_graphicsLayer->setPosition(FloatPoint() + (relativeCompositingBounds.location() - graphicsLayerParentLocation));
    m_graphicsLayer->setOffsetFromRenderer(localCompositingBounds.location() - IntPoint());
    
    FloatSize oldSize = m_graphicsLayer->size();
    FloatSize newSize = relativeCompositingBounds.size();
    if (oldSize != newSize) {
        m_graphicsLayer->setSize(newSize);
        // A bounds change will almost always require redisplay. Usually that redisplay
        // will happen because of a repaint elsewhere, but not always:
        // e.g. see RenderView::setMaximalOutlineSize()
        m_graphicsLayer->setNeedsDisplay();
    }

    // If we have a layer that clips children, position it.
    IntRect clippingBox;
    if (m_clippingLayer) {
        clippingBox = toRenderBox(renderer())->overflowClipRect(0, 0);
        m_clippingLayer->setPosition(FloatPoint() + (clippingBox.location() - localCompositingBounds.location()));
        m_clippingLayer->setSize(clippingBox.size());
        m_clippingLayer->setOffsetFromRenderer(clippingBox.location() - IntPoint());
    }
    
    if (m_maskLayer) {
        m_maskLayer->setSize(m_graphicsLayer->size());
        m_maskLayer->setPosition(FloatPoint());
    }
    
    if (m_owningLayer->hasTransform()) {
        const IntRect borderBox = toRenderBox(renderer())->borderBoxRect();

        // Get layout bounds in the coords of compAncestor to match relativeCompositingBounds.
        IntRect layerBounds = IntRect(deltaX, deltaY, borderBox.width(), borderBox.height());

        // Update properties that depend on layer dimensions
        FloatPoint3D transformOrigin = computeTransformOrigin(borderBox);
        // Compute the anchor point, which is in the center of the renderer box unless transform-origin is set.
        FloatPoint3D anchor(relativeCompositingBounds.width()  != 0.0f ? ((layerBounds.x() - relativeCompositingBounds.x()) + transformOrigin.x()) / relativeCompositingBounds.width()  : 0.5f,
                            relativeCompositingBounds.height() != 0.0f ? ((layerBounds.y() - relativeCompositingBounds.y()) + transformOrigin.y()) / relativeCompositingBounds.height() : 0.5f,
                            transformOrigin.z());
        m_graphicsLayer->setAnchorPoint(anchor);

        RenderStyle* style = renderer()->style();
        if (style->hasPerspective()) {
            TransformationMatrix t = owningLayer()->perspectiveTransform();
            
            if (m_clippingLayer) {
                m_clippingLayer->setChildrenTransform(t);
                m_graphicsLayer->setChildrenTransform(TransformationMatrix());
            }
            else
                m_graphicsLayer->setChildrenTransform(t);
        } else {
            if (m_clippingLayer)
                m_clippingLayer->setChildrenTransform(TransformationMatrix());
            else
                m_graphicsLayer->setChildrenTransform(TransformationMatrix());
        }
    } else {
        m_graphicsLayer->setAnchorPoint(FloatPoint3D(0.5f, 0.5f, 0));
    }

    if (m_foregroundLayer) {
        FloatPoint foregroundPosition;
        FloatSize foregroundSize = newSize;
        IntSize foregroundOffset = m_graphicsLayer->offsetFromRenderer();
        // If we have a clipping layer (which clips descendants), then the foreground layer is a child of it,
        // so that it gets correctly sorted with children. In that case, position relative to the clipping layer.
        if (m_clippingLayer) {
            foregroundPosition = FloatPoint() + (localCompositingBounds.location() - clippingBox.location());
            foregroundSize = FloatSize(clippingBox.size());
            foregroundOffset = clippingBox.location() - IntPoint();
        }

        m_foregroundLayer->setPosition(foregroundPosition);
        m_foregroundLayer->setSize(foregroundSize);
        m_foregroundLayer->setOffsetFromRenderer(foregroundOffset);
    }

    m_graphicsLayer->setContentsRect(contentsBox());
    if (!m_hasDirectlyCompositedContent)
        m_graphicsLayer->setDrawsContent(!isSimpleContainerCompositingLayer() && !paintingGoesToWindow() && !m_artificiallyInflatedBounds);
}

void RenderLayerBacking::updateInternalHierarchy()
{
    // m_foregroundLayer has to be inserted in the correct order with child layers,
    // so it's not inserted here.
    if (m_ancestorClippingLayer) {
        m_ancestorClippingLayer->removeAllChildren();
        m_graphicsLayer->removeFromParent();
        m_ancestorClippingLayer->addChild(m_graphicsLayer.get());
    }

    if (m_clippingLayer) {
        m_clippingLayer->removeFromParent();
        m_graphicsLayer->addChild(m_clippingLayer.get());
    }
}

// Return true if the layers changed.
bool RenderLayerBacking::updateClippingLayers(bool needsAncestorClip, bool needsDescendantClip)
{
    bool layersChanged = false;

    if (needsAncestorClip) {
        if (!m_ancestorClippingLayer) {
            m_ancestorClippingLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
            m_ancestorClippingLayer->setName("Ancestor clipping Layer");
#endif
            m_ancestorClippingLayer->setMasksToBounds(true);
            layersChanged = true;
        }
    } else if (m_ancestorClippingLayer) {
        m_ancestorClippingLayer->removeFromParent();
        m_ancestorClippingLayer = 0;
        layersChanged = true;
    }
    
    if (needsDescendantClip) {
        if (!m_clippingLayer) {
            m_clippingLayer = GraphicsLayer::create(0);
#ifndef NDEBUG
            m_clippingLayer->setName("Child clipping Layer");
#endif
            m_clippingLayer->setMasksToBounds(true);
            layersChanged = true;
        }
    } else if (m_clippingLayer) {
        m_clippingLayer->removeFromParent();
        m_clippingLayer = 0;
        layersChanged = true;
    }
    
    if (layersChanged)
        updateInternalHierarchy();

    return layersChanged;
}

bool RenderLayerBacking::updateForegroundLayer(bool needsForegroundLayer)
{
    bool layerChanged = false;
    if (needsForegroundLayer) {
        if (!m_foregroundLayer) {
            m_foregroundLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
            m_foregroundLayer->setName("Foreground");
#endif
            m_foregroundLayer->setDrawsContent(true);
            m_foregroundLayer->setPaintingPhase(GraphicsLayerPaintForeground);
            layerChanged = true;
        }
    } else if (m_foregroundLayer) {
        m_foregroundLayer->removeFromParent();
        m_foregroundLayer = 0;
        layerChanged = true;
    }

    if (layerChanged)
        m_graphicsLayer->setPaintingPhase(paintingPhaseForPrimaryLayer());

    return layerChanged;
}

bool RenderLayerBacking::updateMaskLayer(bool needsMaskLayer)
{
    bool layerChanged = false;
    if (needsMaskLayer) {
        if (!m_maskLayer) {
            m_maskLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
            m_maskLayer->setName("Mask");
#endif
            m_maskLayer->setDrawsContent(true);
            m_maskLayer->setPaintingPhase(GraphicsLayerPaintMask);
            layerChanged = true;
        }
    } else if (m_maskLayer) {
        m_maskLayer = 0;
        layerChanged = true;
    }

    if (layerChanged)
        m_graphicsLayer->setPaintingPhase(paintingPhaseForPrimaryLayer());

    return layerChanged;
}

GraphicsLayerPaintingPhase RenderLayerBacking::paintingPhaseForPrimaryLayer() const
{
    unsigned phase = GraphicsLayerPaintBackground;
    if (!m_foregroundLayer)
        phase |= GraphicsLayerPaintForeground;
    if (!m_maskLayer)
        phase |= GraphicsLayerPaintMask;

    return static_cast<GraphicsLayerPaintingPhase>(phase);
}

float RenderLayerBacking::compositingOpacity(float rendererOpacity) const
{
    float finalOpacity = rendererOpacity;
    
    for (RenderLayer* curr = m_owningLayer->parent(); curr; curr = curr->parent()) {
        // We only care about parents that are stacking contexts.
        // Recall that opacity creates stacking context.
        if (!curr->isStackingContext())
            continue;
        
        // If we found a compositing layer, we want to compute opacity
        // relative to it. So we can break here.
        if (curr->isComposited())
            break;
        
        finalOpacity *= curr->renderer()->opacity();
    }

    return finalOpacity;
}

static bool hasBorderOutlineOrShadow(const RenderStyle* style)
{
    return style->hasBorder() || style->hasBorderRadius() || style->hasOutline() || style->hasAppearance() || style->boxShadow();
}

static bool hasBoxDecorations(const RenderStyle* style)
{
    return hasBorderOutlineOrShadow(style) || style->hasBackground();
}

static bool hasBoxDecorationsWithBackgroundImage(const RenderStyle* style)
{
    return hasBorderOutlineOrShadow(style) || style->hasBackgroundImage();
}

bool RenderLayerBacking::rendererHasBackground() const
{
    // FIXME: share more code here
    if (renderer()->node() && renderer()->node()->isDocumentNode()) {
        RenderObject* htmlObject = renderer()->firstChild();
        if (!htmlObject)
            return false;
        
        RenderStyle* style = htmlObject->style();
        if (style->hasBackground())
            return true;
        
        RenderObject* bodyObject = htmlObject->firstChild();
        if (!bodyObject)
            return false;
        
        style = bodyObject->style();
        return style->hasBackground();
    }
    
    return renderer()->style()->hasBackground();
}

const Color& RenderLayerBacking::rendererBackgroundColor() const
{
    // FIXME: share more code here
    if (renderer()->node() && renderer()->node()->isDocumentNode()) {
        RenderObject* htmlObject = renderer()->firstChild();
        RenderStyle* style = htmlObject->style();
        if (style->hasBackground())
            return style->backgroundColor();

        RenderObject* bodyObject = htmlObject->firstChild();
        style = bodyObject->style();
        return style->backgroundColor();
    }

    return renderer()->style()->backgroundColor();
}

// A "simple container layer" is a RenderLayer which has no visible content to render.
// It may have no children, or all its children may be themselves composited.
// This is a useful optimization, because it allows us to avoid allocating backing store.
bool RenderLayerBacking::isSimpleContainerCompositingLayer() const
{
    RenderObject* renderObject = renderer();
    if (renderObject->isReplaced() ||       // replaced objects are not containers
        renderObject->hasMask())            // masks require special treatment
        return false;

    RenderStyle* style = renderObject->style();

    // Reject anything that has a border, a border-radius or outline,
    // or any background (color or image).
    // FIXME: we could optimize layers for simple backgrounds.
    if (hasBoxDecorations(style))
        return false;

    // If we have got this far and the renderer has no children, then we're ok.
    if (!renderObject->firstChild())
        return true;
    
    if (renderObject->node() && renderObject->node()->isDocumentNode()) {
        // Look to see if the root object has a non-simple backgound
        RenderObject* rootObject = renderObject->document()->documentElement()->renderer();
        if (!rootObject)
            return false;
        
        style = rootObject->style();
        
        // Reject anything that has a border, a border-radius or outline,
        // or is not a simple background (no background, or solid color).
        if (hasBoxDecorationsWithBackgroundImage(style))
            return false;
        
        // Now look at the body's renderer.
        HTMLElement* body = renderObject->document()->body();
        RenderObject* bodyObject = (body && body->hasLocalName(bodyTag)) ? body->renderer() : 0;
        if (!bodyObject)
            return false;
        
        style = bodyObject->style();
        
        if (hasBoxDecorationsWithBackgroundImage(style))
            return false;

        // Ceck to see if all the body's children are compositing layers.
        if (hasNonCompositingContent())
            return false;
        
        return true;
    }

    // Check to see if all the renderer's children are compositing layers.
    if (hasNonCompositingContent())
        return false;
    
    return true;
}

bool RenderLayerBacking::hasNonCompositingContent() const
{
    // Conservative test for having no rendered children.
    
    // Some HTML can cause whitespace text nodes to have renderers, like:
    // <div>
    // <img src=...>
    // </div>
    // so test for 0x0 RenderTexts here
    for (RenderObject* child = renderer()->firstChild(); child; child = child->nextSibling()) {
        if (!child->hasLayer()) {
            if (child->isRenderInline() || !child->isBox())
                return true;
            
            if (toRenderBox(child)->width() > 0 || toRenderBox(child)->height() > 0)
                return true;
        }
    }

    // FIXME: test for overflow controls.
    if (m_owningLayer->isStackingContext()) {
        // Use the m_hasCompositingDescendant bit to optimize?
        if (Vector<RenderLayer*>* negZOrderList = m_owningLayer->negZOrderList()) {
            size_t listSize = negZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = negZOrderList->at(i);
                if (!curLayer->isComposited())
                    return true;
            }
        }

        if (Vector<RenderLayer*>* posZOrderList = m_owningLayer->posZOrderList()) {
            size_t listSize = posZOrderList->size();
            for (size_t i = 0; i < listSize; ++i) {
                RenderLayer* curLayer = posZOrderList->at(i);
                if (!curLayer->isComposited())
                    return true;
            }
        }
    }

    if (Vector<RenderLayer*>* normalFlowList = m_owningLayer->normalFlowList()) {
        size_t listSize = normalFlowList->size();
        for (size_t i = 0; i < listSize; ++i) {
            RenderLayer* curLayer = normalFlowList->at(i);
            if (!curLayer->isComposited())
                return true;
        }
    }

    return false;
}

// A layer can use direct compositing if the render layer's object is a replaced object and has no children.
// This allows the GraphicsLayer to display the RenderLayer contents directly; it's used for images.
bool RenderLayerBacking::canUseDirectCompositing() const
{
    RenderObject* renderObject = renderer();
    
    // Canvas3D is always direct composited
#if ENABLE(3D_CANVAS)    
    if (renderer()->isCanvas()) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(renderer()->node());
        return canvas->is3D();
    }
#endif

    // Reject anything that isn't an image
    if (!renderObject->isImage() && !renderObject->isVideo())
        return false;
    
    if (renderObject->hasMask() || renderObject->hasReflection())
        return false;

    // Video can use an inner layer even if it has box decorations; we draw those into another layer.
    if (renderObject->isVideo())
        return true;
    
    // Reject anything that would require the image to be drawn via the GraphicsContext,
    // like border, shadows etc. Solid background color is OK.
    return !hasBoxDecorationsWithBackgroundImage(renderObject->style());
}
    
void RenderLayerBacking::rendererContentChanged()
{
    if (canUseDirectCompositing()) {
        if (renderer()->isImage())
            updateImageContents();
        else {
#if ENABLE(3D_CANVAS)    
            if (renderer()->isCanvas()) {
                HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(renderer()->node());
                if (canvas->is3D())
                    m_graphicsLayer->setGraphicsContext3DNeedsDisplay();
            }
#endif
        }
    }
}

void RenderLayerBacking::updateImageContents()
{
    ASSERT(renderer()->isImage());
    RenderImage* imageRenderer = toRenderImage(renderer());

    CachedImage* cachedImage = imageRenderer->cachedImage();
    if (!cachedImage)
        return;

    Image* image = cachedImage->image();
    if (!image)
        return;

    // We have to wait until the image is fully loaded before setting it on the layer.
    if (!cachedImage->isLoaded())
        return;

    // This is a no-op if the layer doesn't have an inner layer for the image.
    m_graphicsLayer->setContentsToImage(image);
    
    // Image animation is "lazy", in that it automatically stops unless someone is drawing
    // the image. So we have to kick the animation each time; this has the downside that the
    // image will keep animating, even if its layer is not visible.
    image->startAnimation();
}

FloatPoint3D RenderLayerBacking::computeTransformOrigin(const IntRect& borderBox) const
{
    RenderStyle* style = renderer()->style();

    FloatPoint3D origin;
    origin.setX(style->transformOriginX().calcFloatValue(borderBox.width()));
    origin.setY(style->transformOriginY().calcFloatValue(borderBox.height()));
    origin.setZ(style->transformOriginZ());

    return origin;
}

FloatPoint RenderLayerBacking::computePerspectiveOrigin(const IntRect& borderBox) const
{
    RenderStyle* style = renderer()->style();

    float boxWidth = borderBox.width();
    float boxHeight = borderBox.height();

    FloatPoint origin;
    origin.setX(style->perspectiveOriginX().calcFloatValue(boxWidth));
    origin.setY(style->perspectiveOriginY().calcFloatValue(boxHeight));

    return origin;
}

// Return the offset from the top-left of this compositing layer at which the renderer's contents are painted.
IntSize RenderLayerBacking::contentOffsetInCompostingLayer() const
{
    return IntSize(-m_compositedBounds.x(), -m_compositedBounds.y());
}

IntRect RenderLayerBacking::contentsBox() const
{
    if (!renderer()->isBox())
        return IntRect();

    IntRect contentsRect;
#if ENABLE(VIDEO)
    if (renderer()->isVideo()) {
        RenderVideo* videoRenderer = toRenderVideo(renderer());
        contentsRect = videoRenderer->videoBox();
    } else
#endif
        contentsRect = toRenderBox(renderer())->contentBoxRect();

    IntSize contentOffset = contentOffsetInCompostingLayer();
    contentsRect.move(contentOffset);
    return contentsRect;
}

// Map the given point from coordinates in the GraphicsLayer to RenderLayer coordinates.
FloatPoint RenderLayerBacking::graphicsLayerToContentsCoordinates(const GraphicsLayer* graphicsLayer, const FloatPoint& point)
{
    return point + FloatSize(graphicsLayer->offsetFromRenderer());
}

// Map the given point from coordinates in the RenderLayer to GraphicsLayer coordinates.
FloatPoint RenderLayerBacking::contentsToGraphicsLayerCoordinates(const GraphicsLayer* graphicsLayer, const FloatPoint& point)
{
    return point - FloatSize(graphicsLayer->offsetFromRenderer());
}

bool RenderLayerBacking::paintingGoesToWindow() const
{
    return m_owningLayer->isRootLayer();
}

void RenderLayerBacking::setContentsNeedDisplay()
{
    if (m_graphicsLayer && m_graphicsLayer->drawsContent())
        m_graphicsLayer->setNeedsDisplay();
    
    if (m_foregroundLayer && m_foregroundLayer->drawsContent())
        m_foregroundLayer->setNeedsDisplay();

    if (m_maskLayer && m_maskLayer->drawsContent())
        m_maskLayer->setNeedsDisplay();
}

// r is in the coordinate space of the layer's render object
void RenderLayerBacking::setContentsNeedDisplayInRect(const IntRect& r)
{
    if (m_graphicsLayer && m_graphicsLayer->drawsContent()) {
        FloatPoint dirtyOrigin = contentsToGraphicsLayerCoordinates(m_graphicsLayer.get(), FloatPoint(r.x(), r.y()));
        FloatRect dirtyRect(dirtyOrigin, r.size());
        FloatRect bounds(FloatPoint(), m_graphicsLayer->size());
        if (bounds.intersects(dirtyRect))
            m_graphicsLayer->setNeedsDisplayInRect(dirtyRect);
    }

    if (m_foregroundLayer && m_foregroundLayer->drawsContent()) {
        // FIXME: do incremental repaint
        m_foregroundLayer->setNeedsDisplay();
    }

    if (m_maskLayer && m_maskLayer->drawsContent()) {
        // FIXME: do incremental repaint
        m_maskLayer->setNeedsDisplay();
    }
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

// Share this with RenderLayer::paintLayer, which would have to be educated about GraphicsLayerPaintingPhase?
void RenderLayerBacking::paintIntoLayer(RenderLayer* rootLayer, GraphicsContext* context,
                    const IntRect& paintDirtyRect,      // in the coords of rootLayer
                    PaintRestriction paintRestriction, GraphicsLayerPaintingPhase paintingPhase,
                    RenderObject* paintingRoot)
{
    if (paintingGoesToWindow()) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    m_owningLayer->updateLayerListsIfNeeded();
    
    // Paint the reflection first if we have one.
    if (m_owningLayer->hasReflection()) {
        // Mark that we are now inside replica painting.
        m_owningLayer->setPaintingInsideReflection(true);
        m_owningLayer->reflectionLayer()->paintLayer(rootLayer, context, paintDirtyRect, paintRestriction, paintingRoot, 0, RenderLayer::PaintLayerPaintingReflection);
        m_owningLayer->setPaintingInsideReflection(false);
    }

    // Calculate the clip rects we should use.
    IntRect layerBounds, damageRect, clipRectToApply, outlineRect;
    m_owningLayer->calculateRects(rootLayer, paintDirtyRect, layerBounds, damageRect, clipRectToApply, outlineRect);
    
    int x = layerBounds.x();        // layerBounds is computed relative to rootLayer
    int y = layerBounds.y();
    int tx = x - m_owningLayer->renderBoxX();
    int ty = y - m_owningLayer->renderBoxY();

    // If this layer's renderer is a child of the paintingRoot, we render unconditionally, which
    // is done by passing a nil paintingRoot down to our renderer (as if no paintingRoot was ever set).
    // Else, our renderer tree may or may not contain the painting root, so we pass that root along
    // so it will be tested against as we decend through the renderers.
    RenderObject *paintingRootForRenderer = 0;
    if (paintingRoot && !renderer()->isDescendantOf(paintingRoot))
        paintingRootForRenderer = paintingRoot;

    bool shouldPaint = (m_owningLayer->hasVisibleContent() || m_owningLayer->hasVisibleDescendant()) && m_owningLayer->isSelfPaintingLayer();

    if (shouldPaint && (paintingPhase & GraphicsLayerPaintBackground)) {
        // If this is the root then we need to send in a bigger bounding box
        // because we'll be painting the background as well (see RenderBox::paintRootBoxDecorations()).
        IntRect paintBox = clipRectToApply;
        
        // FIXME: do we need this code?
        if (renderer()->node() && renderer()->node()->isDocumentNode() && renderer()->document()->isHTMLDocument()) {
            RenderBox* box = toRenderBox(renderer());
            int w = box->width();
            int h = box->height();
            
            int rw;
            int rh;
            if (box->view()->frameView()) {
                rw = box->view()->frameView()->contentsWidth();
                rh = box->view()->frameView()->contentsHeight();
            } else {
                rw = box->view()->width();
                rh = box->view()->height();
            }
            
            int bx = tx - box->marginLeft();
            int by = ty - box->marginTop();
            int bw = max(w + box->marginLeft() + box->marginRight() + box->borderLeft() + box->borderRight(), rw);
            int bh = max(h + box->marginTop() + box->marginBottom() + box->borderTop() + box->borderBottom(), rh);
            paintBox = IntRect(bx, by, bw, bh);
        }

        // Paint our background first, before painting any child layers.
        // Establish the clip used to paint our background.
        setClip(context, paintDirtyRect, damageRect);
        
        RenderObject::PaintInfo info(context, paintBox, PaintPhaseBlockBackground, false, paintingRootForRenderer, 0);
        renderer()->paint(info, tx, ty);

        // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
        // z-index.  We paint after we painted the background/border, so that the scrollbars will
        // sit above the background/border.
        m_owningLayer->paintOverflowControls(context, x, y, damageRect);
        
        // Restore the clip.
        restoreClip(context, paintDirtyRect, damageRect);
    }
                
    bool forceBlackText = paintRestriction == PaintRestrictionSelectionOnlyBlackText;
    bool selectionOnly  = paintRestriction == PaintRestrictionSelectionOnly || paintRestriction == PaintRestrictionSelectionOnlyBlackText;

    if (shouldPaint && (paintingPhase & GraphicsLayerPaintForeground)) {
        // Now walk the sorted list of children with negative z-indices. Only RenderLayers without compositing layers will paint.
        // FIXME: should these be painted as background?
        Vector<RenderLayer*>* negZOrderList = m_owningLayer->negZOrderList();
        if (negZOrderList) {
            for (Vector<RenderLayer*>::iterator it = negZOrderList->begin(); it != negZOrderList->end(); ++it)
                it[0]->paintLayer(rootLayer, context, paintDirtyRect, paintRestriction, paintingRoot);
        }

        // Set up the clip used when painting our children.
        setClip(context, paintDirtyRect, clipRectToApply);
        RenderObject::PaintInfo paintInfo(context, clipRectToApply, 
                                          selectionOnly ? PaintPhaseSelection : PaintPhaseChildBlockBackgrounds,
                                          forceBlackText, paintingRootForRenderer, 0);
        renderer()->paint(paintInfo, tx, ty);

        if (!selectionOnly) {
            paintInfo.phase = PaintPhaseFloat;
            renderer()->paint(paintInfo, tx, ty);

            paintInfo.phase = PaintPhaseForeground;
            renderer()->paint(paintInfo, tx, ty);

            paintInfo.phase = PaintPhaseChildOutlines;
            renderer()->paint(paintInfo, tx, ty);
        }

        // Now restore our clip.
        restoreClip(context, paintDirtyRect, clipRectToApply);

        if (!outlineRect.isEmpty()) {
            // Paint our own outline
            RenderObject::PaintInfo paintInfo(context, outlineRect, PaintPhaseSelfOutline, false, paintingRootForRenderer, 0);
            setClip(context, paintDirtyRect, outlineRect);
            renderer()->paint(paintInfo, tx, ty);
            restoreClip(context, paintDirtyRect, outlineRect);
        }

        // Paint any child layers that have overflow.
        Vector<RenderLayer*>* normalFlowList = m_owningLayer->normalFlowList();
        if (normalFlowList) {
            for (Vector<RenderLayer*>::iterator it = normalFlowList->begin(); it != normalFlowList->end(); ++it)
                it[0]->paintLayer(rootLayer, context, paintDirtyRect, paintRestriction, paintingRoot);
        }

        // Now walk the sorted list of children with positive z-indices.
        Vector<RenderLayer*>* posZOrderList = m_owningLayer->posZOrderList();
        if (posZOrderList) {
            for (Vector<RenderLayer*>::iterator it = posZOrderList->begin(); it != posZOrderList->end(); ++it)
                it[0]->paintLayer(rootLayer, context, paintDirtyRect, paintRestriction, paintingRoot);
        }
    }
    
    if (shouldPaint && (paintingPhase & GraphicsLayerPaintMask)) {
        if (renderer()->hasMask() && !selectionOnly && !damageRect.isEmpty()) {
            setClip(context, paintDirtyRect, damageRect);

            // Paint the mask.
            RenderObject::PaintInfo paintInfo(context, damageRect, PaintPhaseMask, false, paintingRootForRenderer, 0);
            renderer()->paint(paintInfo, tx, ty);
            
            // Restore the clip.
            restoreClip(context, paintDirtyRect, damageRect);
        }
    }

    ASSERT(!m_owningLayer->m_usedTransparency);
}

// Up-call from compositing layer drawing callback.
void RenderLayerBacking::paintContents(const GraphicsLayer*, GraphicsContext& context, GraphicsLayerPaintingPhase paintingPhase, const IntRect& clip)
{
    // We have to use the same root as for hit testing, because both methods
    // can compute and cache clipRects.
    IntRect enclosingBBox = compositedBounds();

    IntRect clipRect(clip);
    
    // Set up the coordinate space to be in the layer's rendering coordinates.
    context.translate(-enclosingBBox.x(), -enclosingBBox.y());

    // Offset the clip.
    clipRect.move(enclosingBBox.x(), enclosingBBox.y());
    
    // The dirtyRect is in the coords of the painting root.
    IntRect dirtyRect = enclosingBBox;
    dirtyRect.intersect(clipRect);

    paintIntoLayer(m_owningLayer, &context, dirtyRect, PaintRestrictionNone, paintingPhase, renderer());
}

bool RenderLayerBacking::startAnimation(double beginTime, const Animation* anim, const KeyframeList& keyframes)
{
    bool hasOpacity = keyframes.containsProperty(CSSPropertyOpacity);
    bool hasTransform = keyframes.containsProperty(CSSPropertyWebkitTransform);
    
    if (!hasOpacity && !hasTransform)
        return false;
    
    KeyframeValueList transformVector(AnimatedPropertyWebkitTransform);
    KeyframeValueList opacityVector(AnimatedPropertyOpacity);

    for (Vector<KeyframeValue>::const_iterator it = keyframes.beginKeyframes(); it != keyframes.endKeyframes(); ++it) {
        const RenderStyle* keyframeStyle = it->style();
        float key = it->key();

        if (!keyframeStyle)
            continue;
            
        // get timing function
        const TimingFunction* tf = keyframeStyle->hasAnimations() ? &((*keyframeStyle->animations()).animation(0)->timingFunction()) : 0;
        
        if (hasTransform)
            transformVector.insert(new TransformAnimationValue(key, &(keyframeStyle->transform()), tf));
        
        if (hasOpacity)
            opacityVector.insert(new FloatAnimationValue(key, keyframeStyle->opacity(), tf));
    }

    bool didAnimateTransform = !hasTransform;
    bool didAnimateOpacity = !hasOpacity;
    
    if (hasTransform && m_graphicsLayer->addAnimation(transformVector, toRenderBox(renderer())->borderBoxRect().size(), anim, keyframes.animationName(), beginTime))
        didAnimateTransform = true;

    if (hasOpacity && m_graphicsLayer->addAnimation(opacityVector, IntSize(), anim, keyframes.animationName(), beginTime))
        didAnimateOpacity = true;
    
    bool runningAcceleratedAnimation = didAnimateTransform && didAnimateOpacity;
    if (runningAcceleratedAnimation)
        compositor()->didStartAcceleratedAnimation();

    return runningAcceleratedAnimation;
}

bool RenderLayerBacking::startTransition(double beginTime, int property, const RenderStyle* fromStyle, const RenderStyle* toStyle)
{
    bool didAnimate = false;
    ASSERT(property != cAnimateAll);

    if (property == (int)CSSPropertyOpacity) {
        const Animation* opacityAnim = toStyle->transitionForProperty(CSSPropertyOpacity);
        if (opacityAnim && !opacityAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList opacityVector(AnimatedPropertyOpacity);
            opacityVector.insert(new FloatAnimationValue(0, compositingOpacity(fromStyle->opacity())));
            opacityVector.insert(new FloatAnimationValue(1, compositingOpacity(toStyle->opacity())));
            // The boxSize param is only used for transform animations (which can only run on RenderBoxes), so we pass an empty size here.
            if (m_graphicsLayer->addAnimation(opacityVector, IntSize(), opacityAnim, String(), beginTime))
                didAnimate = true;
        }
    }

    if (property == (int)CSSPropertyWebkitTransform && m_owningLayer->hasTransform()) {
        const Animation* transformAnim = toStyle->transitionForProperty(CSSPropertyWebkitTransform);
        if (transformAnim && !transformAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList transformVector(AnimatedPropertyWebkitTransform);
            transformVector.insert(new TransformAnimationValue(0, &fromStyle->transform()));
            transformVector.insert(new TransformAnimationValue(1, &toStyle->transform()));
            if (m_graphicsLayer->addAnimation(transformVector, toRenderBox(renderer())->borderBoxRect().size(), transformAnim, String(), beginTime))
                didAnimate = true;
        }
    }

    if (didAnimate)
        compositor()->didStartAcceleratedAnimation();
    
    return didAnimate;
}

void RenderLayerBacking::notifyAnimationStarted(const GraphicsLayer*, double time)
{
    renderer()->animation()->notifyAnimationStarted(renderer(), time);
}

void RenderLayerBacking::notifySyncRequired(const GraphicsLayer*)
{
    if (!renderer()->documentBeingDestroyed())
        compositor()->scheduleSync();
}

void RenderLayerBacking::animationFinished(const String& animationName)
{
    m_graphicsLayer->removeAnimationsForKeyframes(animationName);
}

void RenderLayerBacking::animationPaused(const String& animationName)
{
    m_graphicsLayer->pauseAnimation(animationName);
}

void RenderLayerBacking::transitionFinished(int property)
{
    AnimatedPropertyID animatedProperty = cssToGraphicsLayerProperty(property);
    if (animatedProperty != AnimatedPropertyInvalid)
        m_graphicsLayer->removeAnimationsForProperty(animatedProperty);
}

void RenderLayerBacking::suspendAnimations(double time)
{
    m_graphicsLayer->suspendAnimations(time);
}

void RenderLayerBacking::resumeAnimations()
{
    m_graphicsLayer->resumeAnimations();
}

IntRect RenderLayerBacking::compositedBounds() const
{
    return m_compositedBounds;
}

void RenderLayerBacking::setCompositedBounds(const IntRect& bounds)
{
    m_compositedBounds = bounds;

}
int RenderLayerBacking::graphicsLayerToCSSProperty(AnimatedPropertyID property)
{
    int cssProperty = CSSPropertyInvalid;
    switch (property) {
        case AnimatedPropertyWebkitTransform:
            cssProperty = CSSPropertyWebkitTransform;
            break;
        case AnimatedPropertyOpacity:
            cssProperty = CSSPropertyOpacity;
            break;
        case AnimatedPropertyBackgroundColor:
            cssProperty = CSSPropertyBackgroundColor;
            break;
        case AnimatedPropertyInvalid:
            ASSERT_NOT_REACHED();
    }
    return cssProperty;
}

AnimatedPropertyID RenderLayerBacking::cssToGraphicsLayerProperty(int cssProperty)
{
    switch (cssProperty) {
        case CSSPropertyWebkitTransform:
            return AnimatedPropertyWebkitTransform;
        case CSSPropertyOpacity:
            return AnimatedPropertyOpacity;
        case CSSPropertyBackgroundColor:
            return AnimatedPropertyBackgroundColor;
        // It's fine if we see other css properties here; they are just not accelerated.
    }
    return AnimatedPropertyInvalid;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
