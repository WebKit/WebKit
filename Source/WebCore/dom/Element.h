/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "AXTextStateChangeIntent.h"
#include "Document.h"
#include "ElementData.h"
#include "HTMLNames.h"
#include "KeyframeAnimationOptions.h"
#include "ScrollToOptions.h"
#include "ScrollTypes.h"
#include "ShadowRootMode.h"
#include "SimulatedClickOptions.h"
#include "StyleChange.h"

namespace WebCore {

class CustomElementReactionQueue;
class DatasetDOMStringMap;
class DOMRect;
class DOMRectList;
class DOMTokenList;
class ElementRareData;
class Frame;
class HTMLDocument;
class IntSize;
class JSCustomElementInterface;
class KeyboardEvent;
class Locale;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformWheelEvent;
class PseudoElement;
class RenderTreePosition;
class StylePropertyMap;
class WebAnimation;
struct ElementStyle;
struct ScrollIntoViewOptions;

#if ENABLE(INTERSECTION_OBSERVER)
struct IntersectionObserverData;
#endif

#if ENABLE(RESIZE_OBSERVER)
struct ResizeObserverData;
#endif

enum SpellcheckAttributeState {
    SpellcheckAttributeTrue,
    SpellcheckAttributeFalse,
    SpellcheckAttributeDefault
};

class Element : public ContainerNode {
    WTF_MAKE_ISO_ALLOCATED(Element);
public:
    static Ref<Element> create(const QualifiedName&, Document&);
    virtual ~Element();

    WEBCORE_EXPORT bool hasAttribute(const QualifiedName&) const;
    WEBCORE_EXPORT const AtomString& getAttribute(const QualifiedName&) const;
    template<typename... QualifiedNames>
    const AtomString& getAttribute(const QualifiedName&, const QualifiedNames&...) const;
    WEBCORE_EXPORT void setAttribute(const QualifiedName&, const AtomString& value);
    WEBCORE_EXPORT void setAttributeWithoutSynchronization(const QualifiedName&, const AtomString& value);
    void setSynchronizedLazyAttribute(const QualifiedName&, const AtomString& value);
    bool removeAttribute(const QualifiedName&);
    Vector<String> getAttributeNames() const;

    // Typed getters and setters for language bindings.
    WEBCORE_EXPORT int getIntegralAttribute(const QualifiedName& attributeName) const;
    WEBCORE_EXPORT void setIntegralAttribute(const QualifiedName& attributeName, int value);
    WEBCORE_EXPORT unsigned getUnsignedIntegralAttribute(const QualifiedName& attributeName) const;
    WEBCORE_EXPORT void setUnsignedIntegralAttribute(const QualifiedName& attributeName, unsigned value);

    // Call this to get the value of an attribute that is known not to be the style
    // attribute or one of the SVG animatable attributes.
    bool hasAttributeWithoutSynchronization(const QualifiedName&) const;
    const AtomString& attributeWithoutSynchronization(const QualifiedName&) const;
#ifndef NDEBUG
    WEBCORE_EXPORT bool fastAttributeLookupAllowed(const QualifiedName&) const;
#endif

#if DUMP_NODE_STATISTICS
    bool hasNamedNodeMap() const;
#endif
    WEBCORE_EXPORT bool hasAttributes() const;
    // This variant will not update the potentially invalid attributes. To be used when not interested
    // in style attribute or one of the SVG animation attributes.
    bool hasAttributesWithoutUpdate() const;

    WEBCORE_EXPORT bool hasAttribute(const AtomString& qualifiedName) const;
    WEBCORE_EXPORT bool hasAttributeNS(const AtomString& namespaceURI, const AtomString& localName) const;

    WEBCORE_EXPORT const AtomString& getAttribute(const AtomString& qualifiedName) const;
    WEBCORE_EXPORT const AtomString& getAttributeNS(const AtomString& namespaceURI, const AtomString& localName) const;

    WEBCORE_EXPORT ExceptionOr<void> setAttribute(const AtomString& qualifiedName, const AtomString& value);
    static ExceptionOr<QualifiedName> parseAttributeName(const AtomString& namespaceURI, const AtomString& qualifiedName);
    WEBCORE_EXPORT ExceptionOr<void> setAttributeNS(const AtomString& namespaceURI, const AtomString& qualifiedName, const AtomString& value);

    ExceptionOr<bool> toggleAttribute(const AtomString& qualifiedName, Optional<bool> force);

    const AtomString& getIdAttribute() const;
    void setIdAttribute(const AtomString&);

    const AtomString& getNameAttribute() const;

    // Call this to get the value of the id attribute for style resolution purposes.
    // The value will already be lowercased if the document is in compatibility mode,
    // so this function is not suitable for non-style uses.
    const AtomString& idForStyleResolution() const;

    // Internal methods that assume the existence of attribute storage, one should use hasAttributes()
    // before calling them.
    AttributeIteratorAccessor attributesIterator() const { return elementData()->attributesIterator(); }
    unsigned attributeCount() const;
    const Attribute& attributeAt(unsigned index) const;
    const Attribute* findAttributeByName(const QualifiedName&) const;
    unsigned findAttributeIndexByName(const QualifiedName& name) const { return elementData()->findAttributeIndexByName(name); }
    unsigned findAttributeIndexByName(const AtomString& name, bool shouldIgnoreAttributeCase) const { return elementData()->findAttributeIndexByName(name, shouldIgnoreAttributeCase); }

    WEBCORE_EXPORT void scrollIntoView(Optional<Variant<bool, ScrollIntoViewOptions>>&& arg);
    WEBCORE_EXPORT void scrollIntoView(bool alignToTop = true);
    WEBCORE_EXPORT void scrollIntoViewIfNeeded(bool centerIfNeeded = true);
    WEBCORE_EXPORT void scrollIntoViewIfNotVisible(bool centerIfNotVisible = true);

    void scrollBy(const ScrollToOptions&);
    void scrollBy(double x, double y);
    virtual void scrollTo(const ScrollToOptions&, ScrollClamping = ScrollClamping::Clamped);
    void scrollTo(double x, double y);

    WEBCORE_EXPORT void scrollByLines(int lines);
    WEBCORE_EXPORT void scrollByPages(int pages);

    WEBCORE_EXPORT double offsetLeftForBindings();
    WEBCORE_EXPORT double offsetLeft();
    WEBCORE_EXPORT double offsetTopForBindings();
    WEBCORE_EXPORT double offsetTop();
    WEBCORE_EXPORT double offsetWidth();
    WEBCORE_EXPORT double offsetHeight();

    bool mayCauseRepaintInsideViewport(const IntRect* visibleRect = nullptr) const;

    // FIXME: Replace uses of offsetParent in the platform with calls
    // to the render layer and merge bindingsOffsetParent and offsetParent.
    WEBCORE_EXPORT Element* offsetParentForBindings();

    const Element* rootElement() const;

    Element* offsetParent();
    WEBCORE_EXPORT double clientLeft();
    WEBCORE_EXPORT double clientTop();
    WEBCORE_EXPORT double clientWidth();
    WEBCORE_EXPORT double clientHeight();

    virtual int scrollLeft();
    virtual int scrollTop();
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);
    virtual int scrollWidth();
    virtual int scrollHeight();

    WEBCORE_EXPORT IntRect boundsInRootViewSpace();

    Optional<std::pair<RenderObject*, FloatRect>> boundingAbsoluteRectWithoutLayout();

    WEBCORE_EXPORT FloatRect boundingClientRect();

    WEBCORE_EXPORT Ref<DOMRectList> getClientRects();
    Ref<DOMRect> getBoundingClientRect();

    // Returns the absolute bounding box translated into client coordinates.
    WEBCORE_EXPORT IntRect clientRect() const;
    // Returns the absolute bounding box translated into screen coordinates.
    WEBCORE_EXPORT IntRect screenRect() const;

    WEBCORE_EXPORT bool removeAttribute(const AtomString& qualifiedName);
    WEBCORE_EXPORT bool removeAttributeNS(const AtomString& namespaceURI, const AtomString& localName);

    Ref<Attr> detachAttribute(unsigned index);

    WEBCORE_EXPORT RefPtr<Attr> getAttributeNode(const AtomString& qualifiedName);
    WEBCORE_EXPORT RefPtr<Attr> getAttributeNodeNS(const AtomString& namespaceURI, const AtomString& localName);
    WEBCORE_EXPORT ExceptionOr<RefPtr<Attr>> setAttributeNode(Attr&);
    WEBCORE_EXPORT ExceptionOr<RefPtr<Attr>> setAttributeNodeNS(Attr&);
    WEBCORE_EXPORT ExceptionOr<Ref<Attr>> removeAttributeNode(Attr&);

    RefPtr<Attr> attrIfExists(const QualifiedName&);
    RefPtr<Attr> attrIfExists(const AtomString& localName, bool shouldIgnoreAttributeCase);
    Ref<Attr> ensureAttr(const QualifiedName&);

    const Vector<RefPtr<Attr>>& attrNodeList();

    const QualifiedName& tagQName() const { return m_tagName; }
#if ENABLE(JIT)
    static ptrdiff_t tagQNameMemoryOffset() { return OBJECT_OFFSETOF(Element, m_tagName); }
#endif // ENABLE(JIT)
    String tagName() const { return nodeName(); }
    bool hasTagName(const QualifiedName& tagName) const { return m_tagName.matches(tagName); }
    bool hasTagName(const HTMLQualifiedName& tagName) const { return ContainerNode::hasTagName(tagName); }
    bool hasTagName(const MathMLQualifiedName& tagName) const { return ContainerNode::hasTagName(tagName); }
    bool hasTagName(const SVGQualifiedName& tagName) const { return ContainerNode::hasTagName(tagName); }

    // A fast function for checking the local name against another atomic string.
    bool hasLocalName(const AtomString& other) const { return m_tagName.localName() == other; }

    const AtomString& localName() const final { return m_tagName.localName(); }
    const AtomString& prefix() const final { return m_tagName.prefix(); }
    const AtomString& namespaceURI() const final { return m_tagName.namespaceURI(); }

    ExceptionOr<void> setPrefix(const AtomString&) final;

    String nodeName() const override;

    Ref<Element> cloneElementWithChildren(Document&);
    Ref<Element> cloneElementWithoutChildren(Document&);

    void normalizeAttributes();
    String nodeNamePreservingCase() const;

    WEBCORE_EXPORT void setBooleanAttribute(const QualifiedName& name, bool);

    // For exposing to DOM only.
    WEBCORE_EXPORT NamedNodeMap& attributes() const;

    enum AttributeModificationReason {
        ModifiedDirectly,
        ModifiedByCloning
    };

    // This method is called whenever an attribute is added, changed or removed.
    virtual void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason = ModifiedDirectly);
    virtual void parseAttribute(const QualifiedName&, const AtomString&) { }

    // Only called by the parser immediately after element construction.
    void parserSetAttributes(const Vector<Attribute>&);

    bool isEventHandlerAttribute(const Attribute&) const;
    bool isJavaScriptURLAttribute(const Attribute&) const;

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

    virtual void didMoveToNewDocument(Document& oldDocument, Document& newDocument);

    bool hasEquivalentAttributes(const Element& other) const;

    virtual void copyNonAttributePropertiesFromElement(const Element&) { }

    virtual RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&);
    virtual bool rendererIsNeeded(const RenderStyle&);

    WEBCORE_EXPORT ShadowRoot* shadowRoot() const;
    ShadowRoot* shadowRootForBindings(JSC::ExecState&) const;

    struct ShadowRootInit {
        ShadowRootMode mode;
    };
    ExceptionOr<ShadowRoot&> attachShadow(const ShadowRootInit&);

    RefPtr<ShadowRoot> userAgentShadowRoot() const;
    WEBCORE_EXPORT ShadowRoot& ensureUserAgentShadowRoot();

    void setIsDefinedCustomElement(JSCustomElementInterface&);
    void setIsFailedCustomElement(JSCustomElementInterface&);
    void setIsCustomElementUpgradeCandidate();
    void enqueueToUpgrade(JSCustomElementInterface&);
    CustomElementReactionQueue* reactionQueue() const;

    // FIXME: this should not be virtual, do not override this.
    virtual const AtomString& shadowPseudoId() const;

    bool isInActiveChain() const { return isUserActionElement() && isUserActionElementInActiveChain(); }
    bool active() const { return isUserActionElement() && isUserActionElementActive(); }
    bool hovered() const { return isUserActionElement() && isUserActionElementHovered(); }
    bool focused() const { return isUserActionElement() && isUserActionElementFocused(); }
    bool hasFocusWithin() const { return getFlag(HasFocusWithin); };

    virtual void setActive(bool flag = true, bool pause = false);
    virtual void setHovered(bool flag = true);
    virtual void setFocus(bool flag);
    void setHasFocusWithin(bool flag);

    Optional<int> tabIndexSetExplicitly() const;
    bool shouldBeIgnoredInSequentialFocusNavigation() const { return defaultTabIndex() < 0 && !supportsFocus(); }
    virtual bool supportsFocus() const;
    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;

    virtual bool shouldUseInputMethod();

    virtual int tabIndexForBindings() const;
    WEBCORE_EXPORT void setTabIndexForBindings(int);
    virtual RefPtr<Element> focusDelegate();

    ExceptionOr<void> insertAdjacentHTML(const String& where, const String& html, NodeVector* addedNodes);

    WEBCORE_EXPORT ExceptionOr<Element*> insertAdjacentElement(const String& where, Element& newChild);
    WEBCORE_EXPORT ExceptionOr<void> insertAdjacentHTML(const String& where, const String& html);
    WEBCORE_EXPORT ExceptionOr<void> insertAdjacentText(const String& where, const String& text);

    const RenderStyle* computedStyle(PseudoId = PseudoId::None) override;

    bool needsStyleInvalidation() const;

    // Methods for indicating the style is affected by dynamic updates (e.g., children changing, our position changing in our sibling list, etc.)
    bool styleAffectedByActive() const { return hasStyleFlag(ElementStyleFlag::StyleAffectedByActive); }
    bool styleAffectedByEmpty() const { return hasStyleFlag(ElementStyleFlag::StyleAffectedByEmpty); }
    bool styleAffectedByFocusWithin() const { return getFlag(StyleAffectedByFocusWithinFlag); }
    bool descendantsAffectedByPreviousSibling() const { return getFlag(DescendantsAffectedByPreviousSiblingFlag); }
    bool childrenAffectedByHover() const { return getFlag(ChildrenAffectedByHoverRulesFlag); }
    bool childrenAffectedByDrag() const { return hasStyleFlag(ElementStyleFlag::ChildrenAffectedByDrag); }
    bool childrenAffectedByFirstChildRules() const { return getFlag(ChildrenAffectedByFirstChildRulesFlag); }
    bool childrenAffectedByLastChildRules() const { return getFlag(ChildrenAffectedByLastChildRulesFlag); }
    bool childrenAffectedByForwardPositionalRules() const { return hasStyleFlag(ElementStyleFlag::ChildrenAffectedByForwardPositionalRules); }
    bool descendantsAffectedByForwardPositionalRules() const { return hasStyleFlag(ElementStyleFlag::DescendantsAffectedByForwardPositionalRules); }
    bool childrenAffectedByBackwardPositionalRules() const { return hasStyleFlag(ElementStyleFlag::ChildrenAffectedByBackwardPositionalRules); }
    bool descendantsAffectedByBackwardPositionalRules() const { return hasStyleFlag(ElementStyleFlag::DescendantsAffectedByBackwardPositionalRules); }
    bool childrenAffectedByPropertyBasedBackwardPositionalRules() const { return hasStyleFlag(ElementStyleFlag::ChildrenAffectedByPropertyBasedBackwardPositionalRules); }
    bool affectsNextSiblingElementStyle() const { return getFlag(AffectsNextSiblingElementStyle); }
    unsigned childIndex() const { return hasRareData() ? rareDataChildIndex() : 0; }

    bool hasFlagsSetDuringStylingOfChildren() const;

    void setStyleAffectedByEmpty() { setStyleFlag(ElementStyleFlag::StyleAffectedByEmpty); }
    void setStyleAffectedByFocusWithin() { setFlag(StyleAffectedByFocusWithinFlag); }
    void setDescendantsAffectedByPreviousSibling() { setFlag(DescendantsAffectedByPreviousSiblingFlag); }
    void setChildrenAffectedByHover() { setFlag(ChildrenAffectedByHoverRulesFlag); }
    void setStyleAffectedByActive() { setStyleFlag(ElementStyleFlag::StyleAffectedByActive); }
    void setChildrenAffectedByDrag() { setStyleFlag(ElementStyleFlag::ChildrenAffectedByDrag); }
    void setChildrenAffectedByFirstChildRules() { setFlag(ChildrenAffectedByFirstChildRulesFlag); }
    void setChildrenAffectedByLastChildRules() { setFlag(ChildrenAffectedByLastChildRulesFlag); }
    void setChildrenAffectedByForwardPositionalRules() { setStyleFlag(ElementStyleFlag::ChildrenAffectedByForwardPositionalRules); }
    void setDescendantsAffectedByForwardPositionalRules() { setStyleFlag(ElementStyleFlag::DescendantsAffectedByForwardPositionalRules); }
    void setChildrenAffectedByBackwardPositionalRules() { setStyleFlag(ElementStyleFlag::ChildrenAffectedByBackwardPositionalRules); }
    void setDescendantsAffectedByBackwardPositionalRules() { setStyleFlag(ElementStyleFlag::DescendantsAffectedByBackwardPositionalRules); }
    void setChildrenAffectedByPropertyBasedBackwardPositionalRules() { setStyleFlag(ElementStyleFlag::ChildrenAffectedByPropertyBasedBackwardPositionalRules); }
    void setAffectsNextSiblingElementStyle() { setFlag(AffectsNextSiblingElementStyle); }
    void setStyleIsAffectedByPreviousSibling() { setFlag(StyleIsAffectedByPreviousSibling); }
    void setChildIndex(unsigned);

    WEBCORE_EXPORT AtomString computeInheritedLanguage() const;
    Locale& locale() const;

    virtual void accessKeyAction(bool /*sendToAnyEvent*/) { }

    virtual bool isURLAttribute(const Attribute&) const { return false; }
    virtual bool attributeContainsURL(const Attribute& attribute) const { return isURLAttribute(attribute); }
    virtual String completeURLsInAttributeValue(const URL& base, const Attribute&) const;
    virtual bool isHTMLContentAttribute(const Attribute&) const { return false; }

    WEBCORE_EXPORT URL getURLAttribute(const QualifiedName&) const;
    URL getNonEmptyURLAttribute(const QualifiedName&) const;

    virtual const AtomString& imageSourceURL() const;
    virtual String target() const { return String(); }

    static AXTextStateChangeIntent defaultFocusTextStateChangeIntent() { return AXTextStateChangeIntent(AXTextStateChangeTypeSelectionMove, AXTextSelection { AXTextSelectionDirectionDiscontiguous, AXTextSelectionGranularityUnknown, true }); }
    virtual void focus(bool restorePreviousSelection = true, FocusDirection = FocusDirectionNone);
    virtual RefPtr<Element> focusAppearanceUpdateTarget();
    virtual void updateFocusAppearance(SelectionRestorationMode, SelectionRevealMode = SelectionRevealMode::Reveal);
    virtual void blur();

    WEBCORE_EXPORT String innerHTML() const;
    WEBCORE_EXPORT String outerHTML() const;
    WEBCORE_EXPORT ExceptionOr<void> setInnerHTML(const String&);
    WEBCORE_EXPORT ExceptionOr<void> setOuterHTML(const String&);
    WEBCORE_EXPORT String innerText();
    WEBCORE_EXPORT String outerText();
 
    virtual String title() const;

    const AtomString& pseudo() const;
    WEBCORE_EXPORT void setPseudo(const AtomString&);

    LayoutSize minimumSizeForResizing() const;
    void setMinimumSizeForResizing(const LayoutSize&);

    // Use Document::registerForDocumentActivationCallbacks() to subscribe to these
    virtual void prepareForDocumentSuspension() { }
    virtual void resumeFromDocumentSuspension() { }

    // Use Document::registerForMediaVolumeCallbacks() to subscribe to this
    virtual void mediaVolumeDidChange() { }

    // Use Document::registerForPrivateBrowsingStateChangedCallbacks() to subscribe to this.
    virtual void privateBrowsingStateDidChange(PAL::SessionID) { }

    virtual void willBecomeFullscreenElement();
    virtual void ancestorWillEnterFullscreen() { }
    virtual void didBecomeFullscreenElement() { }
    virtual void willStopBeingFullscreenElement() { }

#if ENABLE(VIDEO_TRACK)
    virtual void captionPreferencesChanged() { }
#endif

    bool isFinishedParsingChildren() const { return isParsingChildrenFinished(); }
    void finishParsingChildren() override;
    void beginParsingChildren() final;

    WEBCORE_EXPORT PseudoElement* beforePseudoElement() const;
    WEBCORE_EXPORT PseudoElement* afterPseudoElement() const;
    bool childNeedsShadowWalker() const;
    void didShadowTreeAwareChildrenChange();

    virtual bool matchesValidPseudoClass() const;
    virtual bool matchesInvalidPseudoClass() const;
    virtual bool matchesReadWritePseudoClass() const;
    virtual bool matchesIndeterminatePseudoClass() const;
    virtual bool matchesDefaultPseudoClass() const;
    WEBCORE_EXPORT ExceptionOr<bool> matches(const String& selectors);
    WEBCORE_EXPORT ExceptionOr<Element*> closest(const String& selectors);
    virtual bool shouldAppearIndeterminate() const;

    WEBCORE_EXPORT DOMTokenList& classList();

    DatasetDOMStringMap& dataset();

#if ENABLE(VIDEO)
    virtual bool isMediaElement() const { return false; }
#endif

    virtual bool isFormControlElement() const { return false; }
    virtual bool isSpinButtonElement() const { return false; }
    virtual bool isTextFormControlElement() const { return false; }
    virtual bool isTextField() const { return false; }
    virtual bool isOptionalFormControl() const { return false; }
    virtual bool isRequiredFormControl() const { return false; }
    virtual bool isInRange() const { return false; }
    virtual bool isOutOfRange() const { return false; }
    virtual bool isUploadButton() const { return false; }
    virtual bool isSliderContainerElement() const { return false; }

    bool canContainRangeEndPoint() const override;

    // Used for disabled form elements; if true, prevents mouse events from being dispatched
    // to event listeners, and prevents DOMActivate events from being sent at all.
    virtual bool isDisabledFormControl() const { return false; }

    virtual bool childShouldCreateRenderer(const Node&) const;

    bool hasPendingResources() const;
    void setHasPendingResources();
    void clearHasPendingResources();
    virtual void buildPendingResource() { };

    bool hasCSSAnimation() const;
    void setHasCSSAnimation();
    void clearHasCSSAnimation();

#if ENABLE(FULLSCREEN_API)
    WEBCORE_EXPORT bool containsFullScreenElement() const;
    void setContainsFullScreenElement(bool);
    void setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(bool);
    WEBCORE_EXPORT virtual void webkitRequestFullscreen();
#endif

#if ENABLE(POINTER_EVENTS)
    ExceptionOr<void> setPointerCapture(int32_t);
    ExceptionOr<void> releasePointerCapture(int32_t);
    bool hasPointerCapture(int32_t);
#endif

#if ENABLE(POINTER_LOCK)
    WEBCORE_EXPORT void requestPointerLock();
#endif

    bool isSpellCheckingEnabled() const;

    bool hasID() const;
    bool hasClass() const;
    bool hasName() const;
    const SpaceSplitString& classNames() const;

    IntPoint savedLayerScrollPosition() const;
    void setSavedLayerScrollPosition(const IntPoint&);

    bool dispatchMouseEvent(const PlatformMouseEvent&, const AtomString& eventType, int clickCount = 0, Element* relatedTarget = nullptr);
    bool dispatchWheelEvent(const PlatformWheelEvent&);
    bool dispatchKeyEvent(const PlatformKeyboardEvent&);
    void dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions = SendNoEvents, SimulatedClickVisualOptions = ShowPressedLook);
    void dispatchFocusInEvent(const AtomString& eventType, RefPtr<Element>&& oldFocusedElement);
    void dispatchFocusOutEvent(const AtomString& eventType, RefPtr<Element>&& newFocusedElement);
    virtual void dispatchFocusEvent(RefPtr<Element>&& oldFocusedElement, FocusDirection);
    virtual void dispatchBlurEvent(RefPtr<Element>&& newFocusedElement);
    void dispatchWebKitImageReadyEventForTesting();

    WEBCORE_EXPORT bool dispatchMouseForceWillBegin();

    virtual void willRecalcStyle(Style::Change);
    virtual void didRecalcStyle(Style::Change);
    virtual void willResetComputedStyle();
    virtual void willAttachRenderers();
    virtual void didAttachRenderers();
    virtual void willDetachRenderers();
    virtual void didDetachRenderers();
    virtual Optional<ElementStyle> resolveCustomStyle(const RenderStyle& parentStyle, const RenderStyle* shadowHostStyle);

    LayoutRect absoluteEventHandlerBounds(bool& includesFixedPositionElements) override;

    const RenderStyle* existingComputedStyle() const;
    WEBCORE_EXPORT const RenderStyle* renderOrDisplayContentsStyle() const;

    void setBeforePseudoElement(Ref<PseudoElement>&&);
    void setAfterPseudoElement(Ref<PseudoElement>&&);
    void clearBeforePseudoElement();
    void clearAfterPseudoElement();
    void resetComputedStyle();
    void resetStyleRelations();
    void clearHoverAndActiveStatusBeforeDetachingRenderer();

    WEBCORE_EXPORT URL absoluteLinkURL() const;

#if ENABLE(TOUCH_EVENTS)
    bool allowsDoubleTapGesture() const override;
#endif

    StyleResolver& styleResolver();
    ElementStyle resolveStyle(const RenderStyle* parentStyle);

    // Invalidates the style of a single element. Style is resolved lazily.
    // Descendant elements are resolved as needed, for example if an inherited property changes.
    // This should be called whenever an element changes in a manner that can affect its style.
    void invalidateStyle();

    // As above but also call RenderElement::setStyle with StyleDifference::RecompositeLayer flag for
    // the element even when the style doesn't change. This is mostly needed by the animation code.
    WEBCORE_EXPORT void invalidateStyleAndLayerComposition();

    // Invalidate the element and all its descendants. This is used when there is some sort of change
    // in the tree that may affect the style of any of the descendants and we don't know how to optimize
    // the case to limit the scope. This is expensive and should be avoided.
    void invalidateStyleForSubtree();

    // Invalidates renderers for the element and all its descendants causing them to be torn down
    // and rebuild during style resolution. Style is also recomputed. This is used in code dealing with
    // custom (not style based) renderers. This is expensive and should be avoided.
    // Elements newly added to the tree are also in this state.
    void invalidateStyleAndRenderersForSubtree();

    void invalidateStyleInternal();
    void invalidateStyleForSubtreeInternal();

    bool hasDisplayContents() const;
    void storeDisplayContentsStyle(std::unique_ptr<RenderStyle>);

    using ContainerNode::setAttributeEventListener;
    void setAttributeEventListener(const AtomString& eventType, const QualifiedName& attributeName, const AtomString& value);

#if ENABLE(INTERSECTION_OBSERVER)
    IntersectionObserverData& ensureIntersectionObserverData();
    IntersectionObserverData* intersectionObserverData();
#endif

#if ENABLE(RESIZE_OBSERVER)
    ResizeObserverData& ensureResizeObserverData();
    ResizeObserverData* resizeObserverData();
#endif

    Element* findAnchorElementForLink(String& outAnchorName);

    ExceptionOr<Ref<WebAnimation>> animate(JSC::ExecState&, JSC::Strong<JSC::JSObject>&&, Optional<Variant<double, KeyframeAnimationOptions>>&&);
    Vector<RefPtr<WebAnimation>> getAnimations();

    ElementIdentifier createElementIdentifier();

protected:
    Element(const QualifiedName&, Document&, ConstructionType);

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;
    void childrenChanged(const ChildChange&) override;
    void removeAllEventListeners() final;
    virtual void parserDidSetAttributes();

    void clearTabIndexExplicitlyIfNeeded();
    void setTabIndexExplicitly(int);

    // classAttributeChanged() exists to share code between
    // parseAttribute (called via setAttribute()) and
    // svgAttributeChanged (called when element.className.baseValue is set)
    void classAttributeChanged(const AtomString& newClassString);

    void addShadowRoot(Ref<ShadowRoot>&&);

    static ExceptionOr<void> mergeWithNextTextNode(Text&);

#if ENABLE(CSS_TYPED_OM)
    StylePropertyMap* attributeStyleMap();
    void setAttributeStyleMap(Ref<StylePropertyMap>&&);
#endif

private:
    Frame* documentFrameWithNonNullView() const;

    bool isTextNode() const;

    bool isUserActionElementInActiveChain() const;
    bool isUserActionElementActive() const;
    bool isUserActionElementFocused() const;
    bool isUserActionElementHovered() const;

    virtual void didAddUserAgentShadowRoot(ShadowRoot&) { }

    void didAddAttribute(const QualifiedName&, const AtomString&);
    void willModifyAttribute(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue);
    void didModifyAttribute(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue);
    void didRemoveAttribute(const QualifiedName&, const AtomString& oldValue);

    void synchronizeAttribute(const QualifiedName&) const;
    void synchronizeAttribute(const AtomString& localName) const;

    void updateName(const AtomString& oldName, const AtomString& newName);
    void updateNameForTreeScope(TreeScope&, const AtomString& oldName, const AtomString& newName);
    void updateNameForDocument(HTMLDocument&, const AtomString& oldName, const AtomString& newName);

    enum class NotifyObservers { No, Yes };
    void updateId(const AtomString& oldId, const AtomString& newId, NotifyObservers = NotifyObservers::Yes);
    void updateIdForTreeScope(TreeScope&, const AtomString& oldId, const AtomString& newId, NotifyObservers = NotifyObservers::Yes);

    enum HTMLDocumentNamedItemMapsUpdatingCondition { AlwaysUpdateHTMLDocumentNamedItemMaps, UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute };
    void updateIdForDocument(HTMLDocument&, const AtomString& oldId, const AtomString& newId, HTMLDocumentNamedItemMapsUpdatingCondition);
    void updateLabel(TreeScope&, const AtomString& oldForAttributeValue, const AtomString& newForAttributeValue);

    ExceptionOr<Node*> insertAdjacent(const String& where, Ref<Node>&& newChild);

    void scrollByUnits(int units, ScrollGranularity);

    NodeType nodeType() const final;
    bool childTypeAllowed(NodeType) const final;

    enum SynchronizationOfLazyAttribute { NotInSynchronizationOfLazyAttribute, InSynchronizationOfLazyAttribute };
    void setAttributeInternal(unsigned index, const QualifiedName&, const AtomString& value, SynchronizationOfLazyAttribute);
    void addAttributeInternal(const QualifiedName&, const AtomString& value, SynchronizationOfLazyAttribute);
    void removeAttributeInternal(unsigned index, SynchronizationOfLazyAttribute);

    LayoutRect absoluteEventBounds(bool& boundsIncludeAllDescendantElements, bool& includesFixedPositionElements);
    LayoutRect absoluteEventBoundsOfElementAndDescendants(bool& includesFixedPositionElements);
    
#if ENABLE(TREE_DEBUGGING)
    void formatForDebugger(char* buffer, unsigned length) const override;
#endif

#if ENABLE(INTERSECTION_OBSERVER)
    void disconnectFromIntersectionObservers();
#endif

#if ENABLE(RESIZE_OBSERVER)
    void disconnectFromResizeObservers();
#endif

    // The cloneNode function is private so that non-virtual cloneElementWith/WithoutChildren are used instead.
    Ref<Node> cloneNodeInternal(Document&, CloningOperation) override;
    virtual Ref<Element> cloneElementWithoutAttributesAndChildren(Document&);

    void removeShadowRoot();

    const RenderStyle& resolveComputedStyle();
    const RenderStyle& resolvePseudoElementStyle(PseudoId);

    unsigned rareDataChildIndex() const;

    SpellcheckAttributeState spellcheckAttributeState() const;

    void createUniqueElementData();

    ElementRareData* elementRareData() const;
    ElementRareData& ensureElementRareData();

    virtual int defaultTabIndex() const;

    void detachAllAttrNodesFromElement();
    void detachAttrNodeFromElementWithValue(Attr*, const AtomString& value);

    // Anyone thinking of using this should call document instead of ownerDocument.
    void ownerDocument() const = delete;
    
    void attachAttributeNodeIfNeeded(Attr&);

    QualifiedName m_tagName;
    RefPtr<ElementData> m_elementData;
};

inline bool Node::hasAttributes() const
{
    return is<Element>(*this) && downcast<Element>(*this).hasAttributes();
}

inline NamedNodeMap* Node::attributes() const
{
    return is<Element>(*this) ? &downcast<Element>(*this).attributes() : nullptr;
}

inline Element* Node::parentElement() const
{
    ContainerNode* parent = parentNode();
    return is<Element>(parent) ? downcast<Element>(parent) : nullptr;
}

inline const Element* Element::rootElement() const
{
    if (isConnected())
        return document().documentElement();

    const Element* highest = this;
    while (highest->parentElement())
        highest = highest->parentElement();
    return highest;
}

inline bool Element::hasAttributeWithoutSynchronization(const QualifiedName& name) const
{
    ASSERT(fastAttributeLookupAllowed(name));
    return elementData() && findAttributeByName(name);
}

inline const AtomString& Element::attributeWithoutSynchronization(const QualifiedName& name) const
{
    if (elementData()) {
        if (const Attribute* attribute = findAttributeByName(name))
            return attribute->value();
    }
    return nullAtom();
}

inline bool Element::hasAttributesWithoutUpdate() const
{
    return elementData() && !elementData()->isEmpty();
}

inline const AtomString& Element::idForStyleResolution() const
{
    return hasID() ? elementData()->idForStyleResolution() : nullAtom();
}

inline const AtomString& Element::getIdAttribute() const
{
    if (hasID())
        return elementData()->findAttributeByName(HTMLNames::idAttr)->value();
    return nullAtom();
}

inline const AtomString& Element::getNameAttribute() const
{
    if (hasName())
        return elementData()->findAttributeByName(HTMLNames::nameAttr)->value();
    return nullAtom();
}

inline void Element::setIdAttribute(const AtomString& value)
{
    setAttributeWithoutSynchronization(HTMLNames::idAttr, value);
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

inline bool shouldIgnoreAttributeCase(const Element& element)
{
    return element.isHTMLElement() && element.document().isHTMLDocument();
}

inline void Element::setHasFocusWithin(bool flag)
{
    if (hasFocusWithin() == flag)
        return;
    setFlag(flag, HasFocusWithin);
    if (styleAffectedByFocusWithin())
        invalidateStyleForSubtree();
}

template<typename... QualifiedNames>
inline const AtomString& Element::getAttribute(const QualifiedName& name, const QualifiedNames&... names) const
{
    const AtomString& value = getAttribute(name);
    if (!value.isNull())
        return value;
    return getAttribute(names...);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Element)
    static bool isType(const WebCore::Node& node) { return node.isElementNode(); }
    static bool isType(const WebCore::EventTarget& target) { return is<WebCore::Node>(target) && isType(downcast<WebCore::Node>(target)); }
SPECIALIZE_TYPE_TRAITS_END()
