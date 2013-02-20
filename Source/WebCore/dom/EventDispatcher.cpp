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

#include "ContainerNode.h"
#include "ElementShadow.h"
#include "EventContext.h"
#include "EventDispatchMediator.h"
#include "EventPathWalker.h"
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
    if (!mediator->event())
        return true;
    EventDispatcher dispatcher(node, mediator->event());
    return mediator->dispatchEvent(&dispatcher);
}

EventDispatcher::EventDispatcher(Node* node, PassRefPtr<Event> event)
    : m_node(node)
    , m_event(event)
    , m_eventPathInitialized(false)
#ifndef NDEBUG
    , m_eventDispatched(false)
#endif
{
    ASSERT(node);
    ASSERT(m_event.get());
    ASSERT(!m_event->type().isNull()); // JavaScript code can create an event with an empty name, but not null.
    m_view = node->document()->view();
}

EventPath& EventDispatcher::ensureEventPath()
{
    if (m_eventPathInitialized)
        return m_eventPath;
    m_eventPathInitialized = true;
    EventRetargeter::calculateEventPath(m_node.get(), m_event.get(), m_eventPath);
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
        EventDispatcher(node, SimulatedMouseEvent::create(eventNames().mouseoverEvent, node->document()->defaultView(), underlyingEvent)).dispatch();

    if (mouseEventOptions != SendNoEvents)
        EventDispatcher(node, SimulatedMouseEvent::create(eventNames().mousedownEvent, node->document()->defaultView(), underlyingEvent)).dispatch();
    node->setActive(true, visualOptions == ShowPressedLook);
    if (mouseEventOptions != SendNoEvents)
        EventDispatcher(node, SimulatedMouseEvent::create(eventNames().mouseupEvent, node->document()->defaultView(), underlyingEvent)).dispatch();
    node->setActive(false);

    // always send click
    EventDispatcher(node, SimulatedMouseEvent::create(eventNames().clickEvent, node->document()->defaultView(), underlyingEvent)).dispatch();

    gNodesDispatchingSimulatedClicks->remove(node);
}

bool EventDispatcher::dispatch()
{
#ifndef NDEBUG
    ASSERT(!m_eventDispatched);
    m_eventDispatched = true;
#endif
    ChildNodesLazySnapshot::takeChildNodesLazySnapshot();

    m_event->setTarget(EventRetargeter::eventTargetRespectingTargetRules(m_node.get()));
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(m_event->target());
    ensureEventPath();
    WindowEventContext windowEventContext(m_event.get(), m_node.get(), topEventContext());
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchEvent(m_node->document(), *m_event, windowEventContext.window(), m_node.get(), m_eventPath);

    void* preDispatchEventHandlerResult;
    if (dispatchEventPreProcess(preDispatchEventHandlerResult) == ContinueDispatching)
        if (dispatchEventAtCapturing(windowEventContext) == ContinueDispatching)
            if (dispatchEventAtTarget() == ContinueDispatching)
                dispatchEventAtBubbling(windowEventContext);
    dispatchEventPostProcess(preDispatchEventHandlerResult);

    // Ensure that after event dispatch, the event's target object is the
    // outermost shadow DOM boundary.
    m_event->setTarget(windowEventContext.target());
    m_event->setCurrentTarget(0);
    InspectorInstrumentation::didDispatchEvent(cookie);

    return !m_event->defaultPrevented();
}

inline EventDispatchContinuation EventDispatcher::dispatchEventPreProcess(void*& preDispatchEventHandlerResult)
{
    // Give the target node a chance to do some work before DOM event handlers get a crack.
    preDispatchEventHandlerResult = m_node->preDispatchEventHandler(m_event.get());
    return (m_eventPath.isEmpty() || m_event->propagationStopped()) ? DoneDispatching : ContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtCapturing(WindowEventContext& windowEventContext)
{
    // Trigger capturing event handlers, starting at the top and working our way down.
    m_event->setEventPhase(Event::CAPTURING_PHASE);

    if (windowEventContext.handleLocalEvents(m_event.get()) && m_event->propagationStopped())
        return DoneDispatching;

    for (size_t i = m_eventPath.size() - 1; i > 0; --i) {
        const EventContext& eventContext = *m_eventPath[i];
        if (eventContext.currentTargetSameAsTarget()) {
            if (m_event->bubbles())
                continue;
            m_event->setEventPhase(Event::AT_TARGET);
        } else
            m_event->setEventPhase(Event::CAPTURING_PHASE);
        eventContext.handleLocalEvents(m_event.get());
        if (m_event->propagationStopped())
            return DoneDispatching;
    }

    return ContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtTarget()
{
    m_event->setEventPhase(Event::AT_TARGET);
    m_eventPath[0]->handleLocalEvents(m_event.get());
    return m_event->propagationStopped() ? DoneDispatching : ContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtBubbling(WindowEventContext& windowContext)
{
    if (m_event->bubbles() && !m_event->cancelBubble()) {
        // Trigger bubbling event handlers, starting at the bottom and working our way up.
        m_event->setEventPhase(Event::BUBBLING_PHASE);

        size_t size = m_eventPath.size();
        for (size_t i = 1; i < size; ++i) {
            const EventContext& eventContext = *m_eventPath[i];
            if (eventContext.currentTargetSameAsTarget())
                m_event->setEventPhase(Event::AT_TARGET);
            else
                m_event->setEventPhase(Event::BUBBLING_PHASE);
            eventContext.handleLocalEvents(m_event.get());
            if (m_event->propagationStopped() || m_event->cancelBubble())
                return DoneDispatching;
        }
        windowContext.handleLocalEvents(m_event.get());
    }
    return ContinueDispatching;
}

inline void EventDispatcher::dispatchEventPostProcess(void* preDispatchEventHandlerResult)
{
    m_event->setTarget(EventRetargeter::eventTargetRespectingTargetRules(m_node.get()));
    m_event->setCurrentTarget(0);
    m_event->setEventPhase(0);

    // Pass the data from the preDispatchEventHandler to the postDispatchEventHandler.
    m_node->postDispatchEventHandler(m_event.get(), preDispatchEventHandlerResult);

    // Call default event handlers. While the DOM does have a concept of preventing
    // default handling, the detail of which handlers are called is an internal
    // implementation detail and not part of the DOM.
    if (!m_event->defaultPrevented() && !m_event->defaultHandled()) {
        // Non-bubbling events call only one default event handler, the one for the target.
        m_node->defaultEventHandler(m_event.get());
        ASSERT(!m_event->defaultPrevented());
        if (m_event->defaultHandled())
            return;
        // For bubbling events, call default event handlers on the same targets in the
        // same order as the bubbling phase.
        if (m_event->bubbles()) {
            size_t size = m_eventPath.size();
            for (size_t i = 1; i < size; ++i) {
                m_eventPath[i]->node()->defaultEventHandler(m_event.get());
                ASSERT(!m_event->defaultPrevented());
                if (m_event->defaultHandled())
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
