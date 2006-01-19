/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef RENDER_IMAGE_H
#define RENDER_IMAGE_H

#include "HTMLElementImpl.h"
#include "render_replaced.h"
#include "dom_string.h"
#include <qpixmap.h>

namespace WebCore {

class DocLoader;
class HTMLMapElementImpl;

class RenderImage : public RenderReplaced
{
public:
    RenderImage(NodeImpl*);
    virtual ~RenderImage();

    virtual const char *renderName() const { return "RenderImage"; }

    virtual bool isImage() const { return true; }
    virtual bool isImageButton() const { return false; }
    
    virtual void paint(PaintInfo& i, int tx, int ty);

    virtual void layout();

    virtual void setPixmap( const QPixmap &, const IntRect&, CachedImage *);

    const QPixmap& pixmap() const { return pix; }
    // don't even think about making this method virtual!
    HTMLElementImpl* element() const
        { return static_cast<HTMLElementImpl*>(RenderReplaced::element()); }


    // hook to keep RendeObject::m_inline() up to date
    virtual void setStyle(RenderStyle *style);
    void updateAltText();
    
    void setImage(CachedImage* image);
    CachedImage* getImage() const { return image; }
    
    virtual bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty,
                             HitTestAction hitTestAction);
    
    virtual int calcReplacedWidth() const;
    virtual int calcReplacedHeight() const;

    // Called to set generated content images (e.g., :before/:after generated images).
    void setContentObject(CachedObject* co);
    
    bool isDisplayingError() const { return berrorPic; }
    
    HTMLMapElementImpl* imageMap();

    void resetAnimation();

private:
    bool isWidthSpecified() const;
    bool isHeightSpecified() const;

    /*
     * Pointer to the image
     * If this pointer is 0L, that means that the picture could not be loaded
     * for some strange reason or that the image is waiting to be downloaded
     * from the internet for example.
     */

    QPixmap pix;

    /*
     * Cache for images that need resizing
     */
    QPixmap resizeCache;

    // text to display as long as the image isn't available
    DOMString alt;

    CachedImage *image;
    bool berrorPic : 1;
};

} //namespace

#endif
