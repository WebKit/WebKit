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
#include "XRDeviceIdentifier.h"
#include "XRDeviceProxy.h"
#include <WebCore/PlatformXR.h>

namespace WebKit {
class PlatformXRSystemProxy;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::PlatformXRSystemProxy> : std::true_type { };
}

namespace WebCore {
class SecurityOriginData;
}

namespace WebKit {

class WebPage;

class PlatformXRSystemProxy : public IPC::MessageReceiver {
public:
    PlatformXRSystemProxy(WebPage&);
    virtual ~PlatformXRSystemProxy();

    void enumerateImmersiveXRDevices(CompletionHandler<void(const PlatformXR::Instance::DeviceList&)>&&);
    void requestPermissionOnSessionFeatures(const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList& /* granted */, const PlatformXR::Device::FeatureList& /* consentRequired */, const PlatformXR::Device::FeatureList& /* consentOptional */, const PlatformXR::Device::FeatureList& /* requiredFeaturesRequested */, const PlatformXR::Device::FeatureList& /* optionalFeaturesRequested */,  CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&&);
    void initializeTrackingAndRendering();
    void shutDownTrackingAndRendering();
    void didCompleteShutdownTriggeredBySystem();
    void requestFrame(PlatformXR::Device::RequestFrameCallback&&);
    std::optional<PlatformXR::LayerHandle> createLayerProjection(uint32_t, uint32_t, bool);
    void submitFrame();

private:
    RefPtr<XRDeviceProxy> deviceByIdentifier(XRDeviceIdentifier);
    bool webXREnabled() const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Message handlers
    void sessionDidEnd(XRDeviceIdentifier);
    void sessionDidUpdateVisibilityState(XRDeviceIdentifier, PlatformXR::VisibilityState);

    PlatformXR::Instance::DeviceList m_devices;
    WebPage& m_page;
};

}

#endif // ENABLE(WEBXR)
