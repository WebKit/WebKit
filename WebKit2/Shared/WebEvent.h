/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebEvent_h
#define WebEvent_h

// FIXME: We should probably move to makeing the WebCore/PlatformFooEvents trivial classes so that
// we can use them as the event type.

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "WebCoreArgumentCoders.h"
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebEvent {
public:
    enum Type {
        // WebMouseEvent
        MouseDown,
        MouseUp,
        MouseMove,

        // WebWheelEvent
        Wheel,

        // WebKeyboardEvent
        KeyDown,
        KeyUp,
        RawKeyDown,
        Char
#if ENABLE(TOUCH_EVENTS)
        ,
        TouchStart,
        TouchMove,
        TouchEnd,
        TouchCancel
#endif
    };

    enum Modifiers {
        ShiftKey    = 1 << 0,
        ControlKey  = 1 << 1,
        AltKey      = 1 << 2,
        MetaKey     = 1 << 3,
    };

    Type type() const { return (Type)m_type; }

    bool shiftKey() const { return m_modifiers & ShiftKey; }
    bool controlKey() const { return m_modifiers & ControlKey; }
    bool altKey() const { return m_modifiers & AltKey; }
    bool metaKey() const { return m_modifiers & MetaKey; }

    double timestamp() const { return m_timestamp; }

protected:
    WebEvent()
    {
    }

    WebEvent(Type type, Modifiers modifiers, double timestamp)
        : m_type(type)
        , m_modifiers(modifiers)
        , m_timestamp(timestamp)
    {
    }

    void encode(CoreIPC::ArgumentEncoder* encoder) const
    {
        encoder->encode(m_type);
        encoder->encode(m_modifiers);
        encoder->encode(m_timestamp);
    }

    static bool decode(CoreIPC::ArgumentDecoder* decoder, WebEvent& t)
    {
        if (!decoder->decode(t.m_type))
            return false;
        if (!decoder->decode(t.m_modifiers))
            return false;
        if (!decoder->decode(t.m_timestamp))
            return false;

        return true;
    }

private:
    uint32_t m_type; // Type
    uint32_t m_modifiers; // Modifiers
    double m_timestamp;
};

class WebMouseEvent : public WebEvent {
public:
    enum Button {
        NoButton = -1,
        LeftButton,
        MiddleButton,
        RightButton
    };

    WebMouseEvent()
    {
    }

    WebMouseEvent(Type type, Button button, int x, int y, int globalX, int globalY, float deltaX, float deltaY, float deltaZ, int clickCount, Modifiers modifiers, double timestamp)
        : WebEvent(type, modifiers, timestamp)
        , m_button(button)
        , m_positionX(x)
        , m_positionY(y)
        , m_globalPositionX(globalX)
        , m_globalPositionY(globalY)
        , m_deltaX(deltaX)
        , m_deltaY(deltaY)
        , m_deltaZ(deltaZ)
        , m_clickCount(clickCount)
    {
        ASSERT(isMouseEventType(type));
    }

    Button button() const { return m_button; }
    int positionX() const { return m_positionX; }
    int positionY() const { return m_positionY; }
    int globalPositionX() const { return m_globalPositionX; }
    int globalPositionY() const { return m_globalPositionY; }
    float deltaX() const { return m_deltaX; }
    float deltaY() const { return m_deltaY; }
    float deltaZ() const { return m_deltaZ; }
    int clickCount() const { return m_clickCount; }

    void encode(CoreIPC::ArgumentEncoder* encoder) const
    {
        encoder->encodeBytes(reinterpret_cast<const uint8_t*>(this), sizeof(*this));
    }

    static bool decode(CoreIPC::ArgumentDecoder* decoder, WebMouseEvent& t)
    {
        return decoder->decodeBytes(reinterpret_cast<uint8_t*>(&t), sizeof(t));
    }

private:
    static bool isMouseEventType(Type type)
    {
        return type == MouseDown || type == MouseUp || type == MouseMove;
    }

    Button m_button;
    int m_positionX;
    int m_positionY;
    int m_globalPositionX;
    int m_globalPositionY;
    float m_deltaX;
    float m_deltaY;
    float m_deltaZ;
    int m_clickCount;
};

class WebWheelEvent : public WebEvent {
public:
    enum Granularity {
        ScrollByPageWheelEvent,
        ScrollByPixelWheelEvent
    };

    WebWheelEvent()
    {
    }

    WebWheelEvent(Type type, int x, int y, int globalX, int globalY, float deltaX, float deltaY, float wheelTicksX, float wheelTicksY, Granularity granularity, Modifiers modifiers, double timestamp)
        : WebEvent(type, modifiers, timestamp)
        , m_positionX(x)
        , m_positionY(y)
        , m_globalPositionX(globalX)
        , m_globalPositionY(globalY)
        , m_deltaX(deltaX)
        , m_deltaY(deltaY)
        , m_wheelTicksX(wheelTicksX)
        , m_wheelTicksY(wheelTicksY)
        , m_granularity(granularity)
    {
        ASSERT(isWheelEventType(type));
    }

    int positionX() const { return m_positionX; }
    int positionY() const { return m_positionY; }
    int globalPositionX() const { return m_globalPositionX; }
    int globalPositionY() const { return m_globalPositionY; }
    float deltaX() const { return m_deltaX; }
    float deltaY() const { return m_deltaY; }
    float wheelTicksX() const { return m_wheelTicksX; }
    float wheelTicksY() const { return m_wheelTicksY; }
    Granularity granularity() const { return (Granularity)m_granularity; }

    void encode(CoreIPC::ArgumentEncoder* encoder) const
    {
        encoder->encodeBytes(reinterpret_cast<const uint8_t*>(this), sizeof(*this));
    }

    static bool decode(CoreIPC::ArgumentDecoder* decoder, WebWheelEvent& t)
    {
        return decoder->decodeBytes(reinterpret_cast<uint8_t*>(&t), sizeof(t));
    }

private:
    static bool isWheelEventType(Type type)
    {
        return type == Wheel;
    }

    int m_positionX;
    int m_positionY;
    int m_globalPositionX;
    int m_globalPositionY;
    float m_deltaX;
    float m_deltaY;
    float m_wheelTicksX;
    float m_wheelTicksY;
    unsigned m_granularity; // Granularity
};

class WebKeyboardEvent : public WebEvent {
public:
    WebKeyboardEvent()
    {
    }

    WebKeyboardEvent(Type type, const String& text, const String& unmodifiedText, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, bool isAutoRepeat, bool isKeypad, bool isSystemKey, Modifiers modifiers, double timestamp)
        : WebEvent(type, modifiers, timestamp)
        , m_text(text)
        , m_unmodifiedText(unmodifiedText)
        , m_keyIdentifier(keyIdentifier)
        , m_windowsVirtualKeyCode(windowsVirtualKeyCode)
        , m_nativeVirtualKeyCode(nativeVirtualKeyCode)
        , m_isAutoRepeat(isAutoRepeat)
        , m_isKeypad(isKeypad)
        , m_isSystemKey(isSystemKey)
    {
        ASSERT(isKeyboardEventType(type));
    }

    const String& text() const { return m_text; }
    const String& unmodifiedText() const { return m_unmodifiedText; }
    const String& keyIdentifier() const { return m_keyIdentifier; }
    int32_t windowsVirtualKeyCode() const { return m_windowsVirtualKeyCode; }
    int32_t nativeVirtualKeyCode() const { return m_nativeVirtualKeyCode; }
    bool isAutoRepeat() const { return m_isAutoRepeat; }
    bool isKeypad() const { return m_isKeypad; }
    bool isSystemKey() const { return m_isSystemKey; }

    void encode(CoreIPC::ArgumentEncoder* encoder) const
    {
        WebEvent::encode(encoder);

        encoder->encode(m_text);
        encoder->encode(m_unmodifiedText);
        encoder->encode(m_keyIdentifier);
        encoder->encode(m_windowsVirtualKeyCode);
        encoder->encode(m_nativeVirtualKeyCode);
        encoder->encode(m_isAutoRepeat);
        encoder->encode(m_isKeypad);
        encoder->encode(m_isSystemKey);
    }

    static bool decode(CoreIPC::ArgumentDecoder* decoder, WebKeyboardEvent& t)
    {
        if (!WebEvent::decode(decoder, t))
            return false;

        String text;
        if (!decoder->decode(text))
            return false;
        t.m_text = text;

        String unmodifiedText;
        if (!decoder->decode(unmodifiedText))
            return false;
        t.m_unmodifiedText = unmodifiedText;

        String keyIdentifier;
        if (!decoder->decode(keyIdentifier))
            return false;
        t.m_keyIdentifier = keyIdentifier;

        if (!decoder->decode(t.m_windowsVirtualKeyCode))
            return false;
        if (!decoder->decode(t.m_nativeVirtualKeyCode))
            return false;
        if (!decoder->decode(t.m_isAutoRepeat))
            return false;
        if (!decoder->decode(t.m_isKeypad))
            return false;
        if (!decoder->decode(t.m_isSystemKey))
            return false;
        return true;
    }

    static bool isKeyboardEventType(Type type)
    {
        return type == RawKeyDown || type == KeyDown || type == KeyUp || type == Char;
    }

private:
    String m_text;
    String m_unmodifiedText;
    String m_keyIdentifier;
    int32_t m_windowsVirtualKeyCode;
    int32_t m_nativeVirtualKeyCode;
    bool m_isAutoRepeat;
    bool m_isKeypad;
    bool m_isSystemKey;
};

#if ENABLE(TOUCH_EVENTS)

class WebPlatformTouchPoint {
public:
    enum TouchPointState {
        TouchReleased,
        TouchPressed,
        TouchMoved,
        TouchStationary,
        TouchCancelled
    };

    WebPlatformTouchPoint()
    {
    }

    WebPlatformTouchPoint(unsigned id, TouchPointState state, int screenPosX, int screenPosY, int posX, int posY)
        : m_id(id)
        , m_state(state)
        , m_screenPosX(screenPosX)
        , m_screenPosY(screenPosY)
        , m_posX(posX)
        , m_posY(posY)
    {
    }

    unsigned id() const { return m_id; }
    TouchPointState state() const { return m_state; }

    int screenPosX() const { return m_screenPosX; }
    int screenPosY() const { return m_screenPosY; }
    int32_t posX() const { return m_posX; }
    int32_t posY() const { return m_posY; }
          
    void setState(TouchPointState state) { m_state = state; }

    void encode(CoreIPC::ArgumentEncoder* encoder) const
    {
        encoder->encodeBytes(reinterpret_cast<const uint8_t*>(this), sizeof(*this));
    }

    static bool decode(CoreIPC::ArgumentDecoder* decoder, WebPlatformTouchPoint& t)
    {
        return decoder->decodeBytes(reinterpret_cast<uint8_t*>(&t), sizeof(t));
    }

private:
    unsigned m_id;
    TouchPointState m_state;
    int m_screenPosX, m_screenPosY;
    int32_t m_posX, m_posY;

};

class WebTouchEvent : public WebEvent {
public:

    WebTouchEvent()
    {
    }
 
    WebTouchEvent(WebEvent::Type type, Vector<WebPlatformTouchPoint> touchPoints, bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, Modifiers modifiers, double timestamp)
        : WebEvent(type, modifiers, timestamp)
        , m_type(type)
        , m_touchPoints(touchPoints)
        , m_ctrlKey(ctrlKey)
        , m_altKey(altKey)
        , m_shiftKey(shiftKey)
        , m_metaKey(metaKey)
    {
        ASSERT(isTouchEventType(type));
    }

    const Vector<WebPlatformTouchPoint> touchPoints() const { return m_touchPoints; }

    void encode(CoreIPC::ArgumentEncoder* encoder) const
    {
        WebEvent::encode(encoder);
        encoder->encode(m_touchPoints);
    }

    static bool decode(CoreIPC::ArgumentDecoder* decoder, WebTouchEvent& t)
    {
        if (!WebEvent::decode(decoder, t))
            return false;

        if (!decoder->decode(t.m_touchPoints))
             return false;

        return true;
    }
  
private:
    static bool isTouchEventType(Type type)
    {
        return type == TouchStart || type == TouchMove || type == TouchEnd;
    }

    Type m_type;
    Vector<WebPlatformTouchPoint> m_touchPoints;
    bool m_ctrlKey;
    bool m_altKey;
    bool m_shiftKey;
    bool m_metaKey;
    double m_timestamp;
};
#endif

} // namespace WebKit

#endif // WebEvent_h
