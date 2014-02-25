/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004-2014 Apple Inc. All rights reserved.
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
#include "CSSParser.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClientRect.h"
#include "ClientRectList.h"
#include "ContainerNodeAlgorithms.h"
#include "DOMTokenList.h"
#include "DocumentSharedObjectPool.h"
#include "ElementIterator.h"
#include "ElementRareData.h"
#include "EventDispatcher.h"
#include "FlowThreadController.h"
#include "FocusController.h"
#include "FocusEvent.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLFormControlsCollection.h"
#include "HTMLLabelElement.h"
#include "HTMLNameCollection.h"
#include "HTMLOptionsCollection.h"
#include "HTMLParserIdioms.h"
#include "HTMLSelectElement.h"
#include "HTMLTableRowsCollection.h"
#include "InsertionPoint.h"
#include "KeyboardEvent.h"
#include "MutationObserverInterestGroup.h"
#include "MutationRecord.h"
#include "NodeRenderStyle.h"
#include "PlatformWheelEvent.h"
#include "PointerLockController.h"
#include "RenderNamedFlowFragment.h"
#include "RenderRegion.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SelectorQuery.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "VoidCallback.h"
#include "WheelEvent.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "htmlediting.h"
#include <wtf/BitVector.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>

namespace WebCore {

using namespace HTMLNames;
using namespace XMLNames;

static inline bool shouldIgnoreAttributeCase(const Element& element)
{
    return element.isHTMLElement() && element.document().isHTMLDocument();
}

typedef Vector<RefPtr<Attr>> AttrNodeList;
typedef HashMap<Element*, OwnPtr<AttrNodeList>> AttrNodeListMap;

static AttrNodeListMap& attrNodeListMap()
{
    DEFINE_STATIC_LOCAL(AttrNodeListMap, map, ());
    return map;
}

static AttrNodeList* attrNodeListForElement(Element* element)
{
    if (!element->hasSyntheticAttrChildNodes())
        return 0;
    ASSERT(attrNodeListMap().contains(element));
    return attrNodeListMap().get(element);
}

static AttrNodeList& ensureAttrNodeListForElement(Element* element)
{
    if (element->hasSyntheticAttrChildNodes()) {
        ASSERT(attrNodeListMap().contains(element));
        return *attrNodeListMap().get(element);
    }
    ASSERT(!attrNodeListMap().contains(element));
    element->setHasSyntheticAttrChildNodes(true);
    AttrNodeListMap::AddResult result = attrNodeListMap().add(element, adoptPtr(new AttrNodeList));
    return *result.iterator->value;
}

static void removeAttrNodeListForElement(Element* element)
{
    ASSERT(element->hasSyntheticAttrChildNodes());
    ASSERT(attrNodeListMap().contains(element));
    attrNodeListMap().remove(element);
    element->setHasSyntheticAttrChildNodes(false);
}

static Attr* findAttrNodeInList(AttrNodeList& attrNodeList, const QualifiedName& name)
{
    for (unsigned i = 0; i < attrNodeList.size(); ++i) {
        if (attrNodeList.at(i)->qualifiedName() == name)
            return attrNodeList.at(i).get();
    }
    return 0;
}

PassRefPtr<Element> Element::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new Element(tagName, document, CreateElement));
}

Element::~Element()
{
#ifndef NDEBUG
    if (document().hasLivingRenderTree()) {
        // When the document is not destroyed, an element that was part of a named flow
        // content nodes should have been removed from the content nodes collection
        // and the inNamedFlow flag reset.
        ASSERT(!inNamedFlow());
    }
#endif

    ASSERT(!beforePseudoElement());
    ASSERT(!afterPseudoElement());

    removeShadowRoot();

    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

    if (hasPendingResources()) {
        document().accessSVGExtensions()->removeElementFromPendingResources(this);
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

void Element::setTabIndexExplicitly(short tabIndex)
{
    ensureElementRareData().setTabIndexExplicitly(tabIndex);
}

bool Element::supportsFocus() const
{
    return hasRareData() && elementRareData()->tabIndexSetExplicitly();
}

Element* Element::focusDelegate()
{
    return this;
}

short Element::tabIndex() const
{
    return hasRareData() ? elementRareData()->tabIndex() : 0;
}

bool Element::isKeyboardFocusable(KeyboardEvent*) const
{
    return isFocusable() && tabIndex() >= 0;
}

bool Element::isMouseFocusable() const
{
    return isFocusable();
}

bool Element::shouldUseInputMethod()
{
    return isContentEditable(UserSelectAllIsAlwaysNonEditable);
}

bool Element::dispatchMouseEvent(const PlatformMouseEvent& platformEvent, const AtomicString& eventType, int detail, Element* relatedTarget)
{
    if (isDisabledFormControl())
        return false;

    RefPtr<MouseEvent> mouseEvent = MouseEvent::create(eventType, document().defaultView(), platformEvent, detail, relatedTarget);

    if (mouseEvent->type().isEmpty())
        return true; // Shouldn't happen.

    ASSERT(!mouseEvent->target() || mouseEvent->target() != relatedTarget);
    bool didNotSwallowEvent = dispatchEvent(mouseEvent) && !mouseEvent->defaultHandled();

    if (mouseEvent->type() == eventNames().clickEvent && mouseEvent->detail() == 2) {
        // Special case: If it's a double click event, we also send the dblclick event. This is not part
        // of the DOM specs, but is used for compatibility with the ondblclick="" attribute. This is treated
        // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
        RefPtr<MouseEvent> doubleClickEvent = MouseEvent::create();
        doubleClickEvent->initMouseEvent(eventNames().dblclickEvent,
            mouseEvent->bubbles(), mouseEvent->cancelable(), mouseEvent->view(), mouseEvent->detail(),
            mouseEvent->screenX(), mouseEvent->screenY(), mouseEvent->clientX(), mouseEvent->clientY(),
            mouseEvent->ctrlKey(), mouseEvent->altKey(), mouseEvent->shiftKey(), mouseEvent->metaKey(),
            mouseEvent->button(), relatedTarget);

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
    if (!(event.deltaX() || event.deltaY()))
        return true;

    RefPtr<WheelEvent> wheelEvent = WheelEvent::create(event, document().defaultView());
    return EventDispatcher::dispatchEvent(this, wheelEvent) && !wheelEvent->defaultHandled();
}

bool Element::dispatchKeyEvent(const PlatformKeyboardEvent& platformEvent)
{
    RefPtr<KeyboardEvent> event = KeyboardEvent::create(platformEvent, document().defaultView());
    return EventDispatcher::dispatchEvent(this, event) && !event->defaultHandled();
}

void Element::dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions eventOptions, SimulatedClickVisualOptions visualOptions)
{
    EventDispatcher::dispatchSimulatedClick(this, underlyingEvent, eventOptions, visualOptions);
}

DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(Element, blur);
DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(Element, error);
DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(Element, focus);
DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(Element, load);

PassRefPtr<Node> Element::cloneNode(bool deep)
{
    return deep ? cloneElementWithChildren() : cloneElementWithoutChildren();
}

PassRefPtr<Element> Element::cloneElementWithChildren()
{
    RefPtr<Element> clone = cloneElementWithoutChildren();
    cloneChildNodes(clone.get());
    return clone.release();
}

PassRefPtr<Element> Element::cloneElementWithoutChildren()
{
    RefPtr<Element> clone = cloneElementWithoutAttributesAndChildren();
    // This will catch HTML elements in the wrong namespace that are not correctly copied.
    // This is a sanity check as HTML overloads some of the DOM methods.
    ASSERT(isHTMLElement() == clone->isHTMLElement());

    clone->cloneDataFromElement(*this);
    return clone.release();
}

PassRefPtr<Element> Element::cloneElementWithoutAttributesAndChildren()
{
    return document().createElement(tagQName(), false);
}

PassRefPtr<Attr> Element::detachAttribute(unsigned index)
{
    ASSERT(elementData());

    const Attribute& attribute = elementData()->attributeAt(index);

    RefPtr<Attr> attrNode = attrIfExists(attribute.name());
    if (attrNode)
        detachAttrNodeFromElementWithValue(attrNode.get(), attribute.value());
    else
        attrNode = Attr::create(document(), attribute.name(), attribute.value());

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    return attrNode.release();
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
        setAttribute(name, emptyAtom);
    else
        removeAttribute(name);
}

NamedNodeMap* Element::attributes() const
{
    ElementRareData& rareData = const_cast<Element*>(this)->ensureElementRareData();
    if (NamedNodeMap* attributeMap = rareData.attributeMap())
        return attributeMap;

    rareData.setAttributeMap(NamedNodeMap::create(const_cast<Element&>(*this)));
    return rareData.attributeMap();
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
        toSVGElement(this)->synchronizeAnimatedSVGAttribute(anyQName());
    }
}

inline void Element::synchronizeAttribute(const QualifiedName& name) const
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
        toSVGElement(this)->synchronizeAnimatedSVGAttribute(name);
    }
}

inline void Element::synchronizeAttribute(const AtomicString& localName) const
{
    // This version of synchronizeAttribute() is streamlined for the case where you don't have a full QualifiedName,
    // e.g when called from DOM API.
    if (!elementData())
        return;
    if (elementData()->styleAttributeIsDirty() && equalPossiblyIgnoringCase(localName, styleAttr.localName(), shouldIgnoreAttributeCase(*this))) {
        ASSERT_WITH_SECURITY_IMPLICATION(isStyledElement());
        static_cast<const StyledElement*>(this)->synchronizeStyleAttributeInternal();
        return;
    }

    if (elementData()->animatedSVGAttributesAreDirty()) {
        // We're not passing a namespace argument on purpose. SVGNames::*Attr are defined w/o namespaces as well.
        ASSERT_WITH_SECURITY_IMPLICATION(isSVGElement());
        toSVGElement(this)->synchronizeAnimatedSVGAttribute(QualifiedName(nullAtom, localName, nullAtom));
    }
}

const AtomicString& Element::getAttribute(const QualifiedName& name) const
{
    if (!elementData())
        return nullAtom;
    synchronizeAttribute(name);
    if (const Attribute* attribute = findAttributeByName(name))
        return attribute->value();
    return nullAtom;
}

bool Element::isFocusable() const
{
    if (!inDocument() || !supportsFocus())
        return false;

    // Elements in canvas fallback content are not rendered, but they are allowed to be
    // focusable as long as their canvas is displayed and visible.
    if (isInCanvasSubtree()) {
        const Element* e = this;
        while (e && !e->hasLocalName(canvasTag))
            e = e->parentElement();
        ASSERT(e);
        return e->renderer() && e->renderer()->style().visibility() == VISIBLE;
    }

    if (!renderer()) {
        // If the node is in a display:none tree it might say it needs style recalc but
        // the whole document is actually up to date.
        ASSERT(!needsStyleRecalc() || !document().childNeedsStyleRecalc());
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

    if (!renderer())
        return;

    bool reactsToPress = renderStyle()->affectedByActive() || childrenAffectedByActive();
    if (reactsToPress)
        setNeedsStyleRecalc();

    if (renderer()->style().hasAppearance() && renderer()->theme().stateChanged(renderer(), PressedState))
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
    setNeedsStyleRecalc();
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
            setNeedsStyleRecalc();

        return;
    }

    if (renderer()->style().affectedByHover() || childrenAffectedByHover())
        setNeedsStyleRecalc();

    if (renderer()->style().hasAppearance())
        renderer()->theme().stateChanged(renderer(), HoverState);
}

void Element::scrollIntoView(bool alignToTop) 
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    LayoutRect bounds = boundingBox();
    // Align to the top / bottom and to the closest edge.
    if (alignToTop)
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways);
    else
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignBottomAlways);
}

void Element::scrollIntoViewIfNeeded(bool centerIfNeeded)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    LayoutRect bounds = boundingBox();
    if (centerIfNeeded)
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded);
    else
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded);
}

void Element::scrollByUnits(int units, ScrollGranularity granularity)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    if (!renderer()->hasOverflowClip())
        return;

    ScrollDirection direction = ScrollDown;
    if (units < 0) {
        direction = ScrollUp;
        units = -units;
    }
    Element* stopElement = this;
    toRenderBox(renderer())->scroll(direction, granularity, units, &stopElement);
}

void Element::scrollByLines(int lines)
{
    scrollByUnits(lines, ScrollByLine);
}

void Element::scrollByPages(int pages)
{
    scrollByUnits(pages, ScrollByPage);
}

static float localZoomForRenderer(RenderElement* renderer)
{
    // FIXME: This does the wrong thing if two opposing zooms are in effect and canceled each
    // other out, but the alternative is that we'd have to crawl up the whole render tree every
    // time (or store an additional bit in the RenderStyle to indicate that a zoom was specified).
    float zoomFactor = 1;
    if (renderer->style().effectiveZoom() != 1) {
        // Need to find the nearest enclosing RenderElement that set up
        // a differing zoom, and then we divide our result by it to eliminate the zoom.
        RenderElement* prev = renderer;
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

static int adjustForLocalZoom(LayoutUnit value, RenderElement* renderer)
{
    float zoomFactor = localZoomForRenderer(renderer);
    if (zoomFactor == 1)
        return value;
#if ENABLE(SUBPIXEL_LAYOUT)
    return lroundf(value / zoomFactor);
#else
    // Needed because computeLengthInt truncates (rather than rounds) when scaling up.
    if (zoomFactor > 1)
        value++;
    return static_cast<int>(value / zoomFactor);
#endif
}

int Element::offsetLeft()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustForLocalZoom(renderer->pixelSnappedOffsetLeft(), renderer);
    return 0;
}

int Element::offsetTop()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustForLocalZoom(renderer->pixelSnappedOffsetTop(), renderer);
    return 0;
}

int Element::offsetWidth()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
#if ENABLE(SUBPIXEL_LAYOUT)
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedOffsetWidth(), *renderer).round();
#else
        return adjustForAbsoluteZoom(renderer->pixelSnappedOffsetWidth(), *renderer);
#endif
    return 0;
}

int Element::offsetHeight()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
#if ENABLE(SUBPIXEL_LAYOUT)
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedOffsetHeight(), *renderer).round();
#else
        return adjustForAbsoluteZoom(renderer->pixelSnappedOffsetHeight(), *renderer);
#endif
    return 0;
}

Element* Element::bindingsOffsetParent()
{
    Element* element = offsetParent();
    if (!element || !element->isInShadowTree())
        return element;
    return element->containingShadowRoot()->type() == ShadowRoot::UserAgentShadowRoot ? 0 : element;
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

    if (RenderBox* renderer = renderBox())
        return adjustForAbsoluteZoom(roundToInt(renderer->clientLeft()), *renderer);
    return 0;
}

int Element::clientTop()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (RenderBox* renderer = renderBox())
        return adjustForAbsoluteZoom(roundToInt(renderer->clientTop()), *renderer);
    return 0;
}

int Element::clientWidth()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!document().hasLivingRenderTree())
        return 0;
    RenderView& renderView = *document().renderView();

    // When in strict mode, clientWidth for the document element should return the width of the containing frame.
    // When in quirks mode, clientWidth for the body element should return the width of the containing frame.
    bool inQuirksMode = document().inQuirksMode();
    if ((!inQuirksMode && document().documentElement() == this) || (inQuirksMode && isHTMLElement() && document().body() == this))
        return adjustForAbsoluteZoom(renderView.frameView().layoutWidth(), renderView);
    
    if (RenderBox* renderer = renderBox())
#if ENABLE(SUBPIXEL_LAYOUT)
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedClientWidth(), *renderer).round();
#else
        return adjustForAbsoluteZoom(renderer->pixelSnappedClientWidth(), *renderer);
#endif
    return 0;
}

int Element::clientHeight()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!document().hasLivingRenderTree())
        return 0;
    RenderView& renderView = *document().renderView();

    // When in strict mode, clientHeight for the document element should return the height of the containing frame.
    // When in quirks mode, clientHeight for the body element should return the height of the containing frame.
    bool inQuirksMode = document().inQuirksMode();
    if ((!inQuirksMode && document().documentElement() == this) || (inQuirksMode && isHTMLElement() && document().body() == this))
        return adjustForAbsoluteZoom(renderView.frameView().layoutHeight(), renderView);

    if (RenderBox* renderer = renderBox())
#if ENABLE(SUBPIXEL_LAYOUT)
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedClientHeight(), *renderer).round();
#else
        return adjustForAbsoluteZoom(renderer->pixelSnappedClientHeight(), *renderer);
#endif
    return 0;
}

int Element::scrollLeft()
{
    if (document().documentElement() == this && document().inQuirksMode())
        return 0;

    document().updateLayoutIgnorePendingStylesheets();

    if (!document().hasLivingRenderTree())
        return 0;
    RenderView& renderView = *document().renderView();

    if (document().documentElement() == this)
        return adjustForAbsoluteZoom(renderView.frameView().scrollX(), renderView);

    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->scrollLeft(), *rend);
    return 0;
}

int Element::scrollTop()
{
    if (document().documentElement() == this && document().inQuirksMode())
        return 0;

    document().updateLayoutIgnorePendingStylesheets();

    if (!document().hasLivingRenderTree())
        return 0;
    RenderView& renderView = *document().renderView();

    if (document().documentElement() == this)
        return adjustForAbsoluteZoom(renderView.frameView().scrollY(), renderView);

    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->scrollTop(), *rend);
    return 0;
}

void Element::setScrollLeft(int newLeft)
{
    if (document().documentElement() == this && document().inQuirksMode())
        return;

    document().updateLayoutIgnorePendingStylesheets();

    if (!document().hasLivingRenderTree())
        return;

    if (document().documentElement() == this) {
        RenderView& renderView = *document().renderView();
        int zoom = renderView.style().effectiveZoom();
        renderView.frameView().setScrollPosition(IntPoint(newLeft * zoom, renderView.frameView().scrollY() * zoom));
        return;
    }

    if (RenderBox* rend = renderBox())
        rend->setScrollLeft(static_cast<int>(newLeft * rend->style().effectiveZoom()));
}

void Element::setScrollTop(int newTop)
{
    if (document().documentElement() == this && document().inQuirksMode())
        return;

    document().updateLayoutIgnorePendingStylesheets();

    if (!document().hasLivingRenderTree())
        return;

    if (document().documentElement() == this) {
        RenderView& renderView = *document().renderView();
        int zoom = renderView.style().effectiveZoom();
        renderView.frameView().setScrollPosition(IntPoint(renderView.frameView().scrollX() * zoom, newTop * zoom));
        return;
    }

    if (RenderBox* rend = renderBox())
        rend->setScrollTop(static_cast<int>(newTop * rend->style().effectiveZoom()));
}

int Element::scrollWidth()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->scrollWidth(), *rend);
    return 0;
}

int Element::scrollHeight()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->scrollHeight(), *rend);
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
        SVGElement* svgElement = toSVGElement(this);
        FloatRect localRect;
        if (svgElement->getBoundingBox(localRect))
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

PassRefPtr<ClientRectList> Element::getClientRects()
{
    document().updateLayoutIgnorePendingStylesheets();

    RenderBoxModelObject* renderBoxModelObject = this->renderBoxModelObject();
    if (!renderBoxModelObject)
        return ClientRectList::create();

    // FIXME: Handle SVG elements.
    // FIXME: Handle table/inline-table with a caption.

    Vector<FloatQuad> quads;
    renderBoxModelObject->absoluteQuads(quads);
    document().adjustFloatQuadsForScrollAndAbsoluteZoomAndFrameScale(quads, renderBoxModelObject->style());
    return ClientRectList::create(quads);
}

PassRefPtr<ClientRect> Element::getBoundingClientRect()
{
    document().updateLayoutIgnorePendingStylesheets();

    Vector<FloatQuad> quads;
    if (isSVGElement() && renderer() && !renderer()->isSVGRoot()) {
        // Get the bounding rectangle from the SVG model.
        SVGElement* svgElement = toSVGElement(this);
        FloatRect localRect;
        if (svgElement->getBoundingBox(localRect))
            quads.append(renderer()->localToAbsoluteQuad(localRect));
    } else {
        // Get the bounding rectangle from the box model.
        if (renderBoxModelObject())
            renderBoxModelObject()->absoluteQuads(quads);
    }

    if (quads.isEmpty())
        return ClientRect::create();

    FloatRect result = quads[0].boundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        result.unite(quads[i].boundingBox());

    document().adjustFloatRectForScrollAndAbsoluteZoomAndFrameScale(result, renderer()->style());
    return ClientRect::create(result);
}

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
        return nullAtom;
    synchronizeAttribute(localName);
    if (const Attribute* attribute = elementData()->findAttributeByName(localName, shouldIgnoreAttributeCase(*this)))
        return attribute->value();
    return nullAtom;
}

const AtomicString& Element::getAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    return getAttribute(QualifiedName(nullAtom, localName, namespaceURI));
}

void Element::setAttribute(const AtomicString& localName, const AtomicString& value, ExceptionCode& ec)
{
    if (!Document::isValidName(localName)) {
        ec = INVALID_CHARACTER_ERR;
        return;
    }

    synchronizeAttribute(localName);
    const AtomicString& caseAdjustedLocalName = shouldIgnoreAttributeCase(*this) ? localName.lower() : localName;

    unsigned index = elementData() ? elementData()->findAttributeIndexByName(caseAdjustedLocalName, false) : ElementData::attributeNotFound;
    const QualifiedName& qName = index != ElementData::attributeNotFound ? attributeAt(index).name() : QualifiedName(nullAtom, caseAdjustedLocalName, nullAtom);
    setAttributeInternal(index, qName, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setAttribute(const QualifiedName& name, const AtomicString& value)
{
    synchronizeAttribute(name);
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

    bool valueChanged = newValue != attributeAt(index).value();
    QualifiedName attributeName = (!inSynchronizationOfLazyAttribute || valueChanged) ? attributeAt(index).name() : name;

    if (!inSynchronizationOfLazyAttribute)
        willModifyAttribute(attributeName, attributeAt(index).value(), newValue);

    if (valueChanged) {
        // If there is an Attr node hooked to this attribute, the Attr::setValue() call below
        // will write into the ElementData.
        // FIXME: Refactor this so it makes some sense.
        if (RefPtr<Attr> attrNode = inSynchronizationOfLazyAttribute ? 0 : attrIfExists(attributeName))
            attrNode->setValue(newValue);
        else
            ensureUniqueElementData().attributeAt(index).setValue(newValue);
    }

    if (!inSynchronizationOfLazyAttribute)
        didModifyAttribute(attributeName, newValue);
}

static inline AtomicString makeIdForStyleResolution(const AtomicString& value, bool inQuirksMode)
{
    if (inQuirksMode)
        return value.lower();
    return value;
}

static bool checkNeedsStyleInvalidationForIdChange(const AtomicString& oldId, const AtomicString& newId, StyleResolver* styleResolver)
{
    ASSERT(newId != oldId);
    if (!oldId.isEmpty() && styleResolver->hasSelectorForId(oldId))
        return true;
    if (!newId.isEmpty() && styleResolver->hasSelectorForId(newId))
        return true;
    return false;
}

void Element::attributeChanged(const QualifiedName& name, const AtomicString& newValue, AttributeModificationReason)
{
    parseAttribute(name, newValue);

    document().incDOMTreeVersion();

    StyleResolver* styleResolver = document().styleResolverIfExists();
    bool testShouldInvalidateStyle = inRenderedDocument() && styleResolver && styleChangeType() < FullStyleChange;
    bool shouldInvalidateStyle = false;

    if (isIdAttributeName(name)) {
        AtomicString oldId = elementData()->idForStyleResolution();
        AtomicString newId = makeIdForStyleResolution(newValue, document().inQuirksMode());
        if (newId != oldId) {
            elementData()->setIdForStyleResolution(newId);
            shouldInvalidateStyle = testShouldInvalidateStyle && checkNeedsStyleInvalidationForIdChange(oldId, newId, styleResolver);
        }
    } else if (name == classAttr)
        classAttributeChanged(newValue);
    else if (name == HTMLNames::nameAttr)
        elementData()->setHasNameAttribute(!newValue.isNull());
    else if (name == HTMLNames::pseudoAttr)
        shouldInvalidateStyle |= testShouldInvalidateStyle && isInShadowTree();


    invalidateNodeListAndCollectionCachesInAncestors(&name, this);

    // If there is currently no StyleResolver, we can't be sure that this attribute change won't affect style.
    shouldInvalidateStyle |= !styleResolver;

    if (shouldInvalidateStyle)
        setNeedsStyleRecalc();

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

static bool checkSelectorForClassChange(const SpaceSplitString& changedClasses, const StyleResolver& styleResolver)
{
    unsigned changedSize = changedClasses.size();
    for (unsigned i = 0; i < changedSize; ++i) {
        if (styleResolver.hasSelectorForClass(changedClasses[i]))
            return true;
    }
    return false;
}

static bool checkSelectorForClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses, const StyleResolver& styleResolver)
{
    unsigned oldSize = oldClasses.size();
    if (!oldSize)
        return checkSelectorForClassChange(newClasses, styleResolver);
    BitVector remainingClassBits;
    remainingClassBits.ensureSize(oldSize);
    // Class vectors tend to be very short. This is faster than using a hash table.
    unsigned newSize = newClasses.size();
    for (unsigned i = 0; i < newSize; ++i) {
        bool foundFromBoth = false;
        for (unsigned j = 0; j < oldSize; ++j) {
            if (newClasses[i] == oldClasses[j]) {
                remainingClassBits.quickSet(j);
                foundFromBoth = true;
            }
        }
        if (foundFromBoth)
            continue;
        if (styleResolver.hasSelectorForClass(newClasses[i]))
            return true;
    }
    for (unsigned i = 0; i < oldSize; ++i) {
        // If the bit is not set the the corresponding class has been removed.
        if (remainingClassBits.quickGet(i))
            continue;
        if (styleResolver.hasSelectorForClass(oldClasses[i]))
            return true;
    }
    return false;
}

void Element::classAttributeChanged(const AtomicString& newClassString)
{
    StyleResolver* styleResolver = document().styleResolverIfExists();
    bool testShouldInvalidateStyle = inRenderedDocument() && styleResolver && styleChangeType() < FullStyleChange;
    bool shouldInvalidateStyle = false;

    if (classStringHasClassName(newClassString)) {
        const bool shouldFoldCase = document().inQuirksMode();
        // Note: We'll need ElementData, but it doesn't have to be UniqueElementData.
        if (!elementData())
            ensureUniqueElementData();
        const SpaceSplitString oldClasses = elementData()->classNames();
        elementData()->setClass(newClassString, shouldFoldCase);
        const SpaceSplitString& newClasses = elementData()->classNames();
        shouldInvalidateStyle = testShouldInvalidateStyle && checkSelectorForClassChange(oldClasses, newClasses, *styleResolver);
    } else if (elementData()) {
        const SpaceSplitString& oldClasses = elementData()->classNames();
        shouldInvalidateStyle = testShouldInvalidateStyle && checkSelectorForClassChange(oldClasses, *styleResolver);
        elementData()->clearClass();
    }

    if (hasRareData())
        elementRareData()->clearClassListValueForQuirksMode();

    if (shouldInvalidateStyle)
        setNeedsStyleRecalc();
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
    size_t destination = 0;
    for (size_t source = 0; source < attributeVector.size(); ++source) {
        if (isEventHandlerAttribute(attributeVector[source])
            || isJavaScriptURLAttribute(attributeVector[source])
            || isHTMLContentAttribute(attributeVector[source]))
            continue;

        if (source != destination)
            attributeVector[destination] = attributeVector[source];

        ++destination;
    }
    attributeVector.shrink(destination);
}

void Element::parserSetAttributes(const Vector<Attribute>& attributeVector)
{
    ASSERT(!inDocument());
    ASSERT(!parentNode());
    ASSERT(!m_elementData);

    if (attributeVector.isEmpty())
        return;

    if (document().sharedObjectPool())
        m_elementData = document().sharedObjectPool()->cachedShareableElementDataWithAttributes(attributeVector);
    else
        m_elementData = ShareableElementData::createWithAttributes(attributeVector);

    // Use attributeVector instead of m_elementData because attributeChanged might modify m_elementData.
    for (unsigned i = 0; i < attributeVector.size(); ++i)
        attributeChanged(attributeVector[i].name(), attributeVector[i].value(), ModifiedDirectly);
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

void Element::setPrefix(const AtomicString& prefix, ExceptionCode& ec)
{
    ec = 0;
    checkSetPrefix(prefix, ec);
    if (ec)
        return;

    m_tagName.setPrefix(prefix.isEmpty() ? AtomicString() : prefix);
}

URL Element::baseURI() const
{
    const AtomicString& baseAttribute = getAttribute(baseAttr);
    URL base(URL(), baseAttribute);
    if (!base.protocol().isEmpty())
        return base;

    ContainerNode* parent = parentNode();
    if (!parent)
        return base;

    const URL& parentBase = parent->baseURI();
    if (parentBase.isNull())
        return base;

    return URL(parentBase, baseAttribute);
}

const AtomicString& Element::imageSourceURL() const
{
    return getAttribute(srcAttr);
}

bool Element::rendererIsNeeded(const RenderStyle& style)
{
    return style.display() != NONE;
}

RenderPtr<RenderElement> Element::createElementRenderer(PassRef<RenderStyle> style)
{
    return RenderElement::createFor(*this, std::move(style));
}

Node::InsertionNotificationRequest Element::insertedInto(ContainerNode& insertionPoint)
{
    bool wasInDocument = inDocument();
    // need to do superclass processing first so inDocument() is true
    // by the time we reach updateId
    ContainerNode::insertedInto(insertionPoint);
    ASSERT(!wasInDocument || inDocument());

#if ENABLE(FULLSCREEN_API)
    if (containsFullScreenElement() && parentElement() && !parentElement()->containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);
#endif

    if (!insertionPoint.isInTreeScope())
        return InsertionDone;

    if (hasRareData())
        elementRareData()->clearClassListValueForQuirksMode();

    TreeScope* newScope = &insertionPoint.treeScope();
    HTMLDocument* newDocument = !wasInDocument && inDocument() && newScope->documentScope().isHTMLDocument() ? toHTMLDocument(&newScope->documentScope()) : nullptr;
    if (newScope != &treeScope())
        newScope = nullptr;

    const AtomicString& idValue = getIdAttribute();
    if (!idValue.isNull()) {
        if (newScope)
            updateIdForTreeScope(*newScope, nullAtom, idValue);
        if (newDocument)
            updateIdForDocument(*newDocument, nullAtom, idValue, AlwaysUpdateHTMLDocumentNamedItemMaps);
    }

    const AtomicString& nameValue = getNameAttribute();
    if (!nameValue.isNull()) {
        if (newScope)
            updateNameForTreeScope(*newScope, nullAtom, nameValue);
        if (newDocument)
            updateNameForDocument(*newDocument, nullAtom, nameValue);
    }

    if (newScope && hasTagName(labelTag)) {
        if (newScope->shouldCacheLabelsByForAttribute())
            updateLabel(*newScope, nullAtom, fastGetAttribute(forAttr));
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
        document().page()->pointerLockController()->elementRemoved(this);
#endif

    setSavedLayerScrollOffset(IntSize());

    if (insertionPoint.isInTreeScope()) {
        TreeScope* oldScope = &insertionPoint.treeScope();
        HTMLDocument* oldDocument = inDocument() && oldScope->documentScope().isHTMLDocument() ? toHTMLDocument(&oldScope->documentScope()) : nullptr;
        if (oldScope != &treeScope())
            oldScope = nullptr;

        const AtomicString& idValue = getIdAttribute();
        if (!idValue.isNull()) {
            if (oldScope)
                updateIdForTreeScope(*oldScope, idValue, nullAtom);
            if (oldDocument)
                updateIdForDocument(*oldDocument, idValue, nullAtom, AlwaysUpdateHTMLDocumentNamedItemMaps);
        }

        const AtomicString& nameValue = getNameAttribute();
        if (!nameValue.isNull()) {
            if (oldScope)
                updateNameForTreeScope(*oldScope, nameValue, nullAtom);
            if (oldDocument)
                updateNameForDocument(*oldDocument, nameValue, nullAtom);
        }

        if (oldScope && hasTagName(labelTag)) {
            if (oldScope->shouldCacheLabelsByForAttribute())
                updateLabel(*oldScope, fastGetAttribute(forAttr), nullAtom);
        }
    }

    ContainerNode::removedFrom(insertionPoint);

    if (hasPendingResources())
        document().accessSVGExtensions()->removeElementFromPendingResources(this);
}

void Element::unregisterNamedFlowContentElement()
{
    if (document().cssRegionsEnabled() && inNamedFlow() && document().renderView())
        document().renderView()->flowThreadController().unregisterNamedFlowContentElement(*this);
}

PassRef<RenderStyle> Element::styleForRenderer()
{
    if (hasCustomStyleResolveCallbacks()) {
        if (RefPtr<RenderStyle> style = customStyleForRenderer())
            return style.releaseNonNull();
    }

    return document().ensureStyleResolver().styleForElement(this);
}

ShadowRoot* Element::shadowRoot() const
{
    return hasRareData() ? elementRareData()->shadowRoot() : 0;
}

void Element::didAffectSelector(AffectedSelectorMask)
{
    setNeedsStyleRecalc();
}

static bool shouldUseNodeRenderingTraversalSlowPath(const Element& element)
{
    if (element.isShadowRoot())
        return true;
    if (element.isPseudoElement() || element.beforePseudoElement() || element.afterPseudoElement())
        return true;
    return element.isInsertionPoint() || element.shadowRoot();
}

void Element::resetNeedsNodeRenderingTraversalSlowPath()
{
    setNeedsNodeRenderingTraversalSlowPath(shouldUseNodeRenderingTraversalSlowPath(*this));
}

void Element::addShadowRoot(PassRefPtr<ShadowRoot> newShadowRoot)
{
    ASSERT(!shadowRoot());

    ShadowRoot* shadowRoot = newShadowRoot.get();
    ensureElementRareData().setShadowRoot(newShadowRoot);

    shadowRoot->setHostElement(this);
    shadowRoot->setParentTreeScope(&treeScope());
    shadowRoot->distributor().didShadowBoundaryChange(this);

    ChildNodeInsertionNotifier(*this).notify(*shadowRoot);

    resetNeedsNodeRenderingTraversalSlowPath();

    setNeedsStyleRecalc(ReconstructRenderTree);

    InspectorInstrumentation::didPushShadowRoot(this, shadowRoot);
}

void Element::removeShadowRoot()
{
    RefPtr<ShadowRoot> oldRoot = shadowRoot();
    if (!oldRoot)
        return;
    InspectorInstrumentation::willPopShadowRoot(this, oldRoot.get());
    document().removeFocusedNodeOfSubtree(oldRoot.get());

    ASSERT(!oldRoot->renderer());

    elementRareData()->clearShadowRoot();

    oldRoot->setHostElement(0);
    oldRoot->setParentTreeScope(&document());

    ChildNodeRemovalNotifier(*this).notify(*oldRoot);

    oldRoot->distributor().invalidateDistribution(this);
}

PassRefPtr<ShadowRoot> Element::createShadowRoot(ExceptionCode& ec)
{
    if (alwaysCreateUserAgentShadowRoot())
        ensureUserAgentShadowRoot();

    ec = HIERARCHY_REQUEST_ERR;
    return nullptr;
}

ShadowRoot* Element::userAgentShadowRoot() const
{
    if (ShadowRoot* shadowRoot = this->shadowRoot()) {
        ASSERT(shadowRoot->type() == ShadowRoot::UserAgentShadowRoot);
        return shadowRoot;
    }
    return nullptr;
}

ShadowRoot& Element::ensureUserAgentShadowRoot()
{
    ShadowRoot* shadowRoot = userAgentShadowRoot();
    if (!shadowRoot) {
        addShadowRoot(ShadowRoot::create(document(), ShadowRoot::UserAgentShadowRoot));
        shadowRoot = userAgentShadowRoot();
        didAddUserAgentShadowRoot(shadowRoot);
    }
    return *shadowRoot;
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
    case ENTITY_REFERENCE_NODE:
        return true;
    default:
        break;
    }
    return false;
}

static void checkForEmptyStyleChange(Element* element, RenderStyle* style)
{
    if (!style && !element->styleAffectedByEmpty())
        return;

    if (!style || (element->styleAffectedByEmpty() && (!style->emptyState() || element->hasChildNodes())))
        element->setNeedsStyleRecalc();
}

enum SiblingCheckType { FinishedParsingChildren, SiblingElementRemoved, Other };

static void checkForSiblingStyleChanges(Element* parent, SiblingCheckType checkType, Element* elementBeforeChange, Element* elementAfterChange)
{
    RenderStyle* style = parent->renderStyle();
    // :empty selector.
    checkForEmptyStyleChange(parent, style);
    
    if (!style || (parent->needsStyleRecalc() && parent->childrenAffectedByPositionalRules()))
        return;

    // :first-child.  In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    // |afterChange| is 0 in the parser case, so it works out that we'll skip this block.
    if (parent->childrenAffectedByFirstChildRules() && elementAfterChange) {
        // Find our new first child.
        Element* newFirstElement = ElementTraversal::firstChild(parent);
        // Find the first element node following |afterChange|

        // This is the insert/append case.
        if (newFirstElement != elementAfterChange && elementAfterChange->renderStyle() && elementAfterChange->renderStyle()->firstChildState())
            elementAfterChange->setNeedsStyleRecalc();
            
        // We also have to handle node removal.
        if (checkType == SiblingElementRemoved && newFirstElement == elementAfterChange && newFirstElement && (!newFirstElement->renderStyle() || !newFirstElement->renderStyle()->firstChildState()))
            newFirstElement->setNeedsStyleRecalc();
    }

    // :last-child.  In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    if (parent->childrenAffectedByLastChildRules() && elementBeforeChange) {
        // Find our new last child.
        Element* newLastElement = ElementTraversal::lastChild(parent);

        if (newLastElement != elementBeforeChange && elementBeforeChange->renderStyle() && elementBeforeChange->renderStyle()->lastChildState())
            elementBeforeChange->setNeedsStyleRecalc();
            
        // We also have to handle node removal.  The parser callback case is similar to node removal as well in that we need to change the last child
        // to match now.
        if ((checkType == SiblingElementRemoved || checkType == FinishedParsingChildren) && newLastElement == elementBeforeChange && newLastElement
            && (!newLastElement->renderStyle() || !newLastElement->renderStyle()->lastChildState()))
            newLastElement->setNeedsStyleRecalc();
    }

    // The + selector.  We need to invalidate the first element following the insertion point.  It is the only possible element
    // that could be affected by this DOM change.
    if (parent->childrenAffectedByDirectAdjacentRules() && elementAfterChange) {
        elementAfterChange->setNeedsStyleRecalc();
    }

    // Forward positional selectors include the ~ selector, nth-child, nth-of-type, first-of-type and only-of-type.
    // Backward positional selectors include nth-last-child, nth-last-of-type, last-of-type and only-of-type.
    // We have to invalidate everything following the insertion point in the forward case, and everything before the insertion point in the
    // backward case.
    // |afterChange| is 0 in the parser callback case, so we won't do any work for the forward case if we don't have to.
    // For performance reasons we just mark the parent node as changed, since we don't want to make childrenChanged O(n^2) by crawling all our kids
    // here.  recalcStyle will then force a walk of the children when it sees that this has happened.
    if (parent->childrenAffectedByForwardPositionalRules() && elementAfterChange)
        parent->setNeedsStyleRecalc();
    if (parent->childrenAffectedByBackwardPositionalRules() && elementBeforeChange)
        parent->setNeedsStyleRecalc();
}

void Element::childrenChanged(const ChildChange& change)
{
    ContainerNode::childrenChanged(change);
    if (change.source == ChildChangeSourceParser)
        checkForEmptyStyleChange(this, renderStyle());
    else {
        SiblingCheckType checkType = change.type == ElementRemoved ? SiblingElementRemoved : Other;
        checkForSiblingStyleChanges(this, checkType, change.previousSiblingElement, change.nextSiblingElement);
    }

    if (ShadowRoot* shadowRoot = this->shadowRoot())
        shadowRoot->invalidateDistribution();
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
    if (auto styleResolver = document().styleResolverIfExists())
        styleResolver->pushParentElement(this);
}

void Element::finishParsingChildren()
{
    ContainerNode::finishParsingChildren();
    setIsParsingChildrenFinished();
    checkForSiblingStyleChanges(this, FinishedParsingChildren, ElementTraversal::lastChild(this), nullptr);
    if (auto styleResolver = document().styleResolverIfExists())
        styleResolver->popParentElement(this);
}

#ifndef NDEBUG
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
    return *attrNodeListForElement(this);
}

PassRefPtr<Attr> Element::setAttributeNode(Attr* attrNode, ExceptionCode& ec)
{
    if (!attrNode) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }

    RefPtr<Attr> oldAttrNode = attrIfExists(attrNode->qualifiedName());
    if (oldAttrNode.get() == attrNode)
        return attrNode; // This Attr is already attached to the element.

    // INUSE_ATTRIBUTE_ERR: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attrNode->ownerElement()) {
        ec = INUSE_ATTRIBUTE_ERR;
        return 0;
    }

    synchronizeAllAttributes();
    UniqueElementData& elementData = ensureUniqueElementData();

    unsigned index = elementData.findAttributeIndexByNameForAttributeNode(attrNode, shouldIgnoreAttributeCase(*this));
    if (index != ElementData::attributeNotFound) {
        if (oldAttrNode)
            detachAttrNodeFromElementWithValue(oldAttrNode.get(), elementData.attributeAt(index).value());
        else
            oldAttrNode = Attr::create(document(), attrNode->qualifiedName(), elementData.attributeAt(index).value());
    }

    setAttributeInternal(index, attrNode->qualifiedName(), attrNode->value(), NotInSynchronizationOfLazyAttribute);

    attrNode->attachToElement(this);
    treeScope().adoptIfNeeded(attrNode);
    ensureAttrNodeListForElement(this).append(attrNode);

    return oldAttrNode.release();
}

PassRefPtr<Attr> Element::setAttributeNodeNS(Attr* attr, ExceptionCode& ec)
{
    return setAttributeNode(attr, ec);
}

PassRefPtr<Attr> Element::removeAttributeNode(Attr* attr, ExceptionCode& ec)
{
    if (!attr) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }
    if (attr->ownerElement() != this) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    ASSERT(&document() == &attr->document());

    synchronizeAttribute(attr->qualifiedName());

    unsigned index = elementData()->findAttributeIndexByNameForAttributeNode(attr);
    if (index == ElementData::attributeNotFound) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    RefPtr<Attr> attrNode = attr;
    detachAttrNodeFromElementWithValue(attr, elementData()->attributeAt(index).value());
    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    return attrNode.release();
}

bool Element::parseAttributeName(QualifiedName& out, const AtomicString& namespaceURI, const AtomicString& qualifiedName, ExceptionCode& ec)
{
    String prefix, localName;
    if (!Document::parseQualifiedName(qualifiedName, prefix, localName, ec))
        return false;
    ASSERT(!ec);

    QualifiedName qName(prefix, localName, namespaceURI);

    if (!Document::hasValidNamespaceForAttributes(qName)) {
        ec = NAMESPACE_ERR;
        return false;
    }

    out = qName;
    return true;
}

void Element::setAttributeNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, const AtomicString& value, ExceptionCode& ec)
{
    QualifiedName parsedName = anyName;
    if (!parseAttributeName(parsedName, namespaceURI, qualifiedName, ec))
        return;
    setAttribute(parsedName, value);
}

void Element::removeAttributeInternal(unsigned index, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < attributeCount());

    UniqueElementData& elementData = ensureUniqueElementData();

    QualifiedName name = elementData.attributeAt(index).name();
    AtomicString valueBeingRemoved = elementData.attributeAt(index).value();

    if (!inSynchronizationOfLazyAttribute) {
        if (!valueBeingRemoved.isNull())
            willModifyAttribute(name, valueBeingRemoved, nullAtom);
    }

    if (RefPtr<Attr> attrNode = attrIfExists(name))
        detachAttrNodeFromElementWithValue(attrNode.get(), elementData.attributeAt(index).value());

    elementData.removeAttribute(index);

    if (!inSynchronizationOfLazyAttribute)
        didRemoveAttribute(name);
}

void Element::addAttributeInternal(const QualifiedName& name, const AtomicString& value, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    if (!inSynchronizationOfLazyAttribute)
        willModifyAttribute(name, nullAtom, value);
    ensureUniqueElementData().addAttribute(name, value);
    if (!inSynchronizationOfLazyAttribute)
        didAddAttribute(name, value);
}

bool Element::removeAttribute(const AtomicString& name)
{
    if (!elementData())
        return false;

    AtomicString localName = shouldIgnoreAttributeCase(*this) ? name.lower() : name;
    unsigned index = elementData()->findAttributeIndexByName(localName, false);
    if (index == ElementData::attributeNotFound) {
        if (UNLIKELY(localName == styleAttr) && elementData()->styleAttributeIsDirty() && isStyledElement())
            toStyledElement(this)->removeAllInlineStyleProperties();
        return false;
    }

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    return true;
}

bool Element::removeAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    return removeAttribute(QualifiedName(nullAtom, localName, namespaceURI));
}

PassRefPtr<Attr> Element::getAttributeNode(const AtomicString& localName)
{
    if (!elementData())
        return 0;
    synchronizeAttribute(localName);
    const Attribute* attribute = elementData()->findAttributeByName(localName, shouldIgnoreAttributeCase(*this));
    if (!attribute)
        return 0;
    return ensureAttr(attribute->name());
}

PassRefPtr<Attr> Element::getAttributeNodeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    if (!elementData())
        return 0;
    QualifiedName qName(nullAtom, localName, namespaceURI);
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
    return elementData()->findAttributeByName(shouldIgnoreAttributeCase(*this) ? localName.lower() : localName, false);
}

bool Element::hasAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    if (!elementData())
        return false;
    QualifiedName qName(nullAtom, localName, namespaceURI);
    synchronizeAttribute(qName);
    return elementData()->findAttributeByName(qName);
}

CSSStyleDeclaration *Element::style()
{
    return 0;
}

void Element::focus(bool restorePreviousSelection, FocusDirection direction)
{
    if (!inDocument())
        return;

    if (document().focusedElement() == this)
        return;

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
        if (!page->focusController().setFocusedElement(this, document().frame(), direction))
            return;
    }

    // Setting the focused node above might have invalidated the layout due to scripts.
    document().updateLayoutIgnorePendingStylesheets();

    if (!isFocusable()) {
        ensureElementRareData().setNeedsFocusAppearanceUpdateSoonAfterAttach(true);
        return;
    }
        
    cancelFocusAppearanceUpdate();
#if PLATFORM(IOS)
    // Focusing a form element triggers animation in UIKit to scroll to the right position.
    // Calling updateFocusAppearance() would generate an unnecessary call to ScrollView::setScrollPosition(),
    // which would jump us around during this animation. See <rdar://problem/6699741>.
    FrameView* view = document().view();
    bool isFormControl = view && isFormControlElement();
    if (isFormControl)
        view->setProhibitsScrolling(true);
#endif
    updateFocusAppearance(restorePreviousSelection);
#if PLATFORM(IOS)
    if (isFormControl)
        view->setProhibitsScrolling(false);
#endif
}

void Element::updateFocusAppearanceAfterAttachIfNeeded()
{
    if (!hasRareData())
        return;
    ElementRareData* data = elementRareData();
    if (!data->needsFocusAppearanceUpdateSoonAfterAttach())
        return;
    if (isFocusable() && document().focusedElement() == this)
        document().updateFocusAppearanceSoon(false /* don't restore selection */);
    data->setNeedsFocusAppearanceUpdateSoonAfterAttach(false);
}

void Element::updateFocusAppearance(bool /*restorePreviousSelection*/)
{
    if (isRootEditableElement()) {
        Frame* frame = document().frame();
        if (!frame)
            return;
        
        // When focusing an editable element in an iframe, don't reset the selection if it already contains a selection.
        if (this == frame->selection().selection().rootEditableElement())
            return;

        // FIXME: We should restore the previous selection if there is one.
        VisibleSelection newSelection = VisibleSelection(firstPositionInOrBeforeNode(this), DOWNSTREAM);
        
        if (frame->selection().shouldChangeSelection(newSelection)) {
            frame->selection().setSelection(newSelection);
            frame->selection().revealSelection();
        }
    } else if (renderer() && !renderer()->isWidget())
        renderer()->scrollRectToVisible(boundingBox());
}

void Element::blur()
{
    cancelFocusAppearanceUpdate();
    if (treeScope().focusedElement() == this) {
        if (Frame* frame = document().frame())
            frame->page()->focusController().setFocusedElement(0, frame);
        else
            document().setFocusedElement(0);
    }
}

void Element::dispatchFocusInEvent(const AtomicString& eventType, PassRefPtr<Element> oldFocusedElement)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(eventType == eventNames().focusinEvent || eventType == eventNames().DOMFocusInEvent);
    dispatchScopedEvent(FocusEvent::create(eventType, true, false, document().defaultView(), 0, oldFocusedElement));
}

void Element::dispatchFocusOutEvent(const AtomicString& eventType, PassRefPtr<Element> newFocusedElement)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(eventType == eventNames().focusoutEvent || eventType == eventNames().DOMFocusOutEvent);
    dispatchScopedEvent(FocusEvent::create(eventType, true, false, document().defaultView(), 0, newFocusedElement));
}

void Element::dispatchFocusEvent(PassRefPtr<Element> oldFocusedElement, FocusDirection)
{
    if (document().page())
        document().page()->chrome().client().elementDidFocus(this);

    RefPtr<FocusEvent> event = FocusEvent::create(eventNames().focusEvent, false, false, document().defaultView(), 0, oldFocusedElement);
    EventDispatcher::dispatchEvent(this, event.release());
}

void Element::dispatchBlurEvent(PassRefPtr<Element> newFocusedElement)
{
    if (document().page())
        document().page()->chrome().client().elementDidBlur(this);

    RefPtr<FocusEvent> event = FocusEvent::create(eventNames().blurEvent, false, false, document().defaultView(), 0, newFocusedElement);
    EventDispatcher::dispatchEvent(this, event.release());
}


String Element::innerText()
{
    // We need to update layout, since plainText uses line boxes in the render tree.
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return textContent(true);

    return plainText(rangeOfContents(*this).get());
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
    return getAttribute(pseudoAttr);
}

void Element::setPseudo(const AtomicString& value)
{
    setAttribute(pseudoAttr, value);
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

static PseudoElement* beforeOrAfterPseudoElement(Element* host, PseudoId pseudoElementSpecifier)
{
    switch (pseudoElementSpecifier) {
    case BEFORE:
        return host->beforePseudoElement();
    case AFTER:
        return host->afterPseudoElement();
    default:
        return 0;
    }
}

RenderStyle* Element::computedStyle(PseudoId pseudoElementSpecifier)
{
    if (PseudoElement* pseudoElement = beforeOrAfterPseudoElement(this, pseudoElementSpecifier))
        return pseudoElement->computedStyle();

    // FIXME: Find and use the renderer from the pseudo element instead of the actual element so that the 'length'
    // properties, which are only known by the renderer because it did the layout, will be correct and so that the
    // values returned for the ":selection" pseudo-element will be correct.
    if (RenderStyle* usedStyle = renderStyle()) {
        if (pseudoElementSpecifier) {
            RenderStyle* cachedPseudoStyle = usedStyle->getCachedPseudoStyle(pseudoElementSpecifier);
            return cachedPseudoStyle ? cachedPseudoStyle : usedStyle;
        }
        return usedStyle;
    }

    if (!inDocument()) {
        // FIXME: Try to do better than this. Ensure that styleForElement() works for elements that are not in the
        // document tree and figure out when to destroy the computed style for such elements.
        return nullptr;
    }

    ElementRareData& data = ensureElementRareData();
    if (!data.computedStyle())
        data.setComputedStyle(document().styleForElementIgnoringPendingStylesheets(this));
    return pseudoElementSpecifier ? data.computedStyle()->getCachedPseudoStyle(pseudoElementSpecifier) : data.computedStyle();
}

void Element::setStyleAffectedByEmpty()
{
    ensureElementRareData().setStyleAffectedByEmpty(true);
}

void Element::setChildrenAffectedByActive()
{
    ensureElementRareData().setChildrenAffectedByActive(true);
}

void Element::setChildrenAffectedByDrag()
{
    ensureElementRareData().setChildrenAffectedByDrag(true);
}

void Element::setChildrenAffectedByDirectAdjacentRules(Element* element)
{
    element->setChildrenAffectedByDirectAdjacentRules();
}

void Element::setChildrenAffectedByForwardPositionalRules(Element* element)
{
    element->ensureElementRareData().setChildrenAffectedByForwardPositionalRules(true);
}

void Element::setChildrenAffectedByBackwardPositionalRules()
{
    ensureElementRareData().setChildrenAffectedByBackwardPositionalRules(true);
}

void Element::setChildIndex(unsigned index)
{
    ElementRareData& rareData = ensureElementRareData();
    if (RenderStyle* style = renderStyle())
        style->setUnique();
    rareData.setChildIndex(index);
}

bool Element::hasFlagsSetDuringStylingOfChildren() const
{
    if (childrenAffectedByHover() || childrenAffectedByFirstChildRules() || childrenAffectedByLastChildRules() || childrenAffectedByDirectAdjacentRules())
        return true;

    if (!hasRareData())
        return false;
    return rareDataChildrenAffectedByActive()
        || rareDataChildrenAffectedByDrag()
        || rareDataChildrenAffectedByForwardPositionalRules()
        || rareDataChildrenAffectedByBackwardPositionalRules();
}

bool Element::rareDataStyleAffectedByEmpty() const
{
    ASSERT(hasRareData());
    return elementRareData()->styleAffectedByEmpty();
}

bool Element::rareDataChildrenAffectedByActive() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByActive();
}

bool Element::rareDataChildrenAffectedByDrag() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByDrag();
}

bool Element::rareDataChildrenAffectedByForwardPositionalRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByForwardPositionalRules();
}

bool Element::rareDataChildrenAffectedByBackwardPositionalRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByBackwardPositionalRules();
}

unsigned Element::rareDataChildIndex() const
{
    ASSERT(hasRareData());
    return elementRareData()->childIndex();
}

void Element::setIsInCanvasSubtree(bool isInCanvasSubtree)
{
    ensureElementRareData().setIsInCanvasSubtree(isInCanvasSubtree);
}

bool Element::isInCanvasSubtree() const
{
    return hasRareData() && elementRareData()->isInCanvasSubtree();
}

void Element::setIsInsideRegion(bool value)
{
    if (value == isInsideRegion())
        return;

    ensureElementRareData().setIsInsideRegion(value);
}

bool Element::isInsideRegion() const
{
    return hasRareData() ? elementRareData()->isInsideRegion() : false;
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
    const Node* n = this;
    AtomicString value;
    // The language property is inherited, so we iterate over the parents to find the first language.
    do {
        if (n->isElementNode()) {
            if (const ElementData* elementData = toElement(n)->elementData()) {
                // Spec: xml:lang takes precedence -- http://www.w3.org/TR/xhtml1/#C_7
                if (const Attribute* attribute = elementData->findAttributeByName(XMLNames::langAttr))
                    value = attribute->value();
                else if (const Attribute* attribute = elementData->findAttributeByName(HTMLNames::langAttr))
                    value = attribute->value();
            }
        } else if (n->isDocumentNode()) {
            // checking the MIME content-language
            value = toDocument(n)->contentLanguage();
        }

        n = n->parentNode();
    } while (n && value.isNull());

    return value;
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
    for (const Attribute& attribute : attributesIterator()) {
        if (RefPtr<Attr> attr = attrIfExists(attribute.name()))
            attr->normalize();
    }
}

PseudoElement* Element::beforePseudoElement() const
{
    return hasRareData() ? elementRareData()->beforePseudoElement() : 0;
}

PseudoElement* Element::afterPseudoElement() const
{
    return hasRareData() ? elementRareData()->afterPseudoElement() : 0;
}

void Element::setBeforePseudoElement(PassRefPtr<PseudoElement> element)
{
    ensureElementRareData().setBeforePseudoElement(element);
    resetNeedsNodeRenderingTraversalSlowPath();
}

void Element::setAfterPseudoElement(PassRefPtr<PseudoElement> element)
{
    ensureElementRareData().setAfterPseudoElement(element);
    resetNeedsNodeRenderingTraversalSlowPath();
}

static void disconnectPseudoElement(PseudoElement* pseudoElement)
{
    if (!pseudoElement)
        return;
    if (pseudoElement->renderer())
        Style::detachRenderTree(*pseudoElement);
    ASSERT(pseudoElement->hostElement());
    pseudoElement->clearHostElement();
}

void Element::clearBeforePseudoElement()
{
    if (!hasRareData())
        return;
    disconnectPseudoElement(elementRareData()->beforePseudoElement());
    elementRareData()->setBeforePseudoElement(nullptr);
    resetNeedsNodeRenderingTraversalSlowPath();
}

void Element::clearAfterPseudoElement()
{
    if (!hasRareData())
        return;
    disconnectPseudoElement(elementRareData()->afterPseudoElement());
    elementRareData()->setAfterPseudoElement(nullptr);
    resetNeedsNodeRenderingTraversalSlowPath();
}

// ElementTraversal API
Element* Element::firstElementChild() const
{
    return ElementTraversal::firstChild(this);
}

Element* Element::lastElementChild() const
{
    return ElementTraversal::lastChild(this);
}

Element* Element::previousElementSibling() const
{
    return ElementTraversal::previousSibling(this);
}

Element* Element::nextElementSibling() const
{
    return ElementTraversal::nextSibling(this);
}

unsigned Element::childElementCount() const
{
    unsigned count = 0;
    Node* n = firstChild();
    while (n) {
        count += n->isElementNode();
        n = n->nextSibling();
    }
    return count;
}

bool Element::matchesReadOnlyPseudoClass() const
{
    return false;
}

bool Element::matchesReadWritePseudoClass() const
{
    return false;
}

bool Element::webkitMatchesSelector(const String& selector, ExceptionCode& ec)
{
    if (selector.isEmpty()) {
        ec = SYNTAX_ERR;
        return false;
    }

    SelectorQuery* selectorQuery = document().selectorQueryCache().add(selector, document(), ec);
    return selectorQuery && selectorQuery->matches(*this);
}

bool Element::shouldAppearIndeterminate() const
{
    return false;
}

DOMTokenList* Element::classList()
{
    ElementRareData& data = ensureElementRareData();
    if (!data.classList())
        data.setClassList(std::make_unique<ClassList>(*this));
    return data.classList();
}

DatasetDOMStringMap* Element::dataset()
{
    ElementRareData& data = ensureElementRareData();
    if (!data.dataset())
        data.setDataset(std::make_unique<DatasetDOMStringMap>(*this));
    return data.dataset();
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
    return getAttribute(attributeName).string().toInt();
}

void Element::setIntegralAttribute(const QualifiedName& attributeName, int value)
{
    setAttribute(attributeName, AtomicString::number(value));
}

unsigned Element::getUnsignedIntegralAttribute(const QualifiedName& attributeName) const
{
    return getAttribute(attributeName).string().toUInt();
}

void Element::setUnsignedIntegralAttribute(const QualifiedName& attributeName, unsigned value)
{
    setAttribute(attributeName, AtomicString::number(value));
}

#if ENABLE(INDIE_UI)
void Element::setUIActions(const AtomicString& actions)
{
    setAttribute(uiactionsAttr, actions);
}

const AtomicString& Element::UIActions() const
{
    return getAttribute(uiactionsAttr);
}
#endif

bool Element::childShouldCreateRenderer(const Node& child) const
{
    // Only create renderers for SVG elements whose parents are SVG elements, or for proper <svg xmlns="svgNS"> subdocuments.
    if (child.isSVGElement()) {
        ASSERT(!isSVGElement());
        return child.hasTagName(SVGNames::svgTag) && toSVGElement(child).isValid();
    }
    return ContainerNode::childShouldCreateRenderer(child);
}

#if ENABLE(FULLSCREEN_API)
void Element::webkitRequestFullscreen()
{
    document().requestFullScreenForElement(this, ALLOW_KEYBOARD_INPUT, Document::EnforceIFrameAllowFullScreenRequirement);
}

void Element::webkitRequestFullScreen(unsigned short flags)
{
    document().requestFullScreenForElement(this, (flags | LEGACY_MOZILLA_REQUEST), Document::EnforceIFrameAllowFullScreenRequirement);
}

bool Element::containsFullScreenElement() const
{
    return hasRareData() && elementRareData()->containsFullScreenElement();
}

void Element::setContainsFullScreenElement(bool flag)
{
    ensureElementRareData().setContainsFullScreenElement(flag);
    setNeedsStyleRecalc(SyntheticStyleChange);
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
void Element::webkitRequestPointerLock()
{
    if (document().page())
        document().page()->pointerLockController()->requestPointerLock(this);
}
#endif

SpellcheckAttributeState Element::spellcheckAttributeState() const
{
    const AtomicString& value = getAttribute(HTMLNames::spellcheckAttr);
    if (value == nullAtom)
        return SpellcheckAttributeDefault;
    if (equalIgnoringCase(value, "true") || equalIgnoringCase(value, ""))
        return SpellcheckAttributeTrue;
    if (equalIgnoringCase(value, "false"))
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
        return toRenderBlockFlow(renderer())->renderNamedFlowFragment();

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

    if (styleToUse.flowThread().isEmpty())
        return false;

    return !document().renderView()->flowThreadController().isContentElementRegisteredWithAnyNamedFlow(*this);
}

const AtomicString& Element::webkitRegionOverset() const
{
    document().updateLayoutIgnorePendingStylesheets();

    DEFINE_STATIC_LOCAL(AtomicString, undefinedState, ("undefined", AtomicString::ConstructFromLiteral));
    if (!document().cssRegionsEnabled() || !renderNamedFlowFragment())
        return undefinedState;

    switch (renderNamedFlowFragment()->regionOversetState()) {
    case RegionFit: {
        DEFINE_STATIC_LOCAL(AtomicString, fitState, ("fit", AtomicString::ConstructFromLiteral));
        return fitState;
    }
    case RegionEmpty: {
        DEFINE_STATIC_LOCAL(AtomicString, emptyState, ("empty", AtomicString::ConstructFromLiteral));
        return emptyState;
    }
    case RegionOverset: {
        DEFINE_STATIC_LOCAL(AtomicString, overflowState, ("overset", AtomicString::ConstructFromLiteral));
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
    if (!document().cssRegionsEnabled())
        return rangeObjects;

    document().updateLayoutIgnorePendingStylesheets();
    if (renderer() && renderer()->isRenderNamedFlowFragmentContainer()) {
        RenderNamedFlowFragment* namedFlowFragment = toRenderBlockFlow(renderer())->renderNamedFlowFragment();
        if (namedFlowFragment->isValid())
            namedFlowFragment->getRanges(rangeObjects);
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
        return !toSVGElement(this)->isAnimatableAttribute(name);

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

    if (!inDocument())
        return;
    if (!document().isHTMLDocument())
        return;
    updateNameForDocument(toHTMLDocument(document()), oldName, newName);
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

    if (WindowNameCollection::nodeMatchesIfNameAttributeMatch(this)) {
        const AtomicString& id = WindowNameCollection::nodeMatchesIfIdAttributeMatch(this) ? getIdAttribute() : nullAtom;
        if (!oldName.isEmpty() && oldName != id)
            document.removeWindowNamedItem(*oldName.impl(), *this);
        if (!newName.isEmpty() && newName != id)
            document.addWindowNamedItem(*newName.impl(), *this);
    }

    if (DocumentNameCollection::nodeMatchesIfNameAttributeMatch(this)) {
        const AtomicString& id = DocumentNameCollection::nodeMatchesIfIdAttributeMatch(this) ? getIdAttribute() : nullAtom;
        if (!oldName.isEmpty() && oldName != id)
            document.removeDocumentNamedItem(*oldName.impl(), *this);
        if (!newName.isEmpty() && newName != id)
            document.addDocumentNamedItem(*newName.impl(), *this);
    }
}

inline void Element::updateId(const AtomicString& oldId, const AtomicString& newId)
{
    if (!isInTreeScope())
        return;

    if (oldId == newId)
        return;

    updateIdForTreeScope(treeScope(), oldId, newId);

    if (!inDocument())
        return;
    if (!document().isHTMLDocument())
        return;
    updateIdForDocument(toHTMLDocument(document()), oldId, newId, UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute);
}

void Element::updateIdForTreeScope(TreeScope& scope, const AtomicString& oldId, const AtomicString& newId)
{
    ASSERT(isInTreeScope());
    ASSERT(oldId != newId);

    if (!oldId.isEmpty())
        scope.removeElementById(*oldId.impl(), *this);
    if (!newId.isEmpty())
        scope.addElementById(*newId.impl(), *this);
}

void Element::updateIdForDocument(HTMLDocument& document, const AtomicString& oldId, const AtomicString& newId, HTMLDocumentNamedItemMapsUpdatingCondition condition)
{
    ASSERT(inDocument());
    ASSERT(oldId != newId);

    if (WindowNameCollection::nodeMatchesIfIdAttributeMatch(this)) {
        const AtomicString& name = condition == UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute && WindowNameCollection::nodeMatchesIfNameAttributeMatch(this) ? getNameAttribute() : nullAtom;
        if (!oldId.isEmpty() && oldId != name)
            document.removeWindowNamedItem(*oldId.impl(), *this);
        if (!newId.isEmpty() && newId != name)
            document.addWindowNamedItem(*newId.impl(), *this);
    }

    if (DocumentNameCollection::nodeMatchesIfIdAttributeMatch(this)) {
        const AtomicString& name = condition == UpdateHTMLDocumentNamedItemMapsOnlyIfDiffersFromNameAttribute && DocumentNameCollection::nodeMatchesIfNameAttributeMatch(this) ? getNameAttribute() : nullAtom;
        if (!oldId.isEmpty() && oldId != name)
            document.removeDocumentNamedItem(*oldId.impl(), *this);
        if (!newId.isEmpty() && newId != name)
            document.addDocumentNamedItem(*newId.impl(), *this);
    }
}

void Element::updateLabel(TreeScope& scope, const AtomicString& oldForAttributeValue, const AtomicString& newForAttributeValue)
{
    ASSERT(hasTagName(labelTag));

    if (!inDocument())
        return;

    if (oldForAttributeValue == newForAttributeValue)
        return;

    if (!oldForAttributeValue.isEmpty())
        scope.removeLabel(*oldForAttributeValue.impl(), *toHTMLLabelElement(this));
    if (!newForAttributeValue.isEmpty())
        scope.addLabel(*newForAttributeValue.impl(), *toHTMLLabelElement(this));
}

void Element::willModifyAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (isIdAttributeName(name))
        updateId(oldValue, newValue);
    else if (name == HTMLNames::nameAttr)
        updateName(oldValue, newValue);
    else if (name == HTMLNames::forAttr && hasTagName(labelTag)) {
        if (treeScope().shouldCacheLabelsByForAttribute())
            updateLabel(treeScope(), oldValue, newValue);
    }

    if (oldValue != newValue) {
        auto styleResolver = document().styleResolverIfExists();
        if (styleResolver && styleResolver->hasSelectorForAttribute(name.localName()))
            setNeedsStyleRecalc();
    }

    if (std::unique_ptr<MutationObserverInterestGroup> recipients = MutationObserverInterestGroup::createForAttributesMutation(*this, name))
        recipients->enqueueMutationRecord(MutationRecord::createAttributes(*this, name, oldValue));

#if ENABLE(INSPECTOR)
    InspectorInstrumentation::willModifyDOMAttr(&document(), this, oldValue, newValue);
#endif
}

void Element::didAddAttribute(const QualifiedName& name, const AtomicString& value)
{
    attributeChanged(name, value);
    InspectorInstrumentation::didModifyDOMAttr(&document(), this, name.localName(), value);
    dispatchSubtreeModifiedEvent();
}

void Element::didModifyAttribute(const QualifiedName& name, const AtomicString& value)
{
    attributeChanged(name, value);
    InspectorInstrumentation::didModifyDOMAttr(&document(), this, name.localName(), value);
    // Do not dispatch a DOMSubtreeModified event here; see bug 81141.
}

void Element::didRemoveAttribute(const QualifiedName& name)
{
    attributeChanged(name, nullAtom);
    InspectorInstrumentation::didRemoveDOMAttr(&document(), this, name.localName());
    dispatchSubtreeModifiedEvent();
}

PassRefPtr<HTMLCollection> Element::ensureCachedHTMLCollection(CollectionType type)
{
    if (HTMLCollection* collection = cachedHTMLCollection(type))
        return collection;

    RefPtr<HTMLCollection> collection;
    if (type == TableRows) {
        return ensureRareData().ensureNodeLists().addCachedCollection<HTMLTableRowsCollection>(toHTMLTableElement(*this), type);
    } else if (type == SelectOptions) {
        return ensureRareData().ensureNodeLists().addCachedCollection<HTMLOptionsCollection>(toHTMLSelectElement(*this), type);
    } else if (type == FormControls) {
        ASSERT(hasTagName(formTag) || hasTagName(fieldsetTag));
        return ensureRareData().ensureNodeLists().addCachedCollection<HTMLFormControlsCollection>(*this, type);
    }
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLCollection>(*this, type);
}

HTMLCollection* Element::cachedHTMLCollection(CollectionType type)
{
    return hasRareData() && rareData()->nodeLists() ? rareData()->nodeLists()->cachedCollection<HTMLCollection>(type) : 0;
}

IntSize Element::savedLayerScrollOffset() const
{
    return hasRareData() ? elementRareData()->savedLayerScrollOffset() : IntSize();
}

void Element::setSavedLayerScrollOffset(const IntSize& size)
{
    if (size.isZero() && !hasRareData())
        return;
    ensureElementRareData().setSavedLayerScrollOffset(size);
}

PassRefPtr<Attr> Element::attrIfExists(const QualifiedName& name)
{
    if (AttrNodeList* attrNodeList = attrNodeListForElement(this))
        return findAttrNodeInList(*attrNodeList, name);
    return 0;
}

PassRefPtr<Attr> Element::ensureAttr(const QualifiedName& name)
{
    AttrNodeList& attrNodeList = ensureAttrNodeListForElement(this);
    RefPtr<Attr> attrNode = findAttrNodeInList(attrNodeList, name);
    if (!attrNode) {
        attrNode = Attr::create(this, name);
        treeScope().adoptIfNeeded(attrNode.get());
        attrNodeList.append(attrNode);
    }
    return attrNode.release();
}

void Element::detachAttrNodeFromElementWithValue(Attr* attrNode, const AtomicString& value)
{
    ASSERT(hasSyntheticAttrChildNodes());
    attrNode->detachFromElementWithValue(value);

    AttrNodeList* attrNodeList = attrNodeListForElement(this);
    for (unsigned i = 0; i < attrNodeList->size(); ++i) {
        if (attrNodeList->at(i)->qualifiedName() == attrNode->qualifiedName()) {
            attrNodeList->remove(i);
            if (attrNodeList->isEmpty())
                removeAttrNodeListForElement(this);
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void Element::detachAllAttrNodesFromElement()
{
    AttrNodeList* attrNodeList = attrNodeListForElement(this);
    ASSERT(attrNodeList);

    for (const Attribute& attribute : attributesIterator()) {
        if (RefPtr<Attr> attrNode = findAttrNodeInList(*attrNodeList, attribute.name()))
            attrNode->detachFromElementWithValue(attribute.value());
    }

    removeAttrNodeListForElement(this);
}

void Element::resetComputedStyle()
{
    if (!hasRareData() || !elementRareData()->computedStyle())
        return;
    elementRareData()->resetComputedStyle();
    for (auto& child : descendantsOfType<Element>(*this)) {
        if (child.hasRareData())
            child.elementRareData()->resetComputedStyle();
    }
}

void Element::clearStyleDerivedDataBeforeDetachingRenderer()
{
    unregisterNamedFlowContentElement();
    cancelFocusAppearanceUpdate();
    clearBeforePseudoElement();
    clearAfterPseudoElement();
    if (!hasRareData())
        return;
    ElementRareData* data = elementRareData();
    data->setIsInCanvasSubtree(false);
    data->resetComputedStyle();
    data->resetDynamicRestyleObservations();
    data->setIsInsideRegion(false);
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

bool Element::willRecalcStyle(Style::Change)
{
    ASSERT(hasCustomStyleResolveCallbacks());
    return true;
}

void Element::didRecalcStyle(Style::Change)
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

PassRefPtr<RenderStyle> Element::customStyleForRenderer()
{
    ASSERT(hasCustomStyleResolveCallbacks());
    return 0;
}

void Element::cloneAttributesFromElement(const Element& other)
{
    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

    other.synchronizeAllAttributes();
    if (!other.m_elementData) {
        m_elementData.clear();
        return;
    }

    // We can't update window and document's named item maps since the presence of image and object elements depend on other attributes and children.
    // Fortunately, those named item maps are only updated when this element is in the document, which should never be the case.
    ASSERT(!inDocument());

    const AtomicString& oldID = getIdAttribute();
    const AtomicString& newID = other.getIdAttribute();

    if (!oldID.isNull() || !newID.isNull())
        updateId(oldID, newID);

    const AtomicString& oldName = getNameAttribute();
    const AtomicString& newName = other.getNameAttribute();

    if (!oldName.isNull() || !newName.isNull())
        updateName(oldName, newName);

    // If 'other' has a mutable ElementData, convert it to an immutable one so we can share it between both elements.
    // We can only do this if there is no CSSOM wrapper for other's inline style, and there are no presentation attributes.
    if (other.m_elementData->isUnique()
        && !other.m_elementData->presentationAttributeStyle()
        && (!other.m_elementData->inlineStyle() || !other.m_elementData->inlineStyle()->hasCSSOMWrapper()))
        const_cast<Element&>(other).m_elementData = static_cast<const UniqueElementData*>(other.m_elementData.get())->makeShareableCopy();

    if (!other.m_elementData->isUnique())
        m_elementData = other.m_elementData;
    else
        m_elementData = other.m_elementData->makeUniqueCopy();

    for (const Attribute& attribute : attributesIterator()) {
        attributeChanged(attribute.name(), attribute.value(), ModifiedByCloning);
    }
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
    else {
        ASSERT(!m_elementData->isUnique());
        m_elementData = static_cast<ShareableElementData*>(m_elementData.get())->makeUniqueCopy();
    }
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

} // namespace WebCore
