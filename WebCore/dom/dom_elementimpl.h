/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef DOM_ELEMENTIMPL_H
#define DOM_ELEMENTIMPL_H

#include "NamedNodeMapImpl.h"
#include "ContainerNodeImpl.h"
#include "StringImpl.h"
#include "css_valueimpl.h"
#include "QualifiedName.h"
#include "AtomicStringList.h"

#if __OBJC__
#define id id_AVOID_KEYWORD
#endif

namespace khtml {
    class CSSStyleSelector;
}

namespace DOM {

class CSSStyleDeclarationImpl;
class ElementImpl;
class NamedAttrMapImpl;
class AttrImpl;

// this has no counterpart in DOM, purely internal
// representation of the nodevalue of an Attr.
// the actual Attr (AttrImpl) with its value as textchild
// is only allocated on demand by the DOM bindings.
// Any use of AttrImpl inside khtml should be avoided.
class AttributeImpl : public Shared<AttributeImpl> {
    friend class NamedAttrMapImpl;
    friend class ElementImpl;
    friend class AttrImpl;

public:
    // null value is forbidden !
    AttributeImpl(const QualifiedName& name, const AtomicString& value)
        : m_name(name), m_value(value), m_impl(0)
    {}
    
    AttributeImpl(const AtomicString& name, const AtomicString& value)
        : m_name(nullAtom, name, nullAtom), m_value(value), m_impl(0)
    {}

    virtual ~AttributeImpl() { }
    
    const AtomicString& value() const { return m_value; }
    const AtomicString& prefix() const { return m_name.prefix(); }
    const AtomicString& localName() const { return m_name.localName(); }
    const AtomicString& namespaceURI() const { return m_name.namespaceURI(); }
    
    const QualifiedName& name() const { return m_name; }
    
    AttrImpl* attrImpl() const { return m_impl; }
    PassRefPtr<AttrImpl> createAttrImplIfNeeded(ElementImpl* e);

    bool isNull() const { return m_value.isNull(); }
    bool isEmpty() const { return m_value.isEmpty(); }
    
    virtual AttributeImpl* clone(bool preserveDecl=true) const;

    // An extension to get the style information for presentational attributes.
    virtual CSSStyleDeclarationImpl* style() const { return 0; }
    
private:
    void setValue(const AtomicString& value) { m_value = value; }
    void setPrefix(const AtomicString& prefix) { m_name.setPrefix(prefix); }

    QualifiedName m_name;
    AtomicString m_value;
    AttrImpl* m_impl;
};

// Attr can have Text and EntityReference children
// therefore it has to be a fullblown Node. The plan
// is to dynamically allocate a textchild and store the
// resulting nodevalue in the AttributeImpl upon
// destruction. however, this is not yet implemented.
class AttrImpl : public ContainerNodeImpl {
    friend class NamedAttrMapImpl;

public:
    AttrImpl(ElementImpl*, DocumentImpl*, AttributeImpl*);
    ~AttrImpl();

    // Call this after calling the constructor so the
    // AttrImpl node isn't floating when we append the text node.
    void createTextChild();
    
    // DOM methods & attributes for Attr
    DOMString name() const { return qualifiedName().toString(); }
    bool specified() const { return m_specified; }
    ElementImpl* ownerElement() const { return m_element; }

    DOMString value() const { return m_attribute->value(); }
    void setValue(const DOMString&, ExceptionCode&);

    // DOM methods overridden from parent classes
    virtual DOMString nodeName() const;
    virtual NodeType nodeType() const;
    virtual const AtomicString& localName() const;
    virtual const AtomicString& namespaceURI() const;
    virtual const AtomicString& prefix() const;
    virtual void setPrefix(const AtomicString&, ExceptionCode&);

    virtual DOMString nodeValue() const;
    virtual void setNodeValue(const DOMString&, ExceptionCode&);
    virtual PassRefPtr<NodeImpl> cloneNode(bool deep);

    // Other methods (not part of DOM)
    virtual bool isAttributeNode() const { return true; }
    virtual bool childTypeAllowed(NodeType);

    virtual void childrenChanged();
    virtual DOMString toString() const;

    AttributeImpl* attrImpl() const { return m_attribute.get(); }
    const QualifiedName& qualifiedName() const { return m_attribute->name(); }

    // An extension to get presentational information for attributes.
    CSSStyleDeclarationImpl* style() { return m_attribute->style(); }

private:
    ElementImpl* m_element;
    RefPtr<AttributeImpl> m_attribute;
    int m_ignoreChildrenChanged;
};

class ElementImpl : public ContainerNodeImpl {
    friend class DocumentImpl;
    friend class NamedAttrMapImpl;
    friend class AttrImpl;
    friend class NodeImpl;
    friend class CSSStyleSelector;
public:
    ElementImpl(const QualifiedName& tagName, DocumentImpl *doc);
    ~ElementImpl();

    // Used to quickly determine whether or not an element has a given CSS class.
    virtual const AtomicStringList* getClassList() const;
    const AtomicString& getIDAttribute() const;
    bool hasAttribute(const QualifiedName& name) const;
    const AtomicString& getAttribute(const QualifiedName& name) const;
    void setAttribute(const QualifiedName& name, DOMStringImpl* value, ExceptionCode&);
    void removeAttribute(const QualifiedName& name, ExceptionCode&);

    bool hasAttributes() const;

    bool hasAttribute(const DOMString &name) const { return hasAttributeNS(DOMString(), name); }
    bool hasAttributeNS(const DOMString &namespaceURI, const DOMString &localName) const;

    const AtomicString& getAttribute(const DOMString& name) const { return getAttributeNS(DOMString(), name); }
    const AtomicString& getAttributeNS(const DOMString &namespaceURI, const DOMString &localName) const;

    void setAttribute(const DOMString &name, const DOMString &value, ExceptionCode&);
    void setAttributeNS(const DOMString &namespaceURI, const DOMString &qualifiedName, const DOMString &value, ExceptionCode&);

    void scrollIntoView (bool alignToTop);
    void scrollIntoViewIfNeeded(bool centerIfNeeded);

    void removeAttribute(const DOMString &name, ExceptionCode& ec) { removeAttributeNS(DOMString(), name, ec); }
    void removeAttributeNS(const DOMString &namespaceURI, const DOMString &localName, ExceptionCode&);

    PassRefPtr<AttrImpl> getAttributeNode(const DOMString &name) { return getAttributeNodeNS(DOMString(), name); }
    PassRefPtr<AttrImpl> getAttributeNodeNS(const DOMString &namespaceURI, const DOMString &localName);
    PassRefPtr<AttrImpl> setAttributeNode(AttrImpl*, ExceptionCode&);
    PassRefPtr<AttrImpl> setAttributeNodeNS(AttrImpl* newAttr, ExceptionCode& ec) { return setAttributeNode(newAttr, ec); }
    PassRefPtr<AttrImpl> removeAttributeNode(AttrImpl*, ExceptionCode&);
    
    virtual CSSStyleDeclarationImpl *style();

    virtual const QualifiedName& tagName() const { return m_tagName; }
    virtual bool hasTagName(const QualifiedName& tagName) const { return m_tagName.matches(tagName); }
    
    // A fast function for checking the local name against another atomic string.
    bool hasLocalName(const AtomicString& other) const { return m_tagName.localName() == other; }
    bool hasLocalName(const QualifiedName& other) const { return m_tagName.localName() == other.localName(); }

    virtual const AtomicString& localName() const { return m_tagName.localName(); }
    virtual const AtomicString& prefix() const { return m_tagName.prefix(); }
    virtual void setPrefix(const AtomicString &_prefix, ExceptionCode&);
    virtual const AtomicString& namespaceURI() const { return m_tagName.namespaceURI(); }
    
    // DOM methods overridden from  parent classes
    virtual NodeType nodeType() const;
    virtual PassRefPtr<NodeImpl> cloneNode(bool deep);
    virtual DOMString nodeName() const;
    virtual bool isElementNode() const { return true; }
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    // convenience methods which ignore exceptions
    void setAttribute(const QualifiedName& name, const DOMString &value);

    virtual NamedAttrMapImpl *attributes() const;
    NamedAttrMapImpl* attributes(bool readonly) const;

    // This method is called whenever an attribute is added, changed or removed.
    virtual void attributeChanged(AttributeImpl* attr, bool preserveDecls = false) {}

    // not part of the DOM
    void setAttributeMap(NamedAttrMapImpl*);

    virtual void copyNonAttributeProperties(const ElementImpl *source) {}
    
    // State of the element.
    virtual QString state() { return QString::null; }

    virtual void attach();
    virtual RenderStyle *styleForRenderer(RenderObject *parent);
    virtual RenderObject *createRenderer(RenderArena *, RenderStyle *);
    virtual void recalcStyle( StyleChange = NoChange );

    virtual bool childTypeAllowed(NodeType);
 
    virtual AttributeImpl* createAttribute(const QualifiedName& name, DOMStringImpl* value);
    
    void dispatchAttrRemovalEvent(AttributeImpl *attr);
    void dispatchAttrAdditionEvent(AttributeImpl *attr);

    virtual void accessKeyAction(bool sendToAnyEvent) { }

    virtual DOMString toString() const;

    virtual bool isURLAttribute(AttributeImpl *attr) const;
        
    virtual void focus();
    void blur();
    
#if !NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
    virtual void formatForDebugger(char *buffer, unsigned length) const;
#endif

protected:
    virtual void createAttributeMap() const;
    DOMString openTagStartToString() const;

private:
    void updateId(const AtomicString& oldId, const AtomicString& newId);

    virtual void updateStyleAttributeIfNeeded() const {}

protected: // member variables
    mutable RefPtr<NamedAttrMapImpl> namedAttrMap;
    QualifiedName m_tagName;
};

// the map of attributes of an element
class NamedAttrMapImpl : public NamedNodeMapImpl {
    friend class ElementImpl;
public:
    NamedAttrMapImpl(ElementImpl *e);
    virtual ~NamedAttrMapImpl();
    NamedAttrMapImpl(const NamedAttrMapImpl&);
    NamedAttrMapImpl &operator =(const NamedAttrMapImpl &other);

    // DOM methods & attributes for NamedNodeMap
    virtual PassRefPtr<NodeImpl> getNamedItemNS(const DOMString &namespaceURI, const DOMString &localName) const;
    virtual PassRefPtr<NodeImpl> removeNamedItemNS(const DOMString &namespaceURI, const DOMString &localName, ExceptionCode& ec);

    virtual PassRefPtr<NodeImpl> getNamedItem(const QualifiedName& name) const;

    virtual PassRefPtr<NodeImpl> removeNamedItem(const QualifiedName& name, ExceptionCode&);
    virtual PassRefPtr<NodeImpl> setNamedItem(NodeImpl* arg, ExceptionCode&);

    virtual PassRefPtr<NodeImpl> item(unsigned index) const;
    unsigned length() const { return len; }

    // Other methods (not part of DOM)
    AttributeImpl* attributeItem(unsigned index) const { return attrs ? attrs[index] : 0; }
    AttributeImpl* getAttributeItem(const QualifiedName& name) const;
    virtual bool isReadOnly() { return element ? element->isReadOnly() : false; }

    // used during parsing: only inserts if not already there
    // no error checking!
    void insertAttribute(AttributeImpl* newAttribute) {
        assert(!element);
        if (!getAttributeItem(newAttribute->name()))
            addAttribute(newAttribute);
        else
            newAttribute->deref();
    }

    virtual bool isMappedAttributeMap() const;

    const AtomicString& id() const { return m_id; }
    void setID(const AtomicString& _id) { m_id = _id; }
    
    bool mapsEquivalent(const NamedAttrMapImpl* otherMap) const;

protected:
    // this method is internal, does no error checking at all
    void addAttribute(AttributeImpl* newAttribute);
    // this method is internal, does no error checking at all
    void removeAttribute(const QualifiedName& name);
    virtual void clearAttributes();
    void detachFromElement();

    ElementImpl *element;
    AttributeImpl **attrs;
    uint len;
    AtomicString m_id;
};

// When adding new entries, make sure to keep eLastEntry at the end of the list.
enum MappedAttributeEntry { eNone, eUniversal, ePersistent, eReplaced, eBlock, eHR, eUnorderedList, eListItem,
    eTable, eCell, eCaption, eBDO, ePre, eLastEntry };

class CSSMappedAttributeDeclarationImpl : public CSSMutableStyleDeclarationImpl {
public:
    CSSMappedAttributeDeclarationImpl(CSSRuleImpl *parentRule)
    : CSSMutableStyleDeclarationImpl(parentRule), m_entryType(eNone), m_attrName(anyQName())
    {}
    
    virtual ~CSSMappedAttributeDeclarationImpl();

    void setMappedState(MappedAttributeEntry type, const QualifiedName& name, const AtomicString& val)
    {
        m_entryType = type;
        m_attrName = name;
        m_attrValue = val;
    }

private:
    MappedAttributeEntry m_entryType;
    QualifiedName m_attrName;
    AtomicString m_attrValue;
};

class MappedAttributeImpl : public AttributeImpl
{
public:
    MappedAttributeImpl(const QualifiedName& name, const AtomicString& value, CSSMappedAttributeDeclarationImpl* decl = 0)
    : AttributeImpl(name, value), m_styleDecl(decl)
    {
    }

    MappedAttributeImpl(const AtomicString& name, const AtomicString& value, CSSMappedAttributeDeclarationImpl* decl = 0)
    : AttributeImpl(name, value), m_styleDecl(decl)
    {
    }

    virtual AttributeImpl* clone(bool preserveDecl=true) const;

    virtual CSSStyleDeclarationImpl* style() const { return m_styleDecl.get(); }

    CSSMappedAttributeDeclarationImpl* decl() const { return m_styleDecl.get(); }
    void setDecl(CSSMappedAttributeDeclarationImpl* decl) { m_styleDecl = decl; }

private:
    RefPtr<CSSMappedAttributeDeclarationImpl> m_styleDecl;
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

    MappedAttributeImpl* attributeItem(unsigned index) const
    { return attrs ? static_cast<MappedAttributeImpl*>(attrs[index]) : 0; }
    
private:
    AtomicStringList m_classList;
    int m_mappedAttributeCount;
};

class StyledElementImpl : public ElementImpl
{
public:
    StyledElementImpl(const QualifiedName& tagName, DocumentImpl *doc);
    virtual ~StyledElementImpl();

    virtual bool isStyledElement() const { return true; }

    NamedMappedAttrMapImpl* mappedAttributes() { return static_cast<NamedMappedAttrMapImpl*>(namedAttrMap.get()); }
    const NamedMappedAttrMapImpl* mappedAttributes() const { return static_cast<NamedMappedAttrMapImpl*>(namedAttrMap.get()); }
    bool hasMappedAttributes() const { return namedAttrMap && mappedAttributes()->hasMappedAttributes(); }
    bool isMappedAttribute(const QualifiedName& name) const { MappedAttributeEntry res = eNone; mapToEntry(name, res); return res != eNone; }

    void addCSSLength(MappedAttributeImpl* attr, int id, const DOMString &value);
    void addCSSProperty(MappedAttributeImpl* attr, int id, const DOMString &value);
    void addCSSProperty(MappedAttributeImpl* attr, int id, int value);
    void addCSSStringProperty(MappedAttributeImpl* attr, int id, const DOMString &value, CSSPrimitiveValueImpl::UnitTypes = CSSPrimitiveValue::CSS_STRING);
    void addCSSImageProperty(MappedAttributeImpl* attr, int id, const DOMString &URL);
    void addCSSColor(MappedAttributeImpl* attr, int id, const DOMString &c);
    void createMappedDecl(MappedAttributeImpl* attr);
    
    static CSSMappedAttributeDeclarationImpl* getMappedAttributeDecl(MappedAttributeEntry type, AttributeImpl* attr);
    static void setMappedAttributeDecl(MappedAttributeEntry type, AttributeImpl* attr, CSSMappedAttributeDeclarationImpl* decl);
    static void removeMappedAttributeDecl(MappedAttributeEntry type, const QualifiedName& attrName, const AtomicString& attrValue);
    
    CSSMutableStyleDeclarationImpl* inlineStyleDecl() const { return m_inlineStyleDecl.get(); }
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
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void createAttributeMap() const;
    virtual AttributeImpl* createAttribute(const QualifiedName& name, DOMStringImpl* value);

protected:
    RefPtr<CSSMutableStyleDeclarationImpl> m_inlineStyleDecl;
    mutable bool m_isStyleAttributeValid : 1;
    mutable bool m_synchronizingStyleAttribute : 1;
};

} //namespace

#undef id

#endif
