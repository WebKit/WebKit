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

#include "EventTarget.h"
#include "JSDOMPromiseDeferredForward.h"
#include "WebFakeXRDevice.h"
#include "XRSessionMode.h"
#include "XRSimulateUserActivationFunction.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Document;
class WebXRSystem;

class WebXRTest final : public RefCounted<WebXRTest> {
public:
    struct FakeXRDeviceInit {
        bool supportsImmersive { false };
        std::optional<Vector<XRSessionMode>> supportedModes;
        Vector<FakeXRViewInit> views;

        std::optional<Vector<JSC::JSValue>> supportedFeatures;
        std::optional<Vector<JSC::JSValue>> enabledFeatures;

        std::optional<Vector<FakeXRBoundsPoint>> boundsCoordinates;

        std::optional<FakeXRRigidTransformInit> floorOrigin;
        std::optional<FakeXRRigidTransformInit> viewerOrigin;
    };

    static Ref<WebXRTest> create(WeakPtr<WebXRSystem, WeakPtrImplWithEventTargetData>&& system) { return adoptRef(*new WebXRTest(WTFMove(system))); }
    virtual ~WebXRTest();

    using WebFakeXRDevicePromise = DOMPromiseDeferred<IDLInterface<WebFakeXRDevice>>;
    void simulateDeviceConnection(ScriptExecutionContext& state, const FakeXRDeviceInit&, WebFakeXRDevicePromise&&);

    // Simulates a user activation (aka user gesture) for the current scope.
    // The activation is only guaranteed to be valid in the provided function and only applies to WebXR
    // Device API methods.
    void simulateUserActivation(Document&, XRSimulateUserActivationFunction&);

    // Disconnect all fake devices
    void disconnectAllDevices(DOMPromiseDeferred<void>&&);

private:
    WebXRTest(WeakPtr<WebXRSystem, WeakPtrImplWithEventTargetData>&&);

    WeakPtr<WebXRSystem, WeakPtrImplWithEventTargetData> m_context;
    Vector<Ref<WebFakeXRDevice>> m_devices;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
