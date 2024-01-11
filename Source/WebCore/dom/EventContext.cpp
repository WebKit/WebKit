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
#include "EventNames.h"
#include "FocusEvent.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "LocalDOMWindow.h"
#include "MouseEvent.h"
#include "TouchEvent.h"

namespace WebCore {

void EventContext::handleLocalEvents(Event& event, EventInvokePhase phase) const
{
    event.setTarget(m_target.copyRef());
    event.setCurrentTarget(m_currentTarget.copyRef(), m_currentTargetIsInShadowTree);

    if (m_relatedTargetIsSet) {
        ASSERT(!m_relatedTarget || m_type == Type::MouseOrFocus);
        event.setRelatedTarget(m_relatedTarget.copyRef());
    }

#if ENABLE(TOUCH_EVENTS)
    if (m_type == Type::Touch) {

#if ASSERT_ENABLED
        auto checkReachability = [&](const Ref<TouchList>& touchList) {
            size_t length = touchList->length();
            for (size_t i = 0; i < length; ++i)
                ASSERT(!isUnreachableNode(downcast<Node>(touchList->item(i)->target())));
        };
        checkReachability(*m_touches);
        checkReachability(*m_targetTouches);
        checkReachability(*m_changedTouches);
#endif

        auto& touchEvent = downcast<TouchEvent>(event);
        touchEvent.setTouches(m_touches.get());
        touchEvent.setTargetTouches(m_targetTouches.get());
        touchEvent.setChangedTouches(m_changedTouches.get());
    }
#endif

    if (!m_node || UNLIKELY(m_type == Type::Window)) {
        protectedCurrentTarget()->fireEventListeners(event, phase);
        return;
    }

    if (UNLIKELY(m_contextNodeIsFormElement)) {
        ASSERT(is<HTMLFormElement>(*m_node));
        auto& eventNames = WebCore::eventNames();
        if ((event.type() == eventNames.submitEvent || event.type() == eventNames.resetEvent)
            && event.eventPhase() != Event::CAPTURING_PHASE && event.target() != m_node && is<Node>(event.target())) {
            event.stopPropagation();
            return;
        }
    }

    if (!m_node->hasEventTargetData())
        return;

    if (event.isTrusted()) {
        auto* element = dynamicDowncast<Element>(m_node.get());
        if (element && element->isDisabledFormControl() && event.isMouseEvent() && !event.isWheelEvent() && !m_node->document().settings().sendMouseEventsToDisabledFormControlsEnabled())
            return;
    }

    protectedNode()->fireEventListeners(event, phase);
}

#if ENABLE(TOUCH_EVENTS)

void EventContext::initializeTouchLists()
{
    m_touches = TouchList::create();
    m_targetTouches = TouchList::create();
    m_changedTouches = TouchList::create();
}

#endif // ENABLE(TOUCH_EVENTS)

#if ASSERT_ENABLED

bool EventContext::isUnreachableNode(EventTarget* target) const
{
    // FIXME: Checks also for SVG elements.
    auto* node = dynamicDowncast<Node>(target);
    return node && !node->isSVGElement() && m_node && m_node->isClosedShadowHidden(*node);
}

#endif

}
