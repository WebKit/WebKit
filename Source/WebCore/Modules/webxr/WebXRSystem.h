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
#include "XRSessionMode.h"
#include <wtf/IsoMalloc.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class ScriptExecutionContext;
class WebXRSession;
struct XRSessionInit;

class WebXRSystem final : public RefCounted<WebXRSystem>, public EventTargetWithInlineData, public ActiveDOMObject, public CanMakeWeakPtr<WebXRSystem> {
    WTF_MAKE_ISO_ALLOCATED(WebXRSystem);
public:
    using IsSessionSupportedPromise = DOMPromiseDeferred<IDLBoolean>;
    using RequestSessionPromise = DOMPromiseDeferred<IDLInterface<WebXRSession>>;

    static Ref<WebXRSystem> create(ScriptExecutionContext&);
    ~WebXRSystem();

    using RefCounted<WebXRSystem>::ref;
    using RefCounted<WebXRSystem>::deref;

    void isSessionSupported(XRSessionMode, IsSessionSupportedPromise&&);
    void requestSession(XRSessionMode, const XRSessionInit&, RequestSessionPromise&&);

    // For testing purpouses only.
    void registerSimulatedXRDeviceForTesting(const PlatformXR::Device&);
    void unregisterSimulatedXRDeviceForTesting(PlatformXR::Device*);

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
    WebXRSystem(ScriptExecutionContext&);

    void ensureImmersiveXRDeviceIsSelected();

    // https://immersive-web.github.io/webxr/#default-inline-xr-device
    class DummyInlineDevice final : public PlatformXR::Device {
    public:
        DummyInlineDevice()
        {
            m_supportedModes.append(XRSessionMode::Inline);
        }
    };
    DummyInlineDevice m_defaultInlineDevice;

    bool m_immersiveXRDevicesHaveBeenEnumerated { false };
    bool m_testingMode { false };

    WeakPtr<WebXRSession> m_activeImmersiveSession;

    WeakPtr<PlatformXR::Device> m_activeImmersiveDevice;
    Vector<WeakPtr<PlatformXR::Device>> m_immersiveDevices;
    WeakPtr<PlatformXR::Device> m_inlineXRDevice;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
