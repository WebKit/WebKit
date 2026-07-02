/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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
#include "WebXRHitTestResult.h"

#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBXR_HIT_TEST)
#include "WebXRHitTestSource.h"
#include "WebXRInputSpace.h"
#include "WebXRPose.h"
#include "WebXRRigidTransform.h"
#include "WebXRSpace.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebXRHitTestResult);

Ref<WebXRHitTestResult> WebXRHitTestResult::create(WebXRFrame& frame, WebXRSpace& space, const PlatformXR::FrameData::HitTestResult& result)
{
    return adoptRef(*new WebXRHitTestResult(frame, space, result));
}

WebXRHitTestResult::WebXRHitTestResult(WebXRFrame& frame, WebXRSpace& space, const PlatformXR::FrameData::HitTestResult& result)
    : m_frame(frame)
    , m_space(space)
    , m_result(result)
{
}

WebXRHitTestResult::~WebXRHitTestResult() = default;

// https://immersive-web.github.io/hit-test/#dom-xrhittestresult-getpose
ExceptionOr<RefPtr<WebXRPose>> WebXRHitTestResult::getPose(Document& document, const WebXRSpace& space)
{
    auto exceptionOrPose = m_frame->populatePose(document, space, m_space);
    if (exceptionOrPose.hasException())
        return exceptionOrPose.releaseException();
    auto populatedPose = exceptionOrPose.releaseReturnValue();
    if (!populatedPose || !populatedPose->transform.isInvertible())
        return nullptr;

    TransformationMatrix poseInHitTestSourceSpaceTransform;
    poseInHitTestSourceSpaceTransform.translate3d(m_result.pose.position.x(), m_result.pose.position.y(), m_result.pose.position.z());
    poseInHitTestSourceSpaceTransform.multiply(TransformationMatrix::fromQuaternion({ m_result.pose.orientation.x, m_result.pose.orientation.y, m_result.pose.orientation.z, m_result.pose.orientation.w }));

    auto poseInDestinationSpace = populatedPose->transform.inverse().value() * poseInHitTestSourceSpaceTransform;
    return RefPtr<WebXRPose>(WebXRPose::create(WebXRRigidTransform::create(poseInDestinationSpace), populatedPose->emulatedPosition));
}

} // namespace WebCore

#endif // ENABLE(WEBXR_HIT_TEST)
