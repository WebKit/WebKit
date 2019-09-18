/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 */

#pragma once

#if ENABLE(POINTER_EVENTS)

#include "EventNames.h"
#include "MouseEvent.h"
#include "Node.h"
#include "PointerID.h"
#include <wtf/text/WTFString.h>

#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
#include "PlatformTouchEventIOS.h"
#endif

namespace WebCore {

class PointerEvent final : public MouseEvent {
    WTF_MAKE_ISO_ALLOCATED(PointerEvent);
public:
    struct Init : MouseEventInit {
        PointerID pointerId { mousePointerID };
        double width { 1 };
        double height { 1 };
        float pressure { 0 };
        float tangentialPressure { 0 };
        long tiltX { 0 };
        long tiltY { 0 };
        long twist { 0 };
        String pointerType { PointerEvent::mousePointerType() };
        bool isPrimary { false };
    };

    enum class IsPrimary : uint8_t { No, Yes };

    static Ref<PointerEvent> create(const AtomString& type, Init&& initializer)
    {
        return adoptRef(*new PointerEvent(type, WTFMove(initializer)));
    }

    static Ref<PointerEvent> createForPointerCapture(const AtomString& type, PointerID pointerId, bool isPrimary, String pointerType)
    {
        Init initializer;
        initializer.bubbles = true;
        initializer.pointerId = pointerId;
        initializer.isPrimary = isPrimary;
        initializer.pointerType = pointerType;
        return adoptRef(*new PointerEvent(type, WTFMove(initializer)));
    }

    static Ref<PointerEvent> createForBindings()
    {
        return adoptRef(*new PointerEvent);
    }

    static RefPtr<PointerEvent> create(short button, const MouseEvent&);
    static Ref<PointerEvent> create(const String& type, short button, const MouseEvent&);
    static Ref<PointerEvent> create(const String& type, PointerID, const String& pointerType, IsPrimary = IsPrimary::No);

#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
    static Ref<PointerEvent> create(const PlatformTouchEvent&, unsigned touchIndex, bool isPrimary, Ref<WindowProxy>&&);
    static Ref<PointerEvent> create(const String& type, const PlatformTouchEvent&, unsigned touchIndex, bool isPrimary, Ref<WindowProxy>&&);
#endif

    static const String& mousePointerType();
    static const String& penPointerType();
    static const String& touchPointerType();

    virtual ~PointerEvent();

    PointerID pointerId() const { return m_pointerId; }
    double width() const { return m_width; }
    double height() const { return m_height; }
    float pressure() const { return m_pressure; }
    float tangentialPressure() const { return m_tangentialPressure; }
    long tiltX() const { return m_tiltX; }
    long tiltY() const { return m_tiltY; }
    long twist() const { return m_twist; }
    String pointerType() const { return m_pointerType; }
    bool isPrimary() const { return m_isPrimary; }

    bool isPointerEvent() const final { return true; }

    // https://w3c.github.io/pointerevents/#attributes-and-default-actions
    // Many user agents expose non-standard attributes fromElement and toElement in MouseEvents to
    // support legacy content. In those user agents, the values of those (inherited) attributes in
    // PointerEvents must be null to encourage the use of the standardized alternates (i.e. target
    // and relatedTarget).
    RefPtr<Node> toElement() const final { return nullptr; }
    RefPtr<Node> fromElement() const final { return nullptr; }

    EventInterface eventInterface() const override;

private:
    static bool typeIsEnterOrLeave(const AtomString& type) { return type == eventNames().pointerenterEvent || type == eventNames().pointerleaveEvent; }
    static CanBubble typeCanBubble(const AtomString& type) { return typeIsEnterOrLeave(type) ? CanBubble::No : CanBubble::Yes; }
    static IsCancelable typeIsCancelable(const AtomString& type) { return typeIsEnterOrLeave(type) ? IsCancelable::No : IsCancelable::Yes; }
    static IsComposed typeIsComposed(const AtomString& type) { return typeIsEnterOrLeave(type) ? IsComposed::No : IsComposed::Yes; }

    PointerEvent();
    PointerEvent(const AtomString&, Init&&);
    PointerEvent(const AtomString& type, short button, const MouseEvent&);
    PointerEvent(const AtomString& type, PointerID, const String& pointerType, IsPrimary);
#if ENABLE(TOUCH_EVENTS) && PLATFORM(IOS_FAMILY)
    PointerEvent(const AtomString& type, const PlatformTouchEvent&, IsCancelable isCancelable, unsigned touchIndex, bool isPrimary, Ref<WindowProxy>&&);
#endif

    PointerID m_pointerId { mousePointerID };
    double m_width { 1 };
    double m_height { 1 };
    float m_pressure { 0 };
    float m_tangentialPressure { 0 };
    long m_tiltX { 0 };
    long m_tiltY { 0 };
    long m_twist { 0 };
    String m_pointerType { PointerEvent::mousePointerType() };
    bool m_isPrimary { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENT(PointerEvent)

#endif // ENABLE(POINTER_EVENTS)
