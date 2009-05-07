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
class ClientRect;
class ClientRectList;
class ElementRareData;
class IntSize;

class Element : public ContainerNode {
public:
    Element(const QualifiedName&, Document*);
    ~Element();

    const AtomicString& getIDAttribute() const;
    bool hasAttribute(const QualifiedName&) const;
    const AtomicString& getAttribute(const QualifiedName&) const;
    void setAttribute(const QualifiedName&, const AtomicString& value, ExceptionCode&);
    void removeAttribute(const QualifiedName&, ExceptionCode&);

    bool hasAttributes() const;

    bool hasAttribute(const String& name) const;
    bool hasAttributeNS(const String& namespaceURI, const String& localName) const;

    const AtomicString& getAttribute(const String& name) const;
    const AtomicString& getAttributeNS(const String& namespaceURI, const String& localName) const;

    void setAttribute(const AtomicString& name, const AtomicString& value, ExceptionCode&);
    void setAttributeNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, const AtomicString& value, ExceptionCode&);

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
    virtual int scrollLeft() const;
    virtual int scrollTop() const;
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);
    virtual int scrollWidth() const;
    virtual int scrollHeight() const;

    PassRefPtr<ClientRectList> getClientRects() const;
    PassRefPtr<ClientRect> getBoundingClientRect() const;

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

    const AtomicString& localName() const { return m_tagName.localName(); }
    const AtomicString& prefix() const { return m_tagName.prefix(); }
    virtual void setPrefix(const AtomicString&, ExceptionCode&);
    const AtomicString& namespaceURI() const { return m_tagName.namespaceURI(); }

    virtual KURL baseURI() const;

    // DOM methods overridden from parent classes
    virtual NodeType nodeType() const;
    virtual String nodeName() const;
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    PassRefPtr<Element> cloneElementWithChildren();
    PassRefPtr<Element> cloneElementWithoutChildren();

    void normalizeAttributes();

    virtual bool isFormControlElement() const { return false; }

    String nodeNamePreservingCase() const;

    // convenience methods which ignore exceptions
    void setAttribute(const QualifiedName&, const AtomicString& value);
    void setBooleanAttribute(const QualifiedName& name, bool);

    virtual NamedNodeMap* attributes() const;
    NamedNodeMap* attributes(bool readonly) const;

    // This method is called whenever an attribute is added, changed or removed.
    virtual void attributeChanged(Attribute*, bool preserveDecls = false);

    // not part of the DOM
    void setAttributeMap(PassRefPtr<NamedNodeMap>);

    virtual void copyNonAttributeProperties(const Element* /*source*/) { }

    virtual void attach();
    virtual void detach();
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void recalcStyle(StyleChange = NoChange);

    virtual RenderStyle* computedStyle();

    virtual bool childTypeAllowed(NodeType);

    virtual PassRefPtr<Attribute> createAttribute(const QualifiedName&, const AtomicString& value);
    
    void dispatchAttrRemovalEvent(Attribute*);
    void dispatchAttrAdditionEvent(Attribute*);

    virtual void accessKeyAction(bool /*sendToAnyEvent*/) { }

    virtual bool isURLAttribute(Attribute*) const;
    virtual const QualifiedName& imageSourceAttributeName() const;
    virtual String target() const { return String(); }

    virtual void focus(bool restorePreviousSelection = true);
    virtual void updateFocusAppearance(bool restorePreviousSelection);
    void blur();

#ifndef NDEBUG
    virtual void formatForDebugger(char* buffer, unsigned length) const;
#endif

    String innerText() const;
    String outerText() const;
 
    virtual String title() const;

    String openTagStartToString() const;

    void updateId(const AtomicString& oldId, const AtomicString& newId);

    IntSize minimumSizeForResizing() const;
    void setMinimumSizeForResizing(const IntSize&);

    // Use Document::registerForDocumentActivationCallbacks() to subscribe to these
    virtual void documentWillBecomeInactive() { }
    virtual void documentDidBecomeActive() { }

    // Use Document::registerForMediaVolumeCallbacks() to subscribe to this
    virtual void mediaVolumeDidChange() { }

    bool isFinishedParsingChildren() const { return m_parsingChildrenFinished; }
    virtual void finishParsingChildren();
    virtual void beginParsingChildren() { m_parsingChildrenFinished = false; }

    // ElementTraversal API
    Element* firstElementChild() const;
    Element* lastElementChild() const;
    Element* previousElementSibling() const;
    Element* nextElementSibling() const;
    unsigned childElementCount() const;

    // FormControlElement API
    virtual bool isEnabledFormControl() const { return true; }
    virtual bool isReadOnlyFormControl() const { return false; }
    virtual bool isTextFormControl() const { return false; }

    virtual bool formControlValueMatchesRenderer() const { return false; }
    virtual void setFormControlValueMatchesRenderer(bool) { }

    virtual const AtomicString& formControlName() const { return nullAtom; }
    virtual const AtomicString& formControlType() const { return nullAtom; }

    virtual bool saveFormControlState(String&) const { return false; }
    virtual void restoreFormControlState(const String&) { }

private:
    virtual void createAttributeMap() const;

    virtual void updateStyleAttribute() const {}

#if ENABLE(SVG)
    virtual void updateAnimatedSVGAttribute(const String&) const {}
#endif

    void updateFocusAppearanceSoonAfterAttach();
    void cancelFocusAppearanceUpdate();

    virtual const AtomicString& virtualPrefix() const { return prefix(); }
    virtual const AtomicString& virtualLocalName() const { return localName(); }
    virtual const AtomicString& virtualNamespaceURI() const { return namespaceURI(); }
    
    // cloneNode is private so that non-virtual cloneElementWithChildren and cloneElementWithoutChildren
    // are used instead.
    virtual PassRefPtr<Node> cloneNode(bool deep);

    QualifiedName m_tagName;
    virtual NodeRareData* createRareData();

protected:
    ElementRareData* rareData() const;
    ElementRareData* ensureRareData();
    
    mutable RefPtr<NamedNodeMap> namedAttrMap;
};
    
inline bool Node::hasTagName(const QualifiedName& name) const
{
    return isElementNode() && static_cast<const Element*>(this)->hasTagName(name);
}

inline bool Node::hasAttributes() const
{
    return isElementNode() && static_cast<const Element*>(this)->hasAttributes();
}

inline NamedNodeMap* Node::attributes() const
{
    return isElementNode() ? static_cast<const Element*>(this)->attributes() : 0;
}

inline Element* Node::parentElement() const
{
    Node* parent = parentNode();
    return parent && parent->isElementNode() ? static_cast<Element*>(parent) : 0;
}

} //namespace

#endif
