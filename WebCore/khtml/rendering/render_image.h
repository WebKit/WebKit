/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
#ifndef RENDER_IMAGE_H
#define RENDER_IMAGE_H

#include "dtd.h"
#include "render_replaced.h"

#include <qmap.h>
#include <qpixmap.h>

#include <dom/dom_string.h>

namespace DOM {
    class HTMLElementImpl;
}

namespace khtml {

class DocLoader;

class RenderImage : public RenderReplaced
{
public:
    RenderImage(DOM::HTMLElementImpl *_element);
    virtual ~RenderImage();

    virtual const char *renderName() const { return "RenderImage"; }

    virtual bool isRendered() const { return true; }

    virtual void printObject( QPainter *p, int /*x*/, int /*y*/, int /*w*/, int /*h*/, int tx, int ty);

    virtual void layout();

    virtual void setPixmap( const QPixmap &, const QRect&, CachedImage *, bool *manualUpdate);

    QPixmap pixmap() const { return pix; }

    void setImageUrl(DOM::DOMString url, DOM::DOMString baseUrl, DocLoader *docLoader);
    void setAlt(DOM::DOMString text);

    virtual short intrinsicWidth() const;
    virtual int intrinsicHeight() const;

    // hook to keep RendeObject::m_inline() up to date
    virtual void setStyle(RenderStyle *style);

    virtual void notifyFinished(CachedObject *finishedObj);

private:
    /*
     * Pointer to the image
     * If this pointer is 0L, that means that the picture could not be loaded
     * for some strange reason or that the image is waiting to be downloaded
     * from the internet for example.
     */

    QPixmap pix;
    QSize  pixSize;

    /*
     * Cache for images that need resizing
     */
    QPixmap resizeCache;

    // text to display as long as the image isn't available
    DOM::DOMString alt;

    CachedImage *image;
    bool berrorPic : 1;

    DOM::HTMLElementImpl *element;
};


}; //namespace

#endif
