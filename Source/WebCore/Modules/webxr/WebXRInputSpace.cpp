/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "WebXRInputSpace.h"

#if ENABLE(WEBXR)

#include "WebXRFrame.h"
#include "WebXRRigidTransform.h"
#include "WebXRSession.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRInputSpace);

// WebXRInputSpace

Ref<WebXRInputSpace> WebXRInputSpace::create(Document& document, WebXRSession& session, const PlatformXR::FrameData::InputSourcePose& pose)
{
    return adoptRef(*new WebXRInputSpace(document, session, pose));
}

WebXRInputSpace::WebXRInputSpace(Document& document, WebXRSession& session, const PlatformXR::FrameData::InputSourcePose& pose)
    : WebXRSpace(document, WebXRRigidTransform::create())
    , m_session(session)
    , m_pose(pose)
{
}

WebXRInputSpace::~WebXRInputSpace() = default;

std::optional<TransformationMatrix> WebXRInputSpace::nativeOrigin() const
{
    return WebXRFrame::matrixFromPose(m_pose.pose);
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
