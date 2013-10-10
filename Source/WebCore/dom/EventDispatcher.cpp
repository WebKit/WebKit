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
#include "WindowEventContext.h"
#include <wtf/RefPtr.h>

namespace WebCore {

bool EventDispatcher::dispatchEvent(Node* node, PassRefPtr<Event> event)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    if (!event)
        return true;
    EventDispatcher dispatcher(node, event);
    return dispatcher.dispatch();
}

EventDispatcher::EventDispatcher(Node* node, PassRefPtr<Event> event)
    : m_eventPath(*node, *event)
    , m_node(node)
    , m_event(event)
#ifndef NDEBUG
    , m_eventDispatched(false)
#endif
{
    ASSERT(m_node);
    ASSERT(m_event);
    ASSERT(!m_event->type().isNull()); // JavaScript code can create an event with an empty name, but not null.
    m_view = node->document().view();
}

void EventDispatcher::dispatchScopedEvent(Node& node, PassRefPtr<Event> event)
{
    // We need to set the target here because it can go away by the time we actually fire the event.
    event->setTarget(&EventRetargeter::eventTargetRespectingTargetRules(node));
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
        EventDispatcher(element, SimulatedMouseEvent::create(eventNames().mouseoverEvent, element->document().defaultView(), underlyingEvent)).dispatch();

    if (mouseEventOptions != SendNoEvents)
        EventDispatcher(element, SimulatedMouseEvent::create(eventNames().mousedownEvent, element->document().defaultView(), underlyingEvent)).dispatch();
    element->setActive(true, visualOptions == ShowPressedLook);
    if (mouseEventOptions != SendNoEvents)
        EventDispatcher(element, SimulatedMouseEvent::create(eventNames().mouseupEvent, element->document().defaultView(), underlyingEvent)).dispatch();
    element->setActive(false);

    // always send click
    EventDispatcher(element, SimulatedMouseEvent::create(eventNames().clickEvent, element->document().defaultView(), underlyingEvent)).dispatch();

    elementsDispatchingSimulatedClicks.remove(element);
}

bool EventDispatcher::dispatch()
{
    if (EventTarget* relatedTarget = m_event->relatedTarget())
        m_eventPath.setRelatedTarget(*relatedTarget);

#ifndef NDEBUG
    ASSERT(!m_eventDispatched);
    m_eventDispatched = true;
#endif
    ChildNodesLazySnapshot::takeChildNodesLazySnapshot();

    ASSERT(m_node);
    m_event->setTarget(&EventRetargeter::eventTargetRespectingTargetRules(*m_node));
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(m_event->target());
    WindowEventContext windowEventContext(m_event.get(), m_node.get(), topEventContext());
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchEvent(&m_node->document(), *m_event, windowEventContext.window(), m_node.get(), m_eventPath);

    InputElementClickState clickHandlingState;
    if (isHTMLInputElement(m_node.get()))
        toHTMLInputElement(m_node.get())->willDispatchEvent(*m_event.get(), clickHandlingState);

    if (!m_event->propagationStopped() && !m_eventPath.isEmpty()) {
        if (dispatchEventAtCapturing(windowEventContext) == ContinueDispatching) {
            if (dispatchEventAtTarget() == ContinueDispatching)
                dispatchEventAtBubbling(windowEventContext);
        }
    }

    dispatchEventPostProcess(clickHandlingState);

    // Ensure that after event dispatch, the event's target object is the
    // outermost shadow DOM boundary.
    m_event->setTarget(windowEventContext.target());
    m_event->setCurrentTarget(0);
    InspectorInstrumentation::didDispatchEvent(cookie);

    return !m_event->defaultPrevented();
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtCapturing(WindowEventContext& windowEventContext)
{
    // Trigger capturing event handlers, starting at the top and working our way down.
    m_event->setEventPhase(Event::CAPTURING_PHASE);

    if (windowEventContext.handleLocalEvents(m_event.get()) && m_event->propagationStopped())
        return DoneDispatching;

    for (size_t i = m_eventPath.size() - 1; i > 0; --i) {
        const EventContext& eventContext = m_eventPath.contextAt(i);
        if (eventContext.currentTargetSameAsTarget())
            continue;
        eventContext.handleLocalEvents(m_event.get());
        if (m_event->propagationStopped())
            return DoneDispatching;
    }

    return ContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtTarget()
{
    m_event->setEventPhase(Event::AT_TARGET);
    m_eventPath.contextAt(0).handleLocalEvents(m_event.get());
    return m_event->propagationStopped() ? DoneDispatching : ContinueDispatching;
}

inline void EventDispatcher::dispatchEventAtBubbling(WindowEventContext& windowContext)
{
    // Trigger bubbling event handlers, starting at the bottom and working our way up.
    size_t size = m_eventPath.size();
    for (size_t i = 1; i < size; ++i) {
        const EventContext& eventContext = m_eventPath.contextAt(i);
        if (eventContext.currentTargetSameAsTarget())
            m_event->setEventPhase(Event::AT_TARGET);
        else if (m_event->bubbles() && !m_event->cancelBubble())
            m_event->setEventPhase(Event::BUBBLING_PHASE);
        else
            continue;
        eventContext.handleLocalEvents(m_event.get());
        if (m_event->propagationStopped())
            return;
    }
    if (m_event->bubbles() && !m_event->cancelBubble()) {
        m_event->setEventPhase(Event::BUBBLING_PHASE);
        windowContext.handleLocalEvents(m_event.get());
    }
}

inline void EventDispatcher::dispatchEventPostProcess(const InputElementClickState& InputElementClickState)
{
    ASSERT(m_node);
    m_event->setTarget(&EventRetargeter::eventTargetRespectingTargetRules(*m_node));
    m_event->setCurrentTarget(0);
    m_event->setEventPhase(0);

    if (InputElementClickState.stateful)
        toHTMLInputElement(m_node.get())->didDispatchClickEvent(*m_event.get(), InputElementClickState);

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
                m_eventPath.contextAt(i).node()->defaultEventHandler(m_event.get());
                ASSERT(!m_event->defaultPrevented());
                if (m_event->defaultHandled())
                    return;
            }
        }
    }
}

const EventContext* EventDispatcher::topEventContext()
{
    return m_eventPath.lastContextIfExists();
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
