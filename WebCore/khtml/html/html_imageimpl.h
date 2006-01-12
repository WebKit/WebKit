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
#include "CachedImage.h"

#include <qregion.h>
#include <qmap.h>
#include <qpixmap.h>

namespace khtml {
    class CachedObjectClient;
}

namespace DOM {

class DOMString;

class HTMLCollectionImpl;
class HTMLFormElementImpl;
    
class HTMLImageLoader: public khtml::CachedObjectClient {
public:
    HTMLImageLoader(ElementImpl* elt);
    virtual ~HTMLImageLoader();

    virtual void updateFromElement();

    void dispatchLoadEvent();

    ElementImpl* element() const { return m_element; }
    bool imageComplete() const { return m_imageComplete; }
    khtml::CachedImage* image() const { return m_image; }

    // CachedObjectClient API
    virtual void notifyFinished(khtml::CachedObject *finishedObj);

protected:

    void setLoadingImage(CachedImage *loadingImage);

private:
    ElementImpl* m_element;
    khtml::CachedImage* m_image;
    bool m_firedLoad : 1;
    bool m_imageComplete : 1;
};

class HTMLImageElementImpl : public HTMLElementImpl
{
    friend class HTMLFormElementImpl;
public:
    HTMLImageElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f = 0);
    HTMLImageElementImpl(const QualifiedName& tagName, DocumentImpl* doc);
    ~HTMLImageElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *);

    virtual void attach();
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    int width(bool ignorePendingStylesheets = false) const;
    int height(bool ignorePendingStylesheets = false) const;

    bool isServerMap() const { return ( ismap && !usemap.length() );  }

    DOMString altText() const;

    DOMString imageMap() const { return usemap; }
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

    QString compositeOperator() const { return _compositeOperator; }

    const QPixmap &pixmap() { return m_imageLoader.image()->pixmap(); }
    
    DOMString name() const;
    void setName( const DOMString & );

    DOMString align() const;
    void setAlign( const DOMString & );

    DOMString alt() const;
    void setAlt( const DOMString & );

    int border() const;
    void setBorder( int );

    void setHeight( int );

    int hspace() const;
    void setHspace( int );

    bool isMap() const;
    void setIsMap( bool );

    DOMString longDesc() const;
    void setLongDesc( const DOMString & );

    DOMString src() const;
    void setSrc( const DOMString & );

    DOMString useMap() const;
    void setUseMap( const DOMString & );

    int vspace() const;
    void setVspace( int );

    void setWidth( int );

    int x() const;
    int y() const;

protected:
    HTMLImageLoader m_imageLoader;
    DOMString usemap;
    bool ismap;
    HTMLFormElementImpl *m_form;
    DOMString oldNameAttr;
    QString _compositeOperator;
};


//------------------------------------------------------------------

class HTMLAreaElementImpl : public HTMLAnchorElementImpl
{
public:

    enum Shape { Default, Poly, Rect, Circle, Unknown };

    HTMLAreaElementImpl(DocumentImpl *doc);
    ~HTMLAreaElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    bool isDefault() const { return m_shape == Default; }

    bool mapMouseEvent(int x_, int y_, int width_, int height_,
                       khtml::RenderObject::NodeInfo& info);

    virtual QRect getRect(khtml::RenderObject* obj) const;

    DOMString accessKey() const;
    void setAccessKey( const DOMString & );

    DOMString alt() const;
    void setAlt( const DOMString & );

    DOMString coords() const;
    void setCoords( const DOMString & );

    DOMString href() const;
    void setHref( const DOMString & );

    bool noHref() const;
    void setNoHref( bool );

    DOMString shape() const;
    void setShape( const DOMString & );

    int tabIndex() const;
    void setTabIndex( int );

    DOMString target() const;
    void setTarget( const DOMString & );

protected:
    QRegion getRegion(int width_, int height) const;
    QRegion region;
    khtml::Length* m_coords;
    int m_coordsLen;
    int lastw, lasth;
    Shape m_shape;
};


// -------------------------------------------------------------------------

class HTMLMapElementImpl : public HTMLElementImpl
{
public:
    HTMLMapElementImpl(DocumentImpl *doc);
    ~HTMLMapElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }
    virtual bool checkDTD(const NodeImpl* newChild);

    virtual DOMString getName() const { return m_name; }

    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    bool mapMouseEvent(int x_, int y_, int width_, int height_,
                       khtml::RenderObject::NodeInfo& info);

    RefPtr<HTMLCollectionImpl> areas();

    DOMString name() const;
    void setName( const DOMString & );

private:
    DOMString m_name;
};

} //namespace

#endif
