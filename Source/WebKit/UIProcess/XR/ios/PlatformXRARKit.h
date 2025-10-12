/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(WEBXR) && USE(ARKITXR_IOS)

#import "PlatformXRCoordinator.h"

#import <wtf/RetainPtr.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/Threading.h>
#import <wtf/threads/BinarySemaphore.h>

@class ARSession;
@protocol WKARPresentationSession;

namespace WebCore {
struct XRCanvasConfiguration;
}

namespace WebKit {

class ARKitCoordinator final : public PlatformXRCoordinator {
    WTF_MAKE_TZONE_ALLOCATED(ARKitCoordinator);
    struct RenderState;
public:
    ARKitCoordinator();
    virtual ~ARKitCoordinator() = default;

    void getPrimaryDeviceInfo(WebPageProxy&, DeviceInfoCallback&&) override;
    void requestPermissionOnSessionFeatures(WebPageProxy&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, const PlatformXR::Device::FeatureList&, FeatureListCallback&&) override;

    void startSession(WebPageProxy&, WeakPtr<SessionEventClient>&&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&, std::optional<WebCore::XRCanvasConfiguration>&&) override;
    void endSessionIfExists(WebPageProxy&) override;

    void scheduleAnimationFrame(WebPageProxy&, std::optional<PlatformXR::RequestData>&&, PlatformXR::Device::RequestFrameCallback&&) override;
    void submitFrame(WebPageProxy&) override;

protected:
    void createSessionIfNeeded();
    void endSessionIfExists(std::optional<WebCore::PageIdentifier>);
    void renderLoop(Box<RenderState>);

private:
    XRDeviceIdentifier m_deviceIdentifier = XRDeviceIdentifier::generate();
    RetainPtr<ARSession> m_session;

    struct Idle {
    };
    struct Active {
        WeakPtr<PlatformXRCoordinatorSessionEventClient> sessionEventClient;
        WebCore::PageIdentifier pageIdentifier;
        Box<RenderState> renderState;
        RefPtr<Thread> renderThread;
    };

    using State = Variant<Idle, Active>;
    State m_state;
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(ARKITXR_IOS)
