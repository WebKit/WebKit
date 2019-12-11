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

#pragma once

#if ENABLE(IOS_TOUCH_EVENTS)
#include <WebKitAdditions/TouchEventIOS.h>
#elif ENABLE(TOUCH_EVENTS)

#include "MouseRelatedEvent.h"
#include "TouchList.h"

namespace WebCore {

class TouchEvent final : public MouseRelatedEvent {
    WTF_MAKE_ISO_ALLOCATED(TouchEvent);
public:
    virtual ~TouchEvent();

    static Ref<TouchEvent> create(TouchList* touches, TouchList* targetTouches, TouchList* changedTouches,
        const AtomString& type, RefPtr<WindowProxy>&& view, const IntPoint& globalLocation, OptionSet<Modifier> modifiers)
    {
        return adoptRef(*new TouchEvent(touches, targetTouches, changedTouches, type, WTFMove(view), globalLocation, modifiers));
    }
    static Ref<TouchEvent> createForBindings()
    {
        return adoptRef(*new TouchEvent);
    }

    struct Init : MouseRelatedEventInit {
        RefPtr<TouchList> touches;
        RefPtr<TouchList> targetTouches;
        RefPtr<TouchList> changedTouches;
    };

    static Ref<TouchEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new TouchEvent(type, initializer, isTrusted));
    }

    void initTouchEvent(TouchList* touches, TouchList* targetTouches, TouchList* changedTouches, const AtomString& type,
        RefPtr<WindowProxy>&&, int screenX, int screenY, int clientX, int clientY, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey);

    TouchList* touches() const { return m_touches.get(); }
    TouchList* targetTouches() const { return m_targetTouches.get(); }
    TouchList* changedTouches() const { return m_changedTouches.get(); }

    void setTouches(RefPtr<TouchList>&& touches) { m_touches = touches; }
    void setTargetTouches(RefPtr<TouchList>&& targetTouches) { m_targetTouches = targetTouches; }
    void setChangedTouches(RefPtr<TouchList>&& changedTouches) { m_changedTouches = changedTouches; }

    bool isTouchEvent() const override;

    EventInterface eventInterface() const override;

private:
    TouchEvent();
    TouchEvent(TouchList* touches, TouchList* targetTouches, TouchList* changedTouches, const AtomString& type,
        RefPtr<WindowProxy>&&, const IntPoint& globalLocation, OptionSet<Modifier>);
    TouchEvent(const AtomString&, const Init&, IsTrusted);

    RefPtr<TouchList> m_touches;
    RefPtr<TouchList> m_targetTouches;
    RefPtr<TouchList> m_changedTouches;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENT(TouchEvent)

#endif // ENABLE(TOUCH_EVENTS)
