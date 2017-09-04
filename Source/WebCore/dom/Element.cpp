/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClassChangeInvalidation.h"
#include "ComposedTreeAncestorIterator.h"
#include "ContainerNodeAlgorithms.h"
#include "CustomElementReactionQueue.h"
#include "CustomElementRegistry.h"
#include "DOMRect.h"
#include "DOMRectList.h"
#include "DOMTokenList.h"
#include "DocumentAnimation.h"
#include "DocumentSharedObjectPool.h"
#include "Editing.h"
#include "ElementIterator.h"
#include "ElementRareData.h"
#include "EventDispatcher.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FlowThreadController.h"
#include "FocusController.h"
#include "FocusEvent.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLCollection.h"
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
#include "KeyframeEffect.h"
#include "MainFrame.h"
#include "MutationObserverInterestGroup.h"
#include "MutationRecord.h"
#include "NoEventDispatchAssertion.h"
#include "NodeRenderStyle.h"
#include "PlatformWheelEvent.h"
#include "PointerLockController.h"
#include "RenderFlowThread.h"
#include "RenderLayer.h"
#include "RenderListBox.h"
#include "RenderNamedFlowFragment.h"
#include "RenderRegion.h"
#include "RenderTheme.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "ScrollLatchingState.h"
#include "SelectorQuery.h"
#include "Settings.h"
#include "SimulatedClick.h"
#include "SlotAssignment.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleTreeResolver.h"
#include "TextIterator.h"
#include "VoidCallback.h"
#include "WheelEvent.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "markup.h"
#include <wtf/CurrentTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

namespace WebCore {

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

static Attr* findAttrNodeInList(Vector<RefPtr<Attr>>& attrNodeList, const AtomicString& localName, bool shouldIgnoreAttributeCase)
{
    const AtomicString& caseAdjustedName = shouldIgnoreAttributeCase ? localName.convertToASCIILowercase() : localName;
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
#ifndef NDEBUG
    if (document().hasLivingRenderTree()) {
        // When the document is not destroyed, an element that was part of a named flow
        // content nodes should have been removed from the content nodes collection
        // and the isNamedFlowContentElement flag reset.
        ASSERT_WITH_SECURITY_IMPLICATION(!isNamedFlowContentElement());
    }
#endif

    ASSERT(!beforePseudoElement());
    ASSERT(!afterPseudoElement());

    removeShadowRoot();

    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

    if (hasPendingResources()) {
        document().accessSVGExtensions().removeElementFromPendingResources(this);
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

void Element::clearTabIndexExplicitlyIfNeeded()
{
    if (hasRareData())
        elementRareData()->clearTabIndexExplicitly();
}

void Element::setTabIndexExplicitly(int tabIndex)
{
    ensureElementRareData().setTabIndexExplicitly(tabIndex);
}

bool Element::tabIndexSetExplicitly() const
{
    return hasRareData() && elementRareData()->tabIndexSetExplicitly();
}

bool Element::supportsFocus() const
{
    return tabIndexSetExplicitly();
}

Element* Element::focusDelegate()
{
    return this;
}

int Element::tabIndex() const
{
    return hasRareData() ? elementRareData()->tabIndex() : 0;
}

void Element::setTabIndex(int value)
{
    setIntegralAttribute(tabindexAttr, value);
}

bool Element::isKeyboardFocusable(KeyboardEvent&) const
{
    return isFocusable() && tabIndex() >= 0;
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

bool Element::dispatchMouseEvent(const PlatformMouseEvent& platformEvent, const AtomicString& eventType, int detail, Element* relatedTarget)
{
    if (isDisabledFormControl())
        return false;

    if (isForceEvent(platformEvent) && !document().hasListenerTypeForEventType(platformEvent.type()))
        return false;

    Ref<MouseEvent> mouseEvent = MouseEvent::create(eventType, document().defaultView(), platformEvent, detail, relatedTarget);

    if (mouseEvent->type().isEmpty())
        return true; // Shouldn't happen.

    ASSERT(!mouseEvent->target() || mouseEvent->target() != relatedTarget);
    bool didNotSwallowEvent = dispatchEvent(mouseEvent) && !mouseEvent->defaultHandled();

    if (mouseEvent->type() == eventNames().clickEvent && mouseEvent->detail() == 2) {
        // Special case: If it's a double click event, we also send the dblclick event. This is not part
        // of the DOM specs, but is used for compatibility with the ondblclick="" attribute. This is treated
        // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
        Ref<MouseEvent> doubleClickEvent = MouseEvent::create(eventNames().dblclickEvent,
            mouseEvent->bubbles(), mouseEvent->cancelable(), mouseEvent->view(), mouseEvent->detail(),
            mouseEvent->screenX(), mouseEvent->screenY(), mouseEvent->clientX(), mouseEvent->clientY(),
            mouseEvent->ctrlKey(), mouseEvent->altKey(), mouseEvent->shiftKey(), mouseEvent->metaKey(),
            mouseEvent->button(), mouseEvent->syntheticClickType(), relatedTarget);

        if (mouseEvent->defaultHandled())
            doubleClickEvent->setDefaultHandled();

        dispatchEvent(doubleClickEvent);
        if (doubleClickEvent->defaultHandled() || doubleClickEvent->defaultPrevented())
            return false;
    }
    return didNotSwallowEvent;
}


bool Element::dispatchWheelEvent(const PlatformWheelEvent& event)
{
    Ref<WheelEvent> wheelEvent = WheelEvent::create(event, document().defaultView());

    // Events with no deltas are important because they convey platform information about scroll gestures
    // and momentum beginning or ending. However, those events should not be sent to the DOM since some
    // websites will break. They need to be dispatched because dispatching them will call into the default
    // event handler, and our platform code will correctly handle the phase changes. Calling stopPropogation()
    // will prevent the event from being sent to the DOM, but will still call the default event handler.
    if (!event.deltaX() && !event.deltaY())
        wheelEvent->stopPropagation();

    return EventDispatcher::dispatchEvent(*this, wheelEvent) && !wheelEvent->defaultHandled();
}

bool Element::dispatchKeyEvent(const PlatformKeyboardEvent& platformEvent)
{
    Ref<KeyboardEvent> event = KeyboardEvent::create(platformEvent, document().defaultView());
    if (Frame* frame = document().frame()) {
        if (frame->eventHandler().accessibilityPreventsEventPropogation(event))
            event->stopPropagation();
    }
    return EventDispatcher::dispatchEvent(*this, event) && !event->defaultHandled();
}

void Element::dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions eventOptions, SimulatedClickVisualOptions visualOptions)
{
    simulateClick(*this, underlyingEvent, eventOptions, visualOptions, SimulatedClickSource::UserAgent);
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

    rareData.setAttributeMap(std::make_unique<NamedNodeMap>(const_cast<Element&>(*this)));
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

    if (elementData()->animatedSVGAttributesAreDirty()) {
        ASSERT(isSVGElement());
        downcast<SVGElement>(*this).synchronizeAnimatedSVGAttribute(anyQName());
    }
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

    if (UNLIKELY(elementData()->animatedSVGAttributesAreDirty())) {
        ASSERT(isSVGElement());
        downcast<SVGElement>(*this).synchronizeAnimatedSVGAttribute(name);
    }
}

static ALWAYS_INLINE bool isStyleAttribute(const Element& element, const AtomicString& attributeLocalName)
{
    if (shouldIgnoreAttributeCase(element))
        return equalLettersIgnoringASCIICase(attributeLocalName, "style");
    return attributeLocalName == styleAttr.localName();
}

ALWAYS_INLINE void Element::synchronizeAttribute(const AtomicString& localName) const
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
    if (elementData()->animatedSVGAttributesAreDirty()) {
        // We're not passing a namespace argument on purpose. SVGNames::*Attr are defined w/o namespaces as well.
        ASSERT_WITH_SECURITY_IMPLICATION(isSVGElement());
        downcast<SVGElement>(*this).synchronizeAnimatedSVGAttribute(QualifiedName(nullAtom(), localName, nullAtom()));
    }
}

const AtomicString& Element::getAttribute(const QualifiedName& name) const
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
        // If the node is in a display:none tree it might say it needs style recalc but
        // the whole document is actually up to date.
        ASSERT(!needsStyleRecalc() || !document().childNeedsStyleRecalc());

        // Elements in canvas fallback content are not rendered, but they are allowed to be
        // focusable as long as their canvas is displayed and visible.
        if (auto* canvas = ancestorsOfType<HTMLCanvasElement>(*this).first())
            return canvas->renderer() && canvas->renderer()->style().visibility() == VISIBLE;
    }

    // FIXME: Even if we are not visible, we might have a child that is visible.
    // Hyatt wants to fix that some day with a "has visible content" flag or the like.
    if (!renderer() || renderer()->style().visibility() != VISIBLE)
        return false;

    return true;
}

bool Element::isUserActionElementInActiveChain() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isInActiveChain(this);
}

bool Element::isUserActionElementActive() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isActive(this);
}

bool Element::isUserActionElementFocused() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isFocused(this);
}

bool Element::isUserActionElementHovered() const
{
    ASSERT(isUserActionElement());
    return document().userActionElements().isHovered(this);
}

void Element::setActive(bool flag, bool pause)
{
    if (flag == active())
        return;

    document().userActionElements().setActive(this, flag);

    const RenderStyle* renderStyle = this->renderStyle();
    bool reactsToPress = (renderStyle && renderStyle->affectedByActive()) || styleAffectedByActive();
    if (reactsToPress)
        invalidateStyleForSubtree();

    if (!renderer())
        return;

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
        double startTime = monotonicallyIncreasingTime();
#endif

        document().updateStyleIfNeeded();

        // Do an immediate repaint.
        if (renderer())
            renderer()->repaint();

        // FIXME: Come up with a less ridiculous way of doing this.
#ifdef HAVE_FUNC_USLEEP
        // Now pause for a small amount of time (1/10th of a second from before we repainted in the pressed state)
        double remainingTime = 0.1 - (monotonicallyIncreasingTime() - startTime);
        if (remainingTime > 0)
            usleep(static_cast<useconds_t>(remainingTime * 1000000.0));
#endif
    }
}

void Element::setFocus(bool flag)
{
    if (flag == focused())
        return;

    document().userActionElements().setFocused(this, flag);
    invalidateStyleForSubtree();

    for (Element* element = this; element; element = element->parentElementInComposedTree())
        element->setHasFocusWithin(flag);
}

void Element::setHovered(bool flag)
{
    if (flag == hovered())
        return;

    document().userActionElements().setHovered(this, flag);

    if (!renderer()) {
        // When setting hover to false, the style needs to be recalc'd even when
        // there's no renderer (imagine setting display:none in the :hover class,
        // if a nil renderer would prevent this element from recalculating its
        // style, it would never go back to its normal style and remain
        // stuck in its hovered style).
        if (!flag)
            invalidateStyleForSubtree();

        return;
    }

    if (renderer()->style().affectedByHover() || childrenAffectedByHover())
        invalidateStyleForSubtree();

    if (renderer()->style().hasAppearance())
        renderer()->theme().stateChanged(*renderer(), ControlStates::HoverState);
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
        renderer()->scrollRectToVisible(SelectionRevealMode::Reveal, absoluteBounds, insideFixed, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways);
    else
        renderer()->scrollRectToVisible(SelectionRevealMode::Reveal, absoluteBounds, insideFixed, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignBottomAlways);
}

void Element::scrollIntoViewIfNeeded(bool centerIfNeeded)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    bool insideFixed;
    LayoutRect absoluteBounds = renderer()->absoluteAnchorRect(&insideFixed);
    if (centerIfNeeded)
        renderer()->scrollRectToVisible(SelectionRevealMode::Reveal, absoluteBounds, insideFixed, ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded);
    else
        renderer()->scrollRectToVisible(SelectionRevealMode::Reveal, absoluteBounds, insideFixed, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded);
}

void Element::scrollIntoViewIfNotVisible(bool centerIfNotVisible)
{
    document().updateLayoutIgnorePendingStylesheets();
    
    if (!renderer())
        return;
    
    bool insideFixed;
    LayoutRect absoluteBounds = renderer()->absoluteAnchorRect(&insideFixed);
    if (centerIfNotVisible)
        renderer()->scrollRectToVisible(SelectionRevealMode::Reveal, absoluteBounds, insideFixed, ScrollAlignment::alignCenterIfNotVisible, ScrollAlignment::alignCenterIfNotVisible);
    else
        renderer()->scrollRectToVisible(SelectionRevealMode::Reveal, absoluteBounds, insideFixed, ScrollAlignment::alignToEdgeIfNotVisible, ScrollAlignment::alignToEdgeIfNotVisible);
}

void Element::scrollBy(const ScrollToOptions& options)
{
    return scrollBy(options.left.value_or(0), options.top.value_or(0));
}

static inline double normalizeNonFiniteValue(double f)
{
    return std::isfinite(f) ? f : 0;
}

void Element::scrollBy(double x, double y)
{
    scrollTo(scrollLeft() + normalizeNonFiniteValue(x), scrollTop() + normalizeNonFiniteValue(y));
}

void Element::scrollTo(const ScrollToOptions& options)
{
    // If the element is the root element and document is in quirks mode, terminate these steps.
    // Note that WebKit always uses quirks mode document scrolling behavior. See Document::scrollingElement().
    if (this == document().documentElement())
        return;

    document().updateLayoutIgnorePendingStylesheets();

    // If the element does not have any associated CSS layout box, the element has no associated scrolling box,
    // or the element has no overflow, terminate these steps.
    RenderBox* renderer = renderBox();
    if (!renderer || !renderer->hasOverflowClip())
        return;

    // Normalize non-finite values for left and top dictionary members of options, if present.
    double x = options.left ? normalizeNonFiniteValue(options.left.value()) : adjustForAbsoluteZoom(renderer->scrollLeft(), *renderer);
    double y = options.top ? normalizeNonFiniteValue(options.top.value()) : adjustForAbsoluteZoom(renderer->scrollTop(), *renderer);

    renderer->setScrollLeft(clampToInteger(x * renderer->style().effectiveZoom()));
    renderer->setScrollTop(clampToInteger(y * renderer->style().effectiveZoom()));
}

void Element::scrollTo(double x, double y)
{
    scrollTo({ x, y });
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

enum LegacyCSSOMElementMetricsRoundingStrategy { Round, Floor };

static bool subpixelMetricsEnabled(const Document& document)
{
    return document.settings().subpixelCSSOMElementMetricsEnabled();
}

static double convertToNonSubpixelValueIfNeeded(double value, const Document& document, LegacyCSSOMElementMetricsRoundingStrategy roundStrategy = Round)
{
    return subpixelMetricsEnabled(document) ? value : roundStrategy == Round ? round(value) : floor(value);
}

double Element::offsetLeft()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject()) {
        LayoutUnit offsetLeft = subpixelMetricsEnabled(renderer->document()) ? renderer->offsetLeft() : LayoutUnit(roundToInt(renderer->offsetLeft()));
        double zoomFactor = 1;
        double offsetLeftAdjustedWithZoom = adjustForLocalZoom(offsetLeft, *renderer, zoomFactor);
        return convertToNonSubpixelValueIfNeeded(offsetLeftAdjustedWithZoom, renderer->document(), zoomFactor == 1 ? Floor : Round);
    }
    return 0;
}

double Element::offsetTop()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject()) {
        LayoutUnit offsetTop = subpixelMetricsEnabled(renderer->document()) ? renderer->offsetTop() : LayoutUnit(roundToInt(renderer->offsetTop()));
        double zoomFactor = 1;
        double offsetTopAdjustedWithZoom = adjustForLocalZoom(offsetTop, *renderer, zoomFactor);
        return convertToNonSubpixelValueIfNeeded(offsetTopAdjustedWithZoom, renderer->document(), zoomFactor == 1 ? Floor : Round);
    }
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

Element* Element::bindingsOffsetParent()
{
    Element* element = offsetParent();
    if (!element || !element->isInShadowTree())
        return element;
    return element->containingShadowRoot()->mode() == ShadowRootMode::UserAgent ? nullptr : element;
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
        return convertToNonSubpixelValueIfNeeded(adjustLayoutUnitForAbsoluteZoom(clientHeight, *renderer).toDouble(), renderer->document());
    }
    return 0;
}

int Element::scrollLeft()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (auto* renderer = renderBox())
        return adjustForAbsoluteZoom(renderer->scrollLeft(), *renderer);
    return 0;
}

int Element::scrollTop()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (RenderBox* renderer = renderBox())
        return adjustForAbsoluteZoom(renderer->scrollTop(), *renderer);
    return 0;
}

void Element::setScrollLeft(int newLeft)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (auto* renderer = renderBox()) {
        renderer->setScrollLeft(static_cast<int>(newLeft * renderer->style().effectiveZoom()));
        if (auto* scrollableArea = renderer->layer())
            scrollableArea->setScrolledProgrammatically(true);
    }
}

void Element::setScrollTop(int newTop)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (auto* renderer = renderBox()) {
        renderer->setScrollTop(static_cast<int>(newTop * renderer->style().effectiveZoom()));
        if (auto* scrollableArea = renderer->layer())
            scrollableArea->setScrolledProgrammatically(true);
    }
}

int Element::scrollWidth()
{
    document().updateLayoutIfDimensionsOutOfDate(*this, WidthDimensionsCheck);
    if (auto* renderer = renderBox())
        return adjustForAbsoluteZoom(renderer->scrollWidth(), *renderer);
    return 0;
}

int Element::scrollHeight()
{
    document().updateLayoutIfDimensionsOutOfDate(*this, HeightDimensionsCheck);
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

    if (quads.isEmpty())
        return IntRect();

    IntRect result = quads[0].enclosingBoundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        result.unite(quads[i].enclosingBoundingBox());

    result = view->contentsToRootView(result);
    return result;
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
            if (positionedBox->style().position() == FixedPosition && renderBox.element()->contains(positionedBox->element()))
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
            
            if (RenderFlowThread* flowThread = box.flowThreadContainingBlock()) {
                bool wasFixed = false;
                Vector<FloatQuad> quads;
                FloatRect localRect(0, 0, box.width(), box.height());
                if (flowThread->absoluteQuadsForBox(quads, &wasFixed, &box, localRect.y(), localRect.maxY())) {
                    FloatRect quadBounds = quads[0].boundingBox();
                    for (size_t i = 1; i < quads.size(); ++i)
                        quadBounds.unite(quads[i].boundingBox());
                    
                    result = LayoutRect(quadBounds);
                    computedBounds = true;
                } else {
                    // Probably columns. Just return the bounds of the multicol block for now.
                    // FIXME: this doesn't handle nested columns.
                    RenderElement* multicolContainer = flowThread->parent();
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

    if (frameView->needsLayout())
        frameView->layout();

    return absoluteEventBoundsOfElementAndDescendants(includesFixedPositionElements);
}

static std::optional<std::pair<RenderObject*, LayoutRect>> listBoxElementBoundingBox(Element& element)
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
        return std::nullopt;

    if (!selectElement || !selectElement->renderer() || !is<RenderListBox>(selectElement->renderer()))
        return std::nullopt;

    auto& renderer = downcast<RenderListBox>(*selectElement->renderer());
    std::optional<LayoutRect> boundingBox;
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
        return std::nullopt;

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

FloatRect Element::boundingClientRect()
{
    document().updateLayoutIgnorePendingStylesheets();

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
        return { };

    FloatRect result = quads[0].boundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        result.unite(quads[i].boundingBox());

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

const AtomicString& Element::getAttribute(const AtomicString& localName) const
{
    if (!elementData())
        return nullAtom();
    synchronizeAttribute(localName);
    if (const Attribute* attribute = elementData()->findAttributeByName(localName, shouldIgnoreAttributeCase(*this)))
        return attribute->value();
    return nullAtom();
}

const AtomicString& Element::getAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    return getAttribute(QualifiedName(nullAtom(), localName, namespaceURI));
}

ExceptionOr<void> Element::setAttribute(const AtomicString& localName, const AtomicString& value)
{
    if (!Document::isValidName(localName))
        return Exception { InvalidCharacterError };

    synchronizeAttribute(localName);
    auto caseAdjustedLocalName = shouldIgnoreAttributeCase(*this) ? localName.convertToASCIILowercase() : localName;
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(caseAdjustedLocalName, false) : ElementData::attributeNotFound;
    auto name = index != ElementData::attributeNotFound ? attributeAt(index).name() : QualifiedName { nullAtom(), caseAdjustedLocalName, nullAtom() };
    setAttributeInternal(index, name, value, NotInSynchronizationOfLazyAttribute);

    return { };
}

void Element::setAttribute(const QualifiedName& name, const AtomicString& value)
{
    synchronizeAttribute(name);
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(name) : ElementData::attributeNotFound;
    setAttributeInternal(index, name, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setAttributeWithoutSynchronization(const QualifiedName& name, const AtomicString& value)
{
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(name) : ElementData::attributeNotFound;
    setAttributeInternal(index, name, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setSynchronizedLazyAttribute(const QualifiedName& name, const AtomicString& value)
{
    unsigned index = elementData() ? elementData()->findAttributeIndexByName(name) : ElementData::attributeNotFound;
    setAttributeInternal(index, name, value, InSynchronizationOfLazyAttribute);
}

inline void Element::setAttributeInternal(unsigned index, const QualifiedName& name, const AtomicString& newValue, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
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
    AtomicString oldValue = attribute.value();

    willModifyAttribute(attributeName, oldValue, newValue);

    if (newValue != oldValue) {
        Style::AttributeChangeInvalidation styleInvalidation(*this, name, oldValue, newValue);
        ensureUniqueElementData().attributeAt(index).setValue(newValue);
    }

    didModifyAttribute(attributeName, oldValue, newValue);
}

static inline AtomicString makeIdForStyleResolution(const AtomicString& value, bool inQuirksMode)
{
    if (inQuirksMode)
        return value.convertToASCIILowercase();
    return value;
}

void Element::attributeChanged(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason)
{
    bool valueIsSameAsBefore = oldValue == newValue;

    if (!valueIsSameAsBefore) {
        if (name == HTMLNames::idAttr) {
            if (!oldValue.isEmpty())
                treeScope().idTargetObserverRegistry().notifyObservers(*oldValue.impl());
            if (!newValue.isEmpty())
                treeScope().idTargetObserverRegistry().notifyObservers(*newValue.impl());

            AtomicString oldId = elementData()->idForStyleResolution();
            AtomicString newId = makeIdForStyleResolution(newValue, document().inQuirksMode());
            if (newId != oldId) {
                Style::IdChangeInvalidation styleInvalidation(*this, oldId, newId);
                elementData()->setIdForStyleResolution(newId);
            }
        } else if (name == classAttr)
            classAttributeChanged(newValue);
        else if (name == HTMLNames::nameAttr)
            elementData()->setHasNameAttribute(!newValue.isNull());
        else if (name == HTMLNames::pseudoAttr) {
            if (needsStyleInvalidation() && isInShadowTree())
                invalidateStyleForSubtree();
        }
        else if (name == HTMLNames::slotAttr) {
            if (auto* parent = parentElement()) {
                if (auto* shadowRoot = parent->shadowRoot())
                    shadowRoot->hostChildElementDidChangeSlotAttribute(*this, oldValue, newValue);
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
        cache->handleAttributeChanged(name, this);
}

template <typename CharacterType>
static inline bool classStringHasClassName(const CharacterType* characters, unsigned length)
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

static inline bool classStringHasClassName(const AtomicString& newClassString)
{
    unsigned length = newClassString.length();

    if (!length)
        return false;

    if (newClassString.is8Bit())
        return classStringHasClassName(newClassString.characters8(), length);
    return classStringHasClassName(newClassString.characters16(), length);
}

void Element::classAttributeChanged(const AtomicString& newClassString)
{
    // Note: We'll need ElementData, but it doesn't have to be UniqueElementData.
    if (!elementData())
        ensureUniqueElementData();

    bool shouldFoldCase = document().inQuirksMode();
    bool newStringHasClasses = classStringHasClassName(newClassString);

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

URL Element::absoluteLinkURL() const
{
    if (!isLink())
        return URL();

    AtomicString linkAttribute;
    if (hasTagName(SVGNames::aTag))
        linkAttribute = getAttribute(XLinkNames::hrefAttr);
    else
        linkAttribute = getAttribute(HTMLNames::hrefAttr);

    if (linkAttribute.isEmpty())
        return URL();

    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(linkAttribute));
}

#if ENABLE(TOUCH_EVENTS)
bool Element::allowsDoubleTapGesture() const
{
    if (renderStyle() && renderStyle()->touchAction() != TouchAction::Auto)
        return false;

    Element* parent = parentElement();
    return !parent || parent->allowsDoubleTapGesture();
}
#endif

StyleResolver& Element::styleResolver()
{
    if (auto* shadowRoot = containingShadowRoot())
        return shadowRoot->styleScope().resolver();

    return document().styleScope().resolver();
}

ElementStyle Element::resolveStyle(const RenderStyle* parentStyle)
{
    return styleResolver().styleForElement(*this, parentStyle);
}

void Element::invalidateStyle()
{
    Node::invalidateStyle(Style::Validity::ElementInvalid);
}

void Element::invalidateStyleAndLayerComposition()
{
    Node::invalidateStyle(Style::Validity::ElementInvalid, Style::InvalidationMode::RecompositeLayer);
}

void Element::invalidateStyleForSubtree()
{
    Node::invalidateStyle(Style::Validity::SubtreeInvalid);
}

void Element::invalidateStyleAndRenderersForSubtree()
{
    Node::invalidateStyle(Style::Validity::SubtreeAndRenderersInvalid);
}

#if ENABLE(WEB_ANIMATIONS)
WebAnimationVector Element::getAnimations()
{
    auto checkTarget = [this](AnimationEffect const& effect)
    {
        return (static_cast<KeyframeEffect const&>(effect).target() == this);
    };

    auto* document = DocumentAnimation::from(&this->document());
    if (document)
        return document->getAnimations(WTFMove(checkTarget));
    return WebAnimationVector();
}
#endif

bool Element::hasDisplayContents() const
{
    if (renderer() || !hasRareData())
        return false;
    const RenderStyle* style = elementRareData()->computedStyle();
    return style && style->display() == CONTENTS;
}

void Element::storeDisplayContentsStyle(std::unique_ptr<RenderStyle> style)
{
    ASSERT(style && style->display() == CONTENTS);
    ASSERT(!renderer());
    ensureElementRareData().setComputedStyle(WTFMove(style));
}

// Returns true is the given attribute is an event handler.
// We consider an event handler any attribute that begins with "on".
// It is a simple solution that has the advantage of not requiring any
// code or configuration change if a new event handler is defined.

static inline bool isEventHandlerAttribute(const Attribute& attribute)
{
    return attribute.name().namespaceURI().isNull() && attribute.name().localName().startsWith("on");
}

bool Element::isJavaScriptURLAttribute(const Attribute& attribute) const
{
    return isURLAttribute(attribute) && protocolIsJavaScript(stripLeadingAndTrailingHTMLSpaces(attribute.value()));
}

void Element::stripScriptingAttributes(Vector<Attribute>& attributeVector) const
{
    attributeVector.removeAllMatching([this](auto& attribute) -> bool {
        return isEventHandlerAttribute(attribute)
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
    Node::didMoveToNewDocument(oldDocument, newDocument);

    if (oldDocument.inQuirksMode() != document().inQuirksMode()) {
        // ElementData::m_classNames or ElementData::m_idForStyleResolution need to be updated with the right case.
        if (hasID())
            attributeChanged(idAttr, nullAtom(), getIdAttribute());
        if (hasClass())
            attributeChanged(classAttr, nullAtom(), getAttribute(classAttr));
    }

    if (UNLIKELY(isDefinedCustomElement()))
        CustomElementReactionQueue::enqueueAdoptedCallbackIfNeeded(*this, oldDocument, newDocument);
}

bool Element::hasAttributes() const
{
    synchronizeAllAttributes();
    return elementData() && elementData()->length();
}

bool Element::hasEquivalentAttributes(const Element* other) const
{
    synchronizeAllAttributes();
    other->synchronizeAllAttributes();
    if (elementData() == other->elementData())
        return true;
    if (elementData())
        return elementData()->isEquivalent(other->elementData());
    if (other->elementData())
        return other->elementData()->isEquivalent(elementData());
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

ExceptionOr<void> Element::setPrefix(const AtomicString& prefix)
{
    auto result = checkSetPrefix(prefix);
    if (result.hasException())
        return result.releaseException();

    m_tagName.setPrefix(prefix.isEmpty() ? nullAtom() : prefix);
    return { };
}

const AtomicString& Element::imageSourceURL() const
{
    return attributeWithoutSynchronization(srcAttr);
}

bool Element::rendererIsNeeded(const RenderStyle& style)
{
    return style.display() != NONE && style.display() != CONTENTS;
}

RenderPtr<RenderElement> Element::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return RenderElement::createFor(*this, WTFMove(style));
}

Node::InsertionNotificationRequest Element::insertedInto(ContainerNode& insertionPoint)
{
    bool wasInDocument = isConnected();
    // need to do superclass processing first so isConnected() is true
    // by the time we reach updateId
    ContainerNode::insertedInto(insertionPoint);
    ASSERT(!wasInDocument || isConnected());

#if ENABLE(FULLSCREEN_API)
    if (containsFullScreenElement() && parentElement() && !parentElement()->containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);
#endif

    if (parentNode() == &insertionPoint) {
        if (auto* shadowRoot = parentNode()->shadowRoot())
            shadowRoot->hostChildElementDidChange(*this);
    }

    if (!insertionPoint.isInTreeScope())
        return InsertionDone;

    // This function could be called when this element's shadow root's host or its ancestor is inserted.
    // This element is new to the shadow tree (and its tree scope) only if the parent into which this element
    // or its ancestor is inserted belongs to the same tree scope as this element's.
    TreeScope* newScope = &insertionPoint.treeScope();
    bool becomeConnected = !wasInDocument && isConnected();
    HTMLDocument* newDocument = becomeConnected && is<HTMLDocument>(newScope->documentScope()) ? &downcast<HTMLDocument>(newScope->documentScope()) : nullptr;
    if (newScope != &treeScope())
        newScope = nullptr;

    const AtomicString& idValue = getIdAttribute();
    if (!idValue.isNull()) {
        if (newScope)
            updateIdForTreeScope(*newScope, nullAtom(), idValue);
        if (newDocument)
            updateIdForDocument(*newDocument, nullAtom(), idValue, AlwaysUpdateHTMLDocumentNamedItemMaps);
    }

    const AtomicString& nameValue = getNameAttribute();
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
        if (UNLIKELY(isCustomElementUpgradeCandidate()))
            CustomElementReactionQueue::enqueueElementUpgradeIfDefined(*this);
        if (UNLIKELY(isDefinedCustomElement()))
            CustomElementReactionQueue::enqueueConnectedCallbackIfNeeded(*this);
    }

    return InsertionDone;
}

void Element::removedFrom(ContainerNode& insertionPoint)
{
#if ENABLE(FULLSCREEN_API)
    if (containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);
#endif
#if ENABLE(POINTER_LOCK)
    if (document().page())
        document().page()->pointerLockController().elementRemoved(*this);
#endif

    setSavedLayerScrollPosition(ScrollPosition());

    if (insertionPoint.isInTreeScope()) {
        TreeScope* oldScope = &insertionPoint.treeScope();
        bool becomeDisconnected = isConnected();
        HTMLDocument* oldDocument = becomeDisconnected && is<HTMLDocument>(oldScope->documentScope()) ? &downcast<HTMLDocument>(oldScope->documentScope()) : nullptr;

        // ContainerNode::removeBetween always sets the removed chid's tree scope to Document's but InTreeScope flag is unset in Node::removedFrom.
        // So this element has been removed from the old tree scope only if InTreeScope flag is set and this element's tree scope is Document's.
        if (!isInTreeScope() || &treeScope() != &document())
            oldScope = nullptr;

        const AtomicString& idValue = getIdAttribute();
        if (!idValue.isNull()) {
            if (oldScope)
                updateIdForTreeScope(*oldScope, idValue, nullAtom());
            if (oldDocument)
                updateIdForDocument(*oldDocument, idValue, nullAtom(), AlwaysUpdateHTMLDocumentNamedItemMaps);
        }

        const AtomicString& nameValue = getNameAttribute();
        if (!nameValue.isNull()) {
            if (oldScope)
                updateNameForTreeScope(*oldScope, nameValue, nullAtom());
            if (oldDocument)
                updateNameForDocument(*oldDocument, nameValue, nullAtom());
        }

        if (oldScope && hasTagName(labelTag)) {
            if (oldScope->shouldCacheLabelsByForAttribute())
                updateLabel(*oldScope, attributeWithoutSynchronization(forAttr), nullAtom());
        }

        if (becomeDisconnected && UNLIKELY(isDefinedCustomElement()))
            CustomElementReactionQueue::enqueueDisconnectedCallbackIfNeeded(*this);
    }

    if (!parentNode()) {
        if (auto* shadowRoot = insertionPoint.shadowRoot())
            shadowRoot->hostChildElementDidChange(*this);
    }

    ContainerNode::removedFrom(insertionPoint);

    if (hasPendingResources())
        document().accessSVGExtensions().removeElementFromPendingResources(this);


#if PLATFORM(MAC)
    if (Frame* frame = document().frame())
        frame->mainFrame().removeLatchingStateForTarget(*this);
#endif
}

void Element::unregisterNamedFlowContentElement()
{
    if (isNamedFlowContentElement() && document().renderView())
        document().renderView()->flowThreadController().unregisterNamedFlowContentElement(*this);
}

ShadowRoot* Element::shadowRoot() const
{
    return hasRareData() ? elementRareData()->shadowRoot() : nullptr;
}

void Element::addShadowRoot(Ref<ShadowRoot>&& newShadowRoot)
{
    ASSERT(!shadowRoot());
    
    if (renderer())
        RenderTreeUpdater::tearDownRenderers(*this);

    ShadowRoot& shadowRoot = newShadowRoot;
    ensureElementRareData().setShadowRoot(WTFMove(newShadowRoot));

    shadowRoot.setHost(this);
    shadowRoot.setParentTreeScope(treeScope());

    NodeVector postInsertionNotificationTargets;
    notifyChildNodeInserted(*this, shadowRoot, postInsertionNotificationTargets);
    for (auto& target : postInsertionNotificationTargets)
        target->finishedInsertingSubtree();

    invalidateStyleAndRenderersForSubtree();

    InspectorInstrumentation::didPushShadowRoot(*this, shadowRoot);

    if (shadowRoot.mode() == ShadowRootMode::UserAgent)
        didAddUserAgentShadowRoot(&shadowRoot);
}

void Element::removeShadowRoot()
{
    RefPtr<ShadowRoot> oldRoot = shadowRoot();
    if (!oldRoot)
        return;

    InspectorInstrumentation::willPopShadowRoot(*this, *oldRoot);
    document().removeFocusedNodeOfSubtree(*oldRoot);

    ASSERT(!oldRoot->renderer());

    elementRareData()->clearShadowRoot();

    oldRoot->setHost(nullptr);
    oldRoot->setParentTreeScope(document());
}

static bool canAttachAuthorShadowRoot(const Element& element)
{
    static NeverDestroyed<HashSet<AtomicString>> tagNames = [] {
        static const HTMLQualifiedName* const tagList[] = {
            &articleTag,
            &asideTag,
            &blockquoteTag,
            &bodyTag,
            &divTag,
            &footerTag,
            &h1Tag,
            &h2Tag,
            &h3Tag,
            &h4Tag,
            &h5Tag,
            &h6Tag,
            &headerTag,
            &navTag,
            &pTag,
            &sectionTag,
            &spanTag
        };
        HashSet<AtomicString> set;
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
        return Exception { InvalidStateError };
    if (init.mode == ShadowRootMode::UserAgent)
        return Exception { TypeError };
    auto shadow = ShadowRoot::create(document(), init.mode);
    auto& result = shadow.get();
    addShadowRoot(WTFMove(shadow));
    return result;
}

ShadowRoot* Element::shadowRootForBindings(JSC::ExecState& state) const
{
    auto* shadow = shadowRoot();
    if (!shadow)
        return nullptr;
    if (shadow->mode() == ShadowRootMode::Open)
        return shadow;
    if (JSC::jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject())->world().shadowRootIsAlwaysOpen())
        return shadow;
    return nullptr;
}

ShadowRoot* Element::userAgentShadowRoot() const
{
    ASSERT(!shadowRoot() || shadowRoot()->mode() == ShadowRootMode::UserAgent);
    return shadowRoot();
}

ShadowRoot& Element::ensureUserAgentShadowRoot()
{
    if (auto* shadow = userAgentShadowRoot())
        return *shadow;
    auto newShadow = ShadowRoot::create(document(), ShadowRootMode::UserAgent);
    ShadowRoot& shadow = newShadow;
    addShadowRoot(WTFMove(newShadow));
    return shadow;
}

void Element::setIsDefinedCustomElement(JSCustomElementInterface& elementInterface)
{
    clearFlag(IsEditingTextOrUndefinedCustomElementFlag);
    setFlag(IsCustomElement);
    auto& data = ensureElementRareData();
    if (!data.customElementReactionQueue())
        data.setCustomElementReactionQueue(std::make_unique<CustomElementReactionQueue>(elementInterface));
    InspectorInstrumentation::didChangeCustomElementState(*this);
}

void Element::setIsFailedCustomElement(JSCustomElementInterface&)
{
    ASSERT(isUndefinedCustomElement());
    ASSERT(getFlag(IsEditingTextOrUndefinedCustomElementFlag));
    clearFlag(IsCustomElement);

    if (hasRareData()) {
        // Clear the queue instead of deleting it since this function can be called inside CustomElementReactionQueue::invokeAll during upgrades.
        if (auto* queue = elementRareData()->customElementReactionQueue())
            queue->clear();
    }
    InspectorInstrumentation::didChangeCustomElementState(*this);
}

void Element::setIsCustomElementUpgradeCandidate()
{
    ASSERT(!getFlag(IsCustomElement));
    setFlag(IsCustomElement);
    setFlag(IsEditingTextOrUndefinedCustomElementFlag);
    InspectorInstrumentation::didChangeCustomElementState(*this);
}

void Element::enqueueToUpgrade(JSCustomElementInterface& elementInterface)
{
    ASSERT(!isDefinedCustomElement() && !isFailedCustomElement());
    setFlag(IsCustomElement);
    setFlag(IsEditingTextOrUndefinedCustomElementFlag);
    InspectorInstrumentation::didChangeCustomElementState(*this);

    auto& data = ensureElementRareData();
    ASSERT(!data.customElementReactionQueue());

    data.setCustomElementReactionQueue(std::make_unique<CustomElementReactionQueue>(elementInterface));
    data.customElementReactionQueue()->enqueueElementUpgrade(*this);
}

CustomElementReactionQueue* Element::reactionQueue() const
{
    ASSERT(isDefinedCustomElement() || isCustomElementUpgradeCandidate());
    if (!hasRareData())
        return nullptr;
    return elementRareData()->customElementReactionQueue();
}

const AtomicString& Element::shadowPseudoId() const
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
        Element* newFirstElement = ElementTraversal::firstChild(parent);
        // Find the first element node following |afterChange|

        // This is the insert/append case.
        if (newFirstElement != elementAfterChange) {
            auto* style = elementAfterChange->renderStyle();
            if (!style || style->firstChildState())
                elementAfterChange->invalidateStyleForSubtree();
        }

        // We also have to handle node removal.
        if (checkType == SiblingElementRemoved && newFirstElement == elementAfterChange && newFirstElement) {
            auto* style = newFirstElement->renderStyle();
            if (!style || !style->firstChildState())
                newFirstElement->invalidateStyleForSubtree();
        }
    }

    // :last-child.  In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    if (parent.childrenAffectedByLastChildRules() && elementBeforeChange) {
        // Find our new last child.
        Element* newLastElement = ElementTraversal::lastChild(parent);

        if (newLastElement != elementBeforeChange) {
            auto* style = elementBeforeChange->renderStyle();
            if (!style || style->lastChildState())
                elementBeforeChange->invalidateStyleForSubtree();
        }

        // We also have to handle node removal.  The parser callback case is similar to node removal as well in that we need to change the last child
        // to match now.
        if ((checkType == SiblingElementRemoved || checkType == FinishedParsingChildren) && newLastElement == elementBeforeChange && newLastElement) {
            auto* style = newLastElement->renderStyle();
            if (!style || !style->lastChildState())
                newLastElement->invalidateStyleForSubtree();
        }
    }

    if (elementAfterChange) {
        if (elementAfterChange->styleIsAffectedByPreviousSibling())
            elementAfterChange->invalidateStyleForSubtree();
        else if (elementAfterChange->affectsNextSiblingElementStyle()) {
            Element* elementToInvalidate = elementAfterChange;
            do {
                elementToInvalidate = elementToInvalidate->nextElementSibling();
            } while (elementToInvalidate && !elementToInvalidate->styleIsAffectedByPreviousSibling());

            if (elementToInvalidate)
                elementToInvalidate->invalidateStyleForSubtree();
        }
    }

    // Backward positional selectors include nth-last-child, nth-last-of-type, last-of-type and only-of-type.
    // We have to invalidate everything following the insertion point in the forward case, and everything before the insertion point in the
    // backward case.
    // |afterChange| is 0 in the parser callback case, so we won't do any work for the forward case if we don't have to.
    // For performance reasons we just mark the parent node as changed, since we don't want to make childrenChanged O(n^2) by crawling all our kids
    // here.  recalcStyle will then force a walk of the children when it sees that this has happened.
    if (parent.childrenAffectedByBackwardPositionalRules() && elementBeforeChange)
        parent.invalidateStyleForSubtree();
}

void Element::childrenChanged(const ChildChange& change)
{
    ContainerNode::childrenChanged(change);
    if (change.source == ChildChangeSourceParser)
        checkForEmptyStyleChange(*this);
    else {
        SiblingCheckType checkType = change.type == ElementRemoved ? SiblingElementRemoved : Other;
        checkForSiblingStyleChanges(*this, checkType, change.previousSiblingElement, change.nextSiblingElement);
    }

    if (ShadowRoot* shadowRoot = this->shadowRoot()) {
        switch (change.type) {
        case ElementInserted:
        case ElementRemoved:
            // For elements, we notify shadowRoot in Element::insertedInto and Element::removedFrom.
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

void Element::setAttributeEventListener(const AtomicString& eventType, const QualifiedName& attributeName, const AtomicString& attributeValue)
{
    setAttributeEventListener(eventType, JSLazyEventListener::create(*this, attributeName, attributeValue), mainThreadNormalWorld());
}

void Element::setIsNamedFlowContentElement()
{
    ensureElementRareData().setIsNamedFlowContentElement(true);
}

void Element::clearIsNamedFlowContentElement()
{
    ensureElementRareData().setIsNamedFlowContentElement(false);
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

    NoEventDispatchAssertion assertNoEventDispatch;

    attrNode.attachToElement(*this);
    ensureAttrNodeListForElement(*this).append(&attrNode);
}

ExceptionOr<RefPtr<Attr>> Element::setAttributeNode(Attr& attrNode)
{
    RefPtr<Attr> oldAttrNode = attrIfExists(attrNode.localName(), shouldIgnoreAttributeCase(*this));
    if (oldAttrNode.get() == &attrNode)
        return WTFMove(oldAttrNode);

    // InUseAttributeError: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attrNode.ownerElement() && attrNode.ownerElement() != this)
        return Exception { InUseAttributeError };

    {
    NoEventDispatchAssertion assertNoEventDispatch;
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

    return WTFMove(oldAttrNode);
}

ExceptionOr<RefPtr<Attr>> Element::setAttributeNodeNS(Attr& attrNode)
{
    RefPtr<Attr> oldAttrNode = attrIfExists(attrNode.qualifiedName());
    if (oldAttrNode.get() == &attrNode)
        return WTFMove(oldAttrNode);

    // InUseAttributeError: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attrNode.ownerElement() && attrNode.ownerElement() != this)
        return Exception { InUseAttributeError };

    unsigned index = 0;
    
    // Attr::value() will return its 'm_standaloneValue' member any time its Element is set to nullptr. We need to cache this value
    // before making changes to attrNode's Element connections.
    auto attrNodeValue = attrNode.value();

    {
    NoEventDispatchAssertion assertNoEventDispatch;
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

    return WTFMove(oldAttrNode);
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

    return WTFMove(oldAttrNode);
}

ExceptionOr<QualifiedName> Element::parseAttributeName(const AtomicString& namespaceURI, const AtomicString& qualifiedName)
{
    auto parseResult = Document::parseQualifiedName(namespaceURI, qualifiedName);
    if (parseResult.hasException())
        return parseResult.releaseException();
    QualifiedName parsedAttributeName { parseResult.releaseReturnValue() };
    if (!Document::hasValidNamespaceForAttributes(parsedAttributeName))
        return Exception { NamespaceError };
    return WTFMove(parsedAttributeName);
}

ExceptionOr<void> Element::setAttributeNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, const AtomicString& value)
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
    AtomicString valueBeingRemoved = elementData.attributeAt(index).value();

    if (RefPtr<Attr> attrNode = attrIfExists(name))
        detachAttrNodeFromElementWithValue(attrNode.get(), elementData.attributeAt(index).value());

    if (inSynchronizationOfLazyAttribute) {
        elementData.removeAttribute(index);
        return;
    }

    if (!valueBeingRemoved.isNull())
        willModifyAttribute(name, valueBeingRemoved, nullAtom());

    {
        Style::AttributeChangeInvalidation styleInvalidation(*this, name, valueBeingRemoved, nullAtom());
        elementData.removeAttribute(index);
    }

    didRemoveAttribute(name, valueBeingRemoved);
}

void Element::addAttributeInternal(const QualifiedName& name, const AtomicString& value, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
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

bool Element::removeAttribute(const AtomicString& name)
{
    if (!elementData())
        return false;

    AtomicString localName = shouldIgnoreAttributeCase(*this) ? name.convertToASCIILowercase() : name;
    unsigned index = elementData()->findAttributeIndexByName(localName, false);
    if (index == ElementData::attributeNotFound) {
        if (UNLIKELY(localName == styleAttr) && elementData()->styleAttributeIsDirty() && is<StyledElement>(*this))
            downcast<StyledElement>(*this).removeAllInlineStyleProperties();
        return false;
    }

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    return true;
}

bool Element::removeAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    return removeAttribute(QualifiedName(nullAtom(), localName, namespaceURI));
}

RefPtr<Attr> Element::getAttributeNode(const AtomicString& localName)
{
    if (!elementData())
        return nullptr;
    synchronizeAttribute(localName);
    const Attribute* attribute = elementData()->findAttributeByName(localName, shouldIgnoreAttributeCase(*this));
    if (!attribute)
        return nullptr;
    return ensureAttr(attribute->name());
}

RefPtr<Attr> Element::getAttributeNodeNS(const AtomicString& namespaceURI, const AtomicString& localName)
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

bool Element::hasAttribute(const AtomicString& localName) const
{
    if (!elementData())
        return false;
    synchronizeAttribute(localName);
    return elementData()->findAttributeByName(localName, shouldIgnoreAttributeCase(*this));
}

bool Element::hasAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    if (!elementData())
        return false;
    QualifiedName qName(nullAtom(), localName, namespaceURI);
    synchronizeAttribute(qName);
    return elementData()->findAttributeByName(qName);
}

void Element::focus(bool restorePreviousSelection, FocusDirection direction)
{
    if (!isConnected())
        return;

    if (document().focusedElement() == this) {
        if (document().page())
            document().page()->chrome().client().elementDidRefocus(*this);

        return;
    }

    // If the stylesheets have already been loaded we can reliably check isFocusable.
    // If not, we continue and set the focused node on the focus controller below so
    // that it can be updated soon after attach. 
    if (document().haveStylesheetsLoaded()) {
        document().updateLayoutIgnorePendingStylesheets();
        if (!isFocusable())
            return;
    }

    if (!supportsFocus())
        return;

    RefPtr<Node> protect;
    if (Page* page = document().page()) {
        // Focus and change event handlers can cause us to lose our last ref.
        // If a focus event handler changes the focus to a different node it
        // does not make sense to continue and update appearence.
        protect = this;
        if (!page->focusController().setFocusedElement(this, *document().frame(), direction))
            return;
    }

    // Setting the focused node above might have invalidated the layout due to scripts.
    document().updateLayoutIgnorePendingStylesheets();

    if (!isFocusable()) {
        ensureElementRareData().setNeedsFocusAppearanceUpdateSoonAfterAttach(true);
        return;
    }
        
    cancelFocusAppearanceUpdate();

    SelectionRevealMode revealMode = SelectionRevealMode::Reveal;
#if PLATFORM(IOS)
    // Focusing a form element triggers animation in UIKit to scroll to the right position.
    // Calling updateFocusAppearance() would generate an unnecessary call to ScrollView::setScrollPosition(),
    // which would jump us around during this animation. See <rdar://problem/6699741>.
    bool isFormControl = is<HTMLFormControlElement>(*this);
    if (isFormControl)
        revealMode = SelectionRevealMode::RevealUpToMainFrame;
#endif

    updateFocusAppearance(restorePreviousSelection ? SelectionRestorationMode::Restore : SelectionRestorationMode::SetDefault, revealMode);
}

void Element::updateFocusAppearanceAfterAttachIfNeeded()
{
    if (!hasRareData())
        return;
    ElementRareData* data = elementRareData();
    if (!data->needsFocusAppearanceUpdateSoonAfterAttach())
        return;
    if (isFocusable() && document().focusedElement() == this)
        document().updateFocusAppearanceSoon(SelectionRestorationMode::SetDefault);
    data->setNeedsFocusAppearanceUpdateSoonAfterAttach(false);
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
        VisibleSelection newSelection = VisibleSelection(firstPositionInOrBeforeNode(this), DOWNSTREAM);
        
        if (frame->selection().shouldChangeSelection(newSelection)) {
            frame->selection().setSelection(newSelection, FrameSelection::defaultSetSelectionOptions(), Element::defaultFocusTextStateChangeIntent());
            frame->selection().revealSelection(revealMode);
        }
    } else if (renderer() && !renderer()->isWidget()) {
        bool insideFixed;
        LayoutRect absoluteBounds = renderer()->absoluteAnchorRect(&insideFixed);
        renderer()->scrollRectToVisible(revealMode, absoluteBounds, insideFixed);
    }
}

void Element::blur()
{
    cancelFocusAppearanceUpdate();
    if (treeScope().focusedElementInScope() == this) {
        if (Frame* frame = document().frame())
            frame->page()->focusController().setFocusedElement(nullptr, *frame);
        else
            document().setFocusedElement(nullptr);
    }
}

void Element::dispatchFocusInEvent(const AtomicString& eventType, RefPtr<Element>&& oldFocusedElement)
{
    ASSERT_WITH_SECURITY_IMPLICATION(NoEventDispatchAssertion::isEventAllowedInMainThread());
    ASSERT(eventType == eventNames().focusinEvent || eventType == eventNames().DOMFocusInEvent);
    dispatchScopedEvent(FocusEvent::create(eventType, true, false, document().defaultView(), 0, WTFMove(oldFocusedElement)));
}

void Element::dispatchFocusOutEvent(const AtomicString& eventType, RefPtr<Element>&& newFocusedElement)
{
    ASSERT_WITH_SECURITY_IMPLICATION(NoEventDispatchAssertion::isEventAllowedInMainThread());
    ASSERT(eventType == eventNames().focusoutEvent || eventType == eventNames().DOMFocusOutEvent);
    dispatchScopedEvent(FocusEvent::create(eventType, true, false, document().defaultView(), 0, WTFMove(newFocusedElement)));
}

void Element::dispatchFocusEvent(RefPtr<Element>&& oldFocusedElement, FocusDirection)
{
    if (document().page())
        document().page()->chrome().client().elementDidFocus(*this);

    EventDispatcher::dispatchEvent(*this, FocusEvent::create(eventNames().focusEvent, false, false, document().defaultView(), 0, WTFMove(oldFocusedElement)));
}

void Element::dispatchBlurEvent(RefPtr<Element>&& newFocusedElement)
{
    if (document().page())
        document().page()->chrome().client().elementDidBlur(*this);

    EventDispatcher::dispatchEvent(*this, FocusEvent::create(eventNames().blurEvent, false, false, document().defaultView(), 0, WTFMove(newFocusedElement)));
}

void Element::dispatchWebKitImageReadyEventForTesting()
{
    if (document().settings().webkitImageReadyEventEnabled())
        dispatchEvent(Event::create("webkitImageFrameReady", true, true));
}

bool Element::dispatchMouseForceWillBegin()
{
#if ENABLE(MOUSE_FORCE_EVENTS)
    if (!document().hasListenerType(Document::FORCEWILLBEGIN_LISTENER))
        return false;

    Frame* frame = document().frame();
    if (!frame)
        return false;

    PlatformMouseEvent platformMouseEvent { frame->eventHandler().lastKnownMousePosition(), frame->eventHandler().lastKnownMouseGlobalPosition(), NoButton, PlatformEvent::NoType, 1, false, false, false, false, WTF::currentTime(), ForceAtClick, NoTap };
    auto mouseForceWillBeginEvent = MouseEvent::create(eventNames().webkitmouseforcewillbeginEvent, document().defaultView(), platformMouseEvent, 0, nullptr);
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
    return createMarkup(*this, ChildrenOnly);
}

String Element::outerHTML() const
{
    return createMarkup(*this);
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
    if (is<Text>(node.get())) {
        auto result = mergeWithNextTextNode(downcast<Text>(*node));
        if (result.hasException())
            return result.releaseException();
    }
    if (is<Text>(prev.get())) {
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

    return plainText(rangeOfContents(*this).ptr());
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

const AtomicString& Element::pseudo() const
{
    return attributeWithoutSynchronization(pseudoAttr);
}

void Element::setPseudo(const AtomicString& value)
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
    case BEFORE:
        return host.beforePseudoElement();
    case AFTER:
        return host.afterPseudoElement();
    default:
        return nullptr;
    }
}

const RenderStyle* Element::existingComputedStyle() const
{
    if (auto* renderTreeStyle = renderStyle())
        return renderTreeStyle;

    if (hasRareData())
        return elementRareData()->computedStyle();

    return nullptr;
}

const RenderStyle& Element::resolveComputedStyle()
{
    ASSERT(isConnected());
    ASSERT(!existingComputedStyle());

    Deque<Element*, 32> elementsRequiringComputedStyle({ this });
    const RenderStyle* computedStyle = nullptr;

    // Collect ancestors until we find one that has style.
    auto composedAncestors = composedTreeAncestors(*this);
    for (auto& ancestor : composedAncestors) {
        if (auto* existingStyle = ancestor.existingComputedStyle()) {
            computedStyle = existingStyle;
            break;
        }
        elementsRequiringComputedStyle.prepend(&ancestor);
    }

    // Resolve and cache styles starting from the most distant ancestor.
    for (auto* element : elementsRequiringComputedStyle) {
        auto style = document().styleForElementIgnoringPendingStylesheets(*element, computedStyle);
        computedStyle = style.get();
        ElementRareData& rareData = element->ensureElementRareData();
        rareData.setComputedStyle(WTFMove(style));
    }

    return *computedStyle;
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
        style = &resolveComputedStyle();

    if (pseudoElementSpecifier) {
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
    if (document().hasPendingForcedStyleRecalc())
        return false;

    return true;
}

void Element::setStyleAffectedByEmpty()
{
    ensureElementRareData().setStyleAffectedByEmpty(true);
}

void Element::setStyleAffectedByFocusWithin()
{
    ensureElementRareData().setStyleAffectedByFocusWithin(true);
}

void Element::setStyleAffectedByActive()
{
    ensureElementRareData().setStyleAffectedByActive(true);
}

void Element::setChildrenAffectedByDrag()
{
    ensureElementRareData().setChildrenAffectedByDrag(true);
}

void Element::setChildrenAffectedByBackwardPositionalRules()
{
    ensureElementRareData().setChildrenAffectedByBackwardPositionalRules(true);
}

void Element::setChildrenAffectedByPropertyBasedBackwardPositionalRules()
{
    ensureElementRareData().setChildrenAffectedByPropertyBasedBackwardPositionalRules(true);
}

void Element::setChildIndex(unsigned index)
{
    ElementRareData& rareData = ensureElementRareData();
    rareData.setChildIndex(index);
}

bool Element::hasFlagsSetDuringStylingOfChildren() const
{
    if (childrenAffectedByHover() || childrenAffectedByFirstChildRules() || childrenAffectedByLastChildRules())
        return true;

    if (!hasRareData())
        return false;
    return rareDataStyleAffectedByActive()
        || rareDataChildrenAffectedByDrag()
        || rareDataChildrenAffectedByBackwardPositionalRules()
        || rareDataChildrenAffectedByPropertyBasedBackwardPositionalRules();
}

bool Element::rareDataStyleAffectedByEmpty() const
{
    ASSERT(hasRareData());
    return elementRareData()->styleAffectedByEmpty();
}

bool Element::rareDataStyleAffectedByFocusWithin() const
{
    ASSERT(hasRareData());
    return elementRareData()->styleAffectedByFocusWithin();
}

bool Element::rareDataIsNamedFlowContentElement() const
{
    ASSERT(hasRareData());
    return elementRareData()->isNamedFlowContentElement();
}

bool Element::rareDataStyleAffectedByActive() const
{
    ASSERT(hasRareData());
    return elementRareData()->styleAffectedByActive();
}

bool Element::rareDataChildrenAffectedByDrag() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByDrag();
}

bool Element::rareDataChildrenAffectedByBackwardPositionalRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByBackwardPositionalRules();
}

bool Element::rareDataChildrenAffectedByPropertyBasedBackwardPositionalRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByPropertyBasedBackwardPositionalRules();
}

unsigned Element::rareDataChildIndex() const
{
    ASSERT(hasRareData());
    return elementRareData()->childIndex();
}

void Element::setRegionOversetState(RegionOversetState state)
{
    ensureElementRareData().setRegionOversetState(state);
}

RegionOversetState Element::regionOversetState() const
{
    return hasRareData() ? elementRareData()->regionOversetState() : RegionUndefined;
}

AtomicString Element::computeInheritedLanguage() const
{
    if (const ElementData* elementData = this->elementData()) {
        if (const Attribute* attribute = elementData->findLanguageAttribute())
            return attribute->value();
    }

    // The language property is inherited, so we iterate over the parents to find the first language.
    const Node* currentNode = this;
    while ((currentNode = currentNode->parentNode())) {
        if (is<Element>(*currentNode)) {
            if (const ElementData* elementData = downcast<Element>(*currentNode).elementData()) {
                if (const Attribute* attribute = elementData->findLanguageAttribute())
                    return attribute->value();
            }
        } else if (is<Document>(*currentNode)) {
            // checking the MIME content-language
            return downcast<Document>(*currentNode).contentLanguage();
        }
    }

    return nullAtom();
}

Locale& Element::locale() const
{
    return document().getCachedLocale(computeInheritedLanguage());
}

void Element::cancelFocusAppearanceUpdate()
{
    if (hasRareData())
        elementRareData()->setNeedsFocusAppearanceUpdateSoonAfterAttach(false);
    if (document().focusedElement() == this)
        document().cancelFocusAppearanceUpdate();
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

PseudoElement* Element::beforePseudoElement() const
{
    return hasRareData() ? elementRareData()->beforePseudoElement() : nullptr;
}

PseudoElement* Element::afterPseudoElement() const
{
    return hasRareData() ? elementRareData()->afterPseudoElement() : nullptr;
}

void Element::setBeforePseudoElement(Ref<PseudoElement>&& element)
{
    ensureElementRareData().setBeforePseudoElement(WTFMove(element));
}

void Element::setAfterPseudoElement(Ref<PseudoElement>&& element)
{
    ensureElementRareData().setAfterPseudoElement(WTFMove(element));
}

static void disconnectPseudoElement(PseudoElement* pseudoElement)
{
    if (!pseudoElement)
        return;
    if (pseudoElement->renderer())
        RenderTreeUpdater::tearDownRenderers(*pseudoElement);
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
        data.setClassList(std::make_unique<DOMTokenList>(*this, HTMLNames::classAttr));
    return *data.classList();
}

DatasetDOMStringMap& Element::dataset()
{
    ElementRareData& data = ensureElementRareData();
    if (!data.dataset())
        data.setDataset(std::make_unique<DatasetDOMStringMap>(*this));
    return *data.dataset();
}

URL Element::getURLAttribute(const QualifiedName& name) const
{
#if !ASSERT_DISABLED
    if (elementData()) {
        if (const Attribute* attribute = findAttributeByName(name))
            ASSERT(isURLAttribute(*attribute));
    }
#endif
    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(getAttribute(name)));
}

URL Element::getNonEmptyURLAttribute(const QualifiedName& name) const
{
#if !ASSERT_DISABLED
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
    return parseHTMLInteger(getAttribute(attributeName)).valueOr(0);
}

void Element::setIntegralAttribute(const QualifiedName& attributeName, int value)
{
    setAttribute(attributeName, AtomicString::number(value));
}

unsigned Element::getUnsignedIntegralAttribute(const QualifiedName& attributeName) const
{
    return parseHTMLNonNegativeInteger(getAttribute(attributeName)).valueOr(0);
}

void Element::setUnsignedIntegralAttribute(const QualifiedName& attributeName, unsigned value)
{
    setAttribute(attributeName, AtomicString::number(limitToOnlyHTMLNonNegative(value)));
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
    document().requestFullScreenForElement(this, Document::EnforceIFrameAllowFullScreenRequirement);
}

bool Element::containsFullScreenElement() const
{
    return hasRareData() && elementRareData()->containsFullScreenElement();
}

void Element::setContainsFullScreenElement(bool flag)
{
    ensureElementRareData().setContainsFullScreenElement(flag);
    invalidateStyleAndLayerComposition();
}

static Element* parentCrossingFrameBoundaries(Element* element)
{
    ASSERT(element);
    return element->parentElement() ? element->parentElement() : element->document().ownerElement();
}

void Element::setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(bool flag)
{
    Element* element = this;
    while ((element = parentCrossingFrameBoundaries(element)))
        element->setContainsFullScreenElement(flag);
}
#endif

#if ENABLE(POINTER_LOCK)
void Element::requestPointerLock()
{
    if (document().page())
        document().page()->pointerLockController().requestPointerLock(this);
}
#endif

SpellcheckAttributeState Element::spellcheckAttributeState() const
{
    const AtomicString& value = attributeWithoutSynchronization(HTMLNames::spellcheckAttr);
    if (value.isNull())
        return SpellcheckAttributeDefault;
    if (value.isEmpty() || equalLettersIgnoringASCIICase(value, "true"))
        return SpellcheckAttributeTrue;
    if (equalLettersIgnoringASCIICase(value, "false"))
        return SpellcheckAttributeFalse;
    return SpellcheckAttributeDefault;
}

bool Element::isSpellCheckingEnabled() const
{
    for (const Element* element = this; element; element = element->parentOrShadowHostElement()) {
        switch (element->spellcheckAttributeState()) {
        case SpellcheckAttributeTrue:
            return true;
        case SpellcheckAttributeFalse:
            return false;
        case SpellcheckAttributeDefault:
            break;
        }
    }

    return true;
}

RenderNamedFlowFragment* Element::renderNamedFlowFragment() const
{
    if (renderer() && renderer()->isRenderNamedFlowFragmentContainer())
        return downcast<RenderBlockFlow>(*renderer()).renderNamedFlowFragment();

    return nullptr;
}

#if ENABLE(CSS_REGIONS)

bool Element::shouldMoveToFlowThread(const RenderStyle& styleToUse) const
{
#if ENABLE(FULLSCREEN_API)
    if (document().webkitIsFullScreen() && document().webkitCurrentFullScreenElement() == this)
        return false;
#endif

    if (isInShadowTree())
        return false;

    if (!styleToUse.hasFlowInto())
        return false;

    return true;
}

const AtomicString& Element::webkitRegionOverset() const
{
    document().updateLayoutIgnorePendingStylesheets();

    static NeverDestroyed<AtomicString> undefinedState("undefined", AtomicString::ConstructFromLiteral);
    if (!renderNamedFlowFragment())
        return undefinedState;

    switch (regionOversetState()) {
    case RegionFit: {
        static NeverDestroyed<AtomicString> fitState("fit", AtomicString::ConstructFromLiteral);
        return fitState;
    }
    case RegionEmpty: {
        static NeverDestroyed<AtomicString> emptyState("empty", AtomicString::ConstructFromLiteral);
        return emptyState;
    }
    case RegionOverset: {
        static NeverDestroyed<AtomicString> overflowState("overset", AtomicString::ConstructFromLiteral);
        return overflowState;
    }
    case RegionUndefined:
        return undefinedState;
    }

    ASSERT_NOT_REACHED();
    return undefinedState;
}

Vector<RefPtr<Range>> Element::webkitGetRegionFlowRanges() const
{
    Vector<RefPtr<Range>> rangeObjects;
    document().updateLayoutIgnorePendingStylesheets();
    auto* renderer = this->renderer();
    if (renderer && renderer->isRenderNamedFlowFragmentContainer()) {
        auto& namedFlowFragment = *downcast<RenderBlockFlow>(*renderer).renderNamedFlowFragment();
        if (namedFlowFragment.isValid())
            namedFlowFragment.getRanges(rangeObjects);
    }
    return rangeObjects;
}

#endif

#ifndef NDEBUG
bool Element::fastAttributeLookupAllowed(const QualifiedName& name) const
{
    if (name == HTMLNames::styleAttr)
        return false;

    if (isSVGElement())
        return !downcast<SVGElement>(*this).isAnimatableAttribute(name);

    return true;
}
#endif

#ifdef DUMP_NODE_STATISTICS
bool Element::hasNamedNodeMap() const
{
    return hasRareData() && elementRareData()->attributeMap();
}
#endif

inline void Element::updateName(const AtomicString& oldName, const AtomicString& newName)
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

void Element::updateNameForTreeScope(TreeScope& scope, const AtomicString& oldName, const AtomicString& newName)
{
    ASSERT(oldName != newName);

    if (!oldName.isEmpty())
        scope.removeElementByName(*oldName.impl(), *this);
    if (!newName.isEmpty())
        scope.addElementByName(*newName.impl(), *this);
}

void Element::updateNameForDocument(HTMLDocument& document, const AtomicString& oldName, const AtomicString& newName)
{
    ASSERT(oldName != newName);

    if (isInShadowTree())
        return;

    if (WindowNameCollection::elementMatchesIfNameAttributeMatch(*this)) {
        const AtomicString& id = WindowNameCollection::elementMatchesIfIdAttributeMatch(*this) ? getIdAttribute() : nullAtom();
        if (!oldName.isEmpty() && oldName != id)
            document.removeWindowNamedItem(*oldName.impl(), *this);
        if (!newName.isEmpty() && newName != id)
            document.addWindowNamedItem(*newName.impl(), *this);
    }

    if (DocumentNameCollection::elementMatchesIfNameAttributeMatch(*this)) {
        const AtomicString& id = DocumentNameCollection::elementMatchesIfIdAttributeMatch(*this) ? getIdAttribute() : nullAtom();
        if (!oldName.isEmpty() && oldName != id)
            document.removeDocumentNamedItem(*oldName.impl(), *this);
        if (!newName.isEmpty() && newName != id)
            document.addDocumentNamedItem(*newName.impl(), *this);
    }
}

inline void Element::updateId(const AtomicString& oldId, const AtomicString& newId, NotifyObservers notifyObservers)
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

void Element::updateIdForTreeScope(TreeScope& scope, const AtomicString& oldId, const AtomicString& newId, NotifyObservers notifyObservers)
{
    ASSERT(isInTreeScope());
    ASSERT(oldId != newId);

    if (!oldId.isEmpty())
        scope.removeElementById(*oldId.impl(), *this, notifyObservers == NotifyObservers::Yes);
    if (!newId.isEmpty())
        scope.addElementById(*newId.impl(), *this, notifyObservers == NotifyObservers::Yes);
}

void Element::updateIdForDocument(HTMLDocument& document, const AtomicString& oldId, const AtomicString& newId, HTMLDocumentNamedItemMapsUpdatingCondition condition)
{
    ASSERT(isConnected());
    ASSERT(oldId != newId);

    if (isInShadowTree())
        return;

    if (WindowNameCollection::elementMatchesIfIdAttributeMatch(*this)) {
        const AtomicString& name = condition == UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute && WindowNameCollection::elementMatchesIfNameAttributeMatch(*this) ? getNameAttribute() : nullAtom();
        if (!oldId.isEmpty() && oldId != name)
            document.removeWindowNamedItem(*oldId.impl(), *this);
        if (!newId.isEmpty() && newId != name)
            document.addWindowNamedItem(*newId.impl(), *this);
    }

    if (DocumentNameCollection::elementMatchesIfIdAttributeMatch(*this)) {
        const AtomicString& name = condition == UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute && DocumentNameCollection::elementMatchesIfNameAttributeMatch(*this) ? getNameAttribute() : nullAtom();
        if (!oldId.isEmpty() && oldId != name)
            document.removeDocumentNamedItem(*oldId.impl(), *this);
        if (!newId.isEmpty() && newId != name)
            document.addDocumentNamedItem(*newId.impl(), *this);
    }
}

void Element::updateLabel(TreeScope& scope, const AtomicString& oldForAttributeValue, const AtomicString& newForAttributeValue)
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

void Element::willModifyAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue)
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

void Element::didAddAttribute(const QualifiedName& name, const AtomicString& value)
{
    attributeChanged(name, nullAtom(), value);
    InspectorInstrumentation::didModifyDOMAttr(document(), *this, name.localName(), value);
    dispatchSubtreeModifiedEvent();
}

void Element::didModifyAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    attributeChanged(name, oldValue, newValue);
    InspectorInstrumentation::didModifyDOMAttr(document(), *this, name.localName(), newValue);
    // Do not dispatch a DOMSubtreeModified event here; see bug 81141.
}

void Element::didRemoveAttribute(const QualifiedName& name, const AtomicString& oldValue)
{
    attributeChanged(name, oldValue, nullAtom());
    InspectorInstrumentation::didRemoveDOMAttr(document(), *this, name.localName());
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

RefPtr<Attr> Element::attrIfExists(const AtomicString& localName, bool shouldIgnoreAttributeCase)
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

void Element::detachAttrNodeFromElementWithValue(Attr* attrNode, const AtomicString& value)
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
    if (!hasRareData())
        return;
    elementRareData()->resetStyleRelations();
}

void Element::clearStyleDerivedDataBeforeDetachingRenderer()
{
    unregisterNamedFlowContentElement();
    cancelFocusAppearanceUpdate();
    clearBeforePseudoElement();
    clearAfterPseudoElement();
}

void Element::clearHoverAndActiveStatusBeforeDetachingRenderer()
{
    if (!isUserActionElement())
        return;
    if (hovered())
        document().hoveredElementDidDetach(this);
    if (inActiveChain())
        document().elementInActiveChainDidDetach(this);
    document().userActionElements().didDetach(this);
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

std::optional<ElementStyle> Element::resolveCustomStyle(const RenderStyle&, const RenderStyle*)
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

    const AtomicString& oldID = getIdAttribute();
    const AtomicString& newID = other.getIdAttribute();

    if (!oldID.isNull() || !newID.isNull())
        updateId(oldID, newID, NotifyObservers::No); // Will notify observers after the attribute is actually changed.

    const AtomicString& oldName = getNameAttribute();
    const AtomicString& newName = other.getNameAttribute();

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

bool Element::hasPendingResources() const
{
    return hasRareData() && elementRareData()->hasPendingResources();
}

void Element::setHasPendingResources()
{
    ensureElementRareData().setHasPendingResources(true);
}

void Element::clearHasPendingResources()
{
    ensureElementRareData().setHasPendingResources(false);
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
ExceptionOr<void> Element::insertAdjacentHTML(const String& where, const String& markup)
{
    // Steps 1 and 2.
    auto contextElement = contextElementForInsertion(where, *this);
    if (contextElement.hasException())
        return contextElement.releaseException();
    // Step 3.
    auto fragment = createFragmentForInnerOuterHTML(contextElement.releaseReturnValue(), markup, AllowScriptingContent);
    if (fragment.hasException())
        return fragment.releaseException();
    // Step 4.
    auto result = insertAdjacent(where, fragment.releaseReturnValue());
    if (result.hasException())
        return result.releaseException();
    return { };
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

    const AtomicString& href = attributeWithoutSynchronization(HTMLNames::hrefAttr);
    if (href.isNull())
        return nullptr;

    Document& document = this->document();
    URL url = document.completeURL(href);
    if (!url.isValid())
        return nullptr;

    if (url.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(url, document.baseURL())) {
        outAnchorName = url.fragmentIdentifier();
        return document.findAnchor(outAnchorName);
    }

    return nullptr;
}

} // namespace WebCore
