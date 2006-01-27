/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
#include "render_image.h"

#include "CachedImage.h"
#include "DocumentImpl.h"
#include "HTMLInputElementImpl.h"
#include "helper.h"
#include "html_imageimpl.h"
#include "render_canvas.h"
#include <qpainter.h>
#include "Pen.h"
#include "htmlnames.h"

namespace WebCore {

using namespace HTMLNames;

RenderImage::RenderImage(NodeImpl *_node)
    : RenderReplaced(_node)
{
    m_image = 0;
    berrorPic = false;
    m_selectionState = SelectionNone;
    setIntrinsicWidth( 0 );
    setIntrinsicHeight( 0 );
    updateAltText();
}

RenderImage::~RenderImage()
{
    if (m_image)
        m_image->deref(this);
    pix.decreaseUseCount();
}

void RenderImage::setStyle(RenderStyle* _style)
{
    RenderReplaced::setStyle(_style);
    
    setShouldPaintBackgroundOrBorder(true);
}

void RenderImage::setContentObject(CachedObject* co)
{
    if (co && m_image != co) {
        if (m_image)
            m_image->deref(this);
        m_image = static_cast<CachedImage*>(co);
        if (m_image)
            m_image->ref(this);
    }
}

void RenderImage::setImage(CachedImage* newImage)
{
    if (m_image != newImage) {
        if (m_image)
            m_image->deref(this);
        m_image = newImage;
        if (m_image)
            m_image->ref(this);
        if (m_image->isErrorImage())
            setImage(m_image->image(), IntRect(0,0,16,16), m_image);
    }
}

void RenderImage::setImage( const Image &p, const IntRect& r, CachedImage *o)
{
    if (o != m_image) {
        RenderReplaced::setImage(p, r, o);
        return;
    }

    bool iwchanged = false;

    if(o->isErrorImage()) {
        int iw = p.width() + 4;
        int ih = p.height() + 4;

        // we have an alt and the user meant it (its not a text we invented)
        if (!alt.isEmpty()) {
            const QFontMetrics &fm = style()->fontMetrics();
            IntRect br = fm.boundingRect (  0, 0, 1024, 256, Qt::AlignAuto|Qt::WordBreak, alt.qstring(), 0, 0);  // FIX: correct tabwidth?
            if ( br.width() > iw )
                iw = br.width();
            if ( br.height() > ih )
                ih = br.height();
        }

        if ( iw != intrinsicWidth() ) {
            setIntrinsicWidth( iw );
            iwchanged = true;
        }
        if ( ih != intrinsicHeight() ) {
            setIntrinsicHeight( ih );
            iwchanged = true;
        }
    }
    berrorPic = o->isErrorImage();

    bool needlayout = false;

    // Image dimensions have been changed, see what needs to be done
     if ( ( o->image_size().width() != intrinsicWidth() ||
           o->image_size().height() != intrinsicHeight() || iwchanged) )
    {
        if(!o->isErrorImage()) {
            setIntrinsicWidth( o->image_size().width() );
            setIntrinsicHeight( o->image_size().height() );
        }

        // In the case of generated image content using :before/:after, we might not be in the
        // render tree yet.  In that case, we don't need to worry about check for layout, since we'll get a
        // layout when we get added in to the render tree hierarchy later.
        if (containingBlock()) {
            // lets see if we need to relayout at all..
            int oldwidth = m_width;
            int oldheight = m_height;
            calcWidth();
            calcHeight();
    
            if(iwchanged || m_width != oldwidth || m_height != oldheight)
                needlayout = true;
    
            m_width = oldwidth;
            m_height = oldheight;
        }
    }

    // Stop the previous image, if it may be animating.
    pix.stopAnimations();
    
    pix.decreaseUseCount();
    pix = p;
    p.increaseUseCount();

    if (needlayout) {
        if (!selfNeedsLayout())
            setNeedsLayout(true);
        if (minMaxKnown())
            setMinMaxKnown(false);
    }
    else {
        // FIXME: We always just do a complete repaint, since we always pass in the full image
        // rect at the moment anyway.
        resizeCache = Image();
        repaintRectangle(IntRect(borderLeft()+paddingLeft(), borderTop()+paddingTop(), contentWidth(), contentHeight()));
    }
}

void RenderImage::resetAnimation()
{
    pix.resetAnimation();
    if (!needsLayout())
        repaint();
}

void RenderImage::paint(PaintInfo& i, int _tx, int _ty)
{
    if (!shouldPaint(i, _tx, _ty)) return;

    _tx += m_x;
    _ty += m_y;
        
    if (shouldPaintBackgroundOrBorder() && i.phase != PaintActionOutline) 
        paintBoxDecorations(i, _tx, _ty);

    QPainter* p = i.p;
    
    if (i.phase == PaintActionOutline && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(p, _tx, _ty, width(), height(), style());
    
    if (i.phase != PaintActionForeground && i.phase != PaintActionSelection)
        return;

    if (!shouldPaintWithinRoot(i))
        return;
        
    bool isPrinting = i.p->printing();
    bool drawSelectionTint = isSelected() && !isPrinting;
    if (i.phase == PaintActionSelection) {
        if (selectionState() == SelectionNone) {
            return;
        }
        drawSelectionTint = false;
    }
        
    int cWidth = contentWidth();
    int cHeight = contentHeight();
    int leftBorder = borderLeft();
    int topBorder = borderTop();
    int leftPad = paddingLeft();
    int topPad = paddingTop();

    if (khtml::printpainter && !canvas()->printImages())
        return;

    //kdDebug( 6040 ) << "    contents (" << contentWidth << "/" << contentHeight << ") border=" << borderLeft() << " padding=" << paddingLeft() << endl;
    if ( pix.isNull() || berrorPic)
    {
        if (i.phase == PaintActionSelection)
            return;

        if(cWidth > 2 && cHeight > 2)
        {
            if ( !berrorPic ) {
                p->setPen (Color::lightGray);
                p->setBrush (WebCore::Brush::NoBrush);
                p->drawRect (_tx + leftBorder + leftPad, _ty + topBorder + topPad, cWidth, cHeight);
            }
            
            bool errorPictureDrawn = false;
            int imageX = 0, imageY = 0;
            int usableWidth = cWidth;
            int usableHeight = cHeight;
            
            if (berrorPic && !pix.isNull() && (usableWidth >= pix.width()) && (usableHeight >= pix.height())) {
                // Center the error image, accounting for border and padding.
                int centerX = (usableWidth - pix.width())/2;
                if (centerX < 0)
                    centerX = 0;
                int centerY = (usableHeight - pix.height())/2;
                if (centerY < 0)
                    centerY = 0;
                imageX = leftBorder + leftPad + centerX;
                imageY = topBorder + topPad + centerY;
                p->drawImage(IntPoint(_tx + imageX, _ty + imageY), pix, pix.rect());
                errorPictureDrawn = true;
            }
            
            if (!alt.isEmpty()) {
                QString text = alt.qstring();
                text.replace(QChar('\\'), backslashAsCurrencySymbol());
                p->setFont (style()->font());
                p->setPen (style()->color());
                int ax = _tx + leftBorder + leftPad;
                int ay = _ty + topBorder + topPad;
                const QFontMetrics &fm = style()->fontMetrics();
                int ascent = fm.ascent();
                
                // Only draw the alt text if it'll fit within the content box,
                // and only if it fits above the error image.
                int textWidth = fm.width (text, 0, 0, text.length());
                if (errorPictureDrawn) {
                    if (usableWidth >= textWidth && fm.height() <= imageY)
                        p->drawText(ax, ay+ascent, tabWidth(), 0, 0 /* ignored */, 0 /* ignored */, Qt::WordBreak  /* not supported */, text );
                } else if (usableWidth >= textWidth && cHeight >= fm.height())
                    p->drawText(ax, ay+ascent, tabWidth(), 0, 0 /* ignored */, 0 /* ignored */, Qt::WordBreak  /* not supported */, text );
            }
        }
    }
    else if (m_image && !m_image->isTransparent()) {
        if ( (cWidth != intrinsicWidth() ||  cHeight != intrinsicHeight()) &&
             pix.width() > 0 && pix.height() > 0 && m_image->valid_rect().isValid())
        {
            IntSize tintSize;
            if (resizeCache.isNull() && cWidth && cHeight)
            {
                IntRect scaledrect(m_image->valid_rect());
                resizeCache = pix;
                scaledrect.setWidth( ( cWidth*scaledrect.width() ) / intrinsicWidth() );
                scaledrect.setHeight( ( cHeight*scaledrect.height() ) / intrinsicHeight() );

                // sometimes scaledrect.width/height are off by one because
                // of rounding errors. if the image is fully loaded, we
                // make sure that we don't do unnecessary resizes during painting
                IntSize s(scaledrect.size());
                if (m_image->valid_rect().size() == IntSize( intrinsicWidth(), intrinsicHeight() )) // fully loaded
                    s = IntSize(cWidth, cHeight);
                if (QABS(s.width() - cWidth) < 2) // rounding errors
                    s.setWidth(cWidth);
                if (resizeCache.size() != s)
                    resizeCache.resize(s);

                p->drawImage( IntPoint( _tx + leftBorder + leftPad, _ty + topBorder + topPad),
                               resizeCache, scaledrect );
                tintSize = s;
            } else {
                p->drawImage( IntPoint( _tx + leftBorder + leftPad, _ty + topBorder + topPad), resizeCache );
                tintSize = resizeCache.rect().size();
            }
            if (drawSelectionTint) {
                Brush brush(selectionColor(p));
                IntRect selRect(selectionRect());
                p->fillRect(selRect.x(), selRect.y(), selRect.width(), selRect.height(), brush);
            }
        }
        else
        {
            // we might be just about switching images
            // so pix contains the old one (we want to paint), but image->valid_rect is still invalid
            // so use intrinsic Size instead.
            // ### maybe no progressive loading for the second image ?
            IntRect rect(m_image->valid_rect().isValid() ? m_image->valid_rect()
                       : IntRect(0, 0, intrinsicWidth(), intrinsicHeight()));

            IntPoint offs( _tx + leftBorder + leftPad, _ty + topBorder + topPad);
             HTMLImageElementImpl* i = (element() && element()->hasTagName(imgTag)) ? static_cast<HTMLImageElementImpl*>(element()) : 0;
             if (i && !i->compositeOperator().isNull()){
                p->drawImage (offs, pix, rect, i->compositeOperator());
             }
             else {
                 p->drawImage(offs, pix, rect);
             }
             if (drawSelectionTint) {
                 Brush brush(selectionColor(p));
                 IntRect selRect(selectionRect());
                 p->fillRect(selRect.x(), selRect.y(), selRect.width(), selRect.height(), brush);
             }
        }
    }
}

void RenderImage::layout()
{
    KHTMLAssert(needsLayout());
    KHTMLAssert( minMaxKnown() );

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = getAbsoluteRepaintRect();
    
    int oldwidth = m_width;
    int oldheight = m_height;

    // minimum height
    m_height = m_image && m_image->isErrorImage() ? intrinsicHeight() : 0;

    calcWidth();
    calcHeight();

    // if they are variable width and we calculate a huge height or width, we assume they
    // actually wanted the intrinsic width.
    if ( m_width > 2048 && !style()->width().isFixed() )
        m_width = intrinsicWidth();
    if ( m_height > 2048 && !style()->height().isFixed() )
        m_height = intrinsicHeight();

// We don't want to impose a constraint on image size here. But there also
// is a bug somewhere that causes the scaled height to be used with the
// original width, causing the image to be compressed vertically.

    if ( m_width != oldwidth || m_height != oldheight )
        resizeCache = Image();

    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
    
    setNeedsLayout(false);
}

HTMLMapElementImpl* RenderImage::imageMap()
{
    HTMLImageElementImpl* i = element() && element()->hasTagName(imgTag) ? static_cast<HTMLImageElementImpl*>(element()) : 0;
    return i ? i->getDocument()->getImageMap(i->imageMap()) : 0;
}

bool RenderImage::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty,
                              HitTestAction hitTestAction)
{
    bool inside = RenderReplaced::nodeAtPoint(info, _x, _y, _tx, _ty, hitTestAction);

    if (inside && element()) {
        int tx = _tx + m_x;
        int ty = _ty + m_y;
        
        HTMLMapElementImpl* map = imageMap();
        if (map) {
            // we're a client side image map
            inside = map->mapMouseEvent(_x - tx, _y - ty, contentWidth(), contentHeight(), info);
            info.setInnerNonSharedNode(element());
        }
    }

    return inside;
}

void RenderImage::updateAltText()
{
    if (!element())
        return;
        
    if (element()->hasTagName(inputTag))
        alt = static_cast<HTMLInputElementImpl*>(element())->altText();
    else if (element()->hasTagName(imgTag))
        alt = static_cast<HTMLImageElementImpl*>(element())->altText();
}

bool RenderImage::isWidthSpecified() const
{
    switch (style()->width().type) {
        case Fixed:
        case Percent:
            return true;
        default:
            return false;
    }
    assert(false);
    return false;
}

bool RenderImage::isHeightSpecified() const
{
    switch (style()->height().type) {
        case Fixed:
        case Percent:
            return true;
        default:
            return false;
    }
    assert(false);
    return false;
}

int RenderImage::calcReplacedWidth() const
{
    // If height is specified and not width, preserve aspect ratio.
    if (isHeightSpecified() && !isWidthSpecified()) {
        if (intrinsicHeight() == 0)
            return 0;
        if (!m_image || m_image->isErrorImage())
            return intrinsicWidth(); // Don't bother scaling.
        return calcReplacedHeight() * intrinsicWidth() / intrinsicHeight();
    }
    return RenderReplaced::calcReplacedWidth();
}

int RenderImage::calcReplacedHeight() const
{
    // If width is specified and not height, preserve aspect ratio.
    if (isWidthSpecified() && !isHeightSpecified()) {
        if (intrinsicWidth() == 0)
            return 0;
        if (!m_image || m_image->isErrorImage())
            return intrinsicHeight(); // Don't bother scaling.
        return calcReplacedWidth() * intrinsicHeight() / intrinsicWidth();
    }
    return RenderReplaced::calcReplacedHeight();
}

}
