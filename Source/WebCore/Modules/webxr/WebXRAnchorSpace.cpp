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
#include "WebXRAnchorSpace.h"

#if ENABLE(WEBXR_ANCHORS)

#include "WebXRFrame.h"
#include "WebXRRigidTransform.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebXRAnchorSpace);

Ref<WebXRAnchorSpace> WebXRAnchorSpace::create(Document& document, WebXRSession& session, PlatformXR::AnchorIdentifier identifier)
{
    return adoptRef(*new WebXRAnchorSpace(document, session, identifier));
}

WebXRAnchorSpace::WebXRAnchorSpace(Document& document, WebXRSession& session, PlatformXR::AnchorIdentifier identifier)
    : WebXRSpace(document, WebXRRigidTransform::create())
    , m_session(session)
    , m_identifier(identifier)
{
}

WebXRAnchorSpace::~WebXRAnchorSpace() = default;

std::optional<TransformationMatrix> WebXRAnchorSpace::nativeOrigin() const
{
    if (!m_session)
        return std::nullopt;

    const auto& frameData = m_session->frameData();
    auto it = frameData.trackedAnchors.find(m_identifier);
    if (it == frameData.trackedAnchors.end() || it->value.trackingState != PlatformXR::AnchorTrackingState::Tracking)
        return std::nullopt;

    return WebXRFrame::matrixFromPose(it->value.pose);
}

} // namespace WebCore

#endif // ENABLE(WEBXR_ANCHORS)
