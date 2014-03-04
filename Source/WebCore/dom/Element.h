/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2013 Apple Inc. All rights reserved.
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
#include "ElementData.h"
#include "HTMLNames.h"
#include "RegionOversetState.h"
#include "ScrollTypes.h"
#include "StyleResolveTree.h"

namespace WebCore {

class ClientRect;
class ClientRectList;
class DatasetDOMStringMap;
class DOMTokenList;
class ElementRareData;
class HTMLDocument;
class IntSize;
class Locale;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformWheelEvent;
class PseudoElement;
class RenderNamedFlowFragment;
class ShadowRoot;

enum AffectedSelectorType {
    AffectedSelectorChecked = 1,
    AffectedSelectorEnabled = 1 << 1,
    AffectedSelectorDisabled = 1 << 2,
    AffectedSelectorIndeterminate = 1 << 3,
    AffectedSelectorLink = 1 << 4,
    AffectedSelectorTarget = 1 << 5,
    AffectedSelectorVisited = 1 << 6
};
typedef int AffectedSelectorMask;

enum SpellcheckAttributeState {
    SpellcheckAttributeTrue,
    SpellcheckAttributeFalse,
    SpellcheckAttributeDefault
};

class Element : public ContainerNode {
public:
    static PassRefPtr<Element> create(const QualifiedName&, Document&);
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
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseenter);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseleave);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mousemove);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseout);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseover);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseup);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mousewheel);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(scroll);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(select);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(submit);
#if ENABLE(WILL_REVEAL_EDGE_EVENTS)
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitwillrevealbottom);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitwillrevealleft);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitwillrevealright);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitwillrevealtop);
#endif
    DEFINE_ATTRIBUTE_EVENT_LISTENER(wheel);

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
#if ENABLE(IOS_GESTURE_EVENTS)
    DEFINE_ATTRIBUTE_EVENT_LISTENER(gesturestart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(gesturechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(gestureend);
#endif
#if ENABLE(FULLSCREEN_API)
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitfullscreenchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitfullscreenerror);
#endif

    bool hasAttribute(const QualifiedName&) const;
    const AtomicString& getAttribute(const QualifiedName&) const;
    void setAttribute(const QualifiedName&, const AtomicString& value);
    void setSynchronizedLazyAttribute(const QualifiedName&, const AtomicString& value);
    bool removeAttribute(const QualifiedName&);

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

    const AtomicString& getIdAttribute() const;
    void setIdAttribute(const AtomicString&);

    const AtomicString& getNameAttribute() const;

    // Call this to get the value of the id attribute for style resolution purposes.
    // The value will already be lowercased if the document is in compatibility mode,
    // so this function is not suitable for non-style uses.
    const AtomicString& idForStyleResolution() const;

    // Internal methods that assume the existence of attribute storage, one should use hasAttributes()
    // before calling them.
    AttributeIteratorAccessor attributesIterator() const { return elementData()->attributesIterator(); }
    unsigned attributeCount() const;
    const Attribute& attributeAt(unsigned index) const;
    const Attribute* findAttributeByName(const QualifiedName&) const;
    unsigned findAttributeIndexByName(const QualifiedName& name) const { return elementData()->findAttributeIndexByName(name); }
    unsigned findAttributeIndexByName(const AtomicString& name, bool shouldIgnoreAttributeCase) const { return elementData()->findAttributeIndexByName(name, shouldIgnoreAttributeCase); }

    void scrollIntoView(bool alignToTop = true);
    void scrollIntoViewIfNeeded(bool centerIfNeeded = true);

    void scrollByLines(int lines);
    void scrollByPages(int pages);

    int offsetLeft();
    int offsetTop();
    int offsetWidth();
    int offsetHeight();

    // FIXME: Replace uses of offsetParent in the platform with calls
    // to the render layer and merge bindingsOffsetParent and offsetParent.
    Element* bindingsOffsetParent();

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
    
    // Returns the absolute bounding box translated into client coordinates.
    IntRect clientRect() const;
    // Returns the absolute bounding box translated into screen coordinates.
    IntRect screenRect() const;

    bool removeAttribute(const AtomicString& name);
    bool removeAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName);

    PassRefPtr<Attr> detachAttribute(unsigned index);

    PassRefPtr<Attr> getAttributeNode(const AtomicString& name);
    PassRefPtr<Attr> getAttributeNodeNS(const AtomicString& namespaceURI, const AtomicString& localName);
    PassRefPtr<Attr> setAttributeNode(Attr*, ExceptionCode&);
    PassRefPtr<Attr> setAttributeNodeNS(Attr*, ExceptionCode&);
    PassRefPtr<Attr> removeAttributeNode(Attr*, ExceptionCode&);

    PassRefPtr<Attr> attrIfExists(const QualifiedName&);
    PassRefPtr<Attr> ensureAttr(const QualifiedName&);

    const Vector<RefPtr<Attr>>& attrNodeList();

    virtual CSSStyleDeclaration* style();

    const QualifiedName& tagQName() const { return m_tagName; }
#if ENABLE(CSS_SELECTOR_JIT)
    static ptrdiff_t tagQNameMemoryOffset() { return OBJECT_OFFSETOF(Element, m_tagName); }
#endif // ENABLE(CSS_SELECTOR_JIT)
    String tagName() const { return nodeName(); }
    bool hasTagName(const QualifiedName& tagName) const { return m_tagName.matches(tagName); }
    
    // A fast function for checking the local name against another atomic string.
    bool hasLocalName(const AtomicString& other) const { return m_tagName.localName() == other; }
    bool hasLocalName(const QualifiedName& other) const { return m_tagName.localName() == other.localName(); }

    virtual const AtomicString& localName() const override final { return m_tagName.localName(); }
    virtual const AtomicString& prefix() const override final { return m_tagName.prefix(); }
    virtual const AtomicString& namespaceURI() const override final { return m_tagName.namespaceURI(); }

    virtual URL baseURI() const override final;

    virtual String nodeName() const override;

    PassRefPtr<Element> cloneElementWithChildren();
    PassRefPtr<Element> cloneElementWithoutChildren();

    void normalizeAttributes();
    String nodeNamePreservingCase() const;

    void setBooleanAttribute(const QualifiedName& name, bool);

    // For exposing to DOM only.
    NamedNodeMap* attributes() const;

    enum AttributeModificationReason {
        ModifiedDirectly,
        ModifiedByCloning
    };

    // This method is called whenever an attribute is added, changed or removed.
    virtual void attributeChanged(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason = ModifiedDirectly);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) { }

    // Only called by the parser immediately after element construction.
    void parserSetAttributes(const Vector<Attribute>&);

    // Remove attributes that might introduce scripting from the vector leaving the element unchanged.
    void stripScriptingAttributes(Vector<Attribute>&) const;

    const ElementData* elementData() const { return m_elementData.get(); }
    static ptrdiff_t elementDataMemoryOffset() { return OBJECT_OFFSETOF(Element, m_elementData); }
    UniqueElementData& ensureUniqueElementData();

    void synchronizeAllAttributes() const;

    // Clones attributes only.
    void cloneAttributesFromElement(const Element&);

    // Clones all attribute-derived data, including subclass specifics (through copyNonAttributeProperties.)
    void cloneDataFromElement(const Element&);

    bool hasEquivalentAttributes(const Element* other) const;

    virtual void copyNonAttributePropertiesFromElement(const Element&) { }

    void lazyReattach();

    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>);
    virtual bool rendererIsNeeded(const RenderStyle&);
    void didAffectSelector(AffectedSelectorMask);

    ShadowRoot* shadowRoot() const;
    PassRefPtr<ShadowRoot> createShadowRoot(ExceptionCode&);

    ShadowRoot* userAgentShadowRoot() const;
    ShadowRoot& ensureUserAgentShadowRoot();

    virtual const AtomicString& shadowPseudoId() const;

    bool inActiveChain() const { return isUserActionElement() && isUserActionElementInActiveChain(); }
    bool active() const { return isUserActionElement() && isUserActionElementActive(); }
    bool hovered() const { return isUserActionElement() && isUserActionElementHovered(); }
    bool focused() const { return isUserActionElement() && isUserActionElementFocused(); }

    virtual void setActive(bool flag = true, bool pause = false);
    virtual void setHovered(bool flag = true);
    virtual void setFocus(bool flag);

    virtual bool supportsFocus() const;
    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;

    virtual bool shouldUseInputMethod();

    virtual short tabIndex() const;
    virtual Element* focusDelegate();

    virtual RenderStyle* computedStyle(PseudoId = NOPSEUDO) override;

    // Methods for indicating the style is affected by dynamic updates (e.g., children changing, our position changing in our sibling list, etc.)
    bool styleAffectedByEmpty() const { return hasRareData() && rareDataStyleAffectedByEmpty(); }
    bool childrenAffectedByHover() const { return getFlag(ChildrenAffectedByHoverRulesFlag); }
    bool childrenAffectedByActive() const { return hasRareData() && rareDataChildrenAffectedByActive(); }
    bool childrenAffectedByDrag() const { return hasRareData() && rareDataChildrenAffectedByDrag(); }
    bool childrenAffectedByPositionalRules() const { return hasRareData() && (rareDataChildrenAffectedByForwardPositionalRules() || rareDataChildrenAffectedByBackwardPositionalRules()); }
    bool childrenAffectedByFirstChildRules() const { return getFlag(ChildrenAffectedByFirstChildRulesFlag); }
    bool childrenAffectedByLastChildRules() const { return getFlag(ChildrenAffectedByLastChildRulesFlag); }
    bool childrenAffectedByDirectAdjacentRules() const { return getFlag(ChildrenAffectedByDirectAdjacentRulesFlag); }
    bool childrenAffectedByForwardPositionalRules() const { return hasRareData() && rareDataChildrenAffectedByForwardPositionalRules(); }
    bool childrenAffectedByBackwardPositionalRules() const { return hasRareData() && rareDataChildrenAffectedByBackwardPositionalRules(); }
    unsigned childIndex() const { return hasRareData() ? rareDataChildIndex() : 0; }

    bool hasFlagsSetDuringStylingOfChildren() const;

    void setStyleAffectedByEmpty();
    void setChildrenAffectedByHover() { setFlag(ChildrenAffectedByHoverRulesFlag); }
    void setChildrenAffectedByActive();
    void setChildrenAffectedByDrag();
    void setChildrenAffectedByFirstChildRules() { setFlag(ChildrenAffectedByFirstChildRulesFlag); }
    void setChildrenAffectedByLastChildRules() { setFlag(ChildrenAffectedByLastChildRulesFlag); }
    static void setChildrenAffectedByDirectAdjacentRules(Element*);
    void setChildrenAffectedByDirectAdjacentRules() { setFlag(ChildrenAffectedByDirectAdjacentRulesFlag); }
    static void setChildrenAffectedByForwardPositionalRules(Element*);
    void setChildrenAffectedByForwardPositionalRules() { setChildrenAffectedByForwardPositionalRules(this); }
    void setChildrenAffectedByBackwardPositionalRules();
    void setChildIndex(unsigned);

    void setIsInCanvasSubtree(bool);
    bool isInCanvasSubtree() const;

    void setIsInsideRegion(bool);
    bool isInsideRegion() const;

    void setRegionOversetState(RegionOversetState);
    RegionOversetState regionOversetState() const;

    AtomicString computeInheritedLanguage() const;
    Locale& locale() const;

    virtual void accessKeyAction(bool /*sendToAnyEvent*/) { }

    virtual bool isURLAttribute(const Attribute&) const { return false; }
    virtual bool isHTMLContentAttribute(const Attribute&) const { return false; }

    URL getURLAttribute(const QualifiedName&) const;
    URL getNonEmptyURLAttribute(const QualifiedName&) const;

    virtual const AtomicString& imageSourceURL() const;
    virtual String target() const { return String(); }

    void updateFocusAppearanceAfterAttachIfNeeded();
    virtual void focus(bool restorePreviousSelection = true, FocusDirection = FocusDirectionNone);
    virtual void updateFocusAppearance(bool restorePreviousSelection);
    virtual void blur();

    String innerText();
    String outerText();
 
    virtual String title() const;

    const AtomicString& pseudo() const;
    void setPseudo(const AtomicString&);

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

#if ENABLE(PAGE_VISIBILITY_API)
    // Use Document::registerForVisibilityStateChangedCallbacks() to subscribe to this.
    virtual void visibilityStateChanged() { }
#endif

#if ENABLE(VIDEO_TRACK)
    virtual void captionPreferencesChanged() { }
#endif

    bool isFinishedParsingChildren() const { return isParsingChildrenFinished(); }
    virtual void finishParsingChildren() override;
    virtual void beginParsingChildren() override final;

    PseudoElement* beforePseudoElement() const;
    PseudoElement* afterPseudoElement() const;
    bool childNeedsShadowWalker() const;
    void didShadowTreeAwareChildrenChange();

    // ElementTraversal API
    Element* firstElementChild() const;
    Element* lastElementChild() const;
    Element* previousElementSibling() const;
    Element* nextElementSibling() const;
    unsigned childElementCount() const;

    virtual bool matchesReadOnlyPseudoClass() const;
    virtual bool matchesReadWritePseudoClass() const;
    bool webkitMatchesSelector(const String& selectors, ExceptionCode&);
    virtual bool shouldAppearIndeterminate() const;

    DOMTokenList* classList();

    DatasetDOMStringMap* dataset();

#if ENABLE(VIDEO)
    virtual bool isMediaElement() const { return false; }
#endif

#if ENABLE(INPUT_SPEECH)
    virtual bool isInputFieldSpeechButtonElement() const { return false; }
#endif

    virtual bool isFormControlElement() const { return false; }
    virtual bool isSpinButtonElement() const { return false; }
    virtual bool isTextFormControl() const { return false; }
    virtual bool isOptionalFormControl() const { return false; }
    virtual bool isRequiredFormControl() const { return false; }
    virtual bool isDefaultButtonForForm() const { return false; }
    virtual bool willValidate() const { return false; }
    virtual bool isValidFormControlElement() const { return false; }
    virtual bool isInRange() const { return false; }
    virtual bool isOutOfRange() const { return false; }
    virtual bool isFrameElementBase() const { return false; }
    virtual bool isSearchFieldCancelButtonElement() const { return false; }

    virtual bool canContainRangeEndPoint() const override { return true; }

    // Used for disabled form elements; if true, prevents mouse events from being dispatched
    // to event listeners, and prevents DOMActivate events from being sent at all.
    virtual bool isDisabledFormControl() const { return false; }

    virtual bool childShouldCreateRenderer(const Node&) const override;

    bool hasPendingResources() const;
    void setHasPendingResources();
    void clearHasPendingResources();
    virtual void buildPendingResource() { };

#if ENABLE(FULLSCREEN_API)
    enum {
        ALLOW_KEYBOARD_INPUT = 1 << 0,
        LEGACY_MOZILLA_REQUEST = 1 << 1,
    };
    
    void webkitRequestFullScreen(unsigned short flags);
    bool containsFullScreenElement() const;
    void setContainsFullScreenElement(bool);
    void setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(bool);

    // W3C API
    void webkitRequestFullscreen();
#endif

#if ENABLE(POINTER_LOCK)
    void webkitRequestPointerLock();
#endif

#if ENABLE(INDIE_UI)
    void setUIActions(const AtomicString&);
    const AtomicString& UIActions() const;
#endif
    
    virtual bool isSpellCheckingEnabled() const;

    PassRef<RenderStyle> styleForRenderer();

    RenderNamedFlowFragment* renderNamedFlowFragment() const;

#if ENABLE(CSS_REGIONS)
    virtual bool shouldMoveToFlowThread(const RenderStyle&) const;
    
    const AtomicString& webkitRegionOverset() const;
    Vector<RefPtr<Range>> webkitGetRegionFlowRanges() const;
#endif

    bool hasID() const;
    bool hasClass() const;
    bool hasName() const;
    const SpaceSplitString& classNames() const;

    IntSize savedLayerScrollOffset() const;
    void setSavedLayerScrollOffset(const IntSize&);

    bool dispatchMouseEvent(const PlatformMouseEvent&, const AtomicString& eventType, int clickCount = 0, Element* relatedTarget = nullptr);
    bool dispatchWheelEvent(const PlatformWheelEvent&);
    bool dispatchKeyEvent(const PlatformKeyboardEvent&);
    void dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions = SendNoEvents, SimulatedClickVisualOptions = ShowPressedLook);
    void dispatchFocusInEvent(const AtomicString& eventType, PassRefPtr<Element> oldFocusedElement);
    void dispatchFocusOutEvent(const AtomicString& eventType, PassRefPtr<Element> newFocusedElement);
    virtual void dispatchFocusEvent(PassRefPtr<Element> oldFocusedElement, FocusDirection);
    virtual void dispatchBlurEvent(PassRefPtr<Element> newFocusedElement);

    virtual bool willRecalcStyle(Style::Change);
    virtual void didRecalcStyle(Style::Change);
    virtual void willAttachRenderers();
    virtual void didAttachRenderers();
    virtual void willDetachRenderers();
    virtual void didDetachRenderers();

    void setBeforePseudoElement(PassRefPtr<PseudoElement>);
    void setAfterPseudoElement(PassRefPtr<PseudoElement>);
    void clearBeforePseudoElement();
    void clearAfterPseudoElement();
    void resetComputedStyle();
    void clearStyleDerivedDataBeforeDetachingRenderer();
    void clearHoverAndActiveStatusBeforeDetachingRenderer();

protected:
    Element(const QualifiedName& tagName, Document& document, ConstructionType type)
        : ContainerNode(document, type)
        , m_tagName(tagName)
    {
    }

    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;
    virtual void childrenChanged(const ChildChange&) override;
    virtual void removeAllEventListeners() override final;

    virtual PassRefPtr<RenderStyle> customStyleForRenderer();

    void clearTabIndexExplicitlyIfNeeded();    
    void setTabIndexExplicitly(short);

    PassRefPtr<HTMLCollection> ensureCachedHTMLCollection(CollectionType);
    HTMLCollection* cachedHTMLCollection(CollectionType);

    // classAttributeChanged() exists to share code between
    // parseAttribute (called via setAttribute()) and
    // svgAttributeChanged (called when element.className.baseValue is set)
    void classAttributeChanged(const AtomicString& newClassString);

private:
    bool isTextNode() const;

    bool isUserActionElementInActiveChain() const;
    bool isUserActionElementActive() const;
    bool isUserActionElementFocused() const;
    bool isUserActionElementHovered() const;

    void resetNeedsNodeRenderingTraversalSlowPath();

    virtual void didAddUserAgentShadowRoot(ShadowRoot*) { }
    virtual bool alwaysCreateUserAgentShadowRoot() const { return false; }

    // FIXME: Remove the need for Attr to call willModifyAttribute/didModifyAttribute.
    friend class Attr;

    enum SynchronizationOfLazyAttribute { NotInSynchronizationOfLazyAttribute = 0, InSynchronizationOfLazyAttribute };

    void didAddAttribute(const QualifiedName&, const AtomicString&);
    void willModifyAttribute(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue);
    void didModifyAttribute(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue);
    void didRemoveAttribute(const QualifiedName&, const AtomicString& oldValue);

    void synchronizeAttribute(const QualifiedName&) const;
    void synchronizeAttribute(const AtomicString& localName) const;

    void updateName(const AtomicString& oldName, const AtomicString& newName);
    void updateNameForTreeScope(TreeScope&, const AtomicString& oldName, const AtomicString& newName);
    void updateNameForDocument(HTMLDocument&, const AtomicString& oldName, const AtomicString& newName);
    void updateId(const AtomicString& oldId, const AtomicString& newId);
    void updateIdForTreeScope(TreeScope&, const AtomicString& oldId, const AtomicString& newId);
    enum HTMLDocumentNamedItemMapsUpdatingCondition { AlwaysUpdateHTMLDocumentNamedItemMaps, UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute };
    void updateIdForDocument(HTMLDocument&, const AtomicString& oldId, const AtomicString& newId, HTMLDocumentNamedItemMapsUpdatingCondition);
    void updateLabel(TreeScope&, const AtomicString& oldForAttributeValue, const AtomicString& newForAttributeValue);

    void scrollByUnits(int units, ScrollGranularity);

    virtual void setPrefix(const AtomicString&, ExceptionCode&) override final;
    virtual NodeType nodeType() const override final;
    virtual bool childTypeAllowed(NodeType) const override final;

    void setAttributeInternal(unsigned index, const QualifiedName&, const AtomicString& value, SynchronizationOfLazyAttribute);
    void addAttributeInternal(const QualifiedName&, const AtomicString& value, SynchronizationOfLazyAttribute);
    void removeAttributeInternal(unsigned index, SynchronizationOfLazyAttribute);

#ifndef NDEBUG
    virtual void formatForDebugger(char* buffer, unsigned length) const override;
#endif

    void cancelFocusAppearanceUpdate();

    // cloneNode is private so that non-virtual cloneElementWithChildren and cloneElementWithoutChildren
    // are used instead.
    virtual PassRefPtr<Node> cloneNode(bool deep) override;
    virtual PassRefPtr<Element> cloneElementWithoutAttributesAndChildren();

    void addShadowRoot(PassRefPtr<ShadowRoot>);
    void removeShadowRoot();

    bool rareDataStyleAffectedByEmpty() const;
    bool rareDataChildrenAffectedByHover() const;
    bool rareDataChildrenAffectedByActive() const;
    bool rareDataChildrenAffectedByDrag() const;
    bool rareDataChildrenAffectedByLastChildRules() const;
    bool rareDataChildrenAffectedByForwardPositionalRules() const;
    bool rareDataChildrenAffectedByBackwardPositionalRules() const;
    unsigned rareDataChildIndex() const;

    SpellcheckAttributeState spellcheckAttributeState() const;

    void unregisterNamedFlowContentElement();

    void createUniqueElementData();

    ElementRareData* elementRareData() const;
    ElementRareData& ensureElementRareData();

    void detachAllAttrNodesFromElement();
    void detachAttrNodeFromElementWithValue(Attr*, const AtomicString& value);

    bool isJavaScriptURLAttribute(const Attribute&) const;

    // Anyone thinking of using this should call document instead of ownerDocument.
    void ownerDocument() const = delete;

    QualifiedName m_tagName;
    RefPtr<ElementData> m_elementData;
};

inline bool isElement(const Node& node) { return node.isElementNode(); }

NODE_TYPE_CASTS(Element)

template <typename Type> bool isElementOfType(const Element&);
template <typename Type> inline bool isElementOfType(const Node& node) { return node.isElementNode() && isElementOfType<const Type>(toElement(node)); }
template <> inline bool isElementOfType<const Element>(const Element&) { return true; }

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
    return isElementNode() ? toElement(this)->attributes() : nullptr;
}

inline Element* Node::parentElement() const
{
    ContainerNode* parent = parentNode();
    return parent && parent->isElementNode() ? toElement(parent) : nullptr;
}

inline bool Element::fastHasAttribute(const QualifiedName& name) const
{
    ASSERT(fastAttributeLookupAllowed(name));
    return elementData() && findAttributeByName(name);
}

inline const AtomicString& Element::fastGetAttribute(const QualifiedName& name) const
{
    ASSERT(fastAttributeLookupAllowed(name));
    if (elementData()) {
        if (const Attribute* attribute = findAttributeByName(name))
            return attribute->value();
    }
    return nullAtom;
}

inline bool Element::hasAttributesWithoutUpdate() const
{
    return elementData() && !elementData()->isEmpty();
}

inline const AtomicString& Element::idForStyleResolution() const
{
    ASSERT(hasID());
    return elementData()->idForStyleResolution();
}

inline const AtomicString& Element::getIdAttribute() const
{
    if (hasID())
        return elementData()->findAttributeByName(HTMLNames::idAttr)->value();
    return nullAtom;
}

inline const AtomicString& Element::getNameAttribute() const
{
    if (hasName())
        return elementData()->findAttributeByName(HTMLNames::nameAttr)->value();
    return nullAtom;
}

inline void Element::setIdAttribute(const AtomicString& value)
{
    setAttribute(HTMLNames::idAttr, value);
}

inline const SpaceSplitString& Element::classNames() const
{
    ASSERT(hasClass());
    ASSERT(elementData());
    return elementData()->classNames();
}

inline unsigned Element::attributeCount() const
{
    ASSERT(elementData());
    return elementData()->length();
}

inline const Attribute& Element::attributeAt(unsigned index) const
{
    ASSERT(elementData());
    return elementData()->attributeAt(index);
}

inline const Attribute* Element::findAttributeByName(const QualifiedName& name) const
{
    ASSERT(elementData());
    return elementData()->findAttributeByName(name);
}

inline bool Element::hasID() const
{
    return elementData() && elementData()->hasID();
}

inline bool Element::hasClass() const
{
    return elementData() && elementData()->hasClass();
}

inline bool Element::hasName() const
{
    return elementData() && elementData()->hasName();
}

inline UniqueElementData& Element::ensureUniqueElementData()
{
    if (!elementData() || !elementData()->isUnique())
        createUniqueElementData();
    return static_cast<UniqueElementData&>(*m_elementData);
}

class PostAttachCallbackDisabler {
public:
    explicit PostAttachCallbackDisabler(Document& document)
        : m_document(document)
    {
        Element::suspendPostAttachCallbacks(m_document);
    }

    ~PostAttachCallbackDisabler()
    {
        Element::resumePostAttachCallbacks(m_document);
    }

private:
    Document& m_document;
};

} // namespace WebCore

#endif
