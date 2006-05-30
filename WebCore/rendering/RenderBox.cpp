/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderBox.h"

#include "CachedImage.h"
#include "Document.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "RenderArena.h"
#include "RenderCanvas.h"
#include "RenderFlexibleBox.h"
#include "RenderTableCell.h"
#include "RenderTheme.h"
#include <assert.h>
#include <algorithm>
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

#define TABLECELLMARGIN -0x4000

RenderBox::RenderBox(WebCore::Node* node)
    : RenderObject(node)
{
    m_minWidth = -1;
    m_maxWidth = -1;
    m_overrideSize = -1;
    m_width = m_height = 0;
    m_x = 0;
    m_y = 0;
    m_marginTop = 0;
    m_marginBottom = 0;
    m_marginLeft = 0;
    m_marginRight = 0;
    m_staticX = 0;
    m_staticY = 0;
    m_layer = 0;
    m_inlineBoxWrapper = 0;
}

void RenderBox::setStyle(RenderStyle *_style)
{
    RenderObject::setStyle(_style);

    // The root always paints its background/border.
    if (isRoot())
        setShouldPaintBackgroundOrBorder(true);

    setInline(_style->isDisplayInlineType());

    switch(_style->position())
    {
    case AbsolutePosition:
    case FixedPosition:
        setPositioned(true);
        break;
    default:
        setPositioned(false);

        if (_style->isFloating())
            setFloating(true);

        if (_style->position() == RelativePosition)
            setRelPositioned(true);
    }

    // We also handle <body> and <html>, whose overflow applies to the viewport.
    if (_style->overflow() != OVISIBLE && !isRoot() && (!isBody() || !document()->isHTMLDocument()) &&
        (isRenderBlock() || isTableRow() || isTableSection()))
        setHasOverflowClip();

    if (requiresLayer()) {
        if (!m_layer) {
            m_layer = new (renderArena()) RenderLayer(this);
            m_layer->insertOnlyThisLayer();
            if (parent() && containingBlock())
                m_layer->updateLayerPositions();
        }
    }
    else if (m_layer && !isRoot() && !isCanvas()) {
        assert(m_layer->parent());
        RenderLayer *layer = m_layer;
        m_layer = 0;
        layer->removeOnlyThisLayer();
    }

    if (m_layer)
        m_layer->styleChanged();
    
    // Set the text color if we're the body.
    if (isBody())
        element()->document()->setTextColor(_style->color());
    
    if (style()->outlineWidth() > 0 && style()->outlineSize() > maximalOutlineSize(PaintPhaseOutline))
        static_cast<RenderCanvas*>(document()->renderer())->setMaximalOutlineSize(style()->outlineSize());
}

RenderBox::~RenderBox()
{
}

void RenderBox::destroy()
{
    // A lot of the code in this funtion is just pasted into
    // RenderWidget::destroy. If anything in this function changes,
    // be sure to fix RenderWidget::destroy() as well. 

    RenderLayer* layer = m_layer;
    RenderArena* arena = renderArena();
    
    // This must be done before we destroy the RenderObject.
    if (layer)
        layer->clearClipRect();

    RenderObject::destroy();
    
    if (layer)
        layer->destroy(arena);
}

int RenderBox::contentWidth() const
{
    int w = m_width - borderLeft() - borderRight();
    w -= paddingLeft() + paddingRight();

    if (includeScrollbarSize())
        w -= m_layer->verticalScrollbarWidth();
    
    return w;
}

int RenderBox::contentHeight() const
{
    int h = m_height - borderTop() - borderBottom();
    h -= paddingTop() + paddingBottom();

    if (includeScrollbarSize())
        h -= m_layer->horizontalScrollbarHeight();

    return h;
}

int RenderBox::overrideWidth() const
{
    return m_overrideSize == -1 ? m_width : m_overrideSize;
}

int RenderBox::overrideHeight() const
{
    return m_overrideSize == -1 ? m_height : m_overrideSize;
}

void RenderBox::setPos( int xPos, int yPos )
{
    if (xPos == m_x && yPos == m_y)
        return; // Optimize for the case where we don't move at all.
    
    m_x = xPos; m_y = yPos;
}

int RenderBox::width() const
{
    return m_width;
}

int RenderBox::height() const
{
    return m_height;
}

int RenderBox::calcBorderBoxWidth(int w) const
{
    int toAdd = borderLeft() + borderRight() + paddingLeft() + paddingRight();
    if (style()->boxSizing() == CONTENT_BOX)
        return w + toAdd;
    return max(w, toAdd);
}

int RenderBox::calcBorderBoxHeight(int h) const
{
    int toAdd = borderTop() + borderBottom() + paddingTop() + paddingBottom();
    if (style()->boxSizing() == CONTENT_BOX)
        return h + toAdd;
    return max(h, toAdd);
}

int RenderBox::calcContentBoxWidth(int w) const
{
    if (style()->boxSizing() == BORDER_BOX)
        w -= (borderLeft() + borderRight() + paddingLeft() + paddingRight());
    return max(0, w);
}

int RenderBox::calcContentBoxHeight(int h) const
{
    if (style()->boxSizing() == BORDER_BOX)
        h -= (borderTop() + borderBottom() + paddingTop() + paddingBottom());
    return max(0, h);
}

// Hit Testing
bool RenderBox::nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty, HitTestAction action)
{
    tx += m_x;
    ty += m_y;

    // Check kids first.
    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        // FIXME: We have to skip over inline flows, since they can show up inside table rows
        // at the moment (a demoted inline <form> for example). If we ever implement a
        // table-specific hit-test method (which we should do for performance reasons anyway),
        // then we can remove this check.
        if (!child->layer() && !child->isInlineFlow() && child->nodeAtPoint(info, x, y, tx, ty, action)) {
            setInnerNode(info);
            return true;
        }
    }
    
    // Check our bounds next. For this purpose always assume that we can only be hit in the
    // foreground phase (which is true for replaced elements like images).
    if (action == HitTestForeground && IntRect(tx, ty, m_width, m_height).contains(x, y)) {
        setInnerNode(info);
        return true;
    }

    return false;
}

// --------------------- painting stuff -------------------------------

void RenderBox::paint(PaintInfo& i, int _tx, int _ty)
{
    _tx += m_x;
    _ty += m_y;

    // default implementation. Just pass paint through to the children
    for (RenderObject* child = firstChild(); child; child = child->nextSibling())
        child->paint(i, _tx, _ty);
}

void RenderBox::paintRootBoxDecorations(PaintInfo& i, int _tx, int _ty)
{
    const BackgroundLayer* bgLayer = style()->backgroundLayers();
    Color bgColor = style()->backgroundColor();
    if (document()->isHTMLDocument() && !style()->hasBackground()) {
        // Locate the <body> element using the DOM.  This is easier than trying
        // to crawl around a render tree with potential :before/:after content and
        // anonymous blocks created by inline <body> tags etc.  We can locate the <body>
        // render object very easily via the DOM.
        HTMLElement* body = document()->body();
        RenderObject* bodyObject = (body && body->hasLocalName(bodyTag)) ? body->renderer() : 0;
        if (bodyObject) {
            bgLayer = bodyObject->style()->backgroundLayers();
            bgColor = bodyObject->style()->backgroundColor();
        }
    }

    int w = width();
    int h = height();

    int rw, rh;
    if (canvas()->view()) {
        rw = canvas()->view()->contentsWidth();
        rh = canvas()->view()->contentsHeight();
    }
    else {
        rw = canvas()->width();
        rh = canvas()->height();
    }
    
    int bx = _tx - marginLeft();
    int by = _ty - marginTop();
    int bw = max(w + marginLeft() + marginRight() + borderLeft() + borderRight(), rw);
    int bh = max(h + marginTop() + marginBottom() + borderTop() + borderBottom(), rh);

    // CSS2 14.2:
    // " The background of the box generated by the root element covers the entire canvas."
    // hence, paint the background even in the margin areas (unlike for every other element!)
    // I just love these little inconsistencies .. :-( (Dirk)
    int my = max(by, i.r.y());

    paintBackgrounds(i.p, bgColor, bgLayer, my, i.r.height(), bx, by, bw, bh);

    if (style()->hasBorder() && style()->display() != INLINE)
        paintBorder( i.p, _tx, _ty, w, h, style() );
}

void RenderBox::paintBoxDecorations(PaintInfo& i, int _tx, int _ty)
{
    if (!shouldPaintWithinRoot(i))
        return;

    if (isRoot())
        return paintRootBoxDecorations(i, _tx, _ty);
    
    int w = width();
    int h = height() + borderTopExtra() + borderBottomExtra();
    _ty -= borderTopExtra();

    int my = max(_ty, i.r.y());
    int mh;
    if (_ty < i.r.y())
        mh = max(0, h - (i.r.y() - _ty));
    else
        mh = min(i.r.height(), h);

    // If we have a native theme appearance, paint that before painting our background.  
    // The theme will tell us whether or not we should also paint the CSS background.
    bool themePainted = style()->hasAppearance() && !theme()->paint(this, i, IntRect(_tx, _ty, w, h));
    if (!themePainted) {
        // The <body> only paints its background if the root element has defined a background
        // independent of the body.  Go through the DOM to get to the root element's render object,
        // since the root could be inline and wrapped in an anonymous block.
        if (!isBody() || !document()->isHTMLDocument() || document()->documentElement()->renderer()->style()->hasBackground())
            paintBackgrounds(i.p, style()->backgroundColor(), style()->backgroundLayers(), my, mh, _tx, _ty, w, h);
    }
    
    // The theme will tell us whether or not we should also paint the CSS border.
    if ((!style()->hasAppearance() || (!themePainted && theme()->paintBorderOnly(this, i, IntRect(_tx, _ty, w, h)))) && style()->hasBorder())
        paintBorder(i.p, _tx, _ty, w, h, style());
}

void RenderBox::paintBackgrounds(GraphicsContext* p, const Color& c, const BackgroundLayer* bgLayer, int clipy, int cliph, int _tx, int _ty, int w, int height)
{
    if (!bgLayer) return;
    paintBackgrounds(p, c, bgLayer->next(), clipy, cliph, _tx, _ty, w, height);
    paintBackground(p, c, bgLayer, clipy, cliph, _tx, _ty, w, height);
}

void RenderBox::paintBackground(GraphicsContext* p, const Color& c, const BackgroundLayer* bgLayer, int clipy, int cliph, int _tx, int _ty, int w, int height)
{
    paintBackgroundExtended(p, c, bgLayer, clipy, cliph, _tx, _ty, w, height,
                            borderLeft(), borderRight(), paddingLeft(), paddingRight());
}

static void cacluateBackgroundSize(const BackgroundLayer* bgLayer, int& scaledWidth, int& scaledHeight)
{
    CachedImage* bg = bgLayer->backgroundImage();
    
    if (bgLayer->isBackgroundSizeSet()) {
        Length bgWidth = bgLayer->backgroundSize().width;
        Length bgHeight = bgLayer->backgroundSize().height;
        
        if (bgWidth.isPercent())
            scaledWidth = scaledWidth * bgWidth.value() / 100;
        else if (bgWidth.isFixed())
            scaledWidth = bgWidth.value();
        else if (bgWidth.isAuto()) {
            // If the width is auto and the height is not, we have to use the appropriate
            // scale to maintain our aspect ratio.
            if (bgHeight.isPercent()) {
                int scaledH = scaledHeight * bgHeight.value() / 100;
                scaledWidth = bg->imageSize().width() * scaledH / bg->imageSize().height();
            } else if (bgHeight.isFixed())
                scaledWidth = bg->imageSize().width() * bgHeight.value() / bg->imageSize().height();
        }
            
        if (bgHeight.isPercent())
            scaledHeight = scaledHeight * bgHeight.value() / 100;
        else if (bgHeight.isFixed())
            scaledHeight = bgHeight.value();
        else if (bgHeight.isAuto()) {
            // If the height is auto and the width is not, we have to use the appropriate
            // scale to maintain our aspect ratio.
            if (bgWidth.isPercent())
                scaledHeight = bg->imageSize().height() * scaledWidth / bg->imageSize().width();
            else if (bgWidth.isFixed())
                scaledHeight = bg->imageSize().height() * bgWidth.value() / bg->imageSize().width();
            else if (bgWidth.isAuto()) {
                // If both width and height are auto, we just want to use the image's
                // intrinsic size.
                scaledWidth = bg->imageSize().width();
                scaledHeight = bg->imageSize().height();
            }
        }
    } else {
        scaledWidth = bg->imageSize().width();
        scaledHeight = bg->imageSize().height();
    }
}

void RenderBox::paintBackgroundExtended(GraphicsContext* p, const Color& c, const BackgroundLayer* bgLayer, int clipy, int cliph,
                                        int _tx, int _ty, int w, int h,
                                        int bleft, int bright, int pleft, int pright)
{
    bool clippedToBorderRadius = false;
    if (style()->hasBorderRadius()) {
        p->save();
        p->addRoundedRectClip(IntRect(_tx, _ty, w, h),
            style()->borderTopLeftRadius(), style()->borderTopRightRadius(),
            style()->borderBottomLeftRadius(), style()->borderBottomRightRadius());
        clippedToBorderRadius = true;
    }
    
    if (bgLayer->backgroundClip() != BGBORDER) {
        // Clip to the padding or content boxes as necessary.
        bool includePadding = bgLayer->backgroundClip() == BGCONTENT;
        int x = _tx + bleft + (includePadding ? pleft : 0);
        int y = _ty + borderTop() + (includePadding ? paddingTop() : 0);
        int width = w - bleft - bright - (includePadding ? pleft + pright : 0);
        int height = h - borderTop() - borderBottom() - (includePadding ? paddingTop() + paddingBottom() : 0);
        p->save();
        p->addClip(IntRect(x, y, width, height));
    }

    CachedImage* bg = bgLayer->backgroundImage();
    bool shouldPaintBackgroundImage = bg && bg->canRender();
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
    if (!bgLayer->next() && isRoot() && !(bgColor.isValid() && bgColor.alpha() > 0) && canvas()->view()) {
        bool isTransparent;
        WebCore::Node* elt = document()->ownerElement();
        if (elt) {
            if (elt->hasTagName(frameTag))
                isTransparent = false;
            else {
                // Locate the <body> element using the DOM.  This is easier than trying
                // to crawl around a render tree with potential :before/:after content and
                // anonymous blocks created by inline <body> tags etc.  We can locate the <body>
                // render object very easily via the DOM.
                HTMLElement* body = document()->body();
                isTransparent = !body || !body->hasLocalName(framesetTag); // Can't scroll a frameset document anyway.
            }
        } else
            isTransparent = canvas()->view()->isTransparent();
        
        if (isTransparent)
            canvas()->view()->useSlowRepaints(); // The parent must show behind the child.
        else
            bgColor = Color::white;
    }

    // Paint the color first underneath all images.
    if (!bgLayer->next() && bgColor.isValid() && bgColor.alpha() > 0) {
        IntRect rect(_tx, clipy, w, cliph);
        // If we have an alpha and we are painting the root element, go ahead and blend with white.
        if (bgColor.alpha() < 0xFF && isRoot() && !canvas()->view()->isTransparent())
            p->fillRect(rect, Color(Color::white));
        p->fillRect(rect, bgColor);
    }
    
    // no progressive loading of the background image
    if (shouldPaintBackgroundImage) {
        int sx = 0;
        int sy = 0;
        int cw,ch;
        int cx,cy;
        int scaledImageWidth, scaledImageHeight;

        
        // CSS2 chapter 14.2.1

        if (bgLayer->backgroundAttachment()) {
            // scroll
            int hpab = 0, vpab = 0, left = 0, top = 0; // Init to 0 for background-origin of 'border'
            if (bgLayer->backgroundOrigin() != BGBORDER) {
                hpab += bleft + bright;
                vpab += borderTop() + borderBottom();
                left += bleft;
                top += borderTop();
                if (bgLayer->backgroundOrigin() == BGCONTENT) {
                    hpab += pleft + pright;
                    vpab += paddingTop() + paddingBottom();
                    left += pleft;
                    top += paddingTop();
                }
            }
            
            int pw = w - hpab;
            int ph = h - vpab;
            scaledImageWidth = pw;
            scaledImageHeight = ph;
            cacluateBackgroundSize(bgLayer, scaledImageWidth, scaledImageHeight);

            EBackgroundRepeat bgr = bgLayer->backgroundRepeat();
            if ((bgr == NO_REPEAT || bgr == REPEAT_Y) && w > scaledImageWidth) {
                cw = scaledImageWidth;
                int xPosition = bgLayer->backgroundXPosition().calcMinValue(pw - scaledImageWidth);
                if (xPosition >= 0)
                    cx = _tx + xPosition;
                else {
                    cx = _tx;
                    if (scaledImageWidth > 0) {
                        sx = -xPosition;
                        cw += xPosition;
                    }
                }
                cx += left;
            } else {
                // repeat over x or background is wider than box
                cw = w;
                cx = _tx;
                if (scaledImageWidth > 0) {
                    int xPosition = bgLayer->backgroundXPosition().calcMinValue(pw - scaledImageWidth);
                    if ((xPosition > 0) && (bgr == NO_REPEAT)) {
                        cx += xPosition;
                        cw -= xPosition;
                    } else {
                        sx = scaledImageWidth - (xPosition % scaledImageWidth);
                        sx -= left % scaledImageWidth;
                    }
                }
            }

            if((bgr == NO_REPEAT || bgr == REPEAT_X) && h > scaledImageHeight) {
                ch = scaledImageHeight;
                int yPosition = bgLayer->backgroundYPosition().calcMinValue(ph - scaledImageHeight);
                if (yPosition >= 0)
                    cy = _ty + yPosition;
                else {
                    cy = _ty;
                    if (scaledImageHeight > 0) {
                        sy = -yPosition;
                        ch += yPosition;
                    }
                }
                
                cy += top;
            } else {
                // repeat over y or background is taller than box
                ch = h;
                cy = _ty;
                if (scaledImageHeight > 0) {
                    int yPosition = bgLayer->backgroundYPosition().calcMinValue(ph - scaledImageHeight);
                    if ((yPosition > 0) && (bgr == NO_REPEAT)) {
                        cy += yPosition;
                        ch -= yPosition;
                    } else {
                        sy = scaledImageHeight - (yPosition % scaledImageHeight);
                        sy -= top % scaledImageHeight;
                    }
                }
            }
        }
        else {
            //fixed
            IntRect vr = viewRect();
            int pw = vr.width();
            int ph = vr.height();
            scaledImageWidth = pw;
            scaledImageHeight = ph;
            cacluateBackgroundSize(bgLayer, scaledImageWidth, scaledImageHeight);
            EBackgroundRepeat bgr = bgLayer->backgroundRepeat();
            
            if ((bgr == NO_REPEAT || bgr == REPEAT_Y) && pw > scaledImageWidth) {
                cw = scaledImageWidth;
                cx = vr.x() + bgLayer->backgroundXPosition().calcMinValue(pw - scaledImageWidth);
            } else {
                cw = pw;
                cx = vr.x();
                if (scaledImageWidth > 0)
                    sx = scaledImageWidth - bgLayer->backgroundXPosition().calcMinValue(pw - scaledImageWidth) % scaledImageWidth;
            }

            if ((bgr == NO_REPEAT || bgr == REPEAT_X) && ph > scaledImageHeight) {
                ch = scaledImageHeight;
                cy = vr.y() + bgLayer->backgroundYPosition().calcMinValue(ph - scaledImageHeight);
            } else {
                ch = ph;
                cy = vr.y();
                if (scaledImageHeight > 0)
                    sy = scaledImageHeight - bgLayer->backgroundYPosition().calcMinValue(ph - scaledImageHeight) % scaledImageHeight;
            }

            IntRect b = intersection(IntRect(cx, cy, cw, ch), IntRect(_tx, _ty, w, h));
            sx += b.x() - cx;
            sy += b.y() - cy;
            cx = b.x();
            cy = b.y();
            cw = b.width();
            ch = b.height();
        }

        if (cw > 0 && ch > 0)
            p->drawTiledImage(bg->image(), IntRect(cx, cy, cw, ch), IntPoint(sx, sy), IntSize(scaledImageWidth, scaledImageHeight));
    }
    
    if (bgLayer->backgroundClip() != BGBORDER)
        p->restore(); // Undo the background clip
        
    if (clippedToBorderRadius)
        p->restore(); // Undo the border radius clip
}

void RenderBox::outlineBox(GraphicsContext* p, int _tx, int _ty, const char* color)
{
    p->setPen(Pen(Color(color), 1, Pen::DotLine));
    p->setFillColor(Color::transparent);
    p->drawRect(IntRect(_tx, _ty, m_width, m_height));
}

IntRect RenderBox::getOverflowClipRect(int tx, int ty)
{
    // XXX When overflow-clip (CSS3) is implemented, we'll obtain the property
    // here.
    int bl=borderLeft(),bt=borderTop(),bb=borderBottom(),br=borderRight();
    int clipx = tx + bl;
    int clipy = ty + bt;
    int clipw = m_width - bl - br;
    int cliph = m_height - bt - bb + borderTopExtra() + borderBottomExtra();

    // Subtract out scrollbars if we have them.
    if (m_layer) {
        clipw -= m_layer->verticalScrollbarWidth();
        cliph -= m_layer->horizontalScrollbarHeight();
    }
    return IntRect(clipx,clipy,clipw,cliph);
}

IntRect RenderBox::getClipRect(int tx, int ty)
{
    int clipx = tx;
    int clipy = ty;
    int clipw = m_width;
    int cliph = m_height;

    if (!style()->clipLeft().isAuto()) {
        int c = style()->clipLeft().calcValue(m_width);
        clipx += c;
        clipw -= c;
    }
        
    if (!style()->clipRight().isAuto()) {
        int w = style()->clipRight().calcValue(m_width);
        clipw -= m_width - w;
    }
    
    if (!style()->clipTop().isAuto()) {
        int c = style()->clipTop().calcValue(m_height);
        clipy += c;
        cliph -= c;
    }

    if (!style()->clipBottom().isAuto()) {
        int h = style()->clipBottom().calcValue(m_height);
        cliph -= m_height - h;
    }

    return IntRect(clipx, clipy, clipw, cliph);
}

int RenderBox::containingBlockWidth() const
{
    RenderBlock* cb = containingBlock();
    if (!cb)
        return 0;
    if (usesLineWidth())
        return cb->lineWidth(m_y);
    else
        return cb->contentWidth();
}

bool RenderBox::absolutePosition(int &xPos, int &yPos, bool f)
{
    if (style()->position() == FixedPosition)
        f = true;
    RenderObject *o = container();
    if (o && o->absolutePosition(xPos, yPos, f)) {
        yPos += o->borderTopExtra();
        if (style()->position() == AbsolutePosition && o->isRelPositioned() && o->isInlineFlow()) {
            // When we have an enclosing relpositioned inline, we need to add in the offset of the first line
            // box from the rest of the content, but only in the cases where we know we're positioned
            // relative to the inline itself.
            RenderFlow* flow = static_cast<RenderFlow*>(o);
            int sx = 0;
            int sy = 0;
            if (flow->firstLineBox()) {
                sx = flow->firstLineBox()->xPos();
                sy = flow->firstLineBox()->yPos();
            } else {
                sx = flow->staticX();
                sy = flow->staticY();
            }
            
            bool isInlineType = style()->isOriginalDisplayInlineType();
            
            if (!hasStaticX())
                xPos += sx;
            // This is not terribly intuitive, but we have to match other browsers.  Despite being a block display type inside
            // an inline, we still keep our x locked to the left of the relative positioned inline.  Arguably the correct
            // behavior would be to go flush left to the block that contains the inline, but that isn't what other browsers
            // do.
            if (hasStaticX() && !isInlineType)
                // Avoid adding in the left border/padding of the containing block twice.  Subtract it out.
                xPos += sx - (containingBlock()->borderLeft() + containingBlock()->paddingLeft());
            
            if (!hasStaticY())
                yPos += sy;
        }
        if (o->hasOverflowClip())
            o->layer()->subtractScrollOffset(xPos, yPos); 
            
        if (!isInline() || isReplaced()) {
            xPos += m_x;
            yPos += m_y;
        }

        if (isRelPositioned())
            relativePositionOffset(xPos, yPos);

        return true;
    }
    else {
        xPos = yPos = 0;
        return false;
    }
}

void RenderBox::dirtyLineBoxes(bool fullLayout, bool)
{
    if (m_inlineBoxWrapper) {
        if (fullLayout) {
            m_inlineBoxWrapper->destroy(renderArena());
            m_inlineBoxWrapper = 0;
        }
        else
            m_inlineBoxWrapper->dirtyLineBoxes();
    }
}

void RenderBox::position(InlineBox* box, int from, int len, bool reverse, bool override)
{
    if (isPositioned()) {
        // Cache the x position only if we were an INLINE type originally.
        bool wasInline = style()->isOriginalDisplayInlineType();
        if (wasInline && hasStaticX()) {
            // The value is cached in the xPos of the box.  We only need this value if
            // our object was inline originally, since otherwise it would have ended up underneath
            // the inlines.
            m_staticX = box->xPos();
        }
        else if (!wasInline && hasStaticY())
            // Our object was a block originally, so we make our normal flow position be
            // just below the line box (as though all the inlines that came before us got
            // wrapped in an anonymous block, which is what would have happened had we been
            // in flow).  This value was cached in the yPos() of the box.
            m_staticY = box->yPos();

        // Nuke the box.
        box->remove();
        box->destroy(renderArena());
    }
    else if (isReplaced()) {
        m_x = box->xPos();
        m_y = box->yPos();
        m_inlineBoxWrapper = box;
    }
}

// For inline replaced elements, this function returns the inline box that owns us.  Enables
// the replaced RenderObject to quickly determine what line it is contained on and to easily
// iterate over structures on the line.
InlineBox* RenderBox::inlineBoxWrapper() const
{
    return m_inlineBoxWrapper;
}

void RenderBox::deleteLineBoxWrapper()
{
    if (m_inlineBoxWrapper) {
        if (!documentBeingDestroyed())
            m_inlineBoxWrapper->remove();
        m_inlineBoxWrapper->destroy(renderArena());
        m_inlineBoxWrapper = 0;
    }
}

void RenderBox::setInlineBoxWrapper(InlineBox* b)
{
    m_inlineBoxWrapper = b;
}

IntRect RenderBox::getAbsoluteRepaintRect()
{
    IntRect r = overflowRect(false);
    if (style()) {
        if (style()->hasAppearance())
            // The theme may wish to inflate the rect used when repainting.
            theme()->adjustRepaintRect(this, r);
        r.inflate(style()->outlineSize()); // FIXME: Technically the outline inflation could fit within the theme inflation.
    }
    computeAbsoluteRepaintRect(r);
    return r;
}

void RenderBox::computeAbsoluteRepaintRect(IntRect& r, bool f)
{
    int x = r.x() + m_x;
    int y = r.y() + m_y;
     
    // Apply the relative position offset when invalidating a rectangle.  The layer
    // is translated, but the render box isn't, so we need to do this to get the
    // right dirty rect.  Since this is called from RenderObject::setStyle, the relative position
    // flag on the RenderObject has been cleared, so use the one on the style().
    if (style()->position() == RelativePosition && m_layer)
        m_layer->relativePositionOffset(x,y);
    
    if (style()->position()==FixedPosition)
        f = true;

    RenderObject* o = container();
    if (o) {
        if (style()->position() == AbsolutePosition && o->isRelPositioned() && o->isInlineFlow()) {
            // When we have an enclosing relpositioned inline, we need to add in the offset of the first line
            // box from the rest of the content, but only in the cases where we know we're positioned
            // relative to the inline itself.
            RenderFlow* flow = static_cast<RenderFlow*>(o);
            int sx = 0;
            int sy = 0;
            if (flow->firstLineBox()) {
                sx = flow->firstLineBox()->xPos();
                sy = flow->firstLineBox()->yPos();
            } else {
                sx = flow->staticX();
                sy = flow->staticY();
            }
            
            bool isInlineType = style()->isOriginalDisplayInlineType();
            
            if (!hasStaticX())
                x += sx;
            // This is not terribly intuitive, but we have to match other browsers.  Despite being a block display type inside
            // an inline, we still keep our x locked to the left of the relative positioned inline.  Arguably the correct
            // behavior would be to go flush left to the block that contains the inline, but that isn't what other browsers
            // do.
            if (hasStaticX() && !isInlineType)
                // Avoid adding in the left border/padding of the containing block twice.  Subtract it out.
                x += sx - (containingBlock()->borderLeft() + containingBlock()->paddingLeft());
            
            if (!hasStaticY())
                y += sy;
        }
        // <body> may not have overflow, since it might be applying its overflow value to the
        // scrollbars.
        if (o->hasOverflowClip()) {
            // o->height() is inaccurate if we're in the middle of a layout of |o|, so use the
            // layer's size instead.  Even if the layer's size is wrong, the layer itself will repaint
            // anyway if its size does change.
            IntRect boxRect(0, 0, o->layer()->width(), o->layer()->height());
            o->layer()->subtractScrollOffset(x,y); // For overflow:auto/scroll/hidden.
            IntRect repaintRect(x, y, r.width(), r.height());
            r = intersection(repaintRect, boxRect);
            if (r.isEmpty())
                return;
        } else {
            r.setX(x);
            r.setY(y);
        }
        o->computeAbsoluteRepaintRect(r, f);
    }
}

void RenderBox::repaintDuringLayoutIfMoved(int oldX, int oldY)
{
    int newX = m_x;
    int newY = m_y;
    if (oldX != newX || oldY != newY) {
        // The child moved.  Invalidate the object's old and new positions.  We have to do this
        // since the object may not have gotten a layout.
        m_x = oldX; m_y = oldY;
        repaint();
        repaintFloatingDescendants();
        m_x = newX; m_y = newY;
        repaint();
        repaintFloatingDescendants();
    }
}

void RenderBox::relativePositionOffset(int &tx, int &ty)
{
    if(!style()->left().isAuto())
        tx += style()->left().calcValue(containingBlockWidth());
    else if(!style()->right().isAuto())
        tx -= style()->right().calcValue(containingBlockWidth());
    if(!style()->top().isAuto())
    {
        if (!style()->top().isPercent()
                || containingBlock()->style()->height().isFixed())
            ty += style()->top().calcValue(containingBlockHeight());
    }
    else if(!style()->bottom().isAuto())
    {
        if (!style()->bottom().isPercent()
                || containingBlock()->style()->height().isFixed())
            ty -= style()->bottom().calcValue(containingBlockHeight());
    }
}

void RenderBox::calcWidth()
{
    if (isPositioned())
        calcAbsoluteHorizontal();
    else
    {
        // The parent box is flexing us, so it has increased or decreased our width.  Use the width
        // from the style context.
        if (m_overrideSize != -1 && parent()->isFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL
            && parent()->isFlexingChildren()) {
            m_width = m_overrideSize;
            return;
        }

        bool inVerticalBox = parent()->isFlexibleBox() && parent()->style()->boxOrient() == VERTICAL;
        bool stretching = parent()->style()->boxAlign() == BSTRETCH;
        bool treatAsReplaced = isReplaced() && !isInlineBlockOrInlineTable() &&
            (!inVerticalBox || !stretching);
        Length w;
        if (treatAsReplaced)
            w = Length( calcReplacedWidth(), Fixed );
        else
            w = style()->width();

        Length ml = style()->marginLeft();
        Length mr = style()->marginRight();

        RenderBlock *cb = containingBlock();
        int cw = containingBlockWidth();

        if (cw<0) cw = 0;

        m_marginLeft = 0;
        m_marginRight = 0;

        if (isInline() && !isInlineBlockOrInlineTable()) {
            // just calculate margins
            m_marginLeft = ml.calcMinValue(cw);
            m_marginRight = mr.calcMinValue(cw);
            if (treatAsReplaced) {
                m_width = w.calcValue(cw) + borderLeft() + borderRight() + paddingLeft() + paddingRight();
                m_width = max(m_width, m_minWidth);
            }
            return;
        }
        else {
            LengthType widthType, minWidthType, maxWidthType;
            if (treatAsReplaced) {
                m_width = w.calcValue(cw) + borderLeft() + borderRight() + paddingLeft() + paddingRight();
                widthType = w.type();
            } else {
                m_width = calcWidthUsing(Width, cw, widthType);
                int minW = calcWidthUsing(MinWidth, cw, minWidthType);
                int maxW = style()->maxWidth().value() == undefinedLength
                    ? m_width : calcWidthUsing(MaxWidth, cw, maxWidthType);
                
                if (m_width > maxW) {
                    m_width = maxW;
                    widthType = maxWidthType;
                }
                if (m_width < minW) {
                    m_width = minW;
                    widthType = minWidthType;
                }
            }
            
            if (widthType == Auto) {
                m_marginLeft = ml.calcMinValue(cw);
                m_marginRight = mr.calcMinValue(cw);
            }
            else
            {
                calcHorizontalMargins(ml,mr,cw);
            }
        }

        if (cw && cw != m_width + m_marginLeft + m_marginRight && !isFloating() && !isInline() &&
            !cb->isFlexibleBox())
        {
            if (cb->style()->direction()==LTR)
                m_marginRight = cw - m_width - m_marginLeft;
            else
                m_marginLeft = cw - m_width - m_marginRight;
        }
    }
}

int RenderBox::calcWidthUsing(WidthType widthType, int cw, LengthType& lengthType)
{
    int width = m_width;
    Length w;
    if (widthType == Width)
        w = style()->width();
    else if (widthType == MinWidth)
        w = style()->minWidth();
    else
        w = style()->maxWidth();

    lengthType = w.type();

    if (w.isIntrinsicOrAuto()) {
        int marginLeft = style()->marginLeft().calcMinValue(cw);
        int marginRight = style()->marginRight().calcMinValue(cw);
        if (cw) width = cw - marginLeft - marginRight;
        
        if (sizesToIntrinsicWidth(widthType)) {
            width = max(width, m_minWidth);
            width = min(width, m_maxWidth);
        }
    }
    else
        width = calcBorderBoxWidth(w.calcValue(cw));
    
    return width;
}

bool RenderBox::sizesToIntrinsicWidth(WidthType widthType) const
{
    // Marquees in WinIE are like a mixture of blocks and inline-blocks.  They size as though they're blocks,
    // but they allow text to sit on the same line as the marquee.
    if (isFloating() || (isCompact() && isInline()) || 
        (isInlineBlockOrInlineTable() && !isHTMLMarquee()))
        return true;
    
    // This code may look a bit strange.  Basically width:intrinsic should clamp the size when testing both
    // min-width and width.  max-width is only clamped if it is also intrinsic.
    Length width = widthType == MaxWidth ? style()->maxWidth() : style()->width();
    if (width.type() == Intrinsic)
        return true;
    
    // Children of a horizontal marquee do not fill the container by default.
    // FIXME: Need to deal with MAUTO value properly.  It could be vertical.
    if (parent()->style()->overflow() == OMARQUEE) {
        EMarqueeDirection dir = parent()->style()->marqueeDirection();
        if (dir == MAUTO || dir == MFORWARD || dir == MBACKWARD || dir == MLEFT || dir == MRIGHT)
            return true;
    }
    
    // Flexible horizontal boxes lay out children at their intrinsic widths.  Also vertical boxes
    // that don't stretch their kids lay out their children at their intrinsic widths.
    if (parent()->isFlexibleBox() &&
        (parent()->style()->boxOrient() == HORIZONTAL || parent()->style()->boxAlign() != BSTRETCH))
        return true;

    return false;
}

void RenderBox::calcHorizontalMargins(const Length& ml, const Length& mr, int cw)
{
    if (isFloating() || isInline()) // Inline blocks/tables and floats don't have their margins increased.
    {
        m_marginLeft = ml.calcMinValue(cw);
        m_marginRight = mr.calcMinValue(cw);
    }
    else
    {
        if ( (ml.isAuto() && mr.isAuto() && m_width<cw) ||
             (!ml.isAuto() && !mr.isAuto() &&
                containingBlock()->style()->textAlign() == KHTML_CENTER) )
        {
            m_marginLeft = (cw - m_width)/2;
            if (m_marginLeft<0) m_marginLeft=0;
            m_marginRight = cw - m_width - m_marginLeft;
        }
        else if ( (mr.isAuto() && m_width<cw) ||
                 (!ml.isAuto() && containingBlock()->style()->direction() == RTL &&
                  containingBlock()->style()->textAlign() == KHTML_LEFT))
        {
            m_marginLeft = ml.calcValue(cw);
            m_marginRight = cw - m_width - m_marginLeft;
        }
        else if ( (ml.isAuto() && m_width<cw) ||
                 (!mr.isAuto() && containingBlock()->style()->direction() == LTR &&
                  containingBlock()->style()->textAlign() == KHTML_RIGHT))
        {
            m_marginRight = mr.calcValue(cw);
            m_marginLeft = cw - m_width - m_marginRight;
        }
        else
        {
            // this makes auto margins 0 if we failed a m_width<cw test above (css2.1, 10.3.3)
            m_marginLeft = ml.calcMinValue(cw);
            m_marginRight = mr.calcMinValue(cw);
        }
    }
}

void RenderBox::calcHeight()
{
    // Cell height is managed by the table and inline non-replaced elements do not support a height property.
    if (isTableCell() || (isInline() && !isReplaced()))
        return;

    if (isPositioned())
        calcAbsoluteVertical();
    else
    {
        calcVerticalMargins();
        
        // For tables, calculate margins only
        if (isTable())
            return;
        
        Length h;
        bool inHorizontalBox = parent()->isFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL;
        bool stretching = parent()->style()->boxAlign() == BSTRETCH;
        bool treatAsReplaced = isReplaced() && !isInlineBlockOrInlineTable() && (!inHorizontalBox || !stretching);
        bool checkMinMaxHeight = false;
        
        // The parent box is flexing us, so it has increased or decreased our height.  We have to
        // grab our cached flexible height.
        if (m_overrideSize != -1 && parent()->isFlexibleBox() && parent()->style()->boxOrient() == VERTICAL
            && parent()->isFlexingChildren())
            h = Length(m_overrideSize - borderTop() - borderBottom() - paddingTop() - paddingBottom(), Fixed);
        else if (treatAsReplaced)
            h = Length(calcReplacedHeight(), Fixed);
        else {
            h = style()->height();
            checkMinMaxHeight = true;
        }
        
        // Block children of horizontal flexible boxes fill the height of the box.
        if (h.isAuto() && parent()->isFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL
            && parent()->isStretchingChildren()) {
            h = Length(parent()->contentHeight() - marginTop() - marginBottom() -
                       borderTop() - paddingTop() - borderBottom() - paddingBottom(), Fixed);
            checkMinMaxHeight = false;
        }

        int height;
        if (checkMinMaxHeight) {
            height = calcHeightUsing(style()->height());
            if (height == -1)
                height = m_height;
            int minH = calcHeightUsing(style()->minHeight()); // Leave as -1 if unset.
            int maxH = style()->maxHeight().value() == undefinedLength ? height : calcHeightUsing(style()->maxHeight());
            if (maxH == -1)
                maxH = height;
            height = min(maxH, height);
            height = max(minH, height);
        }
        else
            // The only times we don't check min/max height are when a fixed length has 
            // been given as an override.  Just use that.  The value has already been adjusted
            // for box-sizing.
            height = h.value() + borderTop() + borderBottom() + paddingTop() + paddingBottom();

        m_height = height;
    }
    
    // Unfurling marquees override with the furled height.
    if (style()->overflow() == OMARQUEE && m_layer && m_layer->marquee() && 
        m_layer->marquee()->isUnfurlMarquee() && !m_layer->marquee()->isHorizontal()) {
        m_layer->marquee()->setEnd(m_height);
        m_height = min(m_height, m_layer->marquee()->unfurlPos());
    }
    
    // WinIE quirk: The <html> block always fills the entire canvas in quirks mode.  The <body> always fills the
    // <html> block in quirks mode.  Only apply this quirk if the block is normal flow and no height
    // is specified.
    if (style()->htmlHacks() && style()->height().isAuto() &&
        !isFloatingOrPositioned() && (isRoot() || isBody())) {
        int margins = collapsedMarginTop() + collapsedMarginBottom();
        int visHeight = canvas()->view()->visibleHeight();
        if (isRoot())
            m_height = max(m_height, visHeight - margins);
        else
            m_height = max(m_height, visHeight - 
                            (margins + parent()->marginTop() + parent()->marginBottom() + 
                             parent()->borderTop() + parent()->borderBottom() +
                             parent()->paddingTop() + parent()->paddingBottom()));
    }
}

int RenderBox::calcHeightUsing(const Length& h)
{
    int height = -1;
    if (!h.isAuto()) {
        if (h.isFixed())
            height = h.value();
        else if (h.isPercent())
            height = calcPercentageHeight(h);
        if (height != -1) {
            height = calcBorderBoxHeight(height);
            return height;
        }
    }
    return height;
}

int RenderBox::calcPercentageHeight(const Length& height)
{
    int result = -1;
    bool includeBorderPadding = isTable();
    RenderBlock* cb = containingBlock();
    if (style()->htmlHacks()) {
        // In quirks mode, blocks with auto height are skipped, and we keep looking for an enclosing
        // block that may have a specified height and then use it.  In strict mode, this violates the
        // specification, which states that percentage heights just revert to auto if the containing
        // block has an auto height.
        for ( ; !cb->isCanvas() && !cb->isBody() && !cb->isTableCell() && !cb->isPositioned() &&
                cb->style()->height().isAuto(); cb = cb->containingBlock());
    }

    // Table cells violate what the CSS spec says to do with heights.  Basically we
    // don't care if the cell specified a height or not.  We just always make ourselves
    // be a percentage of the cell's current content height.
    if (cb->isTableCell()) {
        result = cb->overrideSize();
        if (result == -1) {
            // Normally we would let the cell size intrinsically, but scrolling overflow has to be
            // treated differently, since WinIE lets scrolled overflow regions shrink as needed.
            // While we can't get all cases right, we can at least detect when the cell has a specified
            // height or when the table has a specified height.  In these cases we want to initially have
            // no size and allow the flexing of the table or the cell to its specified height to cause us
            // to grow to fill the space.  This could end up being wrong in some cases, but it is
            // preferable to the alternative (sizing intrinsically and making the row end up too big).
            RenderTableCell* cell = static_cast<RenderTableCell*>(cb);
            if (scrollsOverflow() && 
                (!cell->style()->height().isAuto() || !cell->table()->style()->height().isAuto()))
                return 0;
            return -1;
        }
        includeBorderPadding = true;
    }

    // Otherwise we only use our percentage height if our containing block had a specified
    // height.
    else if (cb->style()->height().isFixed())
        result = cb->calcContentBoxHeight(cb->style()->height().value());
    else if (cb->style()->height().isPercent()) {
        // We need to recur and compute the percentage height for our containing block.
        result = cb->calcPercentageHeight(cb->style()->height());
        if (result != -1)
            result = cb->calcContentBoxHeight(result);
    }
    else if (cb->isCanvas() || (cb->isBody() && style()->htmlHacks())) {
        // Don't allow this to affect the block' m_height member variable, since this
        // can get called while the block is still laying out its kids.
        int oldHeight = cb->height();
        cb->calcHeight();
        result = cb->contentHeight();
        cb->setHeight(oldHeight);
    } else if (cb->isRoot() && isPositioned()) {
        // Match the positioned objects behavior, which is that positioned objects will fill their viewport
        // always.  Note we could only hit this case by recurring into calcPercentageHeight on a positioned containing block.
        result = cb->calcContentBoxHeight(cb->availableHeight());
    }

    if (result != -1) {
        result = height.calcValue(result);
        if (includeBorderPadding) {
            // It is necessary to use the border-box to match WinIE's broken
            // box model.  This is essential for sizing inside
            // table cells using percentage heights.
            result -= (borderTop() + paddingTop() + borderBottom() + paddingBottom());
            result = max(0, result);
        }
    }
    return result;
}

int RenderBox::calcReplacedWidth() const
{
    int width = calcReplacedWidthUsing(Width);
    int minW = calcReplacedWidthUsing(MinWidth);
    int maxW = style()->maxWidth().value() == undefinedLength ? width : calcReplacedWidthUsing(MaxWidth);

    return max(minW, min(width, maxW));
}

int RenderBox::calcReplacedWidthUsing(WidthType widthType) const
{
    Length w;
    if (widthType == Width)
        w = style()->width();
    else if (widthType == MinWidth)
        w = style()->minWidth();
    else
        w = style()->maxWidth();
    
    switch (w.type()) {
    case Fixed:
        return calcContentBoxWidth(w.value());
    case Percent: {
        const int cw = containingBlockWidth();
        if (cw > 0)
            return calcContentBoxWidth(w.calcMinValue(cw));
    }
    // fall through
    default:
        return intrinsicWidth();
    }
}

int RenderBox::calcReplacedHeight() const
{
    int height = calcReplacedHeightUsing(Height);
    int minH = calcReplacedHeightUsing(MinHeight);
    int maxH = style()->maxHeight().value() == undefinedLength ? height : calcReplacedHeightUsing(MaxHeight);

    return max(minH, min(height, maxH));
}

int RenderBox::calcReplacedHeightUsing(HeightType heightType) const
{
    Length h;
    if (heightType == Height)
        h = style()->height();
    else if (heightType == MinHeight)
        h = style()->minHeight();
    else
        h = style()->maxHeight();

    switch (h.type()) {
        case Fixed:
            return calcContentBoxHeight(h.value());
        case Percent:
            return calcContentBoxHeight(h.calcValue(containingBlock()->availableHeight()));
        default:
            return intrinsicHeight();
    }
}

int RenderBox::availableHeight() const
{
    return availableHeightUsing(style()->height());
}

int RenderBox::availableHeightUsing(const Length& h) const
{
    if (h.isFixed())
        return calcContentBoxHeight(h.value());

    if (isCanvas())
        return static_cast<const RenderCanvas*>(this)->view()->visibleHeight();

    // We need to stop here, since we don't want to increase the height of the table
    // artificially.  We're going to rely on this cell getting expanded to some new
    // height, and then when we lay out again we'll use the calculation below.
    if (isTableCell() && (h.isAuto() || h.isPercent()))
        return overrideSize() - (borderLeft() + borderRight() + paddingLeft() + paddingRight());
    
    if (h.isPercent())
       return calcContentBoxHeight(h.calcValue(containingBlock()->availableHeight()));
       
    return containingBlock()->availableHeight();
}

void RenderBox::calcVerticalMargins()
{
    if( isTableCell() ) {
        // table margins are basically infinite
        m_marginTop = TABLECELLMARGIN;
        m_marginBottom = TABLECELLMARGIN;
        return;
    }

    Length tm = style()->marginTop();
    Length bm = style()->marginBottom();

    // margins are calculated with respect to the _width_ of
    // the containing block (8.3)
    int cw = containingBlock()->contentWidth();

    m_marginTop = tm.calcMinValue(cw);
    m_marginBottom = bm.calcMinValue(cw);
}

void RenderBox::setStaticX(int staticX)
{
    m_staticX = staticX;
}

void RenderBox::setStaticY(int staticY)
{
    m_staticY = staticY;
}

void RenderBox::calcAbsoluteHorizontal()
{
    if (isReplaced()) {
        calcAbsoluteHorizontalReplaced();
        return; 
    }


    // QUESTIONS
    // FIXME 1: Which RenderObject's 'direction' property should used: the
    // containing block (cb) as the spec seems to imply, the parent (parent()) as
    // was previously done in calculating the static distances, or ourself, which
    // was also previously done for deciding what to override when you had
    // over-constrained margins?  Also note that the container block is used
    // in similar situations in other parts of the RenderBox class (see calcWidth()
    // and calcHorizontalMargins()). For now we are using the parent for quirks
    // mode and the containing block for strict mode.

    // FIXME 2: Should we still deal with these the cases of 'left' or 'right' having
    // the type 'static' in determining whether to calculate the static distance?
    // NOTE: 'static' is not a legal value for 'left' or 'right' as of CSS 2.1.

    // FIXME 3: Can perhaps optimize out cases when max-width/min-width are greater
    // than or less than the computed m_width.  Be careful of box-sizing and
    // percentage issues.

    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.3.7 "Absolutely positioned, non-replaced elements"
    // <http://www.w3.org/TR/CSS21/visudet.html#abs-non-replaced-width>
    // (block-style-comments in this function and in calcAbsoluteHorizontalValues()
    // correspond to text from the spec)


    // We don't use containingBlock(), since we may be positioned by an enclosing
    // relative positioned inline.
    const RenderObject* containerBlock = container();
    
    // FIXME: This is incorrect for cases where the container block is a relatively
    // positioned inline.
    const int containerWidth = containingBlockWidth() + containerBlock->paddingLeft() + containerBlock->paddingRight();

    // To match WinIE, in quirks mode use the parent's 'direction' property
    // instead of the the container block's.
    TextDirection containerDirection = (style()->htmlHacks()) ? parent()->style()->direction() : containerBlock->style()->direction();

    const int bordersPlusPadding = borderLeft() + borderRight() + paddingLeft() + paddingRight();
    const Length marginLeft = style()->marginLeft();
    const Length marginRight = style()->marginRight();
    Length left = style()->left();
    Length right = style()->right();

    /*---------------------------------------------------------------------------*\
     * For the purposes of this section and the next, the term "static position"
     * (of an element) refers, roughly, to the position an element would have had
     * in the normal flow. More precisely:
     *
     * * The static position for 'left' is the distance from the left edge of the
     *   containing block to the left margin edge of a hypothetical box that would
     *   have been the first box of the element if its 'position' property had
     *   been 'static' and 'float' had been 'none'. The value is negative if the
     *   hypothetical box is to the left of the containing block.
     * * The static position for 'right' is the distance from the right edge of the
     *   containing block to the right margin edge of the same hypothetical box as
     *   above. The value is positive if the hypothetical box is to the left of the
     *   containing block's edge.
     *
     * But rather than actually calculating the dimensions of that hypothetical box,
     * user agents are free to make a guess at its probable position.
     *
     * For the purposes of calculating the static position, the containing block of
     * fixed positioned elements is the initial containing block instead of the
     * viewport, and all scrollable boxes should be assumed to be scrolled to their
     * origin.
    \*---------------------------------------------------------------------------*/

    // see FIXME 2
    // Calculate the static distance if needed.
    if (left.isAuto() && right.isAuto()) {
        if (containerDirection == LTR) {
            // 'm_staticX' should already have been set through layout of the parent.
            int staticPosition = m_staticX - containerBlock->borderLeft();
            for (RenderObject* po = parent(); po && po != containerBlock; po = po->parent())
                staticPosition += po->xPos();
            left.setValue(Fixed, staticPosition);
        } else {
            RenderObject* po = parent();
            // 'm_staticX' should already have been set through layout of the parent.
            int staticPosition = m_staticX + containerWidth + containerBlock->borderRight() - po->width();
            for (; po && po != containerBlock; po = po->parent())
                staticPosition -= po->xPos();
            right.setValue(Fixed, staticPosition);
        }
    }

    // Calculate constraint equation values for 'width' case.
    calcAbsoluteHorizontalValues(style()->width(), containerBlock, containerDirection,
                                 containerWidth, bordersPlusPadding,
                                 left, right, marginLeft, marginRight,
                                 m_width, m_marginLeft, m_marginRight, m_x);

    // Calculate constraint equation values for 'max-width' case.calcContentBoxWidth(width.calcValue(containerWidth));
    if (style()->maxWidth().value() != undefinedLength) {
        int maxWidth;
        int maxMarginLeft;
        int maxMarginRight;
        int maxXPos;

        calcAbsoluteHorizontalValues(style()->maxWidth(), containerBlock, containerDirection,
                                     containerWidth, bordersPlusPadding,
                                     left, right, marginLeft, marginRight,
                                     maxWidth, maxMarginLeft, maxMarginRight, maxXPos);

        if (m_width > maxWidth) {
            m_width = maxWidth;
            m_marginLeft = maxMarginLeft;
            m_marginRight = maxMarginRight;
            m_x = maxXPos;
        }
    }

    // Calculate constraint equation values for 'min-width' case.
    if (style()->minWidth().value()) {
        int minWidth;
        int minMarginLeft;
        int minMarginRight;
        int minXPos;

        calcAbsoluteHorizontalValues(style()->minWidth(), containerBlock, containerDirection, 
                                     containerWidth, bordersPlusPadding,
                                     left, right, marginLeft, marginRight,
                                     minWidth, minMarginLeft, minMarginRight, minXPos);

        if (m_width < minWidth) {
            m_width = minWidth;
            m_marginLeft = minMarginLeft;
            m_marginRight = minMarginRight;
            m_x = minXPos;
        }
    }

    // Put m_width into correct form.
    m_width += bordersPlusPadding;
}

void RenderBox::calcAbsoluteHorizontalValues(Length width, const RenderObject* containerBlock, TextDirection containerDirection,
                                             const int containerWidth, const int bordersPlusPadding,
                                             const Length left, const Length right, const Length marginLeft, const Length marginRight,
                                             int& widthValue, int& marginLeftValue, int& marginRightValue, int& xPos)
{
    // 'left' and 'right' cannot both be 'auto' because one would of been
    // converted to the static postion already
    ASSERT(!(left.isAuto() && right.isAuto()));

    int leftValue = 0;

    bool widthIsAuto = width.isIntrinsicOrAuto();
    bool leftIsAuto = left.isAuto();
    bool rightIsAuto = right.isAuto();

    if (!leftIsAuto && !widthIsAuto && !rightIsAuto) {
        /*-----------------------------------------------------------------------*\
         * If none of the three is 'auto': If both 'margin-left' and 'margin-
         * right' are 'auto', solve the equation under the extra constraint that
         * the two margins get equal values, unless this would make them negative,
         * in which case when direction of the containing block is 'ltr' ('rtl'),
         * set 'margin-left' ('margin-right') to zero and solve for 'margin-right'
         * ('margin-left'). If one of 'margin-left' or 'margin-right' is 'auto',
         * solve the equation for that value. If the values are over-constrained,
         * ignore the value for 'left' (in case the 'direction' property of the
         * containing block is 'rtl') or 'right' (in case 'direction' is 'ltr')
         * and solve for that value.
        \*-----------------------------------------------------------------------*/
        // NOTE:  It is not necessary to solve for 'right' in the over constrained
        // case because the value is not used for any further calculations.

        leftValue = left.calcValue(containerWidth);
        widthValue = calcContentBoxWidth(width.calcValue(containerWidth));

        const int availableSpace = containerWidth - (leftValue + widthValue + right.calcValue(containerWidth) + bordersPlusPadding);

        // Margins are now the only unknown
        if (marginLeft.isAuto() && marginRight.isAuto()) {
            // Both margins auto, solve for equality
            if (availableSpace >= 0) {
                marginLeftValue = availableSpace / 2; // split the diference
                marginRightValue = availableSpace - marginLeftValue;  // account for odd valued differences
            } else {
                // see FIXME 1
                if (containerDirection == LTR) {
                    marginLeftValue = 0;
                    marginRightValue = availableSpace; // will be negative
                } else {
                    marginLeftValue = availableSpace; // will be negative
                    marginRightValue = 0;
                }
            }
        } else if (marginLeft.isAuto()) {
            // Solve for left margin
            marginRightValue = marginRight.calcValue(containerWidth);
            marginLeftValue = availableSpace - marginRightValue;
        } else if (marginRight.isAuto()) {
            // Solve for right margin
            marginLeftValue = marginLeft.calcValue(containerWidth);
            marginRightValue = availableSpace - marginLeftValue;
        } else {
            // Over-constrained, solve for left if direction is RTL
            marginLeftValue = marginLeft.calcValue(containerWidth);
            marginRightValue = marginRight.calcValue(containerWidth);

            // see FIXME 1 -- used to be "this->style()->direction()"
            if (containerDirection == RTL)
                leftValue = (availableSpace + leftValue) - marginLeftValue - marginRightValue;
        }
    } else {
        /*--------------------------------------------------------------------*\
         * Otherwise, set 'auto' values for 'margin-left' and 'margin-right'
         * to 0, and pick the one of the following six rules that applies.
         *
         * 1. 'left' and 'width' are 'auto' and 'right' is not 'auto', then the
         *    width is shrink-to-fit. Then solve for 'left'
         *
         *              OMIT RULE 2 AS IT SHOULD NEVER BE HIT
         * ------------------------------------------------------------------
         * 2. 'left' and 'right' are 'auto' and 'width' is not 'auto', then if
         *    the 'direction' property of the containing block is 'ltr' set
         *    'left' to the static position, otherwise set 'right' to the
         *    static position. Then solve for 'left' (if 'direction is 'rtl')
         *    or 'right' (if 'direction' is 'ltr').
         * ------------------------------------------------------------------
         *
         * 3. 'width' and 'right' are 'auto' and 'left' is not 'auto', then the
         *    width is shrink-to-fit . Then solve for 'right'
         * 4. 'left' is 'auto', 'width' and 'right' are not 'auto', then solve
         *    for 'left'
         * 5. 'width' is 'auto', 'left' and 'right' are not 'auto', then solve
         *    for 'width'
         * 6. 'right' is 'auto', 'left' and 'width' are not 'auto', then solve
         *    for 'right'
         *
         * Calculation of the shrink-to-fit width is similar to calculating the
         * width of a table cell using the automatic table layout algorithm.
         * Roughly: calculate the preferred width by formatting the content
         * without breaking lines other than where explicit line breaks occur,
         * and also calculate the preferred minimum width, e.g., by trying all
         * possible line breaks. CSS 2.1 does not define the exact algorithm.
         * Thirdly, calculate the available width: this is found by solving
         * for 'width' after setting 'left' (in case 1) or 'right' (in case 3)
         * to 0.
         *
         * Then the shrink-to-fit width is:
         * min(max(preferred minimum width, available width), preferred width).
        \*--------------------------------------------------------------------*/
        // NOTE: For rules 3 and 6 it is not necessary to solve for 'right'
        // because the value is not used for any further calculations.

        // Calculate margins, 'auto' margins are ignored.
        marginLeftValue = marginLeft.calcMinValue(containerWidth);
        marginRightValue = marginRight.calcMinValue(containerWidth);

        const int availableSpace = containerWidth - (marginLeftValue + marginRightValue + bordersPlusPadding);

        // FIXME: Is there a faster way to find the correct case?
        // Use rule/case that applies.
        if (leftIsAuto && widthIsAuto && !rightIsAuto) {
            // RULE 1: (use shrink-to-fit for width, and solve of left)
            int rightValue = right.calcValue(containerWidth);

            // FIXME: would it be better to have shrink-to-fit in one step?
            int preferredWidth = m_maxWidth - bordersPlusPadding;
            int preferredMinWidth = m_minWidth - bordersPlusPadding;
            int availableWidth = availableSpace - rightValue;
            widthValue = min(max(preferredMinWidth, availableWidth), preferredWidth);
            leftValue = availableSpace - (widthValue + rightValue);
        } else if (!leftIsAuto && widthIsAuto && rightIsAuto) {
            // RULE 3: (use shrink-to-fit for width, and no need solve of right)
            leftValue = left.calcValue(containerWidth);

            // FIXME: would it be better to have shrink-to-fit in one step?
            int preferredWidth = m_maxWidth - bordersPlusPadding;
            int preferredMinWidth = m_minWidth - bordersPlusPadding;
            int availableWidth = availableSpace - leftValue;
            widthValue = min(max(preferredMinWidth, availableWidth), preferredWidth);
        } else if (leftIsAuto && !width.isAuto() && !rightIsAuto) {
            // RULE 4: (solve for left)
            widthValue = calcContentBoxWidth(width.calcValue(containerWidth));
            leftValue = availableSpace - (widthValue + right.calcValue(containerWidth));
        } else if (!leftIsAuto && widthIsAuto && !rightIsAuto) {
            // RULE 5: (solve for width)
            leftValue = left.calcValue(containerWidth);
            widthValue = availableSpace - (leftValue + right.calcValue(containerWidth));
        } else if (!leftIsAuto&& !widthIsAuto && rightIsAuto) {
            // RULE 6: (no need solve for right)
            leftValue = left.calcValue(containerWidth);
            widthValue = calcContentBoxWidth(width.calcValue(containerWidth));
        }
    }

    // Use computed values to calculate the horizontal position.
    xPos = leftValue + marginLeftValue + containerBlock->borderLeft();
}

void RenderBox::calcAbsoluteVertical()
{
    if (isReplaced()) {
        calcAbsoluteVerticalReplaced();
        return;
    }

    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.6.4 "Absolutely positioned, non-replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-non-replaced-height>
    // (block-style-comments in this function and in calcAbsoluteVerticalValues()
    // correspond to text from the spec)


    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    const RenderObject* containerBlock = container();

    // Even in strict mode (where we don't grow the root to fill the viewport) other browsers
    // position as though the root fills the viewport.
    const int containerHeight = containerBlock->isRoot() ? containerBlock->availableHeight() : (containerBlock->height() - containerBlock->borderTop() - containerBlock->borderBottom());
    
    const int bordersPlusPadding = borderTop() + borderBottom() + paddingTop() + paddingBottom();
    const Length marginTop = style()->marginTop();
    const Length marginBottom = style()->marginBottom();
    Length top = style()->top();
    Length bottom = style()->bottom();

    /*---------------------------------------------------------------------------*\
     * For the purposes of this section and the next, the term "static position"
     * (of an element) refers, roughly, to the position an element would have had
     * in the normal flow. More precisely, the static position for 'top' is the
     * distance from the top edge of the containing block to the top margin edge
     * of a hypothetical box that would have been the first box of the element if
     * its 'position' property had been 'static' and 'float' had been 'none'. The
     * value is negative if the hypothetical box is above the containing block.
     *
     * But rather than actually calculating the dimensions of that hypothetical
     * box, user agents are free to make a guess at its probable position.
     *
     * For the purposes of calculating the static position, the containing block
     * of fixed positioned elements is the initial containing block instead of
     * the viewport.
    \*---------------------------------------------------------------------------*/

    // see FIXME 2
    // Calculate the static distance if needed.
    if (top.isAuto() && bottom.isAuto()) {
        // m_staticY should already have been set through layout of the parent()
        int staticTop = m_staticY - containerBlock->borderTop();
        for (RenderObject* po = parent(); po && po != containerBlock; po = po->parent()) {
            if (!po->isTableRow())
                staticTop += po->yPos();
        }
        top.setValue(Fixed, staticTop);
    }


    int height; // Needed to compute overflow.

    // Calculate constraint equation values for 'height' case.
    calcAbsoluteVerticalValues(style()->height(), containerBlock, containerHeight, bordersPlusPadding,
                               top, bottom, marginTop, marginBottom,
                               height, m_marginTop, m_marginBottom, m_y);

    // Avoid doing any work in the common case (where the values of min-height and max-height are their defaults).    
    // see FIXME 3

    // Calculate constraint equation values for 'max-height' case.
    if (style()->maxHeight().value() != undefinedLength) {
        int maxHeight;
        int maxMarginTop;
        int maxMarginBottom;
        int maxYPos;

        calcAbsoluteVerticalValues(style()->maxHeight(), containerBlock, containerHeight, bordersPlusPadding,
                                   top, bottom, marginTop, marginBottom,
                                   maxHeight, maxMarginTop, maxMarginBottom, maxYPos);

        if (height > maxHeight) {
            height = maxHeight;
            m_marginTop = maxMarginTop;
            m_marginBottom = maxMarginBottom;
            m_y = maxYPos;
        }
    }

    // Calculate constraint equation values for 'min-height' case.
    if (style()->minHeight().value()) {
        int minHeight;
        int minMarginTop;
        int minMarginBottom;
        int minYPos;

        calcAbsoluteVerticalValues(style()->minHeight(), containerBlock, containerHeight, bordersPlusPadding,
                                   top, bottom, marginTop, marginBottom,
                                   minHeight, minMarginTop, minMarginBottom, minYPos);

        if (height < minHeight) {
            height = minHeight;
            m_marginTop = minMarginTop;
            m_marginBottom = minMarginBottom;
            m_y = minYPos;
        }
    }

    height += bordersPlusPadding;

    // If our natural/content height exceeds the new height once we've set it, then we
    // need to make sure to update overflow to track the spillout.
    if (m_height > height)
        setOverflowHeight(m_height);

    // Set final height value.
    m_height = height;
}

void RenderBox::calcAbsoluteVerticalValues(Length height, const RenderObject* containerBlock,
                                           const int containerHeight, const int bordersPlusPadding,
                                           const Length top, const Length bottom, const Length marginTop, const Length marginBottom,
                                           int& heightValue, int& marginTopValue, int& marginBottomValue, int& yPos)
{
    // 'top' and 'bottom' cannot both be 'auto' because 'top would of been
    // converted to the static position in calcAbsoluteVertical()
    ASSERT(!(top.isAuto() && bottom.isAuto()));

    int contentHeight = m_height - bordersPlusPadding;    

    int topValue = 0;

    bool heightIsAuto = height.isAuto();
    bool topIsAuto = top.isAuto();
    bool bottomIsAuto = bottom.isAuto();

    // Height is never unsolved for tables.
    if (isTable() && heightIsAuto) {
        height.setValue(Fixed, contentHeight);
        heightIsAuto = false;
    }

    if (!topIsAuto && !heightIsAuto && !bottomIsAuto) {
        /*-----------------------------------------------------------------------*\
         * If none of the three are 'auto': If both 'margin-top' and 'margin-
         * bottom' are 'auto', solve the equation under the extra constraint that
         * the two margins get equal values. If one of 'margin-top' or 'margin-
         * bottom' is 'auto', solve the equation for that value. If the values
         * are over-constrained, ignore the value for 'bottom' and solve for that
         * value.
        \*-----------------------------------------------------------------------*/
        // NOTE:  It is not necessary to solve for 'bottom' in the over constrained
        // case because the value is not used for any further calculations.

        heightValue = calcContentBoxHeight(height.calcValue(containerHeight));
        topValue = top.calcValue(containerHeight);

        const int availableSpace = containerHeight - (topValue + heightValue + bottom.calcValue(containerHeight) + bordersPlusPadding);

        // Margins are now the only unknown
        if (marginTop.isAuto() && marginBottom.isAuto()) {
            // Both margins auto, solve for equality
            // NOTE: This may result in negative values.
            marginTopValue = availableSpace / 2; // split the diference
            marginBottomValue = availableSpace - marginTopValue; // account for odd valued differences
        } else if (marginTop.isAuto()) {
            // Solve for top margin
            marginBottomValue = marginBottom.calcValue(containerHeight);
            marginTopValue = availableSpace - marginBottomValue;
        } else if (marginBottom.isAuto()) {
            // Solve for bottom margin
            marginTopValue = marginTop.calcValue(containerHeight);
            marginBottomValue = availableSpace - marginTopValue;
        } else {
            // Over-constrained, (no need solve for bottom)
            marginTopValue = marginTop.calcValue(containerHeight);
            marginBottomValue = marginBottom.calcValue(containerHeight);
        }
    } else {
        /*--------------------------------------------------------------------*\
         * Otherwise, set 'auto' values for 'margin-top' and 'margin-bottom'
         * to 0, and pick the one of the following six rules that applies.
         *
         * 1. 'top' and 'height' are 'auto' and 'bottom' is not 'auto', then 
         *    the height is based on the content, and solve for 'top'.
         *
         *              OMIT RULE 2 AS IT SHOULD NEVER BE HIT
         * ------------------------------------------------------------------
         * 2. 'top' and 'bottom' are 'auto' and 'height' is not 'auto', then
         *    set 'top' to the static position, and solve for 'bottom'.
         * ------------------------------------------------------------------
         *
         * 3. 'height' and 'bottom' are 'auto' and 'top' is not 'auto', then
         *    the height is based on the content, and solve for 'bottom'.
         * 4. 'top' is 'auto', 'height' and 'bottom' are not 'auto', and
         *    solve for 'top'.
         * 5. 'height' is 'auto', 'top' and 'bottom' are not 'auto', and
         *    solve for 'height'.
         * 6. 'bottom' is 'auto', 'top' and 'height' are not 'auto', and
         *    solve for 'bottom'.
        \*--------------------------------------------------------------------*/
        // NOTE: For rules 3 and 6 it is not necessary to solve for 'bottom'
        // because the value is not used for any further calculations.

        // Calculate margins, 'auto' margins are ignored.
        marginTopValue = marginTop.calcMinValue(containerHeight);
        marginBottomValue = marginBottom.calcMinValue(containerHeight);

        const int availableSpace = containerHeight - (marginTopValue + marginBottomValue + bordersPlusPadding);

        // Use rule/case that applies.
        if (topIsAuto && heightIsAuto && !bottomIsAuto) {
            // RULE 1: (height is content based, solve of top)
            heightValue = contentHeight;
            topValue = availableSpace - (heightValue + bottom.calcValue(containerHeight));
        } else if (!topIsAuto && heightIsAuto && bottomIsAuto) {
            // RULE 3: (height is content based, no need solve of bottom)
            topValue = top.calcValue(containerHeight);
            heightValue = contentHeight;
        } else if (topIsAuto && !heightIsAuto && !bottomIsAuto) {
            // RULE 4: (solve of top)
            heightValue = calcContentBoxHeight(height.calcValue(containerHeight));
            topValue = availableSpace - (heightValue + bottom.calcValue(containerHeight));
        } else if (!topIsAuto && heightIsAuto && !bottomIsAuto) {
            // RULE 5: (solve of height)
            topValue = top.calcValue(containerHeight);
            heightValue = max(0, availableSpace - (topValue + bottom.calcValue(containerHeight)));
        } else if (!topIsAuto && !heightIsAuto && bottomIsAuto) {
            // RULE 6: (no need solve of bottom)
            heightValue = calcContentBoxHeight(height.calcValue(containerHeight));
            topValue = top.calcValue(containerHeight);
        }
    }

    // Use computed values to calculate the vertical position.
    yPos = topValue + marginTopValue + containerBlock->borderTop();
}

void RenderBox::calcAbsoluteHorizontalReplaced()
{   
    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.3.8 "Absolutly positioned, replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-replaced-width>
    // (block-style-comments in this function correspond to text from the spec and
    // the numbers correspond to numbers in spec)

    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    const RenderObject* containerBlock = container();

    // FIXME: This is incorrect for cases where the container block is a relatively
    // positioned inline.
    const int containerWidth = containingBlockWidth() + containerBlock->paddingLeft() + containerBlock->paddingRight();
    
    // To match WinIE, in quirks mode use the parent's 'direction' property 
    // instead of the the container block's.
    TextDirection containerDirection = (style()->htmlHacks()) ? parent()->style()->direction() : containerBlock->style()->direction();

    // Variables to solve.
    Length left = style()->left();
    Length right = style()->right();
    Length marginLeft = style()->marginLeft();
    Length marginRight = style()->marginRight();


    /*-----------------------------------------------------------------------*\
     * 1. The used value of 'width' is determined as for inline replaced
     *    elements.
    \*-----------------------------------------------------------------------*/
    // NOTE: This value of width is FINAL in that the min/max width calculations
    // are dealt with in calcReplacedWidth().  This means that the steps to produce
    // correct max/min in the non-replaced version, are not necessary.
    m_width = calcReplacedWidth() + borderLeft() + borderRight() + paddingLeft() + paddingRight();
    const int availableSpace = containerWidth - m_width;

    /*-----------------------------------------------------------------------*\
     * 2. If both 'left' and 'right' have the value 'auto', then if 'direction'
     *    of the containing block is 'ltr', set 'left' to the static position;
     *    else if 'direction' is 'rtl', set 'right' to the static position.
    \*-----------------------------------------------------------------------*/
    // see FIXME 2
    if (left.isAuto() && right.isAuto()) {
        // see FIXME 1
        if (containerDirection == LTR) {
            // 'm_staticX' should already have been set through layout of the parent.
            int staticPosition = m_staticX - containerBlock->borderLeft();
            for (RenderObject* po = parent(); po && po != containerBlock; po = po->parent())
                staticPosition += po->xPos();
            left.setValue(Fixed, staticPosition);
        } else {
            RenderObject* po = parent();
            // 'm_staticX' should already have been set through layout of the parent.
            int staticPosition = m_staticX + containerWidth + containerBlock->borderRight() - po->width();
            for (; po && po != containerBlock; po = po->parent())
                staticPosition -= po->xPos();
            right.setValue(Fixed, staticPosition);
        }
    }

    /*-----------------------------------------------------------------------*\
     * 3. If 'left' or 'right' are 'auto', replace any 'auto' on 'margin-left'
     *    or 'margin-right' with '0'.
    \*-----------------------------------------------------------------------*/
    if (left.isAuto() || right.isAuto()) {
        if (marginLeft.isAuto())
            marginLeft.setValue(Fixed, 0);
        if (marginRight.isAuto())
            marginRight.setValue(Fixed, 0);
    }

    /*-----------------------------------------------------------------------*\
     * 4. If at this point both 'margin-left' and 'margin-right' are still
     *    'auto', solve the equation under the extra constraint that the two
     *    margins must get equal values, unless this would make them negative,
     *    in which case when the direction of the containing block is 'ltr'
     *    ('rtl'), set 'margin-left' ('margin-right') to zero and solve for
     *    'margin-right' ('margin-left').
    \*-----------------------------------------------------------------------*/
    int leftValue = 0;
    int rightValue = 0;

    if (marginLeft.isAuto() && marginRight.isAuto()) {
        // 'left' and 'right' cannot be 'auto' due to step 3
        ASSERT(!(left.isAuto() && right.isAuto()));

        leftValue = left.calcValue(containerWidth);
        rightValue = right.calcValue(containerWidth);

        int difference = availableSpace - (leftValue + rightValue);
        if (difference > 0) {
            m_marginLeft = difference / 2; // split the diference
            m_marginRight = difference - m_marginLeft; // account for odd valued differences
        } else {
            // see FIXME 1
            if (containerDirection == LTR) {
                m_marginLeft = 0;
                m_marginRight = difference;  // will be negative
            } else {
                m_marginLeft = difference;  // will be negative
                m_marginRight = 0;
            }
        }

    /*-----------------------------------------------------------------------*\
     * 5. If at this point there is an 'auto' left, solve the equation for
     *    that value.
    \*-----------------------------------------------------------------------*/
    } else if (left.isAuto()) {
        m_marginLeft = marginLeft.calcValue(containerWidth);
        m_marginRight = marginRight.calcValue(containerWidth);
        rightValue = right.calcValue(containerWidth);

        // Solve for 'left'
        leftValue = availableSpace - (rightValue + m_marginLeft + m_marginRight);
    } else if (right.isAuto()) {
        m_marginLeft = marginLeft.calcValue(containerWidth);
        m_marginRight = marginRight.calcValue(containerWidth);
        leftValue = left.calcValue(containerWidth);

        // Solve for 'right'
        rightValue = availableSpace - (leftValue + m_marginLeft + m_marginRight);
    } else if (marginLeft.isAuto()) {
        m_marginRight = marginRight.calcValue(containerWidth);
        leftValue = left.calcValue(containerWidth);
        rightValue = right.calcValue(containerWidth);

        // Solve for 'margin-left'
        m_marginLeft = availableSpace - (leftValue + rightValue + m_marginRight);
    } else if (marginRight.isAuto()) {
        m_marginLeft = marginLeft.calcValue(containerWidth);
        leftValue = left.calcValue(containerWidth);
        rightValue = right.calcValue(containerWidth);

        // Solve for 'margin-right'
        m_marginRight = availableSpace - (leftValue + rightValue + m_marginLeft);
    }

    /*-----------------------------------------------------------------------*\
     * 6. If at this point the values are over-constrained, ignore the value
     *    for either 'left' (in case the 'direction' property of the
     *    containing block is 'rtl') or 'right' (in case 'direction' is
     *    'ltr') and solve for that value.
    \*-----------------------------------------------------------------------*/
    // NOTE:  It is not necessary to solve for 'right' when the direction is
    // LTR because the value is not used.
    int totalWidth = m_width + leftValue + rightValue +  m_marginLeft + m_marginRight;
    if (totalWidth > containerWidth && (containerDirection == RTL))
        leftValue = containerWidth - (totalWidth - leftValue);


    // Use computed values to calculate the horizontal position.
    m_x = leftValue + m_marginLeft + containerBlock->borderLeft();
}

void RenderBox::calcAbsoluteVerticalReplaced()
{
    // The following is based off of the W3C Working Draft from April 11, 2006 of
    // CSS 2.1: Section 10.6.5 "Absolutly positioned, replaced elements"
    // <http://www.w3.org/TR/2005/WD-CSS21-20050613/visudet.html#abs-replaced-height>
    // (block-style-comments in this function correspond to text from the spec and
    // the numbers correspond to numbers in spec)

    // We don't use containingBlock(), since we may be positioned by an enclosing relpositioned inline.
    const RenderObject* containerBlock = container();

    // Even in strict mode (where we don't grow the root to fill the viewport)
    // other browsers position as though the root fills the viewport.
    const int containerHeight = containerBlock->isRoot() ? containerBlock->availableHeight() : (containerBlock->height() - containerBlock->borderTop() - containerBlock->borderBottom());

    // Variables to solve.
    Length top = style()->top();
    Length bottom = style()->bottom();
    Length marginTop = style()->marginTop();
    Length marginBottom = style()->marginBottom();


    /*-----------------------------------------------------------------------*\
     * 1. The used value of 'height' is determined as for inline replaced
     *    elements.
    \*-----------------------------------------------------------------------*/
    // NOTE: This value of height is FINAL in that the min/max height calculations
    // are dealt with in calcReplacedHeight().  This means that the steps to produce
    // correct max/min in the non-replaced version, are not necessary.
    m_height = calcReplacedHeight() + borderTop() + borderBottom() + paddingTop() + paddingBottom();
    const int availableSpace = containerHeight - m_height;

    /*-----------------------------------------------------------------------*\
     * 2. If both 'top' and 'bottom' have the value 'auto', replace 'top'
     *    with the element's static position.
    \*-----------------------------------------------------------------------*/
    // see FIXME 2
    if (top.isAuto() && bottom.isAuto()) {
        // m_staticY should already have been set through layout of the parent().
        int staticTop = m_staticY - containerBlock->borderTop();
        for (RenderObject* po = parent(); po && po != containerBlock; po = po->parent()) {
            if (!po->isTableRow())
                staticTop += po->yPos();
        }
        top.setValue(Fixed, staticTop);
    }

    /*-----------------------------------------------------------------------*\
     * 3. If 'bottom' is 'auto', replace any 'auto' on 'margin-top' or
     *    'margin-bottom' with '0'.
    \*-----------------------------------------------------------------------*/
    // FIXME: The spec. says that this step should only be taken when bottom is
    // auto, but if only top is auto, this makes step 4 impossible.
    if (top.isAuto() || bottom.isAuto()) {
        if (marginTop.isAuto())
            marginTop.setValue(Fixed, 0);
        if (marginBottom.isAuto())
            marginBottom.setValue(Fixed, 0);
    }

    /*-----------------------------------------------------------------------*\
     * 4. If at this point both 'margin-top' and 'margin-bottom' are still
     *    'auto', solve the equation under the extra constraint that the two
     *    margins must get equal values.
    \*-----------------------------------------------------------------------*/
    int topValue = 0;
    int bottomValue = 0;

    if (marginTop.isAuto() && marginBottom.isAuto()) {
        // 'top' and 'bottom' cannot be 'auto' due to step 2 and 3 combinded.
        ASSERT(!(top.isAuto() || bottom.isAuto()));

        topValue = top.calcValue(containerHeight);
        bottomValue = bottom.calcValue(containerHeight);

        int difference = availableSpace - (topValue + bottomValue);
        // NOTE: This may result in negative values.
        m_marginTop =  difference / 2; // split the difference
        m_marginBottom = difference - m_marginTop; // account for odd valued differences

    /*-----------------------------------------------------------------------*\
     * 5. If at this point there is only one 'auto' left, solve the equation
     *    for that value.
    \*-----------------------------------------------------------------------*/
    } else if (top.isAuto()) {
        m_marginTop = marginTop.calcValue(containerHeight);
        m_marginBottom = marginBottom.calcValue(containerHeight);
        bottomValue = bottom.calcValue(containerHeight);

        // Solve for 'top'
        topValue = availableSpace - (bottomValue + m_marginTop + m_marginBottom);
    } else if (bottom.isAuto()) {
        m_marginTop = marginTop.calcValue(containerHeight);
        m_marginBottom = marginBottom.calcValue(containerHeight);
        topValue = top.calcValue(containerHeight);

        // Solve for 'bottom'
        // NOTE: It is not necessary to solve for 'bottom' because we don't ever
        // use the value.
    } else if (marginTop.isAuto()) {
        m_marginBottom = marginBottom.calcValue(containerHeight);
        topValue = top.calcValue(containerHeight);
        bottomValue = bottom.calcValue(containerHeight);

        // Solve for 'margin-top'
        m_marginTop = availableSpace - (topValue + bottomValue + m_marginBottom);
    } else if (marginBottom.isAuto()) {
        m_marginTop = marginTop.calcValue(containerHeight);
        topValue = top.calcValue(containerHeight);
        bottomValue = bottom.calcValue(containerHeight);

        // Solve for 'margin-bottom'
        m_marginBottom = availableSpace - (topValue + bottomValue + m_marginTop);
    }

    /*-----------------------------------------------------------------------*\
     * 6. If at this point the values are over-constrained, ignore the value
     *    for 'bottom' and solve for that value.
    \*-----------------------------------------------------------------------*/
    // NOTE: It is not necessary to do this step because we don't end up using
    // the value of 'bottom' regardless of whether the values are over-constrained
    // or not.

    // Use computed values to calculate the vertical position.
    m_y = topValue + m_marginTop + containerBlock->borderTop();
}

IntRect RenderBox::caretRect(int offset, EAffinity affinity, int* extraWidthToEndOfLine)
{
    // FIXME: Is it OK to check only first child instead of picking
    // right child based on offset? Is it OK to pass the same offset
    // along to the child instead of offset 0 or whatever?

    // propagate it downwards to its children, someone will feel responsible
    if (RenderObject* child = firstChild()) {
        IntRect result = child->caretRect(offset, affinity, extraWidthToEndOfLine);
        if (!result.isEmpty())
            return result;
    }

    // if not, use the extents of this box 
    // offset 0 means left, offset 1 means right
    // FIXME: What about border and padding?
    const int caretWidth = 1;
    IntRect rect(xPos(), yPos(), caretWidth, m_height);
    if (offset != 0)
        rect.move(IntSize(m_width - caretWidth, 0));
    if (InlineBox* box = inlineBoxWrapper()) {
        RootInlineBox* rootBox = box->root();
        int top = rootBox->topOverflow();
        rect.setY(top);
        rect.setHeight(rootBox->bottomOverflow() - top);
    }

    // If height of box is smaller than font height, use the latter one,
    // otherwise the caret might become invisible.
    // 
    // Also, if the box is not a replaced element, always use the font height.
    // This prevents the "big caret" bug described in:
    // <rdar://problem/3777804> Deleting all content in a document can result in giant tall-as-window insertion point
    //
    // FIXME: ignoring :first-line, missing good reason to take care of
    int fontHeight = style()->font().height();
    if (fontHeight > rect.height() || !isReplaced())
        rect.setHeight(fontHeight);

    RenderObject* cb = containingBlock();
    int cbx, cby;
    if (!cb || !cb->absolutePosition(cbx, cby))
        // No point returning a relative position.
        return IntRect();

    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = xPos() + m_width - rect.right();

    rect.move(cbx, cby);
    return rect;
}

int RenderBox::lowestPosition(bool includeOverflowInterior, bool includeSelf) const
{
    return includeSelf ? m_height : 0;
}

int RenderBox::rightmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    return includeSelf ? m_width : 0;
}

int RenderBox::leftmostPosition(bool includeOverflowInterior, bool includeSelf) const
{
    return includeSelf ? 0 : m_width;
}

}
