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
#include "ProcessThrottler.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/PlatformXR.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {
class PlatformXRSystem;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::PlatformXRSystem> : std::true_type { };
}

namespace WebCore {
class SecurityOriginData;
}

namespace WebKit {

class PlatformXRCoordinator;
class WebPageProxy;

struct SharedPreferencesForWebProcess;
struct XRDeviceInfo;

class PlatformXRSystem : public IPC::MessageReceiver, public PlatformXRCoordinatorSessionEventClient {
    WTF_MAKE_TZONE_ALLOCATED(PlatformXRSystem);
public:
    PlatformXRSystem(WebPageProxy&);
    virtual ~PlatformXRSystem();

    const SharedPreferencesForWebProcess& sharedPreferencesForWebProcess() const;

    using PlatformXRCoordinatorSessionEventClient::weakPtrFactory;
    using PlatformXRCoordinatorSessionEventClient::WeakValueType;
    using PlatformXRCoordinatorSessionEventClient::WeakPtrImplType;

    void invalidate();

    bool hasActiveSession() const { return !!m_immersiveSessionActivity; }
    void ensureImmersiveSessionActivity();

private:
    static PlatformXRCoordinator* xrCoordinator();

    bool webXREnabled() const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Message handlers
    void enumerateImmersiveXRDevices(CompletionHandler<void(Vector<XRDeviceInfo>&&)>&&);
    void requestPermissionOnSessionFeatures(const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&&);
    void initializeTrackingAndRendering();
    void shutDownTrackingAndRendering();
    void requestFrame(CompletionHandler<void(PlatformXR::FrameData&&)>&&);
    void submitFrame();
    void didCompleteShutdownTriggeredBySystem();

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

    WebPageProxy& m_page;
    std::unique_ptr<ProcessThrottler::ForegroundActivity> m_immersiveSessionActivity;
};

} // namespace WebKit

#endif // ENABLE(WEBXR)
