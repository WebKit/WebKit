/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef PlatformEvent_h
#define PlatformEvent_h

namespace WebCore {

class PlatformEvent {
public:
    enum Type {
        NoType = 0,

        // PlatformKeyboardEvent
        KeyDown,
        KeyUp,
        RawKeyDown,
        Char,

        // PlatformMouseEvent
        MouseMoved,
        MousePressed,
        MouseReleased,
        MouseScroll,

        // PlatformWheelEvent
        Wheel,

        // PlatformGestureEvent
        GestureScrollBegin,
        GestureScrollEnd,
        GestureScrollUpdate,
        GestureTap,
        GestureTapDown,
        GestureDoubleTap,

#if ENABLE(TOUCH_EVENTS)
        // PlatformTouchEvent
        TouchStart,
        TouchMove,
        TouchEnd,
        TouchCancel,
#endif
    };

    enum ModifierKey {
        AltKey = 1 << 0,
        CtrlKey = 1 << 1,
        MetaKey = 1 << 2,
        ShiftKey = 1 << 3
    };

    Type type() const { return static_cast<Type>(m_type); }

    bool shiftKey() const { return m_shiftKey; }
    bool ctrlKey() const { return m_ctrlKey; }
    bool altKey() const { return m_altKey; }
    bool metaKey() const { return m_metaKey; }

    unsigned modifiers() const
    {
        return (altKey() ? AltKey : 0)
            | (ctrlKey() ? CtrlKey : 0)
            | (metaKey() ? MetaKey : 0)
            | (shiftKey() ? ShiftKey : 0);
    }

    double timestamp() const { return m_timestamp; }

protected:
    PlatformEvent()
        : m_type(NoType)
        , m_shiftKey(false)
        , m_ctrlKey(false)
        , m_altKey(false)
        , m_metaKey(false)
        , m_timestamp(0)
    {
    }

    PlatformEvent(Type type)
        : m_type(type)
        , m_shiftKey(false)
        , m_ctrlKey(false)
        , m_altKey(false)
        , m_metaKey(false)
        , m_timestamp(0)
    {
    }

    PlatformEvent(Type type, bool shiftKey, bool ctrlKey, bool altKey, bool metaKey, double timestamp)
        : m_type(type)
        , m_shiftKey(shiftKey)
        , m_ctrlKey(ctrlKey)
        , m_altKey(altKey)
        , m_metaKey(metaKey)
        , m_timestamp(timestamp)
    {
    }

    // Explicit protected destructor so that people don't accidentally
    // delete a PlatformEvent.
    ~PlatformEvent()
    {
    }

    unsigned m_type;
    bool m_shiftKey;
    bool m_ctrlKey;
    bool m_altKey;
    bool m_metaKey;
    double m_timestamp;
};

} // namespace WebCore

#endif // PlatformEvent_h
