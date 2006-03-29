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

#ifndef Element_h
#define Element_h

#include "ContainerNode.h"
#include "Attr.h"
#include "QualifiedName.h"

namespace WebCore {

class AtomicStringList;
class Attribute;

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
    virtual RenderStyle *createStyleForRenderer(RenderObject *parent);
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

    Node* insertAdjacentElement(const String& where, Node* newChild, int& exception);
    bool contains(const Element*) const;
 
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

} //namespace

#endif
