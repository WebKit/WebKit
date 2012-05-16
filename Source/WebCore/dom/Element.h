/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "CollectionType.h"
#include "Document.h"
#include "ElementAttributeData.h"
#include "FragmentScriptingPermission.h"
#include "HTMLNames.h"
#include "ScrollTypes.h"

namespace WebCore {

class Attribute;
class ClientRect;
class ClientRectList;
class DOMStringMap;
class DOMTokenList;
class ElementRareData;
class IntSize;
class ShadowRoot;
class ElementShadow;
class WebKitAnimationList;

enum SpellcheckAttributeState {
    SpellcheckAttributeTrue,
    SpellcheckAttributeFalse,
    SpellcheckAttributeDefault
};

class Element : public ContainerNode {
public:
    static PassRefPtr<Element> create(const QualifiedName&, Document*);
    virtual ~Element();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(click);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(contextmenu);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dblclick);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragenter);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragover);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragleave);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(drop);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(drag);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(input);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(invalid);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(keydown);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(keypress);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(keyup);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mousedown);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mousemove);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseout);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseover);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseup);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mousewheel);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(scroll);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(select);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(submit);

    // These four attribute event handler attributes are overridden by HTMLBodyElement
    // and HTMLFrameSetElement to forward to the DOMWindow.
    DECLARE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(blur);
    DECLARE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(error);
    DECLARE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(focus);
    DECLARE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(load);

    // WebKit extensions
    DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecut);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(cut);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecopy);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(copy);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(beforepaste);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(paste);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(reset);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(search);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(selectstart);
#if ENABLE(TOUCH_EVENTS)
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchmove);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchcancel);
#endif
#if ENABLE(FULLSCREEN_API)
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitfullscreenchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitfullscreenerror);
#endif

    bool hasAttribute(const QualifiedName&) const;
    const AtomicString& getAttribute(const QualifiedName&) const;
    void setAttribute(const QualifiedName&, const AtomicString& value, EInUpdateStyleAttribute = NotInUpdateStyleAttribute);
    void removeAttribute(const QualifiedName&);
    void removeAttribute(size_t index);

    // Typed getters and setters for language bindings.
    int getIntegralAttribute(const QualifiedName& attributeName) const;
    void setIntegralAttribute(const QualifiedName& attributeName, int value);
    unsigned getUnsignedIntegralAttribute(const QualifiedName& attributeName) const;
    void setUnsignedIntegralAttribute(const QualifiedName& attributeName, unsigned value);

    // Call this to get the value of an attribute that is known not to be the style
    // attribute or one of the SVG animatable attributes.
    bool fastHasAttribute(const QualifiedName&) const;
    const AtomicString& fastGetAttribute(const QualifiedName&) const;
#ifndef NDEBUG
    bool fastAttributeLookupAllowed(const QualifiedName&) const;
#endif

#ifdef DUMP_NODE_STATISTICS
    bool hasNamedNodeMap() const;
#endif
    bool hasAttributes() const;
    // This variant will not update the potentially invalid attributes. To be used when not interested
    // in style attribute or one of the SVG animation attributes.
    bool hasAttributesWithoutUpdate() const;

    bool hasAttribute(const String& name) const;
    bool hasAttributeNS(const String& namespaceURI, const String& localName) const;

    const AtomicString& getAttribute(const String& name) const;
    const AtomicString& getAttributeNS(const String& namespaceURI, const String& localName) const;

    void setAttribute(const AtomicString& name, const AtomicString& value, ExceptionCode&);
    void setAttributeNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, const AtomicString& value, ExceptionCode&, FragmentScriptingPermission = FragmentScriptingAllowed);

    bool isIdAttributeName(const QualifiedName&) const;
    const AtomicString& getIdAttribute() const;
    void setIdAttribute(const AtomicString&);

    const AtomicString& getNameAttribute() const;

    // Call this to get the value of the id attribute for style resolution purposes.
    // The value will already be lowercased if the document is in compatibility mode,
    // so this function is not suitable for non-style uses.
    const AtomicString& idForStyleResolution() const;

    // Internal methods that assume the existence of attribute storage, one should use hasAttributes()
    // before calling them.
    size_t attributeCount() const;
    Attribute* attributeItem(unsigned index) const;
    Attribute* getAttributeItem(const QualifiedName&) const;
    size_t getAttributeItemIndex(const QualifiedName& name) const { return attributeData()->getAttributeItemIndex(name); }
    size_t getAttributeItemIndex(const String& name, bool shouldIgnoreAttributeCase) const { return attributeData()->getAttributeItemIndex(name, shouldIgnoreAttributeCase); }

    void scrollIntoView(bool alignToTop = true);
    void scrollIntoViewIfNeeded(bool centerIfNeeded = true);

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
    virtual int scrollLeft();
    virtual int scrollTop();
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);
    virtual int scrollWidth();
    virtual int scrollHeight();

    IntRect boundsInRootViewSpace();

    PassRefPtr<ClientRectList> getClientRects();
    PassRefPtr<ClientRect> getBoundingClientRect();
    
    // Returns the absolute bounding box translated into screen coordinates:
    IntRect screenRect() const;

    void removeAttribute(const String& name);
    void removeAttributeNS(const String& namespaceURI, const String& localName);

    PassRefPtr<Attr> detachAttribute(size_t index);

    PassRefPtr<Attr> getAttributeNode(const String& name);
    PassRefPtr<Attr> getAttributeNodeNS(const String& namespaceURI, const String& localName);
    PassRefPtr<Attr> setAttributeNode(Attr*, ExceptionCode&);
    PassRefPtr<Attr> setAttributeNodeNS(Attr*, ExceptionCode&);
    PassRefPtr<Attr> removeAttributeNode(Attr*, ExceptionCode&);

    PassRefPtr<Attr> attrIfExists(const QualifiedName&);
    PassRefPtr<Attr> ensureAttr(const QualifiedName&);
    
    virtual CSSStyleDeclaration* style();

    const QualifiedName& tagQName() const { return m_tagName; }
    String tagName() const { return nodeName(); }
    bool hasTagName(const QualifiedName& tagName) const { return m_tagName.matches(tagName); }
    
    // A fast function for checking the local name against another atomic string.
    bool hasLocalName(const AtomicString& other) const { return m_tagName.localName() == other; }
    bool hasLocalName(const QualifiedName& other) const { return m_tagName.localName() == other.localName(); }

    const AtomicString& localName() const { return m_tagName.localName(); }
    const AtomicString& prefix() const { return m_tagName.prefix(); }
    const AtomicString& namespaceURI() const { return m_tagName.namespaceURI(); }

    virtual KURL baseURI() const;

    virtual String nodeName() const;

    PassRefPtr<Element> cloneElementWithChildren();
    PassRefPtr<Element> cloneElementWithoutChildren();

    void normalizeAttributes();
    String nodeNamePreservingCase() const;

    void setBooleanAttribute(const QualifiedName& name, bool);

    // For exposing to DOM only.
    NamedNodeMap* attributes() const;

    // This method is called whenever an attribute is added, changed or removed.
    virtual void attributeChanged(const Attribute&);

    // Only called by the parser immediately after element construction.
    void parserSetAttributes(const Vector<Attribute>&, FragmentScriptingPermission);

    ElementAttributeData* attributeData() const { return m_attributeData.get(); }
    ElementAttributeData* ensureAttributeData() const;
    ElementAttributeData* updatedAttributeData() const;
    ElementAttributeData* ensureUpdatedAttributeData() const;

    void setAttributesFromElement(const Element&);
    bool hasEquivalentAttributes(const Element* other) const;

    virtual void copyNonAttributeProperties(const Element* source);

    virtual void attach();
    virtual void detach();
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    void recalcStyle(StyleChange = NoChange);

    ElementShadow* shadow() const;
    ElementShadow* ensureShadow();

    // FIXME: Remove Element::ensureShadowRoot
    // https://bugs.webkit.org/show_bug.cgi?id=77608
    ShadowRoot* ensureShadowRoot();

    virtual const AtomicString& shadowPseudoId() const;
    void setShadowPseudoId(const AtomicString&, ExceptionCode& = ASSERT_NO_EXCEPTION);

    RenderStyle* computedStyle(PseudoId = NOPSEUDO);

    void setStyleAffectedByEmpty();
    bool styleAffectedByEmpty() const;

    AtomicString computeInheritedLanguage() const;

    virtual void accessKeyAction(bool /*sendToAnyEvent*/) { }

    virtual bool isURLAttribute(const Attribute&) const { return false; }

    KURL getURLAttribute(const QualifiedName&) const;
    KURL getNonEmptyURLAttribute(const QualifiedName&) const;

    virtual const QualifiedName& imageSourceAttributeName() const;
    virtual String target() const { return String(); }

    virtual void focus(bool restorePreviousSelection = true);
    virtual void updateFocusAppearance(bool restorePreviousSelection);
    void blur();

    String innerText();
    String outerText();
 
    virtual String title() const;

    void updateId(const AtomicString& oldId, const AtomicString& newId);
    void updateName(const AtomicString& oldName, const AtomicString& newName);

    void willModifyAttribute(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue);
    void willRemoveAttribute(const QualifiedName&, const AtomicString& value);
    void didAddAttribute(const Attribute&);
    void didModifyAttribute(const Attribute&);
    void didRemoveAttribute(const QualifiedName&);

    LayoutSize minimumSizeForResizing() const;
    void setMinimumSizeForResizing(const LayoutSize&);

    // Use Document::registerForDocumentActivationCallbacks() to subscribe to these
    virtual void documentWillSuspendForPageCache() { }
    virtual void documentDidResumeFromPageCache() { }

    // Use Document::registerForMediaVolumeCallbacks() to subscribe to this
    virtual void mediaVolumeDidChange() { }

    // Use Document::registerForPrivateBrowsingStateChangedCallbacks() to subscribe to this.
    virtual void privateBrowsingStateDidChange() { }

    virtual void didBecomeFullscreenElement() { }
    virtual void willStopBeingFullscreenElement() { }

    bool isFinishedParsingChildren() const { return isParsingChildrenFinished(); }
    virtual void finishParsingChildren();
    virtual void beginParsingChildren();

    // ElementTraversal API
    Element* firstElementChild() const;
    Element* lastElementChild() const;
    Element* previousElementSibling() const;
    Element* nextElementSibling() const;
    unsigned childElementCount() const;

    bool webkitMatchesSelector(const String& selectors, ExceptionCode&);

    DOMTokenList* classList();
    DOMTokenList* optionalClassList() const;

    DOMStringMap* dataset();

#if ENABLE(MATHML)
    virtual bool isMathMLElement() const { return false; }
#else
    static bool isMathMLElement() { return false; }
#endif

#if ENABLE(VIDEO)
    virtual bool isMediaElement() const { return false; }
#endif

#if ENABLE(INPUT_SPEECH)
    virtual bool isInputFieldSpeechButtonElement() const { return false; }
#endif

    virtual bool isFormControlElement() const { return false; }
    virtual bool isEnabledFormControl() const { return true; }
    virtual bool isReadOnlyFormControl() const { return false; }
    virtual bool isSpinButtonElement() const { return false; }
    virtual bool isTextFormControl() const { return false; }
    virtual bool isOptionalFormControl() const { return false; }
    virtual bool isRequiredFormControl() const { return false; }
    virtual bool isDefaultButtonForForm() const { return false; }
    virtual bool willValidate() const { return false; }
    virtual bool isValidFormControlElement() { return false; }
    virtual bool hasUnacceptableValue() const { return false; }
    virtual bool isInRange() const { return false; }
    virtual bool isOutOfRange() const { return false; }
    virtual bool isFrameElementBase() const { return false; }
    virtual bool isTextFieldDecoration() const { return false; }

    virtual bool canContainRangeEndPoint() const { return true; }

    virtual const AtomicString& formControlName() const { return nullAtom; }
    virtual const AtomicString& formControlType() const { return nullAtom; }

    virtual bool wasChangedSinceLastFormControlChangeEvent() const;
    virtual void setChangedSinceLastFormControlChangeEvent(bool);
    virtual void dispatchFormControlChangeEvent() { }

#if ENABLE(SVG)
    virtual bool childShouldCreateRenderer(const NodeRenderingContext&) const;
#endif
    
#if ENABLE(FULLSCREEN_API)
    enum {
        ALLOW_KEYBOARD_INPUT = 1 << 0,
        LEGACY_MOZILLA_REQUEST = 1 << 1,
    };
    
    void webkitRequestFullScreen(unsigned short flags);
    virtual bool containsFullScreenElement() const;
    virtual void setContainsFullScreenElement(bool);
    virtual void setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(bool);

    // W3C API
    void webkitRequestFullscreen();
#endif

    virtual bool isSpellCheckingEnabled() const;

    PassRefPtr<WebKitAnimationList> webkitGetAnimations() const;
    
    PassRefPtr<RenderStyle> styleForRenderer();

    const AtomicString& webkitRegionOverflow() const;

    bool hasID() const;
    bool hasClass() const;

    IntSize savedLayerScrollOffset() const;
    void setSavedLayerScrollOffset(const IntSize&);

protected:
    Element(const QualifiedName& tagName, Document* document, ConstructionType type)
        : ContainerNode(document, type)
        , m_tagName(tagName)
    {
    }

    virtual InsertionNotificationRequest insertedInto(Node*) OVERRIDE;
    virtual void removedFrom(Node*) OVERRIDE;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);
    virtual bool willRecalcStyle(StyleChange) { return true; }
    virtual void didRecalcStyle(StyleChange) { }
    virtual PassRefPtr<RenderStyle> customStyleForRenderer();

    virtual bool shouldRegisterAsNamedItem() const { return false; }
    virtual bool shouldRegisterAsExtraNamedItem() const { return false; }

    HTMLCollection* ensureCachedHTMLCollection(CollectionType);

private:
    void updateInvalidAttributes() const;

    void scrollByUnits(int units, ScrollGranularity);

    virtual void setPrefix(const AtomicString&, ExceptionCode&);
    virtual NodeType nodeType() const;
    virtual bool childTypeAllowed(NodeType) const;

    void setAttributeInternal(size_t index, const QualifiedName&, const AtomicString& value, EInUpdateStyleAttribute);

#ifndef NDEBUG
    virtual void formatForDebugger(char* buffer, unsigned length) const;
#endif

    bool pseudoStyleCacheIsInvalid(const RenderStyle* currentStyle, RenderStyle* newStyle);

    void createAttributeData() const;

    virtual void updateStyleAttribute() const { }

#if ENABLE(SVG)
    virtual void updateAnimatedSVGAttribute(const QualifiedName&) const { }
#endif

    void cancelFocusAppearanceUpdate();

    virtual const AtomicString& virtualPrefix() const { return prefix(); }
    virtual const AtomicString& virtualLocalName() const { return localName(); }
    virtual const AtomicString& virtualNamespaceURI() const { return namespaceURI(); }
    virtual RenderStyle* virtualComputedStyle(PseudoId pseudoElementSpecifier = NOPSEUDO) { return computedStyle(pseudoElementSpecifier); }
    
    // cloneNode is private so that non-virtual cloneElementWithChildren and cloneElementWithoutChildren
    // are used instead.
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual PassRefPtr<Element> cloneElementWithoutAttributesAndChildren();

    QualifiedName m_tagName;
    virtual OwnPtr<NodeRareData> createRareData();

    ElementRareData* rareData() const;
    ElementRareData* ensureRareData();

    SpellcheckAttributeState spellcheckAttributeState() const;

    void updateNamedItemRegistration(const AtomicString& oldName, const AtomicString& newName);
    void updateExtraNamedItemRegistration(const AtomicString& oldName, const AtomicString& newName);

    void unregisterNamedFlowContentNode();

private:
    mutable OwnPtr<ElementAttributeData> m_attributeData;
};
    
inline Element* toElement(Node* node)
{
    ASSERT(!node || node->isElementNode());
    return static_cast<Element*>(node);
}

inline const Element* toElement(const Node* node)
{
    ASSERT(!node || node->isElementNode());
    return static_cast<const Element*>(node);
}

// This will catch anyone doing an unnecessary cast.
void toElement(const Element*);

inline bool Node::hasTagName(const QualifiedName& name) const
{
    return isElementNode() && toElement(this)->hasTagName(name);
}
    
inline bool Node::hasLocalName(const AtomicString& name) const
{
    return isElementNode() && toElement(this)->hasLocalName(name);
}

inline bool Node::hasAttributes() const
{
    return isElementNode() && toElement(this)->hasAttributes();
}

inline NamedNodeMap* Node::attributes() const
{
    return isElementNode() ? toElement(this)->attributes() : 0;
}

inline Element* Node::parentElement() const
{
    ContainerNode* parent = parentNode();
    return parent && parent->isElementNode() ? toElement(parent) : 0;
}

inline Element* Element::previousElementSibling() const
{
    Node* n = previousSibling();
    while (n && !n->isElementNode())
        n = n->previousSibling();
    return static_cast<Element*>(n);
}

inline Element* Element::nextElementSibling() const
{
    Node* n = nextSibling();
    while (n && !n->isElementNode())
        n = n->nextSibling();
    return static_cast<Element*>(n);
}

inline ElementAttributeData* Element::ensureAttributeData() const
{
    if (!m_attributeData)
        createAttributeData();
    return m_attributeData.get();
}

inline ElementAttributeData* Element::updatedAttributeData() const
{
    updateInvalidAttributes();
    return attributeData();
}

inline ElementAttributeData* Element::ensureUpdatedAttributeData() const
{
    updateInvalidAttributes();
    return ensureAttributeData();
}

inline void Element::setAttributesFromElement(const Element& other)
{
    if (ElementAttributeData* attributeData = other.updatedAttributeData())
        ensureUpdatedAttributeData()->setAttributes(*attributeData, this);
}

inline void Element::updateName(const AtomicString& oldName, const AtomicString& newName)
{
    if (!inDocument())
        return;

    if (oldName == newName)
        return;

    if (shouldRegisterAsNamedItem())
        updateNamedItemRegistration(oldName, newName);
}

inline void Element::updateId(const AtomicString& oldId, const AtomicString& newId)
{
    if (!inDocument())
        return;

    if (oldId == newId)
        return;

    TreeScope* scope = treeScope();
    if (!oldId.isEmpty())
        scope->removeElementById(oldId, this);
    if (!newId.isEmpty())
        scope->addElementById(newId, this);

    if (shouldRegisterAsExtraNamedItem())
        updateExtraNamedItemRegistration(oldId, newId);
}

inline void Element::willRemoveAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!value.isNull())
        willModifyAttribute(name, value, nullAtom);
}

inline bool Element::fastHasAttribute(const QualifiedName& name) const
{
    ASSERT(fastAttributeLookupAllowed(name));
    return m_attributeData && getAttributeItem(name);
}

inline const AtomicString& Element::fastGetAttribute(const QualifiedName& name) const
{
    ASSERT(fastAttributeLookupAllowed(name));
    if (m_attributeData) {
        if (Attribute* attribute = getAttributeItem(name))
            return attribute->value();
    }
    return nullAtom;
}

inline bool Element::hasAttributesWithoutUpdate() const
{
    return m_attributeData && !m_attributeData->isEmpty();
}

inline const AtomicString& Element::idForStyleResolution() const
{
    ASSERT(hasID());
    return m_attributeData->idForStyleResolution();
}

inline bool Element::isIdAttributeName(const QualifiedName& attributeName) const
{
    // FIXME: This check is probably not correct for the case where the document has an id attribute
    // with a non-null namespace, because it will return false, a false negative, if the prefixes
    // don't match but the local name and namespace both do. However, since this has been like this
    // for a while and the code paths may be hot, we'll have to measure performance if we fix it.
    return attributeName == document()->idAttributeName();
}

inline const AtomicString& Element::getIdAttribute() const
{
    return hasID() ? fastGetAttribute(document()->idAttributeName()) : nullAtom;
}

inline const AtomicString& Element::getNameAttribute() const
{
    return hasName() ? fastGetAttribute(HTMLNames::nameAttr) : nullAtom;
}

inline void Element::setIdAttribute(const AtomicString& value)
{
    setAttribute(document()->idAttributeName(), value);
}

inline size_t Element::attributeCount() const
{
    ASSERT(m_attributeData);
    return m_attributeData->length();
}

inline Attribute* Element::attributeItem(unsigned index) const
{
    ASSERT(m_attributeData);
    return m_attributeData->attributeItem(index);
}

inline Attribute* Element::getAttributeItem(const QualifiedName& name) const
{
    ASSERT(m_attributeData);
    return m_attributeData->getAttributeItem(name);
}

inline void Element::updateInvalidAttributes() const
{
    if (!isStyleAttributeValid())
        updateStyleAttribute();

#if ENABLE(SVG)
    if (!areSVGAttributesValid())
        updateAnimatedSVGAttribute(anyQName());
#endif
}

inline Element* firstElementChild(const ContainerNode* container)
{
    ASSERT_ARG(container, container);
    Node* child = container->firstChild();
    while (child && !child->isElementNode())
        child = child->nextSibling();
    return static_cast<Element*>(child);
}

inline bool Element::hasID() const
{
    return attributeData() && attributeData()->hasID();
}

inline bool Element::hasClass() const
{
    return attributeData() && attributeData()->hasClass();
}

// Put here to make them inline.
inline bool Node::hasID() const
{
    return isElementNode() && toElement(this)->hasID();
}

inline bool Node::hasClass() const
{
    return isElementNode() && toElement(this)->hasClass();
}

inline bool isShadowHost(const Node* node)
{
    return node && node->isElementNode() && toElement(node)->shadow();
}

} // namespace

#endif
