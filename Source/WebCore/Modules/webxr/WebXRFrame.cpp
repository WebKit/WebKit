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
#include "WebXRFrame.h"

#if ENABLE(WEBXR)

#include "WebXRBoundedReferenceSpace.h"
#include "WebXRReferenceSpace.h"
#include "WebXRSession.h"
#include "WebXRViewerPose.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRFrame);

Ref<WebXRFrame> WebXRFrame::create(WebXRSession& session, IsAnimationFrame isAnimationFrame)
{
    return adoptRef(*new WebXRFrame(session, isAnimationFrame));
}

WebXRFrame::WebXRFrame(WebXRSession& session, IsAnimationFrame isAnimationFrame)
    : m_isAnimationFrame(isAnimationFrame == IsAnimationFrame::Yes)
    , m_session(session)
{
}

WebXRFrame::~WebXRFrame() = default;

bool WebXRFrame::isOutsideNativeBoundsOfBoundedReferenceSpace(const WebXRSpace& space, const WebXRSpace&) const
{
    if (!is<WebXRBoundedReferenceSpace>(space))
        return false;

    // FIXME: return true whenever the distance from the bounded geometry of
    // |space| to the native origin of |other| space is greater than 1m
    // (suggested by specs).

    return false;
}

bool WebXRFrame::isLocalReferenceSpace(const WebXRSpace& space) const
{
    if (!is<WebXRReferenceSpace>(space))
        return false;

    auto type = downcast<WebXRReferenceSpace>(space).type();
    if (type == XRReferenceSpaceType::Local || type == XRReferenceSpaceType::LocalFloor)
        return true;

    return false;
}

// https://immersive-web.github.io/webxr/#poses-must-be-limited
bool WebXRFrame::mustPosesBeLimited(const WebXRSpace& space, const WebXRSpace& baseSpace) const
{
    if (isOutsideNativeBoundsOfBoundedReferenceSpace(space, baseSpace)
        || isOutsideNativeBoundsOfBoundedReferenceSpace(baseSpace, space))
        return true;

    if (isLocalReferenceSpace(space) || isLocalReferenceSpace(baseSpace)) {
        // FIXME: If the distance between native origins of spaces is greater
        // than 15m (suggested by specs) return true.
    }

    return false;
}

struct WebXRFrame::PopulatedPose {
    TransformationMatrix transform;
    bool emulatedPosition { false };
};

// https://immersive-web.github.io/webxr/#populate-the-pose
ExceptionOr<std::optional<WebXRFrame::PopulatedPose>> WebXRFrame::populatePose(const Document& document, const WebXRSpace& space, const WebXRSpace& baseSpace)
{
    // 1. If frame’s active boolean is false, throw an InvalidStateError and abort these steps.
    if (!m_active)
        return Exception { InvalidStateError };

    // 2. Let session be frame’s session object.
    // 3. If space’s session does not equal session, throw an InvalidStateError and abort these steps.
    if (&space.session() != m_session.ptr())
        return Exception { InvalidStateError };

    // 4. If baseSpace’s session does not equal session, throw an InvalidStateError and abort these steps.
    if (&baseSpace.session() != m_session.ptr())
        return Exception { InvalidStateError };

    // 5. Check if poses may be reported and, if not, throw a SecurityError and abort these steps.
    if (!m_session->posesCanBeReported(document))
        return Exception { SecurityError };

    // 6. Let limit be the result of whether poses must be limited between space and baseSpace.
    // 7. Let transform be pose’s transform.
    // 8. Query the XR device's tracking system for space’s pose relative to baseSpace at the frame’s time.

    if (m_isAnimationFrame && !m_session->frameData().isTrackingValid) {
        // FIXME: check if space’s pose relative to baseSpace has been determined in the past.
        // Anyway this emulation is usually provided by the system in the pose (e.g. OpenXR)
        // so we shouldn't hit this path in most XRPlatform ports.
        return { std::nullopt };
    }

    auto baseTransform = baseSpace.effectiveOrigin();
    if (!baseTransform.isInvertible())
        return { std::nullopt };

    auto transform =  *baseTransform.inverse() * space.effectiveOrigin();
    bool emulatedPosition = space.isPositionEmulated() || baseSpace.isPositionEmulated();

    bool limit = mustPosesBeLimited(space, baseSpace);
    if (limit) {
        // FIXME: apply pose limits logic
        // https://immersive-web.github.io/webxr/#poses-must-be-limited
    }

    return { PopulatedPose { transform, emulatedPosition } };
}

// https://immersive-web.github.io/webxr/#dom-xrframe-getviewerpose
ExceptionOr<RefPtr<WebXRViewerPose>> WebXRFrame::getViewerPose(const Document& document, const WebXRReferenceSpace& referenceSpace)
{
    // 1. Let frame be this.
    // 2. Let session be frame’s session object.
    // 3. If frame’s animationFrame boolean is false, throw an InvalidStateError and abort these steps.
    if (!m_isAnimationFrame)
        return Exception { InvalidStateError };

    // 4. Let pose be a new XRViewerPose object in the relevant realm of session.
    // 5. Populate the pose of session’s viewer reference space in referenceSpace at the time represented by frame into pose.
    auto populatePoseResult = populatePose(document, m_session->viewerReferenceSpace(), referenceSpace);
    if (populatePoseResult.hasException())
        return populatePoseResult.releaseException();

    // 6. If pose is null return null.
    auto populateValue = populatePoseResult.releaseReturnValue();
    if (!populateValue)
        return nullptr;

    RefPtr<WebXRViewerPose> pose = WebXRViewerPose::create(WebXRRigidTransform::create(populateValue->transform), populateValue->emulatedPosition);

    // 7. Let xrviews be an empty list.
    Vector<Ref<WebXRView>> xrViews;
    // 8. For each active view view in the list of views on session, perform the following steps:
    const auto& frameData = m_session->frameData();
    for (auto& view : m_session->views()) {
        auto index = xrViews.size();
        if (!view.active || frameData.views.size() <= index)
            continue;

        // 8.1 Let xrview be a new XRView object in the relevant realm of session.
        // 8.2 Initialize xrview’s underlying view to view.
        // 8.3 Initialize xrview’s eye to view’s eye.
        // 8.4 Initialize xrview’s frame time to frame’s time.
        // 8.5 Initialize xrview’s session to session.
        // 8.6. Let offset be an new XRRigidTransform object equal to the view offset of view in the relevant realm of session.
        // 8.7. Set xrview’s transform property to the result of multiplying the XRViewerPose's transform by the offset transform in the relevant realm of session
        auto offset = matrixFromPose(frameData.views[index].offset);
        auto transform = WebXRRigidTransform::create(pose->transform().rawTransform() * offset);

        // Set projection matrix for each view
        std::array<float, 16> projection = switchOn(frameData.views[index].projection, [&](const PlatformXR::Device::FrameData::Fov& fov) {
            double near = m_session->renderState().depthNear();
            double far = m_session->renderState().depthFar();
            return TransformationMatrix::fromProjection(fov.up, fov.down, fov.left, fov.right, near, far).toColumnMajorFloatArray();
        }, [&](const std::array<float, 16>& matrix) {
            return matrix;
        }, [&](const std::nullptr_t&) {
            // Use aspect projection for inline sessions
            double fov =  m_session->renderState().inlineVerticalFieldOfView().value_or(piOverTwoDouble);
            float aspect = 1;
            auto layer = m_session->renderState().baseLayer();
            if (layer)
                aspect = static_cast<double>(layer->framebufferWidth()) / static_cast<double>(layer->framebufferHeight());
            double near = m_session->renderState().depthNear();
            double far = m_session->renderState().depthFar();
            return TransformationMatrix::fromProjection(fov, aspect, near, far).toColumnMajorFloatArray();
        });

        auto xrView = WebXRView::create(makeRef(*this), view.eye, WTFMove(transform), Float32Array::create(projection.data(), projection.size()));
        xrView->setViewportModifiable(m_session->supportsViewportScaling());

        //  8.8. Append xrview to xrviews
        xrViews.append(WTFMove(xrView));
    }

    // 9. Set pose’s views to xrviews
    pose->setViews(WTFMove(xrViews));

    // 10. Return pose.
    return pose;
}

ExceptionOr<RefPtr<WebXRPose>> WebXRFrame::getPose(const Document& document, const WebXRSpace& space, const WebXRSpace& baseSpace)
{
    // 1. Let frame be this.
    // 2. Let pose be a new XRPose object in the relevant realm of frame.
    // 3. Populate the pose of space in baseSpace at the time represented by frame into pose.

    auto populatePoseResult = populatePose(document, space, baseSpace);
    if (populatePoseResult.hasException())
        return populatePoseResult.releaseException();

    auto populateValue = populatePoseResult.releaseReturnValue();
    if (!populateValue)
        return nullptr;

    // 4. Return pose.
    return RefPtr<WebXRPose>(WebXRPose::create(WebXRRigidTransform::create(populateValue->transform), populateValue->emulatedPosition));
}

TransformationMatrix WebXRFrame::matrixFromPose(const PlatformXR::Device::FrameData::Pose& pose)
{
    TransformationMatrix matrix;
    matrix.translate3d(pose.position.x(), pose.position.y(), pose.position.z());
    matrix.multiply(TransformationMatrix::fromQuaternion(pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w));
    return matrix;
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
