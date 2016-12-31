/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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

#pragma once

#include "Node.h"
#include "TreeScope.h"

namespace WebCore {

class TouchList;

class EventContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // FIXME: Use ContainerNode instead of Node.
    EventContext(Node*, EventTarget* currentTarget, EventTarget*);
    virtual ~EventContext();

    Node* node() const { return m_node.get(); }
    EventTarget* currentTarget() const { return m_currentTarget.get(); }
    EventTarget* target() const { return m_target.get(); }
    bool currentTargetSameAsTarget() const { return m_currentTarget.get() == m_target.get(); }
    virtual void handleLocalEvents(Event&) const;
    virtual bool isMouseOrFocusEventContext() const;
    virtual bool isTouchEventContext() const;

protected:
#if !ASSERT_DISABLED
    bool isUnreachableNode(EventTarget*) const;
#endif

    RefPtr<Node> m_node;
    RefPtr<EventTarget> m_currentTarget;
    RefPtr<EventTarget> m_target;
};

class MouseOrFocusEventContext final : public EventContext {
public:
    MouseOrFocusEventContext(Node*, EventTarget* currentTarget, EventTarget*);
    virtual ~MouseOrFocusEventContext();

    Node* relatedTarget() const { return m_relatedTarget.get(); }
    void setRelatedTarget(Node*);

private:
    void handleLocalEvents(Event&) const final;
    bool isMouseOrFocusEventContext() const final;

    RefPtr<Node> m_relatedTarget;
};

#if ENABLE(TOUCH_EVENTS)

class TouchEventContext final : public EventContext {
public:
    TouchEventContext(Node*, EventTarget* currentTarget, EventTarget*);
    virtual ~TouchEventContext();

    void handleLocalEvents(Event&) const override;
    bool isTouchEventContext() const override;

    enum TouchListType { Touches, TargetTouches, ChangedTouches, NotTouchList };
    TouchList* touchList(TouchListType);

    TouchList* touches() { return m_touches.get(); }
    TouchList* targetTouches() { return m_targetTouches.get(); }
    TouchList* changedTouches() { return m_changedTouches.get(); }

private:
#if !ASSERT_DISABLED
    void checkReachability(TouchList*) const;
#endif

    RefPtr<TouchList> m_touches;
    RefPtr<TouchList> m_targetTouches;
    RefPtr<TouchList> m_changedTouches;
};

#endif // ENABLE(TOUCH_EVENTS)

#if !ASSERT_DISABLED

inline bool EventContext::isUnreachableNode(EventTarget* target) const
{
    // FIXME: Checks also for SVG elements.
    return target && target->toNode() && !target->toNode()->isSVGElement() && m_node->isClosedShadowHidden(*target->toNode());
}

#endif

inline void MouseOrFocusEventContext::setRelatedTarget(Node* relatedTarget)
{
    ASSERT(!isUnreachableNode(relatedTarget));
    m_relatedTarget = relatedTarget;
}

#if ENABLE(TOUCH_EVENTS)

inline TouchList* TouchEventContext::touchList(TouchListType type)
{
    switch (type) {
    case Touches:
        return m_touches.get();
    case TargetTouches:
        return m_targetTouches.get();
    case ChangedTouches:
        return m_changedTouches.get();
    case NotTouchList:
        break;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

#endif

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MouseOrFocusEventContext)
static bool isType(const WebCore::EventContext& context) { return context.isMouseOrFocusEventContext(); }
SPECIALIZE_TYPE_TRAITS_END()

#if ENABLE(TOUCH_EVENTS)
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::TouchEventContext)
static bool isType(const WebCore::EventContext& context) { return context.isTouchEventContext(); }
SPECIALIZE_TYPE_TRAITS_END()
#endif
