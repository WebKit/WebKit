/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#ifndef HTML_IMAGEIMPL_H
#define HTML_IMAGEIMPL_H

#include "html/html_inlineimpl.h"
#include "misc/khtmllayout.h"
#include "rendering/render_object.h"

#include <loader.h>

#include <qregion.h>
#include <qmap.h>
#include <qpixmap.h>

namespace khtml {
    class CachedImage;
    class CachedObjectClient;
}

namespace DOM {

class DOMString;

class HTMLFormElementImpl;
    
class HTMLImageLoader: public khtml::CachedObjectClient {
public:
    HTMLImageLoader(ElementImpl* elt);
    virtual ~HTMLImageLoader();

    void updateFromElement();

    void dispatchLoadEvent();

    ElementImpl* element() const { return m_element; }
    bool imageComplete() const { return m_imageComplete; }
    khtml::CachedImage* image() const { return m_image; }

    // CachedObjectClient API
    virtual void notifyFinished(khtml::CachedObject *finishedObj);

private:
    ElementImpl* m_element;
    khtml::CachedImage* m_image;
    bool m_firedLoad : 1;
    bool m_imageComplete : 1;
};

class HTMLImageElementImpl
    : public HTMLElementImpl
{
    friend class HTMLFormElementImpl;
public:
    HTMLImageElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f = 0);
    ~HTMLImageElementImpl();

    virtual Id id() const;

    virtual bool mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const;
    virtual void parseHTMLAttribute(HTMLAttributeImpl *);

    virtual void attach();
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void detach();

    long width(bool ignorePendingStylesheets = false) const;
    long height(bool ignorePendingStylesheets = false) const;

    bool isServerMap() const { return ( ismap && !usemap.length() );  }
    QImage currentImage() const;

    DOMString altText() const;

    DOMString imageMap() const { return usemap; }
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

#if APPLE_CHANGES
    QString compositeOperator() const { return _compositeOperator; }
    const QPixmap &pixmap() { return m_imageLoader.image()->pixmap(); }
#endif
    
protected:
    HTMLImageLoader m_imageLoader;
    DOMString usemap;
    bool ismap;
    HTMLFormElementImpl *m_form;
    QString oldIdAttr;
    QString oldNameAttr;
#if APPLE_CHANGES
    QString _compositeOperator;
#endif
};


//------------------------------------------------------------------

class HTMLAreaElementImpl : public HTMLAnchorElementImpl
{
public:

    enum Shape { Default, Poly, Rect, Circle, Unknown };

    HTMLAreaElementImpl(DocumentPtr *doc);
    ~HTMLAreaElementImpl();

    virtual Id id() const;

    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);

    bool isDefault() const { return shape==Default; }

    bool mapMouseEvent(int x_, int y_, int width_, int height_,
                       khtml::RenderObject::NodeInfo& info);

    virtual QRect getRect(khtml::RenderObject* obj) const;

protected:
    QRegion getRegion(int width_, int height) const;
    QRegion region;
    khtml::Length* m_coords;
    int m_coordsLen;
    int lastw, lasth;
    Shape shape;
};


// -------------------------------------------------------------------------

class HTMLMapElementImpl : public HTMLElementImpl
{
public:
    HTMLMapElementImpl(DocumentPtr *doc);

    ~HTMLMapElementImpl();

    virtual Id id() const;

    virtual DOMString getName() const { return name; }

    virtual void parseHTMLAttribute(HTMLAttributeImpl *attr);

    bool mapMouseEvent(int x_, int y_, int width_, int height_,
                       khtml::RenderObject::NodeInfo& info);
private:

    DOMString name;
};


}; //namespace

#endif
