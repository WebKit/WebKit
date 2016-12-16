/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008, 2013 Apple Inc. All rights reserved.
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

#include "EventNames.h"
#include "EventPath.h"
#include "EventTarget.h"
#include "RuntimeApplicationChecks.h"
#include "UserGestureIndicator.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

Event::Event(IsTrusted isTrusted)
    : m_isTrusted(isTrusted == IsTrusted::Yes)
    , m_createTime(convertSecondsToDOMTimeStamp(currentTime()))
{
}

Event::Event(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg)
    : m_type(eventType)
    , m_isInitialized(true)
    , m_canBubble(canBubbleArg)
    , m_cancelable(cancelableArg)
    , m_isTrusted(true)
    , m_createTime(convertSecondsToDOMTimeStamp(currentTime()))
{
}

Event::Event(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg, double timestamp)
    : m_type(eventType)
    , m_isInitialized(true)
    , m_canBubble(canBubbleArg)
    , m_cancelable(cancelableArg)
    , m_isTrusted(true)
    , m_createTime(convertSecondsToDOMTimeStamp(timestamp))
{
}

Event::Event(const AtomicString& eventType, const EventInit& initializer, IsTrusted isTrusted)
    : m_type(eventType)
    , m_isInitialized(true)
    , m_canBubble(initializer.bubbles)
    , m_cancelable(initializer.cancelable)
    , m_composed(initializer.composed)
    , m_isTrusted(isTrusted == IsTrusted::Yes)
    , m_createTime(convertSecondsToDOMTimeStamp(currentTime()))
{
}

Event::~Event()
{
}

void Event::initEvent(const AtomicString& eventTypeArg, bool canBubbleArg, bool cancelableArg)
{
    if (isBeingDispatched())
        return;

    m_isInitialized = true;
    m_propagationStopped = false;
    m_immediatePropagationStopped = false;
    m_defaultPrevented = false;
    m_isTrusted = false;
    m_target = nullptr;

    m_type = eventTypeArg;
    m_canBubble = canBubbleArg;
    m_cancelable = cancelableArg;
}

ExceptionOr<void> Event::initEventForBindings(ScriptExecutionContext& scriptExecutionContext, const AtomicString& type, bool bubbles)
{
#if PLATFORM(IOS)
    // FIXME: Temporary quirk for Baidu Nuomi App which calls initEvent() with too few parameters (rdar://problem/28707838).
    // FIXME: Quirk also needed for Flipboard for same reason (rdar://problem/28264190).
    if (IOSApplication::isBaiduNuomi() || IOSApplication::isFlipboard()) {
        scriptExecutionContext.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, ASCIILiteral("Calling Event.prototype.initEvent() with less than 3 parameters is deprecated."));
        initEvent(type, bubbles, false);
        return { };
    }
#else
    UNUSED_PARAM(scriptExecutionContext);
    UNUSED_PARAM(type);
    UNUSED_PARAM(bubbles);
#endif
    return Exception { TypeError, ASCIILiteral("Not enough arguments") };
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

EventInterface Event::eventInterface() const
{
    return EventInterfaceType;
}

bool Event::isUIEvent() const
{
    return false;
}

bool Event::isMouseEvent() const
{
    return false;
}

bool Event::isFocusEvent() const
{
    return false;
}

bool Event::isKeyboardEvent() const
{
    return false;
}

bool Event::isInputEvent() const
{
    return false;
}

bool Event::isCompositionEvent() const
{
    return false;
}

bool Event::isTouchEvent() const
{
    return false;
}

bool Event::isDragEvent() const
{
    return false;
}

bool Event::isClipboardEvent() const
{
    return false;
}

bool Event::isBeforeTextInsertedEvent() const
{
    return false;
}

bool Event::isBeforeUnloadEvent() const
{
    return false;
}

bool Event::isErrorEvent() const
{
    return false;
}

bool Event::isTextEvent() const
{
    return false;
}

bool Event::isWheelEvent() const
{
    return false;
}

Ref<Event> Event::cloneFor(HTMLIFrameElement*) const
{
    return Event::create(type(), bubbles(), cancelable());
}

void Event::setTarget(RefPtr<EventTarget>&& target)
{
    if (m_target == target)
        return;

    m_target = WTFMove(target);
    if (m_target)
        receivedTarget();
}

Vector<EventTarget*> Event::composedPath() const
{
    if (!m_eventPath)
        return Vector<EventTarget*>();
    return m_eventPath->computePathUnclosedToTarget(*m_currentTarget);
}

void Event::receivedTarget()
{
}

void Event::setUnderlyingEvent(Event* underlyingEvent)
{
    // Prohibit creation of a cycle -- just do nothing in that case.
    for (Event* event = underlyingEvent; event; event = event->underlyingEvent()) {
        if (event == this)
            return;
    }
    m_underlyingEvent = underlyingEvent;
}

} // namespace WebCore
