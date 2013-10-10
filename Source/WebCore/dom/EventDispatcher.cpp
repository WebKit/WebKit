/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2013 Apple Inc. All rights reserved.
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

#include "ContainerNode.h"
#include "EventContext.h"
#include "EventRetargeter.h"
#include "FocusEvent.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "InsertionPoint.h"
#include "InspectorInstrumentation.h"
#include "MouseEvent.h"
#include "ScopedEventQueue.h"
#include "ShadowRoot.h"
#include "TouchEvent.h"
#include "WindowEventContext.h"
#include <wtf/RefPtr.h>

namespace WebCore {

void EventDispatcher::dispatchScopedEvent(Node& node, PassRefPtr<Event> event)
{
    // We need to set the target here because it can go away by the time we actually fire the event.
    event->setTarget(&eventTargetRespectingTargetRules(node));
    ScopedEventQueue::instance()->enqueueEvent(event);
}

void EventDispatcher::dispatchSimulatedClick(Element* element, Event* underlyingEvent, SimulatedClickMouseEventOptions mouseEventOptions, SimulatedClickVisualOptions visualOptions)
{
    if (element->isDisabledFormControl())
        return;

    DEFINE_STATIC_LOCAL(HashSet<Element*>, elementsDispatchingSimulatedClicks, ());
    if (!elementsDispatchingSimulatedClicks.add(element).isNewEntry)
        return;

    if (mouseEventOptions == SendMouseOverUpDownEvents)
        dispatchEvent(element, SimulatedMouseEvent::create(eventNames().mouseoverEvent, element->document().defaultView(), underlyingEvent));

    if (mouseEventOptions != SendNoEvents)
        dispatchEvent(element, SimulatedMouseEvent::create(eventNames().mousedownEvent, element->document().defaultView(), underlyingEvent));
    element->setActive(true, visualOptions == ShowPressedLook);
    if (mouseEventOptions != SendNoEvents)
        dispatchEvent(element, SimulatedMouseEvent::create(eventNames().mouseupEvent, element->document().defaultView(), underlyingEvent));
    element->setActive(false);

    // always send click
    dispatchEvent(element, SimulatedMouseEvent::create(eventNames().clickEvent, element->document().defaultView(), underlyingEvent));

    elementsDispatchingSimulatedClicks.remove(element);
}

static void callDefaultEventHandlersInTheBubblingOrder(Event& event, const EventPath& path)
{
    // Non-bubbling events call only one default event handler, the one for the target.
    path.contextAt(0).node()->defaultEventHandler(&event);
    ASSERT(!event.defaultPrevented());

    if (event.defaultHandled() || !event.bubbles())
        return;

    size_t size = path.size();
    for (size_t i = 1; i < size; ++i) {
        path.contextAt(i).node()->defaultEventHandler(&event);
        ASSERT(!event.defaultPrevented());
        if (event.defaultHandled())
            return;
    }
}

static void dispatchEventInDOM(Event& event, const EventPath& path, WindowEventContext& windowEventContext)
{
    // Trigger capturing event handlers, starting at the top and working our way down.
    event.setEventPhase(Event::CAPTURING_PHASE);

    if (windowEventContext.handleLocalEvents(event) && event.propagationStopped())
        return;

    for (size_t i = path.size() - 1; i > 0; --i) {
        const EventContext& eventContext = path.contextAt(i);
        if (eventContext.currentTargetSameAsTarget())
            continue;
        eventContext.handleLocalEvents(event);
        if (event.propagationStopped())
            return;
    }

    event.setEventPhase(Event::AT_TARGET);
    path.contextAt(0).handleLocalEvents(event);
    if (event.propagationStopped())
        return;

    // Trigger bubbling event handlers, starting at the bottom and working our way up.
    size_t size = path.size();
    for (size_t i = 1; i < size; ++i) {
        const EventContext& eventContext = path.contextAt(i);
        if (eventContext.currentTargetSameAsTarget())
            event.setEventPhase(Event::AT_TARGET);
        else if (event.bubbles() && !event.cancelBubble())
            event.setEventPhase(Event::BUBBLING_PHASE);
        else
            continue;
        eventContext.handleLocalEvents(event);
        if (event.propagationStopped())
            return;
    }
    if (event.bubbles() && !event.cancelBubble()) {
        event.setEventPhase(Event::BUBBLING_PHASE);
        windowEventContext.handleLocalEvents(event);
    }
}

bool EventDispatcher::dispatchEvent(Node* origin, PassRefPtr<Event> prpEvent)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    if (!prpEvent)
        return true;

    ASSERT(origin);
    RefPtr<Node> node(origin);
    RefPtr<Event> event(prpEvent);
    RefPtr<FrameView> view = node->document().view();
    EventPath eventPath(*node, *event);

    if (EventTarget* relatedTarget = event->relatedTarget())
        eventPath.setRelatedTarget(*relatedTarget);
#if ENABLE(TOUCH_EVENTS)
    if (event->isTouchEvent())
        eventPath.updateTouchLists(*toTouchEvent(m_event.get())); 
#endif

    ChildNodesLazySnapshot::takeChildNodesLazySnapshot();

    event->setTarget(&eventTargetRespectingTargetRules(*node));
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(event->target());
    WindowEventContext windowEventContext(event.get(), node.get(), eventPath.lastContextIfExists());
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchEvent(&node->document(), *event, windowEventContext.window(), node.get(), eventPath);

    InputElementClickState clickHandlingState;
    if (isHTMLInputElement(node.get()))
        toHTMLInputElement(*node).willDispatchEvent(*event, clickHandlingState);

    if (!event->propagationStopped() && !eventPath.isEmpty())
        dispatchEventInDOM(*event, eventPath, windowEventContext);

    event->setTarget(&eventTargetRespectingTargetRules(*node));
    event->setCurrentTarget(0);
    event->setEventPhase(0);

    if (clickHandlingState.stateful)
        toHTMLInputElement(*node).didDispatchClickEvent(*event, clickHandlingState);

    // Call default event handlers. While the DOM does have a concept of preventing
    // default handling, the detail of which handlers are called is an internal
    // implementation detail and not part of the DOM.
    if (!event->defaultPrevented() && !event->defaultHandled())
        callDefaultEventHandlersInTheBubblingOrder(*event, eventPath);

    // Ensure that after event dispatch, the event's target object is the
    // outermost shadow DOM boundary.
    event->setTarget(windowEventContext.target());
    event->setCurrentTarget(0);
    InspectorInstrumentation::didDispatchEvent(cookie);

    return !event->defaultPrevented();
}

bool EventPath::hasEventListeners(const AtomicString& eventType) const
{
    for (size_t i = 0; i < m_path.size(); i++) {
        if (m_path[i]->node()->hasEventListeners(eventType))
            return true;
    }

    return false;
}

}
