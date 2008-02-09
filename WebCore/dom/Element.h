/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef Element_h
#define Element_h

#include "ContainerNode.h"
#include "QualifiedName.h"
#include "ScrollTypes.h"

namespace WebCore {

class Attr;
class Attribute;
class CSSStyleDeclaration;
class ClassNames;
class ElementRareData;
class IntSize;

class Element : public ContainerNode {
public:
    Element(const QualifiedName&, Document*);
    ~Element();

    // Used to quickly determine whether or not an element has a given CSS class.
    virtual const ClassNames* getClassNames() const;
    const AtomicString& getIDAttribute() const;
    bool hasAttribute(const QualifiedName&) const;
    const AtomicString& getAttribute(const QualifiedName&) const;
    void setAttribute(const QualifiedName&, StringImpl* value, ExceptionCode&);
    void removeAttribute(const QualifiedName&, ExceptionCode&);

    bool hasAttributes() const;

    bool hasAttribute(const String& name) const;
    bool hasAttributeNS(const String& namespaceURI, const String& localName) const;

    const AtomicString& getAttribute(const String& name) const;
    const AtomicString& getAttributeNS(const String& namespaceURI, const String& localName) const;

    void setAttribute(const String& name, const String& value, ExceptionCode&);
    void setAttributeNS(const String& namespaceURI, const String& qualifiedName, const String& value, ExceptionCode&);

    void scrollIntoView (bool alignToTop = true);
    void scrollIntoViewIfNeeded(bool centerIfNeeded = true);

    void scrollByUnits(int units, ScrollGranularity);
    void scrollByLines(int lines);
    void scrollByPages(int pages);

    int offsetLeft();
    int offsetTop();
    int offsetWidth();
    int offsetHeight();
    Element* offsetParent();
    int clientLeft();
    int clientTop();
    int clientWidth();
    int clientHeight();
    int scrollLeft();
    int scrollTop();
    void setScrollLeft(int);
    void setScrollTop(int);
    int scrollWidth();
    int scrollHeight();

    void removeAttribute(const String& name, ExceptionCode&);
    void removeAttributeNS(const String& namespaceURI, const String& localName, ExceptionCode&);

    PassRefPtr<Attr> getAttributeNode(const String& name);
    PassRefPtr<Attr> getAttributeNodeNS(const String& namespaceURI, const String& localName);
    PassRefPtr<Attr> setAttributeNode(Attr*, ExceptionCode&);
    PassRefPtr<Attr> setAttributeNodeNS(Attr*, ExceptionCode&);
    PassRefPtr<Attr> removeAttributeNode(Attr*, ExceptionCode&);
    
    virtual CSSStyleDeclaration* style();

    const QualifiedName& tagQName() const { return m_tagName; }
    String tagName() const { return nodeName(); }
    bool hasTagName(const QualifiedName& tagName) const { return m_tagName.matches(tagName); }
    
    // A fast function for checking the local name against another atomic string.
    bool hasLocalName(const AtomicString& other) const { return m_tagName.localName() == other; }
    bool hasLocalName(const QualifiedName& other) const { return m_tagName.localName() == other.localName(); }

    virtual const AtomicString& localName() const { return m_tagName.localName(); }
    virtual const AtomicString& prefix() const { return m_tagName.prefix(); }
    virtual void setPrefix(const AtomicString &_prefix, ExceptionCode&);
    virtual const AtomicString& namespaceURI() const { return m_tagName.namespaceURI(); }

    virtual String baseURI() const;

    // DOM methods overridden from  parent classes
    virtual NodeType nodeType() const;
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual String nodeName() const;
    virtual bool isElementNode() const { return true; }
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void childrenChanged(bool changedByParser = false);

    virtual bool isInputTypeHidden() const { return false; }

    String nodeNamePreservingCase() const;

    // convenience methods which ignore exceptions
    void setAttribute(const QualifiedName&, const String& value);
    void setBooleanAttribute(const QualifiedName& name, bool);

    virtual NamedAttrMap* attributes() const;
    NamedAttrMap* attributes(bool readonly) const;

    // This method is called whenever an attribute is added, changed or removed.
    virtual void attributeChanged(Attribute*, bool preserveDecls = false) {}

    // not part of the DOM
    void setAttributeMap(NamedAttrMap*);

    virtual void copyNonAttributeProperties(const Element* source) {}

    virtual void attach();
    virtual void detach();
    virtual RenderStyle* styleForRenderer(RenderObject* parent);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void recalcStyle(StyleChange = NoChange);

    virtual RenderStyle* computedStyle();

    virtual bool childTypeAllowed(NodeType);

    virtual Attribute* createAttribute(const QualifiedName& name, StringImpl* value);
    
    void dispatchAttrRemovalEvent(Attribute*);
    void dispatchAttrAdditionEvent(Attribute*);

    virtual void accessKeyAction(bool sendToAnyEvent) { }

    virtual String toString() const;

    virtual bool isURLAttribute(Attribute*) const;
    virtual const QualifiedName& imageSourceAttributeName() const;
    virtual String target() const { return String(); }
        
    virtual void focus(bool restorePreviousSelection = true);
    virtual void updateFocusAppearance(bool restorePreviousSelection);
    void blur();

#ifndef NDEBUG
    virtual void dump(TextStream* , DeprecatedString ind = "") const;
    virtual void formatForDebugger(char* buffer, unsigned length) const;
#endif

    Node* insertAdjacentElement(const String& where, Node* newChild, ExceptionCode&);
    bool contains(const Node*) const;

    String innerText() const;
    String outerText() const;
 
    virtual String title() const;

    String openTagStartToString() const;

    void updateId(const AtomicString& oldId, const AtomicString& newId);

    IntSize minimumSizeForResizing() const;
    void setMinimumSizeForResizing(const IntSize&);

    // Use Document::registerForPageCacheCallbacks() to subscribe these
    virtual void willSaveToCache() { }
    virtual void didRestoreFromCache() { }
    
    bool isFinishedParsingChildren() const { return m_parsingChildrenFinished; }
    virtual void finishParsingChildren();
    virtual void beginParsingChildren() { m_parsingChildrenFinished = false; }

private:
    ElementRareData* rareData();
    const ElementRareData* rareData() const;
    ElementRareData* createRareData();

    virtual void createAttributeMap() const;

    virtual void updateStyleAttributeIfNeeded() const {}
    
    void updateFocusAppearanceSoonAfterAttach();
    void cancelFocusAppearanceUpdate();

    virtual bool virtualHasTagName(const QualifiedName&) const;

private:
    QualifiedName m_tagName;

protected:
    mutable RefPtr<NamedAttrMap> namedAttrMap;

    // These two bits are really used by the StyledElement subclass, but they are pulled up here in order to be shared with other
    // Element bits.
    mutable bool m_isStyleAttributeValid : 1;
    mutable bool m_synchronizingStyleAttribute : 1;
    
private:
    bool m_parsingChildrenFinished : 1;
};

} //namespace

#endif
