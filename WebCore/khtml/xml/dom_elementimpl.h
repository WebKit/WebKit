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
#ifndef _DOM_ELEMENTImpl_h_
#define _DOM_ELEMENTImpl_h_

#include "dom_nodeimpl.h"
#include "dom/dom_element.h"

namespace DOM {

class ElementImpl;
class DocumentImpl;
class NamedAttrMapImpl;

class AttrImpl : public NodeImpl
{
    friend class ElementImpl;
    friend class NamedAttrMapImpl;

public:
    AttrImpl();
    AttrImpl(DocumentPtr *doc, const DOMString &name);
    AttrImpl(DocumentPtr *doc, int id);
    AttrImpl(const AttrImpl &other);

    AttrImpl &operator = (const AttrImpl &other);
    ~AttrImpl();

    virtual const DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual bool isAttributeNode() const { return true; }

    DOMString name() const;
    bool specified() const { return m_specified; }

    virtual DOMString value() const;
    virtual void setValue( const DOMString &v );

    virtual DOMString nodeValue() const { return value(); }

    virtual void setNodeValue( const DOMString &, int &exceptioncode );

    virtual NodeImpl *parentNode() const;
    virtual NodeImpl *previousSibling() const;
    virtual NodeImpl *nextSibling() const;
    virtual NodeImpl *cloneNode ( bool deep, int &exceptioncode );

    virtual bool deleteMe();
    DOMStringImpl *val() { return _value; }
    virtual bool childAllowed( NodeImpl *newChild );
    virtual bool childTypeAllowed( unsigned short type );

protected:
    AttrImpl(const DOMString &name, const DOMString &value, DocumentPtr *doc);
    AttrImpl(int _id, const DOMString &value, DocumentPtr *doc);

    void setName(const DOMString &n);

    DOMStringImpl *_name;
    DOMStringImpl *_value;

    ElementImpl *_element;
public:
    unsigned char attrId;

};


class ElementImpl : public NodeBaseImpl
{
    friend class DocumentImpl;
    friend class NamedAttrMapImpl;
    friend class AttrImpl;

public:
    ElementImpl(DocumentPtr *doc);
    ~ElementImpl();

    virtual bool isInline() const;

    virtual unsigned short nodeType() const;
    virtual bool isElementNode() const { return true; }

    virtual bool isHTMLElement() const { return false; }

    DOMString tagName() const;

    DOMString getAttribute ( const DOMString &name ) const;

    void setAttribute ( const DOMString &name, const DOMString &value);
    void removeAttribute ( const DOMString &name );

    AttrImpl *getAttributeNode ( const DOMString &name );
    Attr setAttributeNode ( AttrImpl *newAttr, int &exceptioncode );
    Attr removeAttributeNode ( AttrImpl *oldAttr, int &exceptioncode );

    NodeListImpl *getElementsByTagName ( const DOMString &name );

    void normalize ( int &exceptioncode );

    virtual NodeImpl *cloneNode ( bool deep, int &exceptioncode );
    virtual NamedNodeMapImpl *attributes();

    /**
     * override this in subclasses if you need to parse
     * attributes. This is always called, whenever an attribute changed
      */
    virtual void parseAttribute(AttrImpl *) {}

    // not part of the DOM
    DOMString getAttribute ( int id );
    AttrImpl *getAttributeNode ( int index ) const;
    int getAttributeCount() const;
    void setAttribute ( int id, const DOMString &value );
    void setAttributeMap ( NamedAttrMapImpl* list );

    // State of the element.
    virtual QString state() { return QString::null; }

    virtual void attach();
    virtual void detach();
    virtual void recalcStyle();

    virtual bool prepareMouseEvent( int x, int y,
                                    int _tx, int _ty,
                                    MouseEvent *ev);
    virtual void setFocus(bool = true);
    virtual void setActive(bool = true);
    virtual void mouseEventHandler( MouseEvent */*ev*/, bool /*inside*/ ) {};
    virtual khtml::FindSelectionResult findSelectionNode( int _x, int _y, int _tx, int _ty,
                                                   DOM::Node & node, int & offset );
    virtual bool isSelectable() const;
    virtual bool childAllowed( NodeImpl *newChild );
    virtual bool childTypeAllowed( unsigned short type );

    virtual short tabIndex() const;
    virtual void setTabIndex( short );

    void createDecl();
    virtual DOM::CSSStyleDeclarationImpl *styleRules() {
      if (!m_styleDecls) createDecl();
      return m_styleDecls;
    }

    void dispatchAttrRemovalEvent(NodeImpl *attr);
    void dispatchAttrAdditionEvent(NodeImpl *attr);

protected: // member variables

    friend class NodeImpl;
    NamedAttrMapImpl *namedAttrMap;

    // map of default attributes. derived element classes are responsible
    // for setting this according to the corresponding element description
    // in the DTD
    virtual NamedAttrMapImpl* defaultMap() const;
    DOM::CSSStyleDeclarationImpl *m_styleDecls;
};


class XMLElementImpl : public ElementImpl
{

public:
    XMLElementImpl(DocumentPtr *doc, DOMStringImpl *_name);
    XMLElementImpl(DocumentPtr *doc, DOMStringImpl *_name, DOMStringImpl *_namespaceURI);
    ~XMLElementImpl();

    virtual const DOMString nodeName() const;
    virtual DOMString namespaceURI() const;
    virtual unsigned short id() const { return m_id; };
    virtual bool isXMLElementNode() const;

protected:
    DOMStringImpl *m_name;
    DOMStringImpl *m_namespaceURI;
    unsigned short m_id;
};



class NamedAttrMapImpl : public NamedNodeMapImpl
{
public:
    NamedAttrMapImpl(ElementImpl *e);
    virtual ~NamedAttrMapImpl();
    NamedAttrMapImpl &operator =(const NamedAttrMapImpl &other);

    unsigned long length(int &exceptioncode) const;
    unsigned long length() const; // ### remove?

    NodeImpl *getNamedItem ( const DOMString &name, int &exceptioncode ) const;
    NodeImpl *getNamedItem ( const DOMString &name ) const; // ### remove?
    AttrImpl *getIdItem ( int id ) const;

    Node setNamedItem ( const Node &arg, int &exceptioncode );
    Attr setIdItem ( AttrImpl *attr, int& exceptioncode );

    Node removeNamedItem ( const DOMString &name, int &exceptioncode );
    Attr removeIdItem ( int id );

    NodeImpl *item ( unsigned long index, int &exceptioncode ) const;
    NodeImpl *item ( unsigned long index ) const; // ### remove?

    Attr removeAttr( AttrImpl *oldAttr, int &exceptioncode );

    // only use this during parsing !
    void insertAttr(AttrImpl* newAtt);

    void detachFromElement();

protected:
    friend class ElementImpl;
    ElementImpl *element;
    AttrImpl **attrs;
    uint len;
    void clearAttrs();
};

}; //namespace

#endif
