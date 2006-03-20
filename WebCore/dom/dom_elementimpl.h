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

#include "AtomicStringList.h"
#include "ContainerNode.h"
#include "NamedNodeMap.h"
#include "QualifiedName.h"
#include "StringImpl.h"
#include "css_valueimpl.h"

#if __OBJC__
#define id id_AVOID_KEYWORD
#endif

namespace WebCore {

class Attr;
class CSSStyleDeclaration;
class CSSStyleSelector;
class Element;
class NamedAttrMap;

// this has no counterpart in DOM, purely internal
// representation of the nodevalue of an Attr.
// the actual Attr (Attr) with its value as textchild
// is only allocated on demand by the DOM bindings.
// Any use of Attr inside khtml should be avoided.
class Attribute : public Shared<Attribute> {
    friend class NamedAttrMap;
    friend class Element;
    friend class Attr;

public:
    // null value is forbidden !
    Attribute(const QualifiedName& name, const AtomicString& value)
        : m_name(name), m_value(value), m_impl(0)
    {}
    
    Attribute(const AtomicString& name, const AtomicString& value)
        : m_name(nullAtom, name, nullAtom), m_value(value), m_impl(0)
    {}

    virtual ~Attribute() { }
    
    const AtomicString& value() const { return m_value; }
    const AtomicString& prefix() const { return m_name.prefix(); }
    const AtomicString& localName() const { return m_name.localName(); }
    const AtomicString& namespaceURI() const { return m_name.namespaceURI(); }
    
    const QualifiedName& name() const { return m_name; }
    
    Attr* attr() const { return m_impl; }
    PassRefPtr<Attr> createAttrIfNeeded(Element* e);

    bool isNull() const { return m_value.isNull(); }
    bool isEmpty() const { return m_value.isEmpty(); }
    
    virtual Attribute* clone(bool preserveDecl=true) const;

    // An extension to get the style information for presentational attributes.
    virtual CSSStyleDeclaration* style() const { return 0; }
    
private:
    void setValue(const AtomicString& value) { m_value = value; }
    void setPrefix(const AtomicString& prefix) { m_name.setPrefix(prefix); }

    QualifiedName m_name;
    AtomicString m_value;
    Attr* m_impl;
};

// Attr can have Text and EntityReference children
// therefore it has to be a fullblown Node. The plan
// is to dynamically allocate a textchild and store the
// resulting nodevalue in the Attribute upon
// destruction. however, this is not yet implemented.
class Attr : public ContainerNode {
    friend class NamedAttrMap;

public:
    Attr(Element*, Document*, Attribute*);
    ~Attr();

    // Call this after calling the constructor so the
    // Attr node isn't floating when we append the text node.
    void createTextChild();
    
    // DOM methods & attributes for Attr
    String name() const { return qualifiedName().toString(); }
    bool specified() const { return m_specified; }
    Element* ownerElement() const { return m_element; }

    String value() const { return m_attribute->value(); }
    void setValue(const String&, ExceptionCode&);

    // DOM methods overridden from parent classes
    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual const AtomicString& localName() const;
    virtual const AtomicString& namespaceURI() const;
    virtual const AtomicString& prefix() const;
    virtual void setPrefix(const AtomicString&, ExceptionCode&);

    virtual String nodeValue() const;
    virtual void setNodeValue(const String&, ExceptionCode&);
    virtual PassRefPtr<Node> cloneNode(bool deep);

    // Other methods (not part of DOM)
    virtual bool isAttributeNode() const { return true; }
    virtual bool childTypeAllowed(NodeType);

    virtual void childrenChanged();
    virtual String toString() const;

    Attribute* attr() const { return m_attribute.get(); }
    const QualifiedName& qualifiedName() const { return m_attribute->name(); }

    // An extension to get presentational information for attributes.
    CSSStyleDeclaration* style() { return m_attribute->style(); }

private:
    Element* m_element;
    RefPtr<Attribute> m_attribute;
    int m_ignoreChildrenChanged;
};

class Element : public ContainerNode {
    friend class Document;
    friend class NamedAttrMap;
    friend class Attr;
    friend class Node;
    friend class CSSStyleSelector;
public:
    Element(const QualifiedName& tagName, Document *doc);
    ~Element();

    // Used to quickly determine whether or not an element has a given CSS class.
    virtual const AtomicStringList* getClassList() const;
    const AtomicString& getIDAttribute() const;
    bool hasAttribute(const QualifiedName& name) const;
    const AtomicString& getAttribute(const QualifiedName& name) const;
    void setAttribute(const QualifiedName& name, StringImpl* value, ExceptionCode&);
    void removeAttribute(const QualifiedName& name, ExceptionCode&);

    bool hasAttributes() const;

    bool hasAttribute(const String& name) const;
    bool hasAttributeNS(const String& namespaceURI, const String& localName) const;

    const AtomicString& getAttribute(const String& name) const;
    const AtomicString& getAttributeNS(const String& namespaceURI, const String& localName) const;

    void setAttribute(const String& name, const String& value, ExceptionCode&);
    void setAttributeNS(const String& namespaceURI, const String& qualifiedName, const String& value, ExceptionCode&);

    void scrollIntoView (bool alignToTop);
    void scrollIntoViewIfNeeded(bool centerIfNeeded);

    void removeAttribute(const String &name, ExceptionCode& ec);
    void removeAttributeNS(const String &namespaceURI, const String& localName, ExceptionCode&);

    PassRefPtr<Attr> getAttributeNode(const String& name);
    PassRefPtr<Attr> getAttributeNodeNS(const String& namespaceURI, const String& localName);
    PassRefPtr<Attr> setAttributeNode(Attr*, ExceptionCode&);
    PassRefPtr<Attr> setAttributeNodeNS(Attr* newAttr, ExceptionCode& ec) { return setAttributeNode(newAttr, ec); }
    PassRefPtr<Attr> removeAttributeNode(Attr*, ExceptionCode&);
    
    virtual CSSStyleDeclaration *style();

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
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual String nodeName() const;
    virtual bool isElementNode() const { return true; }
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    // convenience methods which ignore exceptions
    void setAttribute(const QualifiedName& name, const String& value);

    virtual NamedAttrMap *attributes() const;
    NamedAttrMap* attributes(bool readonly) const;

    // This method is called whenever an attribute is added, changed or removed.
    virtual void attributeChanged(Attribute* attr, bool preserveDecls = false) {}

    // not part of the DOM
    void setAttributeMap(NamedAttrMap*);

    virtual void copyNonAttributeProperties(const Element *source) {}
    
    // State of the element.
    virtual DeprecatedString state() { return DeprecatedString::null; }

    virtual void attach();
    virtual RenderStyle *styleForRenderer(RenderObject *parent);
    virtual RenderObject *createRenderer(RenderArena *, RenderStyle *);
    virtual void recalcStyle( StyleChange = NoChange );

    virtual bool childTypeAllowed(NodeType);
 
    virtual Attribute* createAttribute(const QualifiedName& name, StringImpl* value);
    
    void dispatchAttrRemovalEvent(Attribute *attr);
    void dispatchAttrAdditionEvent(Attribute *attr);

    virtual void accessKeyAction(bool sendToAnyEvent) { }

    virtual String toString() const;

    virtual bool isURLAttribute(Attribute *attr) const;
        
    virtual void focus();
    void blur();
    
#if !NDEBUG
    virtual void dump(QTextStream *stream, DeprecatedString ind = "") const;
    virtual void formatForDebugger(char *buffer, unsigned length) const;
#endif

protected:
    virtual void createAttributeMap() const;
    String openTagStartToString() const;

private:
    void updateId(const AtomicString& oldId, const AtomicString& newId);

    virtual void updateStyleAttributeIfNeeded() const {}

protected: // member variables
    mutable RefPtr<NamedAttrMap> namedAttrMap;
    QualifiedName m_tagName;
};

// the map of attributes of an element
class NamedAttrMap : public NamedNodeMap {
    friend class Element;
public:
    NamedAttrMap(Element *e);
    virtual ~NamedAttrMap();
    NamedAttrMap(const NamedAttrMap&);
    NamedAttrMap &operator =(const NamedAttrMap &other);

    // DOM methods & attributes for NamedNodeMap

    virtual PassRefPtr<Node> getNamedItem(const String& name) const;
    virtual PassRefPtr<Node> removeNamedItem(const String& name, ExceptionCode&);

    virtual PassRefPtr<Node> getNamedItemNS(const String& namespaceURI, const String& localName) const;
    virtual PassRefPtr<Node> removeNamedItemNS(const String& namespaceURI, const String& localName, ExceptionCode&);

    virtual PassRefPtr<Node> getNamedItem(const QualifiedName& name) const;
    virtual PassRefPtr<Node> removeNamedItem(const QualifiedName& name, ExceptionCode&);
    virtual PassRefPtr<Node> setNamedItem(Node* arg, ExceptionCode&);

    virtual PassRefPtr<Node> item(unsigned index) const;
    unsigned length() const { return len; }

    // Other methods (not part of DOM)
    Attribute* attributeItem(unsigned index) const { return attrs ? attrs[index] : 0; }
    Attribute* getAttributeItem(const QualifiedName& name) const;
    virtual bool isReadOnly() { return element ? element->isReadOnly() : false; }

    // used during parsing: only inserts if not already there
    // no error checking!
    void insertAttribute(Attribute* newAttribute) {
        assert(!element);
        if (!getAttributeItem(newAttribute->name()))
            addAttribute(newAttribute);
        else
            newAttribute->deref();
    }

    virtual bool isMappedAttributeMap() const;

    const AtomicString& id() const { return m_id; }
    void setID(const AtomicString& _id) { m_id = _id; }
    
    bool mapsEquivalent(const NamedAttrMap* otherMap) const;

protected:
    // this method is internal, does no error checking at all
    void addAttribute(Attribute* newAttribute);
    // this method is internal, does no error checking at all
    void removeAttribute(const QualifiedName& name);
    virtual void clearAttributes();
    void detachFromElement();

    Element *element;
    Attribute **attrs;
    unsigned len;
    AtomicString m_id;
};

// When adding new entries, make sure to keep eLastEntry at the end of the list.
enum MappedAttributeEntry { eNone, eUniversal, ePersistent, eReplaced, eBlock, eHR, eUnorderedList, eListItem,
    eTable, eCell, eCaption, eBDO, ePre, eLastEntry };

class CSSMappedAttributeDeclaration : public CSSMutableStyleDeclaration {
public:
    CSSMappedAttributeDeclaration(CSSRule *parentRule)
    : CSSMutableStyleDeclaration(parentRule), m_entryType(eNone), m_attrName(anyQName())
    {}
    
    virtual ~CSSMappedAttributeDeclaration();

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

class MappedAttribute : public Attribute
{
public:
    MappedAttribute(const QualifiedName& name, const AtomicString& value, CSSMappedAttributeDeclaration* decl = 0)
    : Attribute(name, value), m_styleDecl(decl)
    {
    }

    MappedAttribute(const AtomicString& name, const AtomicString& value, CSSMappedAttributeDeclaration* decl = 0)
    : Attribute(name, value), m_styleDecl(decl)
    {
    }

    virtual Attribute* clone(bool preserveDecl=true) const;

    virtual CSSStyleDeclaration* style() const { return m_styleDecl.get(); }

    CSSMappedAttributeDeclaration* decl() const { return m_styleDecl.get(); }
    void setDecl(CSSMappedAttributeDeclaration* decl) { m_styleDecl = decl; }

private:
    RefPtr<CSSMappedAttributeDeclaration> m_styleDecl;
};

class NamedMappedAttrMap : public NamedAttrMap
{
public:
    NamedMappedAttrMap(Element *e);

    virtual void clearAttributes();
    
    virtual bool isMappedAttributeMap() const;
    
    virtual void parseClassAttribute(const String& classAttr);
    const AtomicStringList* getClassList() const { return &m_classList; }
    
    virtual bool hasMappedAttributes() const { return m_mappedAttributeCount > 0; }
    void declRemoved() { m_mappedAttributeCount--; }
    void declAdded() { m_mappedAttributeCount++; }
    
    bool mapsEquivalent(const NamedMappedAttrMap* otherMap) const;
    int declCount() const;

    MappedAttribute* attributeItem(unsigned index) const
    { return attrs ? static_cast<MappedAttribute*>(attrs[index]) : 0; }
    
private:
    AtomicStringList m_classList;
    int m_mappedAttributeCount;
};

class StyledElement : public Element
{
public:
    StyledElement(const QualifiedName& tagName, Document *doc);
    virtual ~StyledElement();

    virtual bool isStyledElement() const { return true; }

    NamedMappedAttrMap* mappedAttributes() { return static_cast<NamedMappedAttrMap*>(namedAttrMap.get()); }
    const NamedMappedAttrMap* mappedAttributes() const { return static_cast<NamedMappedAttrMap*>(namedAttrMap.get()); }
    bool hasMappedAttributes() const { return namedAttrMap && mappedAttributes()->hasMappedAttributes(); }
    bool isMappedAttribute(const QualifiedName& name) const { MappedAttributeEntry res = eNone; mapToEntry(name, res); return res != eNone; }

    void addCSSLength(MappedAttribute* attr, int id, const String &value);
    void addCSSProperty(MappedAttribute* attr, int id, const String &value);
    void addCSSProperty(MappedAttribute* attr, int id, int value);
    void addCSSStringProperty(MappedAttribute* attr, int id, const String &value, CSSPrimitiveValue::UnitTypes = CSSPrimitiveValue::CSS_STRING);
    void addCSSImageProperty(MappedAttribute* attr, int id, const String &URL);
    void addCSSColor(MappedAttribute* attr, int id, const String &c);
    void createMappedDecl(MappedAttribute* attr);
    
    static CSSMappedAttributeDeclaration* getMappedAttributeDecl(MappedAttributeEntry type, Attribute* attr);
    static void setMappedAttributeDecl(MappedAttributeEntry type, Attribute* attr, CSSMappedAttributeDeclaration* decl);
    static void removeMappedAttributeDecl(MappedAttributeEntry type, const QualifiedName& attrName, const AtomicString& attrValue);
    
    CSSMutableStyleDeclaration* inlineStyleDecl() const { return m_inlineStyleDecl.get(); }
    virtual CSSMutableStyleDeclaration* additionalAttributeStyleDecl();
    CSSMutableStyleDeclaration* getInlineStyleDecl();
    CSSStyleDeclaration* style();
    void createInlineStyleDecl();
    void destroyInlineStyleDecl();
    void invalidateStyleAttribute();
    virtual void updateStyleAttributeIfNeeded() const;
    
    virtual const AtomicStringList* getClassList() const;
    virtual void attributeChanged(Attribute* attr, bool preserveDecls = false);
    virtual void parseMappedAttribute(MappedAttribute* attr);
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void createAttributeMap() const;
    virtual Attribute* createAttribute(const QualifiedName& name, StringImpl* value);

protected:
    RefPtr<CSSMutableStyleDeclaration> m_inlineStyleDecl;
    mutable bool m_isStyleAttributeValid : 1;
    mutable bool m_synchronizingStyleAttribute : 1;
};

} //namespace

#undef id

#endif
