/*
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
 */
#ifndef HTML_IMAGEIMPL_H
#define HTML_IMAGEIMPL_H

#include "html/html_inlineimpl.h"
#include "misc/khtmllayout.h"
#include "rendering/render_object.h"

#include <qregion.h>

namespace DOM {

class DOMString;

class HTMLImageElementImpl
    : public HTMLElementImpl
{
public:
    HTMLImageElementImpl(DocumentPtr *doc);

    ~HTMLImageElementImpl();

    virtual Id id() const;

    virtual void parseAttribute(AttributeImpl *);

    virtual void attach();
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void detach();

    long width() const;
    long height() const;

    bool isServerMap() const { return ( ismap && !usemap.length() );  }
    QImage currentImage() const;

    DOMString altText() const;

    DOMString imageMap() const { return usemap; }

protected:
    DOMString usemap;
    bool ismap;
    QString oldIdAttr;
    QString oldNameAttr;
};


//------------------------------------------------------------------

class HTMLAreaElementImpl : public HTMLAnchorElementImpl
{
public:

    enum Shape { Default, Poly, Rect, Circle, Unknown };

    HTMLAreaElementImpl(DocumentPtr *doc);
    ~HTMLAreaElementImpl();

    virtual Id id() const;

    virtual void parseAttribute(AttributeImpl *attr);

    bool isDefault() const { return shape==Default; }

    bool mapMouseEvent(int x_, int y_, int width_, int height_,
                       khtml::RenderObject::NodeInfo& info);

    virtual QRect getRect() const;

protected:
    QRegion getRegion(int width_, int height) const;
    QRegion region;
    khtml::Length* m_coords;
    int m_coordsLen;
    int lastw, lasth;
    Shape shape  : 3;
    bool nohref  : 1;
};


// -------------------------------------------------------------------------

class HTMLMapElementImpl : public HTMLElementImpl
{
public:
    HTMLMapElementImpl(DocumentPtr *doc);

    ~HTMLMapElementImpl();

    virtual Id id() const;

    virtual DOMString getName() const { return name; }

    virtual void parseAttribute(AttributeImpl *attr);

    bool mapMouseEvent(int x_, int y_, int width_, int height_,
                       khtml::RenderObject::NodeInfo& info);
private:

    QString name;
};


}; //namespace

#endif
