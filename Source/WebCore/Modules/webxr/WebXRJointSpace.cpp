/*
 * Copyright (C) 2021 Apple, Inc. All rights reserved.
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
#include "WebXRJointSpace.h"

#if ENABLE(WEBXR) && ENABLE(WEBXR_HANDS)

#include "WebXRFrame.h"
#include "WebXRHand.h"
#include "WebXRRigidTransform.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRJointSpace);

Ref<WebXRJointSpace> WebXRJointSpace::create(Document& document, WebXRHand& hand, XRHandJoint jointName, std::optional<PlatformXR::FrameData::InputSourceHandJoint>&& joint)
{
    return adoptRef(*new WebXRJointSpace(document, hand, jointName, WTFMove(joint)));
}

WebXRJointSpace::WebXRJointSpace(Document& document, WebXRHand& hand, XRHandJoint jointName, std::optional<PlatformXR::FrameData::InputSourceHandJoint>&& joint)
    : WebXRSpace(document, WebXRRigidTransform::create())
    , m_hand(hand)
    , m_jointName(jointName)
    , m_joint(WTFMove(joint))
{
}

WebXRJointSpace::~WebXRJointSpace() = default;

void WebXRJointSpace::updateFromJoint(const std::optional<PlatformXR::FrameData::InputSourceHandJoint>& joint)
{
    m_joint = joint;
}

bool WebXRJointSpace::handHasMissingPoses() const
{
    return !m_hand || m_hand->hasMissingPoses();
}

WebXRSession* WebXRJointSpace::session() const
{
    return m_hand ? m_hand->session() : nullptr;
}

std::optional<TransformationMatrix> WebXRJointSpace::nativeOrigin() const
{
    // https://immersive-web.github.io/webxr-hand-input/#xrjointspace-interface
    // The native origin of the XRJointSpace may only be reported when native origins of
    // all other XRJointSpaces on the same hand are being reported. When a hand is partially
    // obscured the user agent MUST either emulate the obscured joints, or report null poses
    // for all of the joints.
    if (handHasMissingPoses() || !m_joint)
        return std::nullopt;

    return WebXRFrame::matrixFromPose(m_joint->pose.pose);
}

}

#endif
