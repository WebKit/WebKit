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

#include "CachedImage.h"
#include "Image.h"
#include "Path.h"
#include "html_inlineimpl.h"
#include "render_object.h"

namespace WebCore {

class HTMLCollectionImpl;
class HTMLFormElementImpl;
class Image;
class String;

struct Length;

class HTMLImageLoader : public CachedObjectClient {
public:
    HTMLImageLoader(ElementImpl*);
    virtual ~HTMLImageLoader();

    virtual void updateFromElement();

    void dispatchLoadEvent();

    ElementImpl* element() const { return m_element; }
    bool imageComplete() const { return m_imageComplete; }
    CachedImage* image() const { return m_image; }

    // CachedObjectClient API
    virtual void notifyFinished(CachedObject*);

protected:
    void setLoadingImage(CachedImage*);

private:
    ElementImpl* m_element;
    CachedImage* m_image;
    bool m_firedLoad : 1;
    bool m_imageComplete : 1;
};

class HTMLImageElementImpl : public HTMLElementImpl {
    friend class HTMLFormElementImpl;
public:
    HTMLImageElementImpl(DocumentImpl*, HTMLFormElementImpl* = 0);
    HTMLImageElementImpl(const QualifiedName& tagName, DocumentImpl*);
    ~HTMLImageElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl*);

    virtual void attach();
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    int width(bool ignorePendingStylesheets = false) const;
    int height(bool ignorePendingStylesheets = false) const;

    bool isServerMap() const { return ismap && usemap.isEmpty(); }

    String altText() const;

    String imageMap() const { return usemap; }
    
    virtual bool isURLAttribute(AttributeImpl*) const;

    Image::CompositeOperator compositeOperator() const { return Image::compositeOperatorFromString(_compositeOperator.ascii()); }

    CachedImage* cachedImage() const { return m_imageLoader.image(); }
    
    String name() const;
    void setName(const String&);

    String align() const;
    void setAlign(const String&);

    String alt() const;
    void setAlt(const String&);

    int border() const;
    void setBorder(int);

    void setHeight(int);

    int hspace() const;
    void setHspace(int);

    bool isMap() const;
    void setIsMap(bool);

    String longDesc() const;
    void setLongDesc(const String&);

    String src() const;
    void setSrc(const String&);

    String useMap() const;
    void setUseMap(const String&);

    int vspace() const;
    void setVspace(int);

    void setWidth(int);

    int x() const;
    int y() const;

    bool complete() const;

protected:
    HTMLImageLoader m_imageLoader;
    String usemap;
    bool ismap;
    HTMLFormElementImpl* m_form;
    String oldNameAttr;
    QString _compositeOperator;
};

//------------------------------------------------------------------

class HTMLAreaElementImpl : public HTMLAnchorElementImpl {
public:
    enum Shape { Default, Poly, Rect, Circle, Unknown };

    HTMLAreaElementImpl(DocumentImpl*);
    ~HTMLAreaElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual void parseMappedAttribute(MappedAttributeImpl*);

    bool isDefault() const { return m_shape == Default; }

    bool mapMouseEvent(int x, int y, int width, int height, RenderObject::NodeInfo& info);

    virtual IntRect getRect(RenderObject*) const;

    String accessKey() const;
    void setAccessKey(const String&);

    String alt() const;
    void setAlt(const String&);

    String coords() const;
    void setCoords(const String&);

    String href() const;
    void setHref(const String&);

    bool noHref() const;
    void setNoHref(bool);

    String shape() const;
    void setShape(const String&);

    int tabIndex() const;
    void setTabIndex(int);

    String target() const;
    void setTarget(const String&);

protected:
    Path getRegion(int width, int height) const;
    Path region;
    Length* m_coords;
    int m_coordsLen;
    int lastw, lasth;
    Shape m_shape;
};

// -------------------------------------------------------------------------

class HTMLMapElementImpl : public HTMLElementImpl {
public:
    HTMLMapElementImpl(DocumentImpl*);
    ~HTMLMapElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }
    virtual bool checkDTD(const NodeImpl*);

    const AtomicString& getName() const { return m_name; }

    virtual void parseMappedAttribute(MappedAttributeImpl*);

    bool mapMouseEvent(int x, int y, int width, int height, RenderObject::NodeInfo&);

    PassRefPtr<HTMLCollectionImpl> areas();

    String name() const;
    void setName(const String&);

private:
    AtomicString m_name;
};

} //namespace

#endif
