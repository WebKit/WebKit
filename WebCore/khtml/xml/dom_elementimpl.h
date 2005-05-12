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
#include "xml/dom_stringimpl.h"
#include "misc/shared.h"
#include "css/css_valueimpl.h"

#if APPLE_CHANGES
#ifdef __OBJC__
#define id id_AVOID_KEYWORD
#endif
#endif

#include "dom_atomicstringlist.h"

namespace khtml {
    class CSSStyleSelector;
}

namespace DOM {

class AtomicStringList;
class DocumentImpl;
class CSSStyleDeclarationImpl;
class ElementImpl;
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
class AttrImpl : public ContainerNodeImpl
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
    DOMString name() const;
    bool specified() const { return m_specified; }
    ElementImpl* ownerElement() const { return m_element; }
    AttributeImpl* attrImpl() const { return m_attribute; }

    DOMString value() const;
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

protected:
    ElementImpl* m_element;
    AttributeImpl* m_attribute;
};


class ElementImpl : public ContainerNodeImpl
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
    const AtomicString& getAttribute(Id id ) const;
    void setAttribute( Id id, DOMStringImpl* value, int &exceptioncode );
    void removeAttribute( Id id, int &exceptioncode );

    bool hasAttributes() const;

    bool hasAttribute(const DOMString &name) const { return hasAttributeNS(DOMString(), name); }
    bool hasAttributeNS(const DOMString &namespaceURI, const DOMString &localName) const;

    const AtomicString& getAttribute(const DOMString& name) const { return getAttributeNS(DOMString(), name); }
    const AtomicString& getAttributeNS(const DOMString &namespaceURI, const DOMString &localName) const;

    void setAttribute(const DOMString &name, const DOMString &value, int &exception) { setAttributeNS(DOMString(), name, value, exception); }
    void setAttributeNS(const DOMString &namespaceURI, const DOMString &qualifiedName, const DOMString &value, int &exception);

    void removeAttribute(const DOMString &name, int &exception) { removeAttributeNS(DOMString(), name, exception); }
    void removeAttributeNS(const DOMString &namespaceURI, const DOMString &localName, int &exception);

    AttrImpl *getAttributeNode(const DOMString &name) { return getAttributeNodeNS(DOMString(), name); }
    AttrImpl *getAttributeNodeNS(const DOMString &namespaceURI, const DOMString &localName);
    SharedPtr<AttrImpl> setAttributeNode(AttrImpl *newAttr, int &exception);
    SharedPtr<AttrImpl> setAttributeNodeNS(AttrImpl *newAttr, int &exception) { return setAttributeNode(newAttr, exception); }
    SharedPtr<AttrImpl> removeAttributeNode(AttrImpl *oldAttr, int &exception);
    
    DOMString prefix() const { return m_prefix; }
    void setPrefix(const DOMString &_prefix, int &exceptioncode );

    virtual CSSStyleDeclarationImpl *style();

    // DOM methods overridden from  parent classes
    virtual DOMString tagName() const;
    virtual unsigned short nodeType() const;
    virtual NodeImpl *cloneNode ( bool deep ) = 0;
    virtual DOMString nodeName() const;
    virtual bool isElementNode() const { return true; }
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    // convenience methods which ignore exceptions
    void setAttribute(Id id, const DOMString &value);

    virtual NamedAttrMapImpl *attributes() const;
    NamedAttrMapImpl* attributes(bool readonly) const;

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
 
    virtual AttributeImpl* createAttribute(Id id, DOMStringImpl* value);
    
    void dispatchAttrRemovalEvent(AttributeImpl *attr);
    void dispatchAttrAdditionEvent(AttributeImpl *attr);

    virtual void accessKeyAction(bool sendToAnyEvent) { };

    virtual DOMString toString() const;

    virtual bool isURLAttribute(AttributeImpl *attr) const;
    
#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
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
    virtual DOMString namespaceURI() const;
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

    virtual SharedPtr<NodeImpl> removeNamedItem ( NodeImpl::Id id, int &exceptioncode );
    virtual SharedPtr<NodeImpl> setNamedItem ( NodeImpl* arg, int &exceptioncode );

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

    virtual bool isMappedAttributeMap() const;

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

enum MappedAttributeEntry { eNone, eUniversal, ePersistent, eReplaced, eBlock, eHR, eUnorderedList, eListItem,
    eTable, eCell, eCaption, eLastEntry };

class CSSMappedAttributeDeclarationImpl : public CSSMutableStyleDeclarationImpl
{
public:
    CSSMappedAttributeDeclarationImpl(CSSRuleImpl *parentRule)
    : CSSMutableStyleDeclarationImpl(parentRule), m_entryType(eNone), m_attrName(0)
    {}
    
    virtual ~CSSMappedAttributeDeclarationImpl();

    void setMappedState(MappedAttributeEntry type, NodeImpl::Id attr, const AtomicString& val)
    {
        m_entryType = type;
        m_attrName = attr;
        m_attrValue = val;
    }

private:
    MappedAttributeEntry m_entryType;
    NodeImpl::Id m_attrName;
    AtomicString m_attrValue;
};

class MappedAttributeImpl : public AttributeImpl
{
public:
    MappedAttributeImpl(NodeImpl::Id _id, const AtomicString& value, CSSMappedAttributeDeclarationImpl* decl = 0)
    : AttributeImpl(_id, value), m_styleDecl(decl)
    {
        if (decl)
            decl->ref();
    }

    ~MappedAttributeImpl();

    virtual AttributeImpl* clone(bool preserveDecl=true) const;

    CSSMappedAttributeDeclarationImpl* decl() const { return m_styleDecl; }
    void setDecl(CSSMappedAttributeDeclarationImpl* decl) 
    { 
        if (m_styleDecl) m_styleDecl->deref();
        m_styleDecl = decl;
        if (m_styleDecl) m_styleDecl->ref();
    }

private:
    CSSMappedAttributeDeclarationImpl* m_styleDecl;
};

class NamedMappedAttrMapImpl : public NamedAttrMapImpl
{
public:
    NamedMappedAttrMapImpl(ElementImpl *e);

    virtual void clearAttributes();
    
    virtual bool isMappedAttributeMap() const;
    
    virtual void parseClassAttribute(const DOMString& classAttr);
    const AtomicStringList* getClassList() const { return &m_classList; }
    
    virtual bool hasMappedAttributes() const { return m_mappedAttributeCount > 0; }
    void declRemoved() { m_mappedAttributeCount--; }
    void declAdded() { m_mappedAttributeCount++; }
    
    bool mapsEquivalent(const NamedMappedAttrMapImpl* otherMap) const;
    int declCount() const;

    MappedAttributeImpl* attributeItem(unsigned long index) const
    { return attrs ? static_cast<MappedAttributeImpl*>(attrs[index]) : 0; }
    
private:
    AtomicStringList m_classList;
    int m_mappedAttributeCount;
};

class StyledElementImpl : public ElementImpl
{
public:
    StyledElementImpl(DocumentPtr *doc);
    virtual ~StyledElementImpl();

    virtual bool isStyledElement() const { return true; }

    bool hasMappedAttributes() const { return namedAttrMap ? static_cast<NamedMappedAttrMapImpl*>(namedAttrMap)->hasMappedAttributes() : false; }
    const NamedMappedAttrMapImpl* mappedAttributes() const { return static_cast<NamedMappedAttrMapImpl*>(namedAttrMap); }
    bool isMappedAttribute(NodeImpl::Id attr) const { MappedAttributeEntry res = eNone; mapToEntry(attr, res); return res != eNone; }

    void addCSSLength(MappedAttributeImpl* attr, int id, const DOMString &value);
    void addCSSProperty(MappedAttributeImpl* attr, int id, const DOMString &value);
    void addCSSProperty(MappedAttributeImpl* attr, int id, int value);
    void addCSSStringProperty(MappedAttributeImpl* attr, int id, const DOMString &value, DOM::CSSPrimitiveValue::UnitTypes = DOM::CSSPrimitiveValue::CSS_STRING);
    void addCSSImageProperty(MappedAttributeImpl* attr, int id, const DOMString &URL);
    void addCSSColor(MappedAttributeImpl* attr, int id, const DOMString &c);
    void createMappedDecl(MappedAttributeImpl* attr);
    
    static CSSMappedAttributeDeclarationImpl* getMappedAttributeDecl(MappedAttributeEntry type, AttributeImpl* attr);
    static void setMappedAttributeDecl(MappedAttributeEntry type, AttributeImpl* attr, CSSMappedAttributeDeclarationImpl* decl);
    static void removeMappedAttributeDecl(MappedAttributeEntry type, NodeImpl::Id attrName, const AtomicString& attrValue);
    static QPtrDict<QPtrDict<QPtrDict<CSSMappedAttributeDeclarationImpl> > >* m_mappedAttributeDecls;
    
    CSSMutableStyleDeclarationImpl* inlineStyleDecl() const { return m_inlineStyleDecl; }
    virtual CSSMutableStyleDeclarationImpl* additionalAttributeStyleDecl();
    CSSMutableStyleDeclarationImpl* getInlineStyleDecl();
    CSSStyleDeclarationImpl* style();
    void createInlineStyleDecl();
    void destroyInlineStyleDecl();
    void invalidateStyleAttribute();
    virtual void updateStyleAttributeIfNeeded() const;
    
    virtual const AtomicStringList* getClassList() const;
    virtual void attributeChanged(AttributeImpl* attr, bool preserveDecls = false);
    virtual void parseMappedAttribute(MappedAttributeImpl* attr);
    virtual bool mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const;
    virtual void createAttributeMap() const;
    virtual AttributeImpl* createAttribute(NodeImpl::Id id, DOMStringImpl* value);

protected:
    CSSMutableStyleDeclarationImpl* m_inlineStyleDecl;
    mutable bool m_isStyleAttributeValid : 1;
    mutable bool m_synchronizingStyleAttribute : 1;
};

}; //namespace

#if APPLE_CHANGES
#undef id
#endif

#endif
