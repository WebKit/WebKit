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

#include "config.h"
#include "WebXRSystem.h"

#if ENABLE(WEBXR)

#include "Document.h"
#include "FeaturePolicy.h"
#include "PlatformXR.h"
#include "RuntimeEnabledFeatures.h"
#include "WebXRSession.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRSystem);

Ref<WebXRSystem> WebXRSystem::create(ScriptExecutionContext& scriptExecutionContext)
{
    return adoptRef(*new WebXRSystem(scriptExecutionContext));
}

WebXRSystem::WebXRSystem(ScriptExecutionContext& scriptExecutionContext)
    : ActiveDOMObject(&scriptExecutionContext)
{
    m_inlineXRDevice = makeWeakPtr(m_defaultInlineDevice);
}

WebXRSystem::~WebXRSystem() = default;

// https://immersive-web.github.io/webxr/#ensures-an-immersive-xr-device-is-selected
void WebXRSystem::ensureImmersiveXRDeviceIsSelected()
{
    // Don't ask platform code for XR devices, we're using simulated ones.
    // TODO: should be have a MockPlatformXR implementation instead ?
    if (UNLIKELY(m_testingMode))
        return;

    if (m_activeImmersiveDevice)
        return;

    // https://immersive-web.github.io/webxr/#enumerate-immersive-xr-devices
    auto& platformXR = PlatformXR::Instance::singleton();
    bool isFirstXRDevicesEnumeration = !m_immersiveXRDevicesHaveBeenEnumerated;
    platformXR.enumerateImmersiveXRDevices();
    m_immersiveXRDevicesHaveBeenEnumerated = true;
    const Vector<std::unique_ptr<PlatformXR::Device>>& immersiveXRDevices = platformXR.immersiveXRDevices();

    // https://immersive-web.github.io/webxr/#select-an-immersive-xr-device
    auto* oldDevice = m_activeImmersiveDevice.get();
    if (immersiveXRDevices.isEmpty()) {
        m_activeImmersiveDevice = nullptr;
        return;
    }
    if (immersiveXRDevices.size() == 1) {
        m_activeImmersiveDevice = makeWeakPtr(immersiveXRDevices.first().get());
        return;
    }

    if (m_activeImmersiveSession && oldDevice && immersiveXRDevices.findMatching([&] (auto& entry) { return entry.get() == oldDevice; }) != notFound)
        ASSERT(m_activeImmersiveDevice.get() == oldDevice);
    else {
        // TODO: implement a better UA selection mechanism if required.
        m_activeImmersiveDevice = makeWeakPtr(immersiveXRDevices.first().get());
    }

    if (isFirstXRDevicesEnumeration || m_activeImmersiveDevice.get() == oldDevice)
        return;

    // TODO: 7. Shut down any active XRSessions.
    // TODO: 8. Set the XR compatible boolean of all WebGLRenderingContextBase instances to false.
    // TODO: 9. Queue a task to fire an event named devicechange on the context object.
}

// https://immersive-web.github.io/webxr/#dom-xrsystem-issessionsupported
void WebXRSystem::isSessionSupported(XRSessionMode mode, IsSessionSupportedPromise&& promise)
{
    // 1. Let promise be a new Promise.
    // 2. If mode is "inline", resolve promise with true and return it.
    if (mode == XRSessionMode::Inline) {
        promise.resolve(true);
        return;
    }

    // 3. If the requesting document’s origin is not allowed to use the "xr-spatial-tracking" feature policy,
    //    reject promise with a "SecurityError" DOMException and return it.
    auto document = downcast<Document>(scriptExecutionContext());
    if (!isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::XRSpatialTracking, *document, LogFeaturePolicyFailure::Yes)) {
        promise.reject(Exception { SecurityError });
        return;
    }

    // 4. Run the following steps in parallel:
    scriptExecutionContext()->postTask([this, promise = WTFMove(promise), mode] (ScriptExecutionContext&) mutable {
        // 4.1 Ensure an immersive XR device is selected.
        ensureImmersiveXRDeviceIsSelected();

        // 4.2 If the immersive XR device is null, resolve promise with false and abort these steps.
        if (!m_activeImmersiveDevice) {
            promise.resolve(false);
            return;
        }

        // 4.3 If the immersive XR device's list of supported modes does not contain mode, resolve promise with false and abort these steps.
        if (!m_activeImmersiveDevice->supports(mode)) {
            promise.resolve(false);
            return;
        }

        // 4.4 Resolve promise with true.
        promise.resolve(true);
    });
}

void WebXRSystem::requestSession(XRSessionMode, const XRSessionInit&, RequestSessionPromise&&)
{
    PlatformXR::Instance::singleton();

    // When the requestSession(mode) method is invoked, the user agent MUST return a new Promise promise and run the following steps in parallel:
    //   1. Let immersive be true if mode is "immersive-vr" or "immersive-ar", and false otherwise.
    //   2. If immersive is true:
    //     1. If pending immersive session is true or active immersive session is not null, reject promise with an "InvalidStateError" DOMException and abort these steps.
    //     2. Else set pending immersive session to be true.
    //   3. Ensure an XR device is selected.
    //   4. If the XR device is null, reject promise with null.
    //   5. Else if the XR device's list of supported modes does not contain mode, reject promise with a "NotSupportedError" DOMException.
    //   6. Else If immersive is true and the algorithm is not triggered by user activation, reject promise with a "SecurityError" DOMException and abort these steps.
    //   7. If promise was rejected and immersive is true, set pending immersive session to false.
    //   8. If promise was rejected, abort these steps.
    //   9. Let session be a new XRSession object.
    //   10. Initialize the session with session and mode.
    //   11. If immersive is true, set the active immersive session to session, and set pending immersive session to false.
    //   12. Else append session to the list of inline sessions.
    //   13. Resolve promise with session.
}

const char* WebXRSystem::activeDOMObjectName() const
{
    return "XRSystem";
}

void WebXRSystem::stop()
{
}

void WebXRSystem::registerSimulatedXRDeviceForTesting(const PlatformXR::Device& device)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().webXREnabled())
        return;
    m_testingMode = true;
    if (device.supports(PlatformXR::SessionMode::ImmersiveVr) || device.supports(PlatformXR::SessionMode::ImmersiveAr)) {
        m_immersiveDevices.append(makeWeakPtr(device));
        m_activeImmersiveDevice = m_immersiveDevices.last();
    }
    if (device.supports(PlatformXR::SessionMode::Inline))
        m_inlineXRDevice = makeWeakPtr(device);
}

void WebXRSystem::unregisterSimulatedXRDeviceForTesting(PlatformXR::Device* device)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().webXREnabled())
        return;
    ASSERT(m_immersiveDevices.contains(device));
    m_immersiveDevices.removeFirst(device);
    if (m_activeImmersiveDevice == device)
        m_activeImmersiveDevice = nullptr;
    if (m_inlineXRDevice == device)
        m_inlineXRDevice = makeWeakPtr(m_defaultInlineDevice);
    m_testingMode = false;
}


} // namespace WebCore

#endif // ENABLE(WEBXR)
