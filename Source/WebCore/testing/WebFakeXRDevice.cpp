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
#include "GLContext.h"
#include "GraphicsContextGL.h"
#include "JSDOMPromiseDeferred.h"
#include "WebFakeXRInputController.h"
#include <wtf/CompletionHandler.h>
#include <wtf/MathExtras.h>

namespace WebCore {

static constexpr Seconds FakeXRFrameTime = 15_ms;

void FakeXRView::setProjection(const Vector<float>& projection)
{
    std::copy(std::begin(projection), std::end(projection), std::begin(m_projection));
}

void FakeXRView::setFieldOfView(const FakeXRViewInit::FieldOfViewInit& fov)
{
    m_fov = PlatformXR::Device::FrameData::Fov { deg2rad(fov.upDegrees), deg2rad(fov.downDegrees), deg2rad(fov.leftDegrees), deg2rad(fov.rightDegrees) };
}

SimulatedXRDevice::SimulatedXRDevice()
    : m_frameTimer(*this, &SimulatedXRDevice::frameTimerFired)
{
    m_supportsOrientationTracking = true;
}

SimulatedXRDevice::~SimulatedXRDevice()
{
    stopTimer();
}

void SimulatedXRDevice::setNativeBoundsGeometry(const Vector<FakeXRBoundsPoint>& geometry)
{
    m_stageParameters.id++;
    m_stageParameters.bounds.clear();
    for (auto& point : geometry)
        m_stageParameters.bounds.append({ static_cast<float>(point.x), static_cast<float>(point.z) });
}

void SimulatedXRDevice::simulateShutdownCompleted()
{
    if (m_trackingAndRenderingClient)
        m_trackingAndRenderingClient->sessionDidEnd();
}

WebCore::IntSize SimulatedXRDevice::recommendedResolution(PlatformXR::SessionMode)
{
    // Return at least a valid size for a framebuffer.
    return IntSize(32, 32);
}

void SimulatedXRDevice::initializeTrackingAndRendering(PlatformXR::SessionMode)
{
    GraphicsContextGLAttributes attributes;
    attributes.depth = false;
    attributes.stencil = false;
    attributes.antialias = false;
    m_gl = GraphicsContextGL::create(attributes, nullptr);
}

void SimulatedXRDevice::shutDownTrackingAndRendering()
{
    if (m_supportsShutdownNotification)
        simulateShutdownCompleted();
    stopTimer();
    if (m_gl) {
        for (auto layer : m_layers)
            m_gl->deleteTexture(layer.value);
    }
    m_layers.clear();
}

void SimulatedXRDevice::stopTimer()
{
    if (m_frameTimer.isActive())
        m_frameTimer.stop();
}

void SimulatedXRDevice::frameTimerFired()
{
    auto updates = WTFMove(m_pendingUpdates);
    for (auto& update : updates)
        update();

    FrameData data;
    data.shouldRender = true;
    if (m_viewerOrigin) {
        data.origin = *m_viewerOrigin;
        data.isTrackingValid = true;
        data.isPositionValid = true;
        data.isPositionEmulated = m_emulatedPosition;
    }

    if (m_floorOrigin)
        data.floorTransform = { *m_floorOrigin };

    data.stageParameters = m_stageParameters;

    for (auto& fakeView : m_views) {
        FrameData::View view;
        view.offset = fakeView->offset();
        if (fakeView->fieldOfView().hasValue())
            view.projection = { *fakeView->fieldOfView() };
        else
            view.projection = { fakeView->projection() };
        
        data.views.append(view);
    }

    for (auto& layer : m_layers)
        data.layers.add(layer.key, FrameData::LayerData { .opaqueTexture = layer.value });

    if (m_FrameCallback)
        m_FrameCallback(WTFMove(data));
}

void SimulatedXRDevice::requestFrame(RequestFrameCallback&& callback)
{
    m_FrameCallback = WTFMove(callback);
    if (!m_frameTimer.isActive())
        m_frameTimer.startOneShot(FakeXRFrameTime);
}

Optional<PlatformXR::LayerHandle> SimulatedXRDevice::createLayerProjection(uint32_t width, uint32_t height, bool alpha)
{
    using GL = GraphicsContextGL;
    if (!m_gl)
        return WTF::nullopt;
    PlatformXR::LayerHandle handle = ++m_layerIndex;
    auto texture = m_gl->createTexture();
    auto colorFormat = alpha ? GL::RGBA8 : GL::RGB8;

    m_gl->bindTexture(GL::TEXTURE_2D, texture);
    m_gl->texParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_S, GL::CLAMP_TO_EDGE);
    m_gl->texParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_T, GL::CLAMP_TO_EDGE);
    m_gl->texParameteri(GL::TEXTURE_2D, GL::TEXTURE_MIN_FILTER, GL::LINEAR);
    m_gl->texParameteri(GL::TEXTURE_2D, GL::TEXTURE_MAG_FILTER, GL::LINEAR);
    m_gl->texStorage2D(GL::TEXTURE_2D, 1, colorFormat, width, height);

    m_layers.add(handle, texture);
    return handle;    
}

void SimulatedXRDevice::deleteLayer(PlatformXR::LayerHandle handle)
{
    auto it = m_layers.find(handle);
    if (it != m_layers.end()) {
        if (m_gl)
            m_gl->deleteTexture(it->value);
        m_layers.remove(it);
    }
}

Vector<PlatformXR::Device::ViewData> SimulatedXRDevice::views(PlatformXR::SessionMode mode) const
{
    if (mode == PlatformXR::SessionMode::ImmersiveVr)
        return { { .active = true, .eye = PlatformXR::Eye::Left }, { .active = true, .eye = PlatformXR::Eye::Right } };

    return { { .active = true, .eye = PlatformXR::Eye::None } };
}

void SimulatedXRDevice::scheduleOnNextFrame(Function<void()>&& func)
{
    m_pendingUpdates.append(WTFMove(func));
}

WebFakeXRDevice::WebFakeXRDevice() = default;

void WebFakeXRDevice::setViews(const Vector<FakeXRViewInit>& views)
{
    m_device.scheduleOnNextFrame([this, views]() {
        Vector<Ref<FakeXRView>> deviceViews;

        for (auto& viewInit : views) {
            auto view = parseView(viewInit);
            if (!view.hasException())
                deviceViews.append(view.releaseReturnValue());
        }

        m_device.setViews(WTFMove(deviceViews));
    });
}

void WebFakeXRDevice::disconnect(DOMPromiseDeferred<void>&& promise)
{
    promise.resolve();
}

void WebFakeXRDevice::setViewerOrigin(FakeXRRigidTransformInit origin, bool emulatedPosition)
{
    auto result = parseRigidTransform(origin);
    if (result.hasException())
        return;

    auto pose = result.releaseReturnValue();

    m_device.scheduleOnNextFrame([this, pose = WTFMove(pose), emulatedPosition]() mutable {
        m_device.setViewerOrigin(WTFMove(pose));
        m_device.setEmulatedPosition(emulatedPosition);
    });
}

void WebFakeXRDevice::clearViewerOrigin()
{
    m_device.scheduleOnNextFrame([this]() {
        m_device.setViewerOrigin(WTF::nullopt);
    });
}

void WebFakeXRDevice::simulateVisibilityChange(XRVisibilityState)
{
}

void WebFakeXRDevice::setBoundsGeometry(Vector<FakeXRBoundsPoint>&& bounds)
{
    m_device.scheduleOnNextFrame([this, bounds = WTFMove(bounds)]() {
        m_device.setNativeBoundsGeometry(bounds);
    });
} 

void WebFakeXRDevice::setFloorOrigin(FakeXRRigidTransformInit origin)
{
    auto result = parseRigidTransform(origin);
    if (result.hasException())
        return;

    auto pose = result.releaseReturnValue();

    m_device.scheduleOnNextFrame([this, pose = WTFMove(pose)]() mutable {
        m_device.setFloorOrigin(WTFMove(pose));
    });
}

void WebFakeXRDevice::clearFloorOrigin()
{
    m_device.scheduleOnNextFrame([this]() {
        m_device.setFloorOrigin(WTF::nullopt);
    });
}

void WebFakeXRDevice::simulateResetPose()
{
}

Ref<WebFakeXRInputController> WebFakeXRDevice::simulateInputSourceConnection(FakeXRInputSourceInit)
{
    return WebFakeXRInputController::create();
}

ExceptionOr<PlatformXR::Device::FrameData::Pose> WebFakeXRDevice::parseRigidTransform(const FakeXRRigidTransformInit& init)
{
    if (init.position.size() != 3 || init.orientation.size() != 4)
        return Exception { TypeError };

    PlatformXR::Device::FrameData::Pose pose;
    pose.position = { init.position[0], init.position[1], init.position[2] };
    pose.orientation = { init.orientation[0], init.orientation[1], init.orientation[2], init.orientation[3] };

    return pose;
}

ExceptionOr<Ref<FakeXRView>> WebFakeXRDevice::parseView(const FakeXRViewInit& init)
{
    // https://immersive-web.github.io/webxr-test-api/#parse-a-view
    auto fakeView = FakeXRView::create(init.eye);

    if (init.projectionMatrix.size() != 16)
        return Exception { TypeError };
    fakeView->setProjection(init.projectionMatrix);

    auto viewOffset = parseRigidTransform(init.viewOffset);
    if (viewOffset.hasException())
        return viewOffset.releaseException();
    fakeView->setOffset(viewOffset.releaseReturnValue());

    fakeView->setResolution(init.resolution);

    if (init.fieldOfView) {
        fakeView->setFieldOfView(init.fieldOfView.value());
    }

    return fakeView;
}

void WebFakeXRDevice::setSupportsShutdownNotification()
{
    m_device.setSupportsShutdownNotification(true);
}

void WebFakeXRDevice::simulateShutdown()
{
    m_device.simulateShutdownCompleted();
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
