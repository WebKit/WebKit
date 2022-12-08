/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 *           (C) 2007 Eric Seidel (eric@webkit.org)
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
 */

#include "config.h"
#include "Element.h"

#include "AXObjectCache.h"
#include "Attr.h"
#include "AttributeChangeInvalidation.h"
#include "CSSParser.h"
#include "ChildChangeInvalidation.h"
#include "ChildListMutationScope.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClassChangeInvalidation.h"
#include "ComposedTreeAncestorIterator.h"
#include "ComposedTreeIterator.h"
#include "ComputedStylePropertyMapReadOnly.h"
#include "ContainerNodeAlgorithms.h"
#include "CustomElementReactionQueue.h"
#include "CustomElementRegistry.h"
#include "DOMRect.h"
#include "DOMRectList.h"
#include "DOMTokenList.h"
#include "DOMWindow.h"
#include "DocumentInlines.h"
#include "DocumentSharedObjectPool.h"
#include "Editing.h"
#include "ElementAnimationRareData.h"
#include "ElementInlines.h"
#include "ElementIterator.h"
#include "ElementName.h"
#include "ElementRareData.h"
#include "EventDispatcher.h"
#include "EventHandler.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FocusController.h"
#include "FocusEvent.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "FullscreenManager.h"
#include "FullscreenOptions.h"
#include "GetAnimationsOptions.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDialogElement.h"
#include "HTMLDocument.h"
#include "HTMLHtmlElement.h"
#include "HTMLLabelElement.h"
#include "HTMLNameCollection.h"
#include "HTMLObjectElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLScriptElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTemplateElement.h"
#include "IdChangeInvalidation.h"
#include "IdTargetObserverRegistry.h"
#include "InspectorInstrumentation.h"
#include "JSDOMPromiseDeferred.h"
#include "JSLazyEventListener.h"
#include "KeyboardEvent.h"
#include "KeyframeAnimationOptions.h"
#include "KeyframeEffect.h"
#include "Logging.h"
#include "MutationObserverInterestGroup.h"
#include "MutationRecord.h"
#include "NodeRenderStyle.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "PointerCaptureController.h"
#include "PointerEvent.h"
#include "PointerLockController.h"
#include "PseudoClassChangeInvalidation.h"
#include "Quirks.h"
#include "RenderFragmentedFlow.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderListBox.h"
#include "RenderSVGModelObject.h"
#include "RenderTheme.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "SVGScriptElement.h"
#include "ScriptDisallowedScope.h"
#include "ScrollIntoViewOptions.h"
#include "ScrollLatchingController.h"
#include "SecurityPolicyViolationEvent.h"
#include "SelectorQuery.h"
#include "Settings.h"
#include "ShadowRootInit.h"
#include "SimulatedClick.h"
#include "SlotAssignment.h"
#include "StyleInvalidator.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleTreeResolver.h"
#include "TextIterator.h"
#include "TouchAction.h"
#include "VoidCallback.h"
#include "WebAnimation.h"
#include "WebAnimationTypes.h"
#include "WheelEvent.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "markup.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Element);

using namespace HTMLNames;
using namespace XMLNames;

static HashMap<Element*, Vector<RefPtr<Attr>>>& attrNodeListMap()
{
    static NeverDestroyed<HashMap<Element*, Vector<RefPtr<Attr>>>> map;
    return map;
}

static Vector<RefPtr<Attr>>* attrNodeListForElement(Element& element)
{
    if (!element.hasSyntheticAttrChildNodes())
        return nullptr;
    ASSERT(attrNodeListMap().contains(&element));
    return &attrNodeListMap().find(&element)->value;
}

static Vector<RefPtr<Attr>>& ensureAttrNodeListForElement(Element& element)
{
    if (element.hasSyntheticAttrChildNodes()) {
        ASSERT(attrNodeListMap().contains(&element));
        return attrNodeListMap().find(&element)->value;
    }
    ASSERT(!attrNodeListMap().contains(&element));
    element.setHasSyntheticAttrChildNodes(true);
    return attrNodeListMap().add(&element, Vector<RefPtr<Attr>>()).iterator->value;
}

static void removeAttrNodeListForElement(Element& element)
{
    ASSERT(element.hasSyntheticAttrChildNodes());
    ASSERT(attrNodeListMap().contains(&element));
    attrNodeListMap().remove(&element);
    element.setHasSyntheticAttrChildNodes(false);
}

static Attr* findAttrNodeInList(Vector<RefPtr<Attr>>& attrNodeList, const QualifiedName& name)
{
    for (auto& node : attrNodeList) {
        if (node->qualifiedName().matches(name))
            return node.get();
    }
    return nullptr;
}

static Attr* findAttrNodeInList(Vector<RefPtr<Attr>>& attrNodeList, const AtomString& localName, bool shouldIgnoreAttributeCase)
{
    const AtomString& caseAdjustedName = shouldIgnoreAttributeCase ? localName.convertToASCIILowercase() : localName;
    for (auto& node : attrNodeList) {
        if (node->qualifiedName().localName() == caseAdjustedName)
            return node.get();
    }
    return nullptr;
}

static bool shouldAutofocus(const Element& element)
{
    if (!element.hasAttributeWithoutSynchronization(HTMLNames::autofocusAttr))
        return false;

    auto& document = element.document();
    if (!element.isInDocumentTree() || !document.hasBrowsingContext())
        return false;

    if (document.isSandboxed(SandboxAutomaticFeatures)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        document.addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked autofocusing on a form control because the form's frame is sandboxed and the 'allow-scripts' permission is not set."_s);
        return false;
    }
    if (!document.frame()->isMainFrame() && !document.topOrigin().isSameOriginDomain(document.securityOrigin())) {
        document.addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked autofocusing on a form control in a cross-origin subframe."_s);
        return false;
    }

    if (document.topDocument().isAutofocusProcessed())
        return false;

    return true;
}

Ref<Element> Element::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new Element(tagName, document, CreateElement));
}

Element::Element(const QualifiedName& tagName, Document& document, ConstructionType type)
    : ContainerNode(document, type)
    , m_tagName(tagName)
{
}

Element::~Element()
{
    ASSERT(!beforePseudoElement());
    ASSERT(!afterPseudoElement());

    disconnectFromIntersectionObservers();

    disconnectFromResizeObservers();

    removeShadowRoot();

    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

    if (hasRareData()) {
        if (auto* map = elementRareData()->attributeStyleMap())
            map->clearElement();
    }
}

inline ElementRareData& Element::ensureElementRareData()
{
    return static_cast<ElementRareData&>(ensureRareData());
}

inline void Node::setTabIndexState(TabIndexState state)
{
    auto bitfields = rareDataBitfields();
    bitfields.tabIndexState = static_cast<uint16_t>(state);
    setRareDataBitfields(bitfields);
}

void Element::setTabIndexExplicitly(std::optional<int> tabIndex)
{
    if (!tabIndex) {
        setTabIndexState(TabIndexState::NotSet);
        return;
    }
    setTabIndexState([this, value = tabIndex.value()]() {
        switch (value) {
        case 0:
            return TabIndexState::Zero;
        case -1:
            return TabIndexState::NegativeOne;
        default:
            ensureElementRareData().setUnusualTabIndex(value);
            return TabIndexState::InRareData;
        }
    }());
}

std::optional<int> Element::tabIndexSetExplicitly() const
{
    switch (tabIndexState()) {
    case TabIndexState::NotSet:
        return std::nullopt;
    case TabIndexState::Zero:
        return 0;
    case TabIndexState::NegativeOne:
        return -1;
    case TabIndexState::InRareData:
        ASSERT(hasRareData());
        return elementRareData()->unusualTabIndex();
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

int Element::defaultTabIndex() const
{
    return -1;
}

bool Element::isNonceable() const
{
    // https://www.w3.org/TR/CSP3/#is-element-nonceable
    if (elementRareData()->nonce().isNull())
        return false;

    if (hasDuplicateAttribute())
        return false;

    if (hasAttributes()
        && (is<HTMLScriptElement>(*this) || is<SVGScriptElement>(*this))) {
        static constexpr auto scriptString = "<script"_s;
        static constexpr auto styleString = "<style"_s;

        for (const auto& attribute : attributesIterator()) {
            auto name = attribute.localName().convertToASCIILowercase();
            auto value = attribute.value().convertToASCIILowercase();
            if (name.contains(scriptString)
                || name.contains(styleString)
                || value.contains(scriptString)
                || value.contains(styleString))
                return false;
        }
    }

    return true;
}

const AtomString& Element::nonce() const
{
    if (hasRareData() && isNonceable())
        return elementRareData()->nonce();

    return emptyAtom();
}

void Element::setNonce(const AtomString& newValue)
{
    if (newValue == emptyAtom() && !hasRareData())
        return;

    ensureElementRareData().setNonce(newValue);
}

void Element::hideNonce()
{
    // https://html.spec.whatwg.org/multipage/urls-and-fetching.html#nonce-attributes
    if (!isConnected())
        return;

    const auto& csp = document().contentSecurityPolicy();
    if (!csp->isHeaderDelivered())
        return;

    // Retain previous IDL nonce.
    AtomString currentNonce = nonce();

    if (!getAttribute(nonceAttr).isEmpty())
        setAttribute(nonceAttr, emptyAtom());

    setNonce(currentNonce);
}

bool Element::supportsFocus() const
{
    return !!tabIndexSetExplicitly();
}

int Element::tabIndexForBindings() const
{
    return valueOrCompute(tabIndexSetExplicitly(), [&] { return defaultTabIndex(); });
}

void Element::setTabIndexForBindings(int value)
{
    setIntegralAttribute(tabindexAttr, value);
}

bool Element::isKeyboardFocusable(KeyboardEvent*) const
{
    if (!(isFocusable() && !shouldBeIgnoredInSequentialFocusNavigation() && tabIndexSetExplicitly().value_or(0) >= 0))
        return false;
    if (auto* root = shadowRoot()) {
        if (root->delegatesFocus())
            return false;
    }
    return true;
}

bool Element::isMouseFocusable() const
{
    return isFocusable();
}

bool Element::shouldUseInputMethod()
{
    return computeEditability(UserSelectAllIsAlwaysNonEditable, ShouldUpdateStyle::Update) != Editability::ReadOnly;
}

static bool isForceEvent(const PlatformMouseEvent& platformEvent)
{
    return platformEvent.type() == PlatformEvent::MouseForceChanged || platformEvent.type() == PlatformEvent::MouseForceDown || platformEvent.type() == PlatformEvent::MouseForceUp;
}

static bool isCompatibilityMouseEvent(const MouseEvent& mouseEvent)
{
    // https://www.w3.org/TR/pointerevents/#compatibility-mapping-with-mouse-events
    const auto& type = mouseEvent.type();
    auto& eventNames = WebCore::eventNames();
    return type != eventNames.clickEvent && type != eventNames.mouseoverEvent && type != eventNames.mouseoutEvent && type != eventNames.mouseenterEvent && type != eventNames.mouseleaveEvent;
}

enum class ShouldIgnoreMouseEvent : bool { No, Yes };
static ShouldIgnoreMouseEvent dispatchPointerEventIfNeeded(Element& element, const MouseEvent& mouseEvent, const PlatformMouseEvent& platformEvent, bool& didNotSwallowEvent)
{
    if (auto* page = element.document().page()) {
        auto& pointerCaptureController = page->pointerCaptureController();
#if ENABLE(TOUCH_EVENTS)
        if (platformEvent.pointerId() != mousePointerID && mouseEvent.type() != eventNames().clickEvent && pointerCaptureController.preventsCompatibilityMouseEventsForIdentifier(platformEvent.pointerId()))
            return ShouldIgnoreMouseEvent::Yes;
#else
        UNUSED_PARAM(platformEvent);
#endif
        if (platformEvent.syntheticClickType() != NoTap)
            return ShouldIgnoreMouseEvent::No;

        if (auto pointerEvent = pointerCaptureController.pointerEventForMouseEvent(mouseEvent, platformEvent.pointerId(), platformEvent.pointerType())) {
            pointerCaptureController.dispatchEvent(*pointerEvent, &element);
            if (isCompatibilityMouseEvent(mouseEvent) && pointerCaptureController.preventsCompatibilityMouseEventsForIdentifier(pointerEvent->pointerId()))
                return ShouldIgnoreMouseEvent::Yes;
            if (pointerEvent->defaultPrevented() || pointerEvent->defaultHandled()) {
                didNotSwallowEvent = false;
                if (pointerEvent->type() == eventNames().pointerdownEvent)
                    return ShouldIgnoreMouseEvent::Yes;
            }
        }
    }

    return ShouldIgnoreMouseEvent::No;
}

bool Element::dispatchMouseEvent(const PlatformMouseEvent& platformEvent, const AtomString& eventType, int detail, Element* relatedTarget, IsSyntheticClick isSyntheticClick)
{
    if (isDisabledFormControl())
        return false;

    if (isForceEvent(platformEvent) && !document().hasListenerTypeForEventType(platformEvent.type()))
        return false;

    Ref<MouseEvent> mouseEvent = MouseEvent::create(eventType, document().windowProxy(), platformEvent, detail, relatedTarget);

    if (mouseEvent->type().isEmpty())
        return true; // Shouldn't happen.

    Ref protectedThis { *this };
    bool didNotSwallowEvent = true;

    if (dispatchPointerEventIfNeeded(*this, mouseEvent.get(), platformEvent, didNotSwallowEvent) == ShouldIgnoreMouseEvent::Yes)
        return false;
    
    auto isParentProcessAFullWebBrowser = false;
#if PLATFORM(IOS_FAMILY)
    if (Frame* frame = document().frame())
        isParentProcessAFullWebBrowser = frame->loader().client().isParentProcessAFullWebBrowser();
#elif PLATFORM(MAC)
    isParentProcessAFullWebBrowser = MacApplication::isSafari();
#endif
    if (Quirks::StorageAccessResult::ShouldCancelEvent == document().quirks().triggerOptionalStorageAccessQuirk(*this, platformEvent, eventType, detail, relatedTarget, isParentProcessAFullWebBrowser, isSyntheticClick))
        return false;

    ASSERT(!mouseEvent->target() || mouseEvent->target() != relatedTarget);
    dispatchEvent(mouseEvent);
    if (mouseEvent->defaultPrevented() || mouseEvent->defaultHandled())
        didNotSwallowEvent = false;

    if (mouseEvent->type() == eventNames().clickEvent && mouseEvent->detail() == 2) {
        // Special case: If it's a double click event, we also send the dblclick event. This is not part
        // of the DOM specs, but is used for compatibility with the ondblclick="" attribute. This is treated
        // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
        // FIXME: Is it okay that mouseEvent may have been mutated by scripts via initMouseEvent in dispatchEvent above?
        Ref<MouseEvent> doubleClickEvent = MouseEvent::create(eventNames().dblclickEvent,
            mouseEvent->bubbles() ? Event::CanBubble::Yes : Event::CanBubble::No,
            mouseEvent->cancelable() ? Event::IsCancelable::Yes : Event::IsCancelable::No,
            Event::IsComposed::Yes,
            mouseEvent->view(), mouseEvent->detail(),
            mouseEvent->screenX(), mouseEvent->screenY(), mouseEvent->clientX(), mouseEvent->clientY(),
            mouseEvent->modifierKeys(), mouseEvent->button(), mouseEvent->buttons(), mouseEvent->syntheticClickType(), relatedTarget);

        if (mouseEvent->defaultHandled())
            doubleClickEvent->setDefaultHandled();

        dispatchEvent(doubleClickEvent);
        if (doubleClickEvent->defaultHandled() || doubleClickEvent->defaultPrevented())
            return false;
    }
    return didNotSwallowEvent;
}

bool Element::dispatchWheelEvent(const PlatformWheelEvent& platformEvent, OptionSet<EventHandling>& processing, Event::IsCancelable isCancelable)
{
    auto event = WheelEvent::create(platformEvent, document().windowProxy(), isCancelable);

    // Events with no deltas are important because they convey platform information about scroll gestures
    // and momentum beginning or ending. However, those events should not be sent to the DOM since some
    // websites will break. They need to be dispatched because dispatching them will call into the default
    // event handler, and our platform code will correctly handle the phase changes. Calling stopPropagation()
    // will prevent the event from being sent to the DOM, but will still call the default event handler.
    // FIXME: Move this logic into WheelEvent::create.
    if (platformEvent.delta().isZero())
        event->stopPropagation();
    else
        processing.add(EventHandling::DispatchedToDOM);

    dispatchEvent(event);
    
    LOG_WITH_STREAM(Scrolling, stream << "Element " << *this << " dispatchWheelEvent: (cancelable " << event->cancelable() << ") defaultPrevented " << event->defaultPrevented() << " defaultHandled " << event->defaultHandled());
    
    if (event->defaultPrevented())
        processing.add(EventHandling::DefaultPrevented);

    if (event->defaultHandled())
        processing.add(EventHandling::DefaultHandled);

    return !event->defaultPrevented() && !event->defaultHandled();
}

bool Element::dispatchKeyEvent(const PlatformKeyboardEvent& platformEvent)
{
    auto event = KeyboardEvent::create(platformEvent, document().windowProxy());

    if (Frame* frame = document().frame()) {
        if (frame->eventHandler().accessibilityPreventsEventPropagation(event))
            event->stopPropagation();
    }

    dispatchEvent(event);
    return !event->defaultPrevented() && !event->defaultHandled();
}

bool Element::dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions eventOptions, SimulatedClickVisualOptions visualOptions)
{
    return simulateClick(*this, underlyingEvent, eventOptions, visualOptions, SimulatedClickSource::UserAgent);
}

Ref<Node> Element::cloneNodeInternal(Document& targetDocument, CloningOperation type)
{
    switch (type) {
    case CloningOperation::OnlySelf:
    case CloningOperation::SelfWithTemplateContent:
        return cloneElementWithoutChildren(targetDocument);
    case CloningOperation::Everything:
        break;
    }
    return cloneElementWithChildren(targetDocument);
}

Ref<Element> Element::cloneElementWithChildren(Document& targetDocument)
{
    Ref<Element> clone = cloneElementWithoutChildren(targetDocument);
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { clone };
    cloneChildNodes(clone);
    return clone;
}

Ref<Element> Element::cloneElementWithoutChildren(Document& targetDocument)
{
    Ref<Element> clone = cloneElementWithoutAttributesAndChildren(targetDocument);

    // This will catch HTML elements in the wrong namespace that are not correctly copied.
    // This is a sanity check as HTML overloads some of the DOM methods.
    ASSERT(isHTMLElement() == clone->isHTMLElement());

    clone->cloneDataFromElement(*this);
    return clone;
}

Ref<Element> Element::cloneElementWithoutAttributesAndChildren(Document& targetDocument)
{
    return targetDocument.createElement(tagQName(), false);
}

Ref<Attr> Element::detachAttribute(unsigned index)
{
    ASSERT(elementData());

    const Attribute& attribute = elementData()->attributeAt(index);

    RefPtr<Attr> attrNode = attrIfExists(attribute.name());
    if (attrNode)
        detachAttrNodeFromElementWithValue(attrNode.get(), attribute.value());
    else
        attrNode = Attr::create(document(), attribute.name(), attribute.value());

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    return attrNode.releaseNonNull();
}

bool Element::removeAttribute(const QualifiedName& name)
{
    if (!elementData())
        return false;

    unsigned index = elementData()->findAttributeIndexByName(name);
    if (index == ElementData::attributeNotFound)
        return false;

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    return true;
}

void Element::setBooleanAttribute(const QualifiedName& name, bool value)
{
    if (value)
        setAttribute(name, emptyAtom());
    else
        removeAttribute(name);
}

NamedNodeMap& Element::attributes() const
{
    ElementRareData& rareData = const_cast<Element*>(this)->ensureElementRareData();
    if (NamedNodeMap* attributeMap = rareData.attributeMap())
        return *attributeMap;

    rareData.setAttributeMap(makeUnique<NamedNodeMap>(const_cast<Element&>(*this)));
    return *rareData.attributeMap();
}

Node::NodeType Element::nodeType() const
{
    return ELEMENT_NODE;
}

bool Element::hasAttribute(const QualifiedName& name) const
{
    return hasAttributeNS(name.namespaceURI(), name.localName());
}

void Element::synchronizeAllAttributes() const
{
    if (!elementData())
        return;
    if (elementData()->styleAttributeIsDirty()) {
        ASSERT(isStyledElement());
        static_cast<const StyledElement*>(this)->synchronizeStyleAttributeInternal();
    }
    
    if (isSVGElement())
        downcast<SVGElement>(const_cast<Element&>(*this)).synchronizeAllAttributes();
}

ALWAYS_INLINE void Element::synchronizeAttribute(const QualifiedName& name) const
{
    if (!elementData())
        return;
    if (UNLIKELY(name == styleAttr && elementData()->styleAttributeIsDirty())) {
        ASSERT_WITH_SECURITY_IMPLICATION(isStyledElement());
        static_cast<const StyledElement*>(this)->synchronizeStyleAttributeInternal();
        return;
    }

    if (isSVGElement())
        downcast<SVGElement>(const_cast<Element&>(*this)).synchronizeAttribute(name);
}

static ALWAYS_INLINE bool isStyleAttribute(const Element& element, const AtomString& attributeLocalName)
{
    if (shouldIgnoreAttributeCase(element))
        return equalLettersIgnoringASCIICase(attributeLocalName, "style"_s);
    return attributeLocalName == styleAttr->localName();
}

ALWAYS_INLINE void Element::synchronizeAttribute(const AtomString& localName) const
{
    // This version of synchronizeAttribute() is streamlined for the case where you don't have a full QualifiedName,
    // e.g when called from DOM API.
    if (!elementData())
        return;
    if (elementData()->styleAttributeIsDirty() && isStyleAttribute(*this, localName)) {
        ASSERT_WITH_SECURITY_IMPLICATION(isStyledElement());
        static_cast<const StyledElement*>(this)->synchronizeStyleAttributeInternal();
        return;
    }

    if (isSVGElement())
        downcast<SVGElement>(const_cast<Element&>(*this)).synchronizeAttribute(QualifiedName(nullAtom(), localName, nullAtom()));
}

const AtomString& Element::getAttribute(const QualifiedName& name) const
{
    if (auto* attribute = getAttributeInternal(name))
        return attribute->value();
    return nullAtom();
}

AtomString Element::getAttributeForBindings(const QualifiedName& name, ResolveURLs resolveURLs) const
{
    auto* attribute = getAttributeInternal(name);
    if (!attribute)
        return nullAtom();

    if (!attributeContainsURL(*attribute))
        return attribute->value();

    switch (resolveURLs) {
    case ResolveURLs::Yes:
    case ResolveURLs::YesExcludingURLsForPrivacy:
    case ResolveURLs::NoExcludingURLsForPrivacy:
        return AtomString(completeURLsInAttributeValue(URL(), *attribute, resolveURLs));

    case ResolveURLs::No:
        break;
    }

    return attribute->value();
}

Vector<String> Element::getAttributeNames() const
{
    Vector<String> attributesVector;
    if (!hasAttributes())
        return attributesVector;

    auto attributes = attributesIterator();
    attributesVector.reserveInitialCapacity(attributes.attributeCount());
    for (auto& attribute : attributes)
        attributesVector.uncheckedAppend(attribute.name().toString());
    return attributesVector;
}

bool Element::isFocusable() const
{
    if (!isConnected() || !supportsFocus())
        return false;

    if (!renderer()) {
        // Elements in canvas fallback content are not rendered, but they are allowed to be
        // focusable as long as their canvas is displayed and visible.
        if (auto* canvas = ancestorsOfType<HTMLCanvasElement>(*this).first())
            return canvas->isFocusableWithoutResolvingFullStyle();
    } else if (renderer()->isSkippedContent())
        return false;

    return isFocusableWithoutResolvingFullStyle();
}

bool Element::isUserActionElementInActiveChain() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isInActiveChain(*this);
}

bool Element::isUserActionElementActive() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isActive(*this);
}

bool Element::isUserActionElementFocused() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isFocused(*this);
}

bool Element::isUserActionElementHovered() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isHovered(*this);
}

bool Element::isUserActionElementDragged() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isBeingDragged(*this);
}

bool Element::isUserActionElementHasFocusVisible() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().hasFocusVisible(*this);
}

bool Element::isUserActionElementHasFocusWithin() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().hasFocusWithin(*this);
}

void Element::setActive(bool value, Style::InvalidationScope invalidationScope)
{
    if (value == active())
        return;
    {
        Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassActive, value, invalidationScope);
        document().userActionElements().setActive(*this, value);
    }

    if (!renderer())
        return;

    if (renderer()->style().hasEffectiveAppearance())
        renderer()->theme().stateChanged(*renderer(), ControlStates::States::Pressed);
}

static bool shouldAlwaysHaveFocusVisibleWhenFocused(const Element& element)
{
    return element.isTextField() || element.isContentEditable() || is<HTMLSelectElement>(element);
}

void Element::setFocus(bool value, FocusVisibility visibility)
{
    if (value == focused())
        return;
    
    Style::PseudoClassChangeInvalidation focusStyleInvalidation(*this, { { CSSSelector::PseudoClassFocus, value }, { CSSSelector::PseudoClassFocusVisible, value } });
    document().userActionElements().setFocused(*this, value);

    // Shadow host with a slot that contain focused element is not considered focused.
    for (auto* root = containingShadowRoot(); root; root = root->host()->containingShadowRoot()) {
        root->setContainsFocusedElement(value);
        root->host()->invalidateStyle();
    }

    for (auto* element = this; element; element = element->parentElementInComposedTree())
        element->setHasFocusWithin(value);

    setHasFocusVisible(value && (visibility == FocusVisibility::Visible || shouldAlwaysHaveFocusVisibleWhenFocused(*this)));
}

void Element::setHasFocusVisible(bool value)
{
    if (!document().settings().focusVisibleEnabled())
        return;

#if ASSERT_ENABLED
    ASSERT(!value || focused());
    ASSERT(!focused() || !shouldAlwaysHaveFocusVisibleWhenFocused(*this) || value);
#endif

    if (hasFocusVisible() == value)
        return;

    document().userActionElements().setHasFocusVisible(*this, value);
}

void Element::setHasFocusWithin(bool value)
{
    if (hasFocusWithin() == value)
        return;
    {
        Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassFocusWithin, value);
        document().userActionElements().setHasFocusWithin(*this, value);
    }
}

void Element::setHasTentativeFocus(bool value)
{
    // Tentative focus is used when trying to set the focus on a new element.
    for (auto& ancestor : composedTreeAncestors(*this)) {
        ASSERT(ancestor.hasFocusWithin() != value);
        document().userActionElements().setHasFocusWithin(ancestor, value);
    }
}

void Element::setHovered(bool value, Style::InvalidationScope invalidationScope, HitTestRequest)
{
    if (value == hovered())
        return;
    {
        Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassHover, value, invalidationScope);
        document().userActionElements().setHovered(*this, value);
    }

    if (auto* style = renderStyle(); style && style->hasEffectiveAppearance())
        renderer()->theme().stateChanged(*renderer(), ControlStates::States::Hovered);
}

void Element::setBeingDragged(bool value)
{
    if (value == isBeingDragged())
        return;

    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassDrag, value);
    document().userActionElements().setBeingDragged(*this, value);
}

inline ScrollAlignment toScrollAlignmentForInlineDirection(std::optional<ScrollLogicalPosition> position, WritingMode writingMode, bool isLTR)
{
    switch (position.value_or(ScrollLogicalPosition::Nearest)) {
    case ScrollLogicalPosition::Start: {
        switch (writingMode) {
        case WritingMode::TopToBottom:
        case WritingMode::BottomToTop: {
            return isLTR ? ScrollAlignment::alignLeftAlways : ScrollAlignment::alignRightAlways;
        }
        case WritingMode::LeftToRight:
        case WritingMode::RightToLeft: {
            return isLTR ? ScrollAlignment::alignTopAlways : ScrollAlignment::alignBottomAlways;
        }
        default:
            ASSERT_NOT_REACHED();
            return ScrollAlignment::alignLeftAlways;
        }
    }
    case ScrollLogicalPosition::Center:
        return ScrollAlignment::alignCenterAlways;
    case ScrollLogicalPosition::End: {
        switch (writingMode) {
        case WritingMode::TopToBottom:
        case WritingMode::BottomToTop: {
            return isLTR ? ScrollAlignment::alignRightAlways : ScrollAlignment::alignLeftAlways;
        }
        case WritingMode::LeftToRight:
        case WritingMode::RightToLeft: {
            return isLTR ? ScrollAlignment::alignBottomAlways : ScrollAlignment::alignTopAlways;
        }
        default:
            ASSERT_NOT_REACHED();
            return ScrollAlignment::alignRightAlways;
        }
    }
    case ScrollLogicalPosition::Nearest:
        return ScrollAlignment::alignToEdgeIfNeeded;
    default:
        ASSERT_NOT_REACHED();
        return ScrollAlignment::alignToEdgeIfNeeded;
    }
}

inline ScrollAlignment toScrollAlignmentForBlockDirection(std::optional<ScrollLogicalPosition> position, WritingMode writingMode)
{
    switch (position.value_or(ScrollLogicalPosition::Start)) {
    case ScrollLogicalPosition::Start: {
        switch (writingMode) {
        case WritingMode::TopToBottom:
            return ScrollAlignment::alignTopAlways;
        case WritingMode::BottomToTop:
            return ScrollAlignment::alignBottomAlways;
        case WritingMode::LeftToRight:
            return ScrollAlignment::alignLeftAlways;
        case WritingMode::RightToLeft:
            return ScrollAlignment::alignRightAlways;
        default:
            ASSERT_NOT_REACHED();
            return ScrollAlignment::alignTopAlways;
        }
    }
    case ScrollLogicalPosition::Center:
        return ScrollAlignment::alignCenterAlways;
    case ScrollLogicalPosition::End: {
        switch (writingMode) {
        case WritingMode::TopToBottom:
            return ScrollAlignment::alignBottomAlways;
        case WritingMode::BottomToTop:
            return ScrollAlignment::alignTopAlways;
        case WritingMode::LeftToRight:
            return ScrollAlignment::alignRightAlways;
        case WritingMode::RightToLeft:
            return ScrollAlignment::alignLeftAlways;
        default:
            ASSERT_NOT_REACHED();
            return ScrollAlignment::alignBottomAlways;
        }
    }
    case ScrollLogicalPosition::Nearest:
        return ScrollAlignment::alignToEdgeIfNeeded;
    default:
        ASSERT_NOT_REACHED();
        return ScrollAlignment::alignToEdgeIfNeeded;
    }
}

static std::optional<std::pair<RenderElement*, LayoutRect>> listBoxElementScrollIntoView(const Element& element)
{
    auto owningSelectElement = [](const Element& element) -> HTMLSelectElement* {
        if (is<HTMLOptionElement>(element))
            return downcast<HTMLOptionElement>(element).ownerSelectElement();

        if (is<HTMLOptGroupElement>(element))
            return downcast<HTMLOptGroupElement>(element).ownerSelectElement();

        return nullptr;
    };

    auto* selectElement = owningSelectElement(element);
    if (!selectElement || !selectElement->renderer() || !is<RenderListBox>(selectElement->renderer()))
        return { };

    auto& renderListBox = downcast<RenderListBox>(*selectElement->renderer());
    
    auto itemIndex = selectElement->listItems().find(&element);
    if (itemIndex != notFound)
        renderListBox.scrollToRevealElementAtListIndex(itemIndex);
    else
        itemIndex = 0;

    auto itemLocalRect = renderListBox.itemBoundingBoxRect({ }, itemIndex);
    return std::pair<RenderElement*, LayoutRect> { &renderListBox, itemLocalRect };
}

void Element::scrollIntoView(std::optional<std::variant<bool, ScrollIntoViewOptions>>&& arg)
{
    document().updateLayoutIgnorePendingStylesheets();

    RenderElement* renderer = nullptr;
    bool insideFixed = false;
    LayoutRect absoluteBounds;

    if (auto listBoxScrollResult = listBoxElementScrollIntoView(*this)) {
        renderer = listBoxScrollResult->first;
        absoluteBounds = listBoxScrollResult->second;

        auto listBoxAbsoluteBounds = renderer->absoluteAnchorRectWithScrollMargin(&insideFixed).marginRect;
        absoluteBounds.moveBy(listBoxAbsoluteBounds.location());
    } else {
        renderer = this->renderer();
        if (!renderer)
            return;

        absoluteBounds = renderer->absoluteAnchorRectWithScrollMargin(&insideFixed).marginRect;
    }

    ScrollIntoViewOptions options;
    if (arg) {
        auto value = arg.value();
        if (std::holds_alternative<ScrollIntoViewOptions>(value))
            options = std::get<ScrollIntoViewOptions>(value);
        else if (!std::get<bool>(value))
            options.blockPosition = ScrollLogicalPosition::End;
    }

    auto writingMode = renderer->style().writingMode();
    auto alignX = toScrollAlignmentForInlineDirection(options.inlinePosition, writingMode, renderer->style().isLeftToRightDirection());
    auto alignY = toScrollAlignmentForBlockDirection(options.blockPosition, writingMode);
    alignX.disableLegacyHorizontalVisibilityThreshold();

    bool isHorizontal = renderer->style().isHorizontalWritingMode();
    auto visibleOptions = ScrollRectToVisibleOptions {
        SelectionRevealMode::Reveal,
        isHorizontal ? alignX : alignY,
        isHorizontal ? alignY : alignX,
        ShouldAllowCrossOriginScrolling::No,
        options.behavior.value_or(ScrollBehavior::Auto)
    };
    FrameView::scrollRectToVisible(absoluteBounds, *renderer, insideFixed, visibleOptions);
}

void Element::scrollIntoView(bool alignToTop) 
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    bool insideFixed;
    LayoutRect absoluteBounds = renderer()->absoluteAnchorRectWithScrollMargin(&insideFixed).marginRect;

    // Align to the top / bottom and to the closest edge.
    auto alignY = alignToTop ? ScrollAlignment::alignTopAlways : ScrollAlignment::alignBottomAlways;
    auto alignX = ScrollAlignment::alignToEdgeIfNeeded;
    alignX.disableLegacyHorizontalVisibilityThreshold();

    FrameView::scrollRectToVisible(absoluteBounds, *renderer(), insideFixed, { SelectionRevealMode::Reveal, alignX, alignY, ShouldAllowCrossOriginScrolling::No });
}

void Element::scrollIntoViewIfNeeded(bool centerIfNeeded)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    bool insideFixed;
    LayoutRect absoluteBounds = renderer()->absoluteAnchorRectWithScrollMargin(&insideFixed).marginRect;

    auto alignY = centerIfNeeded ? ScrollAlignment::alignCenterIfNeeded : ScrollAlignment::alignToEdgeIfNeeded;
    auto alignX = centerIfNeeded ? ScrollAlignment::alignCenterIfNeeded : ScrollAlignment::alignToEdgeIfNeeded;
    alignX.disableLegacyHorizontalVisibilityThreshold();

    FrameView::scrollRectToVisible(absoluteBounds, *renderer(), insideFixed, { SelectionRevealMode::Reveal, alignX, alignY, ShouldAllowCrossOriginScrolling::No });
}

void Element::scrollIntoViewIfNotVisible(bool centerIfNotVisible)
{
    document().updateLayoutIgnorePendingStylesheets();
    
    if (!renderer())
        return;
    
    bool insideFixed;
    LayoutRect absoluteBounds = renderer()->absoluteAnchorRectWithScrollMargin(&insideFixed).marginRect;
    auto align = centerIfNotVisible ? ScrollAlignment::alignCenterIfNotVisible : ScrollAlignment::alignToEdgeIfNotVisible;
    FrameView::scrollRectToVisible(absoluteBounds, *renderer(), insideFixed, { SelectionRevealMode::Reveal, align, align, ShouldAllowCrossOriginScrolling::No });
}

void Element::scrollBy(const ScrollToOptions& options)
{
    ScrollToOptions scrollToOptions = normalizeNonFiniteCoordinatesOrFallBackTo(options, 0, 0);
    scrollToOptions.left.value() += scrollLeft();
    scrollToOptions.top.value() += scrollTop();
    scrollTo(scrollToOptions, ScrollClamping::Clamped, ScrollSnapPointSelectionMethod::Directional);
}

void Element::scrollBy(double x, double y)
{
    scrollBy(ScrollToOptions(x, y));
}

void Element::scrollTo(const ScrollToOptions& options, ScrollClamping clamping, ScrollSnapPointSelectionMethod snapPointSelectionMethod)
{
    LOG_WITH_STREAM(Scrolling, stream << "Element " << *this << " scrollTo " << options.left << ", " << options.top);

    if (!document().settings().CSSOMViewScrollingAPIEnabled()) {
        // If the element is the root element and document is in quirks mode, terminate these steps.
        // Note that WebKit always uses quirks mode document scrolling behavior. See Document::scrollingElement().
        if (this == document().documentElement())
            return;
    }
    
    if (auto* view = document().view())
        view->cancelScheduledScrolls();

    document().updateLayoutIgnorePendingStylesheets();

    if (document().scrollingElement() == this) {
        // If the element is the scrolling element and is not potentially scrollable,
        // invoke scroll() on window with options as the only argument, and terminate these steps.
        // FIXME: Scrolling an independently scrollable body is broken: webkit.org/b/161612.
        RefPtr window = document().domWindow();
        if (!window)
            return;

        window->scrollTo(options, clamping, snapPointSelectionMethod);
        return;
    }

    // If the element does not have any associated CSS layout box, the element has no associated scrolling box,
    // or the element has no overflow, terminate these steps.
    RenderBox* renderer = renderBox();
    if (!renderer || !renderer->hasNonVisibleOverflow())
        return;

    auto scrollToOptions = normalizeNonFiniteCoordinatesOrFallBackTo(options,
        adjustForAbsoluteZoom(renderer->scrollLeft(), *renderer),
        adjustForAbsoluteZoom(renderer->scrollTop(), *renderer)
    );
    IntPoint scrollPosition(
        clampToInteger(scrollToOptions.left.value() * renderer->style().effectiveZoom()),
        clampToInteger(scrollToOptions.top.value() * renderer->style().effectiveZoom())
    );

    auto animated = useSmoothScrolling(scrollToOptions.behavior.value_or(ScrollBehavior::Auto), this) ? ScrollIsAnimated::Yes : ScrollIsAnimated::No;
    auto scrollPositionChangeOptions = ScrollPositionChangeOptions::createProgrammaticWithOptions(clamping, animated, snapPointSelectionMethod);
    renderer->setScrollPosition(scrollPosition, scrollPositionChangeOptions);
}

void Element::scrollTo(double x, double y)
{
    scrollTo(ScrollToOptions(x, y));
}

void Element::scrollByUnits(int units, ScrollGranularity granularity)
{
    document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = this->renderer();
    if (!renderer)
        return;

    if (!renderer->hasNonVisibleOverflow())
        return;

    auto direction = units < 0 ? ScrollUp : ScrollDown;
    auto* stopElement = this;
    downcast<RenderBox>(*renderer).scroll(direction, granularity, std::abs(units), &stopElement);
}

void Element::scrollByLines(int lines)
{
    scrollByUnits(lines, ScrollGranularity::Line);
}

void Element::scrollByPages(int pages)
{
    scrollByUnits(pages, ScrollGranularity::Page);
}

static double localZoomForRenderer(const RenderElement& renderer)
{
    // FIXME: This does the wrong thing if two opposing zooms are in effect and canceled each
    // other out, but the alternative is that we'd have to crawl up the whole render tree every
    // time (or store an additional bit in the RenderStyle to indicate that a zoom was specified).
    double zoomFactor = 1;
    if (renderer.style().effectiveZoom() != 1) {
        // Need to find the nearest enclosing RenderElement that set up
        // a differing zoom, and then we divide our result by it to eliminate the zoom.
        const RenderElement* prev = &renderer;
        for (RenderElement* curr = prev->parent(); curr; curr = curr->parent()) {
            if (curr->style().effectiveZoom() != prev->style().effectiveZoom()) {
                zoomFactor = prev->style().zoom();
                break;
            }
            prev = curr;
        }
        if (prev->isRenderView())
            zoomFactor = prev->style().zoom();
    }
    return zoomFactor;
}

static int adjustContentsScrollPositionOrSizeForZoom(int value, const Frame& frame)
{
    double zoomFactor = frame.pageZoomFactor() * frame.frameScaleFactor();
    if (zoomFactor == 1)
        return value;
    // FIXME (webkit.org/b/189397): Why can't we just ceil/floor?
    // Needed because of truncation (rather than rounding) when scaling up.
    if (zoomFactor > 1)
        value++;
    return static_cast<int>(value / zoomFactor);
}

enum LegacyCSSOMElementMetricsRoundingStrategy { Round, Floor };

static int convertToNonSubpixelValue(double value, const LegacyCSSOMElementMetricsRoundingStrategy roundStrategy = Round)
{
    return roundStrategy == Round ? std::round(value) : std::floor(value);
}

static int adjustOffsetForZoomAndSubpixelLayout(RenderBoxModelObject& renderer, const LayoutUnit& offset)
{
    auto offsetLeft = LayoutUnit { roundToInt(offset) };
    double zoomFactor = localZoomForRenderer(renderer);
    if (zoomFactor == 1)
        return convertToNonSubpixelValue(offsetLeft, Floor);
    return convertToNonSubpixelValue(offsetLeft / zoomFactor, Round);
}

static HashSet<TreeScope*> collectAncestorTreeScopeAsHashSet(Node& node)
{
    HashSet<TreeScope*> ancestors;
    for (auto* currentScope = &node.treeScope(); currentScope; currentScope = currentScope->parentTreeScope())
        ancestors.add(currentScope);
    return ancestors;
}

int Element::offsetLeftForBindings()
{
    auto offset = offsetLeft();

    RefPtr parent = offsetParent();
    if (!parent || !parent->isInShadowTree())
        return offset;

    ASSERT(&parent->document() == &document());
    if (&parent->treeScope() == &treeScope())
        return offset;

    auto ancestorTreeScopes = collectAncestorTreeScopeAsHashSet(*this);
    while (parent && !ancestorTreeScopes.contains(&parent->treeScope())) {
        offset += parent->offsetLeft();
        parent = parent->offsetParent();
    }

    return offset;
}

int Element::offsetLeft()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustOffsetForZoomAndSubpixelLayout(*renderer, renderer->offsetLeft());
    return 0;
}

int Element::offsetTopForBindings()
{
    auto offset = offsetTop();

    RefPtr parent = offsetParent();
    if (!parent || !parent->isInShadowTree())
        return offset;

    ASSERT(&parent->document() == &document());
    if (&parent->treeScope() == &treeScope())
        return offset;

    auto ancestorTreeScopes = collectAncestorTreeScopeAsHashSet(*this);
    while (parent && !ancestorTreeScopes.contains(&parent->treeScope())) {
        offset += parent->offsetTop();
        parent = parent->offsetParent();
    }

    return offset;
}

int Element::offsetTop()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustOffsetForZoomAndSubpixelLayout(*renderer, renderer->offsetTop());
    return 0;
}

int Element::offsetWidth()
{
    document().updateLayoutIfDimensionsOutOfDate(*this, WidthDimensionsCheck);
    if (RenderBoxModelObject* renderer = renderBoxModelObject()) {
        auto offsetWidth = LayoutUnit { roundToInt(renderer->offsetWidth()) };
        return convertToNonSubpixelValue(adjustLayoutUnitForAbsoluteZoom(offsetWidth, *renderer).toDouble());
    }
    return 0;
}

int Element::offsetHeight()
{
    document().updateLayoutIfDimensionsOutOfDate(*this, HeightDimensionsCheck);
    if (RenderBoxModelObject* renderer = renderBoxModelObject()) {
        auto offsetHeight = LayoutUnit { roundToInt(renderer->offsetHeight()) };
        return convertToNonSubpixelValue(adjustLayoutUnitForAbsoluteZoom(offsetHeight, *renderer).toDouble());
    }
    return 0;
}

Element* Element::offsetParentForBindings()
{
    Element* element = offsetParent();
    if (!element || !element->isInShadowTree())
        return element;
    while (element && !isDescendantOrShadowDescendantOf(&element->rootNode()))
        element = element->offsetParent();
    return element;
}

Element* Element::offsetParent()
{
    document().updateLayoutIgnorePendingStylesheets();
    auto renderer = this->renderer();
    if (!renderer)
        return nullptr;
    auto offsetParent = renderer->offsetParent();
    if (!offsetParent)
        return nullptr;
    return offsetParent->element();
}

int Element::clientLeft()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (auto* renderer = renderBox()) {
        auto clientLeft = LayoutUnit { roundToInt(renderer->clientLeft()) };
        return convertToNonSubpixelValue(adjustLayoutUnitForAbsoluteZoom(clientLeft, *renderer).toDouble());
    }
    return 0;
}

int Element::clientTop()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (auto* renderer = renderBox()) {
        auto clientTop = LayoutUnit { roundToInt(renderer->clientTop()) };
        return convertToNonSubpixelValue(adjustLayoutUnitForAbsoluteZoom(clientTop, *renderer).toDouble());
    }
    return 0;
}

int Element::clientWidth()
{
    document().updateLayoutIfDimensionsOutOfDate(*this, WidthDimensionsCheck);

    if (!document().hasLivingRenderTree())
        return 0;

    RenderView& renderView = *document().renderView();

    // When in strict mode, clientWidth for the document element should return the width of the containing frame.
    // When in quirks mode, clientWidth for the body element should return the width of the containing frame.
    bool inQuirksMode = document().inQuirksMode();
    if ((!inQuirksMode && document().documentElement() == this) || (inQuirksMode && isHTMLElement() && document().bodyOrFrameset() == this))
        return adjustForAbsoluteZoom(renderView.frameView().layoutWidth(), renderView);
    
    if (RenderBox* renderer = renderBox()) {
        auto clientWidth = LayoutUnit { roundToInt(renderer->clientWidth()) };
        // clientWidth/Height is the visual portion of the box content, not including
        // borders or scroll bars, but includes padding. And per
        // https://www.w3.org/TR/CSS2/tables.html#model,
        // table wrapper box is a principal block box that contains the table box
        // itself and any caption boxes, and table grid box is a block-level box that
        // contains the table's internal table boxes. When table's border is specified
        // in CSS, the border is added to table grid box, not table wrapper box.
        // Currently, WebKit doesn't have table wrapper box, and we are supposed to
        // retrieve clientWidth/Height from table wrapper box, not table grid box. So
        // when we retrieve clientWidth/Height, it includes table's border size.
        if (renderer->isTable())
            clientWidth += renderer->borderLeft() + renderer->borderRight();
        return convertToNonSubpixelValue(adjustLayoutUnitForAbsoluteZoom(clientWidth, *renderer).toDouble());
    }
    return 0;
}

int Element::clientHeight()
{
    document().updateLayoutIfDimensionsOutOfDate(*this, HeightDimensionsCheck);
    if (!document().hasLivingRenderTree())
        return 0;

    RenderView& renderView = *document().renderView();

    // When in strict mode, clientHeight for the document element should return the height of the containing frame.
    // When in quirks mode, clientHeight for the body element should return the height of the containing frame.
    bool inQuirksMode = document().inQuirksMode();
    if ((!inQuirksMode && document().documentElement() == this) || (inQuirksMode && isHTMLElement() && document().bodyOrFrameset() == this))
        return adjustForAbsoluteZoom(renderView.frameView().layoutHeight(), renderView);

    if (RenderBox* renderer = renderBox()) {
        auto clientHeight = LayoutUnit { roundToInt(renderer->clientHeight()) };
        // clientWidth/Height is the visual portion of the box content, not including
        // borders or scroll bars, but includes padding. And per
        // https://www.w3.org/TR/CSS2/tables.html#model,
        // table wrapper box is a principal block box that contains the table box
        // itself and any caption boxes, and table grid box is a block-level box that
        // contains the table's internal table boxes. When table's border is specified
        // in CSS, the border is added to table grid box, not table wrapper box.
        // Currently, WebKit doesn't have table wrapper box, and we are supposed to
        // retrieve clientWidth/Height from table wrapper box, not table grid box. So
        // when we retrieve clientWidth/Height, it includes table's border size.
        if (renderer->isTable())
            clientHeight += renderer->borderTop() + renderer->borderBottom();
        return convertToNonSubpixelValue(adjustLayoutUnitForAbsoluteZoom(clientHeight, *renderer).toDouble());
    }
    return 0;
}

ALWAYS_INLINE Frame* Element::documentFrameWithNonNullView() const
{
    auto* frame = document().frame();
    return frame && frame->view() ? frame : nullptr;
}

int Element::scrollLeft()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (document().scrollingElement() == this) {
        if (auto* frame = documentFrameWithNonNullView())
            return adjustContentsScrollPositionOrSizeForZoom(frame->view()->contentsScrollPosition().x(), *frame);
        return 0;
    }

    if (auto* renderer = renderBox())
        return adjustForAbsoluteZoom(renderer->scrollLeft(), *renderer);
    return 0;
}

int Element::scrollTop()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (document().scrollingElement() == this) {
        if (auto* frame = documentFrameWithNonNullView())
            return adjustContentsScrollPositionOrSizeForZoom(frame->view()->contentsScrollPosition().y(), *frame);
        return 0;
    }

    if (RenderBox* renderer = renderBox())
        return adjustForAbsoluteZoom(renderer->scrollTop(), *renderer);
    return 0;
}

void Element::setScrollLeft(int newLeft)
{
    document().updateLayoutIgnorePendingStylesheets();

    auto options = ScrollPositionChangeOptions::createProgrammatic();
    options.animated = useSmoothScrolling(ScrollBehavior::Auto, this) ? ScrollIsAnimated::Yes : ScrollIsAnimated::No;

    if (document().scrollingElement() == this) {
        if (RefPtr frame = documentFrameWithNonNullView()) {
            IntPoint position(static_cast<int>(newLeft * frame->pageZoomFactor() * frame->frameScaleFactor()), frame->view()->scrollY());
            frame->view()->setScrollPosition(position, options);
        }
        return;
    }

    if (WeakPtr renderer = renderBox()) {
        int clampedLeft = clampToInteger(newLeft * renderer->style().effectiveZoom());
        renderer->setScrollLeft(clampedLeft, options);
        if (auto* scrollableArea = renderer && renderer->layer() ? renderer->layer()->scrollableArea() : nullptr)
            scrollableArea->setScrollShouldClearLatchedState(true);
    }
}

void Element::setScrollTop(int newTop)
{
    document().updateLayoutIgnorePendingStylesheets();

    auto options = ScrollPositionChangeOptions::createProgrammatic();
    options.animated = useSmoothScrolling(ScrollBehavior::Auto, this) ? ScrollIsAnimated::Yes : ScrollIsAnimated::No;

    if (document().scrollingElement() == this) {
        if (RefPtr frame = documentFrameWithNonNullView()) {
            IntPoint position(frame->view()->scrollX(), static_cast<int>(newTop * frame->pageZoomFactor() * frame->frameScaleFactor()));
            frame->view()->setScrollPosition(position, options);
        }
        return;
    }

    if (WeakPtr renderer = renderBox()) {
        int clampedTop = clampToInteger(newTop * renderer->style().effectiveZoom());
        renderer->setScrollTop(clampedTop, options);
        if (auto* scrollableArea = renderer && renderer->layer() ? renderer->layer()->scrollableArea() : nullptr)
            scrollableArea->setScrollShouldClearLatchedState(true);
    }
}

int Element::scrollWidth()
{
    document().updateLayoutIfDimensionsOutOfDate(*this, WidthDimensionsCheck);

    if (document().scrollingElement() == this) {
        // FIXME (webkit.org/b/182289): updateLayoutIfDimensionsOutOfDate seems to ignore zoom level change.
        document().updateLayoutIgnorePendingStylesheets();
        if (auto* frame = documentFrameWithNonNullView())
            return adjustContentsScrollPositionOrSizeForZoom(frame->view()->contentsWidth(), *frame);
        return 0;
    }

    if (auto* renderer = renderBox())
        return adjustForAbsoluteZoom(renderer->scrollWidth(), *renderer);
    return 0;
}

int Element::scrollHeight()
{
    document().updateLayoutIfDimensionsOutOfDate(*this, HeightDimensionsCheck);

    if (document().scrollingElement() == this) {
        // FIXME (webkit.org/b/182289): updateLayoutIfDimensionsOutOfDate seems to ignore zoom level change.
        document().updateLayoutIgnorePendingStylesheets();
        if (auto* frame = documentFrameWithNonNullView())
            return adjustContentsScrollPositionOrSizeForZoom(frame->view()->contentsHeight(), *frame);
        return 0;
    }

    if (auto* renderer = renderBox())
        return adjustForAbsoluteZoom(renderer->scrollHeight(), *renderer);
    return 0;
}

inline bool shouldObtainBoundsFromSVGModel(const Element* element)
{
    ASSERT(element);
    if (!element->isSVGElement() || !element->renderer())
        return false;

    // Legacy SVG engine specific condition.
    if (element->renderer()->isLegacySVGRoot())
        return false;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // LBSE specific condition.
    if (element->document().settings().layerBasedSVGEngineEnabled())
        return false;
#endif

    return true;
}

inline bool shouldObtainBoundsFromBoxModel(const Element* element)
{
    ASSERT(element);
    if (!element->renderer())
        return false;

    if (is<RenderBoxModelObject>(element->renderer()))
        return true;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (is<RenderSVGModelObject>(element->renderer()))
        return true;
#endif

    return false;
}

IntRect Element::boundsInRootViewSpace()
{
    document().updateLayoutIgnorePendingStylesheets();

    FrameView* view = document().view();
    if (!view)
        return IntRect();

    Vector<FloatQuad> quads;

    if (shouldObtainBoundsFromSVGModel(this)) {
        // Get the bounding rectangle from the SVG model.
        SVGElement& svgElement = downcast<SVGElement>(*this);
        if (auto localRect = svgElement.getBoundingBox())
            quads.append(renderer()->localToAbsoluteQuad(*localRect));
    } else if (shouldObtainBoundsFromBoxModel(this)) {
        // Get the bounding rectangle from the box model.
        renderer()->absoluteQuads(quads);
    }

    return view->contentsToRootView(enclosingIntRect(unitedBoundingBoxes(quads)));
}

IntRect Element::boundingBoxInRootViewCoordinates() const
{
    if (RenderObject* renderer = this->renderer())
        return document().view()->contentsToRootView(renderer->absoluteBoundingBoxRect());
    return IntRect();
}

static bool layoutOverflowRectContainsAllDescendants(const RenderBox& renderBox)
{
    if (renderBox.isRenderView())
        return true;

    if (!renderBox.element())
        return false;

    // If there are any position:fixed inside of us, game over.
    if (auto* viewPositionedObjects = renderBox.view().positionedObjects()) {
        for (auto* positionedBox : *viewPositionedObjects) {
            if (positionedBox == &renderBox)
                continue;
            if (positionedBox->isFixedPositioned() && renderBox.element()->contains(positionedBox->element()))
                return false;
        }
    }

    if (renderBox.canContainAbsolutelyPositionedObjects()) {
        // Our layout overflow will include all descendant positioned elements.
        return true;
    }

    // This renderer may have positioned descendants whose containing block is some ancestor.
    if (auto* containingBlock = RenderObject::containingBlockForPositionType(PositionType::Absolute, renderBox)) {
        if (auto* positionedObjects = containingBlock->positionedObjects()) {
            for (auto* positionedBox : *positionedObjects) {
                if (positionedBox == &renderBox)
                    continue;
                if (renderBox.element()->contains(positionedBox->element()))
                    return false;
            }
        }
    }
    return false;
}

LayoutRect Element::absoluteEventBounds(bool& boundsIncludeAllDescendantElements, bool& includesFixedPositionElements)
{
    boundsIncludeAllDescendantElements = false;
    includesFixedPositionElements = false;

    if (!renderer())
        return LayoutRect();

    LayoutRect result;
    if (shouldObtainBoundsFromSVGModel(this)) {
        // Get the bounding rectangle from the SVG model.
        SVGElement& svgElement = downcast<SVGElement>(*this);
        if (auto localRect = svgElement.getBoundingBox())
            result = LayoutRect(renderer()->localToAbsoluteQuad(*localRect, UseTransforms, &includesFixedPositionElements).boundingBox());
    } else {
        auto* renderer = this->renderer();
        if (is<RenderBox>(renderer)) {
            auto& box = downcast<RenderBox>(*renderer);

            bool computedBounds = false;
            
            if (RenderFragmentedFlow* fragmentedFlow = box.enclosingFragmentedFlow()) {
                bool wasFixed = false;
                Vector<FloatQuad> quads;
                if (fragmentedFlow->absoluteQuadsForBox(quads, &wasFixed, &box)) {
                    result = LayoutRect(unitedBoundingBoxes(quads));
                    computedBounds = true;
                } else {
                    // Probably columns. Just return the bounds of the multicol block for now.
                    // FIXME: this doesn't handle nested columns.
                    RenderElement* multicolContainer = fragmentedFlow->parent();
                    if (multicolContainer && is<RenderBox>(multicolContainer)) {
                        auto overflowRect = downcast<RenderBox>(*multicolContainer).layoutOverflowRect();
                        result = LayoutRect(multicolContainer->localToAbsoluteQuad(FloatRect(overflowRect), UseTransforms, &includesFixedPositionElements).boundingBox());
                        computedBounds = true;
                    }
                }
            }

            if (!computedBounds) {
                LayoutRect overflowRect = box.layoutOverflowRect();
                result = LayoutRect(box.localToAbsoluteQuad(FloatRect(overflowRect), UseTransforms, &includesFixedPositionElements).boundingBox());
                boundsIncludeAllDescendantElements = layoutOverflowRectContainsAllDescendants(box);
            }
        } else
            result = LayoutRect(renderer->absoluteBoundingBoxRect(true /* useTransforms */, &includesFixedPositionElements));
    }

    return result;
}

LayoutRect Element::absoluteEventBoundsOfElementAndDescendants(bool& includesFixedPositionElements)
{
    bool boundsIncludeDescendants;
    LayoutRect result = absoluteEventBounds(boundsIncludeDescendants, includesFixedPositionElements);
    if (boundsIncludeDescendants)
        return result;

    for (auto& child : childrenOfType<Element>(*this)) {
        bool includesFixedPosition = false;
        LayoutRect childBounds = child.absoluteEventBoundsOfElementAndDescendants(includesFixedPosition);
        includesFixedPositionElements |= includesFixedPosition;
        result.unite(childBounds);
    }

    return result;
}

LayoutRect Element::absoluteEventHandlerBounds(bool& includesFixedPositionElements)
{
    // This is not web-exposed, so don't call the FOUC-inducing updateLayoutIgnorePendingStylesheets().
    FrameView* frameView = document().view();
    if (!frameView)
        return LayoutRect();

    return absoluteEventBoundsOfElementAndDescendants(includesFixedPositionElements);
}

static std::optional<std::pair<RenderObject*, LayoutRect>> listBoxElementBoundingBox(const Element& element)
{
    auto owningSelectElement = [](const Element& element) -> HTMLSelectElement* {
        if (is<HTMLOptionElement>(element))
            return downcast<HTMLOptionElement>(element).ownerSelectElement();
        
        if (is<HTMLOptGroupElement>(element))
            return downcast<HTMLOptGroupElement>(element).ownerSelectElement();

        return nullptr;
    };

    auto* selectElement = owningSelectElement(element);
    if (!selectElement || !selectElement->renderer() || !is<RenderListBox>(selectElement->renderer()))
        return std::nullopt;

    auto& listBoxRenderer = downcast<RenderListBox>(*selectElement->renderer());
    std::optional<LayoutRect> boundingBox;
    if (is<HTMLOptionElement>(element))
        boundingBox = listBoxRenderer.localBoundsOfOption(downcast<HTMLOptionElement>(element));
    else if (is<HTMLOptGroupElement>(element))
        boundingBox = listBoxRenderer.localBoundsOfOptGroup(downcast<HTMLOptGroupElement>(element));

    if (!boundingBox)
        return { };

    return std::pair<RenderObject*, LayoutRect> { &listBoxRenderer, boundingBox.value() };
}

Ref<DOMRectList> Element::getClientRects()
{
    document().updateLayoutIgnorePendingStylesheets();

    RenderObject* renderer = this->renderer();
    Vector<FloatQuad> quads;

    if (shouldObtainBoundsFromSVGModel(this)) {
        if (auto localRect = downcast<SVGElement>(*this).getBoundingBox())
            quads.append(renderer->localToAbsoluteQuad(*localRect));
    } else if (auto pair = listBoxElementBoundingBox(*this)) {
        renderer = pair.value().first;
        quads.append(renderer->localToAbsoluteQuad(FloatQuad { pair.value().second }));
    } else if (shouldObtainBoundsFromBoxModel(this))
        renderer->absoluteQuads(quads);

    // FIXME: Handle table/inline-table with a caption.

    if (quads.isEmpty())
        return DOMRectList::create();

    document().convertAbsoluteToClientQuads(quads, renderer->style());
    return DOMRectList::create(quads);
}

std::optional<std::pair<RenderObject*, FloatRect>> Element::boundingAbsoluteRectWithoutLayout() const
{
    RenderObject* renderer = this->renderer();
    Vector<FloatQuad> quads;
    if (shouldObtainBoundsFromSVGModel(this)) {
        if (auto localRect = downcast<SVGElement>(*this).getBoundingBox())
            quads.append(renderer->localToAbsoluteQuad(*localRect));
    } else if (auto pair = listBoxElementBoundingBox(*this)) {
        renderer = pair.value().first;
        quads.append(renderer->localToAbsoluteQuad(FloatQuad { pair.value().second }));
    } else if (shouldObtainBoundsFromBoxModel(this))
        renderer->absoluteQuads(quads);

    if (quads.isEmpty())
        return std::nullopt;

    return std::make_pair(renderer, unitedBoundingBoxes(quads));
}

FloatRect Element::boundingClientRect()
{
    document().updateLayoutIgnorePendingStylesheets();
    auto pair = boundingAbsoluteRectWithoutLayout();
    if (!pair)
        return { };
    RenderObject* renderer = pair->first;
    FloatRect result = pair->second;
    document().convertAbsoluteToClientRect(result, renderer->style());
    return result;
}

Ref<DOMRect> Element::getBoundingClientRect()
{
    return DOMRect::create(boundingClientRect());
}

IntRect Element::screenRect() const
{
    if (RenderObject* renderer = this->renderer())
        return document().view()->contentsToScreen(renderer->absoluteBoundingBoxRect());
    return IntRect();
}

const AtomString& Element::getAttribute(const AtomString& qualifiedName) const
{
    if (auto* attribute = getAttributeInternal(qualifiedName))
        return attribute->value();
    return nullAtom();
}

AtomString Element::getAttributeForBindings(const AtomString& qualifiedName, ResolveURLs resolveURLs) const
{
    auto* attribute = getAttributeInternal(qualifiedName);
    if (!attribute)
        return nullAtom();

    if (!attributeContainsURL(*attribute))
        return attribute->value();

    switch (resolveURLs) {
    case ResolveURLs::Yes:
    case ResolveURLs::YesExcludingURLsForPrivacy:
    case ResolveURLs::NoExcludingURLsForPrivacy:
        return AtomString(completeURLsInAttributeValue(URL(), *attribute, resolveURLs));

    case ResolveURLs::No:
        break;
    }

    return attribute->value();
}

const AtomString& Element::getAttributeNS(const AtomString& namespaceURI, const AtomString& localName) const
{
    return getAttribute(QualifiedName(nullAtom(), localName, namespaceURI));
}

// https://dom.spec.whatwg.org/#dom-element-toggleattribute
ExceptionOr<bool> Element::toggleAttribute(const AtomString& qualifiedName, std::optional<bool> force)
{
    if (!Document::isValidName(qualifiedName))
        return Exception { InvalidCharacterError };

    synchronizeAttribute(qualifiedName);

    auto caseAdjustedQualifiedName = shouldIgnoreAttributeCase(*this) ? qualifiedName.convertToASCIILowercase() : qualifiedName;
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(caseAdjustedQualifiedName, false) : ElementData::attributeNotFound;
    if (index == ElementData::attributeNotFound) {
        if (!force || *force) {
            setAttributeInternal(index, QualifiedName { nullAtom(), caseAdjustedQualifiedName, nullAtom() }, emptyAtom(), NotInSynchronizationOfLazyAttribute);
            return true;
        }
        return false;
    }

    if (!force || !*force) {
        removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
        return false;
    }
    return true;
}

ExceptionOr<void> Element::setAttribute(const AtomString& qualifiedName, const AtomString& value)
{
    if (!Document::isValidName(qualifiedName))
        return Exception { InvalidCharacterError };

    synchronizeAttribute(qualifiedName);
    auto caseAdjustedQualifiedName = shouldIgnoreAttributeCase(*this) ? qualifiedName.convertToASCIILowercase() : qualifiedName;
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(caseAdjustedQualifiedName, false) : ElementData::attributeNotFound;
    auto name = index != ElementData::attributeNotFound ? attributeAt(index).name() : QualifiedName { nullAtom(), caseAdjustedQualifiedName, nullAtom() };
    setAttributeInternal(index, name, value, NotInSynchronizationOfLazyAttribute);

    return { };
}

void Element::setAttribute(const QualifiedName& name, const AtomString& value)
{
    synchronizeAttribute(name);
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(name) : ElementData::attributeNotFound;
    setAttributeInternal(index, name, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setAttributeWithoutOverwriting(const QualifiedName& name, const AtomString& value)
{
    synchronizeAttribute(name);
    if (!elementData() || elementData()->findAttributeIndexByName(name) == ElementData::attributeNotFound)
        addAttributeInternal(name, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setAttributeWithoutSynchronization(const QualifiedName& name, const AtomString& value)
{
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(name) : ElementData::attributeNotFound;
    setAttributeInternal(index, name, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setSynchronizedLazyAttribute(const QualifiedName& name, const AtomString& value)
{
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(name) : ElementData::attributeNotFound;
    setAttributeInternal(index, name, value, InSynchronizationOfLazyAttribute);
}

inline void Element::setAttributeInternal(unsigned index, const QualifiedName& name, const AtomString& newValue, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    ASSERT_WITH_MESSAGE(refCount() || parentNode(), "Attribute must not be set on an element before adoptRef");

    if (newValue.isNull()) {
        if (index != ElementData::attributeNotFound)
            removeAttributeInternal(index, inSynchronizationOfLazyAttribute);
        return;
    }

    if (index == ElementData::attributeNotFound) {
        addAttributeInternal(name, newValue, inSynchronizationOfLazyAttribute);
        return;
    }

    if (inSynchronizationOfLazyAttribute) {
        ensureUniqueElementData().attributeAt(index).setValue(newValue);
        return;
    }

    const Attribute& attribute = attributeAt(index);
    QualifiedName attributeName = attribute.name();
    AtomString oldValue = attribute.value();

    willModifyAttribute(attributeName, oldValue, newValue);

    if (newValue != oldValue) {
        Style::AttributeChangeInvalidation styleInvalidation(*this, name, oldValue, newValue);
        ensureUniqueElementData().attributeAt(index).setValue(newValue);
    }

    didModifyAttribute(attributeName, oldValue, newValue);
}

static inline AtomString makeIdForStyleResolution(const AtomString& value, bool inQuirksMode)
{
    if (inQuirksMode)
        return value.convertToASCIILowercase();
    return value;
}

bool Element::isElementReflectionAttribute(const QualifiedName& name)
{
    return name == HTMLNames::aria_activedescendantAttr;
}

bool Element::isElementsArrayReflectionAttribute(const QualifiedName& name)
{
    return name == HTMLNames::aria_controlsAttr
        || name == HTMLNames::aria_describedbyAttr
        || name == HTMLNames::aria_detailsAttr
        || name == HTMLNames::aria_errormessageAttr
        || name == HTMLNames::aria_flowtoAttr
        || name == HTMLNames::aria_labelledbyAttr
        || name == HTMLNames::aria_ownsAttr;
}

void Element::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason)
{
    bool valueIsSameAsBefore = oldValue == newValue;

    if (!valueIsSameAsBefore) {
        if (name == HTMLNames::accesskeyAttr)
            document().invalidateAccessKeyCache();
        else if (name == HTMLNames::classAttr)
            classAttributeChanged(newValue);
        else if (name == HTMLNames::idAttr) {
            AtomString oldId = elementData()->idForStyleResolution();
            AtomString newId = makeIdForStyleResolution(newValue, document().inQuirksMode());
            if (newId != oldId) {
                Style::IdChangeInvalidation styleInvalidation(*this, oldId, newId);
                elementData()->setIdForStyleResolution(newId);
            }

            if (!oldValue.isEmpty())
                treeScope().idTargetObserverRegistry().notifyObservers(*oldValue.impl());
            if (!newValue.isEmpty())
                treeScope().idTargetObserverRegistry().notifyObservers(*newValue.impl());
        } else if (name == HTMLNames::nameAttr)
            elementData()->setHasNameAttribute(!newValue.isNull());
        else if (name == HTMLNames::nonceAttr) {
            if (is<HTMLElement>(*this) || is<SVGElement>(*this))
                setNonce(newValue.isNull() ? emptyAtom() : newValue);
        } else if (name == HTMLNames::pseudoAttr) {
            if (needsStyleInvalidation() && isInShadowTree())
                invalidateStyleForSubtree();
        } else if (name == HTMLNames::slotAttr) {
            if (auto* parent = parentElement()) {
                if (auto* shadowRoot = parent->shadowRoot())
                    shadowRoot->hostChildElementDidChangeSlotAttribute(*this, oldValue, newValue);
            }
        } else if (name == HTMLNames::partAttr)
            partAttributeChanged(newValue);
        else if (document().settings().ariaReflectionForElementReferencesEnabled() && (isElementReflectionAttribute(name) || isElementsArrayReflectionAttribute(name))) {
            if (auto* map = explicitlySetAttrElementsMapIfExists())
                map->remove(name);
        } else if (name == HTMLNames::exportpartsAttr) {
            if (auto* shadowRoot = this->shadowRoot()) {
                shadowRoot->invalidatePartMappings();
                Style::Invalidator::invalidateShadowParts(*shadowRoot);
            }
        } else if (name == HTMLNames::langAttr || name.matches(XMLNames::langAttr)) {
            if (document().documentElement() == this)
                document().setDocumentElementLanguage(newValue);
            else {
                Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassLang, Style::PseudoClassChangeInvalidation::AnyValue);
                AtomString newValue = langFromAttribute();
                auto setEffectiveLang = [&](Element& element) {
                    if (!newValue.isNull())
                        element.ensureElementRareData().setEffectiveLang(newValue);
                    else if (element.hasRareData())
                        element.elementRareData()->setEffectiveLang(nullAtom());
                };
                setEffectiveLang(*this);
                for (auto it = descendantsOfType<Element>(*this).begin(); it;) {
                    auto& element = *it;
                    if (auto* elementData = element.elementData()) {
                        if (elementData->findLanguageAttribute()) {
                            it.traverseNextSkippingChildren();
                            continue;
                        }
                    }
                    Style::PseudoClassChangeInvalidation styleInvalidation(element, CSSSelector::PseudoClassLang, Style::PseudoClassChangeInvalidation::AnyValue);
                    setEffectiveLang(element);
                    it.traverseNext();
                }
            }
        }
    }

    parseAttribute(name, newValue);

    document().incDOMTreeVersion();

    if (UNLIKELY(isDefinedCustomElement()))
        CustomElementReactionQueue::enqueueAttributeChangedCallbackIfNeeded(*this, name, oldValue, newValue);

    if (valueIsSameAsBefore)
        return;

    invalidateNodeListAndCollectionCachesInAncestorsForAttribute(name);

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->deferAttributeChangeIfNeeded(this, name, oldValue, newValue);
}

ExplicitlySetAttrElementsMap& Element::explicitlySetAttrElementsMap()
{
    return ensureElementRareData().explicitlySetAttrElementsMap();
}

ExplicitlySetAttrElementsMap* Element::explicitlySetAttrElementsMapIfExists() const
{
    return hasRareData() ? &elementRareData()->explicitlySetAttrElementsMap() : nullptr;
}

Element* Element::getElementAttribute(const QualifiedName& attributeName) const
{
    ASSERT(document().settings().ariaReflectionForElementReferencesEnabled());
    ASSERT(isElementReflectionAttribute(attributeName));

    if (auto* map = explicitlySetAttrElementsMapIfExists()) {
        auto it = map->find(attributeName);
        if (it != map->end()) {
            ASSERT(it->value.size() == 1);
            auto* element = it->value[0].get();
            if (element && isDescendantOrShadowDescendantOf(element->rootNode()))
                return element;
            return nullptr;
        }
    }

    auto id = getAttribute(attributeName);
    if (id.isNull())
        return nullptr;

    return treeScope().getElementById(id);
}

void Element::setElementAttribute(const QualifiedName& attributeName, Element* element)
{
    ASSERT(document().settings().ariaReflectionForElementReferencesEnabled());
    ASSERT(isElementReflectionAttribute(attributeName));

    if (!element) {
        if (auto* map = explicitlySetAttrElementsMapIfExists())
            map->remove(attributeName);
        removeAttribute(attributeName);
        return;
    }

    setAttribute(attributeName, emptyAtom());

    explicitlySetAttrElementsMap().set(attributeName, Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>> { element });
}

std::optional<Vector<RefPtr<Element>>> Element::getElementsArrayAttribute(const QualifiedName& attributeName) const
{
    ASSERT(document().settings().ariaReflectionForElementReferencesEnabled());
    ASSERT(isElementsArrayReflectionAttribute(attributeName));

    if (auto* map = explicitlySetAttrElementsMapIfExists()) {
        if (auto it = map->find(attributeName); it != map->end()) {
            return compactMap(it->value, [&](auto& element) -> std::optional<RefPtr<Element>> {
                if (element && isDescendantOrShadowDescendantOf(element->rootNode()))
                    return element.get();
                return std::nullopt;
            });
        }
    }

    auto attr = attributeName;
    if (attr == HTMLNames::aria_labelledbyAttr && !hasAttribute(HTMLNames::aria_labelledbyAttr) && hasAttribute(HTMLNames::aria_labeledbyAttr))
        attr = HTMLNames::aria_labeledbyAttr;

    if (!hasAttribute(attr))
        return std::nullopt;

    SpaceSplitString ids(getAttribute(attr), SpaceSplitString::ShouldFoldCase::No);
    Vector<RefPtr<Element>> elements;
    for (unsigned i = 0; i < ids.size(); ++i) {
        if (auto* element = treeScope().getElementById(ids[i]))
            elements.append(element);
    }
    return elements;
}

void Element::setElementsArrayAttribute(const QualifiedName& attributeName, std::optional<Vector<RefPtr<Element>>>&& elements)
{
    ASSERT(document().settings().ariaReflectionForElementReferencesEnabled());
    ASSERT(isElementsArrayReflectionAttribute(attributeName));

    if (!elements) {
        if (auto* map = explicitlySetAttrElementsMapIfExists())
            map->remove(attributeName);
        removeAttribute(attributeName);
        return;
    }

    setAttribute(attributeName, emptyAtom());

    Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>> newElements;
    newElements.reserveInitialCapacity(elements->size());
    for (auto element : elements.value()) {
        newElements.uncheckedAppend(element);
    }
    explicitlySetAttrElementsMap().set(attributeName, WTFMove(newElements));
}

void Element::classAttributeChanged(const AtomString& newClassString)
{
    // Note: We'll need ElementData, but it doesn't have to be UniqueElementData.
    if (!elementData())
        ensureUniqueElementData();

    {
        auto shouldFoldCase = document().inQuirksMode() ? SpaceSplitString::ShouldFoldCase::Yes : SpaceSplitString::ShouldFoldCase::No;
        SpaceSplitString newClassNames(newClassString, shouldFoldCase);
        Style::ClassChangeInvalidation styleInvalidation(*this, elementData()->classNames(), newClassNames);
        elementData()->setClassNames(WTFMove(newClassNames));
    }

    if (hasRareData()) {
        if (auto* classList = elementRareData()->classList())
            classList->associatedAttributeValueChanged(newClassString);
    }
}

void Element::partAttributeChanged(const AtomString& newValue)
{
    SpaceSplitString newParts(newValue, SpaceSplitString::ShouldFoldCase::No);
    if (!newParts.isEmpty() || !partNames().isEmpty())
        ensureElementRareData().setPartNames(WTFMove(newParts));

    if (hasRareData()) {
        if (auto* partList = elementRareData()->partList())
            partList->associatedAttributeValueChanged(newValue);
    }

    if (needsStyleInvalidation() && isInShadowTree())
        invalidateStyleInternal();
}

URL Element::absoluteLinkURL() const
{
    if (!isLink())
        return URL();

    AtomString linkAttribute;
    if (hasTagName(SVGNames::aTag))
        linkAttribute = getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr);
    else
        linkAttribute = getAttribute(HTMLNames::hrefAttr);

    if (linkAttribute.isEmpty())
        return URL();

    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(linkAttribute));
}

#if ENABLE(TOUCH_EVENTS)

bool Element::allowsDoubleTapGesture() const
{
    if (renderStyle() && renderStyle()->touchActions() != TouchAction::Auto)
        return false;

    Element* parent = parentElement();
    return !parent || parent->allowsDoubleTapGesture();
}

#endif

Style::Resolver& Element::styleResolver()
{
    if (auto* shadowRoot = containingShadowRoot())
        return shadowRoot->styleScope().resolver();

    return document().styleScope().resolver();
}

Style::ElementStyle Element::resolveStyle(const Style::ResolutionContext& resolutionContext)
{
    return styleResolver().styleForElement(*this, resolutionContext);
}

void invalidateForSiblingCombinators(Element* sibling)
{
    for (; sibling; sibling = sibling->nextElementSibling()) {
        if (sibling->styleIsAffectedByPreviousSibling())
            sibling->invalidateStyleInternal();
        if (sibling->descendantsAffectedByPreviousSibling()) {
            for (auto* siblingChild = sibling->firstElementChild(); siblingChild; siblingChild = siblingChild->nextElementSibling())
                siblingChild->invalidateStyleForSubtreeInternal();
        }
        if (!sibling->affectsNextSiblingElementStyle())
            return;
    }
}

static void invalidateSiblingsIfNeeded(Element& element)
{
    if (!element.affectsNextSiblingElementStyle())
        return;
    auto* parent = element.parentElement();
    if (parent && parent->styleValidity() >= Style::Validity::SubtreeInvalid)
        return;

    invalidateForSiblingCombinators(element.nextElementSibling());
}

void Element::invalidateStyle()
{
    Node::invalidateStyle(Style::Validity::ElementInvalid);
    invalidateSiblingsIfNeeded(*this);
}

void Element::invalidateStyleAndLayerComposition()
{
    Node::invalidateStyle(Style::Validity::ElementInvalid, Style::InvalidationMode::RecompositeLayer);
    invalidateSiblingsIfNeeded(*this);
}

void Element::invalidateStyleForSubtree()
{
    Node::invalidateStyle(Style::Validity::SubtreeInvalid);
    invalidateSiblingsIfNeeded(*this);
}

void Element::invalidateStyleAndRenderersForSubtree()
{
    Node::invalidateStyle(Style::Validity::SubtreeAndRenderersInvalid);
    invalidateSiblingsIfNeeded(*this);
}

void Element::invalidateStyleInternal()
{
    Node::invalidateStyle(Style::Validity::ElementInvalid);
}

void Element::invalidateStyleForSubtreeInternal()
{
    Node::invalidateStyle(Style::Validity::SubtreeInvalid);
}

void Element::invalidateForQueryContainerSizeChange()
{
    // FIXME: Ideally we would just recompute things that are actually affected by containers queries within the subtree.
    Node::invalidateStyle(Style::Validity::SubtreeInvalid);
    setNodeFlag(NodeFlag::NeedsUpdateQueryContainerDependentStyle);
}

void Element::invalidateForResumingQueryContainerResolution()
{
    markAncestorsForInvalidatedStyle();
}

bool Element::needsUpdateQueryContainerDependentStyle() const
{
    return hasNodeFlag(NodeFlag::NeedsUpdateQueryContainerDependentStyle);
}

void Element::clearNeedsUpdateQueryContainerDependentStyle()
{
    clearNodeFlag(NodeFlag::NeedsUpdateQueryContainerDependentStyle);
}

void Element::invalidateEventListenerRegions()
{
    // Event listener region is updated via style update.
    invalidateStyleInternal();
}

bool Element::hasDisplayContents() const
{
    if (!hasRareData())
        return false;

    const RenderStyle* style = elementRareData()->computedStyle();
    return style && style->display() == DisplayType::Contents;
}

void Element::storeDisplayContentsStyle(std::unique_ptr<RenderStyle> style)
{
    ASSERT(style && style->display() == DisplayType::Contents);
    ASSERT(!renderer() || isPseudoElement());
    ensureElementRareData().setComputedStyle(WTFMove(style));
    clearNodeFlag(NodeFlag::IsComputedStyleInvalidFlag);
}

// Returns true is the given attribute is an event handler.
// We consider an event handler any attribute that begins with "on".
// It is a simple solution that has the advantage of not requiring any
// code or configuration change if a new event handler is defined.

bool Element::isEventHandlerAttribute(const Attribute& attribute) const
{
    return attribute.name().namespaceURI().isNull() && attribute.name().localName().startsWith("on"_s);
}

bool Element::attributeContainsJavaScriptURL(const Attribute& attribute) const
{
    return isURLAttribute(attribute) && WTF::protocolIsJavaScript(stripLeadingAndTrailingHTMLSpaces(attribute.value()));
}

void Element::stripScriptingAttributes(Vector<Attribute>& attributeVector) const
{
    attributeVector.removeAllMatching([this](auto& attribute) -> bool {
        return this->isEventHandlerAttribute(attribute)
            || this->attributeContainsJavaScriptURL(attribute)
            || this->isHTMLContentAttribute(attribute);
    });
}

void Element::parserSetAttributes(const Vector<Attribute>& attributeVector)
{
    ASSERT(!isConnected());
    ASSERT(!parentNode());
    ASSERT(!m_elementData);

    if (!attributeVector.isEmpty()) {
        if (document().sharedObjectPool())
            m_elementData = document().sharedObjectPool()->cachedShareableElementDataWithAttributes(attributeVector);
        else
            m_elementData = ShareableElementData::createWithAttributes(attributeVector);

    }

    parserDidSetAttributes();

    // Use attributeVector instead of m_elementData because attributeChanged might modify m_elementData.
    for (const auto& attribute : attributeVector)
        attributeChanged(attribute.name(), nullAtom(), attribute.value(), ModifiedDirectly);
}

void Element::parserDidSetAttributes()
{
}

void Element::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    ASSERT_WITH_SECURITY_IMPLICATION(&document() == &newDocument);

    if (oldDocument.inQuirksMode() != document().inQuirksMode()) {
        ensureUniqueElementData();
        // ElementData::m_classNames or ElementData::m_idForStyleResolution need to be updated with the right case.
        if (hasID())
            attributeChanged(idAttr, nullAtom(), getIdAttribute());
        if (hasClass())
            attributeChanged(classAttr, nullAtom(), getAttribute(classAttr));
    }

    if (UNLIKELY(isDefinedCustomElement()))
        CustomElementReactionQueue::enqueueAdoptedCallbackIfNeeded(*this, oldDocument, newDocument);

    if (auto* observerData = intersectionObserverDataIfExists()) {
        for (const auto& observer : observerData->observers) {
            if (observer->hasObservationTargets()) {
                oldDocument.removeIntersectionObserver(*observer);
                newDocument.addIntersectionObserver(*observer);
            }
        }
    }
}

bool Element::hasAttributes() const
{
    synchronizeAllAttributes();
    return elementData() && elementData()->length();
}

bool Element::hasEquivalentAttributes(const Element& other) const
{
    synchronizeAllAttributes();
    other.synchronizeAllAttributes();
    if (elementData() == other.elementData())
        return true;
    if (elementData())
        return elementData()->isEquivalent(other.elementData());
    if (other.elementData())
        return other.elementData()->isEquivalent(elementData());
    return true;
}

String Element::nodeName() const
{
    return m_tagName.toString();
}

String Element::nodeNamePreservingCase() const
{
    return m_tagName.toString();
}

ExceptionOr<void> Element::setPrefix(const AtomString& prefix)
{
    auto result = checkSetPrefix(prefix);
    if (result.hasException())
        return result.releaseException();

    m_tagName.setPrefix(prefix.isEmpty() ? nullAtom() : prefix);
    return { };
}

const AtomString& Element::imageSourceURL() const
{
    return attributeWithoutSynchronization(srcAttr);
}

bool Element::rendererIsNeeded(const RenderStyle& style)
{
    return rendererIsEverNeeded() && style.display() != DisplayType::None && style.display() != DisplayType::Contents;
}

RenderPtr<RenderElement> Element::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return RenderElement::createFor(*this, WTFMove(style));
}

Node::InsertedIntoAncestorResult Element::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    ContainerNode::insertedIntoAncestor(insertionType, parentOfInsertedTree);

    if (parentOfInsertedTree.isInTreeScope()) {
        bool becomeConnected = insertionType.connectedToDocument;
        auto* newScope = &parentOfInsertedTree.treeScope();
        auto* newDocument = becomeConnected ? dynamicDowncast<HTMLDocument>(newScope->documentScope()) : nullptr;
        if (!insertionType.treeScopeChanged)
            newScope = nullptr;

        if (auto& idValue = getIdAttribute(); !idValue.isEmpty()) {
            if (newScope)
                newScope->addElementById(*idValue.impl(), *this);
            if (newDocument)
                updateIdForDocument(*newDocument, nullAtom(), idValue, AlwaysUpdateHTMLDocumentNamedItemMaps);
        }

        if (auto& nameValue = getNameAttribute(); !nameValue.isEmpty()) {
            if (newScope)
                newScope->addElementByName(*nameValue.impl(), *this);
            if (newDocument)
                updateNameForDocument(*newDocument, nullAtom(), nameValue);
        }

        if (becomeConnected) {
            if (UNLIKELY(isCustomElementUpgradeCandidate())) {
                ASSERT(isConnected());
                CustomElementReactionQueue::tryToUpgradeElement(*this);
            }
            if (UNLIKELY(isDefinedCustomElement()))
                CustomElementReactionQueue::enqueueConnectedCallbackIfNeeded(*this);
        }

        if (shouldAutofocus(*this))
            document().topDocument().appendAutofocusCandidate(*this);
    }

    if (parentNode() == &parentOfInsertedTree) {
        if (auto* shadowRoot = parentNode()->shadowRoot())
            shadowRoot->hostChildElementDidChange(*this);
    }

    if (auto* parent = parentOrShadowHostElement(); parent && parent != document().documentElement() && UNLIKELY(parent->hasRareData())) {
        auto& lang = parent->elementRareData()->effectiveLang();
        if (!lang.isNull() && langFromAttribute().isNull())
            ensureElementRareData().setEffectiveLang(lang);
    }

    return InsertedIntoAncestorResult::Done;
}

void Element::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (auto* page = document().page()) {
#if ENABLE(POINTER_LOCK)
        page->pointerLockController().elementWasRemoved(*this);
#endif
        page->pointerCaptureController().elementWasRemoved(*this);
#if ENABLE(WHEEL_EVENT_LATCHING)
        if (auto* scrollLatchingController = page->scrollLatchingControllerIfExists())
            scrollLatchingController->removeLatchingStateForTarget(*this);
#endif
    }

    setSavedLayerScrollPosition(ScrollPosition());

    if (oldParentOfRemovedTree.isInTreeScope()) {
        TreeScope* oldScope = &oldParentOfRemovedTree.treeScope();
        Document* oldDocument = removalType.disconnectedFromDocument ? &oldScope->documentScope() : nullptr;
        auto* oldHTMLDocument = dynamicDowncast<HTMLDocument>(oldDocument);
        if (!removalType.treeScopeChanged)
            oldScope = nullptr;

        if (auto& idValue = getIdAttribute(); !idValue.isEmpty()) {
            if (oldScope)
                oldScope->removeElementById(*idValue.impl(), *this);
            if (oldHTMLDocument)
                updateIdForDocument(*oldHTMLDocument, idValue, nullAtom(), AlwaysUpdateHTMLDocumentNamedItemMaps);
        }

        if (auto& nameValue = getNameAttribute(); !nameValue.isEmpty()) {
            if (oldScope)
                oldScope->removeElementByName(*nameValue.impl(), *this);
            if (oldHTMLDocument)
                updateNameForDocument(*oldHTMLDocument, nameValue, nullAtom());
        }

        if (oldDocument && oldDocument->cssTarget() == this)
            oldDocument->setCSSTarget(nullptr);

        if (removalType.disconnectedFromDocument && UNLIKELY(isDefinedCustomElement()))
            CustomElementReactionQueue::enqueueDisconnectedCallbackIfNeeded(*this);
    }

    if (!parentNode()) {
        if (auto* shadowRoot = oldParentOfRemovedTree.shadowRoot())
            shadowRoot->hostChildElementDidChange(*this);
    }

    clearBeforePseudoElement();
    clearAfterPseudoElement();

    ContainerNode::removedFromAncestor(removalType, oldParentOfRemovedTree);

#if ENABLE(FULLSCREEN_API)
    document().fullscreenManager().exitRemovedFullscreenElementIfNeeded(*this);
#endif

    if (UNLIKELY(hasRareData()) && !elementRareData()->effectiveLang().isNull()) {
        if (auto* parent = parentOrShadowHostElement(); langFromAttribute().isNull()
            && !(parent && UNLIKELY(parent->hasRareData()) && !parent->elementRareData()->effectiveLang().isNull()))
            elementRareData()->setEffectiveLang(nullAtom());
    }

    Styleable::fromElement(*this).elementWasRemoved();

    if (UNLIKELY(isInTopLayer()))
        removeFromTopLayer();
}

void Element::addShadowRoot(Ref<ShadowRoot>&& newShadowRoot)
{
    ASSERT(!newShadowRoot->hasChildNodes());
    ASSERT(!shadowRoot());

    ShadowRoot& shadowRoot = newShadowRoot;
    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        if (renderer() || hasDisplayContents())
            RenderTreeUpdater::tearDownRenderers(*this);

        ensureElementRareData().setShadowRoot(WTFMove(newShadowRoot));

        shadowRoot.setHost(*this);
        shadowRoot.setParentTreeScope(treeScope());

        NodeVector postInsertionNotificationTargets;
        notifyChildNodeInserted(*this, shadowRoot, postInsertionNotificationTargets);
        ASSERT_UNUSED(postInsertionNotificationTargets, postInsertionNotificationTargets.isEmpty());

        InspectorInstrumentation::didPushShadowRoot(*this, shadowRoot);

        invalidateStyleAndRenderersForSubtree();
    }

    if (shadowRoot.mode() == ShadowRootMode::UserAgent)
        didAddUserAgentShadowRoot(shadowRoot);
}

void Element::removeShadowRoot()
{
    RefPtr<ShadowRoot> oldRoot = shadowRoot();
    if (!oldRoot)
        return;

    InspectorInstrumentation::willPopShadowRoot(*this, *oldRoot);
    document().adjustFocusedNodeOnNodeRemoval(*oldRoot);

    ASSERT(!oldRoot->renderer());

    elementRareData()->clearShadowRoot();

    oldRoot->setHost(nullptr);
    oldRoot->setParentTreeScope(document());
}

static bool canAttachAuthorShadowRoot(const Element& element)
{
    using namespace ElementNames;

    if (!is<HTMLElement>(element))
        return false;

    switch (element.tagQName().elementName()) {
    case HTML::article:
    case HTML::aside:
    case HTML::blockquote:
    case HTML::body:
    case HTML::div:
    case HTML::footer:
    case HTML::h1:
    case HTML::h2:
    case HTML::h3:
    case HTML::h4:
    case HTML::h5:
    case HTML::h6:
    case HTML::header:
    case HTML::main:
    case HTML::nav:
    case HTML::p:
    case HTML::section:
    case HTML::span:
        return true;
    default:
        break;
    }

    if (auto localName = element.localName(); Document::validateCustomElementName(localName) == CustomElementNameValidationStatus::Valid) {
        if (auto* window = element.document().domWindow()) {
            auto* registry = window->customElementRegistry();
            if (registry && registry->isShadowDisabled(localName))
                return false;
        }

        return true;
    }

    return false;
}

ExceptionOr<ShadowRoot&> Element::attachShadow(const ShadowRootInit& init)
{
    if (!canAttachAuthorShadowRoot(*this))
        return Exception { NotSupportedError };
    if (RefPtr shadowRoot = this->shadowRoot()) {
        if (shadowRoot->isDeclarativeShadowRoot()) {
            ChildListMutationScope mutation(*shadowRoot);
            shadowRoot->removeChildren();
            shadowRoot->setIsDeclarativeShadowRoot(false);
            return *shadowRoot;
        }
        return Exception { NotSupportedError };
    }
    if (init.mode == ShadowRootMode::UserAgent)
        return Exception { TypeError };
    auto shadow = ShadowRoot::create(document(), init.mode, init.slotAssignment, init.delegatesFocus ? ShadowRoot::DelegatesFocus::Yes : ShadowRoot::DelegatesFocus::No, isPrecustomizedOrDefinedCustomElement() ? ShadowRoot::AvailableToElementInternals::Yes : ShadowRoot::AvailableToElementInternals::No);
    auto& result = shadow.get();
    addShadowRoot(WTFMove(shadow));
    return result;
}

ExceptionOr<ShadowRoot&> Element::attachDeclarativeShadow(ShadowRootMode mode, bool delegatesFocus)
{
    auto exceptionOrShadowRoot = attachShadow({ mode, delegatesFocus });
    if (exceptionOrShadowRoot.hasException())
        return exceptionOrShadowRoot.releaseException();
    auto& shadowRoot = exceptionOrShadowRoot.releaseReturnValue();
    shadowRoot.setIsDeclarativeShadowRoot(true);
    return shadowRoot;
}

ShadowRoot* Element::shadowRootForBindings(JSC::JSGlobalObject& lexicalGlobalObject) const
{
    auto* shadow = shadowRoot();
    if (!shadow)
        return nullptr;
    if (shadow->mode() == ShadowRootMode::Open)
        return shadow;
    if (JSC::jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject)->world().shadowRootIsAlwaysOpen())
        return shadow;
    return nullptr;
}

RefPtr<ShadowRoot> Element::userAgentShadowRoot() const
{
    ASSERT(!shadowRoot() || shadowRoot()->mode() == ShadowRootMode::UserAgent);
    return shadowRoot();
}

ShadowRoot& Element::ensureUserAgentShadowRoot()
{
    if (auto shadow = userAgentShadowRoot())
        return *shadow;
    return createUserAgentShadowRoot();
}

ShadowRoot& Element::createUserAgentShadowRoot()
{
    ASSERT(!userAgentShadowRoot());
    auto newShadow = ShadowRoot::create(document(), ShadowRootMode::UserAgent);
    ShadowRoot& shadow = newShadow;
    addShadowRoot(WTFMove(newShadow));
    return shadow;
}

inline void Node::setCustomElementState(CustomElementState state)
{
    auto bitfields = rareDataBitfields();
    bitfields.customElementState = static_cast<uint16_t>(state);
    setRareDataBitfields(bitfields);
}

void Element::setIsDefinedCustomElement(JSCustomElementInterface& elementInterface)
{
    setCustomElementState(CustomElementState::Custom);
    auto& data = ensureElementRareData();
    if (!data.customElementReactionQueue())
        data.setCustomElementReactionQueue(makeUnique<CustomElementReactionQueue>(elementInterface));
    invalidateStyleForSubtree();
    InspectorInstrumentation::didChangeCustomElementState(*this);
}

void Element::setIsFailedCustomElement()
{
    ASSERT(isUnknownElement());
    setIsFailedOrPrecustomizedCustomElementWithoutClearingReactionQueue();
    clearReactionQueueFromFailedCustomElement();
    ASSERT(isFailedCustomElement());
}

void Element::setIsFailedOrPrecustomizedCustomElementWithoutClearingReactionQueue()
{
    ASSERT(customElementState() == CustomElementState::Undefined);
    setCustomElementState(CustomElementState::FailedOrPrecustomized);
    InspectorInstrumentation::didChangeCustomElementState(*this);
}

void Element::clearReactionQueueFromFailedCustomElement()
{
    ASSERT(isFailedOrPrecustomizedCustomElement());
    if (hasRareData()) {
        // Clear the queue instead of deleting it since this function can be called inside CustomElementReactionQueue::invokeAll during upgrades.
        if (auto* queue = elementRareData()->customElementReactionQueue())
            queue->clear();
    }
}

void Element::setIsCustomElementUpgradeCandidate()
{
    ASSERT(customElementState() == CustomElementState::Uncustomized);
    setCustomElementState(CustomElementState::Undefined);
    InspectorInstrumentation::didChangeCustomElementState(*this);
}

void Element::enqueueToUpgrade(JSCustomElementInterface& elementInterface)
{
    ASSERT(isCustomElementUpgradeCandidate());
    auto& data = ensureElementRareData();
    bool alreadyScheduledToUpgrade = data.customElementReactionQueue();
    if (!alreadyScheduledToUpgrade)
        data.setCustomElementReactionQueue(makeUnique<CustomElementReactionQueue>(elementInterface));
    data.customElementReactionQueue()->enqueueElementUpgrade(*this, alreadyScheduledToUpgrade);
}

CustomElementReactionQueue* Element::reactionQueue() const
{
#if ASSERT_ENABLED
    if (isFailedOrPrecustomizedCustomElement()) {
        auto* queue = elementRareData()->customElementReactionQueue();
        ASSERT(queue);
        ASSERT(queue->isEmpty() || queue->hasJustUpgradeReaction());
    }
#endif
    if (!hasRareData())
        return nullptr;
    return elementRareData()->customElementReactionQueue();
}

CustomElementDefaultARIA& Element::customElementDefaultARIA()
{
    ASSERT(isPrecustomizedOrDefinedCustomElement());
    auto* deafultARIA = elementRareData()->customElementDefaultARIA();
    if (!deafultARIA) {
        elementRareData()->setCustomElementDefaultARIA(makeUnique<CustomElementDefaultARIA>());
        deafultARIA = elementRareData()->customElementDefaultARIA();
    }
    return *deafultARIA;
}

CustomElementDefaultARIA* Element::customElementDefaultARIAIfExists()
{
    return isPrecustomizedOrDefinedCustomElement() && hasRareData() ? elementRareData()->customElementDefaultARIA() : nullptr;
}

const AtomString& Element::shadowPseudoId() const
{
    return pseudo();
}

bool Element::childTypeAllowed(NodeType type) const
{
    switch (type) {
    case ELEMENT_NODE:
    case TEXT_NODE:
    case COMMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case CDATA_SECTION_NODE:
        return true;
    default:
        break;
    }
    return false;
}
void Element::childrenChanged(const ChildChange& change)
{
    ContainerNode::childrenChanged(change);

    if (auto* shadowRoot = this->shadowRoot()) {
        switch (change.type) {
        case ChildChange::Type::ElementInserted:
        case ChildChange::Type::ElementRemoved:
            // For elements, we notify shadowRoot in Element::insertedIntoAncestor and Element::removedFromAncestor.
            break;
        case ChildChange::Type::AllChildrenRemoved:
        case ChildChange::Type::AllChildrenReplaced:
            shadowRoot->didRemoveAllChildrenOfShadowHost();
            break;
        case ChildChange::Type::TextInserted:
        case ChildChange::Type::TextRemoved:
        case ChildChange::Type::TextChanged:
            shadowRoot->didMutateTextNodesOfShadowHost();
            break;
        case ChildChange::Type::NonContentsChildInserted:
        case ChildChange::Type::NonContentsChildRemoved:
            break;
        }
    }
}

void Element::setAttributeEventListener(const AtomString& eventType, const QualifiedName& attributeName, const AtomString& attributeValue)
{
    setAttributeEventListener(eventType, JSLazyEventListener::create(*this, attributeName, attributeValue), mainThreadNormalWorld());
}

void Element::removeAllEventListeners()
{
    ContainerNode::removeAllEventListeners();
    if (ShadowRoot* shadowRoot = this->shadowRoot())
        shadowRoot->removeAllEventListeners();
}

void Element::beginParsingChildren()
{
    clearIsParsingChildrenFinished();
}

void Element::finishParsingChildren()
{
    ContainerNode::finishParsingChildren();
    setIsParsingChildrenFinished();

    Style::ChildChangeInvalidation::invalidateAfterFinishedParsingChildren(*this);
}

static void appendAttributes(StringBuilder& builder, const Element& element)
{
    if (element.hasID())
        builder.append(" id=\'", element.getIdAttribute(), '\'');

    if (element.hasClass()) {
        builder.append(" class=\'");
        size_t classNamesToDump = element.classNames().size();
        constexpr size_t maxNumClassNames = 7;
        bool addEllipsis = false;
        if (classNamesToDump > maxNumClassNames) {
            classNamesToDump = maxNumClassNames;
            addEllipsis = true;
        }
        
        for (size_t i = 0; i < classNamesToDump; ++i) {
            if (i > 0)
                builder.append(' ');
            builder.append(element.classNames()[i]);
        }
        if (addEllipsis)
            builder.append(" ...");
        builder.append('\'');
    }
}

String Element::description() const
{
    StringBuilder builder;

    builder.append(ContainerNode::description());
    appendAttributes(builder, *this);

    return builder.toString();
}

String Element::debugDescription() const
{
    StringBuilder builder;

    builder.append(ContainerNode::debugDescription());
    appendAttributes(builder, *this);

    return builder.toString();
}

const Vector<RefPtr<Attr>>& Element::attrNodeList()
{
    ASSERT(hasSyntheticAttrChildNodes());
    return *attrNodeListForElement(*this);
}

void Element::attachAttributeNodeIfNeeded(Attr& attrNode)
{
    ASSERT(!attrNode.ownerElement() || attrNode.ownerElement() == this);
    if (attrNode.ownerElement() == this)
        return;

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    attrNode.attachToElement(*this);
    ensureAttrNodeListForElement(*this).append(&attrNode);
}

ExceptionOr<RefPtr<Attr>> Element::setAttributeNode(Attr& attrNode)
{
    RefPtr<Attr> oldAttrNode = attrIfExists(attrNode.localName(), shouldIgnoreAttributeCase(*this));
    if (oldAttrNode.get() == &attrNode)
        return oldAttrNode;

    // InUseAttributeError: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attrNode.ownerElement() && attrNode.ownerElement() != this)
        return Exception { InUseAttributeError };

    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        synchronizeAllAttributes();
    }

    auto& elementData = ensureUniqueElementData();

    auto existingAttributeIndex = elementData.findAttributeIndexByName(attrNode.localName(), shouldIgnoreAttributeCase(*this));

    // Attr::value() will return its 'm_standaloneValue' member any time its Element is set to nullptr. We need to cache this value
    // before making changes to attrNode's Element connections.
    auto attrNodeValue = attrNode.value();

    if (existingAttributeIndex == ElementData::attributeNotFound) {
        attachAttributeNodeIfNeeded(attrNode);
        setAttributeInternal(elementData.findAttributeIndexByName(attrNode.qualifiedName()), attrNode.qualifiedName(), attrNodeValue, NotInSynchronizationOfLazyAttribute);
    } else {
        const Attribute& attribute = attributeAt(existingAttributeIndex);
        if (oldAttrNode)
            detachAttrNodeFromElementWithValue(oldAttrNode.get(), attribute.value());
        else
            oldAttrNode = Attr::create(document(), attrNode.qualifiedName(), attribute.value());

        attachAttributeNodeIfNeeded(attrNode);

        if (attribute.name().matches(attrNode.qualifiedName()))
            setAttributeInternal(existingAttributeIndex, attrNode.qualifiedName(), attrNodeValue, NotInSynchronizationOfLazyAttribute);
        else {
            removeAttributeInternal(existingAttributeIndex, NotInSynchronizationOfLazyAttribute);
            setAttributeInternal(ensureUniqueElementData().findAttributeIndexByName(attrNode.qualifiedName()), attrNode.qualifiedName(), attrNodeValue, NotInSynchronizationOfLazyAttribute);
        }
    }

    return oldAttrNode;
}

ExceptionOr<RefPtr<Attr>> Element::setAttributeNodeNS(Attr& attrNode)
{
    RefPtr<Attr> oldAttrNode = attrIfExists(attrNode.qualifiedName());
    if (oldAttrNode.get() == &attrNode)
        return oldAttrNode;

    // InUseAttributeError: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attrNode.ownerElement() && attrNode.ownerElement() != this)
        return Exception { InUseAttributeError };

    // Attr::value() will return its 'm_standaloneValue' member any time its Element is set to nullptr. We need to cache this value
    // before making changes to attrNode's Element connections.
    auto attrNodeValue = attrNode.value();
    unsigned index = 0;
    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        synchronizeAllAttributes();
        auto& elementData = ensureUniqueElementData();

        index = elementData.findAttributeIndexByName(attrNode.qualifiedName());

        if (index != ElementData::attributeNotFound) {
            if (oldAttrNode)
                detachAttrNodeFromElementWithValue(oldAttrNode.get(), elementData.attributeAt(index).value());
            else
                oldAttrNode = Attr::create(document(), attrNode.qualifiedName(), elementData.attributeAt(index).value());
        }
    }

    attachAttributeNodeIfNeeded(attrNode);
    setAttributeInternal(index, attrNode.qualifiedName(), attrNodeValue, NotInSynchronizationOfLazyAttribute);

    return oldAttrNode;
}

ExceptionOr<Ref<Attr>> Element::removeAttributeNode(Attr& attr)
{
    if (attr.ownerElement() != this)
        return Exception { NotFoundError };

    ASSERT(&document() == &attr.document());

    synchronizeAllAttributes();

    if (!m_elementData)
        return Exception { NotFoundError };

    auto existingAttributeIndex = m_elementData->findAttributeIndexByName(attr.qualifiedName());
    if (existingAttributeIndex == ElementData::attributeNotFound)
        return Exception { NotFoundError };

    Ref<Attr> oldAttrNode { attr };

    detachAttrNodeFromElementWithValue(&attr, m_elementData->attributeAt(existingAttributeIndex).value());
    removeAttributeInternal(existingAttributeIndex, NotInSynchronizationOfLazyAttribute);

    return oldAttrNode;
}

ExceptionOr<QualifiedName> Element::parseAttributeName(const AtomString& namespaceURI, const AtomString& qualifiedName)
{
    auto parseResult = Document::parseQualifiedName(namespaceURI, qualifiedName);
    if (parseResult.hasException())
        return parseResult.releaseException();
    QualifiedName parsedAttributeName { parseResult.releaseReturnValue() };
    if (!Document::hasValidNamespaceForAttributes(parsedAttributeName))
        return Exception { NamespaceError };
    return parsedAttributeName;
}

ExceptionOr<void> Element::setAttributeNS(const AtomString& namespaceURI, const AtomString& qualifiedName, const AtomString& value)
{
    auto result = parseAttributeName(namespaceURI, qualifiedName);
    if (result.hasException())
        return result.releaseException();
    setAttribute(result.releaseReturnValue(), value);
    return { };
}

void Element::removeAttributeInternal(unsigned index, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < attributeCount());

    UniqueElementData& elementData = ensureUniqueElementData();

    QualifiedName name = elementData.attributeAt(index).name();
    AtomString valueBeingRemoved = elementData.attributeAt(index).value();

    if (RefPtr<Attr> attrNode = attrIfExists(name))
        detachAttrNodeFromElementWithValue(attrNode.get(), elementData.attributeAt(index).value());

    if (inSynchronizationOfLazyAttribute) {
        elementData.removeAttribute(index);
        return;
    }

    ASSERT(!valueBeingRemoved.isNull());
    willModifyAttribute(name, valueBeingRemoved, nullAtom());
    {
        Style::AttributeChangeInvalidation styleInvalidation(*this, name, valueBeingRemoved, nullAtom());
        elementData.removeAttribute(index);
    }

    didRemoveAttribute(name, valueBeingRemoved);
}

void Element::addAttributeInternal(const QualifiedName& name, const AtomString& value, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    if (inSynchronizationOfLazyAttribute) {
        ensureUniqueElementData().addAttribute(name, value);
        return;
    }

    willModifyAttribute(name, nullAtom(), value);
    {
        Style::AttributeChangeInvalidation styleInvalidation(*this, name, nullAtom(), value);
        ensureUniqueElementData().addAttribute(name, value);
    }
    didAddAttribute(name, value);
}

bool Element::removeAttribute(const AtomString& qualifiedName)
{
    if (!elementData())
        return false;

    AtomString caseAdjustedQualifiedName = shouldIgnoreAttributeCase(*this) ? qualifiedName.convertToASCIILowercase() : qualifiedName;
    unsigned index = elementData()->findAttributeIndexByName(caseAdjustedQualifiedName, false);
    if (index == ElementData::attributeNotFound) {
        if (UNLIKELY(caseAdjustedQualifiedName == styleAttr) && elementData()->styleAttributeIsDirty() && is<StyledElement>(*this))
            downcast<StyledElement>(*this).removeAllInlineStyleProperties();
        return false;
    }

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    return true;
}

bool Element::removeAttributeNS(const AtomString& namespaceURI, const AtomString& localName)
{
    return removeAttribute(QualifiedName(nullAtom(), localName, namespaceURI));
}

RefPtr<Attr> Element::getAttributeNode(const AtomString& qualifiedName)
{
    if (!elementData())
        return nullptr;
    synchronizeAttribute(qualifiedName);
    const Attribute* attribute = elementData()->findAttributeByName(qualifiedName, shouldIgnoreAttributeCase(*this));
    if (!attribute)
        return nullptr;
    return ensureAttr(attribute->name());
}

RefPtr<Attr> Element::getAttributeNodeNS(const AtomString& namespaceURI, const AtomString& localName)
{
    if (!elementData())
        return nullptr;
    QualifiedName qName(nullAtom(), localName, namespaceURI);
    synchronizeAttribute(qName);
    const Attribute* attribute = elementData()->findAttributeByName(qName);
    if (!attribute)
        return nullptr;
    return ensureAttr(attribute->name());
}

bool Element::hasAttribute(const AtomString& qualifiedName) const
{
    if (!elementData())
        return false;
    synchronizeAttribute(qualifiedName);
    return elementData()->findAttributeByName(qualifiedName, shouldIgnoreAttributeCase(*this));
}

bool Element::hasAttributeNS(const AtomString& namespaceURI, const AtomString& localName) const
{
    if (!elementData())
        return false;
    QualifiedName qName(nullAtom(), localName, namespaceURI);
    synchronizeAttribute(qName);
    return elementData()->findAttributeByName(qName);
}

static RefPtr<ShadowRoot> shadowRootWithDelegatesFocus(const Element& element)
{
    if (auto* root = element.shadowRoot()) {
        if (root->delegatesFocus())
            return root;
    }
    return nullptr;
}

static bool isProgramaticallyFocusable(Element& element)
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    if (shadowRootWithDelegatesFocus(element))
        return false;

    // If the stylesheets have already been loaded we can reliably check isFocusable.
    // If not, we continue and set the focused node on the focus controller below so that it can be updated soon after attach.
    if (element.document().haveStylesheetsLoaded()) {
        if (!element.isFocusable())
            return false;
    }
    return element.supportsFocus();
}

static RefPtr<Element> findFocusDelegateInternal(ContainerNode& target);

// https://html.spec.whatwg.org/multipage/interaction.html#autofocus-delegate
static RefPtr<Element> autoFocusDelegate(ContainerNode& target)
{
    for (auto& element : descendantsOfType<Element>(target)) {
        if (!element.hasAttributeWithoutSynchronization(HTMLNames::autofocusAttr))
            continue;
        if (auto root = shadowRootWithDelegatesFocus(element)) {
            if (auto target = findFocusDelegateInternal(*root))
                return target;
        }
        if (isProgramaticallyFocusable(element))
            return &element;
    }
    return nullptr;
}

// https://html.spec.whatwg.org/multipage/interaction.html#focus-delegate
static RefPtr<Element> findFocusDelegateInternal(ContainerNode& target)
{
    if (auto element = autoFocusDelegate(target))
        return element;
    for (auto& element : childrenOfType<Element>(target)) {
        if (isProgramaticallyFocusable(element))
            return &element;
        if (auto root = shadowRootWithDelegatesFocus(element)) {
            if (auto target = findFocusDelegateInternal(*root))
                return target;
        }
    }
    return nullptr;
}

// https://html.spec.whatwg.org/multipage/interaction.html#focus-delegate
RefPtr<Element> Element::findFocusDelegate()
{
    return findFocusDelegateInternal(*this);
}

void Element::focus(const FocusOptions& options)
{
    if (!isConnected())
        return;

    Ref document { this->document() };
    if (document->focusedElement() == this) {
        if (document->page())
            document->page()->chrome().client().elementDidRefocus(*this, options);
        return;
    }

    RefPtr<Element> newTarget = this;

    // If we don't have renderer yet, isFocusable will compute it without style update.
    // FIXME: Expand it to avoid style update in all cases.
    if (renderer() && document->haveStylesheetsLoaded())
        document->updateStyleIfNeeded();

    if (&newTarget->document() != document.ptr())
        return;

    if (auto root = shadowRootWithDelegatesFocus(*this)) {
        RefPtr currentlyFocusedElement = document->focusedElement();
        if (root->containsIncludingShadowDOM(currentlyFocusedElement.get())) {
            if (document->page())
                document->page()->chrome().client().elementDidRefocus(*currentlyFocusedElement, options);
            return;
        }

        newTarget = findFocusDelegateInternal(*root);
        if (!newTarget)
            return;
    } else if (!isProgramaticallyFocusable(*newTarget))
        return;

    if (Page* page = document->page()) {
        auto& frame = *document->frame();
        if (!frame.hasHadUserInteraction() && !frame.isMainFrame() && !document->topOrigin().isSameOriginDomain(document->securityOrigin()))
            return;

        FocusOptions optionsWithVisibility = options;
        if (options.trigger == FocusTrigger::Bindings && document->wasLastFocusByClick())
            optionsWithVisibility.visibility = FocusVisibility::Invisible;
        else if (options.trigger != FocusTrigger::Click)
            optionsWithVisibility.visibility = FocusVisibility::Visible;

        // Focus and change event handlers can cause us to lose our last ref.
        // If a focus event handler changes the focus to a different node it
        // does not make sense to continue and update appearence.
        if (!CheckedRef(page->focusController())->setFocusedElement(newTarget.get(), *document->frame(), optionsWithVisibility))
            return;
    }

    newTarget->findTargetAndUpdateFocusAppearance(options.selectionRestorationMode, options.preventScroll ? SelectionRevealMode::DoNotReveal : SelectionRevealMode::Reveal);
}

void Element::focusForBindings(FocusOptions&& options)
{
    options.trigger = FocusTrigger::Bindings;
    focus(WTFMove(options));
}

void Element::findTargetAndUpdateFocusAppearance(SelectionRestorationMode selectionMode, SelectionRevealMode revealMode)
{
#if PLATFORM(IOS_FAMILY)
    // Focusing a form element triggers animation in UIKit to scroll to the right position.
    // Calling updateFocusAppearance() would generate an unnecessary call to ScrollView::setScrollPosition(),
    // which would jump us around during this animation. See <rdar://problem/6699741>.
    if (revealMode == SelectionRevealMode::Reveal && is<HTMLFormControlElement>(*this))
        revealMode = SelectionRevealMode::RevealUpToMainFrame;
#endif

    auto target = focusAppearanceUpdateTarget();
    if (!target)
        return;

    target->updateFocusAppearance(selectionMode, revealMode);
}

// https://html.spec.whatwg.org/#focus-processing-model
RefPtr<Element> Element::focusAppearanceUpdateTarget()
{
    return this;
}

void Element::updateFocusAppearance(SelectionRestorationMode, SelectionRevealMode revealMode)
{
    if (isRootEditableElement()) {
        // Keep frame alive in this method, since setSelection() may release the last reference to |frame|.
        RefPtr<Frame> frame = document().frame();
        if (!frame)
            return;
        
        // When focusing an editable element in an iframe, don't reset the selection if it already contains a selection.
        if (this == frame->selection().selection().rootEditableElement())
            return;

        // FIXME: We should restore the previous selection if there is one.
        VisibleSelection newSelection = VisibleSelection(firstPositionInOrBeforeNode(this));
        
        if (frame->selection().shouldChangeSelection(newSelection)) {
            frame->selection().setSelection(newSelection, FrameSelection::defaultSetSelectionOptions(), Element::defaultFocusTextStateChangeIntent());
            frame->selection().revealSelection(revealMode);
            return;
        }
    }

    if (RefPtr<FrameView> view = document().view())
        view->scheduleScrollToFocusedElement(revealMode);
}

void Element::blur()
{
    if (treeScope().focusedElementInScope() == this) {
        if (RefPtr frame = document().frame())
            CheckedRef(frame->page()->focusController())->setFocusedElement(nullptr, *frame);
        else
            document().setFocusedElement(nullptr);
    }
}

void Element::runFocusingStepsForAutofocus()
{
    focus();
}

void Element::dispatchFocusInEventIfNeeded(RefPtr<Element>&& oldFocusedElement)
{
    if (!document().hasListenerType(Document::FOCUSIN_LISTENER))
        return;
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed() || !isInWebProcess());
    dispatchScopedEvent(FocusEvent::create(eventNames().focusinEvent, Event::CanBubble::Yes, Event::IsCancelable::No, document().windowProxy(), 0, WTFMove(oldFocusedElement)));
}

void Element::dispatchFocusOutEventIfNeeded(RefPtr<Element>&& newFocusedElement)
{
    if (!document().hasListenerType(Document::FOCUSOUT_LISTENER))
        return;
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed() || !isInWebProcess());
    dispatchScopedEvent(FocusEvent::create(eventNames().focusoutEvent, Event::CanBubble::Yes, Event::IsCancelable::No, document().windowProxy(), 0, WTFMove(newFocusedElement)));
}

void Element::dispatchFocusEvent(RefPtr<Element>&& oldFocusedElement, const FocusOptions& options)
{
    if (auto* page = document().page())
        page->chrome().client().elementDidFocus(*this, options);
    dispatchEvent(FocusEvent::create(eventNames().focusEvent, Event::CanBubble::No, Event::IsCancelable::No, document().windowProxy(), 0, WTFMove(oldFocusedElement)));
}

void Element::dispatchBlurEvent(RefPtr<Element>&& newFocusedElement)
{
    if (auto* page = document().page())
        page->chrome().client().elementDidBlur(*this);
    dispatchEvent(FocusEvent::create(eventNames().blurEvent, Event::CanBubble::No, Event::IsCancelable::No, document().windowProxy(), 0, WTFMove(newFocusedElement)));
}

void Element::dispatchWebKitImageReadyEventForTesting()
{
    if (document().settings().webkitImageReadyEventEnabled())
        dispatchEvent(Event::create("webkitImageFrameReady"_s, Event::CanBubble::Yes, Event::IsCancelable::Yes));
}

bool Element::dispatchMouseForceWillBegin()
{
#if ENABLE(MOUSE_FORCE_EVENTS)
    if (!document().hasListenerType(Document::FORCEWILLBEGIN_LISTENER))
        return false;

    Frame* frame = document().frame();
    if (!frame)
        return false;

    PlatformMouseEvent platformMouseEvent { frame->eventHandler().lastKnownMousePosition(), frame->eventHandler().lastKnownMouseGlobalPosition(), NoButton, PlatformEvent::NoType, 1, { }, WallTime::now(), ForceAtClick, NoTap };
    auto mouseForceWillBeginEvent = MouseEvent::create(eventNames().webkitmouseforcewillbeginEvent, document().windowProxy(), platformMouseEvent, 0, nullptr);
    mouseForceWillBeginEvent->setTarget(Ref { *this });
    dispatchEvent(mouseForceWillBeginEvent);

    if (mouseForceWillBeginEvent->defaultHandled() || mouseForceWillBeginEvent->defaultPrevented())
        return true;
#endif

    return false;
}

void Element::enqueueSecurityPolicyViolationEvent(SecurityPolicyViolationEventInit&& eventInit)
{
    document().eventLoop().queueTask(TaskSource::DOMManipulation, [this, protectedThis = Ref { *this }, event = SecurityPolicyViolationEvent::create(eventNames().securitypolicyviolationEvent, WTFMove(eventInit), Event::IsTrusted::Yes)] {
        if (!isConnected())
            document().dispatchEvent(event);
        else
            dispatchEvent(event);
    });
}

ExceptionOr<void> Element::mergeWithNextTextNode(Text& node)
{
    auto* next = node.nextSibling();
    if (!is<Text>(next))
        return { };
    Ref<Text> textNext { downcast<Text>(*next) };
    node.appendData(textNext->data());
    return textNext->remove();
}

String Element::innerHTML() const
{
    return serializeFragment(*this, SerializedNodes::SubtreesOfChildren, nullptr, ResolveURLs::NoExcludingURLsForPrivacy);
}

String Element::outerHTML() const
{
    return serializeFragment(*this, SerializedNodes::SubtreeIncludingNode, nullptr, ResolveURLs::NoExcludingURLsForPrivacy);
}

ExceptionOr<void> Element::setOuterHTML(const String& html)
{
    // The specification allows setting outerHTML on an Element whose parent is a DocumentFragment and Gecko supports this.
    // However, as of June 2021, Blink matches our behavior and throws a NoModificationAllowedError for non-Element parents.
    RefPtr parent = parentElement();
    if (UNLIKELY(!parent)) {
        if (!parentNode())
            return Exception { NoModificationAllowedError, "Cannot set outerHTML on element because it doesn't have a parent"_s };
        return Exception { NoModificationAllowedError, "Cannot set outerHTML on element because its parent is not an Element"_s };
    }

    RefPtr<Node> prev = previousSibling();
    RefPtr<Node> next = nextSibling();

    auto fragment = createFragmentForInnerOuterHTML(*parent, html, { ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent });
    if (fragment.hasException())
        return fragment.releaseException();

    auto replaceResult = parent->replaceChild(fragment.releaseReturnValue().get(), *this);
    if (replaceResult.hasException())
        return replaceResult.releaseException();

    // The following is not part of the specification but matches Blink's behavior as of June 2021.
    RefPtr<Node> node = next ? next->previousSibling() : nullptr;
    if (is<Text>(node)) {
        auto result = mergeWithNextTextNode(downcast<Text>(*node));
        if (result.hasException())
            return result.releaseException();
    }
    if (is<Text>(prev)) {
        auto result = mergeWithNextTextNode(downcast<Text>(*prev));
        if (result.hasException())
            return result.releaseException();
    }
    return { };
}

ExceptionOr<void> Element::setInnerHTML(const String& html)
{
    ContainerNode* container;
    if (!is<HTMLTemplateElement>(*this))
        container = this;
    else
        container = &downcast<HTMLTemplateElement>(*this).content();

    // Parsing empty string creates additional elements only inside <html> container
    // https://html.spec.whatwg.org/multipage/parsing.html#parsing-main-inhtml
    if (html.isEmpty() && !is<HTMLHtmlElement>(*container)) {
        ChildListMutationScope mutation(*container);
        container->removeChildren();
        return { };
    }

    auto fragment = createFragmentForInnerOuterHTML(*this, html, { ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent });
    if (fragment.hasException())
        return fragment.releaseException();

    return replaceChildrenWithFragment(*container, fragment.releaseReturnValue());
}

String Element::innerText()
{
    // We need to update layout, since plainText uses line boxes in the render tree.
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return textContent(true);

    return plainText(makeRangeSelectingNodeContents(*this));
}

String Element::outerText()
{
    // Getting outerText is the same as getting innerText, only
    // setting is different. You would think this should get the plain
    // text for the outer range, but this is wrong, <br> for instance
    // would return different values for inner and outer text by such
    // a rule, but it doesn't in WinIE, and we want to match that.
    return innerText();
}

String Element::title() const
{
    return String();
}

const AtomString& Element::pseudo() const
{
    return attributeWithoutSynchronization(pseudoAttr);
}

void Element::setPseudo(const AtomString& value)
{
    setAttributeWithoutSynchronization(pseudoAttr, value);
}

void Element::willBecomeFullscreenElement()
{
    for (auto& child : descendantsOfType<Element>(*this))
        child.ancestorWillEnterFullscreen();
}

static void forEachRenderLayer(Element& element, const std::function<void(RenderLayer&)>& function)
{
    auto* renderer = element.renderer();
    if (!renderer || !is<RenderLayerModelObject>(renderer))
        return;
        
    auto& layerModelObject = downcast<RenderLayerModelObject>(*renderer);

    if (!is<RenderBoxModelObject>(layerModelObject)) {
        if (layerModelObject.hasLayer())
            function(*layerModelObject.layer());
        return;
    }

    RenderBoxModelObject::forRendererAndContinuations(downcast<RenderBoxModelObject>(*renderer), [function](RenderBoxModelObject& renderer) {
        if (renderer.hasLayer())
            function(*renderer.layer());
    });
}

void Element::addToTopLayer()
{
    RELEASE_ASSERT(!isInTopLayer());
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    forEachRenderLayer(*this, [](RenderLayer& layer) {
        layer.establishesTopLayerWillChange();
    });

    document().addTopLayerElement(*this);
    setNodeFlag(NodeFlag::IsInTopLayer);

    // Invalidate inert state
    invalidateStyleInternal();
    if (document().documentElement())
        document().documentElement()->invalidateStyleInternal();

    forEachRenderLayer(*this, [](RenderLayer& layer) {
        layer.establishesTopLayerDidChange();
    });
}

void Element::removeFromTopLayer()
{
    RELEASE_ASSERT(isInTopLayer());
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    forEachRenderLayer(*this, [](RenderLayer& layer) {
        layer.establishesTopLayerWillChange();
    });

    // We need to call Styleable::fromRenderer() while this element is still contained in
    // Document::topLayerElements(), since Styleable::fromRenderer() relies on this to
    // find the backdrop's associated element.
    if (auto* renderer = this->renderer()) {
        if (auto backdrop = renderer->backdropRenderer()) {
            if (auto styleable = Styleable::fromRenderer(*backdrop))
                styleable->cancelDeclarativeAnimations();
        }
    }

    document().removeTopLayerElement(*this);
    clearNodeFlag(NodeFlag::IsInTopLayer);

    // Invalidate inert state
    invalidateStyleInternal();
    if (document().documentElement())
        document().documentElement()->invalidateStyleInternal();
    if (auto* modalElement = document().activeModalDialog())
        modalElement->invalidateStyleInternal();

    forEachRenderLayer(*this, [](RenderLayer& layer) {
        layer.establishesTopLayerDidChange();
    });
}

static PseudoElement* beforeOrAfterPseudoElement(const Element& host, PseudoId pseudoElementSpecifier)
{
    switch (pseudoElementSpecifier) {
    case PseudoId::Before:
        return host.beforePseudoElement();
    case PseudoId::After:
        return host.afterPseudoElement();
    default:
        return nullptr;
    }
}

const RenderStyle* Element::existingComputedStyle() const
{
    if (hasRareData()) {
        if (auto* style = elementRareData()->computedStyle())
            return style;
    }

    return renderStyle();
}

const RenderStyle* Element::renderOrDisplayContentsStyle(PseudoId pseudoId) const
{
    if (pseudoId != PseudoId::None) {
        if (auto* pseudoElement = beforeOrAfterPseudoElement(*this, pseudoId))
            return pseudoElement->renderOrDisplayContentsStyle();

        if (auto* computedStyle = existingComputedStyle()) {
            if (auto* cachedPseudoStyle = computedStyle->getCachedPseudoStyle(pseudoId))
                return cachedPseudoStyle;
        }

        return nullptr;
    }

    if (auto* style = renderStyle())
        return style;

    if (!hasRareData())
        return nullptr;
    auto* style = elementRareData()->computedStyle();
    if (style && style->display() == DisplayType::Contents)
        return style;

    return nullptr;
}

const RenderStyle* Element::resolveComputedStyle(ResolveComputedStyleMode mode)
{
    ASSERT(isConnected());

    bool isInDisplayNoneTree = false;

    // Traverse the ancestor chain to find the rootmost element that has invalid computed style.
    auto* rootmostInvalidElement = [&]() -> const Element* {
        // In ResolveComputedStyleMode::RenderedOnly case we check for display:none ancestors.
        if (mode == ResolveComputedStyleMode::Normal && !document().hasPendingStyleRecalc() && existingComputedStyle())
            return nullptr;

        if (document().hasPendingFullStyleRebuild())
            return document().documentElement();

        if (!document().documentElement() || document().documentElement()->hasNodeFlag(NodeFlag::IsComputedStyleInvalidFlag))
            return document().documentElement();

        const Element* rootmost = nullptr;

        for (auto* element = this; element; element = element->parentElementInComposedTree()) {
            if (element->hasNodeFlag(NodeFlag::IsComputedStyleInvalidFlag)) {
                rootmost = element;
                continue;
            }
            auto* existing = element->existingComputedStyle();
            if (!existing) {
                rootmost = element;
                continue;
            }
            if (mode == ResolveComputedStyleMode::RenderedOnly && existing->display() == DisplayType::None) {
                isInDisplayNoneTree = true;
                return nullptr;
            }
        }
        return rootmost;
    }();

    if (isInDisplayNoneTree)
        return nullptr;

    if (!rootmostInvalidElement)
        return existingComputedStyle();

    auto* ancestorWithValidStyle = rootmostInvalidElement->parentElementInComposedTree();

    Vector<RefPtr<Element>, 32> elementsRequiringComputedStyle;
    for (auto* toResolve = this; toResolve != ancestorWithValidStyle; toResolve = toResolve->parentElementInComposedTree())
        elementsRequiringComputedStyle.append(toResolve);

    auto* computedStyle = ancestorWithValidStyle ? ancestorWithValidStyle->existingComputedStyle() : nullptr;

    // On iOS request delegates called during styleForElement may result in re-entering WebKit and killing the style resolver.
    Style::PostResolutionCallbackDisabler disabler(document(), Style::PostResolutionCallbackDisabler::DrainCallbacks::No);

    // Resolve and cache styles starting from the most distant ancestor.
    // FIXME: This is not as efficient as it could be. For example if an ancestor has a non-inherited style change but
    // the styles are otherwise clean we would not need to re-resolve descendants.
    for (auto& element : makeReversedRange(elementsRequiringComputedStyle)) {
        bool hadDisplayContents = element->hasDisplayContents();
        auto style = document().styleForElementIgnoringPendingStylesheets(*element, computedStyle);
        computedStyle = style.get();
        ElementRareData& rareData = element->ensureElementRareData();
        if (auto* existing = rareData.computedStyle()) {
            auto change = Style::determineChange(*existing, *style);
            if (change > Style::Change::NonInherited) {
                for (auto& child : composedTreeChildren(*element)) {
                    if (!is<Element>(child))
                        continue;
                    downcast<Element>(child).setNodeFlag(NodeFlag::IsComputedStyleInvalidFlag);
                }
            }
        }
        rareData.setComputedStyle(WTFMove(style));
        element->clearNodeFlag(NodeFlag::IsComputedStyleInvalidFlag);
        if (hadDisplayContents && computedStyle->display() != DisplayType::Contents)
            setDisplayContentsChanged();

        if (mode == ResolveComputedStyleMode::RenderedOnly && computedStyle->display() == DisplayType::None)
            return nullptr;
    }

    return computedStyle;
}

bool Element::isFocusableWithoutResolvingFullStyle() const
{
    auto isFocusableStyle = [](const RenderStyle* style) {
        return style
            && style->display() != DisplayType::None
            && style->display() != DisplayType::Contents
            && style->visibility() == Visibility::Visible
            && !style->effectiveInert();
    };

    if (renderStyle())
        return isFocusableStyle(renderStyle());

    // Compute style in yet unstyled subtree.
    auto* style = const_cast<Element&>(*this).resolveComputedStyle(ResolveComputedStyleMode::RenderedOnly);
    return isFocusableStyle(style);
}

const RenderStyle& Element::resolvePseudoElementStyle(PseudoId pseudoElementSpecifier)
{
    ASSERT(!isPseudoElement());

    auto* parentStyle = existingComputedStyle();
    ASSERT(parentStyle);
    ASSERT(!parentStyle->getCachedPseudoStyle(pseudoElementSpecifier));

    Style::PostResolutionCallbackDisabler disabler(document(), Style::PostResolutionCallbackDisabler::DrainCallbacks::No);

    auto style = document().styleForElementIgnoringPendingStylesheets(*this, parentStyle, pseudoElementSpecifier);
    if (!style) {
        style = RenderStyle::createPtr();
        style->inheritFrom(*parentStyle);
        style->setStyleType(pseudoElementSpecifier);
    }

    auto* computedStyle = style.get();
    const_cast<RenderStyle*>(parentStyle)->addCachedPseudoStyle(WTFMove(style));
    return *computedStyle;
}

const RenderStyle* Element::computedStyle(PseudoId pseudoElementSpecifier)
{
    if (!isConnected())
        return nullptr;

    if (PseudoElement* pseudoElement = beforeOrAfterPseudoElement(*this, pseudoElementSpecifier))
        return pseudoElement->computedStyle();

    // FIXME: This should call resolveComputedStyle() unconditionally so we check if the style is valid.
    auto* style = existingComputedStyle();
    if (!style)
        style = resolveComputedStyle();

    if (pseudoElementSpecifier != PseudoId::None) {
        if (auto* cachedPseudoStyle = style->getCachedPseudoStyle(pseudoElementSpecifier))
            return cachedPseudoStyle;
        return &resolvePseudoElementStyle(pseudoElementSpecifier);
    }

    return style;
}

// FIXME: The caller should be able to just use computedStyle().
const RenderStyle* Element::computedStyleForEditability()
{
    if (!isConnected())
        return nullptr;

    return resolveComputedStyle();
}

bool Element::needsStyleInvalidation() const
{
    if (!inRenderedDocument())
        return false;
    // If :has() is present a change in an element may affect elements outside its subtree.
    if (styleValidity() >= Style::Validity::SubtreeInvalid && !Style::Scope::forNode(*this).usesHasPseudoClass())
        return false;
    if (document().documentElement() && document().documentElement()->styleValidity() >= Style::Validity::SubtreeInvalid)
        return false;
    if (document().hasPendingFullStyleRebuild())
        return false;

    return true;
}

void Element::setChildIndex(unsigned index)
{
    ElementRareData& rareData = ensureElementRareData();
    rareData.setChildIndex(index);
}

bool Element::hasFlagsSetDuringStylingOfChildren() const
{
    return childrenAffectedByFirstChildRules()
        || childrenAffectedByLastChildRules()
        || childrenAffectedByForwardPositionalRules()
        || descendantsAffectedByForwardPositionalRules()
        || childrenAffectedByBackwardPositionalRules()
        || descendantsAffectedByBackwardPositionalRules()
        || childrenAffectedByPropertyBasedBackwardPositionalRules();
}

unsigned Element::rareDataChildIndex() const
{
    ASSERT(hasRareData());
    return elementRareData()->childIndex();
}

AtomString Element::effectiveLang() const
{
    if (hasRareData()) {
        auto lang = elementRareData()->effectiveLang();
        if (!lang.isNull())
            return lang;
    }
    return isConnected() ? document().effectiveDocumentElementLanguage() : nullAtom();
}

AtomString Element::langFromAttribute() const
{
    if (auto* data = elementData()) {
        if (auto* attribute = data->findLanguageAttribute())
            return attribute->value();
    }
    return nullAtom();
}

Locale& Element::locale() const
{
    return document().getCachedLocale(effectiveLang());
}

void Element::normalizeAttributes()
{
    if (!hasAttributes())
        return;

    auto* attrNodeList = attrNodeListForElement(*this);
    if (!attrNodeList)
        return;

    // Copy the Attr Vector because Node::normalize() can fire synchronous JS
    // events (e.g. DOMSubtreeModified) and a JS listener could add / remove
    // attributes while we are iterating.
    auto copyOfAttrNodeList = *attrNodeList;
    for (auto& attrNode : copyOfAttrNodeList)
        attrNode->normalize();
}

PseudoElement& Element::ensurePseudoElement(PseudoId pseudoId)
{
    if (pseudoId == PseudoId::Before) {
        if (!beforePseudoElement())
            ensureElementRareData().setBeforePseudoElement(PseudoElement::create(*this, pseudoId));
        return *beforePseudoElement();
    }

    ASSERT(pseudoId == PseudoId::After);
    if (!afterPseudoElement())
        ensureElementRareData().setAfterPseudoElement(PseudoElement::create(*this, pseudoId));
    return *afterPseudoElement();
}

PseudoElement* Element::beforePseudoElement() const
{
    return hasRareData() ? elementRareData()->beforePseudoElement() : nullptr;
}

PseudoElement* Element::afterPseudoElement() const
{
    return hasRareData() ? elementRareData()->afterPseudoElement() : nullptr;
}

static void disconnectPseudoElement(PseudoElement* pseudoElement)
{
    if (!pseudoElement)
        return;
    ASSERT(!pseudoElement->renderer());
    ASSERT(pseudoElement->hostElement());
    pseudoElement->clearHostElement();
}

void Element::clearBeforePseudoElementSlow()
{
    ASSERT(hasRareData());
    disconnectPseudoElement(elementRareData()->beforePseudoElement());
    elementRareData()->setBeforePseudoElement(nullptr);
}

void Element::clearAfterPseudoElementSlow()
{
    ASSERT(hasRareData());
    disconnectPseudoElement(elementRareData()->afterPseudoElement());
    elementRareData()->setAfterPseudoElement(nullptr);
}

bool Element::matchesValidPseudoClass() const
{
    return false;
}

bool Element::matchesInvalidPseudoClass() const
{
    return false;
}

bool Element::matchesReadWritePseudoClass() const
{
    return false;
}

bool Element::matchesIndeterminatePseudoClass() const
{
    return shouldAppearIndeterminate();
}

bool Element::matchesDefaultPseudoClass() const
{
    return false;
}

ExceptionOr<bool> Element::matches(const String& selector)
{
    auto query = document().selectorQueryForString(selector);
    if (query.hasException())
        return query.releaseException();
    return query.releaseReturnValue().matches(*this);
}

ExceptionOr<Element*> Element::closest(const String& selector)
{
    auto query = document().selectorQueryForString(selector);
    if (query.hasException())
        return query.releaseException();
    return query.releaseReturnValue().closest(*this);
}

bool Element::shouldAppearIndeterminate() const
{
    return false;
}

bool Element::mayCauseRepaintInsideViewport(const IntRect* visibleRect) const
{
    return renderer() && renderer()->mayCauseRepaintInsideViewport(visibleRect);
}

DOMTokenList& Element::classList()
{
    ElementRareData& data = ensureElementRareData();
    if (!data.classList())
        data.setClassList(makeUnique<DOMTokenList>(*this, HTMLNames::classAttr));
    return *data.classList();
}

SpaceSplitString Element::partNames() const
{
    return hasRareData() ? elementRareData()->partNames() : SpaceSplitString();
}

DOMTokenList& Element::part()
{
    auto& data = ensureElementRareData();
    if (!data.partList())
        data.setPartList(makeUnique<DOMTokenList>(*this, HTMLNames::partAttr));
    return *data.partList();
}

DatasetDOMStringMap& Element::dataset()
{
    ElementRareData& data = ensureElementRareData();
    if (!data.dataset())
        data.setDataset(makeUnique<DatasetDOMStringMap>(*this));
    return *data.dataset();
}

URL Element::getURLAttribute(const QualifiedName& name) const
{
#if ASSERT_ENABLED
    if (elementData()) {
        if (const Attribute* attribute = findAttributeByName(name))
            ASSERT(isURLAttribute(*attribute));
    }
#endif
    return document().completeURL(getAttribute(name));
}

URL Element::getNonEmptyURLAttribute(const QualifiedName& name) const
{
#if ASSERT_ENABLED
    if (elementData()) {
        if (const Attribute* attribute = findAttributeByName(name))
            ASSERT(isURLAttribute(*attribute));
    }
#endif
    String value = stripLeadingAndTrailingHTMLSpaces(getAttribute(name));
    if (value.isEmpty())
        return URL();
    return document().completeURL(value);
}

int Element::getIntegralAttribute(const QualifiedName& attributeName) const
{
    return parseHTMLInteger(getAttribute(attributeName)).value_or(0);
}

void Element::setIntegralAttribute(const QualifiedName& attributeName, int value)
{
    setAttribute(attributeName, AtomString::number(value));
}

unsigned Element::getUnsignedIntegralAttribute(const QualifiedName& attributeName) const
{
    return parseHTMLNonNegativeInteger(getAttribute(attributeName)).value_or(0);
}

void Element::setUnsignedIntegralAttribute(const QualifiedName& attributeName, unsigned value)
{
    setAttribute(attributeName, AtomString::number(limitToOnlyHTMLNonNegative(value)));
}

bool Element::childShouldCreateRenderer(const Node& child) const
{
    // Only create renderers for SVG elements whose parents are SVG elements, or for proper <svg xmlns="svgNS"> subdocuments.
    if (child.isSVGElement()) {
        ASSERT(!isSVGElement());
        const SVGElement& childElement = downcast<SVGElement>(child);
        return is<SVGSVGElement>(childElement) && childElement.isValid();
    }
    return true;
}

#if ENABLE(FULLSCREEN_API)

void Element::webkitRequestFullscreen()
{
    requestFullscreen({ }, nullptr);
}

// FIXME: Options are currently ignored.
void Element::requestFullscreen(FullscreenOptions&&, RefPtr<DeferredPromise>&& promise)
{
    document().fullscreenManager().requestFullscreenForElement(*this, WTFMove(promise), FullscreenManager::EnforceIFrameAllowFullscreenRequirement);
}

void Element::setFullscreenFlag(bool flag)
{
    Style::PseudoClassChangeInvalidation styleInvalidation(*this, { { CSSSelector::PseudoClassFullscreen, flag }, { CSSSelector::PseudoClassModal, flag } });
    if (flag)
        setNodeFlag(NodeFlag::IsFullscreen);
    else
        clearNodeFlag(NodeFlag::IsFullscreen);

    clearNodeFlag(NodeFlag::IsIFrameFullscreen);
}

void Element::setIFrameFullscreenFlag(bool flag)
{
    if (flag)
        setNodeFlag(NodeFlag::IsIFrameFullscreen);
    else
        clearNodeFlag(NodeFlag::IsIFrameFullscreen);
}

#endif

ExceptionOr<void> Element::setPointerCapture(int32_t pointerId)
{
    if (document().page())
        return document().page()->pointerCaptureController().setPointerCapture(this, pointerId);
    return { };
}

ExceptionOr<void> Element::releasePointerCapture(int32_t pointerId)
{
    if (document().page())
        return document().page()->pointerCaptureController().releasePointerCapture(this, pointerId);
    return { };
}

bool Element::hasPointerCapture(int32_t pointerId)
{
    if (document().page())
        return document().page()->pointerCaptureController().hasPointerCapture(this, pointerId);
    return false;
}

#if ENABLE(POINTER_LOCK)

void Element::requestPointerLock()
{
    if (document().page())
        document().page()->pointerLockController().requestPointerLock(this);
}

#endif

void Element::disconnectFromIntersectionObservers()
{
    auto* observerData = intersectionObserverDataIfExists();
    if (!observerData)
        return;

    for (const auto& registration : observerData->registrations) {
        if (registration.observer)
            registration.observer->targetDestroyed(*this);
    }
    observerData->registrations.clear();

    for (const auto& observer : observerData->observers) {
        if (observer)
            observer->rootDestroyed();
    }
    observerData->observers.clear();
}

IntersectionObserverData& Element::ensureIntersectionObserverData()
{
    auto& rareData = ensureElementRareData();
    if (!rareData.intersectionObserverData())
        rareData.setIntersectionObserverData(makeUnique<IntersectionObserverData>());
    return *rareData.intersectionObserverData();
}

IntersectionObserverData* Element::intersectionObserverDataIfExists()
{
    return hasRareData() ? elementRareData()->intersectionObserverData() : nullptr;
}

ElementAnimationRareData* Element::animationRareData(PseudoId pseudoId) const
{
    return hasRareData() ? elementRareData()->animationRareData(pseudoId) : nullptr;
}

ElementAnimationRareData& Element::ensureAnimationRareData(PseudoId pseudoId)
{
    return ensureElementRareData().ensureAnimationRareData(pseudoId);
}

KeyframeEffectStack* Element::keyframeEffectStack(PseudoId pseudoId) const
{
    if (auto* animationData = animationRareData(pseudoId))
        return animationData->keyframeEffectStack();
    return nullptr;
}

KeyframeEffectStack& Element::ensureKeyframeEffectStack(PseudoId pseudoId)
{
    return ensureAnimationRareData(pseudoId).ensureKeyframeEffectStack();
}

bool Element::hasKeyframeEffects(PseudoId pseudoId) const
{
    if (auto* animationData = animationRareData(pseudoId)) {
        if (auto* keyframeEffectStack = animationData->keyframeEffectStack())
            return keyframeEffectStack->hasEffects();
    }
    return false;
}

const AnimationCollection* Element::animations(PseudoId pseudoId) const
{
    if (auto* animationData = animationRareData(pseudoId))
        return &animationData->animations();
    return nullptr;
}

bool Element::hasCompletedTransitionForProperty(PseudoId pseudoId, CSSPropertyID property) const
{
    if (auto* animationData = animationRareData(pseudoId))
        return animationData->completedTransitionsByProperty().contains(property);
    return false;
}

bool Element::hasRunningTransitionForProperty(PseudoId pseudoId, CSSPropertyID property) const
{
    if (auto* animationData = animationRareData(pseudoId))
        return animationData->runningTransitionsByProperty().contains(property);
    return false;
}

bool Element::hasRunningTransitions(PseudoId pseudoId) const
{
    if (auto* animationData = animationRareData(pseudoId))
        return !animationData->runningTransitionsByProperty().isEmpty();
    return false;
}

AnimationCollection& Element::ensureAnimations(PseudoId pseudoId)
{
    return ensureAnimationRareData(pseudoId).animations();
}

CSSAnimationCollection& Element::animationsCreatedByMarkup(PseudoId pseudoId)
{
    return ensureAnimationRareData(pseudoId).animationsCreatedByMarkup();
}

void Element::setAnimationsCreatedByMarkup(PseudoId pseudoId, CSSAnimationCollection&& animations)
{
    if (animations.isEmpty() && !animationRareData(pseudoId))
        return;
    ensureAnimationRareData(pseudoId).setAnimationsCreatedByMarkup(WTFMove(animations));
}

PropertyToTransitionMap& Element::ensureCompletedTransitionsByProperty(PseudoId pseudoId)
{
    return ensureAnimationRareData(pseudoId).completedTransitionsByProperty();
}

PropertyToTransitionMap& Element::ensureRunningTransitionsByProperty(PseudoId pseudoId)
{
    return ensureAnimationRareData(pseudoId).runningTransitionsByProperty();
}

const RenderStyle* Element::lastStyleChangeEventStyle(PseudoId pseudoId) const
{
    if (auto* animationData = animationRareData(pseudoId))
        return animationData->lastStyleChangeEventStyle();
    return nullptr;
}

void Element::setLastStyleChangeEventStyle(PseudoId pseudoId, std::unique_ptr<const RenderStyle>&& style)
{
    if (auto* animationData = animationRareData(pseudoId))
        animationData->setLastStyleChangeEventStyle(WTFMove(style));
    else if (style)
        ensureAnimationRareData(pseudoId).setLastStyleChangeEventStyle(WTFMove(style));
}

void Element::cssAnimationsDidUpdate(PseudoId pseudoId)
{
    ensureAnimationRareData(pseudoId).cssAnimationsDidUpdate();
}

void Element::keyframesRuleDidChange(PseudoId pseudoId)
{
    ensureAnimationRareData(pseudoId).keyframesRuleDidChange();
}

bool Element::hasPendingKeyframesUpdate(PseudoId pseudoId) const
{
    auto* data = animationRareData(pseudoId);
    return data && data->hasPendingKeyframesUpdate();
}

void Element::disconnectFromResizeObservers()
{
    auto* observerData = resizeObserverData();
    if (!observerData)
        return;

    for (const auto& observer : observerData->observers)
        observer->targetDestroyed(*this);
    observerData->observers.clear();
}

ResizeObserverData& Element::ensureResizeObserverData()
{
    auto& rareData = ensureElementRareData();
    if (!rareData.resizeObserverData())
        rareData.setResizeObserverData(makeUnique<ResizeObserverData>());
    return *rareData.resizeObserverData();
}

ResizeObserverData* Element::resizeObserverData()
{
    return hasRareData() ? elementRareData()->resizeObserverData() : nullptr;
}

bool Element::isSpellCheckingEnabled() const
{
    for (auto* ancestor = this; ancestor; ancestor = ancestor->parentOrShadowHostElement()) {
        auto& value = ancestor->attributeWithoutSynchronization(HTMLNames::spellcheckAttr);
        if (value.isNull())
            continue;
        if (value.isEmpty() || equalLettersIgnoringASCIICase(value, "true"_s))
            return true;
        if (equalLettersIgnoringASCIICase(value, "false"_s))
            return false;
    }
    return true;
}

#if ASSERT_ENABLED
bool Element::fastAttributeLookupAllowed(const QualifiedName& name) const
{
    if (name == HTMLNames::styleAttr)
        return false;

    if (isSVGElement())
        return !downcast<SVGElement>(*this).isAnimatedPropertyAttribute(name);

    return true;
}
#endif

#if DUMP_NODE_STATISTICS
bool Element::hasNamedNodeMap() const
{
    return hasRareData() && elementRareData()->attributeMap();
}
#endif

inline void Element::updateName(const AtomString& oldName, const AtomString& newName)
{
    if (!isInTreeScope())
        return;

    if (oldName == newName)
        return;

    updateNameForTreeScope(treeScope(), oldName, newName);

    if (!isConnected())
        return;
    if (!is<HTMLDocument>(document()))
        return;
    updateNameForDocument(downcast<HTMLDocument>(document()), oldName, newName);
}

void Element::updateNameForTreeScope(TreeScope& scope, const AtomString& oldName, const AtomString& newName)
{
    ASSERT(oldName != newName);

    if (!oldName.isEmpty())
        scope.removeElementByName(*oldName.impl(), *this);
    if (!newName.isEmpty())
        scope.addElementByName(*newName.impl(), *this);
}

void Element::updateNameForDocument(HTMLDocument& document, const AtomString& oldName, const AtomString& newName)
{
    ASSERT(oldName != newName);

    if (isInShadowTree())
        return;

    if (WindowNameCollection::elementMatchesIfNameAttributeMatch(*this)) {
        const AtomString& id = WindowNameCollection::elementMatchesIfIdAttributeMatch(*this) ? getIdAttribute() : nullAtom();
        if (!oldName.isEmpty() && oldName != id)
            document.removeWindowNamedItem(*oldName.impl(), *this);
        if (!newName.isEmpty() && newName != id)
            document.addWindowNamedItem(*newName.impl(), *this);
    }

    if (DocumentNameCollection::elementMatchesIfNameAttributeMatch(*this)) {
        const AtomString& id = DocumentNameCollection::elementMatchesIfIdAttributeMatch(*this) ? getIdAttribute() : nullAtom();
        if (!oldName.isEmpty() && oldName != id)
            document.removeDocumentNamedItem(*oldName.impl(), *this);
        if (!newName.isEmpty() && newName != id)
            document.addDocumentNamedItem(*newName.impl(), *this);
    }
}

inline void Element::updateId(const AtomString& oldId, const AtomString& newId, NotifyObservers notifyObservers)
{
    if (!isInTreeScope())
        return;

    if (oldId == newId)
        return;

    updateIdForTreeScope(treeScope(), oldId, newId, notifyObservers);

    if (!isConnected())
        return;
    if (!is<HTMLDocument>(document()))
        return;
    updateIdForDocument(downcast<HTMLDocument>(document()), oldId, newId, UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute);
}

void Element::updateIdForTreeScope(TreeScope& scope, const AtomString& oldId, const AtomString& newId, NotifyObservers notifyObservers)
{
    ASSERT(isInTreeScope());
    ASSERT(oldId != newId);

    if (!oldId.isEmpty())
        scope.removeElementById(*oldId.impl(), *this, notifyObservers == NotifyObservers::Yes);
    if (!newId.isEmpty())
        scope.addElementById(*newId.impl(), *this, notifyObservers == NotifyObservers::Yes);
}

void Element::updateIdForDocument(HTMLDocument& document, const AtomString& oldId, const AtomString& newId, HTMLDocumentNamedItemMapsUpdatingCondition condition)
{
    ASSERT(isConnected());
    ASSERT(oldId != newId);

    if (isInShadowTree())
        return;

    if (WindowNameCollection::elementMatchesIfIdAttributeMatch(*this)) {
        const AtomString& name = condition == UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute && WindowNameCollection::elementMatchesIfNameAttributeMatch(*this) ? getNameAttribute() : nullAtom();
        if (!oldId.isEmpty() && oldId != name)
            document.removeWindowNamedItem(*oldId.impl(), *this);
        if (!newId.isEmpty() && newId != name)
            document.addWindowNamedItem(*newId.impl(), *this);
    }

    if (DocumentNameCollection::elementMatchesIfIdAttributeMatch(*this)) {
        const AtomString& name = condition == UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute && DocumentNameCollection::elementMatchesIfNameAttributeMatch(*this) ? getNameAttribute() : nullAtom();
        if (!oldId.isEmpty() && oldId != name)
            document.removeDocumentNamedItem(*oldId.impl(), *this);
        if (!newId.isEmpty() && newId != name)
            document.addDocumentNamedItem(*newId.impl(), *this);
    }
}

void Element::updateLabel(TreeScope& scope, const AtomString& oldForAttributeValue, const AtomString& newForAttributeValue)
{
    ASSERT(hasTagName(labelTag));

    if (!isConnected())
        return;

    if (oldForAttributeValue == newForAttributeValue)
        return;

    if (!oldForAttributeValue.isEmpty())
        scope.removeLabel(*oldForAttributeValue.impl(), downcast<HTMLLabelElement>(*this));
    if (!newForAttributeValue.isEmpty())
        scope.addLabel(*newForAttributeValue.impl(), downcast<HTMLLabelElement>(*this));
}

void Element::willModifyAttribute(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue)
{
    if (name == HTMLNames::idAttr)
        updateId(oldValue, newValue, NotifyObservers::No); // Will notify observers after the attribute is actually changed.
    else if (name == HTMLNames::nameAttr)
        updateName(oldValue, newValue);
    else if (name == HTMLNames::forAttr && hasTagName(labelTag)) {
        if (treeScope().shouldCacheLabelsByForAttribute())
            updateLabel(treeScope(), oldValue, newValue);
    }

    if (auto recipients = MutationObserverInterestGroup::createForAttributesMutation(*this, name))
        recipients->enqueueMutationRecord(MutationRecord::createAttributes(*this, name, oldValue));

    InspectorInstrumentation::willModifyDOMAttr(document(), *this, oldValue, newValue);
}

void Element::didAddAttribute(const QualifiedName& name, const AtomString& value)
{
    attributeChanged(name, nullAtom(), value);
    InspectorInstrumentation::didModifyDOMAttr(document(), *this, name.toAtomString(), value);
    dispatchSubtreeModifiedEvent();
}

void Element::didModifyAttribute(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue)
{
    attributeChanged(name, oldValue, newValue);
    InspectorInstrumentation::didModifyDOMAttr(document(), *this, name.toAtomString(), newValue);
    // Do not dispatch a DOMSubtreeModified event here; see bug 81141.
}

void Element::didRemoveAttribute(const QualifiedName& name, const AtomString& oldValue)
{
    attributeChanged(name, oldValue, nullAtom());
    InspectorInstrumentation::didRemoveDOMAttr(document(), *this, name.toAtomString());
    dispatchSubtreeModifiedEvent();
}

IntPoint Element::savedLayerScrollPosition() const
{
    return hasRareData() ? elementRareData()->savedLayerScrollPosition() : IntPoint();
}

void Element::setSavedLayerScrollPositionSlow(const IntPoint& position)
{
    ASSERT(!position.isZero() || hasRareData());
    ensureElementRareData().setSavedLayerScrollPosition(position);
}

RefPtr<Attr> Element::attrIfExists(const AtomString& localName, bool shouldIgnoreAttributeCase)
{
    if (auto* attrNodeList = attrNodeListForElement(*this))
        return findAttrNodeInList(*attrNodeList, localName, shouldIgnoreAttributeCase);
    return nullptr;
}

RefPtr<Attr> Element::attrIfExists(const QualifiedName& name)
{
    if (auto* attrNodeList = attrNodeListForElement(*this))
        return findAttrNodeInList(*attrNodeList, name);
    return nullptr;
}

Ref<Attr> Element::ensureAttr(const QualifiedName& name)
{
    auto& attrNodeList = ensureAttrNodeListForElement(*this);
    RefPtr<Attr> attrNode = findAttrNodeInList(attrNodeList, name);
    if (!attrNode) {
        attrNode = Attr::create(*this, name);
        attrNode->setTreeScopeRecursively(treeScope());
        attrNodeList.append(attrNode);
    }
    return attrNode.releaseNonNull();
}

void Element::detachAttrNodeFromElementWithValue(Attr* attrNode, const AtomString& value)
{
    ASSERT(hasSyntheticAttrChildNodes());
    attrNode->detachFromElementWithValue(value);

    auto& attrNodeList = *attrNodeListForElement(*this);
    bool found = attrNodeList.removeFirstMatching([attrNode](auto& attribute) {
        return attribute->qualifiedName() == attrNode->qualifiedName();
    });
    ASSERT_UNUSED(found, found);
    if (attrNodeList.isEmpty())
        removeAttrNodeListForElement(*this);
}

void Element::detachAllAttrNodesFromElement()
{
    auto* attrNodeList = attrNodeListForElement(*this);
    ASSERT(attrNodeList);

    for (const Attribute& attribute : attributesIterator()) {
        if (RefPtr<Attr> attrNode = findAttrNodeInList(*attrNodeList, attribute.name()))
            attrNode->detachFromElementWithValue(attribute.value());
    }

    removeAttrNodeListForElement(*this);
}

void Element::resetComputedStyle()
{
    if (!hasRareData() || !elementRareData()->computedStyle())
        return;

    auto reset = [](Element& element) {
        if (element.hasCustomStyleResolveCallbacks())
            element.willResetComputedStyle();
        element.elementRareData()->resetComputedStyle();
    };
    reset(*this);
    for (auto& child : descendantsOfType<Element>(*this)) {
        if (!child.hasRareData() || !child.elementRareData()->computedStyle() || child.hasDisplayContents())
            continue;
        reset(child);
    }
}

void Element::resetStyleRelations()
{
    clearStyleFlags(NodeStyleFlag::StyleAffectedByEmpty);
    if (!hasRareData())
        return;
    elementRareData()->setChildIndex(0);
}

void Element::resetChildStyleRelations()
{
    clearStyleFlags({
        NodeStyleFlag::ChildrenAffectedByFirstChildRules,
        NodeStyleFlag::ChildrenAffectedByLastChildRules,
        NodeStyleFlag::ChildrenAffectedByForwardPositionalRules,
        NodeStyleFlag::ChildrenAffectedByBackwardPositionalRules,
        NodeStyleFlag::ChildrenAffectedByPropertyBasedBackwardPositionalRules
    });
}

void Element::resetAllDescendantStyleRelations()
{
    resetChildStyleRelations();
    
    clearStyleFlags({
        NodeStyleFlag::DescendantsAffectedByForwardPositionalRules,
        NodeStyleFlag::DescendantsAffectedByBackwardPositionalRules
    });
}

void Element::clearHoverAndActiveStatusBeforeDetachingRenderer()
{
    if (!isUserActionElement())
        return;
    if (hovered())
        document().hoveredElementDidDetach(*this);
    if (isInActiveChain())
        document().elementInActiveChainDidDetach(*this);
    document().userActionElements().clearActiveAndHovered(*this);
}

void Element::willRecalcStyle(Style::Change)
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::didRecalcStyle(Style::Change)
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::willResetComputedStyle()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::willAttachRenderers()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::didAttachRenderers()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::willDetachRenderers()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

void Element::didDetachRenderers()
{
    ASSERT(hasCustomStyleResolveCallbacks());
}

std::optional<Style::ElementStyle> Element::resolveCustomStyle(const Style::ResolutionContext&, const RenderStyle*)
{
    ASSERT(hasCustomStyleResolveCallbacks());
    return std::nullopt;
}

void Element::cloneAttributesFromElement(const Element& other)
{
    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

    other.synchronizeAllAttributes();
    if (!other.m_elementData) {
        m_elementData = nullptr;
        return;
    }

    // We can't update window and document's named item maps since the presence of image and object elements depend on other attributes and children.
    // Fortunately, those named item maps are only updated when this element is in the document, which should never be the case.
    ASSERT(!isConnected());

    const AtomString& oldID = getIdAttribute();
    const AtomString& newID = other.getIdAttribute();

    if (!oldID.isNull() || !newID.isNull())
        updateId(oldID, newID, NotifyObservers::No); // Will notify observers after the attribute is actually changed.

    const AtomString& oldName = getNameAttribute();
    const AtomString& newName = other.getNameAttribute();

    if (!oldName.isNull() || !newName.isNull())
        updateName(oldName, newName);

    // If 'other' has a mutable ElementData, convert it to an immutable one so we can share it between both elements.
    // We can only do this if there is no CSSOM wrapper for other's inline style, and there are no presentation attributes.
    if (is<UniqueElementData>(*other.m_elementData)
        && !other.m_elementData->presentationalHintStyle()
        && (!other.m_elementData->inlineStyle() || !other.m_elementData->inlineStyle()->hasCSSOMWrapper()))
        const_cast<Element&>(other).m_elementData = downcast<UniqueElementData>(*other.m_elementData).makeShareableCopy();

    if (!other.m_elementData->isUnique())
        m_elementData = other.m_elementData;
    else
        m_elementData = other.m_elementData->makeUniqueCopy();

    for (const Attribute& attribute : attributesIterator())
        attributeChanged(attribute.name(), nullAtom(), attribute.value(), ModifiedByCloning);

    setNonce(other.nonce());
}

void Element::cloneDataFromElement(const Element& other)
{
    cloneAttributesFromElement(other);
    copyNonAttributePropertiesFromElement(other);
}

void Element::createUniqueElementData()
{
    if (!m_elementData)
        m_elementData = UniqueElementData::create();
    else
        m_elementData = downcast<ShareableElementData>(*m_elementData).makeUniqueCopy();
}

bool Element::canContainRangeEndPoint() const
{
    return !equalLettersIgnoringASCIICase(attributeWithoutSynchronization(roleAttr), "img"_s);
}

String Element::resolveURLStringIfNeeded(const String& urlString, ResolveURLs resolveURLs, const URL& base) const
{
    if (resolveURLs == ResolveURLs::No)
        return urlString;

    static MainThreadNeverDestroyed<const AtomString> maskedURLStringForBindings(document().maskedURLStringForBindings());
    URL completeURL = base.isNull() ? document().completeURL(urlString) : URL(base, urlString);

    switch (resolveURLs) {
    case ResolveURLs::Yes:
        return completeURL.string();

    case ResolveURLs::YesExcludingURLsForPrivacy: {
        if (document().shouldMaskURLForBindings(completeURL))
            return maskedURLStringForBindings.get();
        if (!document().url().isLocalFile())
            return completeURL.string();
        break;
    }

    case ResolveURLs::NoExcludingURLsForPrivacy:
        if (document().shouldMaskURLForBindings(completeURL))
            return maskedURLStringForBindings.get();
        break;

    case ResolveURLs::No:
        ASSERT_NOT_REACHED();
        break;
    }

    return urlString;
}

String Element::completeURLsInAttributeValue(const URL& base, const Attribute& attribute, ResolveURLs resolveURLs) const
{
    return resolveURLStringIfNeeded(attribute.value(), resolveURLs, base);
}

ExceptionOr<Node*> Element::insertAdjacent(const String& where, Ref<Node>&& newChild)
{
    // In Internet Explorer if the element has no parent and where is "beforeBegin" or "afterEnd",
    // a document fragment is created and the elements appended in the correct order. This document
    // fragment isn't returned anywhere.
    //
    // This is impossible for us to implement as the DOM tree does not allow for such structures,
    // Opera also appears to disallow such usage.

    if (equalLettersIgnoringASCIICase(where, "beforebegin"_s)) {
        auto* parent = this->parentNode();
        if (!parent)
            return nullptr;
        auto result = parent->insertBefore(newChild, this);
        if (result.hasException())
            return result.releaseException();
        return newChild.ptr();
    }

    if (equalLettersIgnoringASCIICase(where, "afterbegin"_s)) {
        auto result = insertBefore(newChild, firstChild());
        if (result.hasException())
            return result.releaseException();
        return newChild.ptr();
    }

    if (equalLettersIgnoringASCIICase(where, "beforeend"_s)) {
        auto result = appendChild(newChild);
        if (result.hasException())
            return result.releaseException();
        return newChild.ptr();
    }

    if (equalLettersIgnoringASCIICase(where, "afterend"_s)) {
        auto* parent = this->parentNode();
        if (!parent)
            return nullptr;
        auto result = parent->insertBefore(newChild, nextSibling());
        if (result.hasException())
            return result.releaseException();
        return newChild.ptr();
    }

    return Exception { SyntaxError };
}

ExceptionOr<Element*> Element::insertAdjacentElement(const String& where, Element& newChild)
{
    auto result = insertAdjacent(where, newChild);
    if (result.hasException())
        return result.releaseException();
    return downcast<Element>(result.releaseReturnValue());
}

// Step 1 of https://w3c.github.io/DOM-Parsing/#dom-element-insertadjacenthtml.
static ExceptionOr<ContainerNode&> contextNodeForInsertion(const String& where, Element& element)
{
    if (equalLettersIgnoringASCIICase(where, "beforebegin"_s) || equalLettersIgnoringASCIICase(where, "afterend"_s)) {
        auto* parent = element.parentNode();
        if (!parent || is<Document>(*parent))
            return Exception { NoModificationAllowedError };
        return *parent;
    }
    if (equalLettersIgnoringASCIICase(where, "afterbegin"_s) || equalLettersIgnoringASCIICase(where, "beforeend"_s))
        return element;
    return Exception { SyntaxError };
}

// Step 2 of https://w3c.github.io/DOM-Parsing/#dom-element-insertadjacenthtml.
static ExceptionOr<Ref<Element>> contextElementForInsertion(const String& where, Element& element)
{
    auto contextNodeResult = contextNodeForInsertion(where, element);
    if (contextNodeResult.hasException())
        return contextNodeResult.releaseException();
    auto& contextNode = contextNodeResult.releaseReturnValue();
    if (!is<Element>(contextNode) || (contextNode.document().isHTMLDocument() && is<HTMLHtmlElement>(contextNode)))
        return Ref<Element> { HTMLBodyElement::create(contextNode.document()) };
    return Ref<Element> { downcast<Element>(contextNode) };
}

// https://w3c.github.io/DOM-Parsing/#dom-element-insertadjacenthtml
ExceptionOr<void> Element::insertAdjacentHTML(const String& where, const String& markup, NodeVector* addedNodes)
{
    // Steps 1 and 2.
    auto contextElement = contextElementForInsertion(where, *this);
    if (contextElement.hasException())
        return contextElement.releaseException();
    // Step 3.
    auto fragment = createFragmentForInnerOuterHTML(contextElement.releaseReturnValue(), markup, { ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent });
    if (fragment.hasException())
        return fragment.releaseException();

    if (UNLIKELY(addedNodes)) {
        // Must be called before insertAdjacent, as otherwise the children of fragment will be moved
        // to their new parent and will be harder to keep track of.
        collectChildNodes(fragment.returnValue(), *addedNodes);
    }

    // Step 4.
    auto result = insertAdjacent(where, fragment.releaseReturnValue());
    if (result.hasException())
        return result.releaseException();
    return { };
}

ExceptionOr<void> Element::insertAdjacentHTML(const String& where, const String& markup)
{
    return insertAdjacentHTML(where, markup, nullptr);
}

ExceptionOr<void> Element::insertAdjacentText(const String& where, String&& text)
{
    auto result = insertAdjacent(where, document().createTextNode(WTFMove(text)));
    if (result.hasException())
        return result.releaseException();
    return { };
}

Element* Element::findAnchorElementForLink(String& outAnchorName)
{
    if (!isLink())
        return nullptr;

    const AtomString& href = attributeWithoutSynchronization(HTMLNames::hrefAttr);
    if (href.isNull())
        return nullptr;

    Document& document = this->document();
    URL url = document.completeURL(href);
    if (!url.isValid())
        return nullptr;

    if (url.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(url, document.baseURL())) {
        outAnchorName = url.fragmentIdentifier().toString();
        return document.findAnchor(outAnchorName);
    }

    return nullptr;
}

ExceptionOr<Ref<WebAnimation>> Element::animate(JSC::JSGlobalObject& lexicalGlobalObject, JSC::Strong<JSC::JSObject>&& keyframes, std::optional<std::variant<double, KeyframeAnimationOptions>>&& options)
{
    String id = emptyString();
    std::optional<RefPtr<AnimationTimeline>> timeline;
    std::variant<FramesPerSecond, AnimationFrameRatePreset> frameRate = AnimationFrameRatePreset::Auto;
    std::optional<std::variant<double, KeyframeEffectOptions>> keyframeEffectOptions;
    if (options) {
        auto optionsValue = options.value();
        std::variant<double, KeyframeEffectOptions> keyframeEffectOptionsVariant;
        if (std::holds_alternative<double>(optionsValue))
            keyframeEffectOptionsVariant = std::get<double>(optionsValue);
        else {
            auto keyframeEffectOptions = std::get<KeyframeAnimationOptions>(optionsValue);
            id = keyframeEffectOptions.id;
            frameRate = keyframeEffectOptions.frameRate;
            timeline = keyframeEffectOptions.timeline;
            keyframeEffectOptionsVariant = WTFMove(keyframeEffectOptions);
        }
        keyframeEffectOptions = keyframeEffectOptionsVariant;
    }

    auto keyframeEffectResult = KeyframeEffect::create(lexicalGlobalObject, document(), this, WTFMove(keyframes), WTFMove(keyframeEffectOptions));
    if (keyframeEffectResult.hasException())
        return keyframeEffectResult.releaseException();

    auto animation = WebAnimation::create(document(), &keyframeEffectResult.returnValue().get());
    animation->setId(id);
    if (timeline)
        animation->setTimeline(timeline->get());
    animation->setBindingsFrameRate(WTFMove(frameRate));

    auto animationPlayResult = animation->play();
    if (animationPlayResult.hasException())
        return animationPlayResult.releaseException();

    return animation;
}

Vector<RefPtr<WebAnimation>> Element::getAnimations(std::optional<GetAnimationsOptions> options)
{
    // If we are to return animations in the subtree, we can get all of the document's animations and filter
    // animations targeting that are not registered on this element, one of its pseudo elements or a child's
    // pseudo element.
    if (options && options->subtree) {
        return document().matchingAnimations([&] (Element& target) -> bool {
            return contains(&target);
        });
    }

    // For the list of animations to be current, we need to account for any pending CSS changes,
    // such as updates to CSS Animations and CSS Transitions. This requires updating layout as
    // well since resolving layout-dependent media queries could yield animations.
    // FIXME: We might be able to use ComputedStyleExtractor which is more optimized.
    if (RefPtr owner = document().ownerElement())
        owner->document().updateLayout();
    document().updateStyleIfNeeded();

    Vector<RefPtr<WebAnimation>> animations;
    if (auto* effectStack = keyframeEffectStack(PseudoId::None)) {
        for (auto& effect : effectStack->sortedEffects()) {
            if (effect->animation()->isRelevant())
                animations.append(effect->animation());
        }
    }
    return animations;
}

static WeakHashMap<Element, ElementIdentifier, WeakPtrImplWithEventTargetData>& elementIdentifiersMap()
{
    static MainThreadNeverDestroyed<WeakHashMap<Element, ElementIdentifier, WeakPtrImplWithEventTargetData>> map;
    return map;
}

ElementIdentifier Element::identifier() const
{
    return elementIdentifiersMap().ensure(*this, [] { return ElementIdentifier::generate(); }).iterator->value;
}

Element* Element::fromIdentifier(ElementIdentifier identifier)
{
    for (auto [element, elementIdentifier] : elementIdentifiersMap()) {
        if (elementIdentifier == identifier)
            return &element;
    }
    return nullptr;
}

StylePropertyMap* Element::attributeStyleMap()
{
    if (!hasRareData())
        return nullptr;
    return elementRareData()->attributeStyleMap();
}

void Element::setAttributeStyleMap(Ref<StylePropertyMap>&& map)
{
    ensureElementRareData().setAttributeStyleMap(WTFMove(map));
}

StylePropertyMapReadOnly* Element::computedStyleMap()
{
    auto& rareData = ensureElementRareData();
    if (auto* map = rareData.computedStyleMap())
        return map;

    auto map = ComputedStylePropertyMapReadOnly::create(*this);
    rareData.setComputedStyleMap(WTFMove(map));
    return rareData.computedStyleMap();
}

} // namespace WebCore
