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

#include "EventTarget.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include "LayoutPoint.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class LocalFrame;

class Touch : public RefCounted<Touch> {
public:
    static Ref<Touch> create(LocalFrame* frame, EventTarget* target,
        int identifier, const FloatPoint& screenPos, const FloatPoint& pagePos,
        const FloatSize& radius, float rotationAngle, float force)
    {
        return adoptRef(
            *new Touch(frame, target, identifier, screenPos, pagePos, radius, rotationAngle, force));
    }

    EventTarget* target() const { return m_target.get(); }
    int identifier() const { return m_identifier; }
    double clientX() const { return m_clientPos.x(); }
    double clientY() const { return m_clientPos.y(); }
    double screenX() const { return m_screenPos.x(); }
    double screenY() const { return m_screenPos.y(); }
    double pageX() const { return m_pagePos.x(); }
    double pageY() const { return m_pagePos.y(); }
    double webkitRadiusX() const { return m_radius.width(); }
    double webkitRadiusY() const { return m_radius.height(); }
    float webkitRotationAngle() const { return m_rotationAngle; }
    float webkitForce() const { return m_force; }
    const LayoutPoint& absoluteLocation() const { return m_absoluteLocation; }
    Ref<Touch> cloneWithNewTarget(EventTarget*) const;

private:
    Touch(LocalFrame*, EventTarget*, int identifier,
        const FloatPoint& screenPos, const FloatPoint& pagePos,
            const FloatSize& radius, float rotationAngle, float force);

    Touch(EventTarget*, int identifier, const FloatPoint& clientPos,
        const FloatPoint& screenPos, const FloatPoint& pagePos,
        const FloatSize& radius, float rotationAngle, float force, LayoutPoint absoluteLocation);

    RefPtr<EventTarget> m_target;
    int m_identifier;
    // Position relative to the viewport in CSS px.
    FloatPoint m_clientPos;
    // Position relative to the screen in DIPs.
    FloatPoint m_screenPos;
    // Position relative to the page in CSS px.
    FloatPoint m_pagePos;
    // Radius in CSS px.
    FloatSize m_radius;
    float m_rotationAngle;
    float m_force;
    LayoutPoint m_absoluteLocation;
};

} // namespace WebCore

#endif // ENABLE(TOUCH_EVENTS)
