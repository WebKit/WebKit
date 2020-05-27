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
#include "WebFakeXRDevice.h"

#if ENABLE(WEBXR)

#include "DOMPointReadOnly.h"
#include "JSDOMPromiseDeferred.h"
#include "WebFakeXRInputController.h"

namespace WebCore {

void FakeXRView::setFieldOfView(FakeXRViewInit::FieldOfViewInit fov)
{
    m_fov = fov;
}

WebFakeXRDevice::WebFakeXRDevice() = default;

void WebFakeXRDevice::setViews(const Vector<FakeXRViewInit>& views)
{
    Vector<Ref<FakeXRView>>& deviceViews = m_device.views();
    deviceViews.clear();

    // TODO: do in next animation frame.
    for (auto& viewInit : views) {
        auto view = parseView(viewInit);
        if (!view.hasException())
            deviceViews.append(view.releaseReturnValue());
    }
}

void WebFakeXRDevice::disconnect(DOMPromiseDeferred<void>&& promise)
{
    promise.resolve();
}

void WebFakeXRDevice::setViewerOrigin(FakeXRRigidTransformInit origin, bool emulatedPosition)
{
    auto rigidTransform = parseRigidTransform(origin);
    if (rigidTransform.hasException())
        return;

    // TODO: do in next animation frame.
    m_device.setViewerOrigin(rigidTransform.releaseReturnValue());
    m_device.setEmulatedPosition(emulatedPosition);
}

void WebFakeXRDevice::clearViewerOrigin()
{
    // TODO: do in next animation frame.
    m_device.setViewerOrigin(nullptr);
}

void WebFakeXRDevice::simulateVisibilityChange(XRVisibilityState)
{
}

void WebFakeXRDevice::setBoundsGeometry(Vector<FakeXRBoundsPoint>)
{
}

void WebFakeXRDevice::setFloorOrigin(FakeXRRigidTransformInit origin)
{
    auto rigidTransform = parseRigidTransform(origin);
    if (rigidTransform.hasException())
        return;

    // TODO: do in next animation frame.
    m_device.setFloorOrigin(rigidTransform.releaseReturnValue());
}

void WebFakeXRDevice::clearFloorOrigin()
{
    // TODO: do in next animation frame.
    m_device.setFloorOrigin(nullptr);
}

void WebFakeXRDevice::simulateResetPose()
{
}

Ref<WebFakeXRInputController> WebFakeXRDevice::simulateInputSourceConnection(FakeXRInputSourceInit)
{
    return WebFakeXRInputController::create();
}

ExceptionOr<Ref<WebXRRigidTransform>> WebFakeXRDevice::parseRigidTransform(const FakeXRRigidTransformInit& init)
{
    if (init.position.size() != 3 || init.orientation.size() != 4)
        return Exception { TypeError };

    DOMPointInit position;
    position.x = init.position[0];
    position.y = init.position[1];
    position.z = init.position[2];

    DOMPointInit orientation;
    orientation.x = init.orientation[0];
    orientation.y = init.orientation[1];
    orientation.z = init.orientation[2];
    orientation.w = init.orientation[3];

    return WebXRRigidTransform::create(position, orientation);
}

ExceptionOr<Ref<FakeXRView>> WebFakeXRDevice::parseView(const FakeXRViewInit& init)
{
    // https://immersive-web.github.io/webxr-test-api/#parse-a-view
    auto fakeView = FakeXRView::create(init.eye);

    if (init.projectionMatrix.size() != 16)
        return Exception { TypeError };
    fakeView->view()->setProjectionMatrix(init.projectionMatrix);

    auto viewOffset = parseRigidTransform(init.viewOffset);
    if (viewOffset.hasException())
        return viewOffset.releaseException();
    fakeView->view()->setTransform(viewOffset.releaseReturnValue());

    fakeView->setResolution(init.resolution);

    if (init.fieldOfView) {
        fakeView->setFieldOfView(init.fieldOfView.value());
        // TODO: Set view’s projection matrix to the projection matrix
        // corresponding to this field of view, and depth values equal to
        // depthNear and depthFar of any XRSession associated with the device.
        // If there currently is none, use the default values of near=0.1,
        // far=1000.0.
    }

    return fakeView;
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
