/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
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
 * $Id$
 */
//#define DEBUG_LAYOUT

#include "render_image.h"

#include <qpixmap.h>
#include <qdrawutil.h>
#include <qpalette.h>

#include <kapp.h>
#include <kdebug.h>

#include "rendering/render_style.h"
#include "misc/helper.h"
#include "html/html_elementimpl.h"
#include "xml/dom2_eventsimpl.h"

using namespace DOM;
using namespace khtml;

// -------------------------------------------------------------------------

RenderImage::RenderImage(HTMLElementImpl *_element)
    : RenderReplaced(),
      pixSize(0, 0)
{
    setLayouted(false);
    setParsing(false);
    setSpecialObjects();
    image = 0;
    berrorPic = false;
    element = _element;
}

RenderImage::~RenderImage()
{
    if(image) image->deref(this);
}

void RenderImage::setStyle(RenderStyle* _style)
{
    RenderReplaced::setStyle(_style);
    // init RenderObject attributes
    setInline( style()->display()==INLINE );
}

void RenderImage::setPixmap( const QPixmap &p, const QRect& r, CachedImage *o, bool *manualUpdate )
{
    if(o != image) {
        RenderReplaced::setPixmap(p, r, o, manualUpdate);
        return;
    }

    if (manualUpdate && *manualUpdate)
    {
        updateSize();
        return;
    }

    if(o->isErrorImage())
    {
        pixSize.setWidth(QMAX(p.width()+8, pixSize.width()));
        pixSize.setHeight(QMAX(p.height()+8, pixSize.height()));
    }
    berrorPic = o->isErrorImage();

    bool needlayout = false;

    // Image dimensions have been changed, see what needs to be done
    if(o->pixmap_size() !=  pixSize)
    {
//          qDebug("image dimensions have been changed, old: %d/%d  new: %d/%d", pixSize.width(), pixSize.height(),
//               o->pixmap_size().width(), o->pixmap_size().height());

        if(!o->isErrorImage())
            pixSize = o->pixmap_size();

        // lets see if we need to relayout at all..
        int oldwidth = m_width;
        int oldheight = m_height;
        calcWidth();
        calcHeight();

        if(m_width != oldwidth || m_height != oldheight)
            needlayout = true;

        m_width = oldwidth;
        m_height = oldheight;
    }

    pix = p;

    if(needlayout)
    {
        setLayouted(false);
        setMinMaxKnown(false);

//         kdDebug( 6040 ) << "m_width: : " << m_width << " height: " << m_height << endl;
//         kdDebug( 6040 ) << "Image: size " << m_width << "/" << m_height << endl;
        // the updateSize() call should trigger a repaint too
        if (manualUpdate) {
           *manualUpdate = true;
        }
        else
        {
            updateSize();
        }
    }
    else
    {
        bool completeRepaint = !resizeCache.isNull();
        int cHeight = contentHeight();
        int scaledHeight = pixSize.height() ? ((o->valid_rect().height()*cHeight)/pixSize.height()) : 0;

        // don't bog down X server doing xforms
        if(completeRepaint && cHeight >= 5 &&  o->valid_rect().height() < pixSize.height() &&
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

void RenderImage::printObject(QPainter *p, int /*_x*/, int /*_y*/, int /*_w*/, int /*_h*/, int _tx, int _ty)
{
    // add offset for relative positioning
    if(isRelPositioned())
        relativePositionOffset(_tx, _ty);

    int cWidth = contentWidth();
    int cHeight = contentHeight();
    int leftBorder = borderLeft();
    int topBorder = borderTop();
    int leftPad = paddingLeft();
    int topPad = paddingTop();

    //kdDebug( 6040 ) << "    contents (" << contentWidth << "/" << contentHeight << ") border=" << borderLeft() << " padding=" << paddingLeft() << endl;
    if ( pix.isNull() || berrorPic)
    {
        if(cWidth > 2 && cHeight > 2)
        {
            //qDebug("qDrawShadePanel %d/%d/%d/%d", _tx + leftBorder, _ty + topBorder, cWidth, cHeight);
#ifdef _KWQ_
            p->setPen (Qt::lightGray);
            p->setBrush (Qt::NoBrush);
            p->drawRect (_tx, _ty, cWidth, cHeight);
#else
            qDrawShadePanel( p, _tx + leftBorder + leftPad, _ty + topBorder + topPad, cWidth, cHeight,
                             KApplication::palette().inactive(), true, 1 );
            if(berrorPic && !pix.isNull() && (cWidth >= pix.width()+4) && (cHeight >= pix.height()+4) )
            {
                QRect r(pix.rect());
                r = r.intersect(QRect(0, 0, cWidth-4, cHeight-4));
                p->drawPixmap( QPoint( _tx + leftBorder + leftPad+2, _ty + topBorder + topPad+2), pix, r );
            }
            if(!alt.isEmpty())
            {
                QString text = alt.string();
                p->setFont(style()->font());
                p->setPen( style()->color() );
                int ax = _tx + leftBorder + QMAX(5, leftPad);
                int ay = _ty + topBorder + QMAX(5, topPad);
                int ah = cHeight - QMAX(10, leftPad + paddingRight());
                int aw = cWidth - QMAX(10, topPad + paddingBottom());
                QFontMetrics fm = fontMetrics(style()->font());
                if (aw>15 && ah>fm.height())
                    p->drawText(ax, ay, aw, ah , Qt::WordBreak, text );
            }
#endif
        }
    }
    else if (image && !image->isTransparent())
    {
        if ( (cWidth != pixSize.width() ||  cHeight != pixSize.height() ) &&
             pix.width() > 0 && pix.height() > 0 && image->valid_rect().isValid())
        {
            if (resizeCache.isNull() && cWidth && cHeight)
            {
                QRect scaledrect(image->valid_rect());
//                 kdDebug(6040) << "time elapsed: " << dt->elapsed() << endl;
//                  kdDebug( 6040 ) << "have to scale: " << endl;
//                  qDebug("cw=%d ch=%d  pw=%d ph=%d  rcw=%d, rch=%d",
//                          cWidth, cHeight, pixSize.width(), pixSize.height(), resizeCache.width(), resizeCache.height());
                QWMatrix matrix;
                matrix.scale( (float)(cWidth)/pixSize.width(),
                              (float)(cHeight)/pixSize.height() );
                resizeCache = pix.xForm( matrix );
                scaledrect.setWidth( ( cWidth*scaledrect.width() ) / pixSize.width() );
                scaledrect.setHeight( ( cHeight*scaledrect.height() ) / pixSize.height() );
//                   qDebug("resizeCache size: %d/%d", resizeCache.width(), resizeCache.height());
//                   qDebug("valid: %d/%d, scaled: %d/%d",
//                          image->valid_rect().width(), image->valid_rect().height(),
//                          scaledrect.width(), scaledrect.height());

                // sometimes scaledrect.width/height are off by one because
                // of rounding errors. if the image is fully loaded, we
                // make sure that we don't do unnecessary resizes during painting
                QSize s(scaledrect.size());
                if(image->valid_rect().size() == pixSize) // fully loaded
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
            // so use pixSize instead.
            // ### maybe no progressive loading for the second image ?
            QRect rect(image->valid_rect().isValid() ? image->valid_rect() : QRect(0, 0, pixSize.width(), pixSize.height()));

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
        printOutline(p, _tx, _ty, width(), height(), style());
}

void RenderImage::layout()
{
    if(layouted()) return;

    short oldwidth = m_width;
    int oldheight = m_height;

    // minimum height
    m_height = image && image->isErrorImage() ? intrinsicHeight() : 0;

    calcWidth();
    calcHeight();

    if ( m_width != oldwidth || m_height != oldheight )
        resizeCache = QPixmap();

    if ( !style()->width().isPercent() )
        setLayouted();
}

void RenderImage::setImageUrl(DOMString url, DOMString baseUrl, DocLoader *docLoader)
{
    CachedImage *new_image = docLoader->requestImage(url, baseUrl);
    if(new_image && new_image != image) {
        if(image) image->deref(this);
        image = new_image;
        image->ref(this);
        berrorPic = image->isErrorImage();
    }
}

void RenderImage::setAlt(DOM::DOMString text)
{
    alt = text;
}

short RenderImage::intrinsicWidth() const
{
    return pixSize.width();
}

int RenderImage::intrinsicHeight() const
{
    return pixSize.height();
}

void RenderImage::notifyFinished(CachedObject *finishedObj)
{
    if ( image == finishedObj )
        element->dispatchHTMLEvent(EventImpl::LOAD_EVENT,false,false);
}
