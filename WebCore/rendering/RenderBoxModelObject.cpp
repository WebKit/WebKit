/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RenderBoxModelObject.h"

#include "GraphicsContext.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include <wtf/CurrentTime.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

bool RenderBoxModelObject::s_wasFloating = false;
bool RenderBoxModelObject::s_hadLayer = false;
bool RenderBoxModelObject::s_layerWasSelfPainting = false;

static const double cInterpolationCutoff = 800. * 800.;
static const double cLowQualityTimeThreshold = 0.500; // 500 ms

class RenderBoxModelScaleData : public Noncopyable {
public:
    RenderBoxModelScaleData(RenderBoxModelObject* object, const IntSize& size, const AffineTransform& transform, double time, bool lowQualityScale)
        : m_size(size)
        , m_transform(transform)
        , m_lastPaintTime(time)
        , m_lowQualityScale(lowQualityScale)
        , m_highQualityRepaintTimer(object, &RenderBoxModelObject::highQualityRepaintTimerFired)
    {
    }

    ~RenderBoxModelScaleData()
    {
        m_highQualityRepaintTimer.stop();
    }

    Timer<RenderBoxModelObject>& hiqhQualityRepaintTimer() { return m_highQualityRepaintTimer; }

    const IntSize& size() const { return m_size; }
    void setSize(const IntSize& s) { m_size = s; }
    double lastPaintTime() const { return m_lastPaintTime; }
    void setLastPaintTime(double t) { m_lastPaintTime = t; }
    bool useLowQualityScale() const { return m_lowQualityScale; }
    const AffineTransform& transform() const { return m_transform; }
    void setTransform(const AffineTransform& transform) { m_transform = transform; }
    void setUseLowQualityScale(bool b)
    {
        m_highQualityRepaintTimer.stop();
        m_lowQualityScale = b;
        if (b)
            m_highQualityRepaintTimer.startOneShot(cLowQualityTimeThreshold);
    }

private:
    IntSize m_size;
    AffineTransform m_transform;
    double m_lastPaintTime;
    bool m_lowQualityScale;
    Timer<RenderBoxModelObject> m_highQualityRepaintTimer;
};

class RenderBoxModelScaleObserver {
public:
    static bool shouldPaintBackgroundAtLowQuality(GraphicsContext*, RenderBoxModelObject*, Image*, const IntSize&);

    static void boxModelObjectDestroyed(RenderBoxModelObject* object)
    {
        if (gBoxModelObjects) {
            RenderBoxModelScaleData* data = gBoxModelObjects->take(object);
            delete data;
            if (!gBoxModelObjects->size()) {
                delete gBoxModelObjects;
                gBoxModelObjects = 0;
            }
        }
    }

    static void highQualityRepaintTimerFired(RenderBoxModelObject* object)
    {
        RenderBoxModelScaleObserver::boxModelObjectDestroyed(object);
        object->repaint();
    }

    static HashMap<RenderBoxModelObject*, RenderBoxModelScaleData*>* gBoxModelObjects;
};

bool RenderBoxModelScaleObserver::shouldPaintBackgroundAtLowQuality(GraphicsContext* context, RenderBoxModelObject* object, Image* image, const IntSize& size)
{
    // If the image is not a bitmap image, then none of this is relevant and we just paint at high
    // quality.
    if (!image || !image->isBitmapImage())
        return false;

    // Make sure to use the unzoomed image size, since if a full page zoom is in effect, the image
    // is actually being scaled.
    IntSize imageSize(image->width(), image->height());

    // Look ourselves up in the hashtable.
    RenderBoxModelScaleData* data = 0;
    if (gBoxModelObjects)
        data = gBoxModelObjects->get(object);

    const AffineTransform& currentTransform = context->getCTM();
    bool contextIsScaled = !currentTransform.isIdentityOrTranslation();
    if (!contextIsScaled && imageSize == size) {
        // There is no scale in effect.  If we had a scale in effect before, we can just delete this data.
        if (data) {
            gBoxModelObjects->remove(object);
            delete data;
        }
        return false;
    }

    // There is no need to hash scaled images that always use low quality mode when the page demands it.  This is the iChat case.
    if (object->document()->page()->inLowQualityImageInterpolationMode()) {
        double totalPixels = static_cast<double>(image->width()) * static_cast<double>(image->height());
        if (totalPixels > cInterpolationCutoff)
            return true;
    }

    // If there is no data yet, we will paint the first scale at high quality and record the paint time in case a second scale happens
    // very soon.
    if (!data) {
        data = new RenderBoxModelScaleData(object, size, currentTransform, currentTime(), false);
        if (!gBoxModelObjects)
            gBoxModelObjects = new HashMap<RenderBoxModelObject*, RenderBoxModelScaleData*>;
        gBoxModelObjects->set(object, data);
        return false;
    }

    // We are scaled, but we painted already at this size, so just keep using whatever mode we last painted with.
    if ((!contextIsScaled || data->transform() == currentTransform) && data->size() == size)
        return data->useLowQualityScale();

    // We have data and our size just changed.  If this change happened quickly, go into low quality mode and then set a repaint
    // timer to paint in high quality mode.  Otherwise it is ok to just paint in high quality mode.
    double newTime = currentTime();
    data->setUseLowQualityScale(newTime - data->lastPaintTime() < cLowQualityTimeThreshold);
    data->setLastPaintTime(newTime);
    data->setTransform(currentTransform);
    data->setSize(size);
    return data->useLowQualityScale();
}

HashMap<RenderBoxModelObject*, RenderBoxModelScaleData*>* RenderBoxModelScaleObserver::gBoxModelObjects = 0;

void RenderBoxModelObject::highQualityRepaintTimerFired(Timer<RenderBoxModelObject>*)
{
    RenderBoxModelScaleObserver::highQualityRepaintTimerFired(this);
}

RenderBoxModelObject::RenderBoxModelObject(Node* node)
    : RenderObject(node)
    , m_layer(0)
{
}

RenderBoxModelObject::~RenderBoxModelObject()
{
    // Our layer should have been destroyed and cleared by now
    ASSERT(!hasLayer());
    ASSERT(!m_layer);
    RenderBoxModelScaleObserver::boxModelObjectDestroyed(this);
}

void RenderBoxModelObject::destroyLayer()
{
    ASSERT(!hasLayer()); // Callers should have already called setHasLayer(false)
    ASSERT(m_layer);
    m_layer->destroy(renderArena());
    m_layer = 0;
}

void RenderBoxModelObject::destroy()
{
    // This must be done before we destroy the RenderObject.
    if (m_layer)
        m_layer->clearClipRects();

    // RenderObject::destroy calls back to destroyLayer() for layer destruction
    RenderObject::destroy();
}

bool RenderBoxModelObject::hasSelfPaintingLayer() const
{
    return m_layer && m_layer->isSelfPaintingLayer();
}

void RenderBoxModelObject::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    s_wasFloating = isFloating();
    s_hadLayer = hasLayer();
    if (s_hadLayer)
        s_layerWasSelfPainting = layer()->isSelfPaintingLayer();

    // If our z-index changes value or our visibility changes,
    // we need to dirty our stacking context's z-order list.
    if (style() && newStyle) {
        if (parent()) {
            // Do a repaint with the old style first, e.g., for example if we go from
            // having an outline to not having an outline.
            if (diff == StyleDifferenceRepaintLayer) {
                layer()->repaintIncludingDescendants();
                if (!(style()->clip() == newStyle->clip()))
                    layer()->clearClipRectsIncludingDescendants();
            } else if (diff == StyleDifferenceRepaint || newStyle->outlineSize() < style()->outlineSize())
                repaint();
        }
        
        if (diff == StyleDifferenceLayout) {
            // When a layout hint happens, we go ahead and do a repaint of the layer, since the layer could
            // end up being destroyed.
            if (hasLayer()) {
                if (style()->position() != newStyle->position() ||
                    style()->zIndex() != newStyle->zIndex() ||
                    style()->hasAutoZIndex() != newStyle->hasAutoZIndex() ||
                    !(style()->clip() == newStyle->clip()) ||
                    style()->hasClip() != newStyle->hasClip() ||
                    style()->opacity() != newStyle->opacity() ||
                    style()->transform() != newStyle->transform())
                layer()->repaintIncludingDescendants();
            } else if (newStyle->hasTransform() || newStyle->opacity() < 1) {
                // If we don't have a layer yet, but we are going to get one because of transform or opacity,
                //  then we need to repaint the old position of the object.
                repaint();
            }
        }

        if (hasLayer() && (style()->hasAutoZIndex() != newStyle->hasAutoZIndex() ||
                           style()->zIndex() != newStyle->zIndex() ||
                           style()->visibility() != newStyle->visibility())) {
            layer()->dirtyStackingContextZOrderLists();
            if (style()->hasAutoZIndex() != newStyle->hasAutoZIndex() || style()->visibility() != newStyle->visibility())
                layer()->dirtyZOrderLists();
        }
    }

    RenderObject::styleWillChange(diff, newStyle);
}

void RenderBoxModelObject::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderObject::styleDidChange(diff, oldStyle);
    updateBoxModelInfoFromStyle();
    
    if (requiresLayer()) {
        if (!layer()) {
            if (s_wasFloating && isFloating())
                setChildNeedsLayout(true);
            m_layer = new (renderArena()) RenderLayer(this);
            setHasLayer(true);
            m_layer->insertOnlyThisLayer();
            if (parent() && !needsLayout() && containingBlock())
                m_layer->updateLayerPositions();
        }
    } else if (layer() && layer()->parent()) {
        setHasTransform(false); // Either a transform wasn't specified or the object doesn't support transforms, so just null out the bit.
        setHasReflection(false);
        m_layer->removeOnlyThisLayer(); // calls destroyLayer() which clears m_layer
        if (s_wasFloating && isFloating())
            setChildNeedsLayout(true);
    }

    if (layer()) {
        layer()->styleChanged(diff, oldStyle);
        if (s_hadLayer && layer()->isSelfPaintingLayer() != s_layerWasSelfPainting)
            setChildNeedsLayout(true);
    }
}

void RenderBoxModelObject::updateBoxModelInfoFromStyle()
{
    // Set the appropriate bits for a box model object.  Since all bits are cleared in styleWillChange,
    // we only check for bits that could possibly be set to true.
    setHasBoxDecorations(style()->hasBorder() || style()->hasBackground() || style()->hasAppearance() || style()->boxShadow());
    setInline(style()->isDisplayInlineType());
    setRelPositioned(style()->position() == RelativePosition);
}

int RenderBoxModelObject::relativePositionOffsetX() const
{
    // Objects that shrink to avoid floats normally use available line width when computing containing block width.  However
    // in the case of relative positioning using percentages, we can't do this.  The offset should always be resolved using the
    // available width of the containing block.  Therefore we don't use containingBlockWidthForContent() here, but instead explicitly
    // call availableWidth on our containing block.
    if (!style()->left().isAuto()) {
        RenderBlock* cb = containingBlock();
        if (!style()->right().isAuto() && containingBlock()->style()->direction() == RTL)
            return -style()->right().calcValue(cb->availableWidth());
        return style()->left().calcValue(cb->availableWidth());
    }
    if (!style()->right().isAuto()) {
        RenderBlock* cb = containingBlock();
        return -style()->right().calcValue(cb->availableWidth());
    }
    return 0;
}

int RenderBoxModelObject::relativePositionOffsetY() const
{
    if (!style()->top().isAuto())
        return style()->top().calcValue(containingBlock()->availableHeight());
    else if (!style()->bottom().isAuto())
        return -style()->bottom().calcValue(containingBlock()->availableHeight());

    return 0;
}

int RenderBoxModelObject::offsetLeft() const
{
    // If the element is the HTML body element or does not have an associated box
    // return 0 and stop this algorithm.
    if (isBody())
        return 0;
    
    RenderBoxModelObject* offsetPar = offsetParent();
    int xPos = (isBox() ? toRenderBox(this)->x() : 0);
    
    // If the offsetParent of the element is null, or is the HTML body element,
    // return the distance between the canvas origin and the left border edge 
    // of the element and stop this algorithm.
    if (offsetPar) {
        if (offsetPar->isBox() && !offsetPar->isBody())
            xPos -= toRenderBox(offsetPar)->borderLeft();
        if (!isPositioned()) {
            if (isRelPositioned())
                xPos += relativePositionOffsetX();
            RenderObject* curr = parent();
            while (curr && curr != offsetPar) {
                // FIXME: What are we supposed to do inside SVG content?
                if (curr->isBox() && !curr->isTableRow())
                    xPos += toRenderBox(curr)->x();
                curr = curr->parent();
            }
            if (offsetPar->isBox() && offsetPar->isBody() && !offsetPar->isRelPositioned() && !offsetPar->isPositioned())
                xPos += toRenderBox(offsetPar)->x();
        }
    }

    return xPos;
}

int RenderBoxModelObject::offsetTop() const
{
    // If the element is the HTML body element or does not have an associated box
    // return 0 and stop this algorithm.
    if (isBody())
        return 0;
    
    RenderBoxModelObject* offsetPar = offsetParent();
    int yPos = (isBox() ? toRenderBox(this)->y() : 0);
    
    // If the offsetParent of the element is null, or is the HTML body element,
    // return the distance between the canvas origin and the top border edge 
    // of the element and stop this algorithm.
    if (offsetPar) {
        if (offsetPar->isBox() && !offsetPar->isBody())
            yPos -= toRenderBox(offsetPar)->borderTop();
        if (!isPositioned()) {
            if (isRelPositioned())
                yPos += relativePositionOffsetY();
            RenderObject* curr = parent();
            while (curr && curr != offsetPar) {
                // FIXME: What are we supposed to do inside SVG content?
                if (curr->isBox() && !curr->isTableRow())
                    yPos += toRenderBox(curr)->y();
                curr = curr->parent();
            }
            if (offsetPar->isBox() && offsetPar->isBody() && !offsetPar->isRelPositioned() && !offsetPar->isPositioned())
                yPos += toRenderBox(offsetPar)->y();
        }
    }
    return yPos;
}

int RenderBoxModelObject::paddingTop(bool) const
{
    int w = 0;
    Length padding = style()->paddingTop();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

int RenderBoxModelObject::paddingBottom(bool) const
{
    int w = 0;
    Length padding = style()->paddingBottom();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

int RenderBoxModelObject::paddingLeft(bool) const
{
    int w = 0;
    Length padding = style()->paddingLeft();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

int RenderBoxModelObject::paddingRight(bool) const
{
    int w = 0;
    Length padding = style()->paddingRight();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}


void RenderBoxModelObject::paintFillLayerExtended(const PaintInfo& paintInfo, const Color& c, const FillLayer* bgLayer, int tx, int ty, int w, int h, InlineFlowBox* box, CompositeOperator op, RenderObject* backgroundObject)
{
    GraphicsContext* context = paintInfo.context;
    if (context->paintingDisabled())
        return;

    bool includeLeftEdge = box ? box->includeLeftEdge() : true;
    bool includeRightEdge = box ? box->includeRightEdge() : true;
    int bLeft = includeLeftEdge ? borderLeft() : 0;
    int bRight = includeRightEdge ? borderRight() : 0;
    int pLeft = includeLeftEdge ? paddingLeft() : 0;
    int pRight = includeRightEdge ? paddingRight() : 0;

    bool clippedToBorderRadius = false;
    if (style()->hasBorderRadius() && (includeLeftEdge || includeRightEdge)) {
        IntRect borderRect(tx, ty, w, h);

        if (borderRect.isEmpty())
            return;

        context->save();

        IntSize topLeft, topRight, bottomLeft, bottomRight;
        style()->getBorderRadiiForRect(borderRect, topLeft, topRight, bottomLeft, bottomRight);

        context->addRoundedRectClip(borderRect, includeLeftEdge ? topLeft : IntSize(),
                                                includeRightEdge ? topRight : IntSize(),
                                                includeLeftEdge ? bottomLeft : IntSize(),
                                                includeRightEdge ? bottomRight : IntSize());
        clippedToBorderRadius = true;
    }

    bool clippedWithLocalScrolling = hasOverflowClip() && bgLayer->attachment() == LocalBackgroundAttachment;
    if (clippedWithLocalScrolling) {
        // Clip to the overflow area.
        context->save();
        context->clip(toRenderBox(this)->overflowClipRect(tx, ty));
        
        // Now adjust our tx, ty, w, h to reflect a scrolled content box with borders at the ends.
        layer()->subtractScrolledContentOffset(tx, ty);
        w = bLeft + layer()->scrollWidth() + bRight;
        h = borderTop() + layer()->scrollHeight() + borderBottom();
    }
    
    if (bgLayer->clip() == PaddingFillBox || bgLayer->clip() == ContentFillBox) {
        // Clip to the padding or content boxes as necessary.
        bool includePadding = bgLayer->clip() == ContentFillBox;
        int x = tx + bLeft + (includePadding ? pLeft : 0);
        int y = ty + borderTop() + (includePadding ? paddingTop() : 0);
        int width = w - bLeft - bRight - (includePadding ? pLeft + pRight : 0);
        int height = h - borderTop() - borderBottom() - (includePadding ? paddingTop() + paddingBottom() : 0);
        context->save();
        context->clip(IntRect(x, y, width, height));
    } else if (bgLayer->clip() == TextFillBox) {
        // We have to draw our text into a mask that can then be used to clip background drawing.
        // First figure out how big the mask has to be.  It should be no bigger than what we need
        // to actually render, so we should intersect the dirty rect with the border box of the background.
        IntRect maskRect(tx, ty, w, h);
        maskRect.intersect(paintInfo.rect);
        
        // Now create the mask.
        OwnPtr<ImageBuffer> maskImage = ImageBuffer::create(maskRect.size());
        if (!maskImage)
            return;
        
        GraphicsContext* maskImageContext = maskImage->context();
        maskImageContext->translate(-maskRect.x(), -maskRect.y());
        
        // Now add the text to the clip.  We do this by painting using a special paint phase that signals to
        // InlineTextBoxes that they should just add their contents to the clip.
        PaintInfo info(maskImageContext, maskRect, PaintPhaseTextClip, true, 0, 0);
        if (box)
            box->paint(info, tx - box->x(), ty - box->y());
        else {
            int x = isBox() ? toRenderBox(this)->x() : 0;
            int y = isBox() ? toRenderBox(this)->y() : 0;
            paint(info, tx - x, ty - y);
        }
        
        // The mask has been created.  Now we just need to clip to it.
        context->save();
        context->clipToImageBuffer(maskRect, maskImage.get());
    }
    
    StyleImage* bg = bgLayer->image();
    bool shouldPaintBackgroundImage = bg && bg->canRender(style()->effectiveZoom());
    Color bgColor = c;

    // When this style flag is set, change existing background colors and images to a solid white background.
    // If there's no bg color or image, leave it untouched to avoid affecting transparency.
    // We don't try to avoid loading the background images, because this style flag is only set
    // when printing, and at that point we've already loaded the background images anyway. (To avoid
    // loading the background images we'd have to do this check when applying styles rather than
    // while rendering.)
    if (style()->forceBackgroundsToWhite()) {
        // Note that we can't reuse this variable below because the bgColor might be changed
        bool shouldPaintBackgroundColor = !bgLayer->next() && bgColor.isValid() && bgColor.alpha() > 0;
        if (shouldPaintBackgroundImage || shouldPaintBackgroundColor) {
            bgColor = Color::white;
            shouldPaintBackgroundImage = false;
        }
    }

    bool isRoot = this->isRoot();

    // Only fill with a base color (e.g., white) if we're the root document, since iframes/frames with
    // no background in the child document should show the parent's background.
    bool isOpaqueRoot = false;
    if (isRoot) {
        isOpaqueRoot = true;
        if (!bgLayer->next() && !(bgColor.isValid() && bgColor.alpha() == 255) && view()->frameView()) {
            Element* ownerElement = document()->ownerElement();
            if (ownerElement) {
                if (!ownerElement->hasTagName(frameTag)) {
                    // Locate the <body> element using the DOM.  This is easier than trying
                    // to crawl around a render tree with potential :before/:after content and
                    // anonymous blocks created by inline <body> tags etc.  We can locate the <body>
                    // render object very easily via the DOM.
                    HTMLElement* body = document()->body();
                    if (body) {
                        // Can't scroll a frameset document anyway.
                        isOpaqueRoot = body->hasLocalName(framesetTag);
                    }
                }
            } else
                isOpaqueRoot = !view()->frameView()->isTransparent();
        }
        view()->frameView()->setContentIsOpaque(isOpaqueRoot);
    }

    // Paint the color first underneath all images.
    if (!bgLayer->next()) {
        IntRect rect(tx, ty, w, h);
        rect.intersect(paintInfo.rect);
        // If we have an alpha and we are painting the root element, go ahead and blend with the base background color.
        if (isOpaqueRoot) {
            Color baseColor = view()->frameView()->baseBackgroundColor();
            if (baseColor.alpha() > 0) {
                context->save();
                context->setCompositeOperation(CompositeCopy);
                context->fillRect(rect, baseColor, style()->colorSpace());
                context->restore();
            } else
                context->clearRect(rect);
        }

        if (bgColor.isValid() && bgColor.alpha() > 0)
            context->fillRect(rect, bgColor, style()->colorSpace());
    }

    // no progressive loading of the background image
    if (shouldPaintBackgroundImage) {
        IntRect destRect;
        IntPoint phase;
        IntSize tileSize;

        calculateBackgroundImageGeometry(bgLayer, tx, ty, w, h, destRect, phase, tileSize);
        IntPoint destOrigin = destRect.location();
        destRect.intersect(paintInfo.rect);
        if (!destRect.isEmpty()) {
            phase += destRect.location() - destOrigin;
            CompositeOperator compositeOp = op == CompositeSourceOver ? bgLayer->composite() : op;
            RenderObject* clientForBackgroundImage = backgroundObject ? backgroundObject : this;
            Image* image = bg->image(clientForBackgroundImage, tileSize);
            bool useLowQualityScaling = RenderBoxModelScaleObserver::shouldPaintBackgroundAtLowQuality(context, this, image, destRect.size());
            context->drawTiledImage(image, style()->colorSpace(), destRect, phase, tileSize, compositeOp, useLowQualityScaling);
        }
    }

    if (bgLayer->clip() != BorderFillBox)
        // Undo the background clip
        context->restore();

    if (clippedToBorderRadius)
        // Undo the border radius clip
        context->restore();
        
    if (clippedWithLocalScrolling) // Undo the clip for local background attachments.
        context->restore();
}

IntSize RenderBoxModelObject::calculateFillTileSize(const FillLayer* fillLayer, IntSize positioningAreaSize) const
{
    StyleImage* image = fillLayer->image();
    image->setImageContainerSize(positioningAreaSize); // Use the box established by background-origin.

    EFillSizeType type = fillLayer->size().type;

    switch (type) {
        case SizeLength: {
            int w = positioningAreaSize.width();
            int h = positioningAreaSize.height();
            Length layerWidth = fillLayer->size().size.width();
            Length layerHeight = fillLayer->size().size.height();

            if (layerWidth.isFixed())
                w = layerWidth.value();
            else if (layerWidth.isPercent())
                w = layerWidth.calcValue(positioningAreaSize.width());
            
            if (layerHeight.isFixed())
                h = layerHeight.value();
            else if (layerHeight.isPercent())
                h = layerHeight.calcValue(positioningAreaSize.height());
            
            // If one of the values is auto we have to use the appropriate
            // scale to maintain our aspect ratio.
            if (layerWidth.isAuto() && !layerHeight.isAuto())
                w = image->imageSize(this, style()->effectiveZoom()).width() * h / image->imageSize(this, style()->effectiveZoom()).height();        
            else if (!layerWidth.isAuto() && layerHeight.isAuto())
                h = image->imageSize(this, style()->effectiveZoom()).height() * w / image->imageSize(this, style()->effectiveZoom()).width();
            else if (layerWidth.isAuto() && layerHeight.isAuto()) {
                // If both width and height are auto, we just want to use the image's
                // intrinsic size.
                w = image->imageSize(this, style()->effectiveZoom()).width();
                h = image->imageSize(this, style()->effectiveZoom()).height();
            }
            
            return IntSize(max(1, w), max(1, h));
        }
        case Contain:
        case Cover: {
            IntSize imageIntrinsicSize = image->imageSize(this, 1);
            float horizontalScaleFactor = static_cast<float>(positioningAreaSize.width()) / imageIntrinsicSize.width();
            float verticalScaleFactor = static_cast<float>(positioningAreaSize.height()) / imageIntrinsicSize.height();
            float scaleFactor = type == Contain ? min(horizontalScaleFactor, verticalScaleFactor) : max(horizontalScaleFactor, verticalScaleFactor);

            return IntSize(max<int>(1, imageIntrinsicSize.width() * scaleFactor), max<int>(1, imageIntrinsicSize.height() * scaleFactor));
        }
        case SizeNone:
            break;
    }
    return image->imageSize(this, style()->effectiveZoom());
}

void RenderBoxModelObject::calculateBackgroundImageGeometry(const FillLayer* fillLayer, int tx, int ty, int w, int h, 
                                                            IntRect& destRect, IntPoint& phase, IntSize& tileSize)
{
    int left = 0;
    int top = 0;
    IntSize positioningAreaSize;

    // Determine the background positioning area and set destRect to the background painting area.
    // destRect will be adjusted later if the background is non-repeating.
    bool fixedAttachment = fillLayer->attachment() == FixedBackgroundAttachment;

#if ENABLE(FAST_MOBILE_SCROLLING)
    if (view()->frameView() && view()->frameView()->canBlitOnScroll()) {
        // As a side effect of an optimization to blit on scroll, we do not honor the CSS
        // property "background-attachment: fixed" because it may result in rendering
        // artifacts. Note, these artifacts only appear if we are blitting on scroll of
        // a page that has fixed background images.
        fixedAttachment = false;
    }
#endif

    if (!fixedAttachment) {
        destRect = IntRect(tx, ty, w, h);

        int right = 0;
        int bottom = 0;
        // Scroll and Local.
        if (fillLayer->origin() != BorderFillBox) {
            left = borderLeft();
            right = borderRight();
            top = borderTop();
            bottom = borderBottom();
            if (fillLayer->origin() == ContentFillBox) {
                left += paddingLeft();
                right += paddingRight();
                top += paddingTop();
                bottom += paddingBottom();
            }
        }

        // The background of the box generated by the root element covers the entire canvas including
        // its margins. Since those were added in already, we have to factor them out when computing
        // the background positioning area.
        if (isRoot()) {
            positioningAreaSize = IntSize(toRenderBox(this)->width() - left - right, toRenderBox(this)->height() - top - bottom);
            left += marginLeft();
            top += marginTop();
        } else
            positioningAreaSize = IntSize(w - left - right, h - top - bottom);
    } else {
        destRect = viewRect();
        positioningAreaSize = destRect.size();
    }

    tileSize = calculateFillTileSize(fillLayer, positioningAreaSize);

    EFillRepeat backgroundRepeatX = fillLayer->repeatX();
    EFillRepeat backgroundRepeatY = fillLayer->repeatY();

    int xPosition = fillLayer->xPosition().calcMinValue(positioningAreaSize.width() - tileSize.width(), true);
    if (backgroundRepeatX == RepeatFill)
        phase.setX(tileSize.width() ? tileSize.width() - (xPosition + left) % tileSize.width() : 0);
    else {
        destRect.move(max(xPosition + left, 0), 0);
        phase.setX(-min(xPosition + left, 0));
        destRect.setWidth(tileSize.width() + min(xPosition + left, 0));
    }

    int yPosition = fillLayer->yPosition().calcMinValue(positioningAreaSize.height() - tileSize.height(), true);
    if (backgroundRepeatY == RepeatFill)
        phase.setY(tileSize.height() ? tileSize.height() - (yPosition + top) % tileSize.height() : 0);
    else {
        destRect.move(0, max(yPosition + top, 0));
        phase.setY(-min(yPosition + top, 0));
        destRect.setHeight(tileSize.height() + min(yPosition + top, 0));
    }

    if (fixedAttachment)
        phase.move(max(tx - destRect.x(), 0), max(ty - destRect.y(), 0));

    destRect.intersect(IntRect(tx, ty, w, h));
}

int RenderBoxModelObject::verticalPosition(bool firstLine) const
{
    // This method determines the vertical position for inline elements.
    ASSERT(isInline());
    if (!isInline())
        return 0;

    int vpos = 0;
    EVerticalAlign va = style()->verticalAlign();
    if (va == TOP)
        vpos = PositionTop;
    else if (va == BOTTOM)
        vpos = PositionBottom;
    else {
        bool checkParent = parent()->isRenderInline() && parent()->style()->verticalAlign() != TOP && parent()->style()->verticalAlign() != BOTTOM;
        vpos = checkParent ? toRenderInline(parent())->verticalPositionFromCache(firstLine) : 0;
        // don't allow elements nested inside text-top to have a different valignment.
        if (va == BASELINE)
            return vpos;

        const Font& f = parent()->style(firstLine)->font();
        int fontsize = f.pixelSize();

        if (va == SUB)
            vpos += fontsize / 5 + 1;
        else if (va == SUPER)
            vpos -= fontsize / 3 + 1;
        else if (va == TEXT_TOP)
            vpos += baselinePosition(firstLine) - f.ascent();
        else if (va == MIDDLE)
            vpos += -static_cast<int>(f.xHeight() / 2) - lineHeight(firstLine) / 2 + baselinePosition(firstLine);
        else if (va == TEXT_BOTTOM) {
            vpos += f.descent();
            if (!isReplaced()) // lineHeight - baselinePosition is always 0 for replaced elements, so don't bother wasting time in that case.
                vpos -= (lineHeight(firstLine) - baselinePosition(firstLine));
        } else if (va == BASELINE_MIDDLE)
            vpos += -lineHeight(firstLine) / 2 + baselinePosition(firstLine);
        else if (va == LENGTH)
            vpos -= style()->verticalAlignLength().calcValue(lineHeight(firstLine));
    }

    return vpos;
}

bool RenderBoxModelObject::paintNinePieceImage(GraphicsContext* graphicsContext, int tx, int ty, int w, int h, const RenderStyle* style,
                                               const NinePieceImage& ninePieceImage, CompositeOperator op)
{
    StyleImage* styleImage = ninePieceImage.image();
    if (!styleImage)
        return false;

    if (!styleImage->isLoaded())
        return true; // Never paint a nine-piece image incrementally, but don't paint the fallback borders either.

    if (!styleImage->canRender(style->effectiveZoom()))
        return false;

    // FIXME: border-image is broken with full page zooming when tiling has to happen, since the tiling function
    // doesn't have any understanding of the zoom that is in effect on the tile.
    styleImage->setImageContainerSize(IntSize(w, h));
    IntSize imageSize = styleImage->imageSize(this, 1.0f);
    int imageWidth = imageSize.width();
    int imageHeight = imageSize.height();

    int topSlice = min(imageHeight, ninePieceImage.m_slices.top().calcValue(imageHeight));
    int bottomSlice = min(imageHeight, ninePieceImage.m_slices.bottom().calcValue(imageHeight));
    int leftSlice = min(imageWidth, ninePieceImage.m_slices.left().calcValue(imageWidth));
    int rightSlice = min(imageWidth, ninePieceImage.m_slices.right().calcValue(imageWidth));

    ENinePieceImageRule hRule = ninePieceImage.horizontalRule();
    ENinePieceImageRule vRule = ninePieceImage.verticalRule();

    bool fitToBorder = style->borderImage() == ninePieceImage;
    
    int leftWidth = fitToBorder ? style->borderLeftWidth() : leftSlice;
    int topWidth = fitToBorder ? style->borderTopWidth() : topSlice;
    int rightWidth = fitToBorder ? style->borderRightWidth() : rightSlice;
    int bottomWidth = fitToBorder ? style->borderBottomWidth() : bottomSlice;

    bool drawLeft = leftSlice > 0 && leftWidth > 0;
    bool drawTop = topSlice > 0 && topWidth > 0;
    bool drawRight = rightSlice > 0 && rightWidth > 0;
    bool drawBottom = bottomSlice > 0 && bottomWidth > 0;
    bool drawMiddle = (imageWidth - leftSlice - rightSlice) > 0 && (w - leftWidth - rightWidth) > 0 &&
                      (imageHeight - topSlice - bottomSlice) > 0 && (h - topWidth - bottomWidth) > 0;

    Image* image = styleImage->image(this, imageSize);
    ColorSpace colorSpace = style->colorSpace();

    if (drawLeft) {
        // Paint the top and bottom left corners.

        // The top left corner rect is (tx, ty, leftWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (0, 0, leftSlice, topSlice)
        if (drawTop)
            graphicsContext->drawImage(image, colorSpace, IntRect(tx, ty, leftWidth, topWidth),
                                       IntRect(0, 0, leftSlice, topSlice), op);

        // The bottom left corner rect is (tx, ty + h - bottomWidth, leftWidth, bottomWidth)
        // The rect to use from within the image is (0, imageHeight - bottomSlice, leftSlice, botomSlice)
        if (drawBottom)
            graphicsContext->drawImage(image, colorSpace, IntRect(tx, ty + h - bottomWidth, leftWidth, bottomWidth),
                                       IntRect(0, imageHeight - bottomSlice, leftSlice, bottomSlice), op);

        // Paint the left edge.
        // Have to scale and tile into the border rect.
        graphicsContext->drawTiledImage(image, colorSpace, IntRect(tx, ty + topWidth, leftWidth,
                                        h - topWidth - bottomWidth),
                                        IntRect(0, topSlice, leftSlice, imageHeight - topSlice - bottomSlice),
                                        Image::StretchTile, (Image::TileRule)vRule, op);
    }

    if (drawRight) {
        // Paint the top and bottom right corners
        // The top right corner rect is (tx + w - rightWidth, ty, rightWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (imageWidth - rightSlice, 0, rightSlice, topSlice)
        if (drawTop)
            graphicsContext->drawImage(image, colorSpace, IntRect(tx + w - rightWidth, ty, rightWidth, topWidth),
                                       IntRect(imageWidth - rightSlice, 0, rightSlice, topSlice), op);

        // The bottom right corner rect is (tx + w - rightWidth, ty + h - bottomWidth, rightWidth, bottomWidth)
        // The rect to use from within the image is (imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice)
        if (drawBottom)
            graphicsContext->drawImage(image, colorSpace, IntRect(tx + w - rightWidth, ty + h - bottomWidth, rightWidth, bottomWidth),
                                       IntRect(imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice), op);

        // Paint the right edge.
        graphicsContext->drawTiledImage(image, colorSpace, IntRect(tx + w - rightWidth, ty + topWidth, rightWidth,
                                        h - topWidth - bottomWidth),
                                        IntRect(imageWidth - rightSlice, topSlice, rightSlice, imageHeight - topSlice - bottomSlice),
                                        Image::StretchTile, (Image::TileRule)vRule, op);
    }

    // Paint the top edge.
    if (drawTop)
        graphicsContext->drawTiledImage(image, colorSpace, IntRect(tx + leftWidth, ty, w - leftWidth - rightWidth, topWidth),
                                        IntRect(leftSlice, 0, imageWidth - rightSlice - leftSlice, topSlice),
                                        (Image::TileRule)hRule, Image::StretchTile, op);

    // Paint the bottom edge.
    if (drawBottom)
        graphicsContext->drawTiledImage(image, colorSpace, IntRect(tx + leftWidth, ty + h - bottomWidth,
                                        w - leftWidth - rightWidth, bottomWidth),
                                        IntRect(leftSlice, imageHeight - bottomSlice, imageWidth - rightSlice - leftSlice, bottomSlice),
                                        (Image::TileRule)hRule, Image::StretchTile, op);

    // Paint the middle.
    if (drawMiddle)
        graphicsContext->drawTiledImage(image, colorSpace, IntRect(tx + leftWidth, ty + topWidth, w - leftWidth - rightWidth,
                                        h - topWidth - bottomWidth),
                                        IntRect(leftSlice, topSlice, imageWidth - rightSlice - leftSlice, imageHeight - topSlice - bottomSlice),
                                        (Image::TileRule)hRule, (Image::TileRule)vRule, op);

    return true;
}

void RenderBoxModelObject::paintBorder(GraphicsContext* graphicsContext, int tx, int ty, int w, int h,
                               const RenderStyle* style, bool begin, bool end)
{
    if (paintNinePieceImage(graphicsContext, tx, ty, w, h, style, style->borderImage()))
        return;

    const Color& topColor = style->borderTopColor();
    const Color& bottomColor = style->borderBottomColor();
    const Color& leftColor = style->borderLeftColor();
    const Color& rightColor = style->borderRightColor();

    bool topTransparent = style->borderTopIsTransparent();
    bool bottomTransparent = style->borderBottomIsTransparent();
    bool rightTransparent = style->borderRightIsTransparent();
    bool leftTransparent = style->borderLeftIsTransparent();

    EBorderStyle topStyle = style->borderTopStyle();
    EBorderStyle bottomStyle = style->borderBottomStyle();
    EBorderStyle leftStyle = style->borderLeftStyle();
    EBorderStyle rightStyle = style->borderRightStyle();

    bool renderTop = topStyle > BHIDDEN && !topTransparent;
    bool renderLeft = leftStyle > BHIDDEN && begin && !leftTransparent;
    bool renderRight = rightStyle > BHIDDEN && end && !rightTransparent;
    bool renderBottom = bottomStyle > BHIDDEN && !bottomTransparent;

    bool renderRadii = false;
    IntSize topLeft, topRight, bottomLeft, bottomRight;

    if (style->hasBorderRadius()) {
        IntRect borderRect = IntRect(tx, ty, w, h);

        IntSize topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius;
        style->getBorderRadiiForRect(borderRect, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);

        if (begin) {
            topLeft = topLeftRadius;
            bottomLeft = bottomLeftRadius;
        }
        if (end) {
            topRight = topRightRadius;
            bottomRight = bottomRightRadius;
        }

        renderRadii = true;

        // Clip to the rounded rectangle.
        graphicsContext->save();
        graphicsContext->addRoundedRectClip(borderRect, topLeft, topRight, bottomLeft, bottomRight);
    }

    int firstAngleStart, secondAngleStart, firstAngleSpan, secondAngleSpan;
    float thickness;
    bool upperLeftBorderStylesMatch = renderLeft && (topStyle == leftStyle) && (topColor == leftColor);
    bool upperRightBorderStylesMatch = renderRight && (topStyle == rightStyle) && (topColor == rightColor) && (topStyle != OUTSET) && (topStyle != RIDGE) && (topStyle != INSET) && (topStyle != GROOVE);
    bool lowerLeftBorderStylesMatch = renderLeft && (bottomStyle == leftStyle) && (bottomColor == leftColor) && (bottomStyle != OUTSET) && (bottomStyle != RIDGE) && (bottomStyle != INSET) && (bottomStyle != GROOVE);
    bool lowerRightBorderStylesMatch = renderRight && (bottomStyle == rightStyle) && (bottomColor == rightColor);

    if (renderTop) {
        bool ignore_left = (renderRadii && topLeft.width() > 0) ||
            (topColor == leftColor && topTransparent == leftTransparent && topStyle >= OUTSET &&
             (leftStyle == DOTTED || leftStyle == DASHED || leftStyle == SOLID || leftStyle == OUTSET));

        bool ignore_right = (renderRadii && topRight.width() > 0) ||
            (topColor == rightColor && topTransparent == rightTransparent && topStyle >= OUTSET &&
             (rightStyle == DOTTED || rightStyle == DASHED || rightStyle == SOLID || rightStyle == INSET));

        int x = tx;
        int x2 = tx + w;
        if (renderRadii) {
            x += topLeft.width();
            x2 -= topRight.width();
        }

        drawLineForBoxSide(graphicsContext, x, ty, x2, ty + style->borderTopWidth(), BSTop, topColor, style->color(), topStyle,
                   ignore_left ? 0 : style->borderLeftWidth(), ignore_right ? 0 : style->borderRightWidth());

        if (renderRadii) {
            int leftY = ty;

            // We make the arc double thick and let the clip rect take care of clipping the extra off.
            // We're doing this because it doesn't seem possible to match the curve of the clip exactly
            // with the arc-drawing function.
            thickness = style->borderTopWidth() * 2;

            if (topLeft.width()) {
                int leftX = tx;
                // The inner clip clips inside the arc. This is especially important for 1px borders.
                bool applyLeftInnerClip = (style->borderLeftWidth() < topLeft.width())
                    && (style->borderTopWidth() < topLeft.height())
                    && (topStyle != DOUBLE || style->borderTopWidth() > 6);
                if (applyLeftInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(leftX, leftY, topLeft.width() * 2, topLeft.height() * 2),
                                                             style->borderTopWidth());
                }

                firstAngleStart = 90;
                firstAngleSpan = upperLeftBorderStylesMatch ? 90 : 45;

                // Draw upper left arc
                drawArcForBoxSide(graphicsContext, leftX, leftY, thickness, topLeft, firstAngleStart, firstAngleSpan,
                              BSTop, topColor, style->color(), topStyle, true);
                if (applyLeftInnerClip)
                    graphicsContext->restore();
            }

            if (topRight.width()) {
                int rightX = tx + w - topRight.width() * 2;
                bool applyRightInnerClip = (style->borderRightWidth() < topRight.width())
                    && (style->borderTopWidth() < topRight.height())
                    && (topStyle != DOUBLE || style->borderTopWidth() > 6);
                if (applyRightInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(rightX, leftY, topRight.width() * 2, topRight.height() * 2),
                                                             style->borderTopWidth());
                }

                if (upperRightBorderStylesMatch) {
                    secondAngleStart = 0;
                    secondAngleSpan = 90;
                } else {
                    secondAngleStart = 45;
                    secondAngleSpan = 45;
                }

                // Draw upper right arc
                drawArcForBoxSide(graphicsContext, rightX, leftY, thickness, topRight, secondAngleStart, secondAngleSpan,
                              BSTop, topColor, style->color(), topStyle, false);
                if (applyRightInnerClip)
                    graphicsContext->restore();
            }
        }
    }

    if (renderBottom) {
        bool ignore_left = (renderRadii && bottomLeft.width() > 0) ||
            (bottomColor == leftColor && bottomTransparent == leftTransparent && bottomStyle >= OUTSET &&
             (leftStyle == DOTTED || leftStyle == DASHED || leftStyle == SOLID || leftStyle == OUTSET));

        bool ignore_right = (renderRadii && bottomRight.width() > 0) ||
            (bottomColor == rightColor && bottomTransparent == rightTransparent && bottomStyle >= OUTSET &&
             (rightStyle == DOTTED || rightStyle == DASHED || rightStyle == SOLID || rightStyle == INSET));

        int x = tx;
        int x2 = tx + w;
        if (renderRadii) {
            x += bottomLeft.width();
            x2 -= bottomRight.width();
        }

        drawLineForBoxSide(graphicsContext, x, ty + h - style->borderBottomWidth(), x2, ty + h, BSBottom, bottomColor, style->color(), bottomStyle,
                   ignore_left ? 0 : style->borderLeftWidth(), ignore_right ? 0 : style->borderRightWidth());

        if (renderRadii) {
            thickness = style->borderBottomWidth() * 2;

            if (bottomLeft.width()) {
                int leftX = tx;
                int leftY = ty + h - bottomLeft.height() * 2;
                bool applyLeftInnerClip = (style->borderLeftWidth() < bottomLeft.width())
                    && (style->borderBottomWidth() < bottomLeft.height())
                    && (bottomStyle != DOUBLE || style->borderBottomWidth() > 6);
                if (applyLeftInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(leftX, leftY, bottomLeft.width() * 2, bottomLeft.height() * 2),
                                                             style->borderBottomWidth());
                }

                if (lowerLeftBorderStylesMatch) {
                    firstAngleStart = 180;
                    firstAngleSpan = 90;
                } else {
                    firstAngleStart = 225;
                    firstAngleSpan = 45;
                }

                // Draw lower left arc
                drawArcForBoxSide(graphicsContext, leftX, leftY, thickness, bottomLeft, firstAngleStart, firstAngleSpan,
                              BSBottom, bottomColor, style->color(), bottomStyle, true);
                if (applyLeftInnerClip)
                    graphicsContext->restore();
            }

            if (bottomRight.width()) {
                int rightY = ty + h - bottomRight.height() * 2;
                int rightX = tx + w - bottomRight.width() * 2;
                bool applyRightInnerClip = (style->borderRightWidth() < bottomRight.width())
                    && (style->borderBottomWidth() < bottomRight.height())
                    && (bottomStyle != DOUBLE || style->borderBottomWidth() > 6);
                if (applyRightInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(rightX, rightY, bottomRight.width() * 2, bottomRight.height() * 2),
                                                             style->borderBottomWidth());
                }

                secondAngleStart = 270;
                secondAngleSpan = lowerRightBorderStylesMatch ? 90 : 45;

                // Draw lower right arc
                drawArcForBoxSide(graphicsContext, rightX, rightY, thickness, bottomRight, secondAngleStart, secondAngleSpan,
                              BSBottom, bottomColor, style->color(), bottomStyle, false);
                if (applyRightInnerClip)
                    graphicsContext->restore();
            }
        }
    }

    if (renderLeft) {
        bool ignore_top = (renderRadii && topLeft.height() > 0) ||
            (topColor == leftColor && topTransparent == leftTransparent && leftStyle >= OUTSET &&
             (topStyle == DOTTED || topStyle == DASHED || topStyle == SOLID || topStyle == OUTSET));

        bool ignore_bottom = (renderRadii && bottomLeft.height() > 0) ||
            (bottomColor == leftColor && bottomTransparent == leftTransparent && leftStyle >= OUTSET &&
             (bottomStyle == DOTTED || bottomStyle == DASHED || bottomStyle == SOLID || bottomStyle == INSET));

        int y = ty;
        int y2 = ty + h;
        if (renderRadii) {
            y += topLeft.height();
            y2 -= bottomLeft.height();
        }

        drawLineForBoxSide(graphicsContext, tx, y, tx + style->borderLeftWidth(), y2, BSLeft, leftColor, style->color(), leftStyle,
                   ignore_top ? 0 : style->borderTopWidth(), ignore_bottom ? 0 : style->borderBottomWidth());

        if (renderRadii && (!upperLeftBorderStylesMatch || !lowerLeftBorderStylesMatch)) {
            int topX = tx;
            thickness = style->borderLeftWidth() * 2;

            if (!upperLeftBorderStylesMatch && topLeft.width()) {
                int topY = ty;
                bool applyTopInnerClip = (style->borderLeftWidth() < topLeft.width())
                    && (style->borderTopWidth() < topLeft.height())
                    && (leftStyle != DOUBLE || style->borderLeftWidth() > 6);
                if (applyTopInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(topX, topY, topLeft.width() * 2, topLeft.height() * 2),
                                                             style->borderLeftWidth());
                }

                firstAngleStart = 135;
                firstAngleSpan = 45;

                // Draw top left arc
                drawArcForBoxSide(graphicsContext, topX, topY, thickness, topLeft, firstAngleStart, firstAngleSpan,
                              BSLeft, leftColor, style->color(), leftStyle, true);
                if (applyTopInnerClip)
                    graphicsContext->restore();
            }

            if (!lowerLeftBorderStylesMatch && bottomLeft.width()) {
                int bottomY = ty + h - bottomLeft.height() * 2;
                bool applyBottomInnerClip = (style->borderLeftWidth() < bottomLeft.width())
                    && (style->borderBottomWidth() < bottomLeft.height())
                    && (leftStyle != DOUBLE || style->borderLeftWidth() > 6);
                if (applyBottomInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(topX, bottomY, bottomLeft.width() * 2, bottomLeft.height() * 2),
                                                             style->borderLeftWidth());
                }

                secondAngleStart = 180;
                secondAngleSpan = 45;

                // Draw bottom left arc
                drawArcForBoxSide(graphicsContext, topX, bottomY, thickness, bottomLeft, secondAngleStart, secondAngleSpan,
                              BSLeft, leftColor, style->color(), leftStyle, false);
                if (applyBottomInnerClip)
                    graphicsContext->restore();
            }
        }
    }

    if (renderRight) {
        bool ignore_top = (renderRadii && topRight.height() > 0) ||
            ((topColor == rightColor) && (topTransparent == rightTransparent) &&
            (rightStyle >= DOTTED || rightStyle == INSET) &&
            (topStyle == DOTTED || topStyle == DASHED || topStyle == SOLID || topStyle == OUTSET));

        bool ignore_bottom = (renderRadii && bottomRight.height() > 0) ||
            ((bottomColor == rightColor) && (bottomTransparent == rightTransparent) &&
            (rightStyle >= DOTTED || rightStyle == INSET) &&
            (bottomStyle == DOTTED || bottomStyle == DASHED || bottomStyle == SOLID || bottomStyle == INSET));

        int y = ty;
        int y2 = ty + h;
        if (renderRadii) {
            y += topRight.height();
            y2 -= bottomRight.height();
        }

        drawLineForBoxSide(graphicsContext, tx + w - style->borderRightWidth(), y, tx + w, y2, BSRight, rightColor, style->color(), rightStyle,
                   ignore_top ? 0 : style->borderTopWidth(), ignore_bottom ? 0 : style->borderBottomWidth());

        if (renderRadii && (!upperRightBorderStylesMatch || !lowerRightBorderStylesMatch)) {
            thickness = style->borderRightWidth() * 2;

            if (!upperRightBorderStylesMatch && topRight.width()) {
                int topX = tx + w - topRight.width() * 2;
                int topY = ty;
                bool applyTopInnerClip = (style->borderRightWidth() < topRight.width())
                    && (style->borderTopWidth() < topRight.height())
                    && (rightStyle != DOUBLE || style->borderRightWidth() > 6);
                if (applyTopInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(topX, topY, topRight.width() * 2, topRight.height() * 2),
                                                             style->borderRightWidth());
                }

                firstAngleStart = 0;
                firstAngleSpan = 45;

                // Draw top right arc
                drawArcForBoxSide(graphicsContext, topX, topY, thickness, topRight, firstAngleStart, firstAngleSpan,
                              BSRight, rightColor, style->color(), rightStyle, true);
                if (applyTopInnerClip)
                    graphicsContext->restore();
            }

            if (!lowerRightBorderStylesMatch && bottomRight.width()) {
                int bottomX = tx + w - bottomRight.width() * 2;
                int bottomY = ty + h - bottomRight.height() * 2;
                bool applyBottomInnerClip = (style->borderRightWidth() < bottomRight.width())
                    && (style->borderBottomWidth() < bottomRight.height())
                    && (rightStyle != DOUBLE || style->borderRightWidth() > 6);
                if (applyBottomInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(bottomX, bottomY, bottomRight.width() * 2, bottomRight.height() * 2),
                                                             style->borderRightWidth());
                }

                secondAngleStart = 315;
                secondAngleSpan = 45;

                // Draw bottom right arc
                drawArcForBoxSide(graphicsContext, bottomX, bottomY, thickness, bottomRight, secondAngleStart, secondAngleSpan,
                              BSRight, rightColor, style->color(), rightStyle, false);
                if (applyBottomInnerClip)
                    graphicsContext->restore();
            }
        }
    }

    if (renderRadii)
        graphicsContext->restore();
}

void RenderBoxModelObject::paintBoxShadow(GraphicsContext* context, int tx, int ty, int w, int h, const RenderStyle* s, ShadowStyle shadowStyle, bool begin, bool end)
{
    // FIXME: Deal with border-image.  Would be great to use border-image as a mask.

    if (context->paintingDisabled())
        return;

    IntRect rect(tx, ty, w, h);
    IntSize topLeft;
    IntSize topRight;
    IntSize bottomLeft;
    IntSize bottomRight;

    bool hasBorderRadius = s->hasBorderRadius();
    if (hasBorderRadius && (begin || end)) {
        IntSize topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius;
        s->getBorderRadiiForRect(rect, topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius);

        if (begin) {
            if (shadowStyle == Inset) {
                topLeftRadius.expand(-borderLeft(), -borderTop());
                topLeftRadius.clampNegativeToZero();
                bottomLeftRadius.expand(-borderLeft(), -borderBottom());
                bottomLeftRadius.clampNegativeToZero();
            }
            topLeft = topLeftRadius;
            bottomLeft = bottomLeftRadius;
        }
        if (end) {
            if (shadowStyle == Inset) {
                topRightRadius.expand(-borderRight(), -borderTop());
                topRightRadius.clampNegativeToZero();
                bottomRightRadius.expand(-borderRight(), -borderBottom());
                bottomRightRadius.clampNegativeToZero();
            }
            topRight = topRightRadius;
            bottomRight = bottomRightRadius;
        }
    }

    if (shadowStyle == Inset) {
        rect.move(begin ? borderLeft() : 0, borderTop());
        rect.setWidth(rect.width() - (begin ? borderLeft() : 0) - (end ? borderRight() : 0));
        rect.setHeight(rect.height() - borderTop() - borderBottom());
    }

    bool hasOpaqueBackground = s->backgroundColor().isValid() && s->backgroundColor().alpha() == 255;
    for (ShadowData* shadow = s->boxShadow(); shadow; shadow = shadow->next) {
        if (shadow->style != shadowStyle)
            continue;

        IntSize shadowOffset(shadow->x, shadow->y);
        int shadowBlur = shadow->blur;
        int shadowSpread = shadow->spread;
        Color& shadowColor = shadow->color;

        if (shadow->style == Normal) {
            IntRect fillRect(rect);
            fillRect.inflate(shadowSpread);
            if (fillRect.isEmpty())
                continue;

            IntRect shadowRect(rect);
            shadowRect.inflate(shadowBlur + shadowSpread);
            shadowRect.move(shadowOffset);

            context->save();
            context->clip(shadowRect);

            // Move the fill just outside the clip, adding 1 pixel separation so that the fill does not
            // bleed in (due to antialiasing) if the context is transformed.
            IntSize extraOffset(w + max(0, shadowOffset.width()) + shadowBlur + 2 * shadowSpread + 1, 0);
            shadowOffset -= extraOffset;
            fillRect.move(extraOffset);

            context->setShadow(shadowOffset, shadowBlur, shadowColor, s->colorSpace());
            if (hasBorderRadius) {
                IntRect rectToClipOut = rect;
                IntSize topLeftToClipOut = topLeft;
                IntSize topRightToClipOut = topRight;
                IntSize bottomLeftToClipOut = bottomLeft;
                IntSize bottomRightToClipOut = bottomRight;

                if (shadowSpread < 0) {
                    topLeft.expand(shadowSpread, shadowSpread);
                    topLeft.clampNegativeToZero();

                    topRight.expand(shadowSpread, shadowSpread);
                    topRight.clampNegativeToZero();

                    bottomLeft.expand(shadowSpread, shadowSpread);
                    bottomLeft.clampNegativeToZero();

                    bottomRight.expand(shadowSpread, shadowSpread);
                    bottomRight.clampNegativeToZero();
                }

                // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
                // when painting the shadow. On the other hand, it introduces subpixel gaps along the
                // corners. Those are avoided by insetting the clipping path by one pixel.
                if (hasOpaqueBackground) {
                    rectToClipOut.inflate(-1);

                    topLeftToClipOut.expand(-1, -1);
                    topLeftToClipOut.clampNegativeToZero();

                    topRightToClipOut.expand(-1, -1);
                    topRightToClipOut.clampNegativeToZero();

                    bottomLeftToClipOut.expand(-1, -1);
                    bottomLeftToClipOut.clampNegativeToZero();

                    bottomRightToClipOut.expand(-1, -1);
                    bottomRightToClipOut.clampNegativeToZero();
                }

                if (!rectToClipOut.isEmpty())
                    context->clipOutRoundedRect(rectToClipOut, topLeftToClipOut, topRightToClipOut, bottomLeftToClipOut, bottomRightToClipOut);
                context->fillRoundedRect(fillRect, topLeft, topRight, bottomLeft, bottomRight, Color::black, s->colorSpace());
            } else {
                IntRect rectToClipOut = rect;

                // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
                // when painting the shadow. On the other hand, it introduces subpixel gaps along the
                // edges if they are not pixel-aligned. Those are avoided by insetting the clipping path
                // by one pixel.
                if (hasOpaqueBackground) {
                    AffineTransform currentTransformation = context->getCTM();
                    if (currentTransformation.a() != 1 || (currentTransformation.d() != 1 && currentTransformation.d() != -1)
                            || currentTransformation.b() || currentTransformation.c())
                        rectToClipOut.inflate(-1);
                }

                if (!rectToClipOut.isEmpty())
                    context->clipOut(rectToClipOut);
                context->fillRect(fillRect, Color::black, s->colorSpace());
            }

            context->restore();
        } else {
            // Inset shadow.
            IntRect holeRect(rect);
            holeRect.inflate(-shadowSpread);

            if (holeRect.isEmpty()) {
                if (hasBorderRadius)
                    context->fillRoundedRect(rect, topLeft, topRight, bottomLeft, bottomRight, shadowColor, s->colorSpace());
                else
                    context->fillRect(rect, shadowColor, s->colorSpace());
                continue;
            }
            if (!begin) {
                holeRect.move(-max(shadowOffset.width(), 0) - shadowBlur, 0);
                holeRect.setWidth(holeRect.width() + max(shadowOffset.width(), 0) + shadowBlur);
            }
            if (!end)
                holeRect.setWidth(holeRect.width() - min(shadowOffset.width(), 0) + shadowBlur);

            Color fillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), 255);

            IntRect outerRect(rect);
            outerRect.inflateX(w - 2 * shadowSpread);
            outerRect.inflateY(h - 2 * shadowSpread);

            context->save();

            if (hasBorderRadius)
                context->clip(Path::createRoundedRectangle(rect, topLeft, topRight, bottomLeft, bottomRight));
            else
                context->clip(rect);

            IntSize extraOffset(2 * w + max(0, shadowOffset.width()) + shadowBlur - 2 * shadowSpread + 1, 0);
            context->translate(extraOffset.width(), extraOffset.height());
            shadowOffset -= extraOffset;

            context->beginPath();
            context->addPath(Path::createRectangle(outerRect));

            if (hasBorderRadius) {
                if (shadowSpread > 0) {
                    topLeft.expand(-shadowSpread, -shadowSpread);
                    topLeft.clampNegativeToZero();

                    topRight.expand(-shadowSpread, -shadowSpread);
                    topRight.clampNegativeToZero();

                    bottomLeft.expand(-shadowSpread, -shadowSpread);
                    bottomLeft.clampNegativeToZero();

                    bottomRight.expand(-shadowSpread, -shadowSpread);
                    bottomRight.clampNegativeToZero();
                }
                context->addPath(Path::createRoundedRectangle(holeRect, topLeft, topRight, bottomLeft, bottomRight));
            } else
                context->addPath(Path::createRectangle(holeRect));

            context->setFillRule(RULE_EVENODD);
            context->setFillColor(fillColor, s->colorSpace());
            context->setShadow(shadowOffset, shadowBlur, shadowColor, s->colorSpace());
            context->fillPath();

            context->restore();
        }
    }
}

int RenderBoxModelObject::containingBlockWidthForContent() const
{
    return containingBlock()->availableWidth();
}

} // namespace WebCore
