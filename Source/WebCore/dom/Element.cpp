/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
#include "CSSAnimationController.h"
#include "CSSParser.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClassChangeInvalidation.h"
#include "ComposedTreeAncestorIterator.h"
#include "ComposedTreeIterator.h"
#include "ContainerNodeAlgorithms.h"
#include "CustomElementReactionQueue.h"
#include "CustomElementRegistry.h"
#include "DOMRect.h"
#include "DOMRectList.h"
#include "DOMTokenList.h"
#include "DOMWindow.h"
#include "DocumentSharedObjectPool.h"
#include "DocumentTimeline.h"
#include "Editing.h"
#include "ElementAnimationRareData.h"
#include "ElementIterator.h"
#include "ElementRareData.h"
#include "EventDispatcher.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FocusController.h"
#include "FocusEvent.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "FullscreenManager.h"
#include "GetAnimationsOptions.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDocument.h"
#include "HTMLHtmlElement.h"
#include "HTMLLabelElement.h"
#include "HTMLNameCollection.h"
#include "HTMLObjectElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLSelectElement.h"
#include "HTMLTemplateElement.h"
#include "IdChangeInvalidation.h"
#include "IdTargetObserverRegistry.h"
#include "InspectorInstrumentation.h"
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
#include "RenderListBox.h"
#include "RenderTheme.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "ScriptDisallowedScope.h"
#include "ScrollIntoViewOptions.h"
#include "ScrollLatchingController.h"
#include "SelectorQuery.h"
#include "Settings.h"
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
#include "WheelEvent.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "markup.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
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

#if ENABLE(INTERSECTION_OBSERVER)
    disconnectFromIntersectionObservers();
#endif

#if ENABLE(RESIZE_OBSERVER)
    disconnectFromResizeObservers();
#endif

    removeShadowRoot();

    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

#if ENABLE(CSS_TYPED_OM)
    if (hasRareData()) {
        if (auto* map = elementRareData()->attributeStyleMap())
            map->clearElement();
    }
#endif

    if (hasPendingResources()) {
        document().accessSVGExtensions().removeElementFromPendingResources(*this);
        ASSERT(!hasPendingResources());
    }
}

inline ElementRareData* Element::elementRareData() const
{
    ASSERT_WITH_SECURITY_IMPLICATION(hasRareData());
    return static_cast<ElementRareData*>(rareData());
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

void Element::setTabIndexExplicitly(Optional<int> tabIndex)
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

Optional<int> Element::tabIndexSetExplicitly() const
{
    switch (tabIndexState()) {
    case TabIndexState::NotSet:
        return WTF::nullopt;
    case TabIndexState::Zero:
        return 0;
    case TabIndexState::NegativeOne:
        return -1;
    case TabIndexState::InRareData:
        ASSERT(hasRareData());
        return elementRareData()->unusualTabIndex();
    }
    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

int Element::defaultTabIndex() const
{
    return -1;
}

bool Element::supportsFocus() const
{
    return !!tabIndexSetExplicitly();
}

RefPtr<Element> Element::focusDelegate()
{
    return this;
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
    if (!(isFocusable() && !shouldBeIgnoredInSequentialFocusNavigation() && tabIndexSetExplicitly().valueOr(0) >= 0))
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
    return type != eventNames().clickEvent && type != eventNames().mouseoverEvent && type != eventNames().mouseoutEvent && type != eventNames().mouseenterEvent && type != eventNames().mouseleaveEvent;
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

        if (auto pointerEvent = pointerCaptureController.pointerEventForMouseEvent(mouseEvent)) {
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

bool Element::dispatchMouseEvent(const PlatformMouseEvent& platformEvent, const AtomString& eventType, int detail, Element* relatedTarget)
{
    if (isDisabledFormControl())
        return false;

    if (isForceEvent(platformEvent) && !document().hasListenerTypeForEventType(platformEvent.type()))
        return false;

    Ref<MouseEvent> mouseEvent = MouseEvent::create(eventType, document().windowProxy(), platformEvent, detail, relatedTarget);

    if (mouseEvent->type().isEmpty())
        return true; // Shouldn't happen.

    bool didNotSwallowEvent = true;

    if (dispatchPointerEventIfNeeded(*this, mouseEvent.get(), platformEvent, didNotSwallowEvent) == ShouldIgnoreMouseEvent::Yes)
        return false;

    if (Quirks::StorageAccessResult::ShouldCancelEvent == document().quirks().triggerOptionalStorageAccessQuirk(*this, eventType))
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

bool Element::dispatchWheelEvent(const PlatformWheelEvent& platformEvent)
{
    auto event = WheelEvent::create(platformEvent, document().windowProxy());

    // Events with no deltas are important because they convey platform information about scroll gestures
    // and momentum beginning or ending. However, those events should not be sent to the DOM since some
    // websites will break. They need to be dispatched because dispatching them will call into the default
    // event handler, and our platform code will correctly handle the phase changes. Calling stopPropagation()
    // will prevent the event from being sent to the DOM, but will still call the default event handler.
    // FIXME: Move this logic into WheelEvent::create.
    if (!platformEvent.deltaX() && !platformEvent.deltaY())
        event->stopPropagation();

    dispatchEvent(event);
    
    LOG_WITH_STREAM(Scrolling, stream << "Element " << *this << " dispatchWheelEvent: defaultPrevented " << event->defaultPrevented() << " defaultHandled " << event->defaultHandled());
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
        return equalLettersIgnoringASCIICase(attributeLocalName, "style");
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
    if (!elementData())
        return nullAtom();
    synchronizeAttribute(name);
    if (const Attribute* attribute = findAttributeByName(name))
        return attribute->value();
    return nullAtom();
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
            return canvas->isVisibleWithoutResolvingFullStyle();
    }

    return isVisibleWithoutResolvingFullStyle();
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

void Element::setActive(bool flag, bool pause)
{
    if (flag == active())
        return;
    {
        Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassActive);
        document().userActionElements().setActive(*this, flag);
    }

    if (!renderer())
        return;

    bool reactsToPress = false;
    if (renderer()->style().hasAppearance() && renderer()->theme().stateChanged(*renderer(), ControlStates::PressedState))
        reactsToPress = true;

    // The rest of this function implements a feature that only works if the
    // platform supports immediate invalidations on the ChromeClient, so bail if
    // that isn't supported.
    if (!document().page()->chrome().client().supportsImmediateInvalidation())
        return;

    if (reactsToPress && pause) {
        // The delay here is subtle. It relies on an assumption, namely that the amount of time it takes
        // to repaint the "down" state of the control is about the same time as it would take to repaint the
        // "up" state. Once you assume this, you can just delay for 100ms - that time (assuming that after you
        // leave this method, it will be about that long before the flush of the up state happens again).
#ifdef HAVE_FUNC_USLEEP
        MonotonicTime startTime = MonotonicTime::now();
#endif

        document().updateStyleIfNeeded();

        // Do an immediate repaint.
        if (renderer())
            renderer()->repaint();

        // FIXME: Come up with a less ridiculous way of doing this.
#ifdef HAVE_FUNC_USLEEP
        // Now pause for a small amount of time (1/10th of a second from before we repainted in the pressed state)
        Seconds remainingTime = 100_ms - (MonotonicTime::now() - startTime);
        if (remainingTime > 0_s)
            usleep(static_cast<useconds_t>(remainingTime.microseconds()));
#endif
    }
}

void Element::setFocus(bool flag)
{
    if (flag == focused())
        return;
    {
        Style::PseudoClassChangeInvalidation focusStyleInvalidation(*this, CSSSelector::PseudoClassFocus);
        Style::PseudoClassChangeInvalidation directFocusStyleInvalidation(*this, CSSSelector::PseudoClassDirectFocus);
        document().userActionElements().setFocused(*this, flag);
    }

    // Shadow host with a slot that contain focused element is not considered focused.
    for (auto* root = containingShadowRoot(); root; root = root->host()->containingShadowRoot()) {
        root->setContainsFocusedElement(flag);
        root->host()->invalidateStyle();
    }

    for (auto* element = this; element; element = element->parentElementInComposedTree())
        element->setHasFocusWithin(flag);
}

void Element::setHasFocusWithin(bool flag)
{
    if (hasFocusWithin() == flag)
        return;
    {
        Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassFocusWithin);
        setNodeFlag(NodeFlag::HasFocusWithin, flag);
    }
}

void Element::setHovered(bool flag)
{
    if (flag == hovered())
        return;
    {
        Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassHover);
        document().userActionElements().setHovered(*this, flag);
    }

    if (auto* style = renderStyle(); style && style->hasAppearance())
        renderer()->theme().stateChanged(*renderer(), ControlStates::HoverState);
}

void Element::setBeingDragged(bool flag)
{
    if (flag == isBeingDragged())
        return;

    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassDrag);
    document().userActionElements().setBeingDragged(*this, flag);
}

inline ScrollAlignment toScrollAlignmentForInlineDirection(Optional<ScrollLogicalPosition> position, WritingMode writingMode, bool isLTR)
{
    switch (position.valueOr(ScrollLogicalPosition::Nearest)) {
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

inline ScrollAlignment toScrollAlignmentForBlockDirection(Optional<ScrollLogicalPosition> position, WritingMode writingMode)
{
    switch (position.valueOr(ScrollLogicalPosition::Start)) {
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

void Element::scrollIntoView(Optional<Variant<bool, ScrollIntoViewOptions>>&& arg)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    bool insideFixed;
    LayoutRect absoluteBounds = renderer()->absoluteAnchorRect(&insideFixed);

    ScrollIntoViewOptions options;
    if (arg) {
        auto value = arg.value();
        if (WTF::holds_alternative<ScrollIntoViewOptions>(value))
            options = WTF::get<ScrollIntoViewOptions>(value);
        else if (!WTF::get<bool>(value))
            options.blockPosition = ScrollLogicalPosition::End;
    }

    auto writingMode = renderer()->style().writingMode();
    ScrollAlignment alignX = toScrollAlignmentForInlineDirection(options.inlinePosition, writingMode, renderer()->style().isLeftToRightDirection());
    ScrollAlignment alignY = toScrollAlignmentForBlockDirection(options.blockPosition, writingMode);

    bool isHorizontal = renderer()->style().isHorizontalWritingMode();
    ScrollRectToVisibleOptions visibleOptions {
        SelectionRevealMode::Reveal,
        isHorizontal ? alignX : alignY,
        isHorizontal ? alignY : alignX,
        ShouldAllowCrossOriginScrolling::No,
        options.behavior.valueOr(ScrollBehavior::Auto)
    };
    renderer()->scrollRectToVisible(absoluteBounds, insideFixed, visibleOptions);
}

void Element::scrollIntoView(bool alignToTop) 
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    bool insideFixed;
    LayoutRect absoluteBounds = renderer()->absoluteAnchorRect(&insideFixed);
    // Align to the top / bottom and to the closest edge.
    if (alignToTop)
        renderer()->scrollRectToVisible(absoluteBounds, insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways, ShouldAllowCrossOriginScrolling::No });
    else
        renderer()->scrollRectToVisible(absoluteBounds, insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignBottomAlways, ShouldAllowCrossOriginScrolling::No });
}

void Element::scrollIntoViewIfNeeded(bool centerIfNeeded)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    bool insideFixed;
    LayoutRect absoluteBounds = renderer()->absoluteAnchorRect(&insideFixed);
    if (centerIfNeeded)
        renderer()->scrollRectToVisible(absoluteBounds, insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded, ShouldAllowCrossOriginScrolling::No });
    else
        renderer()->scrollRectToVisible(absoluteBounds, insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded, ShouldAllowCrossOriginScrolling::No });
}

void Element::scrollIntoViewIfNotVisible(bool centerIfNotVisible)
{
    document().updateLayoutIgnorePendingStylesheets();
    
    if (!renderer())
        return;
    
    bool insideFixed;
    LayoutRect absoluteBounds = renderer()->absoluteAnchorRect(&insideFixed);
    if (centerIfNotVisible)
        renderer()->scrollRectToVisible(absoluteBounds, insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignCenterIfNotVisible, ScrollAlignment::alignCenterIfNotVisible, ShouldAllowCrossOriginScrolling::No });
    else
        renderer()->scrollRectToVisible(absoluteBounds, insideFixed, { SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNotVisible, ScrollAlignment::alignToEdgeIfNotVisible, ShouldAllowCrossOriginScrolling::No });
}

void Element::scrollBy(const ScrollToOptions& options)
{
    ScrollToOptions scrollToOptions = normalizeNonFiniteCoordinatesOrFallBackTo(options, 0, 0);
    scrollToOptions.left.value() += scrollLeft();
    scrollToOptions.top.value() += scrollTop();
    scrollTo(scrollToOptions);
}

void Element::scrollBy(double x, double y)
{
    scrollBy(ScrollToOptions(x, y));
}

void Element::scrollTo(const ScrollToOptions& options, ScrollClamping clamping)
{
    if (!document().settings().CSSOMViewScrollingAPIEnabled()) {
        // If the element is the root element and document is in quirks mode, terminate these steps.
        // Note that WebKit always uses quirks mode document scrolling behavior. See Document::scrollingElement().
        if (this == document().documentElement())
            return;
    }

    document().updateLayoutIgnorePendingStylesheets();

    if (document().scrollingElement() == this) {
        // If the element is the scrolling element and is not potentially scrollable,
        // invoke scroll() on window with options as the only argument, and terminate these steps.
        // FIXME: Scrolling an independently scrollable body is broken: webkit.org/b/161612.
        auto window = makeRefPtr(document().domWindow());
        if (!window)
            return;

        window->scrollTo(options, clamping);
        return;
    }

    // If the element does not have any associated CSS layout box, the element has no associated scrolling box,
    // or the element has no overflow, terminate these steps.
    RenderBox* renderer = renderBox();
    if (!renderer || !renderer->hasOverflowClip())
        return;

    ScrollToOptions scrollToOptions = normalizeNonFiniteCoordinatesOrFallBackTo(options,
        adjustForAbsoluteZoom(renderer->scrollLeft(), *renderer),
        adjustForAbsoluteZoom(renderer->scrollTop(), *renderer)
    );
    IntPoint scrollPosition(
        clampToInteger(scrollToOptions.left.value() * renderer->style().effectiveZoom()),
        clampToInteger(scrollToOptions.top.value() * renderer->style().effectiveZoom())
    );
    AnimatedScroll animated = useSmoothScrolling(scrollToOptions.behavior.valueOr(ScrollBehavior::Auto), this) ? AnimatedScroll::Yes : AnimatedScroll::No;
    renderer->setScrollPosition(scrollPosition, ScrollType::Programmatic, clamping, animated);
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

    if (!renderer->hasOverflowClip())
        return;

    ScrollDirection direction = ScrollDown;
    if (units < 0) {
        direction = ScrollUp;
        units = -units;
    }
    Element* stopElement = this;
    downcast<RenderBox>(*renderer).scroll(direction, granularity, units, &stopElement);
}

void Element::scrollByLines(int lines)
{
    scrollByUnits(lines, ScrollByLine);
}

void Element::scrollByPages(int pages)
{
    scrollByUnits(pages, ScrollByPage);
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

static double adjustForLocalZoom(LayoutUnit value, const RenderElement& renderer, double& zoomFactor)
{
    zoomFactor = localZoomForRenderer(renderer);
    if (zoomFactor == 1)
        return value.toDouble();
    return value.toDouble() / zoomFactor;
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

static bool subpixelMetricsEnabled(const Document& document)
{
    return document.settings().subpixelCSSOMElementMetricsEnabled();
}

static double convertToNonSubpixelValueIfNeeded(double value, const Document& document, LegacyCSSOMElementMetricsRoundingStrategy roundStrategy = Round)
{
    return subpixelMetricsEnabled(document) ? value : roundStrategy == Round ? round(value) : floor(value);
}

static double adjustOffsetForZoomAndSubpixelLayout(RenderBoxModelObject* renderer, const LayoutUnit& offset)
{
    LayoutUnit offsetLeft = subpixelMetricsEnabled(renderer->document()) ? offset : LayoutUnit(roundToInt(offset));
    double zoomFactor = 1;
    double offsetLeftAdjustedWithZoom = adjustForLocalZoom(offsetLeft, *renderer, zoomFactor);
    return convertToNonSubpixelValueIfNeeded(offsetLeftAdjustedWithZoom, renderer->document(), zoomFactor == 1 ? Floor : Round);
}

static HashSet<TreeScope*> collectAncestorTreeScopeAsHashSet(Node& node)
{
    HashSet<TreeScope*> ancestors;
    for (auto* currentScope = &node.treeScope(); currentScope; currentScope = currentScope->parentTreeScope())
        ancestors.add(currentScope);
    return ancestors;
}

double Element::offsetLeftForBindings()
{
    auto offset = offsetLeft();

    auto parent = makeRefPtr(offsetParent());
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

double Element::offsetLeft()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustOffsetForZoomAndSubpixelLayout(renderer, renderer->offsetLeft());
    return 0;
}

double Element::offsetTopForBindings()
{
    auto offset = offsetTop();

    auto parent = makeRefPtr(offsetParent());
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

double Element::offsetTop()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustOffsetForZoomAndSubpixelLayout(renderer, renderer->offsetTop());
    return 0;
}

double Element::offsetWidth()
{
    document().updateLayoutIfDimensionsOutOfDate(*this, WidthDimensionsCheck);
    if (RenderBoxModelObject* renderer = renderBoxModelObject()) {
        LayoutUnit offsetWidth = subpixelMetricsEnabled(renderer->document()) ? renderer->offsetWidth() : LayoutUnit(roundToInt(renderer->offsetWidth()));
        return convertToNonSubpixelValueIfNeeded(adjustLayoutUnitForAbsoluteZoom(offsetWidth, *renderer).toDouble(), renderer->document());
    }
    return 0;
}

double Element::offsetHeight()
{
    document().updateLayoutIfDimensionsOutOfDate(*this, HeightDimensionsCheck);
    if (RenderBoxModelObject* renderer = renderBoxModelObject()) {
        LayoutUnit offsetHeight = subpixelMetricsEnabled(renderer->document()) ? renderer->offsetHeight() : LayoutUnit(roundToInt(renderer->offsetHeight()));
        return convertToNonSubpixelValueIfNeeded(adjustLayoutUnitForAbsoluteZoom(offsetHeight, *renderer).toDouble(), renderer->document());
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

double Element::clientLeft()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (auto* renderer = renderBox()) {
        LayoutUnit clientLeft = subpixelMetricsEnabled(renderer->document()) ? renderer->clientLeft() : LayoutUnit(roundToInt(renderer->clientLeft()));
        return convertToNonSubpixelValueIfNeeded(adjustLayoutUnitForAbsoluteZoom(clientLeft, *renderer).toDouble(), renderer->document());
    }
    return 0;
}

double Element::clientTop()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (auto* renderer = renderBox()) {
        LayoutUnit clientTop = subpixelMetricsEnabled(renderer->document()) ? renderer->clientTop() : LayoutUnit(roundToInt(renderer->clientTop()));
        return convertToNonSubpixelValueIfNeeded(adjustLayoutUnitForAbsoluteZoom(clientTop, *renderer).toDouble(), renderer->document());
    }
    return 0;
}

double Element::clientWidth()
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
        LayoutUnit clientWidth = subpixelMetricsEnabled(renderer->document()) ? renderer->clientWidth() : LayoutUnit(roundToInt(renderer->clientWidth()));
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
        return convertToNonSubpixelValueIfNeeded(adjustLayoutUnitForAbsoluteZoom(clientWidth, *renderer).toDouble(), renderer->document());
    }
    return 0;
}

double Element::clientHeight()
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
        LayoutUnit clientHeight = subpixelMetricsEnabled(renderer->document()) ? renderer->clientHeight() : LayoutUnit(roundToInt(renderer->clientHeight()));
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
        return convertToNonSubpixelValueIfNeeded(adjustLayoutUnitForAbsoluteZoom(clientHeight, *renderer).toDouble(), renderer->document());
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

    if (document().scrollingElement() == this) {
        if (auto* frame = documentFrameWithNonNullView()) {
            // FIXME: Should we use document()->scrollingElement()?
            // See https://bugs.webkit.org/show_bug.cgi?id=205059
            AnimatedScroll animated = useSmoothScrolling(ScrollBehavior::Auto, document().documentElement()) ? AnimatedScroll::Yes : AnimatedScroll::No;
            IntPoint position(static_cast<int>(newLeft * frame->pageZoomFactor() * frame->frameScaleFactor()), frame->view()->scrollY());
            frame->view()->setScrollPosition(position, ScrollClamping::Clamped, animated);
        }
        return;
    }

    if (auto* renderer = renderBox()) {
        int clampedLeft = clampToInteger(newLeft * renderer->style().effectiveZoom());
        AnimatedScroll animated = useSmoothScrolling(ScrollBehavior::Auto, this) ? AnimatedScroll::Yes : AnimatedScroll::No;
        renderer->setScrollLeft(clampedLeft, ScrollType::Programmatic, ScrollClamping::Clamped, animated);
        if (auto* scrollableArea = renderer->layer())
            scrollableArea->setScrollShouldClearLatchedState(true);
    }
}

void Element::setScrollTop(int newTop)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (document().scrollingElement() == this) {
        if (auto* frame = documentFrameWithNonNullView()) {
            // FIXME: Should we use document()->scrollingElement()?
            // See https://bugs.webkit.org/show_bug.cgi?id=205059
            AnimatedScroll animated = useSmoothScrolling(ScrollBehavior::Auto, document().documentElement()) ? AnimatedScroll::Yes : AnimatedScroll::No;
            IntPoint position(frame->view()->scrollX(), static_cast<int>(newTop * frame->pageZoomFactor() * frame->frameScaleFactor()));
            frame->view()->setScrollPosition(position, ScrollClamping::Clamped, animated);
        }
        return;
    }

    if (auto* renderer = renderBox()) {
        int clampedTop = clampToInteger(newTop * renderer->style().effectiveZoom());
        AnimatedScroll animated = useSmoothScrolling(ScrollBehavior::Auto, this) ? AnimatedScroll::Yes : AnimatedScroll::No;
        renderer->setScrollTop(clampedTop, ScrollType::Programmatic, ScrollClamping::Clamped, animated);
        if (auto* scrollableArea = renderer->layer())
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

IntRect Element::boundsInRootViewSpace()
{
    document().updateLayoutIgnorePendingStylesheets();

    FrameView* view = document().view();
    if (!view)
        return IntRect();

    Vector<FloatQuad> quads;

    if (isSVGElement() && renderer()) {
        // Get the bounding rectangle from the SVG model.
        SVGElement& svgElement = downcast<SVGElement>(*this);
        FloatRect localRect;
        if (svgElement.getBoundingBox(localRect))
            quads.append(renderer()->localToAbsoluteQuad(localRect));
    } else {
        // Get the bounding rectangle from the box model.
        if (renderBoxModelObject())
            renderBoxModelObject()->absoluteQuads(quads);
    }

    return view->contentsToRootView(enclosingIntRect(unitedBoundingBoxes(quads)));
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
    if (auto* containingBlock = renderBox.containingBlockForAbsolutePosition()) {
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
    if (isSVGElement()) {
        // Get the bounding rectangle from the SVG model.
        SVGElement& svgElement = downcast<SVGElement>(*this);
        FloatRect localRect;
        if (svgElement.getBoundingBox(localRect, SVGLocatable::DisallowStyleUpdate))
            result = LayoutRect(renderer()->localToAbsoluteQuad(localRect, UseTransforms, &includesFixedPositionElements).boundingBox());
    } else {
        auto* renderer = this->renderer();
        if (is<RenderBox>(renderer)) {
            auto& box = downcast<RenderBox>(*renderer);

            bool computedBounds = false;
            
            if (RenderFragmentedFlow* fragmentedFlow = box.enclosingFragmentedFlow()) {
                bool wasFixed = false;
                Vector<FloatQuad> quads;
                FloatRect localRect(0, 0, box.width(), box.height());
                if (fragmentedFlow->absoluteQuadsForBox(quads, &wasFixed, &box, localRect.y(), localRect.maxY())) {
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

static Optional<std::pair<RenderObject*, LayoutRect>> listBoxElementBoundingBox(Element& element)
{
    HTMLSelectElement* selectElement;
    bool isGroup;
    if (is<HTMLOptionElement>(element)) {
        selectElement = downcast<HTMLOptionElement>(element).ownerSelectElement();
        isGroup = false;
    } else if (is<HTMLOptGroupElement>(element)) {
        selectElement = downcast<HTMLOptGroupElement>(element).ownerSelectElement();
        isGroup = true;
    } else
        return WTF::nullopt;

    if (!selectElement || !selectElement->renderer() || !is<RenderListBox>(selectElement->renderer()))
        return WTF::nullopt;

    auto& renderer = downcast<RenderListBox>(*selectElement->renderer());
    Optional<LayoutRect> boundingBox;
    int optionIndex = 0;
    for (auto* item : selectElement->listItems()) {
        if (item == &element) {
            LayoutPoint additionOffset;
            boundingBox = renderer.itemBoundingBoxRect(additionOffset, optionIndex);
            if (!isGroup)
                break;
        } else if (isGroup && boundingBox) {
            if (item->parentNode() != &element)
                break;
            LayoutPoint additionOffset;
            boundingBox->setHeight(boundingBox->height() + renderer.itemBoundingBoxRect(additionOffset, optionIndex).height());
        }
        ++optionIndex;
    }

    if (!boundingBox)
        return WTF::nullopt;

    return std::pair<RenderObject*, LayoutRect> { &renderer, boundingBox.value() };
}

Ref<DOMRectList> Element::getClientRects()
{
    document().updateLayoutIgnorePendingStylesheets();

    RenderObject* renderer = this->renderer();
    Vector<FloatQuad> quads;

    if (auto pair = listBoxElementBoundingBox(*this)) {
        renderer = pair.value().first;
        quads.append(renderer->localToAbsoluteQuad(FloatQuad { pair.value().second }));
    } else if (auto* renderBoxModelObject = this->renderBoxModelObject())
        renderBoxModelObject->absoluteQuads(quads);

    // FIXME: Handle SVG elements.
    // FIXME: Handle table/inline-table with a caption.

    if (quads.isEmpty())
        return DOMRectList::create();

    document().convertAbsoluteToClientQuads(quads, renderer->style());
    return DOMRectList::create(quads);
}

Optional<std::pair<RenderObject*, FloatRect>> Element::boundingAbsoluteRectWithoutLayout()
{
    RenderObject* renderer = this->renderer();
    Vector<FloatQuad> quads;
    if (isSVGElement() && renderer && !renderer->isSVGRoot()) {
        // Get the bounding rectangle from the SVG model.
        SVGElement& svgElement = downcast<SVGElement>(*this);
        FloatRect localRect;
        if (svgElement.getBoundingBox(localRect))
            quads.append(renderer->localToAbsoluteQuad(localRect));
    } else if (auto pair = listBoxElementBoundingBox(*this)) {
        renderer = pair.value().first;
        quads.append(renderer->localToAbsoluteQuad(FloatQuad { pair.value().second }));
    } else if (auto* renderBoxModelObject = this->renderBoxModelObject())
        renderBoxModelObject->absoluteQuads(quads);

    if (quads.isEmpty())
        return WTF::nullopt;

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

// Note that this is not web-exposed, and does not use the same coordinate system as getBoundingClientRect() and friends.
IntRect Element::clientRect() const
{
    if (RenderObject* renderer = this->renderer())
        return document().view()->contentsToRootView(renderer->absoluteBoundingBoxRect());
    return IntRect();
}
    
IntRect Element::screenRect() const
{
    if (RenderObject* renderer = this->renderer())
        return document().view()->contentsToScreen(renderer->absoluteBoundingBoxRect());
    return IntRect();
}

const AtomString& Element::getAttribute(const AtomString& qualifiedName) const
{
    if (!elementData())
        return nullAtom();
    synchronizeAttribute(qualifiedName);
    if (const Attribute* attribute = elementData()->findAttributeByName(qualifiedName, shouldIgnoreAttributeCase(*this)))
        return attribute->value();
    return nullAtom();
}

const AtomString& Element::getAttributeNS(const AtomString& namespaceURI, const AtomString& localName) const
{
    return getAttribute(QualifiedName(nullAtom(), localName, namespaceURI));
}

// https://dom.spec.whatwg.org/#dom-element-toggleattribute
ExceptionOr<bool> Element::toggleAttribute(const AtomString& qualifiedName, Optional<bool> force)
{
    if (!Document::isValidName(qualifiedName))
        return Exception { InvalidCharacterError };

    synchronizeAttribute(qualifiedName);

    auto caseAdjustedQualifiedName = shouldIgnoreAttributeCase(*this) ? qualifiedName.convertToASCIILowercase() : qualifiedName;
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(caseAdjustedQualifiedName, false) : ElementData::attributeNotFound;
    if (index == ElementData::attributeNotFound) {
        if (!force || *force) {
            setAttributeInternal(index, QualifiedName { nullAtom(), caseAdjustedQualifiedName, nullAtom() }, emptyString(), NotInSynchronizationOfLazyAttribute);
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
        else if (name == HTMLNames::pseudoAttr) {
            if (needsStyleInvalidation() && isInShadowTree())
                invalidateStyleForSubtree();
        } else if (name == HTMLNames::slotAttr) {
            if (auto* parent = parentElement()) {
                if (auto* shadowRoot = parent->shadowRoot())
                    shadowRoot->hostChildElementDidChangeSlotAttribute(*this, oldValue, newValue);
            }
        } else if (name == HTMLNames::partAttr)
            partAttributeChanged(newValue);
        else if (name == HTMLNames::exportpartsAttr) {
            if (auto* shadowRoot = this->shadowRoot()) {
                shadowRoot->invalidatePartMappings();
                Style::Invalidator::invalidateShadowParts(*shadowRoot);
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
        cache->deferAttributeChangeIfNeeded(name, this);
}

template <typename CharacterType>
static inline bool isNonEmptyTokenList(const CharacterType* characters, unsigned length)
{
    ASSERT(length > 0);

    unsigned i = 0;
    do {
        if (isNotHTMLSpace(characters[i]))
            break;
        ++i;
    } while (i < length);

    return i < length;
}

static inline bool isNonEmptyTokenList(const AtomString& stringValue)
{
    unsigned length = stringValue.length();

    if (!length)
        return false;

    if (stringValue.is8Bit())
        return isNonEmptyTokenList(stringValue.characters8(), length);
    return isNonEmptyTokenList(stringValue.characters16(), length);
}

void Element::classAttributeChanged(const AtomString& newClassString)
{
    // Note: We'll need ElementData, but it doesn't have to be UniqueElementData.
    if (!elementData())
        ensureUniqueElementData();

    bool shouldFoldCase = document().inQuirksMode();
    bool newStringHasClasses = isNonEmptyTokenList(newClassString);

    auto oldClassNames = elementData()->classNames();
    auto newClassNames = newStringHasClasses ? SpaceSplitString(newClassString, shouldFoldCase) : SpaceSplitString();
    {
        Style::ClassChangeInvalidation styleInvalidation(*this, oldClassNames, newClassNames);
        elementData()->setClassNames(newClassNames);
    }

    if (hasRareData()) {
        if (auto* classList = elementRareData()->classList())
            classList->associatedAttributeValueChanged(newClassString);
    }
}

void Element::partAttributeChanged(const AtomString& newValue)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().cssShadowPartsEnabled())
        return;

    bool hasParts = isNonEmptyTokenList(newValue);
    if (hasParts || !partNames().isEmpty()) {
        auto newParts = hasParts ? SpaceSplitString(newValue, false) : SpaceSplitString();
        ensureElementRareData().setPartNames(WTFMove(newParts));
    }

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

Style::ElementStyle Element::resolveStyle(const RenderStyle* parentStyle)
{
    return styleResolver().styleForElement(*this, parentStyle);
}

static void invalidateForSiblingCombinators(Element* sibling)
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
}

// Returns true is the given attribute is an event handler.
// We consider an event handler any attribute that begins with "on".
// It is a simple solution that has the advantage of not requiring any
// code or configuration change if a new event handler is defined.

bool Element::isEventHandlerAttribute(const Attribute& attribute) const
{
    return attribute.name().namespaceURI().isNull() && attribute.name().localName().startsWith("on");
}

bool Element::isJavaScriptURLAttribute(const Attribute& attribute) const
{
    return isURLAttribute(attribute) && WTF::protocolIsJavaScript(stripLeadingAndTrailingHTMLSpaces(attribute.value()));
}

void Element::stripScriptingAttributes(Vector<Attribute>& attributeVector) const
{
    attributeVector.removeAllMatching([this](auto& attribute) -> bool {
        return this->isEventHandlerAttribute(attribute)
            || this->isJavaScriptURLAttribute(attribute)
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
        // ElementData::m_classNames or ElementData::m_idForStyleResolution need to be updated with the right case.
        if (hasID())
            attributeChanged(idAttr, nullAtom(), getIdAttribute());
        if (hasClass())
            attributeChanged(classAttr, nullAtom(), getAttribute(classAttr));
    }

    if (UNLIKELY(isDefinedCustomElement()))
        CustomElementReactionQueue::enqueueAdoptedCallbackIfNeeded(*this, oldDocument, newDocument);

#if ENABLE(INTERSECTION_OBSERVER)
    if (auto* observerData = intersectionObserverDataIfExists()) {
        for (const auto& observer : observerData->observers) {
            if (observer->hasObservationTargets()) {
                oldDocument.removeIntersectionObserver(*observer);
                newDocument.addIntersectionObserver(*observer);
            }
        }
    }
#endif
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

#if ENABLE(FULLSCREEN_API)
    if (containsFullScreenElement() && parentElement() && !parentElement()->containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);
#endif

    if (parentNode() == &parentOfInsertedTree) {
        if (auto* shadowRoot = parentNode()->shadowRoot())
            shadowRoot->hostChildElementDidChange(*this);
    }

    if (!parentOfInsertedTree.isInTreeScope())
        return InsertedIntoAncestorResult::Done;

    bool becomeConnected = insertionType.connectedToDocument;
    TreeScope* newScope = &parentOfInsertedTree.treeScope();
    HTMLDocument* newDocument = becomeConnected && is<HTMLDocument>(newScope->documentScope()) ? &downcast<HTMLDocument>(newScope->documentScope()) : nullptr;
    if (!insertionType.treeScopeChanged)
        newScope = nullptr;

    const AtomString& idValue = getIdAttribute();
    if (!idValue.isNull()) {
        if (newScope)
            updateIdForTreeScope(*newScope, nullAtom(), idValue);
        if (newDocument)
            updateIdForDocument(*newDocument, nullAtom(), idValue, AlwaysUpdateHTMLDocumentNamedItemMaps);
    }

    const AtomString& nameValue = getNameAttribute();
    if (!nameValue.isNull()) {
        if (newScope)
            updateNameForTreeScope(*newScope, nullAtom(), nameValue);
        if (newDocument)
            updateNameForDocument(*newDocument, nullAtom(), nameValue);
    }

    if (newScope && hasTagName(labelTag)) {
        if (newScope->shouldCacheLabelsByForAttribute())
            updateLabel(*newScope, nullAtom(), attributeWithoutSynchronization(forAttr));
    }

    if (becomeConnected) {
        if (UNLIKELY(isCustomElementUpgradeCandidate())) {
            ASSERT(isConnected());
            CustomElementReactionQueue::tryToUpgradeElement(*this);
        }
        if (UNLIKELY(isDefinedCustomElement()))
            CustomElementReactionQueue::enqueueConnectedCallbackIfNeeded(*this);
    }

    if (UNLIKELY(hasTagName(articleTag) && newDocument))
        newDocument->registerArticleElement(*this);

    return InsertedIntoAncestorResult::Done;
}

void Element::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
#if ENABLE(FULLSCREEN_API)
    if (containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);
#endif
#if ENABLE(POINTER_LOCK)
    if (document().page())
        document().page()->pointerLockController().elementRemoved(*this);
#endif
    if (document().page())
        document().page()->pointerCaptureController().elementWasRemoved(*this);

    setSavedLayerScrollPosition(ScrollPosition());

    if (oldParentOfRemovedTree.isInTreeScope()) {
        TreeScope* oldScope = &oldParentOfRemovedTree.treeScope();
        Document* oldDocument = removalType.disconnectedFromDocument ? &oldScope->documentScope() : nullptr;
        HTMLDocument* oldHTMLDocument = oldDocument && is<HTMLDocument>(*oldDocument) ? &downcast<HTMLDocument>(*oldDocument) : nullptr;
        if (!removalType.treeScopeChanged)
            oldScope = nullptr;

        const AtomString& idValue = getIdAttribute();
        if (!idValue.isNull()) {
            if (oldScope)
                updateIdForTreeScope(*oldScope, idValue, nullAtom());
            if (oldHTMLDocument)
                updateIdForDocument(*oldHTMLDocument, idValue, nullAtom(), AlwaysUpdateHTMLDocumentNamedItemMaps);
        }

        const AtomString& nameValue = getNameAttribute();
        if (!nameValue.isNull()) {
            if (oldScope)
                updateNameForTreeScope(*oldScope, nameValue, nullAtom());
            if (oldHTMLDocument)
                updateNameForDocument(*oldHTMLDocument, nameValue, nullAtom());
        }

        if (oldScope && hasTagName(labelTag)) {
            if (oldScope->shouldCacheLabelsByForAttribute())
                updateLabel(*oldScope, attributeWithoutSynchronization(forAttr), nullAtom());
        }

        if (oldDocument) {
            if (oldDocument->cssTarget() == this)
                oldDocument->setCSSTarget(nullptr);
            if (UNLIKELY(hasTagName(articleTag)))
                oldDocument->unregisterArticleElement(*this);
        }

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

    if (hasPendingResources())
        document().accessSVGExtensions().removeElementFromPendingResources(*this);

    RefPtr<Frame> frame = document().frame();
    if (auto* timeline = document().existingTimeline())
        timeline->elementWasRemoved(*this);

    if (frame)
        frame->legacyAnimation().cancelAnimations(*this);

#if ENABLE(WHEEL_EVENT_LATCHING)
    if (frame && frame->page()) {
        if (auto* scrollLatchingController = frame->page()->scrollLatchingControllerIfExists())
            scrollLatchingController->removeLatchingStateForTarget(*this);
    }
#endif

    if (hasNodeFlag(NodeFlag::HasElementIdentifier)) {
        document().identifiedElementWasRemovedFromDocument(*this);
        clearNodeFlag(NodeFlag::HasElementIdentifier);
    }
}

ShadowRoot* Element::shadowRoot() const
{
    return hasRareData() ? elementRareData()->shadowRoot() : nullptr;
}

void Element::addShadowRoot(Ref<ShadowRoot>&& newShadowRoot)
{
    ASSERT(!newShadowRoot->hasChildNodes());
    ASSERT(!shadowRoot());

    ShadowRoot& shadowRoot = newShadowRoot;
    {
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        if (renderer())
            RenderTreeUpdater::tearDownRenderers(*this);

        ensureElementRareData().setShadowRoot(WTFMove(newShadowRoot));

        shadowRoot.setHost(makeWeakPtr<Element>(this));
        shadowRoot.setParentTreeScope(treeScope());

#if ASSERT_ENABLED
        ASSERT(notifyChildNodeInserted(*this, shadowRoot).isEmpty());
#else
        notifyChildNodeInserted(*this, shadowRoot);
#endif

        invalidateStyleAndRenderersForSubtree();
    }

    if (shadowRoot.mode() == ShadowRootMode::UserAgent)
        didAddUserAgentShadowRoot(shadowRoot);

    InspectorInstrumentation::didPushShadowRoot(*this, shadowRoot);
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
    static NeverDestroyed<HashSet<AtomString>> tagNames = [] {
        static const HTMLQualifiedName* const tagList[] = {
            &articleTag.get(),
            &asideTag.get(),
            &blockquoteTag.get(),
            &bodyTag.get(),
            &divTag.get(),
            &footerTag.get(),
            &h1Tag.get(),
            &h2Tag.get(),
            &h3Tag.get(),
            &h4Tag.get(),
            &h5Tag.get(),
            &h6Tag.get(),
            &headerTag.get(),
            &mainTag.get(),
            &navTag.get(),
            &pTag.get(),
            &sectionTag.get(),
            &spanTag.get()
        };
        HashSet<AtomString> set;
        for (auto& name : tagList)
            set.add(name->localName());
        return set;
    }();

    if (!is<HTMLElement>(element))
        return false;

    const auto& localName = element.localName();
    return tagNames.get().contains(localName) || Document::validateCustomElementName(localName) == CustomElementNameValidationStatus::Valid;
}

ExceptionOr<ShadowRoot&> Element::attachShadow(const ShadowRootInit& init)
{
    if (!canAttachAuthorShadowRoot(*this))
        return Exception { NotSupportedError };
    if (shadowRoot())
        return Exception { NotSupportedError };
    if (init.mode == ShadowRootMode::UserAgent)
        return Exception { TypeError };
    auto shadow = ShadowRoot::create(document(), init.mode, init.delegatesFocus ? ShadowRoot::DelegatesFocus::Yes : ShadowRoot::DelegatesFocus::No);
    auto& result = shadow.get();
    addShadowRoot(WTFMove(shadow));
    return result;
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
    setIsFailedCustomElementWithoutClearingReactionQueue();
    clearReactionQueueFromFailedCustomElement();
}

void Element::setIsFailedCustomElementWithoutClearingReactionQueue()
{
    ASSERT(isUndefinedCustomElement());
    ASSERT(customElementState() == CustomElementState::Undefined);
    setCustomElementState(CustomElementState::Failed);
    InspectorInstrumentation::didChangeCustomElementState(*this);
}

void Element::clearReactionQueueFromFailedCustomElement()
{
    ASSERT(isFailedCustomElement());
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
    ASSERT(!isDefinedCustomElement() && !isFailedCustomElement());
    auto& data = ensureElementRareData();
    bool alreadyScheduledToUpgrade = data.customElementReactionQueue();
    if (!alreadyScheduledToUpgrade)
        data.setCustomElementReactionQueue(makeUnique<CustomElementReactionQueue>(elementInterface));
    data.customElementReactionQueue()->enqueueElementUpgrade(*this, alreadyScheduledToUpgrade);
}

CustomElementReactionQueue* Element::reactionQueue() const
{
#if ASSERT_ENABLED
    if (isFailedCustomElement()) {
        auto* queue = elementRareData()->customElementReactionQueue();
        ASSERT(queue);
        ASSERT(queue->isEmpty() || queue->hasJustUpgradeReaction());
    } else
        ASSERT(isDefinedCustomElement() || isCustomElementUpgradeCandidate());
#endif
    if (!hasRareData())
        return nullptr;
    return elementRareData()->customElementReactionQueue();
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

static void checkForEmptyStyleChange(Element& element)
{
    if (element.styleAffectedByEmpty()) {
        auto* style = element.renderStyle();
        if (!style || (!style->emptyState() || element.hasChildNodes()))
            element.invalidateStyleForSubtree();
    }
}


static void invalidateForForwardPositionalRules(Element& parent, Element* elementAfterChange)
{
    bool childrenAffected = parent.childrenAffectedByForwardPositionalRules();
    bool descendantsAffected = parent.descendantsAffectedByForwardPositionalRules();

    if (!childrenAffected && !descendantsAffected)
        return;

    for (auto* sibling = elementAfterChange; sibling; sibling = sibling->nextElementSibling()) {
        if (childrenAffected)
            sibling->invalidateStyleInternal();
        if (descendantsAffected) {
            for (auto* siblingChild = sibling->firstElementChild(); siblingChild; siblingChild = siblingChild->nextElementSibling())
                siblingChild->invalidateStyleForSubtreeInternal();
        }
    }
}

static void invalidateForBackwardPositionalRules(Element& parent, Element* elementBeforeChange)
{
    bool childrenAffected = parent.childrenAffectedByBackwardPositionalRules();
    bool descendantsAffected = parent.descendantsAffectedByBackwardPositionalRules();

    if (!childrenAffected && !descendantsAffected)
        return;

    for (auto* sibling = elementBeforeChange; sibling; sibling = sibling->previousElementSibling()) {
        if (childrenAffected)
            sibling->invalidateStyleInternal();
        if (descendantsAffected) {
            for (auto* siblingChild = sibling->firstElementChild(); siblingChild; siblingChild = siblingChild->nextElementSibling())
                siblingChild->invalidateStyleForSubtreeInternal();
        }
    }
}

enum SiblingCheckType { FinishedParsingChildren, SiblingElementRemoved, Other };

static void checkForSiblingStyleChanges(Element& parent, SiblingCheckType checkType, Element* elementBeforeChange, Element* elementAfterChange)
{
    // :empty selector.
    checkForEmptyStyleChange(parent);

    if (parent.styleValidity() >= Style::Validity::SubtreeInvalid)
        return;

    // :first-child.  In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    // |afterChange| is 0 in the parser case, so it works out that we'll skip this block.
    if (parent.childrenAffectedByFirstChildRules() && elementAfterChange) {
        // Find our new first child.
        RefPtr<Element> newFirstElement = ElementTraversal::firstChild(parent);
        // Find the first element node following |afterChange|

        // This is the insert/append case.
        if (newFirstElement != elementAfterChange) {
            auto* style = elementAfterChange->renderStyle();
            if (!style || style->firstChildState())
                elementAfterChange->invalidateStyleForSubtreeInternal();
        }

        // We also have to handle node removal.
        if (checkType == SiblingElementRemoved && newFirstElement == elementAfterChange && newFirstElement) {
            auto* style = newFirstElement->renderStyle();
            if (!style || !style->firstChildState())
                newFirstElement->invalidateStyleForSubtreeInternal();
        }
    }

    // :last-child.  In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    if (parent.childrenAffectedByLastChildRules() && elementBeforeChange) {
        // Find our new last child.
        RefPtr<Element> newLastElement = ElementTraversal::lastChild(parent);

        if (newLastElement != elementBeforeChange) {
            auto* style = elementBeforeChange->renderStyle();
            if (!style || style->lastChildState())
                elementBeforeChange->invalidateStyleForSubtreeInternal();
        }

        // We also have to handle node removal.  The parser callback case is similar to node removal as well in that we need to change the last child
        // to match now.
        if ((checkType == SiblingElementRemoved || checkType == FinishedParsingChildren) && newLastElement == elementBeforeChange && newLastElement) {
            auto* style = newLastElement->renderStyle();
            if (!style || !style->lastChildState())
                newLastElement->invalidateStyleForSubtreeInternal();
        }
    }

    invalidateForSiblingCombinators(elementAfterChange);

    invalidateForForwardPositionalRules(parent, elementAfterChange);
    invalidateForBackwardPositionalRules(parent, elementBeforeChange);
}

void Element::childrenChanged(const ChildChange& change)
{
    ContainerNode::childrenChanged(change);
    if (change.source == ChildChangeSource::Parser)
        checkForEmptyStyleChange(*this);
    else {
        SiblingCheckType checkType = change.type == ElementRemoved ? SiblingElementRemoved : Other;
        checkForSiblingStyleChanges(*this, checkType, change.previousSiblingElement, change.nextSiblingElement);
    }

    if (ShadowRoot* shadowRoot = this->shadowRoot()) {
        switch (change.type) {
        case ElementInserted:
        case ElementRemoved:
            // For elements, we notify shadowRoot in Element::insertedIntoAncestor and Element::removedFromAncestor.
            break;
        case AllChildrenRemoved:
        case AllChildrenReplaced:
            shadowRoot->didRemoveAllChildrenOfShadowHost();
            break;
        case TextInserted:
        case TextRemoved:
        case TextChanged:
            shadowRoot->didChangeDefaultSlot();
            break;
        case NonContentsChildInserted:
        case NonContentsChildRemoved:
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
    checkForSiblingStyleChanges(*this, FinishedParsingChildren, ElementTraversal::lastChild(*this), nullptr);
}

String Element::debugDescription() const
{
    StringBuilder builder;

    builder.append(ContainerNode::debugDescription());

    if (hasID())
        builder.append(" id=\'", getIdAttribute(), '\'');

    if (hasClass()) {
        builder.appendLiteral(" class=\'");
        size_t classNamesToDump = classNames().size();
        constexpr size_t maxNumClassNames = 7;
        bool addEllipsis = false;
        if (classNamesToDump > maxNumClassNames) {
            classNamesToDump = maxNumClassNames;
            addEllipsis = true;
        }
        
        for (size_t i = 0; i < classNamesToDump; ++i) {
            if (i > 0)
                builder.append(' ');
            builder.append(classNames()[i]);
        }
        if (addEllipsis)
            builder.append(" ...");
        builder.append('\'');
    }

    return builder.toString();
}

#if ENABLE(TREE_DEBUGGING)

void Element::formatForDebugger(char* buffer, unsigned length) const
{
    StringBuilder result;
    String s;

    result.append(nodeName());

    s = getIdAttribute();
    if (s.length() > 0) {
        if (result.length() > 0)
            result.appendLiteral("; ");
        result.appendLiteral("id=");
        result.append(s);
    }

    s = getAttribute(classAttr);
    if (s.length() > 0) {
        if (result.length() > 0)
            result.appendLiteral("; ");
        result.appendLiteral("class=");
        result.append(s);
    }

    strncpy(buffer, result.toString().utf8().data(), length - 1);
}

#endif

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
        return 0;
    QualifiedName qName(nullAtom(), localName, namespaceURI);
    synchronizeAttribute(qName);
    const Attribute* attribute = elementData()->findAttributeByName(qName);
    if (!attribute)
        return 0;
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

static RefPtr<Element> findFirstProgramaticallyFocusableElementInComposedTree(Element& host)
{
    ASSERT(host.shadowRoot());
    for (auto& node : composedTreeDescendants(host)) {
        if (!is<Element>(node))
            continue;
        auto& element = downcast<Element>(node);
        if (isProgramaticallyFocusable(element))
            return &element;
    }
    return nullptr;
}

void Element::focus(bool restorePreviousSelection, FocusDirection direction)
{
    if (!isConnected())
        return;

    auto document = makeRef(this->document());
    if (document->focusedElement() == this) {
        if (document->page())
            document->page()->chrome().client().elementDidRefocus(*this);
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
        auto currentlyFocusedElement = makeRefPtr(document->focusedElement());
        if (root->containsIncludingShadowDOM(currentlyFocusedElement.get())) {
            if (document->page())
                document->page()->chrome().client().elementDidRefocus(*currentlyFocusedElement);
            return;
        }

        newTarget = findFirstProgramaticallyFocusableElementInComposedTree(*this);            
        if (!newTarget)
            return;
    } else if (!isProgramaticallyFocusable(*newTarget))
        return;

    if (Page* page = document->page()) {
        auto& frame = *document->frame();
        if (!frame.hasHadUserInteraction() && !frame.isMainFrame() && !document->topDocument().securityOrigin().canAccess(document->securityOrigin()))
            return;

        // Focus and change event handlers can cause us to lose our last ref.
        // If a focus event handler changes the focus to a different node it
        // does not make sense to continue and update appearence.
        if (!page->focusController().setFocusedElement(newTarget.get(), *document->frame(), direction))
            return;
    }

    newTarget->revealFocusedElement(restorePreviousSelection ? SelectionRestorationMode::Restore : SelectionRestorationMode::SetDefault);
}

void Element::revealFocusedElement(SelectionRestorationMode selectionMode)
{
    SelectionRevealMode revealMode = SelectionRevealMode::Reveal;

#if PLATFORM(IOS_FAMILY)
    // Focusing a form element triggers animation in UIKit to scroll to the right position.
    // Calling updateFocusAppearance() would generate an unnecessary call to ScrollView::setScrollPosition(),
    // which would jump us around during this animation. See <rdar://problem/6699741>.
    if (is<HTMLFormControlElement>(*this))
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
        if (Frame* frame = document().frame())
            frame->page()->focusController().setFocusedElement(nullptr, *frame);
        else
            document().setFocusedElement(nullptr);
    }
}

void Element::dispatchFocusInEvent(const AtomString& eventType, RefPtr<Element>&& oldFocusedElement)
{
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    ASSERT(eventType == eventNames().focusinEvent || eventType == eventNames().DOMFocusInEvent);
    dispatchScopedEvent(FocusEvent::create(eventType, Event::CanBubble::Yes, Event::IsCancelable::No, document().windowProxy(), 0, WTFMove(oldFocusedElement)));
}

void Element::dispatchFocusOutEvent(const AtomString& eventType, RefPtr<Element>&& newFocusedElement)
{
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    ASSERT(eventType == eventNames().focusoutEvent || eventType == eventNames().DOMFocusOutEvent);
    dispatchScopedEvent(FocusEvent::create(eventType, Event::CanBubble::Yes, Event::IsCancelable::No, document().windowProxy(), 0, WTFMove(newFocusedElement)));
}

void Element::dispatchFocusEvent(RefPtr<Element>&& oldFocusedElement, FocusDirection)
{
    if (auto* page = document().page())
        page->chrome().client().elementDidFocus(*this);
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
        dispatchEvent(Event::create("webkitImageFrameReady", Event::CanBubble::Yes, Event::IsCancelable::Yes));
}

bool Element::dispatchMouseForceWillBegin()
{
#if ENABLE(MOUSE_FORCE_EVENTS)
    if (!document().hasListenerType(Document::FORCEWILLBEGIN_LISTENER))
        return false;

    Frame* frame = document().frame();
    if (!frame)
        return false;

    PlatformMouseEvent platformMouseEvent { frame->eventHandler().lastKnownMousePosition(), frame->eventHandler().lastKnownMouseGlobalPosition(), NoButton, PlatformEvent::NoType, 1, false, false, false, false, WallTime::now(), ForceAtClick, NoTap };
    auto mouseForceWillBeginEvent = MouseEvent::create(eventNames().webkitmouseforcewillbeginEvent, document().windowProxy(), platformMouseEvent, 0, nullptr);
    mouseForceWillBeginEvent->setTarget(this);
    dispatchEvent(mouseForceWillBeginEvent);

    if (mouseForceWillBeginEvent->defaultHandled() || mouseForceWillBeginEvent->defaultPrevented())
        return true;
#endif

    return false;
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
    return serializeFragment(*this, SerializedNodes::SubtreesOfChildren);
}

String Element::outerHTML() const
{
    return serializeFragment(*this, SerializedNodes::SubtreeIncludingNode);
}

ExceptionOr<void> Element::setOuterHTML(const String& html)
{
    auto* parentElement = this->parentElement();
    if (!is<HTMLElement>(parentElement))
        return Exception { NoModificationAllowedError };

    Ref<HTMLElement> parent = downcast<HTMLElement>(*parentElement);
    RefPtr<Node> prev = previousSibling();
    RefPtr<Node> next = nextSibling();

    auto fragment = createFragmentForInnerOuterHTML(parent, html, AllowScriptingContent);
    if (fragment.hasException())
        return fragment.releaseException();

    auto replaceResult = parent->replaceChild(fragment.releaseReturnValue().get(), *this);
    if (replaceResult.hasException())
        return replaceResult.releaseException();

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
    auto fragment = createFragmentForInnerOuterHTML(*this, html, AllowScriptingContent);
    if (fragment.hasException())
        return fragment.releaseException();

    ContainerNode* container;
    if (!is<HTMLTemplateElement>(*this))
        container = this;
    else
        container = &downcast<HTMLTemplateElement>(*this).content();

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

LayoutSize Element::minimumSizeForResizing() const
{
    return hasRareData() ? elementRareData()->minimumSizeForResizing() : defaultMinimumSizeForResizing();
}

void Element::setMinimumSizeForResizing(const LayoutSize& size)
{
    if (!hasRareData() && size == defaultMinimumSizeForResizing())
        return;
    ensureElementRareData().setMinimumSizeForResizing(size);
}

void Element::willBecomeFullscreenElement()
{
    for (auto& child : descendantsOfType<Element>(*this))
        child.ancestorWillEnterFullscreen();
}

static PseudoElement* beforeOrAfterPseudoElement(Element& host, PseudoId pseudoElementSpecifier)
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

const RenderStyle* Element::renderOrDisplayContentsStyle() const
{
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
    ASSERT(!existingComputedStyle());

    Deque<RefPtr<Element>, 32> elementsRequiringComputedStyle({ this });
    const RenderStyle* computedStyle = nullptr;

    // Collect ancestors until we find one that has style.
    for (auto& ancestor : composedTreeAncestors(*this)) {
        if (auto* existingStyle = ancestor.existingComputedStyle()) {
            computedStyle = existingStyle;
            break;
        }
        elementsRequiringComputedStyle.prepend(&ancestor);
    }

    // Resolve and cache styles starting from the most distant ancestor.
    for (auto& element : elementsRequiringComputedStyle) {
        auto style = document().styleForElementIgnoringPendingStylesheets(*element, computedStyle);
        computedStyle = style.get();
        ElementRareData& rareData = element->ensureElementRareData();
        rareData.setComputedStyle(WTFMove(style));

        if (mode == ResolveComputedStyleMode::RenderedOnly && computedStyle->display() == DisplayType::None)
            return nullptr;
    }

    return computedStyle;
}

bool Element::hasValidStyle() const
{
    if (!document().needsStyleRecalc())
        return true;

    if (document().hasPendingFullStyleRebuild())
        return false;
    
    for (auto& element : lineageOfType<Element>(*this)) {
        if (element.styleValidity() != Style::Validity::Valid)
            return false;
    }
    return true;
}

bool Element::isVisibleWithoutResolvingFullStyle() const
{
    if (renderStyle() || hasValidStyle())
        return renderStyle() && renderStyle()->visibility() == Visibility::Visible;

    // Compute style in yet unstyled subtree.
    auto* style = existingComputedStyle();
    if (!style)
        style = const_cast<Element&>(*this).resolveComputedStyle(ResolveComputedStyleMode::RenderedOnly);

    if (!style)
        return false;

    if (style->display() == DisplayType::None || style->display() == DisplayType::Contents)
        return false;

    if (style->visibility() != Visibility::Visible)
        return false;

    for (auto& element : composedTreeAncestors(const_cast<Element&>(*this))) {
        auto* style = element.existingComputedStyle();
        if (!style)
            style = element.resolveComputedStyle(ResolveComputedStyleMode::RenderedOnly);
        if (!style || style->display() == DisplayType::None)
            return false;
    }

    return true;
}

const RenderStyle& Element::resolvePseudoElementStyle(PseudoId pseudoElementSpecifier)
{
    ASSERT(!isPseudoElement());

    auto* parentStyle = existingComputedStyle();
    ASSERT(parentStyle);
    ASSERT(!parentStyle->getCachedPseudoStyle(pseudoElementSpecifier));

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

bool Element::needsStyleInvalidation() const
{
    if (!inRenderedDocument())
        return false;
    if (styleValidity() >= Style::Validity::SubtreeInvalid)
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

AtomString Element::computeInheritedLanguage() const
{
    // The language property is inherited, so we iterate over the parents to find the first language.
    for (auto& element : lineageOfType<Element>(*this)) {
        if (auto* elementData = element.elementData()) {
            if (auto* attribute = elementData->findLanguageAttribute())
                return attribute->value();
        }
    }
    return document().contentLanguage();
}

Locale& Element::locale() const
{
    return document().getCachedLocale(computeInheritedLanguage());
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

void Element::clearBeforePseudoElement()
{
    if (!hasRareData())
        return;
    disconnectPseudoElement(elementRareData()->beforePseudoElement());
    elementRareData()->setBeforePseudoElement(nullptr);
}

void Element::clearAfterPseudoElement()
{
    if (!hasRareData())
        return;
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
    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(getAttribute(name)));
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

static Element* parentCrossingFrameBoundaries(const Element* element)
{
    ASSERT(element);
    if (auto* parent = element->parentElementInComposedTree())
        return parent;
    return element->document().ownerElement();
}

void Element::webkitRequestFullscreen()
{
    document().fullscreenManager().requestFullscreenForElement(this, FullscreenManager::EnforceIFrameAllowFullscreenRequirement);
}

void Element::setContainsFullScreenElement(bool flag)
{
    if (flag)
        setNodeFlag(NodeFlag::ContainsFullScreenElement);
    else
        clearNodeFlag(NodeFlag::ContainsFullScreenElement);
    invalidateStyleAndLayerComposition();
}

void Element::setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(bool flag)
{
    Element* element = this;
    while ((element = parentCrossingFrameBoundaries(element)))
        element->setContainsFullScreenElement(flag);
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

#if ENABLE(INTERSECTION_OBSERVER)

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

#endif

ElementAnimationRareData* Element::animationRareData() const
{
    return hasRareData() ? elementRareData()->elementAnimationRareData() : nullptr;
}

ElementAnimationRareData& Element::ensureAnimationRareData()
{
    return ensureElementRareData().ensureAnimationRareData();
}

KeyframeEffectStack* Element::keyframeEffectStack() const
{
    if (auto* animationData = animationRareData())
        return animationData->keyframeEffectStack();
    return nullptr;
}

KeyframeEffectStack& Element::ensureKeyframeEffectStack()
{
    return ensureAnimationRareData().ensureKeyframeEffectStack();
}

bool Element::hasKeyframeEffects() const
{
    if (auto* animationData = animationRareData()) {
        if (auto* keyframeEffectStack = animationData->keyframeEffectStack())
            return keyframeEffectStack->hasEffects();
    }
    return false;
}

OptionSet<AnimationImpact> Element::applyKeyframeEffects(RenderStyle& targetStyle)
{
    OptionSet<AnimationImpact> impact;

    for (const auto& effect : ensureKeyframeEffectStack().sortedEffects()) {
        ASSERT(effect->animation());
        effect->animation()->resolve(targetStyle);

        if (effect->isRunningAccelerated() || effect->isAboutToRunAccelerated())
            impact.add(AnimationImpact::RequiresRecomposite);

        if (effect->triggersStackingContext())
            impact.add(AnimationImpact::ForcesStackingContext);
    }

    return impact;
}

const AnimationCollection* Element::webAnimations() const
{
    if (auto* animationData = animationRareData())
        return &animationData->webAnimations();
    return nullptr;
}

const AnimationCollection* Element::cssAnimations() const
{
    if (auto* animationData = animationRareData())
        return &animationData->cssAnimations();
    return nullptr;
}

const AnimationCollection* Element::transitions() const
{
    if (auto* animationData = animationRareData())
        return &animationData->transitions();
    return nullptr;
}

bool Element::hasCompletedTransitionsForProperty(CSSPropertyID property) const
{
    if (auto* animationData = animationRareData())
        return animationData->completedTransitionsByProperty().contains(property);
    return false;
}

bool Element::hasRunningTransitionsForProperty(CSSPropertyID property) const
{
    if (auto* animationData = animationRareData())
        return animationData->runningTransitionsByProperty().contains(property);
    return false;
}

bool Element::hasRunningTransitions() const
{
    if (auto* animationData = animationRareData())
        return !animationData->runningTransitionsByProperty().isEmpty();
    return false;
}

AnimationCollection& Element::ensureWebAnimations()
{
    return ensureAnimationRareData().webAnimations();
}

AnimationCollection& Element::ensureCSSAnimations()
{
    return ensureAnimationRareData().cssAnimations();
}

AnimationCollection& Element::ensureTransitions()
{
    return ensureAnimationRareData().transitions();
}

CSSAnimationCollection& Element::animationsCreatedByMarkup()
{
    return ensureAnimationRareData().animationsCreatedByMarkup();
}

void Element::setAnimationsCreatedByMarkup(CSSAnimationCollection&& animations)
{
    ensureAnimationRareData().setAnimationsCreatedByMarkup(WTFMove(animations));
}

PropertyToTransitionMap& Element::ensureCompletedTransitionsByProperty()
{
    return ensureAnimationRareData().completedTransitionsByProperty();
}

PropertyToTransitionMap& Element::ensureRunningTransitionsByProperty()
{
    return ensureAnimationRareData().runningTransitionsByProperty();
}

const RenderStyle* Element::lastStyleChangeEventStyle() const
{
    if (auto* animationData = animationRareData())
        return animationData->lastStyleChangeEventStyle();
    return nullptr;
}

void Element::setLastStyleChangeEventStyle(std::unique_ptr<const RenderStyle>&& style)
{
    if (auto* animationData = animationRareData())
        animationData->setLastStyleChangeEventStyle(WTFMove(style));
    else if (style)
        ensureAnimationRareData().setLastStyleChangeEventStyle(WTFMove(style));
}

#if ENABLE(RESIZE_OBSERVER)

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

#endif

bool Element::isSpellCheckingEnabled() const
{
    for (auto* ancestor = this; ancestor; ancestor = ancestor->parentOrShadowHostElement()) {
        auto& value = ancestor->attributeWithoutSynchronization(HTMLNames::spellcheckAttr);
        if (value.isNull())
            continue;
        if (value.isEmpty() || equalLettersIgnoringASCIICase(value, "true"))
            return true;
        if (equalLettersIgnoringASCIICase(value, "false"))
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
    InspectorInstrumentation::didModifyDOMAttr(document(), *this, name.toString(), value);
    dispatchSubtreeModifiedEvent();
}

void Element::didModifyAttribute(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue)
{
    attributeChanged(name, oldValue, newValue);
    InspectorInstrumentation::didModifyDOMAttr(document(), *this, name.toString(), newValue);
    // Do not dispatch a DOMSubtreeModified event here; see bug 81141.
}

void Element::didRemoveAttribute(const QualifiedName& name, const AtomString& oldValue)
{
    attributeChanged(name, oldValue, nullAtom());
    InspectorInstrumentation::didRemoveDOMAttr(document(), *this, name.toString());
    dispatchSubtreeModifiedEvent();
}

IntPoint Element::savedLayerScrollPosition() const
{
    return hasRareData() ? elementRareData()->savedLayerScrollPosition() : IntPoint();
}

void Element::setSavedLayerScrollPosition(const IntPoint& position)
{
    if (position.isZero() && !hasRareData())
        return;
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
        if (!element.hasRareData() || !element.elementRareData()->computedStyle())
            return;
        if (element.hasCustomStyleResolveCallbacks())
            element.willResetComputedStyle();
        element.elementRareData()->resetComputedStyle();
    };
    reset(*this);
    for (auto& child : descendantsOfType<Element>(*this))
        reset(child);
}

void Element::resetStyleRelations()
{
    // FIXME: Make this code more consistent.
    auto bitfields = styleBitfields();
    bitfields.clearDynamicStyleRelations();
    setStyleBitfields(bitfields);
    if (!hasRareData())
        return;
    elementRareData()->resetStyleRelations();
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

Optional<Style::ElementStyle> Element::resolveCustomStyle(const RenderStyle&, const RenderStyle*)
{
    ASSERT(hasCustomStyleResolveCallbacks());
    return WTF::nullopt;
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
        && !other.m_elementData->presentationAttributeStyle()
        && (!other.m_elementData->inlineStyle() || !other.m_elementData->inlineStyle()->hasCSSOMWrapper()))
        const_cast<Element&>(other).m_elementData = downcast<UniqueElementData>(*other.m_elementData).makeShareableCopy();

    if (!other.m_elementData->isUnique())
        m_elementData = other.m_elementData;
    else
        m_elementData = other.m_elementData->makeUniqueCopy();

    for (const Attribute& attribute : attributesIterator())
        attributeChanged(attribute.name(), nullAtom(), attribute.value(), ModifiedByCloning);
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
    return !equalLettersIgnoringASCIICase(attributeWithoutSynchronization(roleAttr), "img");
}

String Element::completeURLsInAttributeValue(const URL& base, const Attribute& attribute) const
{
    return URL(base, attribute.value()).string();
}

ExceptionOr<Node*> Element::insertAdjacent(const String& where, Ref<Node>&& newChild)
{
    // In Internet Explorer if the element has no parent and where is "beforeBegin" or "afterEnd",
    // a document fragment is created and the elements appended in the correct order. This document
    // fragment isn't returned anywhere.
    //
    // This is impossible for us to implement as the DOM tree does not allow for such structures,
    // Opera also appears to disallow such usage.

    if (equalLettersIgnoringASCIICase(where, "beforebegin")) {
        auto* parent = this->parentNode();
        if (!parent)
            return nullptr;
        auto result = parent->insertBefore(newChild, this);
        if (result.hasException())
            return result.releaseException();
        return newChild.ptr();
    }

    if (equalLettersIgnoringASCIICase(where, "afterbegin")) {
        auto result = insertBefore(newChild, firstChild());
        if (result.hasException())
            return result.releaseException();
        return newChild.ptr();
    }

    if (equalLettersIgnoringASCIICase(where, "beforeend")) {
        auto result = appendChild(newChild);
        if (result.hasException())
            return result.releaseException();
        return newChild.ptr();
    }

    if (equalLettersIgnoringASCIICase(where, "afterend")) {
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
    if (equalLettersIgnoringASCIICase(where, "beforebegin") || equalLettersIgnoringASCIICase(where, "afterend")) {
        auto* parent = element.parentNode();
        if (!parent || is<Document>(*parent))
            return Exception { NoModificationAllowedError };
        return *parent;
    }
    if (equalLettersIgnoringASCIICase(where, "afterbegin") || equalLettersIgnoringASCIICase(where, "beforeend"))
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
    auto fragment = createFragmentForInnerOuterHTML(contextElement.releaseReturnValue(), markup, AllowScriptingContent);
    if (fragment.hasException())
        return fragment.releaseException();

    if (UNLIKELY(addedNodes)) {
        // Must be called before insertAdjacent, as otherwise the children of fragment will be moved
        // to their new parent and will be harder to keep track of.
        *addedNodes = collectChildNodes(fragment.returnValue());
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

ExceptionOr<void> Element::insertAdjacentText(const String& where, const String& text)
{
    auto result = insertAdjacent(where, document().createTextNode(text));
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

ExceptionOr<Ref<WebAnimation>> Element::animate(JSC::JSGlobalObject& lexicalGlobalObject, JSC::Strong<JSC::JSObject>&& keyframes, Optional<Variant<double, KeyframeAnimationOptions>>&& options)
{
    String id = "";
    Optional<Variant<double, KeyframeEffectOptions>> keyframeEffectOptions;
    if (options) {
        auto optionsValue = options.value();
        Variant<double, KeyframeEffectOptions> keyframeEffectOptionsVariant;
        if (WTF::holds_alternative<double>(optionsValue))
            keyframeEffectOptionsVariant = WTF::get<double>(optionsValue);
        else {
            auto keyframeEffectOptions = WTF::get<KeyframeAnimationOptions>(optionsValue);
            id = keyframeEffectOptions.id;
            keyframeEffectOptionsVariant = WTFMove(keyframeEffectOptions);
        }
        keyframeEffectOptions = keyframeEffectOptionsVariant;
    }

    auto keyframeEffectResult = KeyframeEffect::create(lexicalGlobalObject, this, WTFMove(keyframes), WTFMove(keyframeEffectOptions));
    if (keyframeEffectResult.hasException())
        return keyframeEffectResult.releaseException();

    auto animation = WebAnimation::create(document(), &keyframeEffectResult.returnValue().get());
    animation->setId(id);

    auto animationPlayResult = animation->play();
    if (animationPlayResult.hasException())
        return animationPlayResult.releaseException();

    return animation;
}

Vector<RefPtr<WebAnimation>> Element::getAnimations(Optional<GetAnimationsOptions> options)
{
    // If we are to return animations in the subtree, we can get all of the document's animations and filter
    // animations targeting that are not registered on this element, one of its pseudo elements or a child's
    // pseudo element.
    if (options && options->subtree) {
        return document().matchingAnimations([&] (Element& target) -> bool {
            if (is<PseudoElement>(target))
                return contains(downcast<PseudoElement>(target).hostElement());
            return contains(&target);
        });
    }

    // For the list of animations to be current, we need to account for any pending CSS changes,
    // such as updates to CSS Animations and CSS Transitions.
    // FIXME: We might be able to use ComputedStyleExtractor which is more optimized.
    document().updateStyleIfNeeded();

    Vector<RefPtr<WebAnimation>> animations;
    if (auto timeline = document().existingTimeline()) {
        for (auto& animation : timeline->animationsForElement(*this, AnimationTimeline::Ordering::Sorted)) {
            if (animation->isRelevant())
                animations.append(animation);
        }
    }
    return animations;
}

ElementIdentifier Element::createElementIdentifier()
{
    ASSERT(!hasNodeFlag(NodeFlag::HasElementIdentifier));
    setNodeFlag(NodeFlag::HasElementIdentifier);
    return ElementIdentifier::generate();
}

#if ENABLE(CSS_TYPED_OM)

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

#endif

} // namespace WebCore
