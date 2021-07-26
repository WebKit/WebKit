/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "JSDOMPromiseDeferred.h"
#include "WebXRFrame.h"
#include "WebXRInputSourceArray.h"
#include "WebXRRenderState.h"
#include "XREnvironmentBlendMode.h"
#include "XRInteractionMode.h"
#include "XRReferenceSpaceType.h"
#include "XRSessionMode.h"
#include "XRVisibilityState.h"
#include <wtf/MonotonicTime.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class XRFrameRequestCallback;
class WebXRSystem;
class WebXRView;
class WebXRViewerSpace;
struct XRRenderStateInit;

class WebXRSession final : public RefCounted<WebXRSession>, public EventTargetWithInlineData, public ActiveDOMObject, public PlatformXR::TrackingAndRenderingClient {
    WTF_MAKE_ISO_ALLOCATED(WebXRSession);
public:
    using RequestReferenceSpacePromise = DOMPromiseDeferred<IDLInterface<WebXRReferenceSpace>>;
    using EndPromise = DOMPromiseDeferred<void>;
    using FeatureList = PlatformXR::Device::FeatureList;

    static Ref<WebXRSession> create(Document&, WebXRSystem&, XRSessionMode, PlatformXR::Device&, FeatureList&&);
    virtual ~WebXRSession();

    using RefCounted<WebXRSession>::ref;
    using RefCounted<WebXRSession>::deref;

    using TrackingAndRenderingClient::weakPtrFactory;
    using WeakValueType = TrackingAndRenderingClient::WeakValueType;

    XREnvironmentBlendMode environmentBlendMode() const;
    XRInteractionMode interactionMode() const;
    XRVisibilityState visibilityState() const;
    const WebXRRenderState& renderState() const;
    const WebXRInputSourceArray& inputSources() const;
    PlatformXR::Device* device() const { return m_device.get(); }

    ExceptionOr<void> updateRenderState(const XRRenderStateInit&);
    void requestReferenceSpace(XRReferenceSpaceType, RequestReferenceSpacePromise&&);

    unsigned requestAnimationFrame(Ref<XRFrameRequestCallback>&&);
    void cancelAnimationFrame(unsigned callbackId);

    IntSize nativeWebGLFramebufferResolution() const;
    IntSize recommendedWebGLFramebufferResolution() const;
    bool supportsViewportScaling() const; 
    bool isPositionEmulated() const;

    // EventTarget.
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    ExceptionOr<void> end(EndPromise&&);
    bool ended() const { return m_ended; }

    XRSessionMode mode() const { return m_mode; }

    const Vector<PlatformXR::Device::ViewData>& views() const { return m_views; }
    const PlatformXR::Device::FrameData& frameData() const { return m_frameData; }
    const WebXRViewerSpace& viewerReferenceSpace() const { return *m_viewerReferenceSpace; }
    bool posesCanBeReported(const Document&) const;

private:
    WebXRSession(Document&, WebXRSystem&, XRSessionMode, PlatformXR::Device&, FeatureList&&);

    // EventTarget
    EventTargetInterface eventTargetInterface() const override { return WebXRSessionEventTargetInterfaceType; }
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    // ActiveDOMObject
    const char* activeDOMObjectName() const override;
    void stop() override;

    // PlatformXR::TrackingAndRenderingClient
    void sessionDidInitializeInputSources(Vector<PlatformXR::Device::FrameData::InputSource>&&) final;
    void sessionDidEnd() final;

    enum class InitiatedBySystem : bool { No, Yes };
    void shutdown(InitiatedBySystem);
    void didCompleteShutdown();

    bool referenceSpaceIsSupported(XRReferenceSpaceType) const;

    bool frameShouldBeRendered() const;
    void requestFrame();
    void onFrame(PlatformXR::Device::FrameData&&);
    void applyPendingRenderState();

    XREnvironmentBlendMode m_environmentBlendMode;
    XRInteractionMode m_interactionMode;
    XRVisibilityState m_visibilityState { XRVisibilityState::Visible };
    UniqueRef<WebXRInputSourceArray> m_inputSources;
    bool m_ended { false };
    std::optional<EndPromise> m_endPromise;

    WebXRSystem& m_xrSystem;
    XRSessionMode m_mode;
    WeakPtr<PlatformXR::Device> m_device;
    FeatureList m_requestedFeatures;
    RefPtr<WebXRRenderState> m_activeRenderState;
    RefPtr<WebXRRenderState> m_pendingRenderState;
    std::unique_ptr<WebXRViewerSpace> m_viewerReferenceSpace;
    MonotonicTime m_timeOrigin;

    unsigned m_nextCallbackId { 1 };
    Vector<Ref<XRFrameRequestCallback>> m_callbacks;

    Vector<PlatformXR::Device::ViewData> m_views;
    PlatformXR::Device::FrameData m_frameData;

    double m_minimumInlineFOV { 0.0 };
    double m_maximumInlineFOV { piFloat };

    // In meters.
    double m_minimumNearClipPlane { 0.1 };
    double m_maximumFarClipPlane { 1000.0 };

    // https://immersive-web.github.io/webxr/#xrsession-promise-resolved
    bool m_inputInitialized { false };
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
