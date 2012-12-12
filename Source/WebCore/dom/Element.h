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
class ElementShadow;
class IntSize;
class Locale;
class PseudoElement;
class RenderRegion;
class ShadowRoot;

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
    void setAttribute(const QualifiedName&, const AtomicString& value);
    void setSynchronizedLazyAttribute(const QualifiedName&, const AtomicString& value);
    void removeAttribute(const QualifiedName&);

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

    bool hasAttribute(const AtomicString& name) const;
    bool hasAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const;

    const AtomicString& getAttribute(const AtomicString& name) const;
    const AtomicString& getAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const;

    void setAttribute(const AtomicString& name, const AtomicString& value, ExceptionCode&);
    static bool parseAttributeName(QualifiedName&, const AtomicString& namespaceURI, const AtomicString& qualifiedName, ExceptionCode&);
    void setAttributeNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, const AtomicString& value, ExceptionCode&);

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
    const Attribute* attributeItem(unsigned index) const;
    const Attribute* getAttributeItem(const QualifiedName&) const;
    Attribute* getAttributeItem(const QualifiedName&);
    size_t getAttributeItemIndex(const QualifiedName& name) const { return attributeData()->getAttributeItemIndex(name); }
    size_t getAttributeItemIndex(const AtomicString& name, bool shouldIgnoreAttributeCase) const { return attributeData()->getAttributeItemIndex(name, shouldIgnoreAttributeCase); }

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

    void removeAttribute(const AtomicString& name);
    void removeAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName);

    PassRefPtr<Attr> detachAttribute(size_t index);

    PassRefPtr<Attr> getAttributeNode(const AtomicString& name);
    PassRefPtr<Attr> getAttributeNodeNS(const AtomicString& namespaceURI, const AtomicString& localName);
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
    virtual void attributeChanged(const QualifiedName&, const AtomicString&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) { }

    // Only called by the parser immediately after element construction.
    void parserSetAttributes(const Vector<Attribute>&, FragmentScriptingPermission);

    const ElementAttributeData* attributeData() const { return m_attributeData.get(); }
    ElementAttributeData* mutableAttributeData();
    const ElementAttributeData* ensureAttributeData();
    const ElementAttributeData* updatedAttributeData() const;
    const ElementAttributeData* ensureUpdatedAttributeData() const;

    // Clones attributes only.
    void cloneAttributesFromElement(const Element&);

    // Clones all attribute-derived data, including subclass specifics (through copyNonAttributeProperties.)
    void cloneDataFromElement(const Element&);

    bool hasEquivalentAttributes(const Element* other) const;

    virtual void copyNonAttributePropertiesFromElement(const Element&) { }

    virtual void attach();
    virtual void detach();
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual bool rendererIsNeeded(const NodeRenderingContext&);
    void recalcStyle(StyleChange = NoChange);

    ElementShadow* shadow() const;
    ElementShadow* ensureShadow();
    PassRefPtr<ShadowRoot> createShadowRoot(ExceptionCode&);
    ShadowRoot* shadowRoot() const;

    virtual void willAddAuthorShadowRoot() { }
    virtual bool areAuthorShadowsAllowed() const { return true; }

    ShadowRoot* userAgentShadowRoot() const;

    virtual const AtomicString& shadowPseudoId() const;

    RenderStyle* computedStyle(PseudoId = NOPSEUDO);

    // Methods for indicating the style is affected by dynamic updates (e.g., children changing, our position changing in our sibling list, etc.)
    bool styleAffectedByEmpty() const { return hasRareData() && rareDataStyleAffectedByEmpty(); }
    bool childrenAffectedByHover() const { return hasRareData() && rareDataChildrenAffectedByHover(); }
    bool childrenAffectedByActive() const { return hasRareData() && rareDataChildrenAffectedByActive(); }
    bool childrenAffectedByDrag() const { return hasRareData() && rareDataChildrenAffectedByDrag(); }
    bool childrenAffectedByPositionalRules() const { return hasRareData() && (rareDataChildrenAffectedByForwardPositionalRules() || rareDataChildrenAffectedByBackwardPositionalRules()); }
    bool childrenAffectedByFirstChildRules() const { return hasRareData() && rareDataChildrenAffectedByFirstChildRules(); }
    bool childrenAffectedByLastChildRules() const { return hasRareData() && rareDataChildrenAffectedByLastChildRules(); }
    bool childrenAffectedByDirectAdjacentRules() const { return hasRareData() && rareDataChildrenAffectedByDirectAdjacentRules(); }
    bool childrenAffectedByForwardPositionalRules() const { return hasRareData() && rareDataChildrenAffectedByForwardPositionalRules(); }
    bool childrenAffectedByBackwardPositionalRules() const { return hasRareData() && rareDataChildrenAffectedByBackwardPositionalRules(); }
    unsigned childIndex() const { return hasRareData() ? rareDataChildIndex() : 0; }

    void setStyleAffectedByEmpty();
    void setChildrenAffectedByHover(bool);
    void setChildrenAffectedByActive(bool);
    void setChildrenAffectedByDrag(bool);
    void setChildrenAffectedByFirstChildRules();
    void setChildrenAffectedByLastChildRules();
    void setChildrenAffectedByDirectAdjacentRules();
    void setChildrenAffectedByForwardPositionalRules();
    void setChildrenAffectedByBackwardPositionalRules();
    void setChildIndex(unsigned);

    void setIsInCanvasSubtree(bool);
    bool isInCanvasSubtree() const;

    AtomicString computeInheritedLanguage() const;
    Locale& locale() const;

    virtual void accessKeyAction(bool /*sendToAnyEvent*/) { }

    virtual bool isURLAttribute(const Attribute&) const { return false; }

    KURL getURLAttribute(const QualifiedName&) const;
    KURL getNonEmptyURLAttribute(const QualifiedName&) const;

    virtual const QualifiedName& imageSourceAttributeName() const;
    virtual String target() const { return String(); }

    virtual void focus(bool restorePreviousSelection = true);
    virtual void updateFocusAppearance(bool restorePreviousSelection);
    virtual void blur();

    String innerText();
    String outerText();
 
    virtual String title() const;

    const AtomicString& pseudo() const;
    void setPseudo(const AtomicString&);

    void updateId(const AtomicString& oldId, const AtomicString& newId);
    void updateId(TreeScope*, const AtomicString& oldId, const AtomicString& newId);
    void updateName(const AtomicString& oldName, const AtomicString& newName);
    void updateLabel(TreeScope*, const AtomicString& oldForAttributeValue, const AtomicString& newForAttributeValue);

    void removeCachedHTMLCollection(HTMLCollection*, CollectionType);

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

    PseudoElement* beforePseudoElement() const;
    PseudoElement* afterPseudoElement() const;

    // ElementTraversal API
    Element* firstElementChild() const;
    Element* lastElementChild() const;
    Element* previousElementSibling() const;
    Element* nextElementSibling() const;
    unsigned childElementCount() const;

    virtual bool matchesReadOnlyPseudoClass() const;
    virtual bool matchesReadWritePseudoClass() const;
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
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    virtual bool isDateTimeFieldElement() const;
#endif

    virtual bool isFormControlElement() const { return false; }
    virtual bool isEnabledFormControl() const { return true; }
    virtual bool isSpinButtonElement() const { return false; }
    virtual bool isTextFormControl() const { return false; }
    virtual bool isOptionalFormControl() const { return false; }
    virtual bool isRequiredFormControl() const { return false; }
    virtual bool isDefaultButtonForForm() const { return false; }
    virtual bool willValidate() const { return false; }
    virtual bool isValidFormControlElement() { return false; }
    virtual bool isInRange() const { return false; }
    virtual bool isOutOfRange() const { return false; }
    virtual bool isFrameElementBase() const { return false; }
    virtual bool isTextFieldDecoration() const { return false; }

    virtual bool canContainRangeEndPoint() const { return true; }

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

#if ENABLE(DIALOG_ELEMENT)
    bool isInTopLayer() const;
    void setIsInTopLayer(bool);
#endif

#if ENABLE(POINTER_LOCK)
    void webkitRequestPointerLock();
#endif

    virtual bool isSpellCheckingEnabled() const;

    PassRefPtr<RenderStyle> styleForRenderer();

    RenderRegion* renderRegion() const;
#if ENABLE(CSS_REGIONS)
    const AtomicString& webkitRegionOverset() const;
    Vector<RefPtr<Range> > webkitGetRegionFlowRanges() const;
#endif

    bool hasID() const;
    bool hasClass() const;
    const SpaceSplitString& classNames() const;

    IntSize savedLayerScrollOffset() const;
    void setSavedLayerScrollOffset(const IntSize&);

    virtual void reportMemoryUsage(MemoryObjectInfo*) const OVERRIDE;

protected:
    Element(const QualifiedName& tagName, Document* document, ConstructionType type)
        : ContainerNode(document, type)
        , m_tagName(tagName)
    {
    }

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    virtual bool willRecalcStyle(StyleChange);
    virtual void didRecalcStyle(StyleChange);
    virtual PassRefPtr<RenderStyle> customStyleForRenderer();

    virtual bool shouldRegisterAsNamedItem() const { return false; }
    virtual bool shouldRegisterAsExtraNamedItem() const { return false; }

    PassRefPtr<HTMLCollection> ensureCachedHTMLCollection(CollectionType);
    HTMLCollection* cachedHTMLCollection(CollectionType);

    // classAttributeChanged() exists to share code between
    // parseAttribute (called via setAttribute()) and
    // svgAttributeChanged (called when element.className.baseValue is set)
    void classAttributeChanged(const AtomicString& newClassString);

private:
    void updatePseudoElement(PseudoId, StyleChange = NoChange);
    PassRefPtr<PseudoElement> createPseudoElementIfNeeded(PseudoId);

    // FIXME: Remove the need for Attr to call willModifyAttribute/didModifyAttribute.
    friend class Attr;

    enum SynchronizationOfLazyAttribute { NotInSynchronizationOfLazyAttribute = 0, InSynchronizationOfLazyAttribute };

    void didAddAttribute(const QualifiedName&, const AtomicString&);
    void willModifyAttribute(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue);
    void didModifyAttribute(const QualifiedName&, const AtomicString&);
    void didRemoveAttribute(const QualifiedName&);

    void updateInvalidAttributes() const;

    void scrollByUnits(int units, ScrollGranularity);

    virtual void setPrefix(const AtomicString&, ExceptionCode&);
    virtual NodeType nodeType() const;
    virtual bool childTypeAllowed(NodeType) const;

    void setAttributeInternal(size_t index, const QualifiedName&, const AtomicString& value, SynchronizationOfLazyAttribute);
    void addAttributeInternal(const QualifiedName&, const AtomicString& value, SynchronizationOfLazyAttribute);
    void removeAttributeInternal(size_t index, SynchronizationOfLazyAttribute);

#ifndef NDEBUG
    virtual void formatForDebugger(char* buffer, unsigned length) const;
#endif

    bool pseudoStyleCacheIsInvalid(const RenderStyle* currentStyle, RenderStyle* newStyle);

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
    virtual PassRefPtr<Node> cloneNode(bool deep) OVERRIDE;
    virtual PassRefPtr<Element> cloneElementWithoutAttributesAndChildren();

    QualifiedName m_tagName;
    virtual PassOwnPtr<NodeRareData> createRareData();
    bool rareDataStyleAffectedByEmpty() const;
    bool rareDataChildrenAffectedByHover() const;
    bool rareDataChildrenAffectedByActive() const;
    bool rareDataChildrenAffectedByDrag() const;
    bool rareDataChildrenAffectedByFirstChildRules() const;
    bool rareDataChildrenAffectedByLastChildRules() const;
    bool rareDataChildrenAffectedByDirectAdjacentRules() const;
    bool rareDataChildrenAffectedByForwardPositionalRules() const;
    bool rareDataChildrenAffectedByBackwardPositionalRules() const;
    unsigned rareDataChildIndex() const;

    SpellcheckAttributeState spellcheckAttributeState() const;

    void updateNamedItemRegistration(const AtomicString& oldName, const AtomicString& newName);
    void updateExtraNamedItemRegistration(const AtomicString& oldName, const AtomicString& newName);

    void unregisterNamedFlowContentNode();

    void createMutableAttributeData();

    bool shouldInvalidateDistributionWhenAttributeChanged(ElementShadow*, const QualifiedName&, const AtomicString&);

    ElementRareData* elementRareData() const;
    ElementRareData* ensureElementRareData();

    void detachAllAttrNodesFromElement();
    void detachAttrNodeFromElementWithValue(Attr*, const AtomicString& value);

    void createRendererIfNeeded();

    RefPtr<ElementAttributeData> m_attributeData;
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

inline const ElementAttributeData* Element::updatedAttributeData() const
{
    updateInvalidAttributes();
    return attributeData();
}

inline const ElementAttributeData* Element::ensureAttributeData()
{
    if (attributeData())
        return attributeData();
    return mutableAttributeData();
}

inline const ElementAttributeData* Element::ensureUpdatedAttributeData() const
{
    updateInvalidAttributes();
    if (attributeData())
        return attributeData();
    return const_cast<Element*>(this)->mutableAttributeData();
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

    updateId(treeScope(), oldId, newId);
}

inline void Element::updateId(TreeScope* scope, const AtomicString& oldId, const AtomicString& newId)
{
    ASSERT(inDocument());
    ASSERT(oldId != newId);

    if (!oldId.isEmpty())
        scope->removeElementById(oldId, this);
    if (!newId.isEmpty())
        scope->addElementById(newId, this);

    if (shouldRegisterAsExtraNamedItem())
        updateExtraNamedItemRegistration(oldId, newId);
}

inline bool Element::fastHasAttribute(const QualifiedName& name) const
{
    ASSERT(fastAttributeLookupAllowed(name));
    return attributeData() && getAttributeItem(name);
}

inline const AtomicString& Element::fastGetAttribute(const QualifiedName& name) const
{
    ASSERT(fastAttributeLookupAllowed(name));
    if (attributeData()) {
        if (const Attribute* attribute = getAttributeItem(name))
            return attribute->value();
    }
    return nullAtom;
}

inline bool Element::hasAttributesWithoutUpdate() const
{
    return attributeData() && !attributeData()->isEmpty();
}

inline const AtomicString& Element::idForStyleResolution() const
{
    ASSERT(hasID());
    return attributeData()->idForStyleResolution();
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

inline const SpaceSplitString& Element::classNames() const
{
    ASSERT(hasClass());
    ASSERT(attributeData());
    return attributeData()->classNames();
}

inline size_t Element::attributeCount() const
{
    ASSERT(attributeData());
    return attributeData()->length();
}

inline const Attribute* Element::attributeItem(unsigned index) const
{
    ASSERT(attributeData());
    return attributeData()->attributeItem(index);
}

inline const Attribute* Element::getAttributeItem(const QualifiedName& name) const
{
    ASSERT(attributeData());
    return attributeData()->getAttributeItem(name);
}

inline Attribute* Element::getAttributeItem(const QualifiedName& name)
{
    ASSERT(attributeData());
    return mutableAttributeData()->getAttributeItem(name);
}

inline void Element::updateInvalidAttributes() const
{
    if (!attributeData())
        return;

    if (attributeData()->m_styleAttributeIsDirty)
        updateStyleAttribute();

#if ENABLE(SVG)
    if (attributeData()->m_animatedSVGAttributesAreDirty)
        updateAnimatedSVGAttribute(anyQName());
#endif
}

inline bool Element::hasID() const
{
    return attributeData() && attributeData()->hasID();
}

inline bool Element::hasClass() const
{
    return attributeData() && attributeData()->hasClass();
}

inline ElementAttributeData* Element::mutableAttributeData()
{
    if (!attributeData() || !attributeData()->isMutable())
        createMutableAttributeData();
    return m_attributeData.get();
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
