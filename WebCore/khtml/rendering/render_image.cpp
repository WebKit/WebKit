/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
//#define DEBUG_LAYOUT

#include "render_image.h"
#include "render_canvas.h"

#include <qdrawutil.h>
#include <qpainter.h>

#include <kapplication.h>
#include <kdebug.h>

#include "css/csshelper.h"
#include "misc/helper.h"
#include "misc/htmlattrs.h"
#include "misc/htmltags.h"
#include "html/html_formimpl.h"
#include "html/html_imageimpl.h"
#include "html/dtd.h"
#include "xml/dom2_eventsimpl.h"
#include "html/html_documentimpl.h"
#include <math.h>

using namespace DOM;
using namespace khtml;

// -------------------------------------------------------------------------

RenderImage::RenderImage(HTMLElementImpl *_element)
    : RenderReplaced(_element)
{
    image = 0;
    berrorPic = false;
    loadEventSent = false;

    setIntrinsicWidth( 0 );
    setIntrinsicHeight( 0 );
}

RenderImage::~RenderImage()
{
    if(image) image->deref(this);
}

void RenderImage::setStyle(RenderStyle* _style)
{
    RenderReplaced::setStyle(_style);
    
    setOverhangingContents(style()->height().isPercent());
    setShouldPaintBackgroundOrBorder(true);
}

void RenderImage::setContentObject(CachedObject* co)
{
    if (co && image != co) {
        if (image) image->deref(this);
        image = static_cast<CachedImage*>(co);
        if (image) image->ref(this);
    }
}

void RenderImage::setPixmap( const QPixmap &p, const QRect& r, CachedImage *o)
{
    if(o != image) {
        RenderReplaced::setPixmap(p, r, o);
        return;
    }

    bool iwchanged = false;

    if(o->isErrorImage()) {
        int iw = p.width() + 8;
        int ih = p.height() + 8;

        // we have an alt and the user meant it (its not a text we invented)
        if ( element() && !alt.isEmpty() && !element()->getAttribute( ATTR_ALT ).isNull()) {
            const QFontMetrics &fm = style()->fontMetrics();
            QRect br = fm.boundingRect (  0, 0, 1024, 256, Qt::AlignAuto|Qt::WordBreak, alt.string() );
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
     if ( ( o->pixmap_size().width() != intrinsicWidth() ||
           o->pixmap_size().height() != intrinsicHeight() || iwchanged) )
    {
//          qDebug("image dimensions have been changed, old: %d/%d  new: %d/%d",
//                 intrinsicWidth(), intrinsicHeight(),
//               o->pixmap_size().width(), o->pixmap_size().height());

        if(!o->isErrorImage()) {
            setIntrinsicWidth( o->pixmap_size().width() );
            setIntrinsicHeight( o->pixmap_size().height() );
        }

        // In the case of generated image content using :before/:after, we aren't in the
        // tree yet.  We don't need to worry about doing this check, since we'll get a
        // layout when we get added in to the render tree hierarchy anyway.
         
        if (parent()) {
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

#if APPLE_CHANGES
    // Stop the previous image, if it may be animating.
    pix.stopAnimations();
#endif
    
    pix = p;

    if(needlayout)
    {
        setNeedsLayout(true);
        setMinMaxKnown(false);

//         kdDebug( 6040 ) << "m_width: : " << m_width << " height: " << m_height << endl;
//         kdDebug( 6040 ) << "Image: size " << m_width << "/" << m_height << endl;
    }
    else
    {
        bool completeRepaint = !resizeCache.isNull();
        int cHeight = contentHeight();
        int scaledHeight = intrinsicHeight() ? ((o->valid_rect().height()*cHeight)/intrinsicHeight()) : 0;

        // don't bog down X server doing xforms
        if(completeRepaint && cHeight >= 5 &&  o->valid_rect().height() < intrinsicHeight() &&
           (scaledHeight / (cHeight/5) == resizeCache.height() / (cHeight/5)))
            return;

        resizeCache = QPixmap(); // for resized animations
        if(completeRepaint)
            repaintRectangle(borderLeft()+paddingLeft(), borderTop()+paddingTop(), contentWidth(), contentHeight());
        else
        {
            repaintRectangle(r.x() + borderLeft() + paddingLeft(), r.y() + borderTop() + paddingTop(),
                             r.width(), r.height());
        }
    }
}

void RenderImage::paintObject(QPainter *p, int /*_x*/, int /*_y*/, int /*_w*/, int /*_h*/, int _tx, int _ty, PaintAction paintAction)
{
    if (paintAction != PaintActionForeground)
        return;
        
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
        if(cWidth > 2 && cHeight > 2)
        {
#if APPLE_CHANGES
            if ( !berrorPic ) {
                p->setPen (Qt::lightGray);
                p->setBrush (Qt::NoBrush);
                p->drawRect (_tx + leftBorder + leftPad, _ty + topBorder + topPad, cWidth, cHeight);
	    }

            bool errorPictureDrawn = false;
            int imageX = 0, imageY = 0;
            int usableWidth = cWidth - leftBorder - borderRight() - leftPad - paddingRight();
            int usableHeight = cHeight - topBorder - borderBottom() - topPad - paddingBottom();
            
            if(berrorPic && !pix.isNull() && (usableWidth >= pix.width()) && (usableHeight >= pix.height()) )
            {
                // Center the error image, accounting for border and padding.
                int centerX = (usableWidth - pix.width())/2;
                if (centerX < 0)
                    centerX = 0;
                int centerY = (usableHeight - pix.height())/2;
                if (centerY < 0)
                    centerY = 0;
                imageX = leftBorder + leftPad + centerX;
                imageY = topBorder + topPad + centerY;
                p->drawPixmap( QPoint(_tx + imageX, _ty + imageY), pix, pix.rect() );
                errorPictureDrawn = true;
            }
            
            if(!alt.isEmpty()) {
                QString text = alt.string();
                text.replace('\\', backslashAsCurrencySymbol());
                p->setFont (style()->font());
                p->setPen (style()->color());
                int ax = _tx + leftBorder + leftPad;
                int ay = _ty + topBorder + topPad;
                const QFontMetrics &fm = style()->fontMetrics();
                int ascent = fm.ascent();
                
                // Only draw the alt text if it'll fit within the content box,
                // and only if it fits above the error image.
                int textWidth = fm.width (text, text.length());
                if (errorPictureDrawn){
                    if (usableWidth > textWidth && fm.height() <= imageY)
                        p->drawText(ax, ay+ascent, 0 /* ignored */, 0 /* ignored */, Qt::WordBreak  /* not supported */, text );
                }
                else if (usableWidth >= textWidth && cHeight>=fm.height())
                    p->drawText(ax, ay+ascent, 0 /* ignored */, 0 /* ignored */, Qt::WordBreak  /* not supported */, text );
            }
#else /* not APPLE_CHANGES */
            if ( !berrorPic ) {
                //qDebug("qDrawShadePanel %d/%d/%d/%d", _tx + leftBorder, _ty + topBorder, cWidth, cHeight);
                qDrawShadePanel( p, _tx + leftBorder + leftPad, _ty + topBorder + topPad, cWidth, cHeight,
                                 KApplication::palette().inactive(), true, 1 );
            }
            if(berrorPic && !pix.isNull() && (cWidth >= pix.width()+4) && (cHeight >= pix.height()+4) )
            {
                QRect r(pix.rect());
                r = r.intersect(QRect(0, 0, cWidth-4, cHeight-4));
                p->drawPixmap( QPoint( _tx + leftBorder + leftPad+2, _ty + topBorder + topPad+2), pix, r );
            }
            if(!alt.isEmpty()) {
                QString text = alt.string();
                text.replace('\\', backslashAsCurrencySymbol());
                p->setFont(style()->font());
                p->setPen( style()->color() );
                int ax = _tx + leftBorder + leftPad + 2;
                int ay = _ty + topBorder + topPad + 2;
                const QFontMetrics &fm = style()->fontMetrics();
                if (cWidth>5 && cHeight>=fm.height())
                    p->drawText(ax, ay+1, cWidth - 4, cHeight - 4, Qt::WordBreak, text );
            }
#endif /* APPLE_CHANGES not defined */
        }
    }
    else if (image && !image->isTransparent())
    {
        if ( (cWidth != intrinsicWidth() ||  cHeight != intrinsicHeight()) &&
             pix.width() > 0 && pix.height() > 0 && image->valid_rect().isValid())
        {
            if (resizeCache.isNull() && cWidth && cHeight)
            {
                QRect scaledrect(image->valid_rect());
//                 kdDebug(6040) << "time elapsed: " << dt->elapsed() << endl;
//                  kdDebug( 6040 ) << "have to scale: " << endl;
//                  qDebug("cw=%d ch=%d  pw=%d ph=%d  rcw=%d, rch=%d",
//                          cWidth, cHeight, intrinsicWidth(), intrinsicHeight(), resizeCache.width(), resizeCache.height());
                QWMatrix matrix;
                matrix.scale( (float)(cWidth)/intrinsicWidth(),
                              (float)(cHeight)/intrinsicHeight() );
                resizeCache = pix.xForm( matrix );
                scaledrect.setWidth( ( cWidth*scaledrect.width() ) / intrinsicWidth() );
                scaledrect.setHeight( ( cHeight*scaledrect.height() ) / intrinsicHeight() );
//                   qDebug("resizeCache size: %d/%d", resizeCache.width(), resizeCache.height());
//                   qDebug("valid: %d/%d, scaled: %d/%d",
//                          image->valid_rect().width(), image->valid_rect().height(),
//                          scaledrect.width(), scaledrect.height());

                // sometimes scaledrect.width/height are off by one because
                // of rounding errors. if the image is fully loaded, we
                // make sure that we don't do unnecessary resizes during painting
                QSize s(scaledrect.size());
                if(image->valid_rect().size() == QSize( intrinsicWidth(), intrinsicHeight() )) // fully loaded
                    s = QSize(cWidth, cHeight);
                if(QABS(s.width() - cWidth) < 2) // rounding errors
                    s.setWidth(cWidth);
                if(resizeCache.size() != s)
                    resizeCache.resize(s);

                p->drawPixmap( QPoint( _tx + leftBorder + leftPad, _ty + topBorder + topPad),
                               resizeCache, scaledrect );
            }
            else
                p->drawPixmap( QPoint( _tx + leftBorder + leftPad, _ty + topBorder + topPad), resizeCache );
        }
        else
        {
            // we might be just about switching images
            // so pix contains the old one (we want to paint), but image->valid_rect is still invalid
            // so use intrinsic Size instead.
            // ### maybe no progressive loading for the second image ?
            QRect rect(image->valid_rect().isValid() ? image->valid_rect()
                       : QRect(0, 0, intrinsicWidth(), intrinsicHeight()));

            QPoint offs( _tx + leftBorder + leftPad, _ty + topBorder + topPad);
//             qDebug("normal paint rect %d/%d/%d/%d", rect.x(), rect.y(), rect.width(), rect.height());
//             rect = rect & QRect( 0 , y - offs.y() - 10, w, 10 + y + h  - offs.y());

//             qDebug("normal paint rect after %d/%d/%d/%d", rect.x(), rect.y(), rect.width(), rect.height());
//             qDebug("normal paint: offs.y(): %d, y: %d, diff: %d", offs.y(), y, y - offs.y());
//             qDebug("");

//           p->setClipRect(QRect(x,y,w,h));


//             p->drawPixmap( offs.x(), y, pix, rect.x(), rect.y(), rect.width(), rect.height() );
             p->drawPixmap(offs, pix, rect);

        }
    }
    if(style()->outlineWidth())
        paintOutline(p, _tx, _ty, width(), height(), style());
}

void RenderImage::layout()
{
    KHTMLAssert(needsLayout());
    KHTMLAssert( minMaxKnown() );

    short oldwidth = m_width;
    int oldheight = m_height;

    // minimum height
    m_height = image && image->isErrorImage() ? intrinsicHeight() : 0;

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
#if !APPLE_CHANGES
    // limit total size to not run out of memory when doing the xform call.
    if ( m_width * m_height > 2048*2048 ) {
	float scale = sqrt( m_width*m_height / ( 2048.*2048. ) );
	m_width = (int) (m_width/scale);
	m_height = (int) (m_height/scale);
    }
#endif
    
    if ( m_width != oldwidth || m_height != oldheight )
        resizeCache = QPixmap();

    setNeedsLayout(false);
}

void RenderImage::notifyFinished(CachedObject *finishedObj)
{
    if (image == finishedObj) {
        NodeImpl *node = element();
        if (node) {
            DocumentImpl *document = node->getDocument();
            if (document) {
                document->dispatchImageLoadEventSoon(this);
            }
        }
    }
}

void RenderImage::dispatchLoadEvent()
{
    if (!loadEventSent) {
        NodeImpl *node = element();
        if (node) {
            loadEventSent = true;
            if (image->isErrorImage()) {
                node->dispatchHTMLEvent(EventImpl::ERROR_EVENT, false, false);
            } else {
                node->dispatchHTMLEvent(EventImpl::LOAD_EVENT, false, false);
            }
        }
    }
}

#ifdef FIX_3109150
void RenderImage::reload()
{
    khtml::DocLoader *loader = element()->getDocument()->docLoader();
    KIO::CacheControl savedCachePolicy = loader->cachePolicy();
    loader->setCachePolicy(KIO::CC_Reload);
    updateFromElement();
    loader->setCachePolicy(savedCachePolicy);
}
#endif

void RenderImage::detach(RenderArena *arena)
{
    NodeImpl *node = element();
    if (node) {
        DocumentImpl *document = node->getDocument();
        if (document) {
            document->removeImage(this);
        }
    }
    RenderReplaced::detach(arena);
}

bool RenderImage::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty, bool inside)
{
    inside |= RenderReplaced::nodeAtPoint(info, _x, _y, _tx, _ty, inside);

    if (inside && element()) {
        int tx = _tx + m_x;
        int ty = _ty + m_y;
        
        HTMLImageElementImpl* i = element()->id() == ID_IMG ? static_cast<HTMLImageElementImpl*>(element()) : 0;
        HTMLMapElementImpl* map;
        if (i && i->getDocument()->isHTMLDocument() &&
            (map = static_cast<HTMLDocumentImpl*>(i->getDocument())->getMap(i->imageMap()))) {
            // we're a client side image map
            inside = map->mapMouseEvent(_x - tx, _y - ty, contentWidth(), contentHeight(), info);
            info.setInnerNonSharedNode(element());
        }
    }

    return inside;
}

void RenderImage::updateFromElement()
{
    DOMString attr;
    // Support images in OBJECT tags.
    if (element()->id() == ID_OBJECT) {
        attr = element()->getAttribute(ATTR_DATA);
    } else {
        attr = element()->getAttribute(ATTR_SRC);
    }
    
    // Treat a lack of src or empty string for src as no image at all, not the page itself
    // loaded as an image.
    CachedImage *new_image;
    if (attr.isEmpty()) {
        new_image = NULL;
    } else {
        new_image = element()->getDocument()->docLoader()->requestImage(khtml::parseURL(attr));
    }

    if(new_image && new_image != image && (!style() || !style()->contentData())) {
        loadEventSent = false;
        CachedImage *old_image = image;
        image = new_image;
        image->ref(this);
        berrorPic = image->isErrorImage();
        if (old_image) old_image->deref(this);
    }

    if (element()->id() == ID_INPUT)
        alt = static_cast<HTMLInputElementImpl*>(element())->altText();
    else if (element()->id() == ID_IMG)
        alt = static_cast<HTMLImageElementImpl*>(element())->altText();
}

bool RenderImage::isWidthSpecified() const
{
    switch (style()->width().type) {
        case Fixed:
        case Percent:
            return true;
        case Variable:
        case Relative:
        case Static:
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
        case Variable:
        case Relative:
        case Static:
            return false;
    }
    assert(false);
    return false;
}

short RenderImage::calcReplacedWidth() const
{
    // If height is specified and not width, preserve aspect ratio.
    if (isHeightSpecified() && !isWidthSpecified()) {
        if (intrinsicHeight() == 0){
            return 0;
        }
        return calcReplacedHeight() * intrinsicWidth() / intrinsicHeight();
    }
    return RenderReplaced::calcReplacedWidth();
}

int RenderImage::calcReplacedHeight() const
{
    // If width is specified and not height, preserve aspect ratio.
    if (isWidthSpecified() && !isHeightSpecified()) {
        if (intrinsicWidth() == 0){
            return 0;
        }
        return calcReplacedWidth() * intrinsicHeight() / intrinsicWidth();
    }
    return RenderReplaced::calcReplacedHeight();
}
