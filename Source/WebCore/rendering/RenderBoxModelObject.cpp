/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "Page.h"
#include "Path.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "Settings.h"
#include "TransformState.h"
#include <wtf/CurrentTime.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

bool RenderBoxModelObject::s_wasFloating = false;
bool RenderBoxModelObject::s_hadLayer = false;
bool RenderBoxModelObject::s_layerWasSelfPainting = false;

static const double cInterpolationCutoff = 800. * 800.;
static const double cLowQualityTimeThreshold = 0.500; // 500 ms

typedef HashMap<const void*, LayoutSize> LayerSizeMap;
typedef HashMap<RenderBoxModelObject*, LayerSizeMap> ObjectLayerSizeMap;

// The HashMap for storing continuation pointers.
// An inline can be split with blocks occuring in between the inline content.
// When this occurs we need a pointer to the next object. We can basically be
// split into a sequence of inlines and blocks. The continuation will either be
// an anonymous block (that houses other blocks) or it will be an inline flow.
// <b><i><p>Hello</p></i></b>. In this example the <i> will have a block as
// its continuation but the <b> will just have an inline as its continuation.
typedef HashMap<const RenderBoxModelObject*, RenderBoxModelObject*> ContinuationMap;
static ContinuationMap* continuationMap = 0;

class ImageQualityController {
    WTF_MAKE_NONCOPYABLE(ImageQualityController); WTF_MAKE_FAST_ALLOCATED;
public:
    ImageQualityController();
    bool shouldPaintAtLowQuality(GraphicsContext*, RenderBoxModelObject*, Image*, const void* layer, const LayoutSize&);
    void removeLayer(RenderBoxModelObject*, LayerSizeMap* innerMap, const void* layer);
    void set(RenderBoxModelObject*, LayerSizeMap* innerMap, const void* layer, const LayoutSize&);
    void objectDestroyed(RenderBoxModelObject*);
    bool isEmpty() { return m_objectLayerSizeMap.isEmpty(); }

private:
    void highQualityRepaintTimerFired(Timer<ImageQualityController>*);
    void restartTimer();

    ObjectLayerSizeMap m_objectLayerSizeMap;
    Timer<ImageQualityController> m_timer;
    bool m_animatedResizeIsActive;
};

ImageQualityController::ImageQualityController()
    : m_timer(this, &ImageQualityController::highQualityRepaintTimerFired)
    , m_animatedResizeIsActive(false)
{
}

void ImageQualityController::removeLayer(RenderBoxModelObject* object, LayerSizeMap* innerMap, const void* layer)
{
    if (innerMap) {
        innerMap->remove(layer);
        if (innerMap->isEmpty())
            objectDestroyed(object);
    }
}
    
void ImageQualityController::set(RenderBoxModelObject* object, LayerSizeMap* innerMap, const void* layer, const LayoutSize& size)
{
    if (innerMap)
        innerMap->set(layer, size);
    else {
        LayerSizeMap newInnerMap;
        newInnerMap.set(layer, size);
        m_objectLayerSizeMap.set(object, newInnerMap);
    }
}
    
void ImageQualityController::objectDestroyed(RenderBoxModelObject* object)
{
    m_objectLayerSizeMap.remove(object);
    if (m_objectLayerSizeMap.isEmpty()) {
        m_animatedResizeIsActive = false;
        m_timer.stop();
    }
}

void ImageQualityController::highQualityRepaintTimerFired(Timer<ImageQualityController>*)
{
    if (m_animatedResizeIsActive) {
        m_animatedResizeIsActive = false;
        for (ObjectLayerSizeMap::iterator it = m_objectLayerSizeMap.begin(); it != m_objectLayerSizeMap.end(); ++it)
            it->first->repaint();
    }
}

void ImageQualityController::restartTimer()
{
    m_timer.startOneShot(cLowQualityTimeThreshold);
}

bool ImageQualityController::shouldPaintAtLowQuality(GraphicsContext* context, RenderBoxModelObject* object, Image* image, const void *layer, const LayoutSize& size)
{
    // If the image is not a bitmap image, then none of this is relevant and we just paint at high
    // quality.
    if (!image || !image->isBitmapImage() || context->paintingDisabled())
        return false;

    if (object->style()->imageRendering() == ImageRenderingOptimizeContrast)
        return true;
    
    // Make sure to use the unzoomed image size, since if a full page zoom is in effect, the image
    // is actually being scaled.
    IntSize imageSize(image->width(), image->height());

    // Look ourselves up in the hashtables.
    ObjectLayerSizeMap::iterator i = m_objectLayerSizeMap.find(object);
    LayerSizeMap* innerMap = i != m_objectLayerSizeMap.end() ? &i->second : 0;
    LayoutSize oldSize;
    bool isFirstResize = true;
    if (innerMap) {
        LayerSizeMap::iterator j = innerMap->find(layer);
        if (j != innerMap->end()) {
            isFirstResize = false;
            oldSize = j->second;
        }
    }

    const AffineTransform& currentTransform = context->getCTM();
    bool contextIsScaled = !currentTransform.isIdentityOrTranslationOrFlipped();
    // FIXME: Change to use roughlyEquals when we move to float.
    // See https://bugs.webkit.org/show_bug.cgi?id=66148
    if (!contextIsScaled && size == imageSize) {
        // There is no scale in effect. If we had a scale in effect before, we can just remove this object from the list.
        removeLayer(object, innerMap, layer);
        return false;
    }

    // There is no need to hash scaled images that always use low quality mode when the page demands it. This is the iChat case.
    if (object->document()->page()->inLowQualityImageInterpolationMode()) {
        double totalPixels = static_cast<double>(image->width()) * static_cast<double>(image->height());
        if (totalPixels > cInterpolationCutoff)
            return true;
    }

    // If an animated resize is active, paint in low quality and kick the timer ahead.
    if (m_animatedResizeIsActive) {
        set(object, innerMap, layer, size);
        restartTimer();
        return true;
    }
    // If this is the first time resizing this image, or its size is the
    // same as the last resize, draw at high res, but record the paint
    // size and set the timer.
    // FIXME: Change to use roughlyEquals when we move to float.
    // See https://bugs.webkit.org/show_bug.cgi?id=66148
    if (isFirstResize || oldSize == size) {
        restartTimer();
        set(object, innerMap, layer, size);
        return false;
    }
    // If the timer is no longer active, draw at high quality and don't
    // set the timer.
    if (!m_timer.isActive()) {
        removeLayer(object, innerMap, layer);
        return false;
    }
    // This object has been resized to two different sizes while the timer
    // is active, so draw at low quality, set the flag for animated resizes and
    // the object to the list for high quality redraw.
    set(object, innerMap, layer, size);
    m_animatedResizeIsActive = true;
    restartTimer();
    return true;
}

static ImageQualityController* gImageQualityController = 0;

static ImageQualityController* imageQualityController()
{
    if (!gImageQualityController)
        gImageQualityController = new ImageQualityController;

    return gImageQualityController;
}

void RenderBoxModelObject::setSelectionState(SelectionState s)
{
    if (selectionState() == s)
        return;
    
    if (s == SelectionInside && selectionState() != SelectionNone)
        return;

    if ((s == SelectionStart && selectionState() == SelectionEnd)
        || (s == SelectionEnd && selectionState() == SelectionStart))
        RenderObject::setSelectionState(SelectionBoth);
    else
        RenderObject::setSelectionState(s);
    
    // FIXME:
    // We should consider whether it is OK propagating to ancestor RenderInlines.
    // This is a workaround for http://webkit.org/b/32123
    RenderBlock* cb = containingBlock();
    if (cb && !cb->isRenderView())
        cb->setSelectionState(s);
}

bool RenderBoxModelObject::shouldPaintAtLowQuality(GraphicsContext* context, Image* image, const void* layer, const LayoutSize& size)
{
    return imageQualityController()->shouldPaintAtLowQuality(context, this, image, layer, size);
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
    if (gImageQualityController) {
        gImageQualityController->objectDestroyed(this);
        if (gImageQualityController->isEmpty()) {
            delete gImageQualityController;
            gImageQualityController = 0;
        }
    }
}

void RenderBoxModelObject::destroyLayer()
{
    ASSERT(!hasLayer()); // Callers should have already called setHasLayer(false)
    ASSERT(m_layer);
    m_layer->destroy(renderArena());
    m_layer = 0;
}

void RenderBoxModelObject::willBeDestroyed()
{
    // This must be done before we destroy the RenderObject.
    if (m_layer)
        m_layer->clearClipRects();

    // A continuation of this RenderObject should be destroyed at subclasses.
    ASSERT(!continuation());

    // RenderObject::willBeDestroyed calls back to destroyLayer() for layer destruction
    RenderObject::willBeDestroyed();
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
        
        if (diff == StyleDifferenceLayout || diff == StyleDifferenceSimplifiedLayout) {
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
            if (parent() && !needsLayout() && containingBlock()) {
                m_layer->setRepaintStatus(NeedsFullRepaint);
                // There is only one layer to update, it is not worth using |cachedOffset| since
                // we are not sure the value will be used.
                m_layer->updateLayerPositions(0);
            }
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
    setHasBoxDecorations(hasBackground() || style()->hasBorder() || style()->hasAppearance() || style()->boxShadow());
    setInline(style()->isDisplayInlineType());
    setRelPositioned(style()->position() == RelativePosition);
    setHorizontalWritingMode(style()->isHorizontalWritingMode());
}

LayoutUnit RenderBoxModelObject::relativePositionOffsetX() const
{
    // Objects that shrink to avoid floats normally use available line width when computing containing block width.  However
    // in the case of relative positioning using percentages, we can't do this.  The offset should always be resolved using the
    // available width of the containing block.  Therefore we don't use containingBlockLogicalWidthForContent() here, but instead explicitly
    // call availableWidth on our containing block.
    if (!style()->left().isAuto()) {
        RenderBlock* cb = containingBlock();
        if (!style()->right().isAuto() && !cb->style()->isLeftToRightDirection())
            return -style()->right().calcValue(cb->availableWidth());
        return style()->left().calcValue(cb->availableWidth());
    }
    if (!style()->right().isAuto()) {
        RenderBlock* cb = containingBlock();
        return -style()->right().calcValue(cb->availableWidth());
    }
    return 0;
}

LayoutUnit RenderBoxModelObject::relativePositionOffsetY() const
{
    RenderBlock* containingBlock = this->containingBlock();

    // If the containing block of a relatively positioned element does not
    // specify a height, a percentage top or bottom offset should be resolved as
    // auto. An exception to this is if the containing block has the WinIE quirk
    // where <html> and <body> assume the size of the viewport. In this case,
    // calculate the percent offset based on this height.
    // See <https://bugs.webkit.org/show_bug.cgi?id=26396>.
    if (!style()->top().isAuto()
        && (!containingBlock->style()->height().isAuto()
            || !style()->top().isPercent()
            || containingBlock->stretchesToViewport()))
        return style()->top().calcValue(containingBlock->availableHeight());

    if (!style()->bottom().isAuto()
        && (!containingBlock->style()->height().isAuto()
            || !style()->bottom().isPercent()
            || containingBlock->stretchesToViewport()))
        return -style()->bottom().calcValue(containingBlock->availableHeight());

    return 0;
}

LayoutUnit RenderBoxModelObject::offsetLeft() const
{
    // If the element is the HTML body element or does not have an associated box
    // return 0 and stop this algorithm.
    if (isBody())
        return 0;
    
    RenderBoxModelObject* offsetPar = offsetParent();
    LayoutUnit xPos = (isBox() ? toRenderBox(this)->left() : 0);
    
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
                    xPos += toRenderBox(curr)->left();
                curr = curr->parent();
            }
            if (offsetPar->isBox() && offsetPar->isBody() && !offsetPar->isRelPositioned() && !offsetPar->isPositioned())
                xPos += toRenderBox(offsetPar)->left();
        }
    }

    return xPos;
}

LayoutUnit RenderBoxModelObject::offsetTop() const
{
    // If the element is the HTML body element or does not have an associated box
    // return 0 and stop this algorithm.
    if (isBody())
        return 0;
    
    RenderBoxModelObject* offsetPar = offsetParent();
    LayoutUnit yPos = (isBox() ? toRenderBox(this)->top() : 0);
    
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
                    yPos += toRenderBox(curr)->top();
                curr = curr->parent();
            }
            if (offsetPar->isBox() && offsetPar->isBody() && !offsetPar->isRelPositioned() && !offsetPar->isPositioned())
                yPos += toRenderBox(offsetPar)->top();
        }
    }
    return yPos;
}

LayoutUnit RenderBoxModelObject::paddingTop(bool) const
{
    LayoutUnit w = 0;
    Length padding = style()->paddingTop();
    if (padding.isPercent())
        w = containingBlock()->availableLogicalWidth();
    return padding.calcMinValue(w);
}

LayoutUnit RenderBoxModelObject::paddingBottom(bool) const
{
    LayoutUnit w = 0;
    Length padding = style()->paddingBottom();
    if (padding.isPercent())
        w = containingBlock()->availableLogicalWidth();
    return padding.calcMinValue(w);
}

LayoutUnit RenderBoxModelObject::paddingLeft(bool) const
{
    LayoutUnit w = 0;
    Length padding = style()->paddingLeft();
    if (padding.isPercent())
        w = containingBlock()->availableLogicalWidth();
    return padding.calcMinValue(w);
}

LayoutUnit RenderBoxModelObject::paddingRight(bool) const
{
    LayoutUnit w = 0;
    Length padding = style()->paddingRight();
    if (padding.isPercent())
        w = containingBlock()->availableLogicalWidth();
    return padding.calcMinValue(w);
}

LayoutUnit RenderBoxModelObject::paddingBefore(bool) const
{
    LayoutUnit w = 0;
    Length padding = style()->paddingBefore();
    if (padding.isPercent())
        w = containingBlock()->availableLogicalWidth();
    return padding.calcMinValue(w);
}

LayoutUnit RenderBoxModelObject::paddingAfter(bool) const
{
    LayoutUnit w = 0;
    Length padding = style()->paddingAfter();
    if (padding.isPercent())
        w = containingBlock()->availableLogicalWidth();
    return padding.calcMinValue(w);
}

LayoutUnit RenderBoxModelObject::paddingStart(bool) const
{
    LayoutUnit w = 0;
    Length padding = style()->paddingStart();
    if (padding.isPercent())
        w = containingBlock()->availableLogicalWidth();
    return padding.calcMinValue(w);
}

LayoutUnit RenderBoxModelObject::paddingEnd(bool) const
{
    LayoutUnit w = 0;
    Length padding = style()->paddingEnd();
    if (padding.isPercent())
        w = containingBlock()->availableLogicalWidth();
    return padding.calcMinValue(w);
}

RoundedRect RenderBoxModelObject::getBackgroundRoundedRect(const LayoutRect& borderRect, InlineFlowBox* box, LayoutUnit inlineBoxWidth, LayoutUnit inlineBoxHeight,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    RoundedRect border = style()->getRoundedBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);
    if (box && (box->nextLineBox() || box->prevLineBox())) {
        RoundedRect segmentBorder = style()->getRoundedBorderFor(LayoutRect(0, 0, inlineBoxWidth, inlineBoxHeight), includeLogicalLeftEdge, includeLogicalRightEdge);
        border.setRadii(segmentBorder.radii());
    }

    return border;
}

static LayoutRect backgroundRectAdjustedForBleedAvoidance(GraphicsContext* context, const LayoutRect& borderRect, BackgroundBleedAvoidance bleedAvoidance)
{
    if (bleedAvoidance != BackgroundBleedShrinkBackground)
        return borderRect;

    // We shrink the rectangle by one pixel on each side because the bleed is one pixel maximum.
    AffineTransform transform = context->getCTM();
    LayoutRect adjustedRect = borderRect;
    adjustedRect.inflateX(-static_cast<LayoutUnit>(ceil(1 / transform.xScale())));
    adjustedRect.inflateY(-static_cast<LayoutUnit>(ceil(1 / transform.yScale())));
    return adjustedRect;
}

void RenderBoxModelObject::paintFillLayerExtended(const PaintInfo& paintInfo, const Color& color, const FillLayer* bgLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, InlineFlowBox* box, const LayoutSize& boxSize, CompositeOperator op, RenderObject* backgroundObject)
{
    GraphicsContext* context = paintInfo.context;
    if (context->paintingDisabled() || rect.isEmpty())
        return;

    bool includeLeftEdge = box ? box->includeLogicalLeftEdge() : true;
    bool includeRightEdge = box ? box->includeLogicalRightEdge() : true;

    bool hasRoundedBorder = style()->hasBorderRadius() && (includeLeftEdge || includeRightEdge);
    bool clippedWithLocalScrolling = hasOverflowClip() && bgLayer->attachment() == LocalBackgroundAttachment;
    bool isBorderFill = bgLayer->clip() == BorderFillBox;
    bool isRoot = this->isRoot();

    Color bgColor = color;
    StyleImage* bgImage = bgLayer->image();
    bool shouldPaintBackgroundImage = bgImage && bgImage->canRender(this, style()->effectiveZoom());
    
    bool forceBackgroundToWhite = false;
    if (document()->printing()) {
        if (style()->printColorAdjust() == PrintColorAdjustEconomy)
            forceBackgroundToWhite = true;
        if (document()->settings() && document()->settings()->shouldPrintBackgrounds())
            forceBackgroundToWhite = false;
    }

    // When printing backgrounds is disabled or using economy mode,
    // change existing background colors and images to a solid white background.
    // If there's no bg color or image, leave it untouched to avoid affecting transparency.
    // We don't try to avoid loading the background images, because this style flag is only set
    // when printing, and at that point we've already loaded the background images anyway. (To avoid
    // loading the background images we'd have to do this check when applying styles rather than
    // while rendering.)
    if (forceBackgroundToWhite) {
        // Note that we can't reuse this variable below because the bgColor might be changed
        bool shouldPaintBackgroundColor = !bgLayer->next() && bgColor.isValid() && bgColor.alpha() > 0;
        if (shouldPaintBackgroundImage || shouldPaintBackgroundColor) {
            bgColor = Color::white;
            shouldPaintBackgroundImage = false;
        }
    }

    bool colorVisible = bgColor.isValid() && bgColor.alpha() > 0;
    
    // Fast path for drawing simple color backgrounds.
    if (!isRoot && !clippedWithLocalScrolling && !shouldPaintBackgroundImage && isBorderFill && !bgLayer->next()) {
        if (!colorVisible)
            return;

        if (hasRoundedBorder && bleedAvoidance != BackgroundBleedUseTransparencyLayer) {
            RoundedRect border = getBackgroundRoundedRect(backgroundRectAdjustedForBleedAvoidance(context, rect, bleedAvoidance), box, boxSize.width(), boxSize.height(), includeLeftEdge, includeRightEdge);
            context->fillRoundedRect(border, bgColor, style()->colorSpace());
        } else
            context->fillRect(rect, bgColor, style()->colorSpace());
        
        return;
    }

    bool clipToBorderRadius = hasRoundedBorder && bleedAvoidance != BackgroundBleedUseTransparencyLayer;
    GraphicsContextStateSaver clipToBorderStateSaver(*context, clipToBorderRadius);
    if (clipToBorderRadius) {
        RoundedRect border = getBackgroundRoundedRect(backgroundRectAdjustedForBleedAvoidance(context, rect, bleedAvoidance), box, boxSize.width(), boxSize.height(), includeLeftEdge, includeRightEdge);
        context->addRoundedRectClip(border);
    }
    
    LayoutUnit bLeft = includeLeftEdge ? borderLeft() : 0;
    LayoutUnit bRight = includeRightEdge ? borderRight() : 0;
    LayoutUnit pLeft = includeLeftEdge ? paddingLeft() : 0;
    LayoutUnit pRight = includeRightEdge ? paddingRight() : 0;

    GraphicsContextStateSaver clipWithScrollingStateSaver(*context, clippedWithLocalScrolling);
    LayoutRect scrolledPaintRect = rect;
    if (clippedWithLocalScrolling) {
        // Clip to the overflow area.
        context->clip(toRenderBox(this)->overflowClipRect(rect.location(), paintInfo.renderRegion));
        
        // Adjust the paint rect to reflect a scrolled content box with borders at the ends.
        LayoutSize offset = layer()->scrolledContentOffset();
        scrolledPaintRect.move(-offset);
        scrolledPaintRect.setWidth(bLeft + layer()->scrollWidth() + bRight);
        scrolledPaintRect.setHeight(borderTop() + layer()->scrollHeight() + borderBottom());
    }
    
    GraphicsContextStateSaver backgroundClipStateSaver(*context, false);
    if (bgLayer->clip() == PaddingFillBox || bgLayer->clip() == ContentFillBox) {
        // Clip to the padding or content boxes as necessary.
        bool includePadding = bgLayer->clip() == ContentFillBox;
        LayoutRect clipRect = LayoutRect(scrolledPaintRect.x() + bLeft + (includePadding ? pLeft : 0),
                                   scrolledPaintRect.y() + borderTop() + (includePadding ? paddingTop() : 0),
                                   scrolledPaintRect.width() - bLeft - bRight - (includePadding ? pLeft + pRight : 0),
                                   scrolledPaintRect.height() - borderTop() - borderBottom() - (includePadding ? paddingTop() + paddingBottom() : 0));
        backgroundClipStateSaver.save();
        context->clip(clipRect);
    } else if (bgLayer->clip() == TextFillBox) {
        // We have to draw our text into a mask that can then be used to clip background drawing.
        // First figure out how big the mask has to be.  It should be no bigger than what we need
        // to actually render, so we should intersect the dirty rect with the border box of the background.
        LayoutRect maskRect = rect;
        maskRect.intersect(paintInfo.rect);
        
        // Now create the mask.
        OwnPtr<ImageBuffer> maskImage = context->createCompatibleBuffer(maskRect.size());
        if (!maskImage)
            return;
        
        GraphicsContext* maskImageContext = maskImage->context();
        maskImageContext->translate(-maskRect.x(), -maskRect.y());
        
        // Now add the text to the clip.  We do this by painting using a special paint phase that signals to
        // InlineTextBoxes that they should just add their contents to the clip.
        PaintInfo info(maskImageContext, maskRect, PaintPhaseTextClip, true, 0, paintInfo.renderRegion, 0);
        if (box) {
            RootInlineBox* root = box->root();
            box->paint(info, LayoutPoint(scrolledPaintRect.x() - box->x(), scrolledPaintRect.y() - box->y()), root->lineTop(), root->lineBottom());
        } else {
            LayoutSize localOffset = isBox() ? toRenderBox(this)->locationOffset() : LayoutSize();
            paint(info, scrolledPaintRect.location() - localOffset);
        }
        
        // The mask has been created.  Now we just need to clip to it.
        backgroundClipStateSaver.save();
        context->clipToImageBuffer(maskImage.get(), maskRect);
    }
    
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
#if ENABLE(SVG)
                    else {
                        // SVG documents and XML documents with SVG root nodes are transparent.
                        isOpaqueRoot = !document()->hasSVGRootNode();
                    }
#endif
                }
            } else
                isOpaqueRoot = !view()->frameView()->isTransparent();
        }
        view()->frameView()->setContentIsOpaque(isOpaqueRoot);
    }

    // Paint the color first underneath all images.
    if (!bgLayer->next()) {
        LayoutRect backgroundRect(scrolledPaintRect);
        backgroundRect.intersect(paintInfo.rect);
        // If we have an alpha and we are painting the root element, go ahead and blend with the base background color.
        Color baseColor;
        bool shouldClearBackground = false;
        if (isOpaqueRoot) {
            baseColor = view()->frameView()->baseBackgroundColor();
            if (!baseColor.alpha())
                shouldClearBackground = true;
        }

        if (baseColor.alpha()) {
            if (bgColor.alpha())
                baseColor = baseColor.blend(bgColor);

            context->fillRect(backgroundRect, baseColor, style()->colorSpace(), CompositeCopy);
        } else if (bgColor.alpha()) {
            CompositeOperator operation = shouldClearBackground ? CompositeCopy : context->compositeOperation();
            context->fillRect(backgroundRect, bgColor, style()->colorSpace(), operation);
        } else if (shouldClearBackground)
            context->clearRect(backgroundRect);
    }

    // no progressive loading of the background image
    if (shouldPaintBackgroundImage) {
        BackgroundImageGeometry geometry;
        calculateBackgroundImageGeometry(bgLayer, scrolledPaintRect, geometry);
        geometry.clip(paintInfo.rect);
        if (!geometry.destRect().isEmpty()) {
            CompositeOperator compositeOp = op == CompositeSourceOver ? bgLayer->composite() : op;
            RenderObject* clientForBackgroundImage = backgroundObject ? backgroundObject : this;
            RefPtr<Image> image = bgImage->image(clientForBackgroundImage, geometry.tileSize());
            bool useLowQualityScaling = shouldPaintAtLowQuality(context, image.get(), bgLayer, geometry.tileSize());
            context->drawTiledImage(image.get(), style()->colorSpace(), geometry.destRect(), geometry.relativePhase(), geometry.tileSize(), 
                compositeOp, useLowQualityScaling);
        }
    }
}

static inline LayoutUnit resolveWidthForRatio(LayoutUnit height, const FloatSize& intrinsicRatio)
{
    // FIXME: Remove unnecessary rounding when layout is off ints: webkit.org/b/63656
    return static_cast<LayoutUnit>(ceilf(height * intrinsicRatio.width() / intrinsicRatio.height()));
}

static inline LayoutUnit resolveHeightForRatio(LayoutUnit width, const FloatSize& intrinsicRatio)
{
    // FIXME: Remove unnecessary rounding when layout is off ints: webkit.org/b/63656
    return static_cast<LayoutUnit>(ceilf(width * intrinsicRatio.height() / intrinsicRatio.width()));
}

static inline LayoutSize resolveAgainstIntrinsicWidthOrHeightAndRatio(const LayoutSize& size, const FloatSize& intrinsicRatio, LayoutUnit useWidth, LayoutUnit useHeight)
{
    if (intrinsicRatio.isEmpty()) {
        if (useWidth)
            return LayoutSize(useWidth, size.height());
        return LayoutSize(size.width(), useHeight);
    }

    if (useWidth)
        return LayoutSize(useWidth, resolveHeightForRatio(useWidth, intrinsicRatio));
    return LayoutSize(resolveWidthForRatio(useHeight, intrinsicRatio), useHeight);
}

static inline LayoutSize resolveAgainstIntrinsicRatio(const LayoutSize& size, const FloatSize& intrinsicRatio)
{
    // Two possible solutions: (size.width(), solutionHeight) or (solutionWidth, size.height())
    // "... must be assumed to be the largest dimensions..." = easiest answer: the rect with the largest surface area.

    LayoutUnit solutionWidth = resolveWidthForRatio(size.height(), intrinsicRatio);
    LayoutUnit solutionHeight = resolveHeightForRatio(size.width(), intrinsicRatio);
    if (solutionWidth <= size.width()) {
        if (solutionHeight <= size.height()) {
            // If both solutions fit, choose the one covering the larger area.
            LayoutUnit areaOne = solutionWidth * size.height();
            LayoutUnit areaTwo = size.width() * solutionHeight;
            if (areaOne < areaTwo)
                return LayoutSize(size.width(), solutionHeight);
            return LayoutSize(solutionWidth, size.height());
        }

        // Only the first solution fits.
        return LayoutSize(solutionWidth, size.height());
    }

    // Only the second solution fits, assert that.
    ASSERT(solutionHeight <= size.height());
    return LayoutSize(size.width(), solutionHeight);
}

IntSize RenderBoxModelObject::calculateImageIntrinsicDimensions(StyleImage* image, const IntSize& positioningAreaSize) const
{
    int resolvedWidth = 0;
    int resolvedHeight = 0;
    FloatSize intrinsicRatio;

    // A generated image without a fixed size, will always return the container size as intrinsic size.
    if (image->isGeneratedImage() && image->usesImageContainerSize()) {
        resolvedWidth = positioningAreaSize.width();
        resolvedHeight = positioningAreaSize.height();
    } else {
        Length intrinsicWidth;
        Length intrinsicHeight;
        image->computeIntrinsicDimensions(this, intrinsicWidth, intrinsicHeight, intrinsicRatio);

        // FIXME: Remove unnecessary rounding when layout is off ints: webkit.org/b/63656
        if (intrinsicWidth.isFixed())
            resolvedWidth = static_cast<int>(ceilf(intrinsicWidth.value() * style()->effectiveZoom()));
        if (intrinsicHeight.isFixed())
            resolvedHeight = static_cast<int>(ceilf(intrinsicHeight.value() * style()->effectiveZoom()));
    }

    // Intrinsic dimensions expressed as percentages must be resolved relative to the dimensions of the rectangle
    // that establishes the coordinate system for the 'background-position' property. SVG on the other hand
    // _explicitely_ says that percentage values for the width/height attributes do NOT define intrinsic dimensions.
    if (resolvedWidth > 0 && resolvedHeight > 0)
        return IntSize(resolvedWidth, resolvedHeight);

    // If the image has one of either an intrinsic width or an intrinsic height:
    // * and an intrinsic aspect ratio, then the missing dimension is calculated from the given dimension and the ratio.
    // * and no intrinsic aspect ratio, then the missing dimension is assumed to be the size of the rectangle that
    //   establishes the coordinate system for the 'background-position' property.
    if ((resolvedWidth && !resolvedHeight) || (!resolvedWidth && resolvedHeight))
        return resolveAgainstIntrinsicWidthOrHeightAndRatio(positioningAreaSize, intrinsicRatio, resolvedWidth, resolvedHeight);

    // If the image has no intrinsic dimensions and has an intrinsic ratio the dimensions must be assumed to be the
    // largest dimensions at that ratio such that neither dimension exceeds the dimensions of the rectangle that
    // establishes the coordinate system for the 'background-position' property.
    if (!resolvedWidth && !resolvedHeight && !intrinsicRatio.isEmpty())
        return resolveAgainstIntrinsicRatio(positioningAreaSize, intrinsicRatio);

    // If the image has no intrinsic ratio either, then the dimensions must be assumed to be the rectangle that
    // establishes the coordinate system for the 'background-position' property.
    return positioningAreaSize;
}

IntSize RenderBoxModelObject::calculateFillTileSize(const FillLayer* fillLayer, const IntSize& positioningAreaSize) const
{
    StyleImage* image = fillLayer->image();
    EFillSizeType type = fillLayer->size().type;

    IntSize imageIntrinsicSize = calculateImageIntrinsicDimensions(image, positioningAreaSize);

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
            if (layerWidth.isAuto() && !layerHeight.isAuto()) {
                if (imageIntrinsicSize.height())
                    w = imageIntrinsicSize.width() * h / imageIntrinsicSize.height();        
            } else if (!layerWidth.isAuto() && layerHeight.isAuto()) {
                if (imageIntrinsicSize.width())
                    h = imageIntrinsicSize.height() * w / imageIntrinsicSize.width();
            } else if (layerWidth.isAuto() && layerHeight.isAuto()) {
                // If both width and height are auto, use the image's intrinsic size.
                w = imageIntrinsicSize.width();
                h = imageIntrinsicSize.height();
            }
            
            return IntSize(max(1, w), max(1, h));
        }
        case SizeNone: {
            // If both values are ‘auto’ then the intrinsic width and/or height of the image should be used, if any.
            if (!imageIntrinsicSize.isEmpty())
                return imageIntrinsicSize;

            // If the image has neither an intrinsic width nor an intrinsic height, its size is determined as for ‘contain’.
            type = Contain;
        }
        case Contain:
        case Cover: {
            float horizontalScaleFactor = imageIntrinsicSize.width()
                ? static_cast<float>(positioningAreaSize.width()) / imageIntrinsicSize.width() : 1;
            float verticalScaleFactor = imageIntrinsicSize.height()
                ? static_cast<float>(positioningAreaSize.height()) / imageIntrinsicSize.height() : 1;
            float scaleFactor = type == Contain ? min(horizontalScaleFactor, verticalScaleFactor) : max(horizontalScaleFactor, verticalScaleFactor);
            return IntSize(max(1, static_cast<int>(imageIntrinsicSize.width() * scaleFactor)), max(1, static_cast<int>(imageIntrinsicSize.height() * scaleFactor)));
       }
    }

    ASSERT_NOT_REACHED();
    return IntSize();
}

void RenderBoxModelObject::BackgroundImageGeometry::setNoRepeatX(int xOffset)
{
    m_destRect.move(max(xOffset, 0), 0);
    m_phase.setX(-min(xOffset, 0));
    m_destRect.setWidth(m_tileSize.width() + min(xOffset, 0));
}
void RenderBoxModelObject::BackgroundImageGeometry::setNoRepeatY(int yOffset)
{
    m_destRect.move(0, max(yOffset, 0));
    m_phase.setY(-min(yOffset, 0));
    m_destRect.setHeight(m_tileSize.height() + min(yOffset, 0));
}

void RenderBoxModelObject::BackgroundImageGeometry::useFixedAttachment(const IntPoint& attachmentPoint)
{
    IntPoint alignedPoint = attachmentPoint;
    m_phase.move(max(alignedPoint.x() - m_destRect.x(), 0), max(alignedPoint.y() - m_destRect.y(), 0));
}

void RenderBoxModelObject::BackgroundImageGeometry::clip(const IntRect& clipRect)
{
    m_destRect.intersect(clipRect);
}

IntPoint RenderBoxModelObject::BackgroundImageGeometry::relativePhase() const
{
    IntPoint phase = m_phase;
    phase += m_destRect.location() - m_destOrigin;
    return phase;
}

void RenderBoxModelObject::calculateBackgroundImageGeometry(const FillLayer* fillLayer, const LayoutRect& paintRect, 
                                                            BackgroundImageGeometry& geometry)
{
    LayoutUnit left = 0;
    LayoutUnit top = 0;
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
        geometry.setDestRect(paintRect);

        LayoutUnit right = 0;
        LayoutUnit bottom = 0;
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
            positioningAreaSize = LayoutSize(toRenderBox(this)->width() - left - right, toRenderBox(this)->height() - top - bottom);
            left += marginLeft();
            top += marginTop();
        } else
            positioningAreaSize = LayoutSize(paintRect.width() - left - right, paintRect.height() - top - bottom);
    } else {
        geometry.setDestRect(viewRect());
        positioningAreaSize = geometry.destRect().size();
    }

    IntSize fillTileSize = calculateFillTileSize(fillLayer, positioningAreaSize);
    fillLayer->image()->setContainerSizeForRenderer(this, fillTileSize, style()->effectiveZoom());
    geometry.setTileSize(fillTileSize);

    EFillRepeat backgroundRepeatX = fillLayer->repeatX();
    EFillRepeat backgroundRepeatY = fillLayer->repeatY();

    LayoutUnit xPosition = fillLayer->xPosition().calcMinValue(positioningAreaSize.width() - geometry.tileSize().width(), true);
    if (backgroundRepeatX == RepeatFill)
        geometry.setPhaseX(geometry.tileSize().width() ? layoutMod(geometry.tileSize().width() - (xPosition + left), geometry.tileSize().width()) : LayoutUnit(0));
    else
        geometry.setNoRepeatX(xPosition + left);

    LayoutUnit yPosition = fillLayer->yPosition().calcMinValue(positioningAreaSize.height() - geometry.tileSize().height(), true);
    if (backgroundRepeatY == RepeatFill)
        geometry.setPhaseY(geometry.tileSize().height() ? layoutMod(geometry.tileSize().height() - (yPosition + top), geometry.tileSize().height()) : LayoutUnit(0));
    else 
        geometry.setNoRepeatY(yPosition + top);

    if (fixedAttachment)
        geometry.useFixedAttachment(paintRect.location());

    geometry.clip(paintRect);
    geometry.setDestOrigin(geometry.destRect().location());
}

static LayoutUnit computeBorderImageSide(Length borderSlice, LayoutUnit borderSide, LayoutUnit imageSide, LayoutUnit boxExtent)
{
    if (borderSlice.isRelative())
        return borderSlice.value() * borderSide;
    if (borderSlice.isAuto())
        return imageSide;
    return borderSlice.calcValue(boxExtent);
}

bool RenderBoxModelObject::paintNinePieceImage(GraphicsContext* graphicsContext, const LayoutRect& rect, const RenderStyle* style,
                                               const NinePieceImage& ninePieceImage, CompositeOperator op)
{
    StyleImage* styleImage = ninePieceImage.image();
    if (!styleImage)
        return false;

    if (!styleImage->isLoaded())
        return true; // Never paint a nine-piece image incrementally, but don't paint the fallback borders either.

    if (!styleImage->canRender(this, style->effectiveZoom()))
        return false;

    // FIXME: border-image is broken with full page zooming when tiling has to happen, since the tiling function
    // doesn't have any understanding of the zoom that is in effect on the tile.
    LayoutUnit topOutset;
    LayoutUnit rightOutset;
    LayoutUnit bottomOutset;
    LayoutUnit leftOutset;
    style->getImageOutsets(ninePieceImage, topOutset, rightOutset, bottomOutset, leftOutset);

    LayoutUnit topWithOutset = rect.y() - topOutset;
    LayoutUnit bottomWithOutset = rect.maxY() + bottomOutset;
    LayoutUnit leftWithOutset = rect.x() - leftOutset;
    LayoutUnit rightWithOutset = rect.maxX() + rightOutset;
    LayoutRect borderImageRect = LayoutRect(leftWithOutset, topWithOutset, rightWithOutset - leftWithOutset, bottomWithOutset - topWithOutset);

    IntSize imageSize = calculateImageIntrinsicDimensions(styleImage, borderImageRect.size());

    // If both values are ‘auto’ then the intrinsic width and/or height of the image should be used, if any.
    IntSize containerSize = imageSize.isEmpty() ? borderImageRect.size() : imageSize;
    styleImage->setContainerSizeForRenderer(this, containerSize, style->effectiveZoom());

    int imageWidth = imageSize.width();
    int imageHeight = imageSize.height();

    int topSlice = min<int>(imageHeight, ninePieceImage.imageSlices().top().calcValue(imageHeight));
    int rightSlice = min<int>(imageWidth, ninePieceImage.imageSlices().right().calcValue(imageWidth));
    int bottomSlice = min<int>(imageHeight, ninePieceImage.imageSlices().bottom().calcValue(imageHeight));
    int leftSlice = min<int>(imageWidth, ninePieceImage.imageSlices().left().calcValue(imageWidth));

    ENinePieceImageRule hRule = ninePieceImage.horizontalRule();
    ENinePieceImageRule vRule = ninePieceImage.verticalRule();
   
    LayoutUnit topWidth = computeBorderImageSide(ninePieceImage.borderSlices().top(), style->borderTopWidth(), topSlice, borderImageRect.height());
    LayoutUnit rightWidth = computeBorderImageSide(ninePieceImage.borderSlices().right(), style->borderRightWidth(), rightSlice, borderImageRect.width());
    LayoutUnit bottomWidth = computeBorderImageSide(ninePieceImage.borderSlices().bottom(), style->borderBottomWidth(), bottomSlice, borderImageRect.height());
    LayoutUnit leftWidth = computeBorderImageSide(ninePieceImage.borderSlices().left(), style->borderLeftWidth(), leftSlice, borderImageRect.width());
    
    // Reduce the widths if they're too large.
    // The spec says: Given Lwidth as the width of the border image area, Lheight as its height, and Wside as the border image width
    // offset for the side, let f = min(Lwidth/(Wleft+Wright), Lheight/(Wtop+Wbottom)). If f < 1, then all W are reduced by
    // multiplying them by f.
    LayoutUnit borderSideWidth = max<LayoutUnit>(1, leftWidth + rightWidth);
    LayoutUnit borderSideHeight = max<LayoutUnit>(1, topWidth + bottomWidth);
    float borderSideScaleFactor = min((float)borderImageRect.width() / borderSideWidth, (float)borderImageRect.height() / borderSideHeight);
    if (borderSideScaleFactor < 1) {
        topWidth *= borderSideScaleFactor;
        rightWidth *= borderSideScaleFactor;
        bottomWidth *= borderSideScaleFactor;
        leftWidth *= borderSideScaleFactor;
    }

    bool drawLeft = leftSlice > 0 && leftWidth > 0;
    bool drawTop = topSlice > 0 && topWidth > 0;
    bool drawRight = rightSlice > 0 && rightWidth > 0;
    bool drawBottom = bottomSlice > 0 && bottomWidth > 0;
    bool drawMiddle = ninePieceImage.fill() && (imageWidth - leftSlice - rightSlice) > 0 && (borderImageRect.width() - leftWidth - rightWidth) > 0
                      && (imageHeight - topSlice - bottomSlice) > 0 && (borderImageRect.height() - topWidth - bottomWidth) > 0;

    RefPtr<Image> image = styleImage->image(this, imageSize);
    ColorSpace colorSpace = style->colorSpace();
    
    float destinationWidth = borderImageRect.width() - leftWidth - rightWidth;
    float destinationHeight = borderImageRect.height() - topWidth - bottomWidth;
    
    float sourceWidth = imageWidth - leftSlice - rightSlice;
    float sourceHeight = imageHeight - topSlice - bottomSlice;
    
    float leftSideScale = drawLeft ? (float)leftWidth / leftSlice : 1;
    float rightSideScale = drawRight ? (float)rightWidth / rightSlice : 1;
    float topSideScale = drawTop ? (float)topWidth / topSlice : 1;
    float bottomSideScale = drawBottom ? (float)bottomWidth / bottomSlice : 1;
    
    if (drawLeft) {
        // Paint the top and bottom left corners.

        // The top left corner rect is (tx, ty, leftWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (0, 0, leftSlice, topSlice)
        if (drawTop)
            graphicsContext->drawImage(image.get(), colorSpace, LayoutRect(borderImageRect.location(), LayoutSize(leftWidth, topWidth)),
                                       LayoutRect(0, 0, leftSlice, topSlice), op);

        // The bottom left corner rect is (tx, ty + h - bottomWidth, leftWidth, bottomWidth)
        // The rect to use from within the image is (0, imageHeight - bottomSlice, leftSlice, botomSlice)
        if (drawBottom)
            graphicsContext->drawImage(image.get(), colorSpace, LayoutRect(borderImageRect.x(), borderImageRect.maxY() - bottomWidth, leftWidth, bottomWidth),
                                       LayoutRect(0, imageHeight - bottomSlice, leftSlice, bottomSlice), op);

        // Paint the left edge.
        // Have to scale and tile into the border rect.
        graphicsContext->drawTiledImage(image.get(), colorSpace, IntRect(borderImageRect.x(), borderImageRect.y() + topWidth, leftWidth,
                                        destinationHeight),
                                        IntRect(0, topSlice, leftSlice, sourceHeight),
                                        FloatSize(leftSideScale, leftSideScale), Image::StretchTile, (Image::TileRule)vRule, op);
    }

    if (drawRight) {
        // Paint the top and bottom right corners
        // The top right corner rect is (tx + w - rightWidth, ty, rightWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (imageWidth - rightSlice, 0, rightSlice, topSlice)
        if (drawTop)
            graphicsContext->drawImage(image.get(), colorSpace, LayoutRect(borderImageRect.maxX() - rightWidth, borderImageRect.y(), rightWidth, topWidth),
                                       LayoutRect(imageWidth - rightSlice, 0, rightSlice, topSlice), op);

        // The bottom right corner rect is (tx + w - rightWidth, ty + h - bottomWidth, rightWidth, bottomWidth)
        // The rect to use from within the image is (imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice)
        if (drawBottom)
            graphicsContext->drawImage(image.get(), colorSpace, LayoutRect(borderImageRect.maxX() - rightWidth, borderImageRect.maxY() - bottomWidth, rightWidth, bottomWidth),
                                       LayoutRect(imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice), op);

        // Paint the right edge.
        graphicsContext->drawTiledImage(image.get(), colorSpace, IntRect(borderImageRect.maxX() - rightWidth, borderImageRect.y() + topWidth, rightWidth,
                                        destinationHeight),
                                        IntRect(imageWidth - rightSlice, topSlice, rightSlice, sourceHeight),
                                        FloatSize(rightSideScale, rightSideScale),
                                        Image::StretchTile, (Image::TileRule)vRule, op);
    }

    // Paint the top edge.
    if (drawTop)
        graphicsContext->drawTiledImage(image.get(), colorSpace, IntRect(borderImageRect.x() + leftWidth, borderImageRect.y(), destinationWidth, topWidth),
                                        IntRect(leftSlice, 0, sourceWidth, topSlice),
                                        FloatSize(topSideScale, topSideScale), (Image::TileRule)hRule, Image::StretchTile, op);

    // Paint the bottom edge.
    if (drawBottom)
        graphicsContext->drawTiledImage(image.get(), colorSpace, IntRect(borderImageRect.x() + leftWidth, borderImageRect.maxY() - bottomWidth,
                                        destinationWidth, bottomWidth),
                                        IntRect(leftSlice, imageHeight - bottomSlice, sourceWidth, bottomSlice),
                                        FloatSize(bottomSideScale, bottomSideScale),
                                        (Image::TileRule)hRule, Image::StretchTile, op);

    // Paint the middle.
    if (drawMiddle) {
        FloatSize middleScaleFactor(1, 1);
        if (drawTop)
            middleScaleFactor.setWidth(topSideScale);
        else if (drawBottom)
            middleScaleFactor.setWidth(bottomSideScale);
        if (drawLeft)
            middleScaleFactor.setHeight(leftSideScale);
        else if (drawRight)
            middleScaleFactor.setHeight(rightSideScale);
            
        // For "stretch" rules, just override the scale factor and replace. We only had to do this for the
        // center tile, since sides don't even use the scale factor unless they have a rule other than "stretch".
        // The middle however can have "stretch" specified in one axis but not the other, so we have to
        // correct the scale here.
        if (hRule == StretchImageRule)
            middleScaleFactor.setWidth(destinationWidth / sourceWidth);
            
        if (vRule == StretchImageRule)
            middleScaleFactor.setHeight(destinationHeight / sourceHeight);
        
        graphicsContext->drawTiledImage(image.get(), colorSpace,
            IntRect(borderImageRect.x() + leftWidth, borderImageRect.y() + topWidth, destinationWidth, destinationHeight),
            IntRect(leftSlice, topSlice, sourceWidth, sourceHeight),
            middleScaleFactor, (Image::TileRule)hRule, (Image::TileRule)vRule, op);
    }

    return true;
}

class BorderEdge {
public:
    BorderEdge(int edgeWidth, const Color& edgeColor, EBorderStyle edgeStyle, bool edgeIsTransparent, bool edgeIsPresent = true)
        : width(edgeWidth)
        , color(edgeColor)
        , style(edgeStyle)
        , isTransparent(edgeIsTransparent)
        , isPresent(edgeIsPresent)
    {
        if (style == DOUBLE && edgeWidth < 3)
            style = SOLID;
    }
    
    BorderEdge()
        : width(0)
        , style(BHIDDEN)
        , isTransparent(false)
        , isPresent(false)
    {
    }
    
    bool hasVisibleColorAndStyle() const { return style > BHIDDEN && !isTransparent; }
    bool shouldRender() const { return isPresent && hasVisibleColorAndStyle(); }
    bool presentButInvisible() const { return usedWidth() && !hasVisibleColorAndStyle(); }
    bool obscuresBackgroundEdge(float scale) const
    {
        if (!isPresent || isTransparent || width < (2 * scale) || color.hasAlpha() || style == BHIDDEN)
            return false;

        if (style == DOTTED || style == DASHED)
            return false;

        if (style == DOUBLE)
            return width >= 5 * scale; // The outer band needs to be >= 2px wide at unit scale.

        return true;
    }
    bool obscuresBackground() const
    {
        if (!isPresent || isTransparent || color.hasAlpha() || style == BHIDDEN)
            return false;

        if (style == DOTTED || style == DASHED || style == DOUBLE)
            return false;

        return true;
    }

    int usedWidth() const { return isPresent ? width : 0; }
    
    void getDoubleBorderStripeWidths(int& outerWidth, int& innerWidth) const
    {
        int fullWidth = usedWidth();
        outerWidth = fullWidth / 3;
        innerWidth = fullWidth * 2 / 3;

        // We need certain integer rounding results
        if (fullWidth % 3 == 2)
            outerWidth += 1;

        if (fullWidth % 3 == 1)
            innerWidth += 1;
    }
    
    int width;
    Color color;
    EBorderStyle style;
    bool isTransparent;
    bool isPresent;
};

static bool allCornersClippedOut(const RoundedRect& border, const LayoutRect& clipRect)
{
    LayoutRect boundingRect = border.rect();
    if (clipRect.contains(boundingRect))
        return false;

    RoundedRect::Radii radii = border.radii();

    LayoutRect topLeftRect(boundingRect.location(), radii.topLeft());
    if (clipRect.intersects(topLeftRect))
        return false;

    LayoutRect topRightRect(boundingRect.location(), radii.topRight());
    topRightRect.setX(boundingRect.maxX() - topRightRect.width());
    if (clipRect.intersects(topRightRect))
        return false;

    LayoutRect bottomLeftRect(boundingRect.location(), radii.bottomLeft());
    bottomLeftRect.setY(boundingRect.maxY() - bottomLeftRect.height());
    if (clipRect.intersects(bottomLeftRect))
        return false;

    LayoutRect bottomRightRect(boundingRect.location(), radii.bottomRight());
    bottomRightRect.setX(boundingRect.maxX() - bottomRightRect.width());
    bottomRightRect.setY(boundingRect.maxY() - bottomRightRect.height());
    if (clipRect.intersects(bottomRightRect))
        return false;

    return true;
}

#if HAVE(PATH_BASED_BORDER_RADIUS_DRAWING)
static bool borderWillArcInnerEdge(const LayoutSize& firstRadius, const FloatSize& secondRadius)
{
    return !firstRadius.isZero() || !secondRadius.isZero();
}

enum BorderEdgeFlag {
    TopBorderEdge = 1 << BSTop,
    RightBorderEdge = 1 << BSRight,
    BottomBorderEdge = 1 << BSBottom,
    LeftBorderEdge = 1 << BSLeft,
    AllBorderEdges = TopBorderEdge | BottomBorderEdge | LeftBorderEdge | RightBorderEdge
};

static inline BorderEdgeFlag edgeFlagForSide(BoxSide side)
{
    return static_cast<BorderEdgeFlag>(1 << side);
}

static inline bool includesEdge(BorderEdgeFlags flags, BoxSide side)
{
    return flags & edgeFlagForSide(side);
}

inline bool edgesShareColor(const BorderEdge& firstEdge, const BorderEdge& secondEdge)
{
    return firstEdge.color == secondEdge.color;
}

inline bool styleRequiresClipPolygon(EBorderStyle style)
{
    return style == DOTTED || style == DASHED; // These are drawn with a stroke, so we have to clip to get corner miters.
}

static bool borderStyleFillsBorderArea(EBorderStyle style)
{
    return !(style == DOTTED || style == DASHED || style == DOUBLE);
}

static bool borderStyleHasInnerDetail(EBorderStyle style)
{
    return style == GROOVE || style == RIDGE || style == DOUBLE;
}

static bool borderStyleIsDottedOrDashed(EBorderStyle style)
{
    return style == DOTTED || style == DASHED;
}

// OUTSET darkens the bottom and right (and maybe lightens the top and left)
// INSET darkens the top and left (and maybe lightens the bottom and right)
static inline bool borderStyleHasUnmatchedColorsAtCorner(EBorderStyle style, BoxSide side, BoxSide adjacentSide)
{
    // These styles match at the top/left and bottom/right.
    if (style == INSET || style == GROOVE || style == RIDGE || style == OUTSET) {
        const BorderEdgeFlags topRightFlags = edgeFlagForSide(BSTop) | edgeFlagForSide(BSRight);
        const BorderEdgeFlags bottomLeftFlags = edgeFlagForSide(BSBottom) | edgeFlagForSide(BSLeft);

        BorderEdgeFlags flags = edgeFlagForSide(side) | edgeFlagForSide(adjacentSide);
        return flags == topRightFlags || flags == bottomLeftFlags;
    }
    return false;
}

static inline bool colorsMatchAtCorner(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[])
{
    if (edges[side].shouldRender() != edges[adjacentSide].shouldRender())
        return false;

    if (!edgesShareColor(edges[side], edges[adjacentSide]))
        return false;

    return !borderStyleHasUnmatchedColorsAtCorner(edges[side].style, side, adjacentSide);
}


static inline bool colorNeedsAntiAliasAtCorner(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[])
{
    if (!edges[side].color.hasAlpha())
        return false;

    if (edges[side].shouldRender() != edges[adjacentSide].shouldRender())
        return false;

    if (!edgesShareColor(edges[side], edges[adjacentSide]))
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(edges[side].style, side, adjacentSide);
}

// This assumes that we draw in order: top, bottom, left, right.
static inline bool willBeOverdrawn(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[])
{
    switch (side) {
    case BSTop:
    case BSBottom:
        if (edges[adjacentSide].presentButInvisible())
            return false;

        if (!edgesShareColor(edges[side], edges[adjacentSide]) && edges[adjacentSide].color.hasAlpha())
            return false;
        
        if (!borderStyleFillsBorderArea(edges[adjacentSide].style))
            return false;

        return true;

    case BSLeft:
    case BSRight:
        // These draw last, so are never overdrawn.
        return false;
    }
    return false;
}

static inline bool borderStylesRequireMitre(BoxSide side, BoxSide adjacentSide, EBorderStyle style, EBorderStyle adjacentStyle)
{
    if (style == DOUBLE || adjacentStyle == DOUBLE || adjacentStyle == GROOVE || adjacentStyle == RIDGE)
        return true;

    if (borderStyleIsDottedOrDashed(style) != borderStyleIsDottedOrDashed(adjacentStyle))
        return true;

    if (style != adjacentStyle)
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(style, side, adjacentSide);
}

static bool joinRequiresMitre(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[], bool allowOverdraw)
{
    if ((edges[side].isTransparent && edges[adjacentSide].isTransparent) || !edges[adjacentSide].isPresent)
        return false;

    if (allowOverdraw && willBeOverdrawn(side, adjacentSide, edges))
        return false;

    if (!edgesShareColor(edges[side], edges[adjacentSide]))
        return true;

    if (borderStylesRequireMitre(side, adjacentSide, edges[side].style, edges[adjacentSide].style))
        return true;
    
    return false;
}

void RenderBoxModelObject::paintOneBorderSide(GraphicsContext* graphicsContext, const RenderStyle* style, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
    const LayoutRect& sideRect, BoxSide side, BoxSide adjacentSide1, BoxSide adjacentSide2, const BorderEdge edges[], const Path* path,
    BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor)
{
    const BorderEdge& edgeToRender = edges[side];
    const BorderEdge& adjacentEdge1 = edges[adjacentSide1];
    const BorderEdge& adjacentEdge2 = edges[adjacentSide2];

    bool mitreAdjacentSide1 = joinRequiresMitre(side, adjacentSide1, edges, !antialias);
    bool mitreAdjacentSide2 = joinRequiresMitre(side, adjacentSide2, edges, !antialias);
    
    bool adjacentSide1StylesMatch = colorsMatchAtCorner(side, adjacentSide1, edges);
    bool adjacentSide2StylesMatch = colorsMatchAtCorner(side, adjacentSide2, edges);

    const Color& colorToPaint = overrideColor ? *overrideColor : edgeToRender.color;

    if (path) {
        GraphicsContextStateSaver stateSaver(*graphicsContext);
        if (innerBorder.isRenderable())
            clipBorderSidePolygon(graphicsContext, outerBorder, innerBorder, side, adjacentSide1StylesMatch, adjacentSide2StylesMatch);
        else
            clipBorderSideForComplexInnerPath(graphicsContext, outerBorder, innerBorder, side, edges);
        float thickness = max(max(edgeToRender.width, adjacentEdge1.width), adjacentEdge2.width);
        drawBoxSideFromPath(graphicsContext, outerBorder.rect(), *path, edges, edgeToRender.width, thickness, side, style,
            colorToPaint, edgeToRender.style, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
    } else {
        bool clipForStyle = styleRequiresClipPolygon(edgeToRender.style) && (mitreAdjacentSide1 || mitreAdjacentSide2);
        bool clipAdjacentSide1 = colorNeedsAntiAliasAtCorner(side, adjacentSide1, edges) && mitreAdjacentSide1;
        bool clipAdjacentSide2 = colorNeedsAntiAliasAtCorner(side, adjacentSide2, edges) && mitreAdjacentSide2;
        bool shouldClip = clipForStyle || clipAdjacentSide1 || clipAdjacentSide2;
        
        GraphicsContextStateSaver clipStateSaver(*graphicsContext, shouldClip);
        if (shouldClip) {
            bool aliasAdjacentSide1 = clipAdjacentSide1 || (clipForStyle && mitreAdjacentSide1);
            bool aliasAdjacentSide2 = clipAdjacentSide2 || (clipForStyle && mitreAdjacentSide2);
            clipBorderSidePolygon(graphicsContext, outerBorder, innerBorder, side, !aliasAdjacentSide1, !aliasAdjacentSide2);
            // Since we clipped, no need to draw with a mitre.
            mitreAdjacentSide1 = false;
            mitreAdjacentSide2 = false;
        }
        
        drawLineForBoxSide(graphicsContext, sideRect.x(), sideRect.y(), sideRect.maxX(), sideRect.maxY(), side, colorToPaint, edgeToRender.style,
                mitreAdjacentSide1 ? adjacentEdge1.width : 0, mitreAdjacentSide2 ? adjacentEdge2.width : 0, antialias);
    }
}

static LayoutRect calculateSideRect(const RoundedRect& outerBorder, const BorderEdge edges[], int side)
{
    LayoutRect sideRect = outerBorder.rect();
    int width = edges[side].width;

    if (side == BSTop)
        sideRect.setHeight(width);
    else if (side == BSBottom)
        sideRect.shiftYEdgeTo(sideRect.maxY() - width);
    else if (side == BSLeft)
        sideRect.setWidth(width);
    else
        sideRect.shiftXEdgeTo(sideRect.maxX() - width);

    return sideRect;
}

void RenderBoxModelObject::paintBorderSides(GraphicsContext* graphicsContext, const RenderStyle* style, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
                                            const BorderEdge edges[], BorderEdgeFlags edgeSet, BackgroundBleedAvoidance bleedAvoidance,
                                            bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor)
{
    bool renderRadii = outerBorder.isRounded();

    Path roundedPath;
    if (renderRadii)
        roundedPath.addRoundedRect(outerBorder);
    
    if (edges[BSTop].shouldRender() && includesEdge(edgeSet, BSTop)) {
        LayoutRect sideRect = outerBorder.rect();
        sideRect.setHeight(edges[BSTop].width);

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSTop].style) || borderWillArcInnerEdge(innerBorder.radii().topLeft(), innerBorder.radii().topRight()));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, BSTop, BSLeft, BSRight, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }

    if (edges[BSBottom].shouldRender() && includesEdge(edgeSet, BSBottom)) {
        LayoutRect sideRect = outerBorder.rect();
        sideRect.shiftYEdgeTo(sideRect.maxY() - edges[BSBottom].width);

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSBottom].style) || borderWillArcInnerEdge(innerBorder.radii().bottomLeft(), innerBorder.radii().bottomRight()));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, BSBottom, BSLeft, BSRight, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }

    if (edges[BSLeft].shouldRender() && includesEdge(edgeSet, BSLeft)) {
        LayoutRect sideRect = outerBorder.rect();
        sideRect.setWidth(edges[BSLeft].width);

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSLeft].style) || borderWillArcInnerEdge(innerBorder.radii().bottomLeft(), innerBorder.radii().topLeft()));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, BSLeft, BSTop, BSBottom, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }

    if (edges[BSRight].shouldRender() && includesEdge(edgeSet, BSRight)) {
        LayoutRect sideRect = outerBorder.rect();
        sideRect.shiftXEdgeTo(sideRect.maxX() - edges[BSRight].width);

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSRight].style) || borderWillArcInnerEdge(innerBorder.radii().bottomRight(), innerBorder.radii().topRight()));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, BSRight, BSTop, BSBottom, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }
}

void RenderBoxModelObject::paintTranslucentBorderSides(GraphicsContext* graphicsContext, const RenderStyle* style, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
                                                       const BorderEdge edges[], BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias)
{
    BorderEdgeFlags edgesToDraw = AllBorderEdges;
    while (edgesToDraw) {
        // Find undrawn edges sharing a color.
        Color commonColor;
        
        BorderEdgeFlags commonColorEdgeSet = 0;
        for (int i = BSTop; i <= BSLeft; ++i) {
            BoxSide currSide = static_cast<BoxSide>(i);
            if (!includesEdge(edgesToDraw, currSide))
                continue;

            bool includeEdge;
            if (!commonColorEdgeSet) {
                commonColor = edges[currSide].color;
                includeEdge = true;
            } else
                includeEdge = edges[currSide].color == commonColor;

            if (includeEdge)
                commonColorEdgeSet |= edgeFlagForSide(currSide);
        }

        bool useTransparencyLayer = commonColor.hasAlpha();
        if (useTransparencyLayer) {
            graphicsContext->beginTransparencyLayer(static_cast<float>(commonColor.alpha()) / 255);
            commonColor = Color(commonColor.red(), commonColor.green(), commonColor.blue());
        }

        paintBorderSides(graphicsContext, style, outerBorder, innerBorder, edges, commonColorEdgeSet, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, &commonColor);
            
        if (useTransparencyLayer)
            graphicsContext->endTransparencyLayer();
        
        edgesToDraw &= ~commonColorEdgeSet;
    }
}

void RenderBoxModelObject::paintBorder(const PaintInfo& info, const LayoutRect& rect, const RenderStyle* style,
                                       BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    GraphicsContext* graphicsContext = info.context;
    // border-image is not affected by border-radius.
    if (paintNinePieceImage(graphicsContext, rect, style, style->borderImage()))
        return;

    if (graphicsContext->paintingDisabled())
        return;

    BorderEdge edges[4];
    getBorderEdgeInfo(edges, includeLogicalLeftEdge, includeLogicalRightEdge);

    RoundedRect outerBorder = style->getRoundedBorderFor(rect, includeLogicalLeftEdge, includeLogicalRightEdge);
    RoundedRect innerBorder = style->getRoundedInnerBorderFor(rect, includeLogicalLeftEdge, includeLogicalRightEdge);

    bool haveAlphaColor = false;
    bool haveAllSolidEdges = true;
    bool allEdgesVisible = true;
    bool allEdgesShareColor = true;
    int firstVisibleEdge = -1;

    for (int i = BSTop; i <= BSLeft; ++i) {
        const BorderEdge& currEdge = edges[i];
        if (currEdge.presentButInvisible()) {
            allEdgesVisible = false;
            allEdgesShareColor = false;
            continue;
        }
        
        if (!currEdge.width) {
            allEdgesVisible = false;
            continue;
        }

        if (firstVisibleEdge == -1)
            firstVisibleEdge = i;
        else if (currEdge.color != edges[firstVisibleEdge].color)
            allEdgesShareColor = false;

        if (currEdge.color.hasAlpha())
            haveAlphaColor = true;
        
        if (currEdge.style != SOLID)
            haveAllSolidEdges = false;
    }

    // If no corner intersects the clip region, we can pretend outerBorder is
    // rectangular to improve performance.
    if (haveAllSolidEdges && outerBorder.isRounded() && allCornersClippedOut(outerBorder, info.rect))
        outerBorder.setRadii(RoundedRect::Radii());

    // isRenderable() check avoids issue described in https://bugs.webkit.org/show_bug.cgi?id=38787
    if (haveAllSolidEdges && allEdgesShareColor && innerBorder.isRenderable()) {
        // Fast path for drawing all solid edges.
        if (allEdgesVisible && (outerBorder.isRounded() || haveAlphaColor)) {
            Path path;
            
            if (outerBorder.isRounded() && bleedAvoidance != BackgroundBleedUseTransparencyLayer)
                path.addRoundedRect(outerBorder);
            else
                path.addRect(outerBorder.rect());

            if (innerBorder.isRounded())
                path.addRoundedRect(innerBorder);
            else
                path.addRect(innerBorder.rect());
            
            graphicsContext->setFillRule(RULE_EVENODD);
            graphicsContext->setFillColor(edges[firstVisibleEdge].color, style->colorSpace());
            graphicsContext->fillPath(path);
            return;
        } 
        // Avoid creating transparent layers
        if (!allEdgesVisible && !outerBorder.isRounded() && haveAlphaColor) {
            Path path;

            for (int i = BSTop; i <= BSLeft; ++i) {
                const BorderEdge& currEdge = edges[i];
                if (currEdge.shouldRender()) {
                    LayoutRect sideRect = calculateSideRect(outerBorder, edges, i);
                    path.addRect(sideRect);
                }
            }

            graphicsContext->setFillRule(RULE_NONZERO);
            graphicsContext->setFillColor(edges[firstVisibleEdge].color, style->colorSpace());
            graphicsContext->fillPath(path);
            return;
        }
    }

    bool clipToOuterBorder = outerBorder.isRounded();
    GraphicsContextStateSaver stateSaver(*graphicsContext, clipToOuterBorder);
    if (clipToOuterBorder) {
        // Clip to the inner and outer radii rects.
        if (bleedAvoidance != BackgroundBleedUseTransparencyLayer)
            graphicsContext->addRoundedRectClip(outerBorder);
        // isRenderable() check avoids issue described in https://bugs.webkit.org/show_bug.cgi?id=38787
        // The inside will be clipped out later (in clipBorderSideForComplexInnerPath)
        if (innerBorder.isRenderable())
            graphicsContext->clipOutRoundedRect(innerBorder);
    }

    bool antialias = shouldAntialiasLines(graphicsContext);    
    if (haveAlphaColor)
        paintTranslucentBorderSides(graphicsContext, style, outerBorder, innerBorder, edges, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias);
    else
        paintBorderSides(graphicsContext, style, outerBorder, innerBorder, edges, AllBorderEdges, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias);
}

void RenderBoxModelObject::drawBoxSideFromPath(GraphicsContext* graphicsContext, const LayoutRect& borderRect, const Path& borderPath, const BorderEdge edges[],
                                    float thickness, float drawThickness, BoxSide side, const RenderStyle* style, 
                                    Color color, EBorderStyle borderStyle, BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    if (thickness <= 0)
        return;

    if (borderStyle == DOUBLE && thickness < 3)
        borderStyle = SOLID;

    switch (borderStyle) {
    case BNONE:
    case BHIDDEN:
        return;
    case DOTTED:
    case DASHED: {
        graphicsContext->setStrokeColor(color, style->colorSpace());

        // The stroke is doubled here because the provided path is the 
        // outside edge of the border so half the stroke is clipped off. 
        // The extra multiplier is so that the clipping mask can antialias
        // the edges to prevent jaggies.
        graphicsContext->setStrokeThickness(drawThickness * 2 * 1.1f);
        graphicsContext->setStrokeStyle(borderStyle == DASHED ? DashedStroke : DottedStroke);

        // If the number of dashes that fit in the path is odd and non-integral then we
        // will have an awkwardly-sized dash at the end of the path. To try to avoid that
        // here, we simply make the whitespace dashes ever so slightly bigger.
        // FIXME: This could be even better if we tried to manipulate the dash offset
        // and possibly the gapLength to get the corners dash-symmetrical.
        float dashLength = thickness * ((borderStyle == DASHED) ? 3.0f : 1.0f);
        float gapLength = dashLength;
        float numberOfDashes = borderPath.length() / dashLength;
        // Don't try to show dashes if we have less than 2 dashes + 2 gaps.
        // FIXME: should do this test per side.
        if (numberOfDashes >= 4) {
            bool evenNumberOfFullDashes = !((int)numberOfDashes % 2);
            bool integralNumberOfDashes = !(numberOfDashes - (int)numberOfDashes);
            if (!evenNumberOfFullDashes && !integralNumberOfDashes) {
                float numberOfGaps = numberOfDashes / 2;
                gapLength += (dashLength  / numberOfGaps);
            }

            DashArray lineDash;
            lineDash.append(dashLength);
            lineDash.append(gapLength);
            graphicsContext->setLineDash(lineDash, dashLength);
        }
        
        // FIXME: stroking the border path causes issues with tight corners:
        // https://bugs.webkit.org/show_bug.cgi?id=58711
        // Also, to get the best appearance we should stroke a path between the two borders.
        graphicsContext->strokePath(borderPath);
        return;
    }
    case DOUBLE: {
        // Get the inner border rects for both the outer border line and the inner border line
        int outerBorderTopWidth;
        int innerBorderTopWidth;
        edges[BSTop].getDoubleBorderStripeWidths(outerBorderTopWidth, innerBorderTopWidth);

        int outerBorderRightWidth;
        int innerBorderRightWidth;
        edges[BSRight].getDoubleBorderStripeWidths(outerBorderRightWidth, innerBorderRightWidth);

        int outerBorderBottomWidth;
        int innerBorderBottomWidth;
        edges[BSBottom].getDoubleBorderStripeWidths(outerBorderBottomWidth, innerBorderBottomWidth);

        int outerBorderLeftWidth;
        int innerBorderLeftWidth;
        edges[BSLeft].getDoubleBorderStripeWidths(outerBorderLeftWidth, innerBorderLeftWidth);

        // Draw inner border line
        {
            GraphicsContextStateSaver stateSaver(*graphicsContext);
            RoundedRect innerClip = style->getRoundedInnerBorderFor(borderRect,
                innerBorderTopWidth, innerBorderBottomWidth, innerBorderLeftWidth, innerBorderRightWidth,
                includeLogicalLeftEdge, includeLogicalRightEdge);
            
            graphicsContext->addRoundedRectClip(innerClip);
            drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, SOLID, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        }

        // Draw outer border line
        {
            GraphicsContextStateSaver stateSaver(*graphicsContext);
            LayoutRect outerRect = borderRect;
            if (bleedAvoidance == BackgroundBleedUseTransparencyLayer) {
                outerRect.inflate(1);
                ++outerBorderTopWidth;
                ++outerBorderBottomWidth;
                ++outerBorderLeftWidth;
                ++outerBorderRightWidth;
            }
                
            RoundedRect outerClip = style->getRoundedInnerBorderFor(outerRect,
                outerBorderTopWidth, outerBorderBottomWidth, outerBorderLeftWidth, outerBorderRightWidth,
                includeLogicalLeftEdge, includeLogicalRightEdge);
            graphicsContext->clipOutRoundedRect(outerClip);
            drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, SOLID, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        }
        return;
    }
    case RIDGE:
    case GROOVE:
    {
        EBorderStyle s1;
        EBorderStyle s2;
        if (borderStyle == GROOVE) {
            s1 = INSET;
            s2 = OUTSET;
        } else {
            s1 = OUTSET;
            s2 = INSET;
        }
        
        // Paint full border
        drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, s1, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);

        // Paint inner only
        GraphicsContextStateSaver stateSaver(*graphicsContext);
        LayoutUnit topWidth = edges[BSTop].usedWidth() / 2;
        LayoutUnit bottomWidth = edges[BSBottom].usedWidth() / 2;
        LayoutUnit leftWidth = edges[BSLeft].usedWidth() / 2;
        LayoutUnit rightWidth = edges[BSRight].usedWidth() / 2;

        RoundedRect clipRect = style->getRoundedInnerBorderFor(borderRect,
            topWidth, bottomWidth, leftWidth, rightWidth,
            includeLogicalLeftEdge, includeLogicalRightEdge);

        graphicsContext->addRoundedRectClip(clipRect);
        drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, s2, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        return;
    }
    case INSET:
        if (side == BSTop || side == BSLeft)
            color = color.dark();
        break;
    case OUTSET:
        if (side == BSBottom || side == BSRight)
            color = color.dark();
        break;
    default:
        break;
    }

    graphicsContext->setStrokeStyle(NoStroke);
    graphicsContext->setFillColor(color, style->colorSpace());
    graphicsContext->drawRect(borderRect);
}
#else
void RenderBoxModelObject::paintBorder(const PaintInfo& info, const IntRect& rect, const RenderStyle* style,
                                       BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    GraphicsContext* graphicsContext = info.context;
    // FIXME: This old version of paintBorder should be removed when all ports implement 
    // GraphicsContext::clipConvexPolygon()!! This should happen soon.
    if (paintNinePieceImage(graphicsContext, rect, style, style->borderImage()))
        return;

    const Color& topColor = style->visitedDependentColor(CSSPropertyBorderTopColor);
    const Color& bottomColor = style->visitedDependentColor(CSSPropertyBorderBottomColor);
    const Color& leftColor = style->visitedDependentColor(CSSPropertyBorderLeftColor);
    const Color& rightColor = style->visitedDependentColor(CSSPropertyBorderRightColor);

    bool topTransparent = style->borderTopIsTransparent();
    bool bottomTransparent = style->borderBottomIsTransparent();
    bool rightTransparent = style->borderRightIsTransparent();
    bool leftTransparent = style->borderLeftIsTransparent();

    EBorderStyle topStyle = style->borderTopStyle();
    EBorderStyle bottomStyle = style->borderBottomStyle();
    EBorderStyle leftStyle = style->borderLeftStyle();
    EBorderStyle rightStyle = style->borderRightStyle();

    bool horizontal = style->isHorizontalWritingMode();
    bool renderTop = topStyle > BHIDDEN && !topTransparent && (horizontal || includeLogicalLeftEdge);
    bool renderLeft = leftStyle > BHIDDEN && !leftTransparent && (!horizontal || includeLogicalLeftEdge);
    bool renderRight = rightStyle > BHIDDEN && !rightTransparent && (!horizontal || includeLogicalRightEdge);
    bool renderBottom = bottomStyle > BHIDDEN && !bottomTransparent && (horizontal || includeLogicalRightEdge);


    RoundedRect border(rect);
    
    GraphicsContextStateSaver stateSaver(*graphicsContext, false);
    if (style->hasBorderRadius()) {
        border.includeLogicalEdges(style->getRoundedBorderFor(border.rect()).radii(),
                                   horizontal, includeLogicalLeftEdge, includeLogicalRightEdge);
        if (border.isRounded()) {
            stateSaver.save();
            graphicsContext->addRoundedRectClip(border);
        }
    }

    int firstAngleStart, secondAngleStart, firstAngleSpan, secondAngleSpan;
    float thickness;
    bool renderRadii = border.isRounded();
    bool upperLeftBorderStylesMatch = renderLeft && (topStyle == leftStyle) && (topColor == leftColor);
    bool upperRightBorderStylesMatch = renderRight && (topStyle == rightStyle) && (topColor == rightColor) && (topStyle != OUTSET) && (topStyle != RIDGE) && (topStyle != INSET) && (topStyle != GROOVE);
    bool lowerLeftBorderStylesMatch = renderLeft && (bottomStyle == leftStyle) && (bottomColor == leftColor) && (bottomStyle != OUTSET) && (bottomStyle != RIDGE) && (bottomStyle != INSET) && (bottomStyle != GROOVE);
    bool lowerRightBorderStylesMatch = renderRight && (bottomStyle == rightStyle) && (bottomColor == rightColor);

    if (renderTop) {
        bool ignoreLeft = (renderRadii && border.radii().topLeft().width() > 0)
            || (topColor == leftColor && topTransparent == leftTransparent && topStyle >= OUTSET
                && (leftStyle == DOTTED || leftStyle == DASHED || leftStyle == SOLID || leftStyle == OUTSET));
        
        bool ignoreRight = (renderRadii && border.radii().topRight().width() > 0)
            || (topColor == rightColor && topTransparent == rightTransparent && topStyle >= OUTSET
                && (rightStyle == DOTTED || rightStyle == DASHED || rightStyle == SOLID || rightStyle == INSET));

        int x = rect.x();
        int x2 = rect.maxX();
        if (renderRadii) {
            x += border.radii().topLeft().width();
            x2 -= border.radii().topRight().width();
        }

        drawLineForBoxSide(graphicsContext, x, rect.y(), x2, rect.y() + style->borderTopWidth(), BSTop, topColor, topStyle,
                   ignoreLeft ? 0 : style->borderLeftWidth(), ignoreRight ? 0 : style->borderRightWidth());

        if (renderRadii) {
            int leftY = rect.y();

            // We make the arc double thick and let the clip rect take care of clipping the extra off.
            // We're doing this because it doesn't seem possible to match the curve of the clip exactly
            // with the arc-drawing function.
            thickness = style->borderTopWidth() * 2;

            if (border.radii().topLeft().width()) {
                int leftX = rect.x();
                // The inner clip clips inside the arc. This is especially important for 1px borders.
                bool applyLeftInnerClip = (style->borderLeftWidth() < border.radii().topLeft().width())
                    && (style->borderTopWidth() < border.radii().topLeft().height())
                    && (topStyle != DOUBLE || style->borderTopWidth() > 6);
                
                GraphicsContextStateSaver stateSaver(*graphicsContext, applyLeftInnerClip);
                if (applyLeftInnerClip)
                    graphicsContext->addInnerRoundedRectClip(IntRect(leftX, leftY, border.radii().topLeft().width() * 2, border.radii().topLeft().height() * 2),
                                                             style->borderTopWidth());

                firstAngleStart = 90;
                firstAngleSpan = upperLeftBorderStylesMatch ? 90 : 45;

                // Draw upper left arc
                drawArcForBoxSide(graphicsContext, leftX, leftY, thickness, border.radii().topLeft(), firstAngleStart, firstAngleSpan,
                              BSTop, topColor, topStyle, true);
            }

            if (border.radii().topRight().width()) {
                int rightX = rect.maxX() - border.radii().topRight().width() * 2;
                bool applyRightInnerClip = (style->borderRightWidth() < border.radii().topRight().width())
                    && (style->borderTopWidth() < border.radii().topRight().height())
                    && (topStyle != DOUBLE || style->borderTopWidth() > 6);

                GraphicsContextStateSaver stateSaver(*graphicsContext, applyRightInnerClip);
                if (applyRightInnerClip)
                    graphicsContext->addInnerRoundedRectClip(IntRect(rightX, leftY, border.radii().topRight().width() * 2, border.radii().topRight().height() * 2),
                                                             style->borderTopWidth());

                if (upperRightBorderStylesMatch) {
                    secondAngleStart = 0;
                    secondAngleSpan = 90;
                } else {
                    secondAngleStart = 45;
                    secondAngleSpan = 45;
                }

                // Draw upper right arc
                drawArcForBoxSide(graphicsContext, rightX, leftY, thickness, border.radii().topRight(), secondAngleStart, secondAngleSpan,
                              BSTop, topColor, topStyle, false);
            }
        }
    }

    if (renderBottom) {
        bool ignoreLeft = (renderRadii && border.radii().bottomLeft().width() > 0)
            || (bottomColor == leftColor && bottomTransparent == leftTransparent && bottomStyle >= OUTSET
                && (leftStyle == DOTTED || leftStyle == DASHED || leftStyle == SOLID || leftStyle == OUTSET));

        bool ignoreRight = (renderRadii && border.radii().bottomRight().width() > 0)
            || (bottomColor == rightColor && bottomTransparent == rightTransparent && bottomStyle >= OUTSET
                && (rightStyle == DOTTED || rightStyle == DASHED || rightStyle == SOLID || rightStyle == INSET));

        int x = rect.x();
        int x2 = rect.maxX();
        if (renderRadii) {
            x += border.radii().bottomLeft().width();
            x2 -= border.radii().bottomRight().width();
        }

        drawLineForBoxSide(graphicsContext, x, rect.maxY() - style->borderBottomWidth(), x2, rect.maxY(), BSBottom, bottomColor, bottomStyle,
                   ignoreLeft ? 0 : style->borderLeftWidth(), ignoreRight ? 0 : style->borderRightWidth());

        if (renderRadii) {
            thickness = style->borderBottomWidth() * 2;

            if (border.radii().bottomLeft().width()) {
                int leftX = rect.x();
                int leftY = rect.maxY() - border.radii().bottomLeft().height() * 2;
                bool applyLeftInnerClip = (style->borderLeftWidth() < border.radii().bottomLeft().width())
                    && (style->borderBottomWidth() < border.radii().bottomLeft().height())
                    && (bottomStyle != DOUBLE || style->borderBottomWidth() > 6);

                GraphicsContextStateSaver stateSaver(*graphicsContext, applyLeftInnerClip);
                if (applyLeftInnerClip)
                    graphicsContext->addInnerRoundedRectClip(IntRect(leftX, leftY, border.radii().bottomLeft().width() * 2, border.radii().bottomLeft().height() * 2),
                                                             style->borderBottomWidth());

                if (lowerLeftBorderStylesMatch) {
                    firstAngleStart = 180;
                    firstAngleSpan = 90;
                } else {
                    firstAngleStart = 225;
                    firstAngleSpan = 45;
                }

                // Draw lower left arc
                drawArcForBoxSide(graphicsContext, leftX, leftY, thickness, border.radii().bottomLeft(), firstAngleStart, firstAngleSpan,
                              BSBottom, bottomColor, bottomStyle, true);
            }

            if (border.radii().bottomRight().width()) {
                int rightY = rect.maxY() - border.radii().bottomRight().height() * 2;
                int rightX = rect.maxX() - border.radii().bottomRight().width() * 2;
                bool applyRightInnerClip = (style->borderRightWidth() < border.radii().bottomRight().width())
                    && (style->borderBottomWidth() < border.radii().bottomRight().height())
                    && (bottomStyle != DOUBLE || style->borderBottomWidth() > 6);

                GraphicsContextStateSaver stateSaver(*graphicsContext, applyRightInnerClip);
                if (applyRightInnerClip)
                    graphicsContext->addInnerRoundedRectClip(IntRect(rightX, rightY, border.radii().bottomRight().width() * 2, border.radii().bottomRight().height() * 2),
                                                             style->borderBottomWidth());

                secondAngleStart = 270;
                secondAngleSpan = lowerRightBorderStylesMatch ? 90 : 45;

                // Draw lower right arc
                drawArcForBoxSide(graphicsContext, rightX, rightY, thickness, border.radii().bottomRight(), secondAngleStart, secondAngleSpan,
                              BSBottom, bottomColor, bottomStyle, false);
            }
        }
    }

    if (renderLeft) {
        bool ignoreTop = (renderRadii && border.radii().topLeft().height() > 0)
            || (topColor == leftColor && topTransparent == leftTransparent && leftStyle >= OUTSET
                && (topStyle == DOTTED || topStyle == DASHED || topStyle == SOLID || topStyle == OUTSET));

        bool ignoreBottom = (renderRadii && border.radii().bottomLeft().height() > 0)
            || (bottomColor == leftColor && bottomTransparent == leftTransparent && leftStyle >= OUTSET
                && (bottomStyle == DOTTED || bottomStyle == DASHED || bottomStyle == SOLID || bottomStyle == INSET));

        int y = rect.y();
        int y2 = rect.maxY();
        if (renderRadii) {
            y += border.radii().topLeft().height();
            y2 -= border.radii().bottomLeft().height();
        }

        drawLineForBoxSide(graphicsContext, rect.x(), y, rect.x() + style->borderLeftWidth(), y2, BSLeft, leftColor, leftStyle,
                   ignoreTop ? 0 : style->borderTopWidth(), ignoreBottom ? 0 : style->borderBottomWidth());

        if (renderRadii && (!upperLeftBorderStylesMatch || !lowerLeftBorderStylesMatch)) {
            int topX = rect.x();
            thickness = style->borderLeftWidth() * 2;

            if (!upperLeftBorderStylesMatch && border.radii().topLeft().width()) {
                int topY = rect.y();
                bool applyTopInnerClip = (style->borderLeftWidth() < border.radii().topLeft().width())
                    && (style->borderTopWidth() < border.radii().topLeft().height())
                    && (leftStyle != DOUBLE || style->borderLeftWidth() > 6);

                GraphicsContextStateSaver stateSaver(*graphicsContext, applyTopInnerClip);
                if (applyTopInnerClip)
                    graphicsContext->addInnerRoundedRectClip(IntRect(topX, topY, border.radii().topLeft().width() * 2, border.radii().topLeft().height() * 2),
                                                             style->borderLeftWidth());

                firstAngleStart = 135;
                firstAngleSpan = 45;

                // Draw top left arc
                drawArcForBoxSide(graphicsContext, topX, topY, thickness, border.radii().topLeft(), firstAngleStart, firstAngleSpan,
                              BSLeft, leftColor, leftStyle, true);
            }

            if (!lowerLeftBorderStylesMatch && border.radii().bottomLeft().width()) {
                int bottomY = rect.maxY() - border.radii().bottomLeft().height() * 2;
                bool applyBottomInnerClip = (style->borderLeftWidth() < border.radii().bottomLeft().width())
                    && (style->borderBottomWidth() < border.radii().bottomLeft().height())
                    && (leftStyle != DOUBLE || style->borderLeftWidth() > 6);

                GraphicsContextStateSaver stateSaver(*graphicsContext, applyBottomInnerClip);
                if (applyBottomInnerClip)
                    graphicsContext->addInnerRoundedRectClip(IntRect(topX, bottomY, border.radii().bottomLeft().width() * 2, border.radii().bottomLeft().height() * 2),
                                                             style->borderLeftWidth());

                secondAngleStart = 180;
                secondAngleSpan = 45;

                // Draw bottom left arc
                drawArcForBoxSide(graphicsContext, topX, bottomY, thickness, border.radii().bottomLeft(), secondAngleStart, secondAngleSpan,
                              BSLeft, leftColor, leftStyle, false);
            }
        }
    }

    if (renderRight) {
        bool ignoreTop = (renderRadii && border.radii().topRight().height() > 0)
            || ((topColor == rightColor) && (topTransparent == rightTransparent)
                && (rightStyle >= DOTTED || rightStyle == INSET)
                && (topStyle == DOTTED || topStyle == DASHED || topStyle == SOLID || topStyle == OUTSET));

        bool ignoreBottom = (renderRadii && border.radii().bottomRight().height() > 0)
            || ((bottomColor == rightColor) && (bottomTransparent == rightTransparent)
                && (rightStyle >= DOTTED || rightStyle == INSET)
                && (bottomStyle == DOTTED || bottomStyle == DASHED || bottomStyle == SOLID || bottomStyle == INSET));

        int y = rect.y();
        int y2 = rect.maxY();
        if (renderRadii) {
            y += border.radii().topRight().height();
            y2 -= border.radii().bottomRight().height();
        }

        drawLineForBoxSide(graphicsContext, rect.maxX() - style->borderRightWidth(), y, rect.maxX(), y2, BSRight, rightColor, rightStyle,
                   ignoreTop ? 0 : style->borderTopWidth(), ignoreBottom ? 0 : style->borderBottomWidth());

        if (renderRadii && (!upperRightBorderStylesMatch || !lowerRightBorderStylesMatch)) {
            thickness = style->borderRightWidth() * 2;

            if (!upperRightBorderStylesMatch && border.radii().topRight().width()) {
                int topX = rect.maxX() - border.radii().topRight().width() * 2;
                int topY = rect.y();
                bool applyTopInnerClip = (style->borderRightWidth() < border.radii().topRight().width())
                    && (style->borderTopWidth() < border.radii().topRight().height())
                    && (rightStyle != DOUBLE || style->borderRightWidth() > 6);

                GraphicsContextStateSaver stateSaver(*graphicsContext, applyTopInnerClip);
                if (applyTopInnerClip)
                    graphicsContext->addInnerRoundedRectClip(IntRect(topX, topY, border.radii().topRight().width() * 2, border.radii().topRight().height() * 2),
                                                             style->borderRightWidth());

                firstAngleStart = 0;
                firstAngleSpan = 45;

                // Draw top right arc
                drawArcForBoxSide(graphicsContext, topX, topY, thickness, border.radii().topRight(), firstAngleStart, firstAngleSpan,
                              BSRight, rightColor, rightStyle, true);
            }

            if (!lowerRightBorderStylesMatch && border.radii().bottomRight().width()) {
                int bottomX = rect.maxX() - border.radii().bottomRight().width() * 2;
                int bottomY = rect.maxY() - border.radii().bottomRight().height() * 2;
                bool applyBottomInnerClip = (style->borderRightWidth() < border.radii().bottomRight().width())
                    && (style->borderBottomWidth() < border.radii().bottomRight().height())
                    && (rightStyle != DOUBLE || style->borderRightWidth() > 6);

                GraphicsContextStateSaver stateSaver(*graphicsContext, applyBottomInnerClip);
                if (applyBottomInnerClip)
                    graphicsContext->addInnerRoundedRectClip(IntRect(bottomX, bottomY, border.radii().bottomRight().width() * 2, border.radii().bottomRight().height() * 2),
                                                             style->borderRightWidth());

                secondAngleStart = 315;
                secondAngleSpan = 45;

                // Draw bottom right arc
                drawArcForBoxSide(graphicsContext, bottomX, bottomY, thickness, border.radii().bottomRight(), secondAngleStart, secondAngleSpan,
                              BSRight, rightColor, rightStyle, false);
            }
        }
    }
}
#endif

static void findInnerVertex(const FloatPoint& outerCorner, const FloatPoint& innerCorner, const FloatPoint& centerPoint, FloatPoint& result)
{
    // If the line between outer and inner corner is towards the horizontal, intersect with a vertical line through the center,
    // otherwise with a horizontal line through the center. The points that form this line are arbitrary (we use 0, 100).
    // Note that if findIntersection fails, it will leave result untouched.
    if (fabs(outerCorner.x() - innerCorner.x()) > fabs(outerCorner.y() - innerCorner.y()))
        findIntersection(outerCorner, innerCorner, FloatPoint(centerPoint.x(), 0), FloatPoint(centerPoint.x(), 100), result);
    else
        findIntersection(outerCorner, innerCorner, FloatPoint(0, centerPoint.y()), FloatPoint(100, centerPoint.y()), result);
}

void RenderBoxModelObject::clipBorderSidePolygon(GraphicsContext* graphicsContext, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
                                                 BoxSide side, bool firstEdgeMatches, bool secondEdgeMatches)
{
    FloatPoint quad[4];

    const LayoutRect& outerRect = outerBorder.rect();
    const LayoutRect& innerRect = innerBorder.rect();

    FloatPoint centerPoint(innerRect.location().x() + static_cast<float>(innerRect.width()) / 2, innerRect.location().y() + static_cast<float>(innerRect.height()) / 2);

    // For each side, create a quad that encompasses all parts of that side that may draw,
    // including areas inside the innerBorder.
    //
    //         0----------------3
    //       0  \              /  0
    //       |\  1----------- 2  /|
    //       | 1                1 |   
    //       | |                | |
    //       | |                | |  
    //       | 2                2 |  
    //       |/  1------------2  \| 
    //       3  /              \  3   
    //         0----------------3
    //
    switch (side) {
    case BSTop:
        quad[0] = outerRect.minXMinYCorner();
        quad[1] = innerRect.minXMinYCorner();
        quad[2] = innerRect.maxXMinYCorner();
        quad[3] = outerRect.maxXMinYCorner();

        if (!innerBorder.radii().topLeft().isZero())
            findInnerVertex(outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), centerPoint, quad[1]);

        if (!innerBorder.radii().topRight().isZero())
            findInnerVertex(outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), centerPoint, quad[2]);
        break;

    case BSLeft:
        quad[0] = outerRect.minXMinYCorner();
        quad[1] = innerRect.minXMinYCorner();
        quad[2] = innerRect.minXMaxYCorner();
        quad[3] = outerRect.minXMaxYCorner();

        if (!innerBorder.radii().topLeft().isZero())
            findInnerVertex(outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), centerPoint, quad[1]);

        if (!innerBorder.radii().bottomLeft().isZero())
            findInnerVertex(outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), centerPoint, quad[2]);
        break;

    case BSBottom:
        quad[0] = outerRect.minXMaxYCorner();
        quad[1] = innerRect.minXMaxYCorner();
        quad[2] = innerRect.maxXMaxYCorner();
        quad[3] = outerRect.maxXMaxYCorner();

        if (!innerBorder.radii().bottomLeft().isZero())
            findInnerVertex(outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), centerPoint, quad[1]);

        if (!innerBorder.radii().bottomRight().isZero())
            findInnerVertex(outerRect.maxXMaxYCorner(), innerRect.maxXMaxYCorner(), centerPoint, quad[2]);
        break;

    case BSRight:
        quad[0] = outerRect.maxXMinYCorner();
        quad[1] = innerRect.maxXMinYCorner();
        quad[2] = innerRect.maxXMaxYCorner();
        quad[3] = outerRect.maxXMaxYCorner();

        if (!innerBorder.radii().topRight().isZero())
            findInnerVertex(outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), centerPoint, quad[1]);

        if (!innerBorder.radii().bottomRight().isZero())
            findInnerVertex(outerRect.maxXMaxYCorner(), innerRect.maxXMaxYCorner(), centerPoint, quad[2]);
        break;
    }

    // If the border matches both of its adjacent sides, don't anti-alias the clip, and
    // if neither side matches, anti-alias the clip.
    if (firstEdgeMatches == secondEdgeMatches) {
        graphicsContext->clipConvexPolygon(4, quad, !firstEdgeMatches);
        return;
    }

    // Square off the end which shouldn't be affected by antialiasing, and clip.
    FloatPoint firstQuad[4];
    firstQuad[0] = quad[0];
    firstQuad[1] = quad[1];
    firstQuad[2] = side == BSTop || side == BSBottom ? FloatPoint(quad[3].x(), quad[2].y())
        : FloatPoint(quad[2].x(), quad[3].y());
    firstQuad[3] = quad[3];
    graphicsContext->clipConvexPolygon(4, firstQuad, !firstEdgeMatches);

    FloatPoint secondQuad[4];
    secondQuad[0] = quad[0];
    secondQuad[1] = side == BSTop || side == BSBottom ? FloatPoint(quad[0].x(), quad[1].y())
        : FloatPoint(quad[1].x(), quad[0].y());
    secondQuad[2] = quad[2];
    secondQuad[3] = quad[3];
    // Antialiasing affects the second side.
    graphicsContext->clipConvexPolygon(4, secondQuad, !secondEdgeMatches);
}

static LayoutRect calculateSideRectIncludingInner(const RoundedRect& outerBorder, const BorderEdge edges[], BoxSide side)
{
    LayoutRect sideRect = outerBorder.rect();
    int width;

    switch (side) {
    case BSTop:
        width = sideRect.height() - edges[BSBottom].width;
        sideRect.setHeight(width);
        break;
    case BSBottom:
        width = sideRect.height() - edges[BSTop].width;
        sideRect.shiftYEdgeTo(sideRect.maxY() - width);
        break;
    case BSLeft:
        width = sideRect.width() - edges[BSRight].width;
        sideRect.setWidth(width);
        break;
    case BSRight:
        width = sideRect.width() - edges[BSLeft].width;
        sideRect.shiftXEdgeTo(sideRect.maxX() - width);
        break;
    }

    return sideRect;
}

static RoundedRect calculateAdjustedInnerBorder(const RoundedRect&innerBorder, BoxSide side)
{
    // Expand the inner border as necessary to make it a rounded rect (i.e. radii contained within each edge).
    // This function relies on the fact we only get radii not contained within each edge if one of the radii
    // for an edge is zero, so we can shift the arc towards the zero radius corner.
    RoundedRect::Radii newRadii = innerBorder.radii();
    IntRect newRect = innerBorder.rect();

    float overshoot;
    float maxRadii;

    switch (side) {
    case BSTop:
        overshoot = newRadii.topLeft().width() + newRadii.topRight().width() - newRect.width();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topLeft().width() && newRadii.topRight().width()));
            newRect.setWidth(newRect.width() + overshoot);
            if (!newRadii.topLeft().width())
                newRect.move(-overshoot, 0);
        }
        newRadii.setBottomLeft(IntSize(0, 0));
        newRadii.setBottomRight(IntSize(0, 0));
        maxRadii = max(newRadii.topLeft().height(), newRadii.topRight().height());
        if (maxRadii > newRect.height())
            newRect.setHeight(maxRadii);
        break;

    case BSBottom:
        overshoot = newRadii.bottomLeft().width() + newRadii.bottomRight().width() - newRect.width();
        if (overshoot > 0) {
            ASSERT(!(newRadii.bottomLeft().width() && newRadii.bottomRight().width()));
            newRect.setWidth(newRect.width() + overshoot);
            if (!newRadii.bottomLeft().width())
                newRect.move(-overshoot, 0);
        }
        newRadii.setTopLeft(IntSize(0, 0));
        newRadii.setTopRight(IntSize(0, 0));
        maxRadii = max(newRadii.bottomLeft().height(), newRadii.bottomRight().height());
        if (maxRadii > newRect.height()) {
            newRect.move(0, newRect.height() - maxRadii);
            newRect.setHeight(maxRadii);
        }
        break;

    case BSLeft:
        overshoot = newRadii.topLeft().height() + newRadii.bottomLeft().height() - newRect.height();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topLeft().height() && newRadii.bottomLeft().height()));
            newRect.setHeight(newRect.height() + overshoot);
            if (!newRadii.topLeft().height())
                newRect.move(0, -overshoot);
        }
        newRadii.setTopRight(IntSize(0, 0));
        newRadii.setBottomRight(IntSize(0, 0));
        maxRadii = max(newRadii.topLeft().width(), newRadii.bottomLeft().width());
        if (maxRadii > newRect.width())
            newRect.setWidth(maxRadii);
        break;

    case BSRight:
        overshoot = newRadii.topRight().height() + newRadii.bottomRight().height() - newRect.height();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topRight().height() && newRadii.bottomRight().height()));
            newRect.setHeight(newRect.height() + overshoot);
            if (!newRadii.topRight().height())
                newRect.move(0, -overshoot);
        }
        newRadii.setTopLeft(IntSize(0, 0));
        newRadii.setBottomLeft(IntSize(0, 0));
        maxRadii = max(newRadii.topRight().width(), newRadii.bottomRight().width());
        if (maxRadii > newRect.width()) {
            newRect.move(newRect.width() - maxRadii, 0);
            newRect.setWidth(maxRadii);
        }
        break;
    }

    return RoundedRect(newRect, newRadii);
}

void RenderBoxModelObject::clipBorderSideForComplexInnerPath(GraphicsContext* graphicsContext, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
    BoxSide side, const class BorderEdge edges[])
{
    graphicsContext->clip(calculateSideRectIncludingInner(outerBorder, edges, side));
    graphicsContext->clipOutRoundedRect(calculateAdjustedInnerBorder(innerBorder, side));
}

void RenderBoxModelObject::getBorderEdgeInfo(BorderEdge edges[], bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    const RenderStyle* style = this->style();
    bool horizontal = style->isHorizontalWritingMode();

    edges[BSTop] = BorderEdge(style->borderTopWidth(),
        style->visitedDependentColor(CSSPropertyBorderTopColor),
        style->borderTopStyle(),
        style->borderTopIsTransparent(),
        horizontal || includeLogicalLeftEdge);

    edges[BSRight] = BorderEdge(style->borderRightWidth(),
        style->visitedDependentColor(CSSPropertyBorderRightColor),
        style->borderRightStyle(),
        style->borderRightIsTransparent(),
        !horizontal || includeLogicalRightEdge);

    edges[BSBottom] = BorderEdge(style->borderBottomWidth(),
        style->visitedDependentColor(CSSPropertyBorderBottomColor),
        style->borderBottomStyle(),
        style->borderBottomIsTransparent(),
        horizontal || includeLogicalRightEdge);

    edges[BSLeft] = BorderEdge(style->borderLeftWidth(),
        style->visitedDependentColor(CSSPropertyBorderLeftColor),
        style->borderLeftStyle(),
        style->borderLeftIsTransparent(),
        !horizontal || includeLogicalLeftEdge);
}

bool RenderBoxModelObject::borderObscuresBackgroundEdge(const FloatSize& contextScale) const
{
    BorderEdge edges[4];
    getBorderEdgeInfo(edges);

    for (int i = BSTop; i <= BSLeft; ++i) {
        const BorderEdge& currEdge = edges[i];
        // FIXME: for vertical text
        float axisScale = (i == BSTop || i == BSBottom) ? contextScale.height() : contextScale.width();
        if (!currEdge.obscuresBackgroundEdge(axisScale))
            return false;
    }

    return true;
}

bool RenderBoxModelObject::borderObscuresBackground() const
{
    if (!style()->hasBorder())
        return false;

    // Bail if we have any border-image for now. We could look at the image alpha to improve this.
    if (style()->borderImage().image())
        return false;

    BorderEdge edges[4];
    getBorderEdgeInfo(edges);

    for (int i = BSTop; i <= BSLeft; ++i) {
        const BorderEdge& currEdge = edges[i];
        if (!currEdge.obscuresBackground())
            return false;
    }

    return true;
}

static inline LayoutRect areaCastingShadowInHole(const LayoutRect& holeRect, int shadowBlur, int shadowSpread, const LayoutSize& shadowOffset)
{
    LayoutRect bounds(holeRect);
    
    bounds.inflate(shadowBlur);

    if (shadowSpread < 0)
        bounds.inflate(-shadowSpread);
    
    LayoutRect offsetBounds = bounds;
    offsetBounds.move(-shadowOffset);
    return unionRect(bounds, offsetBounds);
}

void RenderBoxModelObject::paintBoxShadow(const PaintInfo& info, const LayoutRect& paintRect, const RenderStyle* s, ShadowStyle shadowStyle, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    // FIXME: Deal with border-image.  Would be great to use border-image as a mask.
    GraphicsContext* context = info.context;
    if (context->paintingDisabled() || !s->boxShadow())
        return;

    RoundedRect border = (shadowStyle == Inset) ? s->getRoundedInnerBorderFor(paintRect, includeLogicalLeftEdge, includeLogicalRightEdge)
                                                   : s->getRoundedBorderFor(paintRect, includeLogicalLeftEdge, includeLogicalRightEdge);

    bool hasBorderRadius = s->hasBorderRadius();
    bool isHorizontal = s->isHorizontalWritingMode();
    
    bool hasOpaqueBackground = s->visitedDependentColor(CSSPropertyBackgroundColor).isValid() && s->visitedDependentColor(CSSPropertyBackgroundColor).alpha() == 255;
    for (const ShadowData* shadow = s->boxShadow(); shadow; shadow = shadow->next()) {
        if (shadow->style() != shadowStyle)
            continue;

        LayoutSize shadowOffset(shadow->x(), shadow->y());
        LayoutUnit shadowBlur = shadow->blur();
        LayoutUnit shadowSpread = shadow->spread();
        
        if (shadowOffset.isZero() && !shadowBlur && !shadowSpread)
            continue;
        
        const Color& shadowColor = shadow->color();

        if (shadow->style() == Normal) {
            RoundedRect fillRect = border;
            fillRect.inflate(shadowSpread);
            if (fillRect.isEmpty())
                continue;

            LayoutRect shadowRect(border.rect());
            shadowRect.inflate(shadowBlur + shadowSpread);
            shadowRect.move(shadowOffset);

            GraphicsContextStateSaver stateSaver(*context);
            context->clip(shadowRect);

            // Move the fill just outside the clip, adding 1 pixel separation so that the fill does not
            // bleed in (due to antialiasing) if the context is transformed.
            LayoutSize extraOffset(paintRect.width() + max<LayoutUnit>(0, shadowOffset.width()) + shadowBlur + 2 * shadowSpread + 1, 0);
            shadowOffset -= extraOffset;
            fillRect.move(extraOffset);

            if (shadow->isWebkitBoxShadow())
                context->setLegacyShadow(shadowOffset, shadowBlur, shadowColor, s->colorSpace());
            else
                context->setShadow(shadowOffset, shadowBlur, shadowColor, s->colorSpace());

            if (hasBorderRadius) {
                RoundedRect rectToClipOut = border;

                // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
                // when painting the shadow. On the other hand, it introduces subpixel gaps along the
                // corners. Those are avoided by insetting the clipping path by one pixel.
                if (hasOpaqueBackground) {
                    rectToClipOut.inflateWithRadii(-1);
                }

                if (!rectToClipOut.isEmpty())
                    context->clipOutRoundedRect(rectToClipOut);

                RoundedRect influenceRect(shadowRect, border.radii());
                influenceRect.expandRadii(2 * shadowBlur + shadowSpread);
                if (allCornersClippedOut(influenceRect, info.rect))
                    context->fillRect(fillRect.rect(), Color::black, s->colorSpace());
                else {
                    fillRect.expandRadii(shadowSpread);
                    context->fillRoundedRect(fillRect, Color::black, s->colorSpace());
                }
            } else {
                LayoutRect rectToClipOut = border.rect();

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
                context->fillRect(fillRect.rect(), Color::black, s->colorSpace());
            }
        } else {
            // Inset shadow.
            LayoutRect holeRect(border.rect());
            holeRect.inflate(-shadowSpread);

            if (holeRect.isEmpty()) {
                if (hasBorderRadius)
                    context->fillRoundedRect(border, shadowColor, s->colorSpace());
                else
                    context->fillRect(border.rect(), shadowColor, s->colorSpace());
                continue;
            }

            if (!includeLogicalLeftEdge) {
                if (isHorizontal) {
                    holeRect.move(-max<LayoutUnit>(shadowOffset.width(), 0) - shadowBlur, 0);
                    holeRect.setWidth(holeRect.width() + max<LayoutUnit>(shadowOffset.width(), 0) + shadowBlur);
                } else {
                    holeRect.move(0, -max<LayoutUnit>(shadowOffset.height(), 0) - shadowBlur);
                    holeRect.setHeight(holeRect.height() + max<LayoutUnit>(shadowOffset.height(), 0) + shadowBlur);
                }
            }
            if (!includeLogicalRightEdge) {
                if (isHorizontal)
                    holeRect.setWidth(holeRect.width() - min<LayoutUnit>(shadowOffset.width(), 0) + shadowBlur);
                else
                    holeRect.setHeight(holeRect.height() - min<LayoutUnit>(shadowOffset.height(), 0) + shadowBlur);
            }

            Color fillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), 255);

            LayoutRect outerRect = areaCastingShadowInHole(border.rect(), shadowBlur, shadowSpread, shadowOffset);
            RoundedRect roundedHole(holeRect, border.radii());

            GraphicsContextStateSaver stateSaver(*context);
            if (hasBorderRadius) {
                Path path;
                path.addRoundedRect(border);
                context->clip(path);
                roundedHole.shrinkRadii(shadowSpread);
            } else
                context->clip(border.rect());

            LayoutSize extraOffset(2 * paintRect.width() + max<LayoutUnit>(0, shadowOffset.width()) + shadowBlur - 2 * shadowSpread + 1, 0);
            context->translate(extraOffset.width(), extraOffset.height());
            shadowOffset -= extraOffset;

            if (shadow->isWebkitBoxShadow())
                context->setLegacyShadow(shadowOffset, shadowBlur, shadowColor, s->colorSpace());
            else
                context->setShadow(shadowOffset, shadowBlur, shadowColor, s->colorSpace());

            context->fillRectWithRoundedHole(outerRect, roundedHole, fillColor, s->colorSpace());
        }
    }
}

LayoutUnit RenderBoxModelObject::containingBlockLogicalWidthForContent() const
{
    return containingBlock()->availableLogicalWidth();
}

RenderBoxModelObject* RenderBoxModelObject::continuation() const
{
    if (!continuationMap)
        return 0;
    return continuationMap->get(this);
}

void RenderBoxModelObject::setContinuation(RenderBoxModelObject* continuation)
{
    if (continuation) {
        if (!continuationMap)
            continuationMap = new ContinuationMap;
        continuationMap->set(this, continuation);
    } else {
        if (continuationMap)
            continuationMap->remove(this);
    }
}

bool RenderBoxModelObject::shouldAntialiasLines(GraphicsContext* context)
{
    // FIXME: We may want to not antialias when scaled by an integral value,
    // and we may want to antialias when translated by a non-integral value.
    return !context->getCTM().isIdentityOrTranslationOrFlipped();
}

void RenderBoxModelObject::mapAbsoluteToLocalPoint(bool fixed, bool useTransforms, TransformState& transformState) const
{
    // We don't expect absoluteToLocal() to be called during layout (yet)
    ASSERT(!view() || !view()->layoutStateEnabled());

    RenderObject* o = container();
    if (!o)
        return;

    o->mapAbsoluteToLocalPoint(fixed, useTransforms, transformState);

    LayoutSize containerOffset = offsetFromContainer(o, LayoutPoint());

    if (style()->position() != AbsolutePosition && style()->position() != FixedPosition && o->hasColumns()) {
        RenderBlock* block = static_cast<RenderBlock*>(o);
        LayoutPoint point(roundedLayoutPoint(transformState.mappedPoint()));
        point -= containerOffset;
        block->adjustForColumnRect(containerOffset, point);
    }

    bool preserve3D = useTransforms && (o->style()->preserves3D() || style()->preserves3D());
    if (useTransforms && shouldUseTransformFromContainer(o)) {
        TransformationMatrix t;
        getTransformFromContainer(o, containerOffset, t);
        transformState.applyTransform(t, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    } else
        transformState.move(containerOffset.width(), containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
}

} // namespace WebCore
