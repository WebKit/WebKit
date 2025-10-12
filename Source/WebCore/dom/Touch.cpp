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

#include "config.h"

#if ENABLE(TOUCH_EVENTS)

#include "Touch.h"

#include "EventTargetInlines.h"
#include "DocumentView.h"
#include "DoublePoint.h"
#include "LocalDOMWindow.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"

namespace WebCore {

static IntPoint contentsOffset(LocalFrame* frame)
{
    if (!frame)
        return IntPoint();
    auto* frameView = frame->view();
    if (!frameView)
        return IntPoint();
    float scale = 1.0f / frame->pageZoomFactor();
    return frameView->scrollPosition().scaled(scale);
}

static DoublePoint scaledLocation(LocalFrame* frame, const DoublePoint& pagePosition)
{
    if (!frame)
        return pagePosition;
    float scaleFactor = frame->pageZoomFactor() * frame->frameScaleFactor();
    return pagePosition.scaled(scaleFactor);
}

Touch::Touch(LocalFrame* frame, EventTarget* target, int identifier, const DoublePoint& screenPosition, const DoublePoint& pagePosition, const DoubleSize& radius, float rotationAngle, double twist, float force)
    : m_target(target)
    , m_identifier(identifier)
    , m_clientPosition(DoublePoint(pagePosition - contentsOffset(frame)))
    , m_screenPosition(screenPosition)
    , m_pagePosition(pagePosition)
    , m_radius(radius)
    , m_rotationAngle(rotationAngle)
    , m_twist(twist)
    , m_force(force)
    , m_absoluteLocation(scaledLocation(frame, pagePosition))
{
}

Touch::Touch(EventTarget* target, int identifier, const DoublePoint& clientPosition, const DoublePoint& screenPosition, const DoublePoint& pagePosition, const DoubleSize& radius, float rotationAngle, double twist, float force, DoublePoint absoluteLocation)
    : m_target(target)
    , m_identifier(identifier)
    , m_clientPosition(clientPosition)
    , m_screenPosition(screenPosition)
    , m_pagePosition(pagePosition)
    , m_radius(radius)
    , m_rotationAngle(rotationAngle)
    , m_twist(twist)
    , m_force(force)
    , m_absoluteLocation(absoluteLocation)
{
}

Touch::~Touch() = default;

Ref<Touch> Touch::cloneWithNewTarget(EventTarget* eventTarget) const
{
    return adoptRef(*new Touch(eventTarget, m_identifier, m_clientPosition, m_screenPosition, m_pagePosition, m_radius, m_rotationAngle, m_twist, m_force, m_absoluteLocation));
}

} // namespace WebCore

#endif
