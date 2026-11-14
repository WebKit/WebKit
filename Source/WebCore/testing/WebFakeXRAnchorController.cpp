/*
 * Copyright (C) 2026 Igalia, S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in source code must reproduce the above copyright
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

#include "config.h"
#include "WebFakeXRAnchorController.h"

#if ENABLE(WEBXR) && ENABLE(WEBXR_ANCHORS)

#include "WebFakeXRDevice.h"

namespace WebCore {

Ref<WebFakeXRAnchorController> WebFakeXRAnchorController::create(const PlatformXR::FrameData::Pose& origin)
{
    return adoptRef(*new WebFakeXRAnchorController(origin));
}

WebFakeXRAnchorController::WebFakeXRAnchorController(const PlatformXR::FrameData::Pose& origin)
    : m_origin(origin)
{
}

bool WebFakeXRAnchorController::isIgnoringChanges() const
{
    return m_deleted || m_trackingState == PlatformXR::AnchorTrackingState::Stopped;
}

void WebFakeXRAnchorController::pauseTracking()
{
    if (isIgnoringChanges())
        return;
    if (m_trackingState == PlatformXR::AnchorTrackingState::Tracking)
        m_trackingState = PlatformXR::AnchorTrackingState::Paused;
}

void WebFakeXRAnchorController::resumeTracking()
{
    if (isIgnoringChanges())
        return;
    if (m_trackingState == PlatformXR::AnchorTrackingState::Paused)
        m_trackingState = PlatformXR::AnchorTrackingState::Tracking;
}

void WebFakeXRAnchorController::stopTracking()
{
    if (!m_deleted)
        m_trackingState = PlatformXR::AnchorTrackingState::Stopped;
}

ExceptionOr<void> WebFakeXRAnchorController::setAnchorOrigin(FakeXRRigidTransformInit origin)
{
    if (isIgnoringChanges())
        return { };

    auto pose = WebFakeXRDevice::parseRigidTransform(origin);
    if (pose.hasException())
        return pose.releaseException();
    m_origin = pose.releaseReturnValue();
    return { };
}

} // namespace WebCore

#endif // ENABLE(WEBXR) && ENABLE(WEBXR_ANCHORS)
