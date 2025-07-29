/*
 * Copyright (C) 2025 Igalia, S.L.
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

#include "OpenXRExtensions.h"
#include "PlatformXRCoordinator.h"

#include <WebCore/PageIdentifier.h>
#include <openxr/openxr.h>
#include <wtf/Box.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Threading.h>

namespace WebCore {
class GLContext;
class PlatformDisplay;
}

namespace WebKit {

class OpenXRLayer;
class OpenXRSwapchain;

class OpenXRCoordinator final : public PlatformXRCoordinator {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRCoordinator);
    struct RenderState;
public:
    OpenXRCoordinator();
    virtual ~OpenXRCoordinator();

    void getPrimaryDeviceInfo(WebPageProxy&, DeviceInfoCallback&&) override;
    void requestPermissionOnSessionFeatures(WebPageProxy&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, FeatureListCallback&&) override;

    void createLayerProjection(uint32_t, uint32_t, bool) override;

    void startSession(WebPageProxy&, WeakPtr<PlatformXRCoordinatorSessionEventClient>&&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&) override;
    void endSessionIfExists(WebPageProxy&) override;

    void scheduleAnimationFrame(WebPageProxy&, std::optional<PlatformXR::RequestData>&&, PlatformXR::Device::RequestFrameCallback&& onFrameUpdateCallback) override;
    void submitFrame(WebPageProxy&, Vector<XRDeviceLayer>&&) override;

private:
    void createInstance();
    void createSessionIfNeeded();
    std::unique_ptr<OpenXRSwapchain> createSwapchain(uint32_t width, uint32_t height, bool alpha);
    void cleanupSessionAndAssociatedResources();
    void initializeDevice();
    void initializeSystem();
    void initializeBlendModes();
    void tryInitializeGraphicsBinding();
    void collectViewConfigurations();
    bool collectSwapchainFormatsIfNeeded();

    struct Idle {
    };
    struct Active {
        WeakPtr<PlatformXRCoordinatorSessionEventClient> sessionEventClient;
        WebCore::PageIdentifier pageIdentifier;
        Box<RenderState> renderState;
        RefPtr<Thread> renderThread;
    };
    using State = Variant<Idle, Active>;

    void handleSessionStateChange(Box<RenderState>);
    void endSessionIfExists(std::optional<WebCore::PageIdentifier>);
    enum class PollResult : bool;
    PollResult pollEvents(Box<RenderState>);
    PlatformXR::FrameData populateFrameData(Box<RenderState>);
    void createReferenceSpacesIfNeeded(Box<RenderState>);
    void renderLoop(Box<RenderState>);
    void submitFrameInternal(Box<RenderState>, Vector<XRDeviceLayer>&&);

    XRDeviceIdentifier m_deviceIdentifier { XRDeviceIdentifier::generate() };
    XrInstance m_instance { XR_NULL_HANDLE };
    XrSystemId m_systemId { XR_NULL_SYSTEM_ID };
    XrSession m_session { XR_NULL_HANDLE };
    Vector<XrViewConfigurationView> m_viewConfigurationViews;
    XrViewConfigurationType m_currentViewConfiguration;
    Vector<XrView> m_views;
    XrSessionState m_sessionState { XR_SESSION_STATE_UNKNOWN };
    XrEnvironmentBlendMode m_vrBlendMode;
    XrEnvironmentBlendMode m_arBlendMode;
    XrSpace m_localSpace { XR_NULL_HANDLE };
    XrSpace m_floorSpace { XR_NULL_HANDLE };

    std::unique_ptr<OpenXRExtensions>
        m_extensions;
    bool m_isSessionRunning { false };
    HashMap<PlatformXR::LayerHandle, std::unique_ptr<OpenXRLayer>> m_layers;
    Vector<int64_t> m_supportedSwapchainFormats;

    std::unique_ptr<WebCore::PlatformDisplay> m_platformDisplay;
    std::unique_ptr<WebCore::GLContext> m_glContext;
    XrGraphicsBindingEGLMNDX m_graphicsBinding;

    State m_state;
    PlatformXR::SessionMode m_sessionMode;
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
