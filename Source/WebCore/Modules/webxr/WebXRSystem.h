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
#include "HTMLCanvasElement.h"
#include "JSDOMPromiseDeferredForward.h"
#include "PlatformXR.h"
#include "WebGLContextAttributes.h"
#include "WebGLRenderingContextBase.h"
#include "XRReferenceSpaceType.h"
#include "XRSessionMode.h"
#include <wtf/HashSet.h>
#include <wtf/IsoMalloc.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeWeakHashSet.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace JSC {
class JSGlobalObject;
}

namespace WebCore {

class LocalDOMWindow;
class Navigator;
class ScriptExecutionContext;
class WebXRSession;
class SecurityOriginData;
struct XRSessionInit;

class WebXRSystem final : public RefCounted<WebXRSystem>, public EventTarget, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(WebXRSystem);
public:
    using IsSessionSupportedPromise = DOMPromiseDeferred<IDLBoolean>;
    using RequestSessionPromise = DOMPromiseDeferred<IDLInterface<WebXRSession>>;

    static Ref<WebXRSystem> create(Navigator&);
    ~WebXRSystem();

    using RefCounted<WebXRSystem>::ref;
    using RefCounted<WebXRSystem>::deref;

    void isSessionSupported(XRSessionMode, IsSessionSupportedPromise&&);
    void requestSession(Document&, XRSessionMode, const XRSessionInit&, RequestSessionPromise&&);

    // This is also needed by WebGLRenderingContextBase::makeXRCompatible() and HTMLCanvasElement::createContextWebGL().
    void ensureImmersiveXRDeviceIsSelected(CompletionHandler<void()>&&);
    bool hasActiveImmersiveXRDevice() { return !!m_activeImmersiveDevice.get(); }

    void sessionEnded(WebXRSession&);

    // For testing purpouses only.
    WEBCORE_EXPORT void registerSimulatedXRDeviceForTesting(PlatformXR::Device&);
    WEBCORE_EXPORT void unregisterSimulatedXRDeviceForTesting(PlatformXR::Device&);

    Navigator* navigator();

protected:
    // EventTarget
    EventTargetInterface eventTargetInterface() const override { return WebXRSystemEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    // ActiveDOMObject
    const char* activeDOMObjectName() const override;
    void stop() override;

private:
    WebXRSystem(Navigator&);

    using FeatureList = PlatformXR::Device::FeatureList;
    using JSFeatureList = Vector<JSC::JSValue>;
    void obtainCurrentDevice(XRSessionMode, const JSFeatureList& requiredFeatures, const JSFeatureList& optionalFeatures, CompletionHandler<void(ThreadSafeWeakPtr<PlatformXR::Device>)>&&);

    bool immersiveSessionRequestIsAllowedForGlobalObject(LocalDOMWindow&, Document&) const;
    bool inlineSessionRequestIsAllowedForGlobalObject(LocalDOMWindow&, Document&, const XRSessionInit&) const;

    bool isFeatureSupported(PlatformXR::SessionFeature, XRSessionMode, const PlatformXR::Device&) const;
    struct ResolvedRequestedFeatures;
    std::optional<ResolvedRequestedFeatures> resolveRequestedFeatures(XRSessionMode, const XRSessionInit&, RefPtr<PlatformXR::Device>, JSC::JSGlobalObject&) const;
    void resolveFeaturePermissions(XRSessionMode, const XRSessionInit&, RefPtr<PlatformXR::Device>, JSC::JSGlobalObject&, CompletionHandler<void(std::optional<FeatureList>&&)>&&) const;

    // https://immersive-web.github.io/webxr/#default-inline-xr-device
    class DummyInlineDevice final : public PlatformXR::Device, public ContextDestructionObserver {
    public:
        static Ref<DummyInlineDevice> create(ScriptExecutionContext&);
        virtual ~DummyInlineDevice() = default;

    private:
        DummyInlineDevice(ScriptExecutionContext&);

        void initializeTrackingAndRendering(const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList&) final { }
        void shutDownTrackingAndRendering() final { }
        void initializeReferenceSpace(PlatformXR::ReferenceSpaceType) final { }

        void requestFrame(PlatformXR::Device::RequestFrameCallback&&) final;
        Vector<Device::ViewData> views(XRSessionMode) const final;
        std::optional<PlatformXR::LayerHandle> createLayerProjection(uint32_t, uint32_t, bool) final { return std::nullopt; }
        void deleteLayer(PlatformXR::LayerHandle) final { }
    };

    WeakPtr<Navigator> m_navigator;
    Ref<PlatformXR::Device> m_defaultInlineDevice;

    bool m_immersiveXRDevicesHaveBeenEnumerated { false };
    uint m_testingDevices { 0 };

    bool m_pendingImmersiveSession { false };
    RefPtr<WebXRSession> m_activeImmersiveSession;

    // We use a set here although the specs talk about a list of inline sessions
    // https://immersive-web.github.io/webxr/#list-of-inline-sessions.
    HashSet<Ref<WebXRSession>> m_inlineSessions;

    ThreadSafeWeakPtr<PlatformXR::Device> m_activeImmersiveDevice;
    ThreadSafeWeakHashSet<PlatformXR::Device> m_immersiveDevices;
    ThreadSafeWeakPtr<PlatformXR::Device> m_inlineXRDevice;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
