/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "JSDOMPromiseDeferredForward.h"
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
class WebCoreOpaqueRoot;
class WebXRSystem;
class WebXRView;
class WebXRViewerSpace;
struct XRRenderStateInit;

class WebXRSession final : public RefCounted<WebXRSession>, public EventTarget, public ActiveDOMObject, public PlatformXR::TrackingAndRenderingClient {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebXRSession);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    USING_CAN_MAKE_WEAKPTR(PlatformXR::TrackingAndRenderingClient);

    using RequestReferenceSpacePromise = DOMPromiseDeferred<IDLInterface<WebXRReferenceSpace>>;
    using EndPromise = DOMPromiseDeferred<void>;
    using FeatureList = PlatformXR::Device::FeatureList;

    static Ref<WebXRSession> create(Document&, WebXRSystem&, XRSessionMode, PlatformXR::Device&, FeatureList&&);
    virtual ~WebXRSession();

    XREnvironmentBlendMode environmentBlendMode() const;
    XRInteractionMode interactionMode() const;
    XRVisibilityState visibilityState() const;
    const WebXRRenderState& renderState() const;
    const WebXRInputSourceArray& inputSources() const;
    RefPtr<PlatformXR::Device> device() const { return m_device.get(); }

    const Vector<String> enabledFeatures() const;

    ExceptionOr<void> updateRenderState(const XRRenderStateInit&);
    void requestReferenceSpace(XRReferenceSpaceType, RequestReferenceSpacePromise&&);

    unsigned requestAnimationFrame(Ref<XRFrameRequestCallback>&&);
    void cancelAnimationFrame(unsigned callbackId);

    IntSize nativeWebGLFramebufferResolution() const;
    IntSize recommendedWebGLFramebufferResolution() const;
    bool supportsViewportScaling() const; 
    bool isPositionEmulated() const;

    // If the immersive session obscures the HTML document (for example, in standalone devices),
    // Page::updateRendering() won't be called and the WebXRSession needs to take over the
    // responsibility to service requestVideoFrameCallbacks.
    void applicationDidEnterBackground() { m_shouldServiceRequestVideoFrameCallbacks = true; }
    void applicationWillEnterForeground() { m_shouldServiceRequestVideoFrameCallbacks = false; }

    // EventTarget.
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    ExceptionOr<void> end(EndPromise&&);
    bool ended() const { return m_ended; }

    XRSessionMode mode() const { return m_mode; }

    const Vector<PlatformXR::Device::ViewData>& views() const { return m_views; }
    const PlatformXR::FrameData& frameData() const { return m_frameData; }
    const WebXRViewerSpace& viewerReferenceSpace() const { return m_viewerReferenceSpace; }
    bool posesCanBeReported(const Document&) const;
    
#if ENABLE(WEBXR_HANDS)
    bool isHandTrackingEnabled() const;
#endif

private:
    WebXRSession(Document&, WebXRSystem&, XRSessionMode, PlatformXR::Device&, FeatureList&&);

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const override { return EventTargetInterfaceType::WebXRSession; }
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    // ActiveDOMObject.
    void stop() override;

    // PlatformXR::TrackingAndRenderingClient
    void sessionDidInitializeInputSources(Vector<PlatformXR::FrameData::InputSource>&&) final;
    void sessionDidEnd() final;
    void updateSessionVisibilityState(PlatformXR::VisibilityState) final;

    enum class InitiatedBySystem : bool { No, Yes };
    void shutdown(InitiatedBySystem);
    void didCompleteShutdown();

    bool referenceSpaceIsSupported(XRReferenceSpaceType) const;

    bool frameShouldBeRendered() const;
    void requestFrameIfNeeded();
    void onFrame(PlatformXR::FrameData&&);
    void applyPendingRenderState();
    void minimalUpdateRendering();

    XREnvironmentBlendMode m_environmentBlendMode { XREnvironmentBlendMode::Opaque };
    XRInteractionMode m_interactionMode { XRInteractionMode::WorldSpace };
    XRVisibilityState m_visibilityState { XRVisibilityState::Visible };
    UniqueRef<WebXRInputSourceArray> m_inputSources;
    bool m_ended { false };
    bool m_shouldServiceRequestVideoFrameCallbacks { false };
    std::unique_ptr<EndPromise> m_endPromise;

    WebXRSystem& m_xrSystem;
    XRSessionMode m_mode;
    ThreadSafeWeakPtr<PlatformXR::Device> m_device;
    FeatureList m_requestedFeatures;
    RefPtr<WebXRRenderState> m_activeRenderState;
    RefPtr<WebXRRenderState> m_pendingRenderState;
    Ref<WebXRViewerSpace> m_viewerReferenceSpace;
    MonotonicTime m_timeOrigin;

    unsigned m_nextCallbackId { 1 };
    Vector<Ref<XRFrameRequestCallback>> m_callbacks;
    bool m_isDeviceFrameRequestPending { false };

    Vector<PlatformXR::Device::ViewData> m_views;
    PlatformXR::FrameData m_frameData;
    std::optional<PlatformXR::RequestData> m_requestData;

    double m_minimumInlineFOV { 0.0 };
    double m_maximumInlineFOV { piFloat };

    // In meters.
    double m_minimumNearClipPlane { 0.1 };
    double m_maximumFarClipPlane { 1000.0 };

    // https://immersive-web.github.io/webxr/#xrsession-promise-resolved
    bool m_inputInitialized { false };
};

WebCoreOpaqueRoot root(WebXRSession*);

} // namespace WebCore

#endif // ENABLE(WEBXR)
