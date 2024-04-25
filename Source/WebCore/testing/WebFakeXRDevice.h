/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBXR)

#include "ExceptionOr.h"
#include "FakeXRBoundsPoint.h"
#include "FakeXRInputSourceInit.h"
#include "FakeXRViewInit.h"
#include "JSDOMPromiseDeferredForward.h"
#include "PlatformXR.h"
#include "Timer.h"
#include "WebFakeXRInputController.h"
#include "XRVisibilityState.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {
class GraphicsContextGL;

class FakeXRView final : public RefCounted<FakeXRView> {
public:
    static Ref<FakeXRView> create(XREye eye) { return adoptRef(*new FakeXRView(eye)); }
    using Pose = PlatformXR::FrameData::Pose;
    using Fov = PlatformXR::FrameData::Fov;

    XREye eye() const { return m_eye; }
    const Pose& offset() const { return m_offset; }
    const std::array<float, 16>& projection() const { return m_projection; }
    const std::optional<Fov>& fieldOfView() const { return m_fov;}

    void setResolution(FakeXRViewInit::DeviceResolution resolution) { m_resolution = resolution; }
    void setOffset(Pose&& offset) { m_offset = WTFMove(offset); }
    void setProjection(const Vector<float>&);
    void setFieldOfView(const FakeXRViewInit::FieldOfViewInit&);
private:
    FakeXRView(XREye eye)
        : m_eye(eye) { }


    XREye m_eye;
    FakeXRViewInit::DeviceResolution m_resolution;
    Pose m_offset;
    std::array<float, 16> m_projection;
    std::optional<Fov> m_fov;
};

class SimulatedXRDevice final : public PlatformXR::Device {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SimulatedXRDevice();
    virtual ~SimulatedXRDevice();
    void setViews(Vector<PlatformXR::FrameData::View>&&);
    void setNativeBoundsGeometry(const Vector<FakeXRBoundsPoint>&);
    void setViewerOrigin(const std::optional<PlatformXR::FrameData::Pose>&);
    void setFloorOrigin(std::optional<PlatformXR::FrameData::Pose>&& origin) { m_frameData.floorTransform = WTFMove(origin); }
    void setEmulatedPosition(bool emulated) { m_frameData.isPositionEmulated = emulated; }
    void setSupportsShutdownNotification(bool supportsShutdownNotification) { m_supportsShutdownNotification = supportsShutdownNotification; }
    void setVisibilityState(XRVisibilityState);
    void simulateShutdownCompleted();
    void scheduleOnNextFrame(Function<void()>&&);
    void addInputConnection(Ref<WebFakeXRInputController>&& input) { m_inputConnections.append(WTFMove(input)); };
private:
    WebCore::IntSize recommendedResolution(PlatformXR::SessionMode) final;
    void initializeTrackingAndRendering(const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&) final;
    void shutDownTrackingAndRendering() final;
    bool supportsSessionShutdownNotification() const final { return m_supportsShutdownNotification; }
    void initializeReferenceSpace(PlatformXR::ReferenceSpaceType) final { }
    Vector<PlatformXR::Device::ViewData> views(PlatformXR::SessionMode) const final;
    void requestFrame(RequestFrameCallback&&) final;
    std::optional<PlatformXR::LayerHandle> createLayerProjection(uint32_t width, uint32_t height, bool alpha) final;
    void deleteLayer(PlatformXR::LayerHandle) final;

    void stopTimer();
    void frameTimerFired();

    PlatformXR::FrameData m_frameData;
    bool m_supportsShutdownNotification { false };
    Timer m_frameTimer;
    RequestFrameCallback m_FrameCallback;
    RefPtr<WebCore::GraphicsContextGL> m_gl;
    HashMap<PlatformXR::LayerHandle, PlatformGLObject> m_layers;
    uint32_t m_layerIndex { 0 };
    Vector<Ref<WebFakeXRInputController>> m_inputConnections;
};

class WebFakeXRDevice final : public RefCounted<WebFakeXRDevice> {
public:
    static Ref<WebFakeXRDevice> create() { return adoptRef(*new WebFakeXRDevice()); }

    void setViews(const Vector<FakeXRViewInit>&);
    void disconnect(DOMPromiseDeferred<void>&&);
    void setViewerOrigin(FakeXRRigidTransformInit origin, bool emulatedPosition = false);
    void clearViewerOrigin() { m_device->setViewerOrigin(std::nullopt); }
    void simulateVisibilityChange(XRVisibilityState);
    void setBoundsGeometry(Vector<FakeXRBoundsPoint>&& bounds) { m_device->setNativeBoundsGeometry(WTFMove(bounds)); }
    void setFloorOrigin(FakeXRRigidTransformInit);
    void clearFloorOrigin() { m_device->setFloorOrigin(std::nullopt); }
    void simulateResetPose();
    Ref<WebFakeXRInputController> simulateInputSourceConnection(const FakeXRInputSourceInit&);
    static ExceptionOr<Ref<FakeXRView>> parseView(const FakeXRViewInit&);
    SimulatedXRDevice& simulatedXRDevice() { return m_device; }
    void setSupportsShutdownNotification();
    void simulateShutdown();

    static ExceptionOr<PlatformXR::FrameData::Pose> parseRigidTransform(const FakeXRRigidTransformInit&);

private:
    WebFakeXRDevice();

    Ref<SimulatedXRDevice> m_device;
    PlatformXR::InputSourceHandle mInputSourceHandleIndex { 0 };
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
