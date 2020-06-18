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

// FIXME: We should probably move to makeing the WebCore/PlatformFooEvents trivial classes so that
// we can use them as the event type.

#include "EditingRange.h"
#include <WebCore/CompositionUnderline.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/FloatSize.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntSize.h>
#include <WebCore/KeypressCommand.h>
#include <wtf/EnumTraits.h>
#include <wtf/OptionSet.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Decoder;
class Encoder;
}

#if USE(APPKIT)
namespace WebCore {
struct KeypressCommand;
}
#endif

namespace WebKit {

class WebEvent {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum Type {
        NoType = -1,
        
        // WebMouseEvent
        MouseDown,
        MouseUp,
        MouseMove,
        MouseForceChanged,
        MouseForceDown,
        MouseForceUp,

        // WebWheelEvent
        Wheel,

        // WebKeyboardEvent
        KeyDown,
        KeyUp,
        RawKeyDown,
        Char,

#if ENABLE(TOUCH_EVENTS)
        // WebTouchEvent
        TouchStart,
        TouchMove,
        TouchEnd,
        TouchCancel,
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
        GestureStart,
        GestureChange,
        GestureEnd,
#endif
    };

    enum class Modifier : uint8_t {
        ShiftKey    = 1 << 0,
        ControlKey  = 1 << 1,
        AltKey      = 1 << 2,
        MetaKey     = 1 << 3,
        CapsLockKey = 1 << 4,
    };

    Type type() const { return static_cast<Type>(m_type); }

    bool shiftKey() const { return m_modifiers.contains(Modifier::ShiftKey); }
    bool controlKey() const { return m_modifiers.contains(Modifier::ControlKey); }
    bool altKey() const { return m_modifiers.contains(Modifier::AltKey); }
    bool metaKey() const { return m_modifiers.contains(Modifier::MetaKey); }
    bool capsLockKey() const { return m_modifiers.contains(Modifier::CapsLockKey); }

    OptionSet<Modifier> modifiers() const { return m_modifiers; }

    WallTime timestamp() const { return m_timestamp; }

protected:
    WebEvent();

    WebEvent(Type, OptionSet<Modifier>, WallTime timestamp);

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, WebEvent&);

private:
    uint32_t m_type;
    OptionSet<Modifier> m_modifiers;
    WallTime m_timestamp;
};

// FIXME: Move this class to its own header file.
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
    WebMouseEvent(Type, Button, unsigned short buttons, const WebCore::IntPoint& positionInView, const WebCore::IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, OptionSet<Modifier>, WallTime timestamp, double force, SyntheticClickType = NoTap, int eventNumber = -1, int menuType = 0);
#else
    WebMouseEvent(Type, Button, unsigned short buttons, const WebCore::IntPoint& positionInView, const WebCore::IntPoint& globalPosition, float deltaX, float deltaY, float deltaZ, int clickCount, OptionSet<Modifier>, WallTime timestamp, double force = 0, SyntheticClickType = NoTap);
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
};

// FIXME: Move this class to its own header file.
class WebWheelEvent : public WebEvent {
public:
    enum Granularity {
        ScrollByPageWheelEvent,
        ScrollByPixelWheelEvent
    };

    enum Phase {
        PhaseNone        = 0,
        PhaseBegan       = 1 << 0,
        PhaseStationary  = 1 << 1,
        PhaseChanged     = 1 << 2,
        PhaseEnded       = 1 << 3,
        PhaseCancelled   = 1 << 4,
        PhaseMayBegin    = 1 << 5,
    };

    WebWheelEvent() = default;

    WebWheelEvent(Type, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, Granularity, OptionSet<Modifier>, WallTime timestamp);
#if PLATFORM(COCOA)
    WebWheelEvent(Type, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, Granularity, bool directionInvertedFromDevice, Phase, Phase momentumPhase, bool hasPreciseScrollingDeltas, uint32_t scrollCount, const WebCore::FloatSize& unacceleratedScrollingDelta, OptionSet<Modifier>, WallTime timestamp);
#elif PLATFORM(GTK) || USE(LIBWPE)
    WebWheelEvent(Type, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, Phase, Phase momentumPhase, Granularity, OptionSet<Modifier>, WallTime timestamp);
#endif

    const WebCore::IntPoint position() const { return m_position; }
    const WebCore::IntPoint globalPosition() const { return m_globalPosition; }
    const WebCore::FloatSize delta() const { return m_delta; }
    const WebCore::FloatSize wheelTicks() const { return m_wheelTicks; }
    Granularity granularity() const { return static_cast<Granularity>(m_granularity); }
    bool directionInvertedFromDevice() const { return m_directionInvertedFromDevice; }
    Phase phase() const { return static_cast<Phase>(m_phase); }
    Phase momentumPhase() const { return static_cast<Phase>(m_momentumPhase); }
#if PLATFORM(COCOA)
    bool hasPreciseScrollingDeltas() const { return m_hasPreciseScrollingDeltas; }
    uint32_t scrollCount() const { return m_scrollCount; }
    const WebCore::FloatSize& unacceleratedScrollingDelta() const { return m_unacceleratedScrollingDelta; }
#endif

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, WebWheelEvent&);

private:
    static bool isWheelEventType(Type);

    WebCore::IntPoint m_position;
    WebCore::IntPoint m_globalPosition;
    WebCore::FloatSize m_delta;
    WebCore::FloatSize m_wheelTicks;
    uint32_t m_granularity { ScrollByPageWheelEvent };
    bool m_directionInvertedFromDevice { false };
    uint32_t m_phase { Phase::PhaseNone };
    uint32_t m_momentumPhase { Phase::PhaseNone };
#if PLATFORM(COCOA)
    bool m_hasPreciseScrollingDeltas { false };
    uint32_t m_scrollCount { 0 };
    WebCore::FloatSize m_unacceleratedScrollingDelta;
#endif
};

// FIXME: Move this class to its own header file.
class WebKeyboardEvent : public WebEvent {
public:
    WebKeyboardEvent();
    ~WebKeyboardEvent();

#if USE(APPKIT)
    WebKeyboardEvent(Type, const String& text, const String& unmodifiedText, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, int macCharCode, bool handledByInputMethod, const Vector<WebCore::KeypressCommand>&, bool isAutoRepeat, bool isKeypad, bool isSystemKey, OptionSet<Modifier>, WallTime timestamp);
#elif PLATFORM(GTK)
    WebKeyboardEvent(Type, const String& text, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, bool handledByInputMethod, Optional<Vector<WebCore::CompositionUnderline>>&&, Optional<EditingRange>&&, Vector<String>&& commands, bool isKeypad, OptionSet<Modifier>, WallTime timestamp);
#elif PLATFORM(IOS_FAMILY)
    WebKeyboardEvent(Type, const String& text, const String& unmodifiedText, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, int macCharCode, bool handledByInputMethod, bool isAutoRepeat, bool isKeypad, bool isSystemKey, OptionSet<Modifier>, WallTime timestamp);
#elif USE(LIBWPE)
    WebKeyboardEvent(Type, const String& text, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, bool handledByInputMethod, Optional<Vector<WebCore::CompositionUnderline>>&&, Optional<EditingRange>&&, bool isKeypad, OptionSet<Modifier>, WallTime timestamp);
#else
    WebKeyboardEvent(Type, const String& text, const String& unmodifiedText, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, int macCharCode, bool isAutoRepeat, bool isKeypad, bool isSystemKey, OptionSet<Modifier>, WallTime timestamp);
#endif

    const String& text() const { return m_text; }
    const String& unmodifiedText() const { return m_unmodifiedText; }
    const String& key() const { return m_key; }
    const String& code() const { return m_code; }
    const String& keyIdentifier() const { return m_keyIdentifier; }
    int32_t windowsVirtualKeyCode() const { return m_windowsVirtualKeyCode; }
    int32_t nativeVirtualKeyCode() const { return m_nativeVirtualKeyCode; }
    int32_t macCharCode() const { return m_macCharCode; }
#if USE(APPKIT) || USE(UIKIT_KEYBOARD_ADDITIONS) || PLATFORM(GTK) || USE(LIBWPE)
    bool handledByInputMethod() const { return m_handledByInputMethod; }
#endif
#if PLATFORM(GTK) || USE(LIBWPE)
    const Optional<Vector<WebCore::CompositionUnderline>>& preeditUnderlines() const { return m_preeditUnderlines; }
    const Optional<EditingRange>& preeditSelectionRange() const { return m_preeditSelectionRange; }
#endif
#if USE(APPKIT)
    const Vector<WebCore::KeypressCommand>& commands() const { return m_commands; }
#elif PLATFORM(GTK)
    const Vector<String>& commands() const { return m_commands; }
#endif
    bool isAutoRepeat() const { return m_isAutoRepeat; }
    bool isKeypad() const { return m_isKeypad; }
    bool isSystemKey() const { return m_isSystemKey; }

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, WebKeyboardEvent&);

    static bool isKeyboardEventType(Type);

private:
    String m_text;
    String m_unmodifiedText;
    String m_key;
    String m_code;
    String m_keyIdentifier;
    int32_t m_windowsVirtualKeyCode { 0 };
    int32_t m_nativeVirtualKeyCode { 0 };
    int32_t m_macCharCode { 0 };
#if USE(APPKIT) || USE(UIKIT_KEYBOARD_ADDITIONS) || PLATFORM(GTK) || USE(LIBWPE)
    bool m_handledByInputMethod { false };
#endif
#if PLATFORM(GTK) || USE(LIBWPE)
    Optional<Vector<WebCore::CompositionUnderline>> m_preeditUnderlines;
    Optional<EditingRange> m_preeditSelectionRange;
#endif
#if USE(APPKIT)
    Vector<WebCore::KeypressCommand> m_commands;
#elif PLATFORM(GTK)
    Vector<String> m_commands;
#endif
    bool m_isAutoRepeat { false };
    bool m_isKeypad { false };
    bool m_isSystemKey { false };
};

#if ENABLE(TOUCH_EVENTS)
#if PLATFORM(IOS_FAMILY)
class WebPlatformTouchPoint {
public:
    enum TouchPointState {
        TouchReleased,
        TouchPressed,
        TouchMoved,
        TouchStationary,
        TouchCancelled
    };

    enum class TouchType {
        Direct,
        Stylus
    };

    WebPlatformTouchPoint() = default;
    WebPlatformTouchPoint(unsigned identifier, WebCore::IntPoint location, TouchPointState phase)
        : m_identifier(identifier)
        , m_location(location)
        , m_phase(phase)
    {
    }

    unsigned identifier() const { return m_identifier; }
    WebCore::IntPoint location() const { return m_location; }
    TouchPointState phase() const { return static_cast<TouchPointState>(m_phase); }
    TouchPointState state() const { return phase(); }

#if ENABLE(IOS_TOUCH_EVENTS)
    void setRadiusX(double radiusX) { m_radiusX = radiusX; }
    double radiusX() const { return m_radiusX; }
    void setRadiusY(double radiusY) { m_radiusY = radiusY; }
    double radiusY() const { return m_radiusY; }
    void setRotationAngle(double rotationAngle) { m_rotationAngle = rotationAngle; }
    double rotationAngle() const { return m_rotationAngle; }
    void setForce(double force) { m_force = force; }
    double force() const { return m_force; }
    void setAltitudeAngle(double altitudeAngle) { m_altitudeAngle = altitudeAngle; }
    double altitudeAngle() const { return m_altitudeAngle; }
    void setAzimuthAngle(double azimuthAngle) { m_azimuthAngle = azimuthAngle; }
    double azimuthAngle() const { return m_azimuthAngle; }
    void setTouchType(TouchType touchType) { m_touchType = static_cast<uint32_t>(touchType); }
    TouchType touchType() const { return static_cast<TouchType>(m_touchType); }
#endif

    void encode(IPC::Encoder&) const;
    static Optional<WebPlatformTouchPoint> decode(IPC::Decoder&);

private:
    unsigned m_identifier { 0 };
    WebCore::IntPoint m_location;
    uint32_t m_phase { TouchReleased };
#if ENABLE(IOS_TOUCH_EVENTS)
    double m_radiusX { 0 };
    double m_radiusY { 0 };
    double m_rotationAngle { 0 };
    double m_force { 0 };
    double m_altitudeAngle { 0 };
    double m_azimuthAngle { 0 };
    uint32_t m_touchType { static_cast<uint32_t>(TouchType::Direct) };
#endif
};

class WebTouchEvent : public WebEvent {
public:
    WebTouchEvent() = default;
    WebTouchEvent(WebEvent::Type type, OptionSet<Modifier> modifiers, WallTime timestamp, const Vector<WebPlatformTouchPoint>& touchPoints, WebCore::IntPoint position, bool isPotentialTap, bool isGesture, float gestureScale, float gestureRotation)
        : WebEvent(type, modifiers, timestamp)
        , m_touchPoints(touchPoints)
        , m_position(position)
        , m_canPreventNativeGestures(true)
        , m_isPotentialTap(isPotentialTap)
        , m_isGesture(isGesture)
        , m_gestureScale(gestureScale)
        , m_gestureRotation(gestureRotation)
    {
        ASSERT(type == TouchStart || type == TouchMove || type == TouchEnd || type == TouchCancel);
    }

    const Vector<WebPlatformTouchPoint>& touchPoints() const { return m_touchPoints; }

    WebCore::IntPoint position() const { return m_position; }

    bool isPotentialTap() const { return m_isPotentialTap; }

    bool isGesture() const { return m_isGesture; }
    float gestureScale() const { return m_gestureScale; }
    float gestureRotation() const { return m_gestureRotation; }

    bool canPreventNativeGestures() const { return m_canPreventNativeGestures; }
    void setCanPreventNativeGestures(bool canPreventNativeGestures) { m_canPreventNativeGestures = canPreventNativeGestures; }

    bool allTouchPointsAreReleased() const;

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, WebTouchEvent&);
    
private:
    Vector<WebPlatformTouchPoint> m_touchPoints;
    
    WebCore::IntPoint m_position;
    bool m_canPreventNativeGestures { false };
    bool m_isPotentialTap { false };
    bool m_isGesture { false };
    float m_gestureScale { 0 };
    float m_gestureRotation { 0 };
};
#else
// FIXME: Move this class to its own header file.
// FIXME: Having "Platform" in the name makes it sound like this event is platform-specific or low-
// level in some way. That doesn't seem to be the case.
class WebPlatformTouchPoint {
public:
    enum TouchPointState {
        TouchReleased,
        TouchPressed,
        TouchMoved,
        TouchStationary,
        TouchCancelled
    };

    WebPlatformTouchPoint() : m_rotationAngle(0.0), m_force(0.0) { }

    WebPlatformTouchPoint(uint32_t id, TouchPointState, const WebCore::IntPoint& screenPosition, const WebCore::IntPoint& position);

    WebPlatformTouchPoint(uint32_t id, TouchPointState, const WebCore::IntPoint& screenPosition, const WebCore::IntPoint& position, const WebCore::IntSize& radius, float rotationAngle = 0.0, float force = 0.0);
    
    uint32_t id() const { return m_id; }
    TouchPointState state() const { return static_cast<TouchPointState>(m_state); }

    const WebCore::IntPoint& screenPosition() const { return m_screenPosition; }
    const WebCore::IntPoint& position() const { return m_position; }
    const WebCore::IntSize& radius() const { return m_radius; }
    float rotationAngle() const { return m_rotationAngle; }
    float force() const { return m_force; }

    void setState(TouchPointState state) { m_state = state; }

    void encode(IPC::Encoder&) const;
    static Optional<WebPlatformTouchPoint> decode(IPC::Decoder&);

private:
    uint32_t m_id;
    uint32_t m_state;
    WebCore::IntPoint m_screenPosition;
    WebCore::IntPoint m_position;
    WebCore::IntSize m_radius;
    float m_rotationAngle;
    float m_force;
};

// FIXME: Move this class to its own header file.
class WebTouchEvent : public WebEvent {
public:
    WebTouchEvent() { }
    WebTouchEvent(Type, Vector<WebPlatformTouchPoint>&&, OptionSet<Modifier>, WallTime timestamp);

    const Vector<WebPlatformTouchPoint>& touchPoints() const { return m_touchPoints; }

    bool allTouchPointsAreReleased() const;

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, WebTouchEvent&);
  
private:
    static bool isTouchEventType(Type);

    Vector<WebPlatformTouchPoint> m_touchPoints;
};

#endif // PLATFORM(IOS_FAMILY)
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::WebEvent::Modifier> {
    using values = EnumValues<
        WebKit::WebEvent::Modifier,
        WebKit::WebEvent::Modifier::ShiftKey,
        WebKit::WebEvent::Modifier::ControlKey,
        WebKit::WebEvent::Modifier::AltKey,
        WebKit::WebEvent::Modifier::MetaKey,
        WebKit::WebEvent::Modifier::CapsLockKey
    >;
};

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
