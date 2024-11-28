/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "GraphicsContextGL.h"
#include "JSDOMPromiseDeferred.h"
#include "WebFakeXRInputController.h"
#include <wtf/CompletionHandler.h>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SimulatedXRDevice);

static constexpr Seconds FakeXRFrameTime = 15_ms;

void FakeXRView::setProjection(const Vector<float>& projection)
{
    std::copy(std::begin(projection), std::end(projection), std::begin(m_projection));
}

void FakeXRView::setFieldOfView(const FakeXRViewInit::FieldOfViewInit& fov)
{
    m_fov = PlatformXR::FrameData::Fov { deg2rad(fov.upDegrees), deg2rad(fov.downDegrees), deg2rad(fov.leftDegrees), deg2rad(fov.rightDegrees) };
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

void SimulatedXRDevice::setViews(Vector<PlatformXR::FrameData::View>&& views)
{
    m_frameData.views = WTFMove(views);
}

void SimulatedXRDevice::setNativeBoundsGeometry(const Vector<FakeXRBoundsPoint>& geometry)
{
    m_frameData.stageParameters.id++;
    m_frameData.stageParameters.bounds.clear();
    for (auto& point : geometry)
        m_frameData.stageParameters.bounds.append({ static_cast<float>(point.x), static_cast<float>(point.z) });
}

void SimulatedXRDevice::setViewerOrigin(const std::optional<PlatformXR::FrameData::Pose>& origin)
{
    if (origin) {
        m_frameData.origin = *origin;
        m_frameData.isPositionValid = true;
        m_frameData.isTrackingValid = true;
        return;
    }

    m_frameData.origin = PlatformXR::FrameData::Pose();
    m_frameData.isPositionValid = false;
    m_frameData.isTrackingValid = false;
}

void SimulatedXRDevice::setVisibilityState(XRVisibilityState visibilityState)
{
    if (m_trackingAndRenderingClient)
        m_trackingAndRenderingClient->updateSessionVisibilityState(visibilityState);
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

void SimulatedXRDevice::initializeTrackingAndRendering(const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&)
{
#if !PLATFORM(COCOA)
    GraphicsContextGLAttributes attributes;
    attributes.depth = false;
    attributes.stencil = false;
    attributes.antialias = false;
    m_gl = createWebProcessGraphicsContextGL(attributes);
#endif

    if (m_trackingAndRenderingClient) {
        // WebXR FakeDevice waits for simulateInputConnection calls to add input sources-
        // There is no way to know how many simulateInputConnection calls will the device receive,
        // so notify the input sources have been initialized with an empty list. This is not a problem because
        // WPT tests rely on requestAnimationFrame updates to test the input sources.
        callOnMainThread([this, weakThis = ThreadSafeWeakPtr { *this }]() {
            auto protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (m_trackingAndRenderingClient)
                m_trackingAndRenderingClient->sessionDidInitializeInputSources({ });
        });
    }
}

void SimulatedXRDevice::shutDownTrackingAndRendering()
{
    if (m_supportsShutdownNotification)
        simulateShutdownCompleted();
    stopTimer();
#if !PLATFORM(COCOA)
    if (m_gl) {
        for (auto layer : m_layers)
            m_gl->deleteTexture(layer.value);
        m_gl = nullptr;
    }
#endif
    m_layers.clear();
}

void SimulatedXRDevice::stopTimer()
{
    if (m_frameTimer.isActive())
        m_frameTimer.stop();
}

void SimulatedXRDevice::frameTimerFired()
{
    PlatformXR::FrameData data = m_frameData.copy();
    data.shouldRender = true;

    for (auto& layer : m_layers) {
#if PLATFORM(COCOA)
        PlatformXR::FrameData::LayerSetupData layerSetupData;
        auto width = layer.value.width();
        auto height = layer.value.height();
        layerSetupData.physicalSize[0] = { static_cast<uint16_t>(width), static_cast<uint16_t>(height) };
        layerSetupData.viewports[0] = { 0, 0, width, height };
        layerSetupData.physicalSize[1] = { 0, 0 };
        layerSetupData.viewports[1] = { 0, 0, 0, 0 };

        auto layerData = makeUniqueRef<PlatformXR::FrameData::LayerData>(PlatformXR::FrameData::LayerData {
            .layerSetup = layerSetupData,
            .renderingFrameIndex = 0,
            .textureData = std::nullopt,
            .requestDepth = false
        });
        data.layers.add(layer.key, WTFMove(layerData));
#else
        auto layerData = makeUniqueRef<PlatformXR::FrameData::LayerData>(PlatformXR::FrameData::LayerData {
            .framebufferSize = IntSize(0, 0),
            .opaqueTexture = layer.value
        });
        data.layers.add(layer.key, WTFMove(layerData));
#endif
    }

    for (auto& input : m_inputConnections) {
        if (input->isConnected())
            data.inputSources.append(input->getFrameData());
    }

    if (m_FrameCallback)
        m_FrameCallback(WTFMove(data));
}

void SimulatedXRDevice::requestFrame(std::optional<PlatformXR::RequestData>&&, RequestFrameCallback&& callback)
{
    m_FrameCallback = WTFMove(callback);
    if (!m_frameTimer.isActive())
        m_frameTimer.startOneShot(FakeXRFrameTime);
}

std::optional<PlatformXR::LayerHandle> SimulatedXRDevice::createLayerProjection(uint32_t width, uint32_t height, bool alpha)
{
#if PLATFORM(COCOA)
    // TODO: Might need to pass the format type to WebXROpaqueFramebuffer to ensure alpha is handled correctly in tests.
    UNUSED_PARAM(alpha);
    PlatformXR::LayerHandle handle = ++m_layerIndex;
    m_layers.add(handle, IntSize { static_cast<int>(width), static_cast<int>(height) });
#else
    using GL = GraphicsContextGL;
    if (!m_gl)
        return std::nullopt;
    PlatformXR::LayerHandle handle = ++m_layerIndex;
    auto texture = m_gl->createTexture();
    auto colorFormat = alpha ? GL::RGBA : GL::RGB;

    m_gl->bindTexture(GL::TEXTURE_2D, texture);
    m_gl->texParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_S, GL::CLAMP_TO_EDGE);
    m_gl->texParameteri(GL::TEXTURE_2D, GL::TEXTURE_WRAP_T, GL::CLAMP_TO_EDGE);
    m_gl->texParameteri(GL::TEXTURE_2D, GL::TEXTURE_MIN_FILTER, GL::LINEAR);
    m_gl->texParameteri(GL::TEXTURE_2D, GL::TEXTURE_MAG_FILTER, GL::LINEAR);
    m_gl->texImage2D(GL::TEXTURE_2D, 0, colorFormat, width, height, 0, colorFormat, GL::UNSIGNED_BYTE, 0);

    m_layers.add(handle, texture);
#endif
    return handle;
}

void SimulatedXRDevice::deleteLayer(PlatformXR::LayerHandle handle)
{
    auto it = m_layers.find(handle);
    if (it != m_layers.end()) {
#if !PLATFORM(COCOA)
        if (m_gl)
            m_gl->deleteTexture(it->value);
#endif
        m_layers.remove(it);
    }
}

Vector<PlatformXR::Device::ViewData> SimulatedXRDevice::views(PlatformXR::SessionMode mode) const
{
    if (mode == PlatformXR::SessionMode::ImmersiveVr)
        return { { .active = true, .eye = PlatformXR::Eye::Left }, { .active = true, .eye = PlatformXR::Eye::Right } };

    return { { .active = true, .eye = PlatformXR::Eye::None } };
}

WebFakeXRDevice::WebFakeXRDevice()
    : m_device(adoptRef(*new SimulatedXRDevice()))
{
}

void WebFakeXRDevice::setViews(const Vector<FakeXRViewInit>& views)
{
    Vector<PlatformXR::FrameData::View> deviceViews;

    for (auto& viewInit : views) {
        auto parsedView = parseView(viewInit);
        if (!parsedView.hasException()) {
            auto fakeView = parsedView.releaseReturnValue();
            PlatformXR::FrameData::View view;
            view.offset = fakeView->offset();
            if (fakeView->fieldOfView())
                view.projection = { *fakeView->fieldOfView() };
            else
                view.projection = { fakeView->projection() };

            deviceViews.append(view);
        }
    }

    m_device->setViews(WTFMove(deviceViews));
}

void WebFakeXRDevice::disconnect(DOMPromiseDeferred<void>&& promise)
{
    promise.resolve();
}

void WebFakeXRDevice::setViewerOrigin(FakeXRRigidTransformInit origin, bool emulatedPosition)
{
    auto pose = parseRigidTransform(origin);
    if (pose.hasException())
        return;

    m_device->setViewerOrigin(pose.releaseReturnValue());
    m_device->setEmulatedPosition(emulatedPosition);
}

void WebFakeXRDevice::simulateVisibilityChange(XRVisibilityState visibilityState)
{
    m_device->setVisibilityState(visibilityState);
}

void WebFakeXRDevice::setFloorOrigin(FakeXRRigidTransformInit origin)
{
    auto pose = parseRigidTransform(origin);
    if (pose.hasException())
        return;

    m_device->setFloorOrigin(pose.releaseReturnValue());
}

void WebFakeXRDevice::simulateResetPose()
{
}

Ref<WebFakeXRInputController> WebFakeXRDevice::simulateInputSourceConnection(const FakeXRInputSourceInit& init)
{
    auto handle = ++mInputSourceHandleIndex;
    auto input = WebFakeXRInputController::create(handle, init);
    m_device->addInputConnection(input.copyRef());
    return input;
}

ExceptionOr<PlatformXR::FrameData::Pose> WebFakeXRDevice::parseRigidTransform(const FakeXRRigidTransformInit& init)
{
    if (init.position.size() != 3 || init.orientation.size() != 4)
        return Exception { ExceptionCode::TypeError };

    PlatformXR::FrameData::Pose pose;
    pose.position = { init.position[0], init.position[1], init.position[2] };
    pose.orientation = { init.orientation[0], init.orientation[1], init.orientation[2], init.orientation[3] };

    return pose;
}

ExceptionOr<Ref<FakeXRView>> WebFakeXRDevice::parseView(const FakeXRViewInit& init)
{
    // https://immersive-web.github.io/webxr-test-api/#parse-a-view
    auto fakeView = FakeXRView::create(init.eye);

    if (init.projectionMatrix.size() != 16)
        return Exception { ExceptionCode::TypeError };
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
    m_device->setSupportsShutdownNotification(true);
}

void WebFakeXRDevice::simulateShutdown()
{
    m_device->simulateShutdownCompleted();
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
