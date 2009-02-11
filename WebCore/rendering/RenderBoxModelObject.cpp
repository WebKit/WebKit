/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

using namespace std;

namespace WebCore {

using namespace HTMLNames;

bool RenderBoxModelObject::s_wasFloating = false;

RenderBoxModelObject::RenderBoxModelObject(Node* node)
    : RenderObject(node)
    , m_layer(0)
{
}

RenderBoxModelObject::~RenderBoxModelObject()
{
}

void RenderBoxModelObject::destroy()
{
    // This must be done before we destroy the RenderObject.
    if (m_layer)
        m_layer->clearClipRects();
    RenderObject::destroy();
}

void RenderBoxModelObject::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    s_wasFloating = isFloating();
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
        
        RenderLayer* layer = m_layer;
        m_layer = 0;
        setHasLayer(false);
        setHasTransform(false); // Either a transform wasn't specified or the object doesn't support transforms, so just null out the bit.
        setHasReflection(false);
        layer->removeOnlyThisLayer();
        if (s_wasFloating && isFloating())
            setChildNeedsLayout(true);
    }

    if (m_layer)
        m_layer->styleChanged(diff, oldStyle);
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
    if (!style()->left().isAuto()) {
        if (!style()->right().isAuto() && containingBlock()->style()->direction() == RTL)
            return -style()->right().calcValue(containingBlockWidth());
        return style()->left().calcValue(containingBlockWidth());
    }
    if (!style()->right().isAuto())
        return -style()->right().calcValue(containingBlockWidth());
    return 0;
}

int RenderBoxModelObject::relativePositionOffsetY() const
{
    if (!style()->top().isAuto()) {
        if (!style()->top().isPercent() || containingBlock()->style()->height().isFixed())
            return style()->top().calcValue(containingBlockHeight());
    } else if (!style()->bottom().isAuto()) {
        if (!style()->bottom().isPercent() || containingBlock()->style()->height().isFixed())
            return -style()->bottom().calcValue(containingBlockHeight());
    }
    return 0;
}

int RenderBoxModelObject::offsetLeft() const
{
    RenderBoxModelObject* offsetPar = offsetParent();
    if (!offsetPar)
        return 0;
    int xPos = (isBox() ? toRenderBox(this)->x() : 0);
    if (offsetPar->isBox())
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
    return xPos;
}

int RenderBoxModelObject::offsetTop() const
{
    RenderBoxModelObject* offsetPar = offsetParent();
    if (!offsetPar)
        return 0;
    int yPos = (isBox() ? toRenderBox(this)->y() : 0);
    if (offsetPar->isBox())
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


void RenderBoxModelObject::paintFillLayerExtended(const PaintInfo& paintInfo, const Color& c, const FillLayer* bgLayer, int clipY, int clipH,
                                                  int tx, int ty, int w, int h, InlineFlowBox* box, CompositeOperator op)
{
    GraphicsContext* context = paintInfo.context;
    bool includeLeftEdge = box ? box->includeLeftEdge() : true;
    bool includeRightEdge = box ? box->includeRightEdge() : true;
    int bLeft = includeLeftEdge ? borderLeft() : 0;
    int bRight = includeRightEdge ? borderRight() : 0;
    int pLeft = includeLeftEdge ? paddingLeft() : 0;
    int pRight = includeRightEdge ? paddingRight() : 0;

    bool clippedToBorderRadius = false;
    if (style()->hasBorderRadius() && (includeLeftEdge || includeRightEdge)) {
        context->save();
        context->addRoundedRectClip(IntRect(tx, ty, w, h),
            includeLeftEdge ? style()->borderTopLeftRadius() : IntSize(),
            includeRightEdge ? style()->borderTopRightRadius() : IntSize(),
            includeLeftEdge ? style()->borderBottomLeftRadius() : IntSize(),
            includeRightEdge ? style()->borderBottomRightRadius() : IntSize());
        clippedToBorderRadius = true;
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
        auto_ptr<ImageBuffer> maskImage = ImageBuffer::create(maskRect.size(), false);
        if (!maskImage.get())
            return;
        
        GraphicsContext* maskImageContext = maskImage->context();
        maskImageContext->translate(-maskRect.x(), -maskRect.y());
        
        // Now add the text to the clip.  We do this by painting using a special paint phase that signals to
        // InlineTextBoxes that they should just add their contents to the clip.
        PaintInfo info(maskImageContext, maskRect, PaintPhaseTextClip, true, 0, 0);
        if (box)
            box->paint(info, tx - box->x(), ty - box->y());
        else
            paint(info, tx, ty);
            
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

    // Only fill with a base color (e.g., white) if we're the root document, since iframes/frames with
    // no background in the child document should show the parent's background.
    bool isTransparent = false;
    if (!bgLayer->next() && isRoot() && !(bgColor.isValid() && bgColor.alpha() > 0) && view()->frameView()) {
        Node* elt = document()->ownerElement();
        if (elt) {
            if (!elt->hasTagName(frameTag)) {
                // Locate the <body> element using the DOM.  This is easier than trying
                // to crawl around a render tree with potential :before/:after content and
                // anonymous blocks created by inline <body> tags etc.  We can locate the <body>
                // render object very easily via the DOM.
                HTMLElement* body = document()->body();
                isTransparent = !body || !body->hasLocalName(framesetTag); // Can't scroll a frameset document anyway.
            }
        } else
            isTransparent = view()->frameView()->isTransparent();

        // FIXME: This needs to be dynamic.  We should be able to go back to blitting if we ever stop being transparent.
        if (isTransparent)
            view()->frameView()->setUseSlowRepaints(); // The parent must show behind the child.
    }

    // Paint the color first underneath all images.
    if (!bgLayer->next()) {
        IntRect rect(tx, clipY, w, clipH);
        // If we have an alpha and we are painting the root element, go ahead and blend with the base background color.
        if (isRoot() && (!bgColor.isValid() || bgColor.alpha() < 0xFF) && !isTransparent) {
            Color baseColor = view()->frameView()->baseBackgroundColor();
            if (baseColor.alpha() > 0) {
                context->save();
                context->setCompositeOperation(CompositeCopy);
                context->fillRect(rect, baseColor);
                context->restore();
            } else
                context->clearRect(rect);
        }

        if (bgColor.isValid() && bgColor.alpha() > 0)
            context->fillRect(rect, bgColor);
    }

    // no progressive loading of the background image
    if (shouldPaintBackgroundImage) {
        IntRect destRect;
        IntPoint phase;
        IntSize tileSize;

        calculateBackgroundImageGeometry(bgLayer, tx, ty, w, h, destRect, phase, tileSize);
        if (!destRect.isEmpty()) {
            CompositeOperator compositeOp = op == CompositeSourceOver ? bgLayer->composite() : op;
            context->drawTiledImage(bg->image(this, tileSize), destRect, phase, tileSize, compositeOp);
        }
    }

    if (bgLayer->clip() != BorderFillBox)
        // Undo the background clip
        context->restore();

    if (clippedToBorderRadius)
        // Undo the border radius clip
        context->restore();
}

IntSize RenderBoxModelObject::calculateBackgroundSize(const FillLayer* bgLayer, int scaledWidth, int scaledHeight) const
{
    StyleImage* bg = bgLayer->image();
    bg->setImageContainerSize(IntSize(scaledWidth, scaledHeight)); // Use the box established by background-origin.

    if (bgLayer->isSizeSet()) {
        int w = scaledWidth;
        int h = scaledHeight;
        Length bgWidth = bgLayer->size().width();
        Length bgHeight = bgLayer->size().height();

        if (bgWidth.isFixed())
            w = bgWidth.value();
        else if (bgWidth.isPercent())
            w = bgWidth.calcValue(scaledWidth);
        
        if (bgHeight.isFixed())
            h = bgHeight.value();
        else if (bgHeight.isPercent())
            h = bgHeight.calcValue(scaledHeight);
        
        // If one of the values is auto we have to use the appropriate
        // scale to maintain our aspect ratio.
        if (bgWidth.isAuto() && !bgHeight.isAuto())
            w = bg->imageSize(this, style()->effectiveZoom()).width() * h / bg->imageSize(this, style()->effectiveZoom()).height();        
        else if (!bgWidth.isAuto() && bgHeight.isAuto())
            h = bg->imageSize(this, style()->effectiveZoom()).height() * w / bg->imageSize(this, style()->effectiveZoom()).width();
        else if (bgWidth.isAuto() && bgHeight.isAuto()) {
            // If both width and height are auto, we just want to use the image's
            // intrinsic size.
            w = bg->imageSize(this, style()->effectiveZoom()).width();
            h = bg->imageSize(this, style()->effectiveZoom()).height();
        }
        
        return IntSize(max(1, w), max(1, h));
    } else
        return bg->imageSize(this, style()->effectiveZoom());
}

void RenderBoxModelObject::calculateBackgroundImageGeometry(const FillLayer* bgLayer, int tx, int ty, int w, int h, 
                                                            IntRect& destRect, IntPoint& phase, IntSize& tileSize)
{
    int pw;
    int ph;
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
    int cx;
    int cy;
    int rw = 0;
    int rh = 0;

    // CSS2 chapter 14.2.1

    if (bgLayer->attachment()) {
        // Scroll
        if (bgLayer->origin() != BorderFillBox) {
            left = borderLeft();
            right = borderRight();
            top = borderTop();
            bottom = borderBottom();
            if (bgLayer->origin() == ContentFillBox) {
                left += paddingLeft();
                right += paddingRight();
                top += paddingTop();
                bottom += paddingBottom();
            }
        }
        
        // The background of the box generated by the root element covers the entire canvas including
        // its margins.  Since those were added in already, we have to factor them out when computing the
        // box used by background-origin/size/position.
        if (isRoot()) {
            rw = toRenderBox(this)->width() - left - right;
            rh = toRenderBox(this)->height() - top - bottom; 
            left += marginLeft();
            right += marginRight();
            top += marginTop();
            bottom += marginBottom();
        }
        cx = tx;
        cy = ty;
        pw = w - left - right;
        ph = h - top - bottom;
    } else {
        // Fixed
        IntRect vr = viewRect();
        cx = vr.x();
        cy = vr.y();
        pw = vr.width();
        ph = vr.height();
    }

    int sx = 0;
    int sy = 0;
    int cw;
    int ch;

    IntSize scaledImageSize;
    if (isRoot() && bgLayer->attachment())
        scaledImageSize = calculateBackgroundSize(bgLayer, rw, rh);
    else
        scaledImageSize = calculateBackgroundSize(bgLayer, pw, ph);
        
    int scaledImageWidth = scaledImageSize.width();
    int scaledImageHeight = scaledImageSize.height();

    EFillRepeat backgroundRepeat = bgLayer->repeat();
    
    int xPosition;
    if (isRoot() && bgLayer->attachment())
        xPosition = bgLayer->xPosition().calcMinValue(rw - scaledImageWidth, true);
    else
        xPosition = bgLayer->xPosition().calcMinValue(pw - scaledImageWidth, true);
    if (backgroundRepeat == RepeatFill || backgroundRepeat == RepeatXFill) {
        cw = pw + left + right;
        sx = scaledImageWidth ? scaledImageWidth - (xPosition + left) % scaledImageWidth : 0;
    } else {
        cx += max(xPosition + left, 0);
        sx = -min(xPosition + left, 0);
        cw = scaledImageWidth + min(xPosition + left, 0);
    }
    
    int yPosition;
    if (isRoot() && bgLayer->attachment())
        yPosition = bgLayer->yPosition().calcMinValue(rh - scaledImageHeight, true);
    else 
        yPosition = bgLayer->yPosition().calcMinValue(ph - scaledImageHeight, true);
    if (backgroundRepeat == RepeatFill || backgroundRepeat == RepeatYFill) {
        ch = ph + top + bottom;
        sy = scaledImageHeight ? scaledImageHeight - (yPosition + top) % scaledImageHeight : 0;
    } else {
        cy += max(yPosition + top, 0);
        sy = -min(yPosition + top, 0);
        ch = scaledImageHeight + min(yPosition + top, 0);
    }

    if (!bgLayer->attachment()) {
        sx += max(tx - cx, 0);
        sy += max(ty - cy, 0);
    }

    destRect = IntRect(cx, cy, cw, ch);
    destRect.intersect(IntRect(tx, ty, w, h));
    phase = IntPoint(sx, sy);
    tileSize = IntSize(scaledImageWidth, scaledImageHeight);
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
            if (!isReplaced())
                vpos -= style(firstLine)->font().descent();
        } else if (va == BASELINE_MIDDLE)
            vpos += -lineHeight(firstLine) / 2 + baselinePosition(firstLine);
        else if (va == LENGTH)
            vpos -= style()->verticalAlignLength().calcValue(lineHeight(firstLine));
    }

    return vpos;
}

} // namespace WebCore
