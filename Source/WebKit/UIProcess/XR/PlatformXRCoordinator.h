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
#include <WebCore/ExceptionData.h>
#include <WebCore/ExceptionOr.h>
#include <WebCore/IntSize.h>
#include <WebCore/PlatformXR.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>

namespace WebCore {
class SecurityOriginData;

struct XRCanvasConfiguration;
}

namespace WebKit {

class WebPageProxy;

class PlatformXRCoordinatorSessionEventClient : public AbstractRefCountedAndCanMakeWeakPtr<PlatformXRCoordinatorSessionEventClient> {
public:
    virtual ~PlatformXRCoordinatorSessionEventClient() = default;

    virtual void sessionDidEnd(XRDeviceIdentifier) = 0;
    virtual void sessionDidUpdateVisibilityState(XRDeviceIdentifier, PlatformXR::VisibilityState) = 0;
    virtual void sessionDidInitializeRendering(XRDeviceIdentifier, uint32_t width, uint32_t height, uint32_t arrayLength) = 0;
};

class PlatformXRCoordinator {
public:
    virtual ~PlatformXRCoordinator() = default;

    // FIXME: Temporary and will be fixed later.
    static PlatformXR::LayerHandle defaultLayerHandle() { return 1; }

    using DeviceInfoCallback = Function<void(std::optional<XRDeviceInfo>)>;
    virtual void getPrimaryDeviceInfo(WebPageProxy&, DeviceInfoCallback&&) = 0;

    using FeatureListCallback = CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>;
    virtual void requestPermissionOnSessionFeatures(WebPageProxy&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& /* consentRequired */, const PlatformXR::Device::FeatureList& /* consentOptional */, const PlatformXR::Device::FeatureList& /* requiredFeaturesRequested */, const PlatformXR::Device::FeatureList& /* optionalFeaturesRequested */, FeatureListCallback&& completionHandler) { completionHandler(granted); }

#if USE(OPENXR)
    using CreateLayerProjectionCallback = CompletionHandler<void(std::optional<PlatformXR::LayerInfo>)>;
    virtual void createLayerProjection(uint32_t width, uint32_t height, bool alpha, CreateLayerProjectionCallback&&) = 0;
#endif

#if ENABLE(WEBXR_LAYERS)
    using CreateCompositionLayerCallback = CompletionHandler<void(std::optional<PlatformXR::LayerInfo>)>;
    virtual void createCompositionLayer(PlatformXR::CompositionLayerType, WebCore::IntSize, PlatformXR::LayerLayout, CreateCompositionLayerCallback&&) = 0;
#endif

    // Session creation/termination.
    virtual void startSession(WebPageProxy&, WeakPtr<PlatformXRCoordinatorSessionEventClient>&&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&, std::optional<WebCore::XRCanvasConfiguration>&&) = 0;
    virtual void endSessionIfExists(WebPageProxy&) = 0;

    virtual void stopWhenIdle() { }

    // Session display loop.
    virtual void scheduleAnimationFrame(WebPageProxy&, std::optional<PlatformXR::RequestData>&&, PlatformXR::Device::RequestFrameCallback&&) = 0;
#if USE(OPENXR)
    virtual void submitFrame(WebPageProxy&, Vector<PlatformXR::DeviceLayer>&&) = 0;
#else
    virtual void submitFrame(WebPageProxy&) { }
#endif

#if ENABLE(WEBXR_HIT_TEST)
    virtual void requestHitTestSource(WebPageProxy&, const PlatformXR::HitTestOptions&, CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::HitTestSource>)>&& completionHandler) { completionHandler(WebCore::Exception { WebCore::ExceptionCode::InvalidStateError }); }
    CompletionHandlerCalledToken requestHitTestSource(WebPageProxy& page, const PlatformXR::HitTestOptions& options, CompletionHandler<void(Expected<PlatformXR::HitTestSource, WebCore::ExceptionData>), true>&& handler)
    {
        return CompletionHandlerCalledToken::deferUnchecked(handler, [&](auto& handler, auto deferred) -> CompletionHandlerCalledToken {
            requestHitTestSource(page, options, [handler = WTF::move(handler)](WebCore::ExceptionOr<PlatformXR::HitTestSource> exceptionOrValue) mutable {
                if (exceptionOrValue.hasException()) {
                    auto exception = exceptionOrValue.releaseException();
                    handler(makeUnexpected(WebCore::ExceptionData { exception.code(), exception.releaseMessage() }));
                    return;
                }
                handler(exceptionOrValue.releaseReturnValue());
            });
            return WTF::move(deferred);
        });
    }
    virtual void deleteHitTestSource(WebPageProxy&, PlatformXR::HitTestSource) { }
    virtual void requestTransientInputHitTestSource(WebPageProxy&, const PlatformXR::TransientInputHitTestOptions&, CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::TransientInputHitTestSource>)>&& completionHandler) { completionHandler(WebCore::Exception { WebCore::ExceptionCode::InvalidStateError }); }
    CompletionHandlerCalledToken requestTransientInputHitTestSource(WebPageProxy& page, const PlatformXR::TransientInputHitTestOptions& options, CompletionHandler<void(Expected<PlatformXR::TransientInputHitTestSource, WebCore::ExceptionData>), true>&& handler)
    {
        return CompletionHandlerCalledToken::deferUnchecked(handler, [&](auto& handler, auto deferred) -> CompletionHandlerCalledToken {
            requestTransientInputHitTestSource(page, options, [handler = WTF::move(handler)](WebCore::ExceptionOr<PlatformXR::TransientInputHitTestSource> exceptionOrValue) mutable {
                if (exceptionOrValue.hasException()) {
                    auto exception = exceptionOrValue.releaseException();
                    handler(makeUnexpected(WebCore::ExceptionData { exception.code(), exception.releaseMessage() }));
                    return;
                }
                handler(exceptionOrValue.releaseReturnValue());
            });
            return WTF::move(deferred);
        });
    }
    virtual void deleteTransientInputHitTestSource(WebPageProxy&, PlatformXR::TransientInputHitTestSource) { }
#endif
};

} // namespace WebKit

#endif // ENABLE(WEBXR)
