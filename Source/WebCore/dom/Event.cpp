/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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
 */

#include "config.h"
#include "Event.h"

#include "DOMWindow.h"
#include "Document.h"
#include "EventNames.h"
#include "EventPath.h"
#include "EventTarget.h"
#include "Performance.h"
#include "UserGestureIndicator.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

Event::Event(IsTrusted isTrusted)
    : m_isTrusted(isTrusted == IsTrusted::Yes)
    , m_createTime(MonotonicTime::now())
{
}

Event::Event(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg)
    : m_type(eventType)
    , m_isInitialized(true)
    , m_canBubble(canBubbleArg)
    , m_cancelable(cancelableArg)
    , m_isTrusted(true)
    , m_createTime(MonotonicTime::now())
{
}

Event::Event(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg, MonotonicTime timestamp)
    : m_type(eventType)
    , m_isInitialized(true)
    , m_canBubble(canBubbleArg)
    , m_cancelable(cancelableArg)
    , m_isTrusted(true)
    , m_createTime(timestamp)
{
}

Event::Event(const AtomicString& eventType, const EventInit& initializer, IsTrusted isTrusted)
    : m_type(eventType)
    , m_isInitialized(true)
    , m_canBubble(initializer.bubbles)
    , m_cancelable(initializer.cancelable)
    , m_composed(initializer.composed)
    , m_isTrusted(isTrusted == IsTrusted::Yes)
    , m_createTime(MonotonicTime::now())
{
}

Event::~Event() = default;

Ref<Event> Event::create(const AtomicString& type, bool canBubble, bool cancelable)
{
    return adoptRef(*new Event(type, canBubble, cancelable));
}

Ref<Event> Event::createForBindings()
{
    return adoptRef(*new Event);
}

Ref<Event> Event::create(const AtomicString& type, const EventInit& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new Event(type, initializer, isTrusted));
}

void Event::initEvent(const AtomicString& eventTypeArg, bool canBubbleArg, bool cancelableArg)
{
    if (isBeingDispatched())
        return;

    m_isInitialized = true;
    m_propagationStopped = false;
    m_immediatePropagationStopped = false;
    m_wasCanceled = false;
    m_isTrusted = false;
    m_target = nullptr;
    m_type = eventTypeArg;
    m_canBubble = canBubbleArg;
    m_cancelable = cancelableArg;

    m_underlyingEvent = nullptr;
}

bool Event::composed() const
{
    if (m_composed)
        return true;

    // http://w3c.github.io/webcomponents/spec/shadow/#scoped-flag
    if (!isTrusted())
        return false;

    return m_type == eventNames().inputEvent
        || m_type == eventNames().textInputEvent
        || m_type == eventNames().DOMActivateEvent
        || isCompositionEvent()
        || isClipboardEvent()
        || isFocusEvent()
        || isKeyboardEvent()
        || isMouseEvent()
        || isTouchEvent()
        || isInputEvent();
}

void Event::setTarget(RefPtr<EventTarget>&& target)
{
    if (m_target == target)
        return;

    m_target = WTFMove(target);
    if (m_target)
        receivedTarget();
}

void Event::setCurrentTarget(EventTarget* currentTarget)
{
    m_currentTarget = currentTarget;
}

Vector<EventTarget*> Event::composedPath() const
{
    if (!m_eventPath)
        return Vector<EventTarget*>();
    return m_eventPath->computePathUnclosedToTarget(*m_currentTarget);
}

void Event::setUnderlyingEvent(Event* underlyingEvent)
{
    // Prohibit creation of a cycle by doing nothing if a cycle would be created.
    for (Event* event = underlyingEvent; event; event = event->underlyingEvent()) {
        if (event == this)
            return;
    }
    m_underlyingEvent = underlyingEvent;
}

DOMHighResTimeStamp Event::timeStampForBindings(ScriptExecutionContext& context) const
{
    Performance* performance = nullptr;
    if (is<WorkerGlobalScope>(context))
        performance = &downcast<WorkerGlobalScope>(context).performance();
    else if (auto* window = downcast<Document>(context).domWindow())
        performance = window->performance();

    if (!performance)
        return 0;

    return std::max(performance->relativeTimeFromTimeOriginInReducedResolution(m_createTime), 0.);
}

void Event::resetBeforeDispatch()
{
    m_defaultHandled = false;
}

void Event::resetAfterDispatch()
{
    m_eventPath = nullptr;
    m_currentTarget = nullptr;
    m_eventPhase = NONE;
    m_propagationStopped = false;
    m_immediatePropagationStopped = false;
}

} // namespace WebCore
