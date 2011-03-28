/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "EventDispatcher.h"

#include "Event.h"
#include "EventContext.h"
#include "EventTarget.h"
#include "FrameView.h"
#include "InspectorInstrumentation.h"
#include "MouseEvent.h"
#include "Node.h"

#if ENABLE(SVG)
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGUseElement.h"
#endif

#include "UIEvent.h"
#include "UIEventWithKeyState.h"
#include "WindowEventContext.h"

#include <wtf/RefPtr.h>

namespace WebCore {

static HashSet<Node*>* gNodesDispatchingSimulatedClicks = 0;

bool EventDispatcher::dispatchEvent(Node* node, PassRefPtr<Event> event)
{
    EventDispatcher dispatcher(node);
    return dispatcher.dispatchEvent(event);
}

bool EventDispatcher::dispatchMouseEvent(Node* node, const PlatformMouseEvent& event, const AtomicString& eventType,
    int detail, Node* relatedTarget)
{
    ASSERT(!eventDispatchForbidden());
    EventDispatcher dispatcher(node);

    IntPoint contentsPos;
    if (FrameView* view = node->document()->view())
        contentsPos = view->windowToContents(event.pos());

    short button = event.button();

    ASSERT(event.eventType() == MouseEventMoved || button != NoButton);

    return dispatcher.dispatchMouseEvent(eventType, button, detail,
        contentsPos.x(), contentsPos.y(), event.globalX(), event.globalY(),
        event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(),
        false, relatedTarget, 0);
}

void EventDispatcher::dispatchSimulatedClick(Node* node, PassRefPtr<Event> event, bool sendMouseEvents, bool showPressedLook)
{
    EventDispatcher dispatcher(node);

    if (!gNodesDispatchingSimulatedClicks)
        gNodesDispatchingSimulatedClicks = new HashSet<Node*>;
    else if (gNodesDispatchingSimulatedClicks->contains(node))
        return;

    gNodesDispatchingSimulatedClicks->add(node);

    // send mousedown and mouseup before the click, if requested
    if (sendMouseEvents)
        dispatcher.dispatchSimulatedMouseEvent(eventNames().mousedownEvent, event.get());
    node->setActive(true, showPressedLook);
    if (sendMouseEvents)
        dispatcher.dispatchSimulatedMouseEvent(eventNames().mouseupEvent, event.get());
    node->setActive(false);

    // always send click
    dispatcher.dispatchSimulatedMouseEvent(eventNames().clickEvent, event);

    gNodesDispatchingSimulatedClicks->remove(node);
}

// FIXME: Once https://bugs.webkit.org/show_bug.cgi?id=52963 lands, this should
// be greatly improved. See https://bugs.webkit.org/show_bug.cgi?id=54025.
static Node* pullOutOfShadow(Node* node)
{
    Node* outermostShadowBoundary = node;
    for (Node* n = node; n; n = n->parentOrHostNode()) {
        if (n->isShadowRoot())
            outermostShadowBoundary = n->parentOrHostNode();
    }
    return outermostShadowBoundary;
}

EventTarget* EventDispatcher::eventTargetRespectingSVGTargetRules(Node* referenceNode)
{
    ASSERT(referenceNode);

#if ENABLE(SVG)
    if (!referenceNode->isSVGElement())
        return referenceNode;

    // Spec: The event handling for the non-exposed tree works as if the referenced element had been textually included
    // as a deeply cloned child of the 'use' element, except that events are dispatched to the SVGElementInstance objects
    for (Node* n = referenceNode; n; n = n->parentNode()) {
        if (!n->isShadowRoot() || !n->isSVGElement())
            continue;

        Element* shadowTreeParentElement = n->shadowHost();
        ASSERT(shadowTreeParentElement->hasTagName(SVGNames::useTag));

        if (SVGElementInstance* instance = static_cast<SVGUseElement*>(shadowTreeParentElement)->instanceForShadowTreeElement(referenceNode))
            return instance;
    }
#endif

    return referenceNode;
}

EventDispatcher::EventDispatcher(Node* node)
    : m_node(node)
{
    ASSERT(node);
    m_view = node->document()->view();
}

void EventDispatcher::getEventAncestors(EventTarget* originalTarget, EventDispatchBehavior behavior)
{
    if (!m_node->inDocument())
        return;

    if (ancestorsInitialized())
        return;

    EventTarget* target = originalTarget;
    Node* ancestor = m_node.get();
    bool shouldSkipNextAncestor = false;
    while (true) {
        if (ancestor->isShadowRoot()) {
            if (behavior == StayInsideShadowDOM)
                return;
            ancestor = ancestor->shadowHost();
            if (!shouldSkipNextAncestor)
                target = ancestor;
        } else
            ancestor = ancestor->parentNodeGuaranteedHostFree();

        if (!ancestor)
            return;

#if ENABLE(SVG)
        // Skip SVGShadowTreeRootElement.
        shouldSkipNextAncestor = ancestor->isSVGElement() && ancestor->isShadowRoot();
        if (shouldSkipNextAncestor)
            continue;
#endif
        // FIXME: Unroll the extra loop inside eventTargetRespectingSVGTargetRules into this loop.
        m_ancestors.append(EventContext(ancestor, eventTargetRespectingSVGTargetRules(ancestor), target));
    }
}

bool EventDispatcher::dispatchEvent(PassRefPtr<Event> prpEvent)
{
    RefPtr<Event> event = prpEvent;

    event->setTarget(eventTargetRespectingSVGTargetRules(m_node.get()));

    ASSERT(!eventDispatchForbidden());
    ASSERT(event->target());
    ASSERT(!event->type().isNull()); // JavaScript code can create an event with an empty name, but not null.

    RefPtr<EventTarget> originalTarget = event->target();
    getEventAncestors(originalTarget.get(), determineDispatchBehavior(event.get()));

    WindowEventContext windowContext(event.get(), m_node.get(), topEventContext());

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchEvent(m_node->document(), *event, windowContext.window(), m_node.get(), m_ancestors);

    // Give the target node a chance to do some work before DOM event handlers get a crack.
    void* data = m_node->preDispatchEventHandler(event.get());
    if (event->propagationStopped())
        goto doneDispatching;

    // Trigger capturing event handlers, starting at the top and working our way down.
    event->setEventPhase(Event::CAPTURING_PHASE);

    if (windowContext.handleLocalEvents(event.get()) && event->propagationStopped())
        goto doneDispatching;

    for (size_t i = m_ancestors.size(); i; --i) {
        m_ancestors[i - 1].handleLocalEvents(event.get());
        if (event->propagationStopped())
            goto doneDispatching;
    }

    event->setEventPhase(Event::AT_TARGET);
    event->setTarget(originalTarget.get());
    event->setCurrentTarget(EventDispatcher::eventTargetRespectingSVGTargetRules(m_node.get()));
    m_node->handleLocalEvents(event.get());
    if (event->propagationStopped())
        goto doneDispatching;

    if (event->bubbles() && !event->cancelBubble()) {
        // Trigger bubbling event handlers, starting at the bottom and working our way up.
        event->setEventPhase(Event::BUBBLING_PHASE);

        size_t size = m_ancestors.size();
        for (size_t i = 0; i < size; ++i) {
            m_ancestors[i].handleLocalEvents(event.get());
            if (event->propagationStopped() || event->cancelBubble())
                goto doneDispatching;
        }
        windowContext.handleLocalEvents(event.get());
    }

doneDispatching:
    event->setTarget(originalTarget.get());
    event->setCurrentTarget(0);
    event->setEventPhase(0);

    // Pass the data from the preDispatchEventHandler to the postDispatchEventHandler.
    m_node->postDispatchEventHandler(event.get(), data);

    // Call default event handlers. While the DOM does have a concept of preventing
    // default handling, the detail of which handlers are called is an internal
    // implementation detail and not part of the DOM.
    if (!event->defaultPrevented() && !event->defaultHandled()) {
        // Non-bubbling events call only one default event handler, the one for the target.
        m_node->defaultEventHandler(event.get());
        ASSERT(!event->defaultPrevented());
        if (event->defaultHandled())
            goto doneWithDefault;
        // For bubbling events, call default event handlers on the same targets in the
        // same order as the bubbling phase.
        if (event->bubbles()) {
            size_t size = m_ancestors.size();
            for (size_t i = 0; i < size; ++i) {
                m_ancestors[i].node()->defaultEventHandler(event.get());
                ASSERT(!event->defaultPrevented());
                if (event->defaultHandled())
                    goto doneWithDefault;
            }
        }
    }

doneWithDefault:

    // Ensure that after event dispatch, the event's target object is the
    // outermost shadow DOM boundary.
    event->setTarget(windowContext.target());
    event->setCurrentTarget(0);
    InspectorInstrumentation::didDispatchEvent(cookie);

    return !event->defaultPrevented();
}

bool EventDispatcher::dispatchMouseEvent(const AtomicString& eventType, int button, int detail,
    int pageX, int pageY, int screenX, int screenY,
    bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
    bool isSimulated, Node* relatedTargetArg, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());
    if (m_node->disabled()) // Don't even send DOM events for disabled controls..
        return true;

    if (eventType.isEmpty())
        return false; // Shouldn't happen.

    bool cancelable = eventType != eventNames().mousemoveEvent;

    bool swallowEvent = false;

    // Attempting to dispatch with a non-EventTarget relatedTarget causes the relatedTarget to be silently ignored.
    RefPtr<Node> relatedTarget = pullOutOfShadow(relatedTargetArg);

    int adjustedPageX = pageX;
    int adjustedPageY = pageY;
    if (Frame* frame = m_node->document()->frame()) {
        float pageZoom = frame->pageZoomFactor();
        if (pageZoom != 1.0f) {
            // Adjust our pageX and pageY to account for the page zoom.
            adjustedPageX = lroundf(pageX / pageZoom);
            adjustedPageY = lroundf(pageY / pageZoom);
        }
    }

    RefPtr<MouseEvent> mouseEvent = MouseEvent::create(eventType,
        true, cancelable, m_node->document()->defaultView(),
        detail, screenX, screenY, adjustedPageX, adjustedPageY,
        ctrlKey, altKey, shiftKey, metaKey, button,
        relatedTarget, 0, isSimulated);
    mouseEvent->setUnderlyingEvent(underlyingEvent.get());
    mouseEvent->setAbsoluteLocation(IntPoint(pageX, pageY));

    dispatchEvent(mouseEvent);
    bool defaultHandled = mouseEvent->defaultHandled();
    bool defaultPrevented = mouseEvent->defaultPrevented();
    if (defaultHandled || defaultPrevented)
        swallowEvent = true;

    // Special case: If it's a double click event, we also send the dblclick event. This is not part
    // of the DOM specs, but is used for compatibility with the ondblclick="" attribute. This is treated
    // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
    if (eventType == eventNames().clickEvent && detail == 2) {
        RefPtr<Event> doubleClickEvent = MouseEvent::create(eventNames().dblclickEvent,
            true, cancelable, m_node->document()->defaultView(),
            detail, screenX, screenY, adjustedPageX, adjustedPageY,
            ctrlKey, altKey, shiftKey, metaKey, button,
            relatedTarget, 0, isSimulated);
        doubleClickEvent->setUnderlyingEvent(underlyingEvent.get());
        if (defaultHandled)
            doubleClickEvent->setDefaultHandled();
        dispatchEvent(doubleClickEvent);
        if (doubleClickEvent->defaultHandled() || doubleClickEvent->defaultPrevented())
            swallowEvent = true;
    }

    return swallowEvent;
}

void EventDispatcher::dispatchSimulatedMouseEvent(const AtomicString& eventType, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());

    bool ctrlKey = false;
    bool altKey = false;
    bool shiftKey = false;
    bool metaKey = false;
    if (UIEventWithKeyState* keyStateEvent = findEventWithKeyState(underlyingEvent.get())) {
        ctrlKey = keyStateEvent->ctrlKey();
        altKey = keyStateEvent->altKey();
        shiftKey = keyStateEvent->shiftKey();
        metaKey = keyStateEvent->metaKey();
    }

    // Like Gecko, we just pass 0 for everything when we make a fake mouse event.
    // Internet Explorer instead gives the current mouse position and state.
    dispatchMouseEvent(eventType, 0, 0, 0, 0, 0, 0, ctrlKey, altKey, shiftKey, metaKey, true, 0, underlyingEvent);
}

const EventContext* EventDispatcher::topEventContext()
{
    return m_ancestors.isEmpty() ? 0 : &m_ancestors.last();
}

bool EventDispatcher::ancestorsInitialized() const
{
    return m_ancestors.size();
}

EventDispatchBehavior EventDispatcher::determineDispatchBehavior(Event* event)
{
    // Per XBL 2.0 spec, mutation events should never cross shadow DOM boundary:
    // http://dev.w3.org/2006/xbl2/#event-flow-and-targeting-across-shadow-s
    if (event->isMutationEvent())
        return StayInsideShadowDOM;

    // WebKit never allowed selectstart event to cross the the shadow DOM boundary.
    // Changing this breaks existing sites.
    // See https://bugs.webkit.org/show_bug.cgi?id=52195 for details.
    if (event->type() == eventNames().selectstartEvent)
        return StayInsideShadowDOM;

    return RetargetEvent;
}

}

