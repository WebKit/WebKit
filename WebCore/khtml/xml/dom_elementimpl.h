/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
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
#ifndef _DOM_ELEMENTImpl_h_
#define _DOM_ELEMENTImpl_h_

#include "dom_nodeimpl.h"
#include "dom/dom_element.h"
#include "xml/dom_stringimpl.h"
#include "misc/shared.h"

#if APPLE_CHANGES
#ifdef __OBJC__
#define id id_AVOID_KEYWORD
#endif
#endif

namespace khtml {
    class CSSStyleSelector;
}

namespace DOM {

class ElementImpl;
class DocumentImpl;
class NamedAttrMapImpl;

// this has no counterpart in DOM, purely internal
// representation of the nodevalue of an Attr.
// the actual Attr (AttrImpl) with its value as textchild
// is only allocated on demand by the DOM bindings.
// Any use of AttrImpl inside khtml should be avoided.
class AttributeImpl : public khtml::Shared<AttributeImpl>
{
    friend class NamedAttrMapImpl;
    friend class ElementImpl;
    friend class AttrImpl;

public:
    // null value is forbidden !
    AttributeImpl(NodeImpl::Id id, DOMStringImpl* value)
        : m_id(id), _prefix(0), _value(value), _impl(0)
        { _value->ref(); };
    ~AttributeImpl() {
        if (_prefix) _prefix->deref();
        if (_value) _value->deref();
        // assert : _impl == 0
    }

    DOMString value() const { return _value; }
    DOMStringImpl* val() const { return _value; }
    DOMStringImpl* prefix() const { return _prefix; }
    NodeImpl::Id id() const { return m_id; }
    AttrImpl* attrImpl() const { return _impl; }

private:
    // null pointers can never happen here
    void setValue(DOMStringImpl* value) {
        _value->deref();
        _value = value;
        _value->ref();
    }
    void setPrefix(DOMStringImpl* prefix) {
        if (_prefix) _prefix->deref();
        _prefix = prefix;
        if (_prefix) _prefix->ref();
    }
    void allocateImpl(ElementImpl* e);

protected:
    NodeImpl::Id m_id;
    DOMStringImpl *_prefix;
    DOMStringImpl *_value;
    AttrImpl* _impl;
};

// Attr can have Text and EntityReference children
// therefore it has to be a fullblown Node. The plan
// is to dynamically allocate a textchild and store the
// resulting nodevalue in the AttributeImpl upon
// destruction. however, this is not yet implemented.
class AttrImpl : public NodeBaseImpl
{
    friend class ElementImpl;
    friend class NamedAttrMapImpl;

public:
    AttrImpl(ElementImpl* element, DocumentPtr* docPtr, AttributeImpl* a);
    ~AttrImpl();

private:
    AttrImpl(const AttrImpl &other);
    AttrImpl &operator = (const AttrImpl &other);
public:

    // DOM methods & attributes for Attr
    bool specified() const { return m_specified; }
    ElementImpl* ownerElement() const { return m_element; }
    AttributeImpl* attrImpl() const { return m_attribute; }

    //DOMString value() const;
    void setValue( const DOMString &v, int &exceptioncode );

    // DOM methods overridden from  parent classes
    virtual DOMString nodeName() const;
    virtual unsigned short nodeType() const;
    virtual DOMString prefix() const;
    virtual void setPrefix(const DOMString &_prefix, int &exceptioncode );

    virtual DOMString nodeValue() const;
    virtual void setNodeValue( const DOMString &, int &exceptioncode );
    virtual NodeImpl *cloneNode ( bool deep );

    // Other methods (not part of DOM)
    virtual bool isAttributeNode() const { return true; }
    virtual bool childAllowed( NodeImpl *newChild );
    virtual bool childTypeAllowed( unsigned short type );

    virtual DOMString toString() const;

#if APPLE_CHANGES
    static Attr createInstance(AttrImpl *impl);
#endif

protected:
    ElementImpl* m_element;
    AttributeImpl* m_attribute;
};


class ElementImpl : public NodeBaseImpl
{
    friend class DocumentImpl;
    friend class NamedAttrMapImpl;
    friend class AttrImpl;
    friend class NodeImpl;
    friend class khtml::CSSStyleSelector;
public:
    ElementImpl(DocumentPtr *doc);
    ~ElementImpl();

    DOMString getAttribute( NodeImpl::Id id ) const;
    void setAttribute( NodeImpl::Id id, DOMStringImpl* value, int &exceptioncode );
    void removeAttribute( NodeImpl::Id id, int &exceptioncode );

    DOMString prefix() const { return m_prefix; }
    void setPrefix(const DOMString &_prefix, int &exceptioncode );

    // DOM methods overridden from  parent classes
    virtual DOMString tagName() const;
    virtual unsigned short nodeType() const;
    virtual NodeImpl *cloneNode ( bool deep );
    virtual DOMString nodeName() const;
    virtual bool isElementNode() const { return true; }

    // convenience methods which ignore exceptions
    void setAttribute (NodeImpl::Id id, const DOMString &value);

    NamedAttrMapImpl* attributes(bool readonly = false) const
    {
        if (!readonly && !namedAttrMap) createAttributeMap();
        return namedAttrMap;
    }

    //This is always called, whenever an attribute changed
    virtual void parseAttribute(AttributeImpl *) {}

    // not part of the DOM
    void setAttributeMap ( NamedAttrMapImpl* list );

    // State of the element.
    virtual QString state() { return QString::null; }

    virtual void attach();
    virtual void detach();
    virtual khtml::RenderStyle *styleForRenderer(khtml::RenderObject *parent);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void recalcStyle( StyleChange = NoChange );

    virtual void mouseEventHandler( MouseEvent */*ev*/, bool /*inside*/ ) {};
    virtual bool childAllowed( NodeImpl *newChild );
    virtual bool childTypeAllowed( unsigned short type );

    DOM::CSSStyleDeclarationImpl *styleRules() {
      if (!m_styleDecls) createDecl();
      return m_styleDecls;
    }
    // used by table cells to share style decls created by the enclosing table.
    virtual DOM::CSSStyleDeclarationImpl* getAdditionalStyleDecls() { return 0; }
    
    void dispatchAttrRemovalEvent(AttributeImpl *attr);
    void dispatchAttrAdditionEvent(AttributeImpl *attr);

    virtual void accessKeyAction() {};

    virtual DOMString toString() const;

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

#if APPLE_CHANGES
    static Element createInstance(ElementImpl *impl);
#endif
protected:
    void createAttributeMap() const;
    void createDecl();
    DOMString openTagStartToString() const;

private:
    // map of default attributes. derived element classes are responsible
    // for setting this according to the corresponding element description
    // in the DTD
    virtual NamedAttrMapImpl* defaultMap() const;

    void updateId(DOMStringImpl* oldId, DOMStringImpl* newId);

protected: // member variables
    mutable NamedAttrMapImpl *namedAttrMap;

    DOM::CSSStyleDeclarationImpl *m_styleDecls;
    DOMStringImpl *m_prefix;
};


class XMLElementImpl : public ElementImpl
{

public:
    XMLElementImpl(DocumentPtr *doc, DOMStringImpl *_tagName);
    XMLElementImpl(DocumentPtr *doc, DOMStringImpl *_qualifiedName, DOMStringImpl *_namespaceURI);
    ~XMLElementImpl();

    // DOM methods overridden from  parent classes

    virtual DOMString localName() const;
    virtual NodeImpl *cloneNode ( bool deep );

    // Other methods (not part of DOM)
    virtual bool isXMLElementNode() const { return true; }
    virtual Id id() const { return m_id; }

protected:
    Id m_id;
};

// the map of attributes of an element
class NamedAttrMapImpl : public NamedNodeMapImpl
{
    friend class ElementImpl;
public:
    NamedAttrMapImpl(ElementImpl *e);
    virtual ~NamedAttrMapImpl();
    NamedAttrMapImpl(const NamedAttrMapImpl&);
    NamedAttrMapImpl &operator =(const NamedAttrMapImpl &other);

    // DOM methods & attributes for NamedNodeMap
    virtual AttrImpl *getNamedItem ( NodeImpl::Id id ) const;
    virtual Node removeNamedItem ( NodeImpl::Id id, int &exceptioncode );
    virtual Node setNamedItem ( NodeImpl* arg, int &exceptioncode );

    virtual AttrImpl *item ( unsigned long index ) const;
    virtual unsigned long length(  ) const;

    // Other methods (not part of DOM)
    virtual NodeImpl::Id mapId(const DOMString& namespaceURI,  const DOMString& localName,  bool readonly);
    AttributeImpl* attributeItem(unsigned long index) const { return attrs ? attrs[index] : 0; }
    AttributeImpl* getAttributeItem(NodeImpl::Id id) const;
    virtual bool isReadOnly() { return element ? element->isReadOnly() : false; }

    // used during parsing: only inserts if not already there
    // no error checking!
    void insertAttribute(AttributeImpl* newAttribute) {
        if (!getAttributeItem(newAttribute->id()))
            addAttribute(newAttribute);
        else
            newAttribute->deref();
    }

private:
    // this method is internal, does no error checking at all
    void addAttribute(AttributeImpl* newAttribute);
    // this method is internal, does no error checking at all
    void removeAttribute(NodeImpl::Id id);
    void clearAttributes();
    void detachFromElement();

protected:
    ElementImpl *element;
    AttributeImpl **attrs;
    uint len;
};

}; //namespace

#if APPLE_CHANGES
#undef id
#endif

#endif
