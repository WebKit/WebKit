/*
 * Copyright (C) 2025-2026 Igalia, S.L.
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

#if ENABLE(WEBXR) && USE(OPENXR)

#include "OpenXRExtensions.h"
#include "PlatformXRCoordinator.h"
#include <WebCore/IntSize.h>
#include <WebCore/PageIdentifier.h>
#include <openxr/openxr.h>
#include <wtf/Box.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Threading.h>

namespace WebKit {

#if ENABLE(WEBXR_HIT_TEST)
class OpenXRHitTestManager;
#endif
class OpenXRGraphicsBinding;
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

    void createLayerProjection(uint32_t, uint32_t, bool, CompletionHandler<void(std::optional<PlatformXR::LayerInfo>)>&&) override;

#if ENABLE(WEBXR_LAYERS)
    void createCompositionLayer(PlatformXR::CompositionLayerType, WebCore::IntSize, PlatformXR::LayerLayout, CreateCompositionLayerCallback&&) override;
#endif

    void startSession(WebPageProxy&, WeakPtr<PlatformXRCoordinatorSessionEventClient>&&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&, std::optional<WebCore::XRCanvasConfiguration>&&) override;
    void endSessionIfExists(WebPageProxy&) override;

    void scheduleAnimationFrame(WebPageProxy&, std::optional<PlatformXR::RequestData>&&, PlatformXR::Device::RequestFrameCallback&& onFrameUpdateCallback) override;
    void submitFrame(WebPageProxy&, Vector<PlatformXR::DeviceLayer>&&) override;

#if ENABLE(WEBXR_HIT_TEST)
    void requestHitTestSource(WebPageProxy&, const PlatformXR::HitTestOptions&, CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::HitTestSource>)>&&) override;
    void deleteHitTestSource(WebPageProxy&, PlatformXR::HitTestSource) override;
    void requestTransientInputHitTestSource(WebPageProxy&, const PlatformXR::TransientInputHitTestOptions&, CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::TransientInputHitTestSource>)>&&) override;
    void deleteTransientInputHitTestSource(WebPageProxy&, PlatformXR::TransientInputHitTestSource) override;
#endif

private:
    void createInstance();
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
    void cleanupAllResources();
    void cleanupInstanceAndAssociatedResources();
    void cleanupSessionAndAssociatedResources();
    bool collectSwapchainFormatsIfNeeded();
    enum class PollResult : bool;
    PollResult pollEvents();
    std::unique_ptr<OpenXRSwapchain> createSwapchain(uint32_t width, uint32_t height, bool alpha, uint32_t faceCount = 1) const;
    void createReferenceSpacesIfNeeded(Box<RenderState>);
#if ENABLE(WEBXR_HIT_TEST)
    XrSpace spaceForHitTest(const PlatformXR::NativeOriginInformation&) const;
#endif
    PlatformXR::FrameData populateFrameData(Box<RenderState>);
    void beginFrame(Box<RenderState>);
    void endFrame(Box<RenderState>, Vector<PlatformXR::DeviceLayer>&&);
    void maybeBeginFrame(Box<RenderState>);
    void waitForSessionReady(Box<RenderState>, Function<void()>&&);
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
    std::unique_ptr<OpenXRGraphicsBinding> m_graphicsBinding;
    std::unique_ptr<OpenXRInput> m_input;

    XrSession m_session { XR_NULL_HANDLE };
    XrSessionState m_sessionState { XR_SESSION_STATE_UNKNOWN };
    bool m_isSessionRunning { false };
    Vector<XrView> m_views;
    HashMap<PlatformXR::LayerHandle, std::unique_ptr<OpenXRLayer>> m_layers;
    Vector<int64_t> m_supportedSwapchainFormats;
    XrSpace m_viewerSpace { XR_NULL_HANDLE };
    XrSpace m_localSpace { XR_NULL_HANDLE };
    XrSpace m_floorSpace { XR_NULL_HANDLE };

    PlatformXR::LayerHandle m_nextLayerHandle { 1 };

#if ENABLE(WEBXR_HIT_TEST)
    std::unique_ptr<OpenXRHitTestManager> m_hitTestManager;
#endif
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
