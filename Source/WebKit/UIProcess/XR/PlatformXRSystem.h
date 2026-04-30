/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "PlatformXRCoordinator.h"
#include "ProcessActivityGroup.h"
#include "ProcessThrottler.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/PlatformXR.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Variant.h>

namespace WebCore {
class SecurityOriginData;
struct XRCanvasConfiguration;

namespace WebGPU {
enum class TextureFormat : uint8_t;
}
}

namespace WebKit {

class PlatformXRCoordinator;
class WebPageProxy;

struct SharedPreferencesForWebProcess;
struct XRDeviceInfo;

class PlatformXRSystem : public IPC::MessageReceiver, public PlatformXRCoordinatorSessionEventClient, public RefCounted<PlatformXRSystem> {
    WTF_MAKE_TZONE_ALLOCATED(PlatformXRSystem);
public:
    static Ref<PlatformXRSystem> create(WebPageProxy& page)
    {
        return adoptRef(*new PlatformXRSystem(page));
    }

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    virtual ~PlatformXRSystem();

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess(IPC::Connection&) const;

    USING_CAN_MAKE_WEAKPTR(PlatformXRCoordinatorSessionEventClient);

    enum class InvalidationReason : uint8_t {
        Client,
        State
    };
    void invalidate(InvalidationReason invalidateReason = InvalidationReason::State);

    bool hasActiveSession() const;
    void ensureImmersiveSessionActivity();

private:
    explicit PlatformXRSystem(WebPageProxy&);

    static PlatformXRCoordinator* xrCoordinator();

    bool webXREnabled() const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    // Message handlers
    void enumerateImmersiveXRDevices(CompletionHandler<void(Vector<XRDeviceInfo>&&)>&&);
    void requestPermissionOnSessionFeatures(IPC::Connection&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&&);
    void initializeTrackingAndRendering(IPC::Connection&, std::optional<WebCore::WebGPU::TextureFormat>, std::optional<WebCore::WebGPU::TextureFormat>);
    void shutDownTrackingAndRendering(IPC::Connection&);
    void requestFrame(IPC::Connection&, std::optional<PlatformXR::RequestData>&&, CompletionHandler<void(PlatformXR::FrameData&&)>&&);
#if USE(OPENXR)
    void createLayerProjection(IPC::Connection&, uint32_t width, uint32_t height, bool alpha, CompletionHandler<void(std::optional<PlatformXR::LayerInfo>)>&&);
    void submitFrame(IPC::Connection&, Vector<PlatformXR::DeviceLayer>&&);
#else
    void submitFrame(IPC::Connection&);
#endif
#if ENABLE(WEBXR_LAYERS)
#if !USE(OPENXR) && !defined(NDEBUG)
#define XR_NORETURN [[noreturn]]
#else
#define XR_NORETURN
#endif
    XR_NORETURN void NODELETE createCompositionLayer(IPC::Connection&, PlatformXR::CompositionLayerType, WebCore::IntSize, PlatformXR::LayerLayout, CompletionHandler<void(std::optional<PlatformXR::LayerInfo>)>&&);
#endif
#if ENABLE(WEBXR_HIT_TEST)
    void requestHitTestSource(const PlatformXR::HitTestOptions&, CompletionHandler<void(Expected<PlatformXR::HitTestSource, WebCore::ExceptionData>)>&&);
    void deleteHitTestSource(PlatformXR::HitTestSource);
    void requestTransientInputHitTestSource(const PlatformXR::TransientInputHitTestOptions&, CompletionHandler<void(Expected<PlatformXR::TransientInputHitTestSource, WebCore::ExceptionData>)>&&);
    void deleteTransientInputHitTestSource(PlatformXR::TransientInputHitTestSource);
#endif
    void didCompleteShutdownTriggeredBySystem(IPC::Connection&);

    // PlatformXRCoordinatorSessionEventClient
    void sessionDidEnd(XRDeviceIdentifier) final;
    void sessionDidUpdateVisibilityState(XRDeviceIdentifier, PlatformXR::VisibilityState) final;

    std::optional<PlatformXR::SessionMode> m_immersiveSessionMode;
    std::optional<WebCore::SecurityOriginData> m_immersiveSessionSecurityOriginData;
    std::optional<PlatformXR::Device::FeatureList> m_immersiveSessionGrantedFeatures;
    enum class ImmersiveSessionState : uint8_t {
        Idle,
        RequestingPermissions,
        PermissionsGranted,
        SessionRunning,
        SessionEndingFromWebContent,
        SessionEndingFromSystem,
    };
    ImmersiveSessionState m_immersiveSessionState { ImmersiveSessionState::Idle };
    void setImmersiveSessionState(ImmersiveSessionState, CompletionHandler<void(bool)>&&);
    void invalidateImmersiveSessionState(ImmersiveSessionState nextSessionState = ImmersiveSessionState::Idle);

    WeakPtr<WebPageProxy> m_page;
    Variant<std::monostate, Ref<ProcessThrottler::ForegroundActivity>, Ref<ProcessActivityGroup>> m_immersiveSessionActivity;
};

} // namespace WebKit

#endif // ENABLE(WEBXR)
