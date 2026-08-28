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
#include <WebCore/DoublePoint.h>

namespace WebCore {
class RemoteFrameGeometryTransformer;
}

namespace WebKit {

#if ENABLE(TOUCH_EVENTS)
#if PLATFORM(IOS_FAMILY)

// FIXME: Having "Platform" in the name makes it sound like this event is platform-specific or
// low-level in some way. That doesn't seem to be the case.
class WebPlatformTouchPoint {
public:
    enum class State : uint8_t {
        Released,
        Pressed,
        Moved,
        Stationary,
        Cancelled
    };

    enum class TouchType : bool {
        Direct,
        Stylus
    };

    WebPlatformTouchPoint() = default;
    WebPlatformTouchPoint(unsigned identifier, WebCore::DoublePoint locationInRootView, WebCore::DoublePoint previousLocationInRootView, WebCore::DoublePoint locationInViewport, State phase)
        : m_identifier(identifier)
        , m_locationInRootView(locationInRootView)
        , m_previousLocationInRootView(previousLocationInRootView)
        , m_locationInViewport(locationInViewport)
        , m_phase(phase)
    {
    }
#if ENABLE(IOS_TOUCH_EVENTS)
    WebPlatformTouchPoint(unsigned identifier, WebCore::DoublePoint locationInRootView, WebCore::DoublePoint previousLocationInRootView, WebCore::DoublePoint locationInViewport, State phase, double radiusX, double radiusY, double rotationAngle, double twist, double force, double altitudeAngle, double azimuthAngle, TouchType touchType)
        : m_identifier(identifier)
        , m_locationInRootView(locationInRootView)
        , m_previousLocationInRootView(previousLocationInRootView)
        , m_locationInViewport(locationInViewport)
        , m_phase(phase)
        , m_radiusX(radiusX)
        , m_radiusY(radiusY)
        , m_rotationAngle(rotationAngle)
        , m_twist(twist)
        , m_force(force)
        , m_altitudeAngle(altitudeAngle)
        , m_azimuthAngle(azimuthAngle)
        , m_touchType(touchType)
    {
    }
#endif

    unsigned identifier() const { return m_identifier; }
    WebCore::DoublePoint locationInRootView() const { return m_locationInRootView; }
    WebCore::DoublePoint previousLocationInRootView() const { return m_previousLocationInRootView; }
    WebCore::DoublePoint locationInViewport() const { return m_locationInViewport; }
    State phase() const { return m_phase; }
    State state() const { return phase(); }

    void transformToRemoteFrameCoordinates(const WebCore::RemoteFrameGeometryTransformer&);

#if ENABLE(IOS_TOUCH_EVENTS)
    void setRadiusX(double radiusX) { m_radiusX = radiusX; }
    double radiusX() const { return m_radiusX; }
    void setRadiusY(double radiusY) { m_radiusY = radiusY; }
    double radiusY() const { return m_radiusY; }
    void setRotationAngle(double rotationAngle) { m_rotationAngle = rotationAngle; }
    double rotationAngle() const { return m_rotationAngle; }
    void setTwist(double twist) { m_twist = twist; }
    double twist() const { return m_twist; }
    void setForce(double force) { m_force = force; }
    double force() const { return m_force; }
    void setAltitudeAngle(double altitudeAngle) { m_altitudeAngle = altitudeAngle; }
    double altitudeAngle() const { return m_altitudeAngle; }
    void setAzimuthAngle(double azimuthAngle) { m_azimuthAngle = azimuthAngle; }
    double azimuthAngle() const { return m_azimuthAngle; }
    void setTouchType(TouchType touchType) { m_touchType = touchType; }
    TouchType touchType() const { return m_touchType; }
#endif


private:
    unsigned m_identifier { 0 };
    WebCore::DoublePoint m_locationInRootView;
    WebCore::DoublePoint m_previousLocationInRootView;
    WebCore::DoublePoint m_locationInViewport;
    State m_phase { State::Released };
#if ENABLE(IOS_TOUCH_EVENTS)
    double m_radiusX { 0 };
    double m_radiusY { 0 };
    double m_rotationAngle { 0 };
    double m_twist { std::numbers::pi };
    double m_force { 0 };
    double m_altitudeAngle { 0 };
    double m_azimuthAngle { 0 };
    TouchType m_touchType { TouchType::Direct };
#endif
};

class WebTouchEvent;

// Field order matches WebEvent.serialization.in.
struct WebTouchEventData {
    Vector<WebPlatformTouchPoint> touchPoints;
    Vector<Ref<WebTouchEvent>> coalescedEvents;
    Vector<Ref<WebTouchEvent>> predictedEvents;
    WebCore::DoublePoint position;
    bool isPotentialTap { false };
    bool isGesture { false };
    float gestureScale { 0 };
    float gestureRotation { 0 };
    bool canPreventNativeGestures { true };
};

struct WebTouchEventInit {
    WebEventData event;
    WebTouchEventData touch;
};

class WebTouchEvent : public WebEvent {
    WTF_MAKE_TZONE_ALLOCATED(WebTouchEvent);
public:
    static Ref<WebTouchEvent> create(WebEventData&&, WebTouchEventData&&);
    static Ref<WebTouchEvent> create(WebTouchEventInit&&);

    // Deep copy, including the coalesced and predicted events. Callers that mutate an event they
    // did not create must copy first, since events are now shared rather than copied by value.
    Ref<WebTouchEvent> copy() const;

    const Vector<WebPlatformTouchPoint>& touchPoints() const LIFETIME_BOUND { return m_data.touchPoints; }

    const Vector<Ref<WebTouchEvent>>& coalescedEvents() const LIFETIME_BOUND { return m_data.coalescedEvents; }
    void setCoalescedEvents(const Vector<Ref<WebTouchEvent>>& coalescedEvents) { m_data.coalescedEvents = coalescedEvents; }

    const Vector<Ref<WebTouchEvent>>& predictedEvents() const LIFETIME_BOUND { return m_data.predictedEvents; }
    void setPredictedEvents(const Vector<Ref<WebTouchEvent>>& predictedEvents) { m_data.predictedEvents = predictedEvents; }

    WebCore::DoublePoint position() const { return m_data.position; }

    void transformToRemoteFrameCoordinates(const WebCore::RemoteFrameGeometryTransformer&);

    bool isPotentialTap() const { return m_data.isPotentialTap; }

    bool isGesture() const { return m_data.isGesture; }
    float gestureScale() const { return m_data.gestureScale; }
    float gestureRotation() const { return m_data.gestureRotation; }

    bool canPreventNativeGestures() const { return m_data.canPreventNativeGestures; }
    void setCanPreventNativeGestures(bool canPreventNativeGestures) { m_data.canPreventNativeGestures = canPreventNativeGestures; }

    bool allTouchPointsAreReleased() const;

    const WebTouchEventData& touchData() const LIFETIME_BOUND { return m_data; }

protected:
    WebTouchEvent(WebEventData&&, WebTouchEventData&&);

private:
    static bool isTouchEventType(WebEventType);

    WebTouchEventData m_data;
#if ASSERT_ENABLED
    bool m_hasTransformedToRemoteFrameCoordinates { false };
#endif
};

#else // !PLATFORM(IOS_FAMILY)

class WebPlatformTouchPoint {
public:
    enum class State : uint8_t {
        Released,
        Pressed,
        Moved,
        Stationary,
        Cancelled
    };

    WebPlatformTouchPoint()
        : m_rotationAngle(0.0), m_force(0.0) { }

    WebPlatformTouchPoint(uint32_t id, State, const WebCore::DoublePoint& screenPosition, const WebCore::DoublePoint& position);

    WebPlatformTouchPoint(uint32_t id, State, const WebCore::DoublePoint& screenPosition, const WebCore::DoublePoint& position, const WebCore::DoubleSize& radius, float rotationAngle = 0.0, float force = 0.0);

    uint32_t id() const { return m_id; }
    State state() const { return m_state; }

    const WebCore::DoublePoint& screenPosition() const LIFETIME_BOUND { return m_screenPosition; }
    const WebCore::DoublePoint& position() const LIFETIME_BOUND { return m_position; }
    const WebCore::DoubleSize& radius() const LIFETIME_BOUND { return m_radius; }
    float rotationAngle() const { return m_rotationAngle; }
    float force() const { return m_force; }

    void setState(State state) { m_state = state; }

private:
    uint32_t m_id;
    State m_state;
    WebCore::DoublePoint m_screenPosition;
    WebCore::DoublePoint m_position;
    WebCore::DoubleSize m_radius;
    float m_rotationAngle;
    float m_force;
};

class WebTouchEvent;

// Field order matches WebEvent.serialization.in.
struct WebTouchEventData {
    Vector<WebPlatformTouchPoint> touchPoints;
    Vector<Ref<WebTouchEvent>> coalescedEvents;
    Vector<Ref<WebTouchEvent>> predictedEvents;
};

struct WebTouchEventInit {
    WebEventData event;
    WebTouchEventData touch;
};

class WebTouchEvent : public WebEvent {
    WTF_MAKE_TZONE_ALLOCATED(WebTouchEvent);
public:
    static Ref<WebTouchEvent> create(WebEventData&&, WebTouchEventData&&);
    static Ref<WebTouchEvent> create(WebTouchEventInit&&);

    // See the IOS_FAMILY variant.
    Ref<WebTouchEvent> copy() const;

    const Vector<WebPlatformTouchPoint>& touchPoints() const LIFETIME_BOUND { return m_data.touchPoints; }

    const Vector<Ref<WebTouchEvent>>& coalescedEvents() const LIFETIME_BOUND { return m_data.coalescedEvents; }

    const Vector<Ref<WebTouchEvent>>& predictedEvents() const LIFETIME_BOUND { return m_data.predictedEvents; }

    bool allTouchPointsAreReleased() const;

#if USE(LIBWPE) || ENABLE(WPE_PLATFORM)
    virtual bool isNativeWebTouchEvent() const { return false; }
#endif

    const WebTouchEventData& touchData() const LIFETIME_BOUND { return m_data; }

protected:
    WebTouchEvent(WebEventData&&, WebTouchEventData&&);

private:
    static bool isTouchEventType(WebEventType);

    WebTouchEventData m_data;
};

#endif // PLATFORM(IOS_FAMILY)
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebKit
