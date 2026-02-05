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
class GBMDevice;
class GLContext;
class GLDisplay;
}

namespace WebKit {

class OpenXRInput;
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

    void createLayerProjection(uint32_t, uint32_t, bool, CompletionHandler<void(std::optional<PlatformXR::LayerHandle>)>&&) override;

    void startSession(WebPageProxy&, WeakPtr<PlatformXRCoordinatorSessionEventClient>&&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&, std::optional<WebCore::XRCanvasConfiguration>&&) override;
    void endSessionIfExists(WebPageProxy&) override;

    void scheduleAnimationFrame(WebPageProxy&, std::optional<PlatformXR::RequestData>&&, PlatformXR::Device::RequestFrameCallback&& onFrameUpdateCallback) override;
    void submitFrame(WebPageProxy&, Vector<XRDeviceLayer>&&) override;

#if ENABLE(WEBXR_HIT_TEST)
    void requestHitTestSource(WebPageProxy&, const PlatformXR::HitTestOptions&, CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::HitTestSource>)>&&) override;
    void deleteHitTestSource(WebPageProxy&, PlatformXR::HitTestSource) override;
    void requestTransientInputHitTestSource(WebPageProxy&, const PlatformXR::TransientInputHitTestOptions&, CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::TransientInputHitTestSource>)>&&) override;
    void deleteTransientInputHitTestSource(WebPageProxy&, PlatformXR::TransientInputHitTestSource) override;
#endif

private:
    void createInstance();
    RefPtr<WebCore::GLDisplay> createGLDisplay(bool isForTesting) const;
    void initializeDevice(bool isForTesting);
    void initializeSystem();
    void initializeBlendModes();
    void collectViewConfigurations();

    struct Idle {
    };
    struct Active {
        WeakPtr<PlatformXRCoordinatorSessionEventClient> sessionEventClient;
        WebCore::PageIdentifier pageIdentifier;
        Box<RenderState> renderState;
        RefPtr<WorkQueue> renderQueue;
        bool didStart { false };
    };
    using State = Variant<Idle, Active>;

    void createSessionIfNeeded();
    void handleSessionStateChange();
    void tryInitializeGraphicsBinding();
    void cleanupAllResources();
    void cleanupInstanceAndAssociatedResources();
    void cleanupSessionAndAssociatedResources();
    bool collectSwapchainFormatsIfNeeded();
    enum class PollResult : bool;
    PollResult pollEvents();
    std::unique_ptr<OpenXRSwapchain> createSwapchain(uint32_t width, uint32_t height, bool alpha) const;
    void createReferenceSpacesIfNeeded(Box<RenderState>);
#if ENABLE(WEBXR_HIT_TEST)
    XrSpace spaceForHitTest(const PlatformXR::NativeOriginInformation&) const;
#endif
    PlatformXR::FrameData populateFrameData(Box<RenderState>);
    void beginFrame(Box<RenderState>);
    void endFrame(Box<RenderState>, Vector<XRDeviceLayer>&&);
    void renderLoop(Box<RenderState>);
    XrEnvironmentBlendMode blendModeForSessionMode(Box<RenderState>) const;

    XRDeviceIdentifier m_deviceIdentifier { XRDeviceIdentifier::generate() };
    XrInstance m_instance { XR_NULL_HANDLE };
    XrSystemId m_systemId { XR_NULL_SYSTEM_ID };
    State m_state;
    Vector<XrViewConfigurationView> m_viewConfigurationViews;
    XrViewConfigurationType m_currentViewConfiguration;
    XrEnvironmentBlendMode m_vrBlendMode;
    XrEnvironmentBlendMode m_arBlendMode;
    PlatformXR::SessionMode m_sessionMode;
    RefPtr<WebCore::GLDisplay> m_glDisplay;
#if USE(GBM)
    mutable RefPtr<WebCore::GBMDevice> m_gbmDevice;
#endif
    std::unique_ptr<OpenXRInput> m_input;

    XrSession m_session { XR_NULL_HANDLE };
    XrSessionState m_sessionState { XR_SESSION_STATE_UNKNOWN };
    bool m_isSessionRunning { false };
    Vector<XrView> m_views;
    HashMap<PlatformXR::LayerHandle, std::unique_ptr<OpenXRLayer>> m_layers;
    Vector<int64_t> m_supportedSwapchainFormats;
#if OS(ANDROID)
    XrGraphicsBindingOpenGLESAndroidKHR m_graphicsBinding;
#else
    XrGraphicsBindingEGLMNDX m_graphicsBinding;
#endif
    std::unique_ptr<WebCore::GLContext> m_glContext;
    XrSpace m_viewerSpace { XR_NULL_HANDLE };
    XrSpace m_localSpace { XR_NULL_HANDLE };
    XrSpace m_floorSpace { XR_NULL_HANDLE };

    PlatformXR::LayerHandle m_nextLayerHandle { 1 };
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
