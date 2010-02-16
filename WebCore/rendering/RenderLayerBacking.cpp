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
#if ENABLE(3D_CANVAS)    
#include "WebGLRenderingContext.h"
#endif
#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "HTMLCanvasElement.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "InspectorTimelineAgent.h"
#include "KeyframeList.h"
#include "PluginWidget.h"
#include "RenderBox.h"
#include "RenderImage.h"
#include "RenderLayerCompositor.h"
#include "RenderEmbeddedObject.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "Settings.h"

#include "RenderLayerBacking.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

static bool hasBorderOutlineOrShadow(const RenderStyle*);
static bool hasBoxDecorationsOrBackground(const RenderStyle*);
static bool hasBoxDecorationsOrBackgroundImage(const RenderStyle*);

static inline bool is3DCanvas(RenderObject* renderer)
{
#if ENABLE(3D_CANVAS)    
    if (renderer->isCanvas())
        return static_cast<HTMLCanvasElement*>(renderer->node())->is3D();
#else
    UNUSED_PARAM(renderer);
#endif
    return false;
}

RenderLayerBacking::RenderLayerBacking(RenderLayer* layer)
    : m_owningLayer(layer)
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
                m_graphicsLayer->setName(renderer()->renderName() + String(" ") + static_cast<HTMLElement*>(renderer()->node())->getIDAttribute());
            else
                m_graphicsLayer->setName(renderer()->renderName());
        }
    } else if (m_owningLayer->isReflection())
        m_graphicsLayer->setName("Reflection");
    else
        m_graphicsLayer->setName("Anonymous Node");
#endif  // NDEBUG

    updateLayerOpacity(renderer()->style());
    updateLayerTransform(renderer()->style());
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

void RenderLayerBacking::updateLayerOpacity(const RenderStyle* style)
{
    m_graphicsLayer->setOpacity(compositingOpacity(style->opacity()));
}

void RenderLayerBacking::updateLayerTransform(const RenderStyle* style)
{
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

void RenderLayerBacking::updateAfterLayout(UpdateDepth updateDepth, bool isUpdateRoot)
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
        
        if (isUpdateRoot) {
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

    if (m_owningLayer->hasReflection()) {
        if (m_owningLayer->reflectionLayer()->backing()) {
            GraphicsLayer* reflectionLayer = m_owningLayer->reflectionLayer()->backing()->graphicsLayer();
            m_graphicsLayer->setReplicatedByLayer(reflectionLayer);
        }
    } else
        m_graphicsLayer->setReplicatedByLayer(0);

    if (isDirectlyCompositedImage())
        updateImageContents();

    if (renderer()->isEmbeddedObject() && toRenderEmbeddedObject(renderer())->allowsAcceleratedCompositing()) {
        PluginWidget* pluginWidget = static_cast<PluginWidget*>(toRenderEmbeddedObject(renderer())->widget());
        m_graphicsLayer->setContentsToMedia(pluginWidget->platformLayer());
    }

#if ENABLE(3D_CANVAS)    
    if (is3DCanvas(renderer())) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(renderer()->node());
        WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(canvas->renderingContext());
        if (context->graphicsContext3D()->platformGraphicsContext3D())
            m_graphicsLayer->setContentsToGraphicsContext3D(context->graphicsContext3D());
    }
#endif

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
        updateLayerTransform(renderer()->style());

    // Set opacity, if it is not animating.
    if (!renderer()->animation()->isAnimatingPropertyOnRenderer(renderer(), CSSPropertyOpacity))
        updateLayerOpacity(renderer()->style());
    
    RenderStyle* style = renderer()->style();
    m_graphicsLayer->setPreserves3D(style->transformStyle3D() == TransformStyle3DPreserve3D && !renderer()->hasReflection());
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

    if (m_owningLayer->reflectionLayer() && m_owningLayer->reflectionLayer()->isComposited()) {
        RenderLayerBacking* reflectionBacking = m_owningLayer->reflectionLayer()->backing();
        reflectionBacking->updateGraphicsLayerGeometry();
        
        // The reflection layer has the bounds of m_owningLayer->reflectionLayer(),
        // but the reflected layer is the bounds of this layer, so we need to position it appropriately.
        FloatRect layerBounds = compositedBounds();
        FloatRect reflectionLayerBounds = reflectionBacking->compositedBounds();
        reflectionBacking->graphicsLayer()->setReplicatedLayerPosition(FloatPoint() + (layerBounds.location() - reflectionLayerBounds.location()));
    }

    m_graphicsLayer->setContentsRect(contentsBox());
    m_graphicsLayer->setDrawsContent(containsPaintedContent());
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

static bool hasBoxDecorationsOrBackground(const RenderStyle* style)
{
    return hasBorderOutlineOrShadow(style) || style->hasBackground();
}

static bool hasBoxDecorationsOrBackgroundImage(const RenderStyle* style)
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
    if (hasBoxDecorationsOrBackground(style))
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
        if (hasBoxDecorationsOrBackgroundImage(style))
            return false;
        
        // Now look at the body's renderer.
        HTMLElement* body = renderObject->document()->body();
        RenderObject* bodyObject = (body && body->hasLocalName(bodyTag)) ? body->renderer() : 0;
        if (!bodyObject)
            return false;
        
        style = bodyObject->style();
        
        if (hasBoxDecorationsOrBackgroundImage(style))
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

// Conservative test for having no rendered children.
bool RenderLayerBacking::hasNonCompositingContent() const
{
    if (m_owningLayer->hasOverflowControls())
        return true;
    
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

bool RenderLayerBacking::containsPaintedContent() const
{
    if (isSimpleContainerCompositingLayer() || paintingGoesToWindow() || m_artificiallyInflatedBounds || m_owningLayer->isReflection())
        return false;

    if (isDirectlyCompositedImage())
        return false;

    // FIXME: we could optimize cases where the image, video or canvas is known to fill the border box entirely,
    // and set background color on the layer in that case, instead of allocating backing store and painting.
    if (renderer()->isVideo() || is3DCanvas(renderer()))
        return hasBoxDecorationsOrBackground(renderer()->style());

    return true;
}

// An image can be directly compositing if it's the sole content of the layer, and has no box decorations
// that require painting. Direct compositing saves backing store.
bool RenderLayerBacking::isDirectlyCompositedImage() const
{
    RenderObject* renderObject = renderer();
    return renderObject->isImage() && !hasBoxDecorationsOrBackground(renderObject->style());
}

void RenderLayerBacking::rendererContentChanged()
{
    if (isDirectlyCompositedImage()) {
        updateImageContents();
        return;
    }

#if ENABLE(3D_CANVAS)    
    if (is3DCanvas(renderer())) {
        m_graphicsLayer->setGraphicsContext3DNeedsDisplay();
        return;
    }
#endif
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
                    PaintBehavior paintBehavior, GraphicsLayerPaintingPhase paintingPhase,
                    RenderObject* paintingRoot)
{
    if (paintingGoesToWindow()) {
        ASSERT_NOT_REACHED();
        return;
    }
    
    m_owningLayer->updateLayerListsIfNeeded();
    
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
            if (FrameView* frameView = box->view()->frameView()) {
                rw = frameView->contentsWidth();
                rh = frameView->contentsHeight();
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
                
    bool forceBlackText = paintBehavior & PaintBehaviorForceBlackText;
    bool selectionOnly  = paintBehavior & PaintBehaviorSelectionOnly;

    if (shouldPaint && (paintingPhase & GraphicsLayerPaintForeground)) {
        // Now walk the sorted list of children with negative z-indices. Only RenderLayers without compositing layers will paint.
        // FIXME: should these be painted as background?
        Vector<RenderLayer*>* negZOrderList = m_owningLayer->negZOrderList();
        if (negZOrderList) {
            for (Vector<RenderLayer*>::iterator it = negZOrderList->begin(); it != negZOrderList->end(); ++it)
                it[0]->paintLayer(rootLayer, context, paintDirtyRect, paintBehavior, paintingRoot);
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
                it[0]->paintLayer(rootLayer, context, paintDirtyRect, paintBehavior, paintingRoot);
        }

        // Now walk the sorted list of children with positive z-indices.
        Vector<RenderLayer*>* posZOrderList = m_owningLayer->posZOrderList();
        if (posZOrderList) {
            for (Vector<RenderLayer*>::iterator it = posZOrderList->begin(); it != posZOrderList->end(); ++it)
                it[0]->paintLayer(rootLayer, context, paintDirtyRect, paintBehavior, paintingRoot);
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

#if ENABLE(INSPECTOR)
static InspectorTimelineAgent* inspectorTimelineAgent(RenderObject* renderer)
{
    Frame* frame = renderer->document()->frame();
    if (!frame)
        return 0;
    Page* page = frame->page();
    if (!page)
        return 0;
    return page->inspectorTimelineAgent();
}
#endif

// Up-call from compositing layer drawing callback.
void RenderLayerBacking::paintContents(const GraphicsLayer*, GraphicsContext& context, GraphicsLayerPaintingPhase paintingPhase, const IntRect& clip)
{
#if ENABLE(INSPECTOR)
    if (InspectorTimelineAgent* timelineAgent = inspectorTimelineAgent(m_owningLayer->renderer()))
        timelineAgent->willPaint(clip);
#endif

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

    paintIntoLayer(m_owningLayer, &context, dirtyRect, PaintBehaviorNormal, paintingPhase, renderer());

#if ENABLE(INSPECTOR)
    if (InspectorTimelineAgent* timelineAgent = inspectorTimelineAgent(m_owningLayer->renderer()))
        timelineAgent->didPaint();
#endif
}

bool RenderLayerBacking::showDebugBorders() const
{
    return compositor() ? compositor()->showDebugBorders() : false;
}

bool RenderLayerBacking::showRepaintCounter() const
{
    return compositor() ? compositor()->showRepaintCounter() : false;
}

bool RenderLayerBacking::startAnimation(double timeOffset, const Animation* anim, const KeyframeList& keyframes)
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
    
    if (hasTransform && m_graphicsLayer->addAnimation(transformVector, toRenderBox(renderer())->borderBoxRect().size(), anim, keyframes.animationName(), timeOffset))
        didAnimateTransform = true;

    if (hasOpacity && m_graphicsLayer->addAnimation(opacityVector, IntSize(), anim, keyframes.animationName(), timeOffset))
        didAnimateOpacity = true;
    
    bool runningAcceleratedAnimation = didAnimateTransform && didAnimateOpacity;
    if (runningAcceleratedAnimation)
        compositor()->didStartAcceleratedAnimation();

    return runningAcceleratedAnimation;
}

bool RenderLayerBacking::startTransition(double timeOffset, int property, const RenderStyle* fromStyle, const RenderStyle* toStyle)
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
            if (m_graphicsLayer->addAnimation(opacityVector, IntSize(), opacityAnim, String(), timeOffset)) {
                // To ensure that the correct opacity is visible when the animation ends, also set the final opacity.
                updateLayerOpacity(toStyle);
                didAnimate = true;
            }
        }
    }

    if (property == (int)CSSPropertyWebkitTransform && m_owningLayer->hasTransform()) {
        const Animation* transformAnim = toStyle->transitionForProperty(CSSPropertyWebkitTransform);
        if (transformAnim && !transformAnim->isEmptyOrZeroDuration()) {
            KeyframeValueList transformVector(AnimatedPropertyWebkitTransform);
            transformVector.insert(new TransformAnimationValue(0, &fromStyle->transform()));
            transformVector.insert(new TransformAnimationValue(1, &toStyle->transform()));
            if (m_graphicsLayer->addAnimation(transformVector, toRenderBox(renderer())->borderBoxRect().size(), transformAnim, String(), timeOffset)) {
                // To ensure that the correct transform is visible when the animation ends, also set the final opacity.
                updateLayerTransform(toStyle);
                didAnimate = true;
            }
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

void RenderLayerBacking::animationPaused(double timeOffset, const String& animationName)
{
    m_graphicsLayer->pauseAnimation(animationName, timeOffset);
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
