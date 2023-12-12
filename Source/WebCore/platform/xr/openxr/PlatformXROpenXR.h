/*
 * Copyright (C) 2020 Igalia, S.L.
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(WEBXR) && USE(OPENXR)

#include "GLContext.h"
#include "OpenXRLayer.h"
#include "OpenXRUtils.h"
#include "PlatformXR.h"

#include <wtf/HashMap.h>
#include <wtf/WorkQueue.h>

namespace WebCore {
class GraphicsContextGL;
}
namespace PlatformXR {

class OpenXRExtensions;
class OpenXRInput;

// https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#system
// A system represents a collection of related devices in the runtime, often made up of several individual
// hardware components working together to enable XR experiences.
//
// WebXR talks about XR devices, is a physical unit of hardware that can present imagery to the user, so
// there is not direct correspondence between an OpenXR system and a WebXR device because the system API
// is an abstraction for a collection of devices while the WebXR device is mostly one physical unit,
// usually an HMD or a phone/tablet.
//
// It's important also not to try to associate OpenXR system with WebXR's XRSystem because they're totally
// different concepts. The system in OpenXR was defined above as a collection of related devices. In WebXR,
// the XRSystem is basically the entry point for the WebXR API available via the Navigator object.
class OpenXRDevice final : public Device {
public:
    static Ref<OpenXRDevice> create(XrInstance, XrSystemId, Ref<WorkQueue>&&, const OpenXRExtensions&, CompletionHandler<void()>&&);
    virtual ~OpenXRDevice() = default;

private:
    OpenXRDevice(XrInstance, XrSystemId, Ref<WorkQueue>&&, const OpenXRExtensions&);
    void initialize(CompletionHandler<void()>&& callback);

    // PlatformXR::Device
    WebCore::IntSize recommendedResolution(SessionMode) final;
    void initializeTrackingAndRendering(const WebCore::SecurityOriginData&, SessionMode, const Device::FeatureList&) final;
    void shutDownTrackingAndRendering() final;
    void initializeReferenceSpace(PlatformXR::ReferenceSpaceType) final;
    bool supportsSessionShutdownNotification() const final { return true; }
    void requestFrame(RequestFrameCallback&&) final;
    void submitFrame(Vector<Device::Layer>&&) final;
    Vector<ViewData> views(SessionMode) const final;
    std::optional<LayerHandle> createLayerProjection(uint32_t width, uint32_t height, bool alpha) final;
    void deleteLayer(LayerHandle) final;

    // Custom methods
    FeatureList collectSupportedFeatures() const;
    void collectSupportedSessionModes();
    void collectConfigurationViews();
    XrSpace createReferenceSpace(XrReferenceSpaceType);
    void pollEvents();
    XrResult beginSession();
    void endSession();
    void resetSession();
    void handleSessionStateChange();
    void waitUntilStopping();
    void updateStageParameters();
    void updateInteractionProfile();
    LayerHandle generateLayerHandle() { return ++m_handleIndex; }

    XrInstance m_instance;
    XrSystemId m_systemId;
    WorkQueue& m_queue;
    const OpenXRExtensions& m_extensions;
    XrSession m_session { XR_NULL_HANDLE };
    XrSessionState m_sessionState { XR_SESSION_STATE_UNKNOWN };
    XrGraphicsBindingEGLMNDX m_graphicsBinding;
    std::unique_ptr<WebCore::GLContext> m_egl;
    RefPtr<WebCore::GraphicsContextGL> m_gl;
    XrFrameState m_frameState;
    Vector<XrView> m_frameViews;
    HashMap<LayerHandle, std::unique_ptr<OpenXRLayer>> m_layers;
    LayerHandle m_handleIndex { 0 };
    std::unique_ptr<OpenXRInput> m_input;
    bool didNotifyInputInitialization { false };

    using ViewConfigurationPropertiesMap = HashMap<XrViewConfigurationType, XrViewConfigurationProperties, IntHash<XrViewConfigurationType>, WTF::StrongEnumHashTraits<XrViewConfigurationType>>;
    ViewConfigurationPropertiesMap m_viewConfigurationProperties;
    using ViewConfigurationViewsMap = HashMap<XrViewConfigurationType, Vector<XrViewConfigurationView>, IntHash<XrViewConfigurationType>, WTF::StrongEnumHashTraits<XrViewConfigurationType>>;
    ViewConfigurationViewsMap m_configurationViews;
    XrViewConfigurationType m_currentViewConfigurationType;
    XrSpace m_localSpace { XR_NULL_HANDLE };
    XrSpace m_viewSpace { XR_NULL_HANDLE };
    XrSpace m_stageSpace { XR_NULL_HANDLE };
    FrameData::StageParameters m_stageParameters;
};

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(OPENXR)
