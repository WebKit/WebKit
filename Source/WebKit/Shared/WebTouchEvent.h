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

#include "WebEvent.h"
#include <WebCore/IntPoint.h>

namespace WebKit {

#if ENABLE(TOUCH_EVENTS)
#if PLATFORM(IOS_FAMILY)

// FIXME: Having "Platform" in the name makes it sound like this event is platform-specific or
// low-level in some way. That doesn't seem to be the case.
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
    static std::optional<WebPlatformTouchPoint> decode(IPC::Decoder&);

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
    WebTouchEvent(WebEvent&& event, const Vector<WebPlatformTouchPoint>& touchPoints, WebCore::IntPoint position, bool isPotentialTap, bool isGesture, float gestureScale, float gestureRotation, bool canPreventNativeGestures = true)
        : WebEvent(WTFMove(event))
        , m_touchPoints(touchPoints)
        , m_position(position)
        , m_canPreventNativeGestures(canPreventNativeGestures)
        , m_isPotentialTap(isPotentialTap)
        , m_isGesture(isGesture)
        , m_gestureScale(gestureScale)
        , m_gestureRotation(gestureRotation)
    {
        ASSERT(type() == TouchStart || type() == TouchMove || type() == TouchEnd || type() == TouchCancel);
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

#else // !PLATFORM(IOS_FAMILY)

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
        : m_rotationAngle(0.0), m_force(0.0) { }

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
    static std::optional<WebPlatformTouchPoint> decode(IPC::Decoder&);

private:
    uint32_t m_id;
    uint32_t m_state;
    WebCore::IntPoint m_screenPosition;
    WebCore::IntPoint m_position;
    WebCore::IntSize m_radius;
    float m_rotationAngle;
    float m_force;
};

class WebTouchEvent : public WebEvent {
public:
    WebTouchEvent() { }
    WebTouchEvent(WebEvent&&, Vector<WebPlatformTouchPoint>&&);

    const Vector<WebPlatformTouchPoint>& touchPoints() const { return m_touchPoints; }

    bool allTouchPointsAreReleased() const;

private:
    static bool isTouchEventType(Type);

    Vector<WebPlatformTouchPoint> m_touchPoints;
};

#endif // PLATFORM(IOS_FAMILY)
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebKit
