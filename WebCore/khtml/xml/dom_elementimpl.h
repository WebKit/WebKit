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
class AtomicStringList;

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
    AttributeImpl(NodeImpl::Id id, const AtomicString& value)
        : m_id(id), _value(value), _impl(0)
        { };
    virtual ~AttributeImpl() {};
    
    MAIN_THREAD_ALLOCATED;

    const AtomicString& value() const { return _value; }
    const AtomicString& prefix() const { return _prefix; }
    NodeImpl::Id id() const { return m_id; }
    AttrImpl* attrImpl() const { return _impl; }

    bool isNull() const { return _value.isNull(); }
    bool isEmpty() const { return _value.isEmpty(); }
    
    virtual AttributeImpl* clone(bool preserveDecl=true) const;

private:
    void setValue(const AtomicString& value) {
        _value = value;
    }
    void setPrefix(const AtomicString& prefix) {
        _prefix = prefix;
    }
    void allocateImpl(ElementImpl* e);

protected:
    NodeImpl::Id m_id;
    AtomicString _prefix;
    AtomicString _value;
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

    // Used to quickly determine whether or not an element has a given CSS class.
    virtual const AtomicStringList* getClassList() const;
    const AtomicString& getIDAttribute() const;
    const AtomicString& getAttribute( NodeImpl::Id id ) const;
    const AtomicString& getAttribute(const DOMString& localName) const { return getAttributeNS(QString::null, localName); }
    const AtomicString& getAttributeNS(const DOMString &namespaceURI,
                                       const DOMString &localName) const;
    void setAttribute( NodeImpl::Id id, DOMStringImpl* value, int &exceptioncode );
    void removeAttribute( NodeImpl::Id id, int &exceptioncode );
    bool hasAttributes() const;
    
    DOMString prefix() const { return m_prefix; }
    void setPrefix(const DOMString &_prefix, int &exceptioncode );

    // DOM methods overridden from  parent classes
    virtual DOMString tagName() const;
    virtual unsigned short nodeType() const;
    virtual NodeImpl *cloneNode ( bool deep );
    virtual DOMString nodeName() const;
    virtual bool isElementNode() const { return true; }
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    // convenience methods which ignore exceptions
    void setAttribute (NodeImpl::Id id, const DOMString &value);

    NamedAttrMapImpl* attributes(bool readonly = false) const;

    // This method is called whenever an attribute is added, changed or removed.
    virtual void attributeChanged(AttributeImpl* attr, bool preserveDecls = false) {}

    // not part of the DOM
    void setAttributeMap ( NamedAttrMapImpl* list );

    // State of the element.
    virtual QString state() { return QString::null; }

    virtual void attach();
    virtual khtml::RenderStyle *styleForRenderer(khtml::RenderObject *parent);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void recalcStyle( StyleChange = NoChange );

    virtual void mouseEventHandler( MouseEvent */*ev*/, bool /*inside*/ ) {};
    virtual bool childAllowed( NodeImpl *newChild );
    virtual bool childTypeAllowed( unsigned short type );
 
    virtual AttributeImpl* createAttribute(NodeImpl::Id id, DOMStringImpl* value);
    
    void dispatchAttrRemovalEvent(AttributeImpl *attr);
    void dispatchAttrAdditionEvent(AttributeImpl *attr);

    virtual void accessKeyAction(bool sendToAnyEvent) { };

    virtual DOMString toString() const;

    virtual bool isURLAttribute(AttributeImpl *attr) const;
    
#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

#if APPLE_CHANGES
    static Element createInstance(ElementImpl *impl);
#endif

#ifndef NDEBUG
    virtual void formatForDebugger(char *buffer, unsigned length) const;
#endif

protected:
    virtual void createAttributeMap() const;
    DOMString openTagStartToString() const;

private:
    void updateId(const AtomicString& oldId, const AtomicString& newId);

    virtual void updateStyleAttributeIfNeeded() const {};

protected: // member variables
    mutable NamedAttrMapImpl *namedAttrMap;
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
    unsigned long length() const { return len; }

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

    virtual bool isHTMLAttributeMap() const;

    const AtomicString& id() const { return m_id; }
    void setID(const AtomicString& _id) { m_id = _id; }
    
protected:
    // this method is internal, does no error checking at all
    void addAttribute(AttributeImpl* newAttribute);
    // this method is internal, does no error checking at all
    void removeAttribute(NodeImpl::Id id);
    virtual void clearAttributes();
    void detachFromElement();

protected:
    ElementImpl *element;
    AttributeImpl **attrs;
    uint len;
    AtomicString m_id;
};

}; //namespace

#if APPLE_CHANGES
#undef id
#endif

#endif
