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

#include "FloatPoint.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"

namespace WebCore {

static FloatPoint contentsOffset(LocalFrame* frame)
{
    if (!frame)
        return FloatPoint();
    auto* frameView = frame->view();
    if (!frameView)
        return FloatPoint();
    float scale = 1.0f / frame->pageZoomFactor();
    return FloatPoint(frameView->scrollPosition()).scaledBy(scale);
}

static LayoutPoint scaledLocation(LocalFrame* frame, const FloatPoint& pagePos)
{
    if (!frame)
        return LayoutPoint(pagePos);
    float scaleFactor = frame->pageZoomFactor() * frame->frameScaleFactor();
    return LayoutPoint(pagePos.scaledBy(scaleFactor));
}

Touch::Touch(LocalFrame* frame, EventTarget* target, int identifier, const FloatPoint& screenPos, const FloatPoint& pagePos, const FloatSize& radius, float rotationAngle, float force)
    : m_target(target)
    , m_identifier(identifier)
    , m_clientPos(pagePos - contentsOffset(frame))
    , m_screenPos(screenPos)
    , m_pagePos(pagePos)
    , m_radius(radius)
    , m_rotationAngle(rotationAngle)
    , m_force(force)
    , m_absoluteLocation(scaledLocation(frame, pagePos))
{
}

Touch::Touch(EventTarget* target, int identifier, const FloatPoint& clientPos, const FloatPoint& screenPos, const FloatPoint& pagePos, const FloatSize& radius, float rotationAngle, float force, LayoutPoint absoluteLocation)
    : m_target(target)
    , m_identifier(identifier)
    , m_clientPos(clientPos)
    , m_screenPos(screenPos)
    , m_pagePos(pagePos)
    , m_radius(radius)
    , m_rotationAngle(rotationAngle)
    , m_force(force)
    , m_absoluteLocation(absoluteLocation)
{
}

Ref<Touch> Touch::cloneWithNewTarget(EventTarget* eventTarget) const
{
    return adoptRef(*new Touch(eventTarget, m_identifier, m_clientPos, m_screenPos, m_pagePos, m_radius, m_rotationAngle, m_force, m_absoluteLocation));
}

} // namespace WebCore

#endif
