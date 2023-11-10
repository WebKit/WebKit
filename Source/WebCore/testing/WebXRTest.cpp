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
#include "WebXRTest.h"

#if ENABLE(WEBXR)

#include "JSDOMPromiseDeferred.h"
#include "JSWebFakeXRDevice.h"
#include "JSXRReferenceSpaceType.h"
#include "PlatformXR.h"
#include "UserGestureIndicator.h"
#include "WebXRSystem.h"
#include "XRSessionMode.h"

namespace WebCore {

WebXRTest::~WebXRTest() = default;

static PlatformXR::Device::FeatureList parseFeatures(const Vector<JSC::JSValue>& featureList, ScriptExecutionContext& context)
{
    PlatformXR::Device::FeatureList features;
    if (auto* globalObject = context.globalObject()) {
        for (auto& feature : featureList) {
            auto featureString = feature.toWTFString(globalObject);
            if (auto sessionFeature = PlatformXR::parseSessionFeatureDescriptor(featureString))
                features.append(*sessionFeature);
        }
    }
    return features;
}

void WebXRTest::simulateDeviceConnection(ScriptExecutionContext& context, const FakeXRDeviceInit& init, WebFakeXRDevicePromise&& promise)
{
    // https://immersive-web.github.io/webxr-test-api/#dom-xrtest-simulatedeviceconnection
    context.postTask([this, protectedThis = Ref { *this }, init, promise = WTFMove(promise)] (ScriptExecutionContext& context) mutable {
        auto device = WebFakeXRDevice::create();
        auto& simulatedDevice = device->simulatedXRDevice();

        device->setViews(init.views);

        PlatformXR::Device::FeatureList supportedFeatures;
        if (init.supportedFeatures)
            supportedFeatures = parseFeatures(init.supportedFeatures.value(), context);
        PlatformXR::Device::FeatureList enabledFeatures;
        if (init.enabledFeatures)
            enabledFeatures = parseFeatures(init.enabledFeatures.value(), context);

        if (init.boundsCoordinates) {
            if (init.boundsCoordinates->size() < 3) {
                promise.reject(Exception { ExceptionCode::TypeError });
                return;
            }
            simulatedDevice.setNativeBoundsGeometry(init.boundsCoordinates.value());
        }

        if (init.viewerOrigin)
            device->setViewerOrigin(init.viewerOrigin.value());

        if (init.floorOrigin)
            device->setFloorOrigin(init.floorOrigin.value());

        Vector<XRSessionMode> supportedModes;
        if (init.supportedModes) {
            supportedModes = init.supportedModes.value();
            if (supportedModes.isEmpty())
                supportedModes.append(XRSessionMode::Inline);
        } else {
            supportedModes.append(XRSessionMode::Inline);
            if (init.supportsImmersive)
                supportedModes.append(XRSessionMode::ImmersiveVr);
        }

        for (auto& mode : supportedModes) {
            simulatedDevice.setSupportedFeatures(mode, supportedFeatures);
            simulatedDevice.setEnabledFeatures(mode, enabledFeatures);
        }

        m_context->registerSimulatedXRDeviceForTesting(simulatedDevice);

        promise.resolve(device.get());
        m_devices.append(WTFMove(device));
    });
}

void WebXRTest::simulateUserActivation(Document& document, XRSimulateUserActivationFunction& function)
{
    // https://immersive-web.github.io/webxr-test-api/#dom-xrtest-simulateuseractivation
    // Invoke function as if it had transient activation.
    UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, &document);
    function.handleEvent();
}

void WebXRTest::disconnectAllDevices(DOMPromiseDeferred<void>&& promise)
{
    for (auto& device : m_devices)
        m_context->unregisterSimulatedXRDeviceForTesting(device->simulatedXRDevice());
    m_devices.clear();
    promise.resolve();
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
