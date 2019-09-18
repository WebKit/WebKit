/*
 * Copyright 2008, The Android Open Source Project
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
 */

#include "config.h"

#if ENABLE(TOUCH_EVENTS)

#include "TouchEvent.h"

#include "EventDispatcher.h"

namespace WebCore {

TouchEvent::TouchEvent() = default;

TouchEvent::TouchEvent(TouchList* touches, TouchList* targetTouches, TouchList* changedTouches, const AtomString& type,
    RefPtr<WindowProxy>&& view, const IntPoint& globalLocation, OptionSet<Modifier> modifiers)
    : MouseRelatedEvent(type, IsCancelable::Yes, MonotonicTime::now(), WTFMove(view), globalLocation, modifiers)
    , m_touches(touches)
    , m_targetTouches(targetTouches)
    , m_changedTouches(changedTouches)
{
}

TouchEvent::TouchEvent(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
    : MouseRelatedEvent(type, initializer, isTrusted)
    , m_touches(initializer.touches ? initializer.touches : TouchList::create())
    , m_targetTouches(initializer.targetTouches ? initializer.targetTouches : TouchList::create())
    , m_changedTouches(initializer.changedTouches ? initializer.changedTouches : TouchList::create())
{
}

TouchEvent::~TouchEvent() = default;

void TouchEvent::initTouchEvent(TouchList* touches, TouchList* targetTouches,
        TouchList* changedTouches, const AtomString& type, 
        RefPtr<WindowProxy>&& view, int screenX, int screenY, int clientX, int clientY,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey)
{
    if (isBeingDispatched())
        return;

    initUIEvent(type, true, true, WTFMove(view), 0);

    m_touches = touches;
    m_targetTouches = targetTouches;
    m_changedTouches = changedTouches;
    m_screenLocation = IntPoint(screenX, screenY);
    setModifierKeys(ctrlKey, altKey, shiftKey, metaKey);
    initCoordinates(IntPoint(clientX, clientY));
}

EventInterface TouchEvent::eventInterface() const
{
    return TouchEventInterfaceType;
}

bool TouchEvent::isTouchEvent() const
{
    return true;
}

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)
