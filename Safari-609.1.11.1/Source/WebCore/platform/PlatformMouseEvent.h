/*
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef PlatformMouseEvent_h
#define PlatformMouseEvent_h

#include "IntPoint.h"
#include "PlatformEvent.h"
#include "PointerID.h"
#include <wtf/WindowsExtras.h>

#if PLATFORM(GTK)
typedef struct _GdkEventButton GdkEventButton;
typedef struct _GdkEventMotion GdkEventMotion;
#endif

namespace WebCore {

const double ForceAtClick = 1;
const double ForceAtForceClick = 2;

    // These button numbers match the ones used in the DOM API, 0 through 2, except for NoButton which isn't specified.
    // We use -2 for NoButton because -1 is a valid value in the DOM API for Pointer Events for pointermove events that
    // indicate that the pressed mouse button hasn't changed since the last event.
    enum MouseButton : int8_t { LeftButton = 0, MiddleButton, RightButton, NoButton = -2 };
    enum SyntheticClickType : int8_t { NoTap, OneFingerTap, TwoFingerTap };

    class PlatformMouseEvent : public PlatformEvent {
    public:
        PlatformMouseEvent()
            : PlatformEvent(PlatformEvent::MouseMoved)
            , m_button(NoButton)
            , m_clickCount(0)
            , m_modifierFlags(0)
#if PLATFORM(MAC)
            , m_eventNumber(0)
            , m_menuTypeForEvent(0)
#elif PLATFORM(WIN)
            , m_didActivateWebView(false)
#endif
        {
        }

        PlatformMouseEvent(const IntPoint& position, const IntPoint& globalPosition, MouseButton button, PlatformEvent::Type type,
                           int clickCount, bool shiftKey, bool ctrlKey, bool altKey, bool metaKey, WallTime timestamp, double force, SyntheticClickType syntheticClickType, PointerID pointerId = mousePointerID)
            : PlatformEvent(type, shiftKey, ctrlKey, altKey, metaKey, timestamp)
            , m_position(position)
            , m_globalPosition(globalPosition)
            , m_button(button)
            , m_clickCount(clickCount)
            , m_modifierFlags(0)
            , m_force(force)
            , m_syntheticClickType(syntheticClickType)
            , m_pointerId(pointerId)
#if PLATFORM(MAC)
            , m_eventNumber(0)
            , m_menuTypeForEvent(0)
#elif PLATFORM(WIN)
            , m_didActivateWebView(false)
#endif
        {
        }

        const IntPoint& position() const { return m_position; }
        const IntPoint& globalPosition() const { return m_globalPosition; }
#if ENABLE(POINTER_LOCK)
        const IntPoint& movementDelta() const { return m_movementDelta; }
#endif

        MouseButton button() const { return m_button; }
        unsigned short buttons() const { return m_buttons; }
        int clickCount() const { return m_clickCount; }
        unsigned modifierFlags() const { return m_modifierFlags; }
        double force() const { return m_force; }
        SyntheticClickType syntheticClickType() const { return m_syntheticClickType; }
        PointerID pointerId() const { return m_pointerId; }

#if PLATFORM(GTK) 
        explicit PlatformMouseEvent(GdkEventButton*);
        explicit PlatformMouseEvent(GdkEventMotion*);
        void setClickCount(int count) { m_clickCount = count; }
#endif

#if PLATFORM(MAC)
        int eventNumber() const { return m_eventNumber; }
        int menuTypeForEvent() const { return m_menuTypeForEvent; }
#endif

#if PLATFORM(WIN)
        PlatformMouseEvent(HWND, UINT, WPARAM, LPARAM, bool didActivateWebView = false);
        void setClickCount(int count) { m_clickCount = count; }
        bool didActivateWebView() const { return m_didActivateWebView; }
#endif

    protected:
        IntPoint m_position;
        IntPoint m_globalPosition;
#if ENABLE(POINTER_LOCK)
        IntPoint m_movementDelta;
#endif
        MouseButton m_button;
        unsigned short m_buttons { 0 };
        int m_clickCount;
        unsigned m_modifierFlags;
        double m_force { 0 };
        SyntheticClickType m_syntheticClickType { NoTap };
        PointerID m_pointerId { mousePointerID };

#if PLATFORM(MAC)
        int m_eventNumber;
        int m_menuTypeForEvent;
#elif PLATFORM(WIN)
        bool m_didActivateWebView;
#endif
    };

#if COMPILER(MSVC)
    // These functions are necessary to work around the fact that MSVC will not find a most-specific
    // operator== to use after implicitly converting MouseButton to a short.
    inline bool operator==(short a, MouseButton b)
    {
        return a == static_cast<short>(b);
    }

    inline bool operator!=(short a, MouseButton b)
    {
        return a != static_cast<short>(b);
    }
#endif

} // namespace WebCore

#endif // PlatformMouseEvent_h
