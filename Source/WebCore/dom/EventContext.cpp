/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "config.h"
#include "EventContext.h"

#include "Document.h"
#include "FocusEvent.h"
#include "MouseEvent.h"
#include "TouchEvent.h"

namespace WebCore {

EventContext::EventContext(Node* node, EventTarget* currentTarget, EventTarget* target, int closedShadowDepth)
    : m_node { node }
    , m_currentTarget { currentTarget }
    , m_target { target }
    , m_closedShadowDepth { closedShadowDepth }
{
    ASSERT(!isUnreachableNode(m_target.get()));
}

EventContext::~EventContext() = default;

void EventContext::handleLocalEvents(Event& event, EventInvokePhase phase) const
{
    event.setTarget(m_target.get());
    event.setCurrentTarget(m_currentTarget.get());
    // FIXME: Consider merging handleLocalEvents and fireEventListeners.
    if (m_node)
        m_node->handleLocalEvents(event, phase);
    else
        m_currentTarget->fireEventListeners(event, phase);
}

bool EventContext::isMouseOrFocusEventContext() const
{
    return false;
}

bool EventContext::isTouchEventContext() const
{
    return false;
}

MouseOrFocusEventContext::MouseOrFocusEventContext(Node& node, EventTarget* currentTarget, EventTarget* target, int closedShadowDepth)
    : EventContext(&node, currentTarget, target, closedShadowDepth)
{
}

MouseOrFocusEventContext::~MouseOrFocusEventContext() = default;

void MouseOrFocusEventContext::handleLocalEvents(Event& event, EventInvokePhase phase) const
{
    if (m_relatedTarget)
        event.setRelatedTarget(*m_relatedTarget);
    EventContext::handleLocalEvents(event, phase);
}

bool MouseOrFocusEventContext::isMouseOrFocusEventContext() const
{
    return true;
}

#if ENABLE(TOUCH_EVENTS)

TouchEventContext::TouchEventContext(Node& node, EventTarget* currentTarget, EventTarget* target, int closedShadowDepth)
    : EventContext(&node, currentTarget, target, closedShadowDepth)
    , m_touches(TouchList::create())
    , m_targetTouches(TouchList::create())
    , m_changedTouches(TouchList::create())
{
}

TouchEventContext::~TouchEventContext() = default;

void TouchEventContext::handleLocalEvents(Event& event, EventInvokePhase phase) const
{
    checkReachability(m_touches);
    checkReachability(m_targetTouches);
    checkReachability(m_changedTouches);
    auto& touchEvent = downcast<TouchEvent>(event);
    touchEvent.setTouches(m_touches.ptr());
    touchEvent.setTargetTouches(m_targetTouches.ptr());
    touchEvent.setChangedTouches(m_changedTouches.ptr());
    EventContext::handleLocalEvents(event, phase);
}

bool TouchEventContext::isTouchEventContext() const
{
    return true;
}

#if !ASSERT_DISABLED

void TouchEventContext::checkReachability(const Ref<TouchList>& touchList) const
{
    size_t length = touchList->length();
    for (size_t i = 0; i < length; ++i)
        ASSERT(!isUnreachableNode(downcast<Node>(touchList->item(i)->target())));
}

#endif

#endif // ENABLE(TOUCH_EVENTS)

}
