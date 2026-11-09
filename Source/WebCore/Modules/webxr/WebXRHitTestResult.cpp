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
#if ENABLE(WEBXR_ANCHORS)
#include "JSDOMConvertInterface.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebXRAnchor.h"
#include "WebXRAnchor.h"
#include "WebXRFrame.h"
#endif

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

#if ENABLE(WEBXR_ANCHORS)

// https://immersive-web.github.io/anchors/#dom-xrhittestresult-createanchor
ExceptionOr<void> WebXRHitTestResult::createAnchor(Document& document, CreateAnchorPromise&& promise)
{
    if (!m_frame->session().supportsFeature(PlatformXR::SessionFeature::Anchors)) {
        promise.reject(Exception { ExceptionCode::NotSupportedError, "The anchors feature was not requested for this session"_s });
        return { };
    }

    if (m_frame->session().ended()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "The session was already ended"_s });
        return { };
    }

    RefPtr device = m_frame->session().device();
    if (!device) {
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return { };
    }

    // Hit test result pose is in hit test source space. Convert to local space (same as XRFrame.createAnchor).
    auto spaceNativeOrigin = m_space->nativeOrigin();
    if (!spaceNativeOrigin) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Unable to determine hit test source space origin"_s });
        return { };
    }

    auto poseTransform = WebXRFrame::matrixFromPose(m_result.pose);
    auto anchorTransform = spaceNativeOrigin.value() * poseTransform;

    TransformationMatrix::Decomposed4Type decomposed;
    if (!anchorTransform.decompose4(decomposed)) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Unable to decompose anchor transform"_s });
        return { };
    }
    PlatformXR::FrameData::Pose anchorPose;
    anchorPose.position = FloatPoint3D(decomposed.translateX, decomposed.translateY, decomposed.translateZ);
    anchorPose.orientation = {
        static_cast<float>(decomposed.quaternion.x),
        static_cast<float>(decomposed.quaternion.y),
        static_cast<float>(decomposed.quaternion.z),
        static_cast<float>(decomposed.quaternion.w)
    };

    Ref<WebXRFrame> frameRef = m_frame;
    device->createAnchor(anchorPose, [frameRef = WTF::move(frameRef), promise = WTF::move(promise), protectedDocument = Ref { document }](ExceptionOr<PlatformXR::AnchorIdentifier> exceptionOrIdentifier) mutable {
        if (exceptionOrIdentifier.hasException()) {
            promise.reject(exceptionOrIdentifier.releaseException());
            return;
        }

        auto identifier = exceptionOrIdentifier.releaseReturnValue();
        Ref anchor = WebXRAnchor::create(protectedDocument.get(), frameRef->session(), identifier);
        frameRef->session().addAnchor(anchor.copyRef());
        promise.resolve(WTF::move(anchor));
    });

    return { };
}

#endif // ENABLE(WEBXR_ANCHORS)

} // namespace WebCore

#endif // ENABLE(WEBXR_HIT_TEST)
