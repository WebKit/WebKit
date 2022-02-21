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

#include "XRDeviceIdentifier.h"
#include "XRDeviceInfo.h"
#include <WebCore/PlatformXR.h>
#include <wtf/Function.h>

namespace WebCore {
struct SecurityOriginData;
}

namespace WebKit {

class WebPageProxy;

class PlatformXRCoordinator {
public:
    virtual ~PlatformXRCoordinator() = default;

    // FIXME: Temporary and will be fixed later.
    static PlatformXR::LayerHandle defaultLayerHandle() { return 1; }

    using DeviceInfoCallback = Function<void(std::optional<XRDeviceInfo>)>;
    virtual void getPrimaryDeviceInfo(DeviceInfoCallback&&) = 0;

    using FeatureListCallback = CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>;
    virtual void requestPermissionOnSessionFeatures(WebPageProxy&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& /* consentRequired */, const PlatformXR::Device::FeatureList& /* consentOptional */, FeatureListCallback&& completionHandler) { completionHandler(granted); }

    class SessionEventClient : public CanMakeWeakPtr<SessionEventClient> {
    public:
        virtual ~SessionEventClient() = default;

        virtual void sessionDidEnd(XRDeviceIdentifier) = 0;
        virtual void sessionDidUpdateVisibilityState(XRDeviceIdentifier, PlatformXR::VisibilityState) = 0;
    };

    // Session creation/termination.
    virtual void startSession(WebPageProxy&, WeakPtr<SessionEventClient>&&) = 0;
    virtual void endSessionIfExists(WebPageProxy&) = 0;

    // Session display loop.
    virtual void scheduleAnimationFrame(WebPageProxy&, PlatformXR::Device::RequestFrameCallback&&) = 0;
    virtual void submitFrame(WebPageProxy&) { }
};

} // namespace WebKit

#endif // ENABLE(WEBXR)
