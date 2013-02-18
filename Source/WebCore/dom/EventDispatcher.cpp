/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#include "AncestorChainWalker.h"
#include "ContainerNode.h"
#include "ElementShadow.h"
#include "EventContext.h"
#include "EventDispatchMediator.h"
#include "EventRetargeter.h"
#include "FrameView.h"
#include "HTMLMediaElement.h"
#include "InsertionPoint.h"
#include "InspectorInstrumentation.h"
#include "MouseEvent.h"
#include "ScopedEventQueue.h"
#include "ShadowRoot.h"
#include "WindowEventContext.h"
#include <wtf/RefPtr.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

static HashSet<Node*>* gNodesDispatchingSimulatedClicks = 0;

bool EventDispatcher::dispatchEvent(Node* node, PassRefPtr<EventDispatchMediator> mediator)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    EventDispatcher dispatcher(node);
    return mediator->dispatchEvent(&dispatcher);
}

EventDispatcher::EventDispatcher(Node* node)
    : m_node(node)
    , m_eventPathInitialized(false)
#ifndef NDEBUG
    , m_eventDispatched(false)
#endif
{
    ASSERT(node);
    m_view = node->document()->view();
}

EventPath& EventDispatcher::ensureEventPath(Event* event)
{
    if (m_eventPathInitialized)
        return m_eventPath;
    m_eventPathInitialized = true;
    EventRetargeter::calculateEventPath(m_node.get(), event, m_eventPath);
    return m_eventPath;
}

void EventDispatcher::dispatchScopedEvent(Node* node, PassRefPtr<EventDispatchMediator> mediator)
{
    // We need to set the target here because it can go away by the time we actually fire the event.
    mediator->event()->setTarget(EventRetargeter::eventTargetRespectingTargetRules(node));
    ScopedEventQueue::instance()->enqueueEventDispatchMediator(mediator);
}

void EventDispatcher::dispatchSimulatedClick(Node* node, Event* underlyingEvent, SimulatedClickMouseEventOptions mouseEventOptions, SimulatedClickVisualOptions visualOptions)
{
    if (node->disabled())
        return;

    if (!gNodesDispatchingSimulatedClicks)
        gNodesDispatchingSimulatedClicks = new HashSet<Node*>;
    else if (gNodesDispatchingSimulatedClicks->contains(node))
        return;

    gNodesDispatchingSimulatedClicks->add(node);

    if (mouseEventOptions == SendMouseOverUpDownEvents)
        EventDispatcher(node).dispatchEvent(SimulatedMouseEvent::create(eventNames().mouseoverEvent, node->document()->defaultView(), underlyingEvent));

    if (mouseEventOptions != SendNoEvents)
        EventDispatcher(node).dispatchEvent(SimulatedMouseEvent::create(eventNames().mousedownEvent, node->document()->defaultView(), underlyingEvent));
    node->setActive(true, visualOptions == ShowPressedLook);
    if (mouseEventOptions != SendNoEvents)
        EventDispatcher(node).dispatchEvent(SimulatedMouseEvent::create(eventNames().mouseupEvent, node->document()->defaultView(), underlyingEvent));
    node->setActive(false);

    // always send click
    EventDispatcher(node).dispatchEvent(SimulatedMouseEvent::create(eventNames().clickEvent, node->document()->defaultView(), underlyingEvent));

    gNodesDispatchingSimulatedClicks->remove(node);
}

bool EventDispatcher::dispatchEvent(PassRefPtr<Event> prpEvent)
{
#ifndef NDEBUG
    ASSERT(!m_eventDispatched);
    m_eventDispatched = true;
#endif
    RefPtr<Event> event = prpEvent;
    ChildNodesLazySnapshot::takeChildNodesLazySnapshot();

    event->setTarget(EventRetargeter::eventTargetRespectingTargetRules(m_node.get()));
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(event->target());
    ASSERT(!event->type().isNull()); // JavaScript code can create an event with an empty name, but not null.
    ensureEventPath(event.get());
    WindowEventContext windowEventContext(event.get(), m_node.get(), topEventContext());
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchEvent(m_node->document(), *event, windowEventContext.window(), m_node.get(), m_eventPath);

    void* preDispatchEventHandlerResult;
    if (dispatchEventPreProcess(event, preDispatchEventHandlerResult) == ContinueDispatching)
        if (dispatchEventAtCapturing(event, windowEventContext) == ContinueDispatching)
            if (dispatchEventAtTarget(event) == ContinueDispatching)
                dispatchEventAtBubbling(event, windowEventContext);
    dispatchEventPostProcess(event, preDispatchEventHandlerResult);

    // Ensure that after event dispatch, the event's target object is the
    // outermost shadow DOM boundary.
    event->setTarget(windowEventContext.target());
    event->setCurrentTarget(0);
    InspectorInstrumentation::didDispatchEvent(cookie);

    return !event->defaultPrevented();
}

inline EventDispatchContinuation EventDispatcher::dispatchEventPreProcess(PassRefPtr<Event> event, void*& preDispatchEventHandlerResult)
{
    // Give the target node a chance to do some work before DOM event handlers get a crack.
    preDispatchEventHandlerResult = m_node->preDispatchEventHandler(event.get());
    return (m_eventPath.isEmpty() || event->propagationStopped()) ? DoneDispatching : ContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtCapturing(PassRefPtr<Event> event, WindowEventContext& windowEventContext)
{
    // Trigger capturing event handlers, starting at the top and working our way down.
    event->setEventPhase(Event::CAPTURING_PHASE);

    if (windowEventContext.handleLocalEvents(event.get()) && event->propagationStopped())
        return DoneDispatching;

    for (size_t i = m_eventPath.size() - 1; i > 0; --i) {
        const EventContext& eventContext = *m_eventPath[i];
        if (eventContext.currentTargetSameAsTarget()) {
            if (event->bubbles())
                continue;
            event->setEventPhase(Event::AT_TARGET);
        } else
            event->setEventPhase(Event::CAPTURING_PHASE);
        eventContext.handleLocalEvents(event.get());
        if (event->propagationStopped())
            return DoneDispatching;
    }

    return ContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtTarget(PassRefPtr<Event> event)
{
    event->setEventPhase(Event::AT_TARGET);
    m_eventPath[0]->handleLocalEvents(event.get());
    return event->propagationStopped() ? DoneDispatching : ContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtBubbling(PassRefPtr<Event> event, WindowEventContext& windowContext)
{
    if (event->bubbles() && !event->cancelBubble()) {
        // Trigger bubbling event handlers, starting at the bottom and working our way up.
        event->setEventPhase(Event::BUBBLING_PHASE);

        size_t size = m_eventPath.size();
        for (size_t i = 1; i < size; ++i) {
            const EventContext& eventContext = *m_eventPath[i];
            if (eventContext.currentTargetSameAsTarget())
                event->setEventPhase(Event::AT_TARGET);
            else
                event->setEventPhase(Event::BUBBLING_PHASE);
            eventContext.handleLocalEvents(event.get());
            if (event->propagationStopped() || event->cancelBubble())
                return DoneDispatching;
        }
        windowContext.handleLocalEvents(event.get());
    }
    return ContinueDispatching;
}

inline void EventDispatcher::dispatchEventPostProcess(PassRefPtr<Event> event, void* preDispatchEventHandlerResult)
{
    event->setTarget(EventRetargeter::eventTargetRespectingTargetRules(m_node.get()));
    event->setCurrentTarget(0);
    event->setEventPhase(0);

    // Pass the data from the preDispatchEventHandler to the postDispatchEventHandler.
    m_node->postDispatchEventHandler(event.get(), preDispatchEventHandlerResult);

    // Call default event handlers. While the DOM does have a concept of preventing
    // default handling, the detail of which handlers are called is an internal
    // implementation detail and not part of the DOM.
    if (!event->defaultPrevented() && !event->defaultHandled()) {
        // Non-bubbling events call only one default event handler, the one for the target.
        m_node->defaultEventHandler(event.get());
        ASSERT(!event->defaultPrevented());
        if (event->defaultHandled())
            return;
        // For bubbling events, call default event handlers on the same targets in the
        // same order as the bubbling phase.
        if (event->bubbles()) {
            size_t size = m_eventPath.size();
            for (size_t i = 1; i < size; ++i) {
                m_eventPath[i]->node()->defaultEventHandler(event.get());
                ASSERT(!event->defaultPrevented());
                if (event->defaultHandled())
                    return;
            }
        }
    }
}

const EventContext* EventDispatcher::topEventContext()
{
    return m_eventPath.isEmpty() ? 0 : m_eventPath.last().get();
}

}
