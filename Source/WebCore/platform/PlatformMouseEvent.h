/*
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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

#include <WebCore/DoublePoint.h>
#include <WebCore/IntPoint.h>
#include <WebCore/MouseEventTypes.h>
#include <WebCore/PlatformEvent.h>
#include <WebCore/PointerEventTypeNames.h>
#include <WebCore/PointerID.h>
#include <wtf/Platform.h>
#include <wtf/UUID.h>
#include <wtf/WindowsExtras.h>

namespace WebCore {

class PlatformMouseEvent : public PlatformEvent {
public:
    PlatformMouseEvent()
        : PlatformEvent(Type::MouseMoved)
    {
    }

    PlatformMouseEvent(const DoublePoint& position, const DoublePoint& globalPosition, MouseButton button, PlatformEvent::Type type, int clickCount, OptionSet<PlatformEvent::Modifier> modifiers, MonotonicTime timestamp, double force, SyntheticClickType syntheticClickType, PointerID pointerId = mousePointerID)
        : PlatformEvent(type, modifiers, timestamp)
        , m_button(button)
        , m_syntheticClickType(syntheticClickType)
        , m_position(position)
        , m_globalPosition(globalPosition)
        , m_force(force)
        , m_pointerId(pointerId)
        , m_clickCount(clickCount)
    {
    }

    // This position is relative to the enclosing NSWindow in WebKit1, and is WKWebView-relative in WebKit2.
    // Use ScrollView::windowToContents() to convert it to into the contents of a given view.
    const DoublePoint& position() const { return m_position; }
    const DoublePoint& globalPosition() const { return m_globalPosition; }
    const DoublePoint& movementDelta() const { return m_movementDelta; }
    // Unaccelerated pointer movement
    const DoublePoint& unadjustedMovementDelta() const { return m_unadjustedMovementDelta; }

    MouseButton button() const { return m_button; }
    unsigned short buttons() const { return m_buttons; }
    int clickCount() const { return m_clickCount; }
    unsigned modifierFlags() const { return m_modifierFlags; }
    double force() const { return m_force; }
    SyntheticClickType syntheticClickType() const { return m_syntheticClickType; }
    PointerID pointerId() const { return m_pointerId; }
    const String& pointerType() const { return m_pointerType; }

    Vector<PlatformMouseEvent> coalescedEvents() const { return m_coalescedEvents; }
    Vector<PlatformMouseEvent> predictedEvents() const { return m_predictedEvents; }

#if PLATFORM(MAC)
    int eventNumber() const { return m_eventNumber; }
    int menuTypeForEvent() const { return m_menuTypeForEvent; }
#endif

#if PLATFORM(WIN)
    WEBCORE_EXPORT PlatformMouseEvent(HWND, UINT, WPARAM, LPARAM, bool didActivateWebView = false);
    void setClickCount(int count) { m_clickCount = count; }
    bool didActivateWebView() const { return m_didActivateWebView; }
#endif

#if PLATFORM(GTK)
    enum class IsTouch : bool { No, Yes };

    bool isTouchEvent() const { return m_isTouchEvent == IsTouch::Yes; }
#endif

protected:
    MouseButton m_button { MouseButton::None };
    SyntheticClickType m_syntheticClickType { SyntheticClickType::NoTap };

    DoublePoint m_position;
    DoublePoint m_globalPosition;
    DoublePoint m_movementDelta;
    DoublePoint m_unadjustedMovementDelta;
    double m_force { 0 };
    PointerID m_pointerId { mousePointerID };
    String m_pointerType { mousePointerEventType() };
    int m_clickCount { 0 };
    unsigned m_modifierFlags { 0 };
    unsigned short m_buttons { 0 };
    Vector<PlatformMouseEvent> m_coalescedEvents;
    Vector<PlatformMouseEvent> m_predictedEvents;
#if PLATFORM(MAC)
    int m_eventNumber { 0 };
    int m_menuTypeForEvent { 0 };
#elif PLATFORM(WIN)
    bool m_didActivateWebView { false };
#elif PLATFORM(GTK)
    IsTouch m_isTouchEvent { IsTouch::No };
#endif
};

} // namespace WebCore
