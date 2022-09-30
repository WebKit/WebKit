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
#include "ContainerNode.h"
#include "ElementIdentifier.h"
#include "EventOptions.h"
#include "FocusOptions.h"
#include "HitTestRequest.h"
#include "QualifiedName.h"
#include "RenderPtr.h"
#include "ScrollTypes.h"
#include "ShadowRootMode.h"
#include "SimulatedClickOptions.h"
#include "WebAnimationTypes.h"
#include <JavaScriptCore/Forward.h>

#define DUMP_NODE_STATISTICS 0

namespace JSC {
class JSGlobalObject;
}

namespace WebCore {

class Attr;
class Attribute;
class AttributeIteratorAccessor;
class CustomElementDefaultARIA;
class CustomElementReactionQueue;
class DatasetDOMStringMap;
class DOMRect;
class DOMRectList;
class DOMTokenList;
class Document;
class ElementAnimationRareData;
class ElementData;
class ElementRareData;
class Frame;
class HTMLDocument;
class IntSize;
class JSCustomElementInterface;
class KeyframeEffectStack;
class KeyboardEvent;
class Locale;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformWheelEvent;
class PseudoElement;
class RenderStyle;
class RenderTreePosition;
class SpaceSplitString;
class StylePropertyMap;
class StylePropertyMapReadOnly;
class Text;
class UniqueElementData;
class WebAnimation;

enum class AnimationImpact;
enum class EventHandling : uint8_t;
enum class EventProcessing : uint8_t;
enum class IsSyntheticClick : bool { No, Yes };
enum class ResolveURLs : uint8_t { No, NoExcludingURLsForPrivacy, Yes, YesExcludingURLsForPrivacy };
enum class SelectionRestorationMode : uint8_t;

struct GetAnimationsOptions;
struct IntersectionObserverData;
struct KeyframeAnimationOptions;
struct ResizeObserverData;
struct ScrollIntoViewOptions;
struct ScrollToOptions;
struct SecurityPolicyViolationEventInit;
struct ShadowRootInit;

using ExplicitlySetAttrElementsMap = HashMap<QualifiedName, Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>>>;

namespace Style {
class Resolver;
enum class Change : uint8_t;
struct ElementStyle;
struct ResolutionContext;
}

class Element : public ContainerNode {
    WTF_MAKE_ISO_ALLOCATED(Element);
public:
    static Ref<Element> create(const QualifiedName&, Document&);
    virtual ~Element();

    WEBCORE_EXPORT bool hasAttribute(const QualifiedName&) const;
    WEBCORE_EXPORT const AtomString& getAttribute(const QualifiedName&) const;
    AtomString getAttributeForBindings(const QualifiedName&, ResolveURLs = ResolveURLs::NoExcludingURLsForPrivacy) const;
    template<typename... QualifiedNames>
    inline const AtomString& getAttribute(const QualifiedName&, const QualifiedNames&...) const;
    WEBCORE_EXPORT void setAttribute(const QualifiedName&, const AtomString& value);
    void setAttributeWithoutOverwriting(const QualifiedName&, const AtomString& value);
    WEBCORE_EXPORT void setAttributeWithoutSynchronization(const QualifiedName&, const AtomString& value);
    void setSynchronizedLazyAttribute(const QualifiedName&, const AtomString& value);
    bool removeAttribute(const QualifiedName&);
    Vector<String> getAttributeNames() const;

    // Typed getters and setters for language bindings.
    WEBCORE_EXPORT int getIntegralAttribute(const QualifiedName& attributeName) const;
    WEBCORE_EXPORT void setIntegralAttribute(const QualifiedName& attributeName, int value);
    WEBCORE_EXPORT unsigned getUnsignedIntegralAttribute(const QualifiedName& attributeName) const;
    WEBCORE_EXPORT void setUnsignedIntegralAttribute(const QualifiedName& attributeName, unsigned value);
    WEBCORE_EXPORT Element* getElementAttribute(const QualifiedName& attributeName) const;
    WEBCORE_EXPORT void setElementAttribute(const QualifiedName& attributeName, Element* value);
    WEBCORE_EXPORT std::optional<Vector<RefPtr<Element>>> getElementsArrayAttribute(const QualifiedName& attributeName) const;
    WEBCORE_EXPORT void setElementsArrayAttribute(const QualifiedName& attributeName, std::optional<Vector<RefPtr<Element>>>&& value);
    static bool isElementReflectionAttribute(const QualifiedName&);
    static bool isElementsArrayReflectionAttribute(const QualifiedName&);

    // Call this to get the value of an attribute that is known not to be the style
    // attribute or one of the SVG animatable attributes.
    inline bool hasAttributeWithoutSynchronization(const QualifiedName&) const;
    inline const AtomString& attributeWithoutSynchronization(const QualifiedName&) const;

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
    AtomString getAttributeForBindings(const AtomString& qualifiedName, ResolveURLs = ResolveURLs::NoExcludingURLsForPrivacy) const;
    inline AtomString getAttributeNSForBindings(const AtomString& namespaceURI, const AtomString& localName, ResolveURLs = ResolveURLs::NoExcludingURLsForPrivacy) const;

    WEBCORE_EXPORT ExceptionOr<void> setAttribute(const AtomString& qualifiedName, const AtomString& value);
    static ExceptionOr<QualifiedName> parseAttributeName(const AtomString& namespaceURI, const AtomString& qualifiedName);
    WEBCORE_EXPORT ExceptionOr<void> setAttributeNS(const AtomString& namespaceURI, const AtomString& qualifiedName, const AtomString& value);

    ExceptionOr<bool> toggleAttribute(const AtomString& qualifiedName, std::optional<bool> force);

    inline const AtomString& getIdAttribute() const;
    inline void setIdAttribute(const AtomString&);

    inline const AtomString& getNameAttribute() const;

    // Call this to get the value of the id attribute for style resolution purposes.
    // The value will already be lowercased if the document is in compatibility mode,
    // so this function is not suitable for non-style uses.
    inline const AtomString& idForStyleResolution() const;

    // Internal methods that assume the existence of attribute storage, one should use hasAttributes()
    // before calling them.
    inline AttributeIteratorAccessor attributesIterator() const;
    inline unsigned attributeCount() const;
    inline const Attribute& attributeAt(unsigned index) const;
    inline const Attribute* findAttributeByName(const QualifiedName&) const;
    inline unsigned findAttributeIndexByName(const QualifiedName&) const;
    inline unsigned findAttributeIndexByName(const AtomString&, bool shouldIgnoreAttributeCase) const;

    WEBCORE_EXPORT void scrollIntoView(std::optional<std::variant<bool, ScrollIntoViewOptions>>&& arg);
    WEBCORE_EXPORT void scrollIntoView(bool alignToTop = true);
    WEBCORE_EXPORT void scrollIntoViewIfNeeded(bool centerIfNeeded = true);
    WEBCORE_EXPORT void scrollIntoViewIfNotVisible(bool centerIfNotVisible = true);

    void scrollBy(const ScrollToOptions&);
    void scrollBy(double x, double y);
    virtual void scrollTo(const ScrollToOptions&, ScrollClamping = ScrollClamping::Clamped, ScrollSnapPointSelectionMethod = ScrollSnapPointSelectionMethod::Closest);
    void scrollTo(double x, double y);

    // These are only used by WebKitLegacy DOM API.
    WEBCORE_EXPORT void scrollByLines(int lines);
    WEBCORE_EXPORT void scrollByPages(int pages);

    WEBCORE_EXPORT int offsetLeftForBindings();
    WEBCORE_EXPORT int offsetLeft();
    WEBCORE_EXPORT int offsetTopForBindings();
    WEBCORE_EXPORT int offsetTop();
    WEBCORE_EXPORT int offsetWidth();
    WEBCORE_EXPORT int offsetHeight();

    bool mayCauseRepaintInsideViewport(const IntRect* visibleRect = nullptr) const;

    // FIXME: Replace uses of offsetParent in the platform with calls
    // to the render layer and merge bindingsOffsetParent and offsetParent.
    WEBCORE_EXPORT Element* offsetParentForBindings();

    inline const Element* rootElement() const;

    Element* offsetParent();
    WEBCORE_EXPORT int clientLeft();
    WEBCORE_EXPORT int clientTop();
    WEBCORE_EXPORT int clientWidth();
    WEBCORE_EXPORT int clientHeight();

    virtual int scrollLeft();
    virtual int scrollTop();
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);
    virtual int scrollWidth();
    virtual int scrollHeight();

    // This updates layout, and has custom handling for SVG.
    WEBCORE_EXPORT IntRect boundsInRootViewSpace();
    // This does not update layout, and uses absoluteBoundingBoxRect().
    WEBCORE_EXPORT IntRect boundingBoxInRootViewCoordinates() const;

    WEBCORE_EXPORT std::optional<std::pair<RenderObject*, FloatRect>> boundingAbsoluteRectWithoutLayout() const;

    WEBCORE_EXPORT FloatRect boundingClientRect();

    WEBCORE_EXPORT Ref<DOMRectList> getClientRects();
    Ref<DOMRect> getBoundingClientRect();

    // Returns the absolute bounding box translated into screen coordinates.
    WEBCORE_EXPORT IntRect screenRect() const;

    WEBCORE_EXPORT bool removeAttribute(const AtomString& qualifiedName);
    WEBCORE_EXPORT bool removeAttributeNS(const AtomString& namespaceURI, const AtomString& localName);
    void removeAttributeForBindings(const AtomString& qualifiedName) { removeAttribute(qualifiedName); }
    void removeAttributeNSForBindings(const AtomString& namespaceURI, const AtomString& localName) { removeAttributeNS(namespaceURI, localName); }

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
#endif
    String tagName() const { return nodeName(); }
    bool hasTagName(const QualifiedName& tagName) const { return m_tagName.matches(tagName); }
    bool hasTagName(const HTMLQualifiedName& tagName) const { return ContainerNode::hasTagName(tagName); }
    bool hasTagName(const MathMLQualifiedName& tagName) const { return ContainerNode::hasTagName(tagName); }
    inline bool hasTagName(const SVGQualifiedName& tagName) const;

    bool hasLocalName(const AtomString& other) const { return m_tagName.localName() == other; }

    const AtomString& localName() const final { return m_tagName.localName(); }
    const AtomString& prefix() const final { return m_tagName.prefix(); }
    const AtomString& namespaceURI() const final { return m_tagName.namespaceURI(); }

    ElementName elementName() const { return m_tagName.elementName(); }
    Namespace nodeNamespace() const { return m_tagName.nodeNamespace(); }

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

    // These functions are called whenever an attribute is added, changed or removed.
    virtual void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason = ModifiedDirectly);
    virtual void parseAttribute(const QualifiedName&, const AtomString&) { }

    // Only called by the parser immediately after element construction.
    void parserSetAttributes(const Vector<Attribute>&);

    bool isEventHandlerAttribute(const Attribute&) const;
    virtual bool attributeContainsJavaScriptURL(const Attribute&) const;

    // Remove attributes that might introduce scripting from the vector leaving the element unchanged.
    void stripScriptingAttributes(Vector<Attribute>&) const;

    const ElementData* elementData() const { return m_elementData.get(); }
    static ptrdiff_t elementDataMemoryOffset() { return OBJECT_OFFSETOF(Element, m_elementData); }
    inline UniqueElementData& ensureUniqueElementData();

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
    virtual bool rendererIsEverNeeded() { return true; }

    inline ShadowRoot* shadowRoot() const; // Defined in ElementRareData.h
    ShadowRoot* shadowRootForBindings(JSC::JSGlobalObject&) const;

    WEBCORE_EXPORT ExceptionOr<ShadowRoot&> attachShadow(const ShadowRootInit&);
    ExceptionOr<ShadowRoot&> attachDeclarativeShadow(ShadowRootMode, bool delegatesFocus);

    RefPtr<ShadowRoot> userAgentShadowRoot() const;
    WEBCORE_EXPORT ShadowRoot& ensureUserAgentShadowRoot();
    WEBCORE_EXPORT ShadowRoot& createUserAgentShadowRoot();

    void setIsDefinedCustomElement(JSCustomElementInterface&);
    void setIsFailedCustomElement();
    void setIsFailedOrPrecustomizedCustomElementWithoutClearingReactionQueue();
    void clearReactionQueueFromFailedCustomElement();
    void setIsCustomElementUpgradeCandidate();
    void enqueueToUpgrade(JSCustomElementInterface&);
    CustomElementReactionQueue* reactionQueue() const;

    CustomElementDefaultARIA& customElementDefaultARIA();
    CustomElementDefaultARIA* customElementDefaultARIAIfExists();

    // FIXME: This should not be virtual. Please do not add additional overrides of this function.
    virtual const AtomString& shadowPseudoId() const;

    bool isInActiveChain() const { return isUserActionElement() && isUserActionElementInActiveChain(); }
    bool active() const { return isUserActionElement() && isUserActionElementActive(); }
    bool hovered() const { return isUserActionElement() && isUserActionElementHovered(); }
    bool focused() const { return isUserActionElement() && isUserActionElementFocused(); }
    bool isBeingDragged() const { return isUserActionElement() && isUserActionElementDragged(); }
    bool hasFocusVisible() const { return isUserActionElement() && isUserActionElementHasFocusVisible(); }
    bool hasFocusWithin() const { return isUserActionElement() && isUserActionElementHasFocusWithin(); };

    virtual void setActive(bool = true, Style::InvalidationScope = Style::InvalidationScope::All);
    virtual void setHovered(bool = true, Style::InvalidationScope = Style::InvalidationScope::All, HitTestRequest = {});
    virtual void setFocus(bool, FocusVisibility = FocusVisibility::Invisible);
    void setBeingDragged(bool);
    void setHasFocusVisible(bool);
    void setHasFocusWithin(bool);

    std::optional<int> tabIndexSetExplicitly() const;
    bool shouldBeIgnoredInSequentialFocusNavigation() const { return defaultTabIndex() < 0 && !supportsFocus(); }
    virtual bool supportsFocus() const;
    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;

    virtual bool shouldUseInputMethod();

    virtual int tabIndexForBindings() const;
    WEBCORE_EXPORT void setTabIndexForBindings(int);

    // Used by the HTMLElement and SVGElement IDLs.
    WEBCORE_EXPORT const AtomString& nonce() const;
    WEBCORE_EXPORT void setNonce(const AtomString&);
    void hideNonce();

    ExceptionOr<void> insertAdjacentHTML(const String& where, const String& html, NodeVector* addedNodes);

    WEBCORE_EXPORT ExceptionOr<Element*> insertAdjacentElement(const String& where, Element& newChild);
    WEBCORE_EXPORT ExceptionOr<void> insertAdjacentHTML(const String& where, const String& html);
    WEBCORE_EXPORT ExceptionOr<void> insertAdjacentText(const String& where, String&& text);

    const RenderStyle* computedStyle(PseudoId = PseudoId::None) override;

    bool needsStyleInvalidation() const;

    bool hasValidStyle() const;
    bool isFocusableWithoutResolvingFullStyle() const;

    // Methods for indicating the style is affected by dynamic updates (e.g., children changing, our position changing in our sibling list, etc.)
    bool styleAffectedByEmpty() const { return hasStyleFlag(NodeStyleFlag::StyleAffectedByEmpty); }
    bool descendantsAffectedByPreviousSibling() const { return hasStyleFlag(NodeStyleFlag::DescendantsAffectedByPreviousSibling); }
    bool childrenAffectedByFirstChildRules() const { return hasStyleFlag(NodeStyleFlag::ChildrenAffectedByFirstChildRules); }
    bool childrenAffectedByLastChildRules() const { return hasStyleFlag(NodeStyleFlag::ChildrenAffectedByLastChildRules); }
    bool childrenAffectedByForwardPositionalRules() const { return hasStyleFlag(NodeStyleFlag::ChildrenAffectedByForwardPositionalRules); }
    bool descendantsAffectedByForwardPositionalRules() const { return hasStyleFlag(NodeStyleFlag::DescendantsAffectedByForwardPositionalRules); }
    bool childrenAffectedByBackwardPositionalRules() const { return hasStyleFlag(NodeStyleFlag::ChildrenAffectedByBackwardPositionalRules); }
    bool descendantsAffectedByBackwardPositionalRules() const { return hasStyleFlag(NodeStyleFlag::DescendantsAffectedByBackwardPositionalRules); }
    bool childrenAffectedByPropertyBasedBackwardPositionalRules() const { return hasStyleFlag(NodeStyleFlag::ChildrenAffectedByPropertyBasedBackwardPositionalRules); }
    bool affectsNextSiblingElementStyle() const { return hasStyleFlag(NodeStyleFlag::AffectsNextSiblingElementStyle); }
    bool styleIsAffectedByPreviousSibling() const { return hasStyleFlag(NodeStyleFlag::StyleIsAffectedByPreviousSibling); }
    unsigned childIndex() const { return hasRareData() ? rareDataChildIndex() : 0; }

    bool hasFlagsSetDuringStylingOfChildren() const;

    void setStyleAffectedByEmpty() { setStyleFlag(NodeStyleFlag::StyleAffectedByEmpty); }
    void setDescendantsAffectedByPreviousSibling() { setStyleFlag(NodeStyleFlag::DescendantsAffectedByPreviousSibling); }
    void setChildrenAffectedByFirstChildRules() { setStyleFlag(NodeStyleFlag::ChildrenAffectedByFirstChildRules); }
    void setChildrenAffectedByLastChildRules() { setStyleFlag(NodeStyleFlag::ChildrenAffectedByLastChildRules); }
    void setChildrenAffectedByForwardPositionalRules() { setStyleFlag(NodeStyleFlag::ChildrenAffectedByForwardPositionalRules); }
    void setDescendantsAffectedByForwardPositionalRules() { setStyleFlag(NodeStyleFlag::DescendantsAffectedByForwardPositionalRules); }
    void setChildrenAffectedByBackwardPositionalRules() { setStyleFlag(NodeStyleFlag::ChildrenAffectedByBackwardPositionalRules); }
    void setDescendantsAffectedByBackwardPositionalRules() { setStyleFlag(NodeStyleFlag::DescendantsAffectedByBackwardPositionalRules); }
    void setChildrenAffectedByPropertyBasedBackwardPositionalRules() { setStyleFlag(NodeStyleFlag::ChildrenAffectedByPropertyBasedBackwardPositionalRules); }
    void setAffectsNextSiblingElementStyle() { setStyleFlag(NodeStyleFlag::AffectsNextSiblingElementStyle); }
    void setStyleIsAffectedByPreviousSibling() { setStyleFlag(NodeStyleFlag::StyleIsAffectedByPreviousSibling); }
    void setChildIndex(unsigned);

    AtomString effectiveLang() const;
    AtomString langFromAttribute() const;
    Locale& locale() const;

    virtual bool accessKeyAction(bool /*sendToAnyEvent*/) { return false; }

    virtual bool isURLAttribute(const Attribute&) const { return false; }
    virtual bool attributeContainsURL(const Attribute& attribute) const { return isURLAttribute(attribute); }
    String resolveURLStringIfNeeded(const String& urlString, ResolveURLs = ResolveURLs::Yes, const URL& base = URL()) const;
    virtual String completeURLsInAttributeValue(const URL& base, const Attribute&, ResolveURLs = ResolveURLs::Yes) const;
    virtual bool isHTMLContentAttribute(const Attribute&) const { return false; }

    WEBCORE_EXPORT URL getURLAttribute(const QualifiedName&) const;
    inline URL getURLAttributeForBindings(const QualifiedName&) const;
    URL getNonEmptyURLAttribute(const QualifiedName&) const;

    virtual const AtomString& imageSourceURL() const;
    virtual AtomString target() const { return nullAtom(); }

    RefPtr<Element> findFocusDelegate();

    static AXTextStateChangeIntent defaultFocusTextStateChangeIntent() { return AXTextStateChangeIntent(AXTextStateChangeTypeSelectionMove, AXTextSelection { AXTextSelectionDirectionDiscontiguous, AXTextSelectionGranularityUnknown, true }); }
    virtual void focus(const FocusOptions& = { });
    virtual void focusForBindings(FocusOptions&&);
    void findTargetAndUpdateFocusAppearance(SelectionRestorationMode, SelectionRevealMode = SelectionRevealMode::Reveal);
    virtual RefPtr<Element> focusAppearanceUpdateTarget();
    virtual void updateFocusAppearance(SelectionRestorationMode, SelectionRevealMode = SelectionRevealMode::Reveal);
    virtual void blur();
    virtual void runFocusingStepsForAutofocus();

    WEBCORE_EXPORT String innerHTML() const;
    WEBCORE_EXPORT String outerHTML() const;
    WEBCORE_EXPORT ExceptionOr<void> setInnerHTML(const String&);
    WEBCORE_EXPORT ExceptionOr<void> setOuterHTML(const String&);
    WEBCORE_EXPORT String innerText();
    WEBCORE_EXPORT String outerText();
 
    virtual String title() const;

    const AtomString& pseudo() const;
    WEBCORE_EXPORT void setPseudo(const AtomString&);

    // Use Document::registerForDocumentActivationCallbacks() to subscribe to these
    virtual void prepareForDocumentSuspension() { }
    virtual void resumeFromDocumentSuspension() { }

    virtual void willBecomeFullscreenElement();
    virtual void ancestorWillEnterFullscreen() { }
    virtual void didBecomeFullscreenElement() { }
    virtual void willStopBeingFullscreenElement() { }
    virtual void didStopBeingFullscreenElement() { }

    bool isFinishedParsingChildren() const { return isParsingChildrenFinished(); }
    void finishParsingChildren() override;
    void beginParsingChildren() final;

    PseudoElement& ensurePseudoElement(PseudoId);
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

    SpaceSplitString partNames() const;
    DOMTokenList& part();

    DatasetDOMStringMap& dataset();

#if ENABLE(VIDEO)
    virtual bool isMediaElement() const { return false; }
#endif

    virtual bool isFormControlElement() const { return false; }
    virtual bool isFormControlElementWithState() const { return false; }
    virtual bool isSpinButtonElement() const { return false; }
    virtual bool isTextFormControlElement() const { return false; }
    virtual bool isTextField() const { return false; }
    virtual bool isTextPlaceholderElement() const { return false; }
    virtual bool isOptionalFormControl() const { return false; }
    virtual bool isRequiredFormControl() const { return false; }
    virtual bool isInRange() const { return false; }
    virtual bool isOutOfRange() const { return false; }
    virtual bool isUploadButton() const { return false; }
    virtual bool isSliderContainerElement() const { return false; }
    virtual bool isSliderThumbElement() const { return false; }
    virtual bool isHTMLTablePartElement() const { return false; }

    bool canContainRangeEndPoint() const override;

    // Used for disabled form elements; if true, prevents mouse events from being dispatched
    // to event listeners, and prevents DOMActivate events from being sent at all.
    virtual bool isDisabledFormControl() const { return false; }

    virtual bool childShouldCreateRenderer(const Node&) const;

    KeyframeEffectStack* keyframeEffectStack(PseudoId) const;
    KeyframeEffectStack& ensureKeyframeEffectStack(PseudoId);
    bool hasKeyframeEffects(PseudoId) const;

    const AnimationCollection* animations(PseudoId) const;
    bool hasCompletedTransitionForProperty(PseudoId, CSSPropertyID) const;
    bool hasRunningTransitionForProperty(PseudoId, CSSPropertyID) const;
    bool hasRunningTransitions(PseudoId) const;
    AnimationCollection& ensureAnimations(PseudoId);

    PropertyToTransitionMap& ensureCompletedTransitionsByProperty(PseudoId);
    PropertyToTransitionMap& ensureRunningTransitionsByProperty(PseudoId);
    CSSAnimationCollection& animationsCreatedByMarkup(PseudoId);
    void setAnimationsCreatedByMarkup(PseudoId, CSSAnimationCollection&&);

    const RenderStyle* lastStyleChangeEventStyle(PseudoId) const;
    void setLastStyleChangeEventStyle(PseudoId, std::unique_ptr<const RenderStyle>&&);

    void cssAnimationsDidUpdate(PseudoId);
    void keyframesRuleDidChange(PseudoId);
    bool hasPendingKeyframesUpdate(PseudoId) const;

    bool isInTopLayer() const { return hasNodeFlag(NodeFlag::IsInTopLayer); }
    void addToTopLayer();
    void removeFromTopLayer();

#if ENABLE(FULLSCREEN_API)
    bool containsFullScreenElement() const { return hasNodeFlag(NodeFlag::ContainsFullScreenElement); }
    void setContainsFullScreenElement(bool);
    void setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(bool);
    WEBCORE_EXPORT virtual void webkitRequestFullscreen();
#endif

    ExceptionOr<void> setPointerCapture(int32_t);
    ExceptionOr<void> releasePointerCapture(int32_t);
    bool hasPointerCapture(int32_t);

#if ENABLE(POINTER_LOCK)
    WEBCORE_EXPORT void requestPointerLock();
#endif

    bool isSpellCheckingEnabled() const;

    inline bool hasID() const;
    inline bool hasClass() const;
    inline bool hasName() const;
    inline const SpaceSplitString& classNames() const;

    IntPoint savedLayerScrollPosition() const;
    void setSavedLayerScrollPosition(const IntPoint&);

    bool dispatchMouseEvent(const PlatformMouseEvent&, const AtomString& eventType, int clickCount = 0, Element* relatedTarget = nullptr, IsSyntheticClick isSyntethicClick = IsSyntheticClick::No);
    bool dispatchWheelEvent(const PlatformWheelEvent&, OptionSet<EventHandling>&, EventIsCancelable = EventIsCancelable::Yes);
    bool dispatchKeyEvent(const PlatformKeyboardEvent&);
    bool dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions = SendNoEvents, SimulatedClickVisualOptions = ShowPressedLook);

    // FIXME: Consider changing signature to accept Element* because all callers perform copyRef().
    void dispatchFocusInEventIfNeeded(RefPtr<Element>&& oldFocusedElement);
    void dispatchFocusOutEventIfNeeded(RefPtr<Element>&& newFocusedElement);
    virtual void dispatchFocusEvent(RefPtr<Element>&& oldFocusedElement, const FocusOptions&);
    virtual void dispatchBlurEvent(RefPtr<Element>&& newFocusedElement);
    void dispatchWebKitImageReadyEventForTesting();

    WEBCORE_EXPORT bool dispatchMouseForceWillBegin();

    void enqueueSecurityPolicyViolationEvent(SecurityPolicyViolationEventInit&&);

    virtual void willRecalcStyle(Style::Change);
    virtual void didRecalcStyle(Style::Change);
    virtual void willResetComputedStyle();
    virtual void willAttachRenderers();
    virtual void didAttachRenderers();
    virtual void willDetachRenderers();
    virtual void didDetachRenderers();
    virtual std::optional<Style::ElementStyle> resolveCustomStyle(const Style::ResolutionContext&, const RenderStyle* shadowHostStyle);

    LayoutRect absoluteEventHandlerBounds(bool& includesFixedPositionElements) override;

    const RenderStyle* existingComputedStyle() const;
    WEBCORE_EXPORT const RenderStyle* renderOrDisplayContentsStyle(PseudoId = PseudoId::None) const;

    void clearBeforePseudoElement();
    void clearAfterPseudoElement();
    void resetComputedStyle();
    void resetStyleRelations();
    void resetChildStyleRelations();
    void resetAllDescendantStyleRelations();
    void clearHoverAndActiveStatusBeforeDetachingRenderer();

    WEBCORE_EXPORT URL absoluteLinkURL() const;

#if ENABLE(TOUCH_EVENTS)
    bool allowsDoubleTapGesture() const override;
#endif

    Style::Resolver& styleResolver();
    Style::ElementStyle resolveStyle(const Style::ResolutionContext&);

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
    void invalidateForQueryContainerSizeChange();
    void invalidateForResumingQueryContainerResolution();

    bool needsUpdateQueryContainerDependentStyle() const;
    void clearNeedsUpdateQueryContainerDependentStyle();

    void invalidateEventListenerRegions();

    bool hasDisplayContents() const;
    void storeDisplayContentsStyle(std::unique_ptr<RenderStyle>);

    using ContainerNode::setAttributeEventListener;
    void setAttributeEventListener(const AtomString& eventType, const QualifiedName& attributeName, const AtomString& value);

    IntersectionObserverData& ensureIntersectionObserverData();
    IntersectionObserverData* intersectionObserverDataIfExists();

    ResizeObserverData& ensureResizeObserverData();
    ResizeObserverData* resizeObserverData();

    Element* findAnchorElementForLink(String& outAnchorName);

    ExceptionOr<Ref<WebAnimation>> animate(JSC::JSGlobalObject&, JSC::Strong<JSC::JSObject>&&, std::optional<std::variant<double, KeyframeAnimationOptions>>&&);
    Vector<RefPtr<WebAnimation>> getAnimations(std::optional<GetAnimationsOptions>);

    WEBCORE_EXPORT ElementIdentifier identifier() const;
    WEBCORE_EXPORT static Element* fromIdentifier(ElementIdentifier);

    String description() const override;
    String debugDescription() const override;

    bool hasDuplicateAttribute() const { return m_hasDuplicateAttribute; };
    void setHasDuplicateAttribute(bool hasDuplicateAttribute) { m_hasDuplicateAttribute = hasDuplicateAttribute; };

    virtual void updateUserAgentShadowTree() { }

#if ENABLE(CSS_TYPED_OM)
    StylePropertyMapReadOnly* computedStyleMap();
#endif

    ExplicitlySetAttrElementsMap& explicitlySetAttrElementsMap();
    ExplicitlySetAttrElementsMap* explicitlySetAttrElementsMapIfExists() const;

protected:
    Element(const QualifiedName&, Document&, ConstructionType);

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;
    void childrenChanged(const ChildChange&) override;
    void removeAllEventListeners() final;
    virtual void parserDidSetAttributes();

    void setTabIndexExplicitly(std::optional<int>);

    void classAttributeChanged(const AtomString& newClassString);
    void partAttributeChanged(const AtomString& newValue);

    void addShadowRoot(Ref<ShadowRoot>&&);

    static ExceptionOr<void> mergeWithNextTextNode(Text&);

#if ENABLE(CSS_TYPED_OM)
    StylePropertyMap* attributeStyleMap();
    void setAttributeStyleMap(Ref<StylePropertyMap>&&);
#endif

    void updateLabel(TreeScope&, const AtomString& oldForAttributeValue, const AtomString& newForAttributeValue);

private:
    Frame* documentFrameWithNonNullView() const;

    bool isTextNode() const;

    bool isUserActionElementInActiveChain() const;
    bool isUserActionElementActive() const;
    bool isUserActionElementFocused() const;
    bool isUserActionElementHovered() const;
    bool isUserActionElementDragged() const;
    bool isUserActionElementHasFocusVisible() const;
    bool isUserActionElementHasFocusWithin() const;

    bool isNonceable() const;

    virtual void didAddUserAgentShadowRoot(ShadowRoot&) { }

    void didAddAttribute(const QualifiedName&, const AtomString&);
    void willModifyAttribute(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue);
    void didModifyAttribute(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue);
    void didRemoveAttribute(const QualifiedName&, const AtomString& oldValue);

    void synchronizeAttribute(const QualifiedName&) const;
    void synchronizeAttribute(const AtomString& localName) const;

    inline const Attribute* getAttributeInternal(const QualifiedName&) const;
    inline const Attribute* getAttributeInternal(const AtomString& qualifiedName) const;

    void updateName(const AtomString& oldName, const AtomString& newName);
    void updateNameForTreeScope(TreeScope&, const AtomString& oldName, const AtomString& newName);
    void updateNameForDocument(HTMLDocument&, const AtomString& oldName, const AtomString& newName);

    enum class NotifyObservers { No, Yes };
    void updateId(const AtomString& oldId, const AtomString& newId, NotifyObservers = NotifyObservers::Yes);
    void updateIdForTreeScope(TreeScope&, const AtomString& oldId, const AtomString& newId, NotifyObservers = NotifyObservers::Yes);

    enum HTMLDocumentNamedItemMapsUpdatingCondition { AlwaysUpdateHTMLDocumentNamedItemMaps, UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute };
    void updateIdForDocument(HTMLDocument&, const AtomString& oldId, const AtomString& newId, HTMLDocumentNamedItemMapsUpdatingCondition);

    ExceptionOr<Node*> insertAdjacent(const String& where, Ref<Node>&& newChild);

    void scrollByUnits(int units, ScrollGranularity);

    NodeType nodeType() const final;
    bool childTypeAllowed(NodeType) const final;

    enum SynchronizationOfLazyAttribute { NotInSynchronizationOfLazyAttribute, InSynchronizationOfLazyAttribute };
    void setAttributeInternal(unsigned index, const QualifiedName&, const AtomString& value, SynchronizationOfLazyAttribute);
    void addAttributeInternal(const QualifiedName&, const AtomString& value, SynchronizationOfLazyAttribute);
    void removeAttributeInternal(unsigned index, SynchronizationOfLazyAttribute);

    void setSavedLayerScrollPositionSlow(const IntPoint&);
    void clearBeforePseudoElementSlow();
    void clearAfterPseudoElementSlow();

    LayoutRect absoluteEventBounds(bool& boundsIncludeAllDescendantElements, bool& includesFixedPositionElements);
    LayoutRect absoluteEventBoundsOfElementAndDescendants(bool& includesFixedPositionElements);

    void disconnectFromIntersectionObservers();

    void disconnectFromResizeObservers();

    // The cloneNode function is private so that non-virtual cloneElementWith/WithoutChildren are used instead.
    Ref<Node> cloneNodeInternal(Document&, CloningOperation) override;
    virtual Ref<Element> cloneElementWithoutAttributesAndChildren(Document&);

    void removeShadowRoot();

    enum class ResolveComputedStyleMode { Normal, RenderedOnly };
    const RenderStyle* resolveComputedStyle(ResolveComputedStyleMode = ResolveComputedStyleMode::Normal);
    const RenderStyle& resolvePseudoElementStyle(PseudoId);

    unsigned rareDataChildIndex() const;

    void createUniqueElementData();

    inline ElementRareData* elementRareData() const;
    ElementRareData& ensureElementRareData();

    ElementAnimationRareData* animationRareData(PseudoId) const;
    ElementAnimationRareData& ensureAnimationRareData(PseudoId);

    virtual int defaultTabIndex() const;

    void detachAllAttrNodesFromElement();
    void detachAttrNodeFromElementWithValue(Attr*, const AtomString& value);

    // Anyone thinking of using this should call document instead of ownerDocument.
    void ownerDocument() const = delete;
    
    void attachAttributeNodeIfNeeded(Attr&);
    
#if ASSERT_ENABLED
    WEBCORE_EXPORT bool fastAttributeLookupAllowed(const QualifiedName&) const;
#endif

    QualifiedName m_tagName;
    RefPtr<ElementData> m_elementData;

    bool m_hasDuplicateAttribute { false };
};

inline void Element::setSavedLayerScrollPosition(const IntPoint& position)
{
    if (position.isZero() && !hasRareData())
        return;
    setSavedLayerScrollPositionSlow(position);
}

inline void Element::clearBeforePseudoElement()
{
    if (hasRareData())
        clearBeforePseudoElementSlow();
}

inline void Element::clearAfterPseudoElement()
{
    if (hasRareData())
        clearAfterPseudoElementSlow();
}

void invalidateForSiblingCombinators(Element* sibling);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Element)
    static bool isType(const WebCore::Node& node) { return node.isElementNode(); }
    static bool isType(const WebCore::EventTarget& target) { return is<WebCore::Node>(target) && isType(downcast<WebCore::Node>(target)); }
SPECIALIZE_TYPE_TRAITS_END()
