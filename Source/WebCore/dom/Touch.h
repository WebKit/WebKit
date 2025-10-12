/*
 * Copyright 2008, The Android Open Source Project
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
#include <WebKitAdditions/TouchIOS.h>
#elif ENABLE(TOUCH_EVENTS)

#include <WebCore/DoublePoint.h>
#include <WebCore/EventTarget.h>
#include <WebCore/LayoutPoint.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class LocalFrame;

class Touch : public RefCounted<Touch> {
public:
    static Ref<Touch> create(LocalFrame* frame, EventTarget* target,
        int identifier, const DoublePoint& screenPos, const DoublePoint& pagePos,
        const DoubleSize& radius, float rotationAngle, float force, double twist = 0)
    {
        return adoptRef(
            *new Touch(frame, target, identifier, screenPos, pagePos, radius, rotationAngle, twist, force));
    }

    ~Touch();

    EventTarget* target() const { return m_target.get(); }
    int identifier() const { return m_identifier; }
    double clientX() const { return m_clientPosition.x(); }
    double clientY() const { return m_clientPosition.y(); }
    double screenX() const { return m_screenPosition.x(); }
    double screenY() const { return m_screenPosition.y(); }
    double pageX() const { return m_pagePosition.x(); }
    double pageY() const { return m_pagePosition.y(); }
    double webkitRadiusX() const { return m_radius.width(); }
    double webkitRadiusY() const { return m_radius.height(); }
    float webkitRotationAngle() const { return m_rotationAngle; }
    double twist() const { return m_twist; }
    float webkitForce() const { return m_force; }
    const DoublePoint& absoluteLocation() const { return m_absoluteLocation; }
    Ref<Touch> cloneWithNewTarget(EventTarget*) const;

private:
    Touch(LocalFrame*, EventTarget*, int identifier,
        const DoublePoint& screenPosition, const DoublePoint& pagePosition,
        const DoubleSize& radius, float rotationAngle, double twist, float force);

    Touch(EventTarget*, int identifier, const DoublePoint& clientPosition,
        const DoublePoint& screenPosition, const DoublePoint& pagePosition,
        const DoubleSize& radius, float rotationAngle, double twist, float force, DoublePoint absoluteLocation);

    RefPtr<EventTarget> m_target;
    int m_identifier;
    // Position relative to the viewport in CSS px.
    DoublePoint m_clientPosition;
    // Position relative to the screen in DIPs.
    DoublePoint m_screenPosition;
    // Position relative to the page in CSS px.
    DoublePoint m_pagePosition;
    // Radius in CSS px.
    DoubleSize m_radius;
    float m_rotationAngle;
    // Twist is stored so that it can be exposed for pointer events.
    double m_twist;
    float m_force;
    DoublePoint m_absoluteLocation;
};

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)
