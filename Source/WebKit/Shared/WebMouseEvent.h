/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

// FIXME: We should probably move to making the WebCore/PlatformFooEvents trivial classes so that
// we can use them as the event type.

#include "WebEvent.h"
#include <WebCore/IntPoint.h>
#include <WebCore/PointerEventTypeNames.h>
#include <WebCore/PointerID.h>

namespace WebKit {

enum class GestureWasCancelled : bool { No, Yes };

class WebMouseEvent : public WebEvent {
public:
    enum Button {
        LeftButton = 0,
        MiddleButton,
        RightButton,
        NoButton = -2
    };

    enum SyntheticClickType { NoTap, OneFingerTap, TwoFingerTap };

    WebMouseEvent();

#if PLATFORM(MAC)
    WebMouseEvent(Type, Button, unsigned short buttons, const WebCore::IntPoint& positionInView, const WebCore::IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, OptionSet<Modifier>, WallTime timestamp, double force, SyntheticClickType = NoTap, int eventNumber = -1, int menuType = 0, GestureWasCancelled = GestureWasCancelled::No);
#else
    WebMouseEvent(Type, Button, unsigned short buttons, const WebCore::IntPoint& positionInView, const WebCore::IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, OptionSet<Modifier>, WallTime timestamp, double force = 0, SyntheticClickType = NoTap, WebCore::PointerID = WebCore::mousePointerID, const String& pointerType = WebCore::mousePointerEventType(), GestureWasCancelled = GestureWasCancelled::No);
#endif

    Button button() const { return static_cast<Button>(m_button); }
    unsigned short buttons() const { return m_buttons; }
    const WebCore::IntPoint& position() const { return m_position; } // Relative to the view.
    const WebCore::IntPoint& globalPosition() const { return m_globalPosition; }
    float deltaX() const { return m_deltaX; }
    float deltaY() const { return m_deltaY; }
    float deltaZ() const { return m_deltaZ; }
    int32_t clickCount() const { return m_clickCount; }
#if PLATFORM(MAC)
    int32_t eventNumber() const { return m_eventNumber; }
    int32_t menuTypeForEvent() const { return m_menuTypeForEvent; }
#endif
    double force() const { return m_force; }
    SyntheticClickType syntheticClickType() const { return static_cast<SyntheticClickType>(m_syntheticClickType); }
    WebCore::PointerID pointerId() const { return m_pointerId; }
    const String& pointerType() const { return m_pointerType; }
    GestureWasCancelled gestureWasCancelled() const { return m_gestureWasCancelled; }

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, WebMouseEvent&);

private:
    static bool isMouseEventType(Type);

    uint32_t m_button { static_cast<uint32_t>(NoButton) };
    unsigned short m_buttons { 0 };
    WebCore::IntPoint m_position; // Relative to the view.
    WebCore::IntPoint m_globalPosition;
    float m_deltaX { 0 };
    float m_deltaY { 0 };
    float m_deltaZ { 0 };
    int32_t m_clickCount { 0 };
#if PLATFORM(MAC)
    int32_t m_eventNumber { -1 };
    int32_t m_menuTypeForEvent { 0 };
#endif
    double m_force { 0 };
    uint32_t m_syntheticClickType { NoTap };
    WebCore::PointerID m_pointerId { WebCore::mousePointerID };
    String m_pointerType { WebCore::mousePointerEventType() };
    GestureWasCancelled m_gestureWasCancelled { GestureWasCancelled::No };
};

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::WebMouseEvent::Button> {
    using values = EnumValues<
        WebKit::WebMouseEvent::Button,
        WebKit::WebMouseEvent::Button::LeftButton,
        WebKit::WebMouseEvent::Button::MiddleButton,
        WebKit::WebMouseEvent::Button::RightButton,
        WebKit::WebMouseEvent::Button::NoButton
    >;
};

template<> struct EnumTraits<WebKit::WebMouseEvent::SyntheticClickType> {
    using values = EnumValues<
        WebKit::WebMouseEvent::SyntheticClickType,
        WebKit::WebMouseEvent::SyntheticClickType::NoTap,
        WebKit::WebMouseEvent::SyntheticClickType::OneFingerTap,
        WebKit::WebMouseEvent::SyntheticClickType::TwoFingerTap
    >;
};

} // namespace WTF
