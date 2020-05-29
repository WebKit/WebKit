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

#include "DOMWindow.h"
#include "Document.h"
#include "FeaturePolicy.h"
#include "IDLTypes.h"
#include "JSWebXRSession.h"
#include "JSXRReferenceSpaceType.h"
#include "PlatformXR.h"
#include "RuntimeEnabledFeatures.h"
#include "SecurityOrigin.h"
#include "UserGestureIndicator.h"
#include "WebXRSession.h"
#include "XRReferenceSpaceType.h"
#include "XRSessionInit.h"
#include <JavaScriptCore/JSGlobalObject.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Scope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRSystem);

WebXRSystem::DummyInlineDevice::DummyInlineDevice()
{
    setEnabledFeatures(XRSessionMode::Inline, { XRReferenceSpaceType::Viewer });
}

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
    if (UNLIKELY(m_testingDevices))
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


PlatformXR::Device* WebXRSystem::obtainCurrentDevice(XRSessionMode mode, const JSFeaturesArray& requiredFeatures, const JSFeaturesArray& optionalFeatures)
{
    if (mode == XRSessionMode::ImmersiveAr || mode == XRSessionMode::ImmersiveVr) {
        ensureImmersiveXRDeviceIsSelected();
        return m_activeImmersiveDevice.get();
    }
    if (!requiredFeatures.isEmpty() || !optionalFeatures.isEmpty())
        return m_inlineXRDevice.get();

    return &m_defaultInlineDevice;
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

    // 3. If the requesting document's origin is not allowed to use the "xr-spatial-tracking" feature policy,
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

// https://immersive-web.github.io/webxr/#immersive-session-request-is-allowed
bool WebXRSystem::immersiveSessionRequestIsAllowedForGlobalObject(DOMWindow& globalObject, Document& document) const
{
    // 1. If the request was not made while the global object has transient
    //    activation or when launching a web application, return false
    // TODO: add a check for "not launching a web application".
    if (!globalObject.hasTransientActivation())
        return false;

    // 2. If the requesting document is not considered trustworthy, return false.
    // https://immersive-web.github.io/webxr/#trustworthy.
    if (&document != globalObject.document())
        return false;

    // https://immersive-web.github.io/webxr/#active-and-focused
    if (!document.hasFocus() || !document.securityOrigin().isSameOriginAs(globalObject.document()->securityOrigin()))
        return false;

    // 3. If user intent to begin an immersive session is not well understood,
    //    either via explicit consent or implicit consent, return false
    if (!UserGestureIndicator::processingUserGesture())
        return false;

    return true;
}

// https://immersive-web.github.io/webxr/#inline-session-request-is-allowed
bool WebXRSystem::inlineSessionRequestIsAllowedForGlobalObject(DOMWindow& globalObject, Document& document, const XRSessionInit& init) const
{
    // 1. If the session request contained any required features or optional features and the request was not made
    //    while the global object has transient activation or when launching a web application, return false.
    bool sessionRequestContainedAnyFeature = !init.optionalFeatures.isEmpty() || !init.requiredFeatures.isEmpty();
    if (sessionRequestContainedAnyFeature && !globalObject.hasTransientActivation() /* TODO: || !launching a web app */)
        return false;

    // 2. If the requesting document is not responsible, return false.
    if (&document != globalObject.document())
        return false;

    return true;
}


struct WebXRSystem::ResolvedRequestedFeatures {
    FeaturesArray granted;
    FeaturesArray consentRequired;
    FeaturesArray consentOptional;
};

#define RETURN_FALSE_OR_CONTINUE(mustReturn) { \
    if (mustReturn) {\
        return false; \
    } \
    continue; \
}

// https://immersive-web.github.io/webxr/#resolve-the-requested-features
Optional<WebXRSystem::ResolvedRequestedFeatures> WebXRSystem::resolveRequestedFeatures(XRSessionMode mode, const XRSessionInit& init, PlatformXR::Device* device, JSC::JSGlobalObject& globalObject) const
{
    // 1. Let consentRequired be an empty list of DOMString.
    // 2. Let consentOptional be an empty list of DOMString.
    ResolvedRequestedFeatures resolvedFeatures;

    // 3. Let device be the result of obtaining the current device for mode, requiredFeatures, and optionalFeatures.
    // 4. Let granted be a list of DOMString initialized to device's list of enabled features for mode.
    // 5. If device is null or device's list of supported modes does not contain mode, run the following steps:
    //  5.1 Return the tuple (consentRequired, consentOptional, granted)
    if (!device || !device->supports(mode))
        return resolvedFeatures;

    resolvedFeatures.granted = device->enabledFeatures(mode);

    // 6. Add every feature descriptor in the default features table associated
    //    with mode to the indicated feature list if it is not already present.
    // https://immersive-web.github.io/webxr/#default-features
    auto requiredFeaturesWithDefaultFeatures = init.requiredFeatures;
    requiredFeaturesWithDefaultFeatures.append(convertEnumerationToJS(globalObject, XRReferenceSpaceType::Viewer));
    if (mode == XRSessionMode::ImmersiveAr || mode == XRSessionMode::ImmersiveVr)
        requiredFeaturesWithDefaultFeatures.append(convertEnumerationToJS(globalObject, XRReferenceSpaceType::Local));

    // 7. For each feature in requiredFeatures|optionalFeatures perform the following steps:
    // 8. For each feature in optionalFeatures perform the following steps:
    // We're merging both loops in a single lambda. The only difference is that a failure on any required features
    // implies cancelling the whole process while failures in optional features are just skipped.
    enum class ParsingMode { Strict, Loose };
    auto parseFeatures = [&device, &globalObject, mode, &resolvedFeatures] (const JSFeaturesArray& sessionFeatures, ParsingMode parsingMode) -> bool {
        bool returnOnFailure = parsingMode == ParsingMode::Strict;
        for (const auto& sessionFeature : sessionFeatures) {
            // 1. If the feature is null, continue to the next entry.
            if (sessionFeature.isNull())
                continue;

            // 2. If feature is not a valid feature descriptor, perform the following steps
            //   2.1. Let s be the result of calling ? ToString(feature).
            //   2.2. If s is not a valid feature descriptor or is undefined, (return null|continue to next entry).
            //   2.3. Set feature to s.
            auto feature = parseEnumeration<XRReferenceSpaceType>(globalObject, sessionFeature);
            if (!feature)
                RETURN_FALSE_OR_CONTINUE(returnOnFailure);

            // 3. If feature is already in granted, continue to the next entry.
            if (resolvedFeatures.granted.contains(feature.value()))
                continue;

            // 4. If the requesting document's origin is not allowed to use any feature policy required by feature
            //    as indicated by the feature requirements table, (return null|continue to next entry).

            // 5. If session's XR device is not capable of supporting the functionality described by feature or the
            //    user agent has otherwise determined to reject the feature, (return null|continue to next entry).
            if (!device->enabledFeatures(mode).contains(feature.value()))
                RETURN_FALSE_OR_CONTINUE(returnOnFailure);

            // 6. If the functionality described by feature requires explicit consent, append it to (consentRequired|consentOptional).
            // 7. Else append feature to granted.
            resolvedFeatures.granted.append(feature.value());
        }
        return true;
    };

    if (!parseFeatures(requiredFeaturesWithDefaultFeatures, ParsingMode::Strict))
        return WTF::nullopt;

    parseFeatures(init.optionalFeatures, ParsingMode::Loose);
    return resolvedFeatures;
}

// https://immersive-web.github.io/webxr/#request-the-xr-permission
bool WebXRSystem::isXRPermissionGranted(XRSessionMode mode, const XRSessionInit& init, PlatformXR::Device* device, JSC::JSGlobalObject& globalObject) const
{
    // 1. Set status's granted to an empty FrozenArray.
    // 2. Let requiredFeatures be descriptor's requiredFeatures.
    // 3. Let optionalFeatures be descriptor's optionalFeatures.
    // 4. Let device be the result of obtaining the current device for mode, requiredFeatures, and optionalFeatures.

    // 5. Let result be the result of resolving the requested features given requiredFeatures,optionalFeatures, and mode.
    auto resolvedFeatures = resolveRequestedFeatures(mode, init, device, globalObject);

    // 6. If result is null, run the following steps:
    //  6.1. Set status's state to "denied".
    //  6.2. Abort these steps.
    if (!resolvedFeatures)
        return false;

    // 7. Let (consentRequired, consentOptional, granted) be the fields of result.
    // 8. The user agent MAY at this point ask the user's permission for the calling algorithm to use any of the features
    //    in consentRequired and consentOptional. The results of these prompts should be included when determining if there
    //    is a clear signal of user intent for enabling these features.
    // 9. For each feature in consentRequired perform the following steps:
    //  9.1. The user agent MAY at this point ask the user's permission for the calling algorithm to use feature. The results
    //       of these prompts should be included when determining if there is a clear signal of user intent to enable feature.
    //  9.2. If a clear signal of user intent to enable feature has not been determined, set status's state to "denied" and
    //       abort these steps.
    //  9.3. If feature is not in granted, append feature to granted.
    // 10. For each feature in consentOptional perform the following steps:
    //  10.1. The user agent MAY at this point ask the user's permission for the calling algorithm to use feature. The results
    //        of these prompts should be included when determining if there is a clear signal of user intent to enable feature.
    //  10.2. If a clear signal of user intent to enable feature has not been determined, continue to the next entry.
    //  10.3. If feature is not in granted, append feature to granted.
    // 11. Set status's granted to granted.
    // 12. Set device's list of enabled features for mode to granted.
    // 13. Set status's state to "granted".
    return true;
}


// https://immersive-web.github.io/webxr/#dom-xrsystem-requestsession
void WebXRSystem::requestSession(Document& document, XRSessionMode mode, const XRSessionInit& init, RequestSessionPromise&& promise)
{
    // 1. Let promise be a new Promise.
    // 2. Let immersive be true if mode is an immersive session mode, and false otherwise.
    // 3. Let global object be the relevant Global object for the XRSystem on which this method was invoked.
    bool immersive = mode == XRSessionMode::ImmersiveAr || mode == XRSessionMode::ImmersiveVr;
    auto* globalObject = document.domWindow();
    if (!globalObject) {
        promise.reject(Exception { InvalidAccessError});
        return;
    }

    // 4. Check whether the session request is allowed as follows:
    if (immersive) {
        if (!immersiveSessionRequestIsAllowedForGlobalObject(*globalObject, document)) {
            promise.reject(Exception { SecurityError });
            return;
        }
        if (m_pendingImmersiveSession || m_activeImmersiveSession) {
            promise.reject(Exception { InvalidStateError });
            return;
        }
        m_pendingImmersiveSession = true;
    } else if (!inlineSessionRequestIsAllowedForGlobalObject(*globalObject, document, init)) {
        promise.reject(Exception { SecurityError });
        return;
    }

    // 5. Run the following steps in parallel:
    queueTaskKeepingObjectAlive(*this, TaskSource::WebXR, [this, document = makeWeakPtr(document), immersive, init, mode, promise = WTFMove(promise)] () mutable {
        // 5.1 Let requiredFeatures be options' requiredFeatures.
        // 5.2 Let optionalFeatures be options' optionalFeatures.
        // 5.3 Set device to the result of obtaining the current device for mode, requiredFeatures, and optionalFeatures.
        auto* device = obtainCurrentDevice(mode, init.requiredFeatures, init.optionalFeatures);

        // 5.4 Queue a task to perform the following steps:
        queueTaskKeepingObjectAlive(*this, TaskSource::WebXR, [this, document = WTFMove(document), device = makeWeakPtr(device), immersive, init, mode, promise = WTFMove(promise)] () mutable {
            auto rejectPromiseWithNotSupportedError = makeScopeExit([&] () {
                promise.reject(Exception { NotSupportedError });
                m_pendingImmersiveSession = false;
            });

            // 5.4.1 If device is null or device's list of supported modes does not contain mode, run the following steps:
            //  - Reject promise with a "NotSupportedError" DOMException.
            //  - If immersive is true, set pending immersive session to false.
            //  - Abort these steps.
            if (!device || !device->supports(mode))
                return;

            auto* globalObject = document ? document->execState() : nullptr;
            if (!globalObject)
                return;

            // WebKit does not currently support the Permissions API. https://w3c.github.io/permissions/
            // However we do implement here the permission request algorithm without the
            // Permissions API bits as it handles, among others, the session features parsing. We also
            // do it here before creating the session as there is no need to do it on advance.
            // TODO: we just perform basic checks without asking any permission to the user so far. Maybe we should implement
            // a mechanism similar to what others do involving passing a message to the UI process.

            // 5.4.4 Let descriptor be an XRPermissionDescriptor initialized with session, requiredFeatures, and optionalFeatures
            // 5.4.5 Let status be an XRPermissionStatus, initially null
            // 5.4.6 Request the xr permission with descriptor and status.
            // 5.4.7 If status' state is "denied" run the following steps: (same as above in 5.4.1)
            if (!isXRPermissionGranted(mode, init, device.get(), *globalObject))
                return;

            // 5.4.2 Let session be a new XRSession object.
            // 5.4.3 Initialize the session with session, mode, and device.
            auto session = WebXRSession::create(*document, *this, mode, *device);

            // 5.4.8 Potentially set the active immersive session as follows:
            if (immersive) {
                m_activeImmersiveSession = session.copyRef();
                m_pendingImmersiveSession = false;
            } else
                m_inlineSessions.add(session.copyRef());

            // 5.4.9 Resolve promise with session.
            promise.resolve(session);
            rejectPromiseWithNotSupportedError.release();

            // TODO:
            // 5.4.10 Queue a task to perform the following steps: NOTE: These steps ensure that initial inputsourceschange
            // events occur after the initial session is resolved.
            //     1. Set session's promise resolved flag to true.
            //     2. Let sources be any existing input sources attached to session.
            //     3. If sources is non-empty, perform the following steps:
            //        1. Set session's list of active XR input sources to sources.
            //        2. Fire an XRInputSourcesChangeEvent named inputsourceschange on session with added set to sources.

        });
    });
}

const char* WebXRSystem::activeDOMObjectName() const
{
    return "XRSystem";
}

void WebXRSystem::stop()
{
}

void WebXRSystem::registerSimulatedXRDeviceForTesting(PlatformXR::Device& device)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().webXREnabled())
        return;
    m_testingDevices++;
    if (device.supports(XRSessionMode::ImmersiveVr) || device.supports(XRSessionMode::ImmersiveAr)) {
        m_immersiveDevices.add(device);
        m_activeImmersiveDevice = makeWeakPtr(device);
    }
    if (device.supports(XRSessionMode::Inline))
        m_inlineXRDevice = makeWeakPtr(device);
}

void WebXRSystem::unregisterSimulatedXRDeviceForTesting(PlatformXR::Device& device)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().webXREnabled())
        return;
    ASSERT(m_testingDevices);
    bool removed = m_immersiveDevices.remove(device);
    ASSERT_UNUSED(removed, removed || m_inlineXRDevice == &device);
    if (m_activeImmersiveDevice == &device)
        m_activeImmersiveDevice = nullptr;
    if (m_inlineXRDevice == &device)
        m_inlineXRDevice = makeWeakPtr(m_defaultInlineDevice);
    m_testingDevices--;
}

void WebXRSystem::sessionEnded(WebXRSession& session)
{
    if (m_activeImmersiveSession == &session)
        m_activeImmersiveSession = nullptr;

    m_inlineSessions.remove(session);
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
