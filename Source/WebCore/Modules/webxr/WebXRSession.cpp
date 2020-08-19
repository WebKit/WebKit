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
#include "WebXRSession.h"

#if ENABLE(WEBXR)

#include "JSWebXRReferenceSpace.h"
#include "WebXRBoundedReferenceSpace.h"
#include "WebXRFrame.h"
#include "WebXRSystem.h"
#include "XRFrameRequestCallback.h"
#include "XRRenderStateInit.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRSession);

Ref<WebXRSession> WebXRSession::create(Document& document, WebXRSystem& system, XRSessionMode mode, PlatformXR::Device& device)
{
    return adoptRef(*new WebXRSession(document, system, mode, device));
}

WebXRSession::WebXRSession(Document& document, WebXRSystem& system, XRSessionMode mode, PlatformXR::Device& device)
    : ActiveDOMObject(&document)
    , m_inputSources(WebXRInputSourceArray::create())
    , m_xrSystem(system)
    , m_mode(mode)
    , m_device(makeWeakPtr(device))
    , m_activeRenderState(WebXRRenderState::create(mode))
    , m_workQueue(WorkQueue::create("XRSession work queue"))
    , m_animationTimer(*this, &WebXRSession::animationTimerFired)
{
    // FIXME: If no other features of the user agent have done so already, perform the necessary platform-specific steps to
    // initialize the device's tracking and rendering capabilities, including showing any necessary instructions to the user.
    suspendIfNeeded();
}

WebXRSession::~WebXRSession() = default;

XREnvironmentBlendMode WebXRSession::environmentBlendMode() const
{
    return m_environmentBlendMode;
}

XRInteractionMode WebXRSession::interactionMode() const
{
    return m_interactionMode;
}

XRVisibilityState WebXRSession::visibilityState() const
{
    return m_visibilityState;
}

const WebXRRenderState& WebXRSession::renderState() const
{
    return *m_activeRenderState;
}

const WebXRInputSourceArray& WebXRSession::inputSources() const
{
    return m_inputSources;
}

static bool isImmersive(XRSessionMode mode)
{
    return mode == XRSessionMode::ImmersiveAr || mode == XRSessionMode::ImmersiveVr;
}

// https://immersive-web.github.io/webxr/#dom-xrsession-updaterenderstate
ExceptionOr<void> WebXRSession::updateRenderState(const XRRenderStateInit& newState)
{
    // 1. Let session be this.
    // 2. If session's ended value is true, throw an InvalidStateError and abort these steps.
    if (m_ended)
        return Exception { InvalidStateError };

    // 3. If newState's baseLayer was created with an XRSession other than session,
    //    throw an InvalidStateError and abort these steps.
    if (newState.baseLayer && &newState.baseLayer->session() != this)
        return Exception { InvalidStateError };

    // 4. If newState's inlineVerticalFieldOfView is set and session is an immersive session,
    //    throw an InvalidStateError and abort these steps.
    if (newState.inlineVerticalFieldOfView && isImmersive(m_mode))
        return Exception { InvalidStateError };

    // 5. If none of newState's depthNear, depthFar, inlineVerticalFieldOfView, baseLayer,
    //    layers are set, abort these steps.
    if (!newState.depthNear && !newState.depthFar && !newState.inlineVerticalFieldOfView && !newState.baseLayer && !newState.layers)
        return { };

    // 6. Run update the pending layers state with session and newState.
    // https://immersive-web.github.io/webxr/#update-the-pending-layers-state
    if (newState.layers)
        return Exception { NotSupportedError };

    // 7. Let activeState be session's active render state.
    // 8. If session's pending render state is null, set it to a copy of activeState.
    if (!m_pendingRenderState)
        m_pendingRenderState = m_activeRenderState->clone();

    // 9. If newState's depthNear value is set, set session's pending render state's depthNear to newState's depthNear.
    if (newState.depthNear)
        m_pendingRenderState->setDepthNear(newState.depthNear.value());

    // 10. If newState's depthFar value is set, set session's pending render state's depthFar to newState's depthFar.
    if (newState.depthFar)
        m_pendingRenderState->setDepthFar(newState.depthFar.value());

    // 11. If newState's inlineVerticalFieldOfView is set, set session's pending render state's inlineVerticalFieldOfView
    //     to newState's inlineVerticalFieldOfView.
    if (newState.inlineVerticalFieldOfView)
        m_pendingRenderState->setInlineVerticalFieldOfView(newState.inlineVerticalFieldOfView.value());

    // 12. If newState's baseLayer is set, set session's pending render state's baseLayer to newState's baseLayer.
    if (newState.baseLayer)
        m_pendingRenderState->setBaseLayer(newState.baseLayer.get());

    return { };
}

// https://immersive-web.github.io/webxr/#reference-space-is-supported
bool WebXRSession::referenceSpaceIsSupported(XRReferenceSpaceType type) const
{
    // 1. If type is not contained in session’s XR device's list of enabled features for mode return false.
    if (!m_device->enabledFeatures(m_mode).contains(type))
        return false;

    // 2. If type is viewer, return true.
    if (type == XRReferenceSpaceType::Viewer)
        return true;

    bool isImmersiveSession = isImmersive(m_mode);
    if (type == XRReferenceSpaceType::Local || type == XRReferenceSpaceType::LocalFloor) {
        // 3. If type is local or local-floor, and session is an immersive session, return true.
        if (isImmersiveSession)
            return true;

        // 4. If type is local or local-floor, and the XR device supports reporting orientation data, return true.
        if (m_device->supportsOrientationTracking())
            return true;
    }

    // 5. If type is bounded-floor and session is an immersive session, return the result of whether bounded
    //    reference spaces are supported by the XR device.
    // https://immersive-web.github.io/webxr/#bounded-reference-spaces-are-supported
    // TODO: add API to PlatformXR::Device
    if (type == XRReferenceSpaceType::BoundedFloor && isImmersiveSession)
        return true;

    // 6. If type is unbounded, session is an immersive session, and the XR device supports stable tracking
    //    near the user over an unlimited distance, return true.
    // TODO: add API to PlatformXR::Device to check stable tracking over unlimited distance.
    if (type == XRReferenceSpaceType::Unbounded && isImmersiveSession)
        return true;

    // 7. Return false.
    return false;
}

// https://immersive-web.github.io/webxr/#dom-xrsession-requestreferencespace
void WebXRSession::requestReferenceSpace(XRReferenceSpaceType type, RequestReferenceSpacePromise&& promise)
{
    auto* context = scriptExecutionContext();
    if (!context) {
        promise.reject(Exception { InvalidStateError });
        return;
    }
    // 1. Let promise be a new Promise.
    // 2. Run the following steps in parallel:
    m_workQueue->dispatch([this, promise = WTFMove(promise), weakDocument = makeWeakPtr(downcast<Document>(*context)), type] () mutable {
        //  2.1. Create a reference space, referenceSpace, with the XRReferenceSpaceType type.
        //  2.2. If referenceSpace is null, reject promise with a NotSupportedError and abort these steps.
        if (!referenceSpaceIsSupported(type)) {
            callOnMainThread([promise = WTFMove(promise)] () mutable {
                promise.reject(Exception { NotSupportedError });
            });
            return;
        }

        if (!weakDocument) {
            callOnMainThread([promise = WTFMove(promise)] () mutable {
                promise.reject(Exception { InvalidStateError });
            });
            return;
        }

        // https://immersive-web.github.io/webxr/#create-a-reference-space
        RefPtr<WebXRReferenceSpace> referenceSpace;
        ASSERT(is<Document>(context));
        if (type == XRReferenceSpaceType::BoundedFloor)
            referenceSpace = WebXRBoundedReferenceSpace::create(*weakDocument, makeRef(*this), type);
        else
            referenceSpace = WebXRReferenceSpace::create(*weakDocument, makeRef(*this), type);

        //  2.3. Resolve promise with referenceSpace.
        // 3. Return promise.
        callOnMainThread([promise = WTFMove(promise), referenceSpace = referenceSpace.releaseNonNull()] () mutable {
            promise.resolve(referenceSpace);
        });
    });
}

void WebXRSession::animationTimerFired()
{
    m_lastAnimationFrameTimestamp = MonotonicTime::now();

    if (m_callbacks.isEmpty())
        return;

    // TODO: retrieve frame from platform.
    auto frame = WebXRFrame::create(*this);

    m_runningCallbacks.swap(m_callbacks);
    for (auto& callback : m_runningCallbacks) {
        if (callback->isCancelled())
            continue;
        callback->handleEvent(m_lastAnimationFrameTimestamp.secondsSinceEpoch().milliseconds(), frame.get());
    }

    m_runningCallbacks.clear();
}

void WebXRSession::scheduleAnimation()
{
    if (m_animationTimer.isActive())
        return;

    if (m_ended)
        return;

    // TODO: use device's refresh rate. Let's start with 60fps.
    Seconds animationInterval = 15_ms;
    Seconds scheduleDelay = std::max(animationInterval - (MonotonicTime::now() - m_lastAnimationFrameTimestamp), 0_s);
    m_animationTimer.startOneShot(scheduleDelay);
}

// https://immersive-web.github.io/webxr/#dom-xrsession-requestanimationframe
unsigned WebXRSession::requestAnimationFrame(Ref<XRFrameRequestCallback>&& callback)
{
    // 1. Let session be the target XRSession object.
    // 2. Increment session's animation frame callback identifier by one.
    unsigned newId = m_nextCallbackId++;

    // 3. Append callback to session's list of animation frame callbacks, associated with session's
    // animation frame callback identifier's current value.
    callback->setCallbackId(newId);
    m_callbacks.append(WTFMove(callback));

    scheduleAnimation();

    // 4. Return session's animation frame callback identifier's current value.
    return newId;
}

// https://immersive-web.github.io/webxr/#dom-xrsession-cancelanimationframe
void WebXRSession::cancelAnimationFrame(unsigned callbackId)
{
    // 1. Let session be the target XRSession object.
    // 2. Find the entry in session's list of animation frame callbacks or session's list of
    //    currently running animation frame callbacks that is associated with the value handle.
    // 3. If there is such an entry, set its cancelled boolean to true and remove it from
    //    session's list of animation frame callbacks.
    size_t position = m_callbacks.findMatching([callbackId] (auto& item) {
        return item->callbackId() == callbackId;
    });

    if (position != notFound) {
        m_callbacks[position]->cancel();
        m_callbacks.remove(position);
        return;
    }

    position = m_runningCallbacks.findMatching([callbackId] (auto& item) {
        return item->callbackId() == callbackId;
    });

    if (position != notFound)
        m_runningCallbacks[position]->cancel();
}

// https://immersive-web.github.io/webxr/#native-webgl-framebuffer-resolution
IntSize WebXRSession::nativeWebGLFramebufferResolution() const
{
    if (m_mode == XRSessionMode::Inline) {
        // FIXME: replace the conditional by ASSERTs once we properly initialize the outputCanvas.
        return m_activeRenderState && m_activeRenderState->outputCanvas() ? m_activeRenderState->outputCanvas()->size() : IntSize(1, 1);
    }

    return recommendedWebGLFramebufferResolution();
}

// https://immersive-web.github.io/webxr/#recommended-webgl-framebuffer-resolution
IntSize WebXRSession::recommendedWebGLFramebufferResolution() const
{
    ASSERT(m_device);
    return m_device->recommendedResolution(m_mode);
}

// https://immersive-web.github.io/webxr/#shut-down-the-session
void WebXRSession::shutdown()
{
    // 1. Let session be the target XRSession object.
    // 2. Set session's ended value to true.
    m_ended = true;

    // 3. If the active immersive session is equal to session, set the active immersive session to null.
    // 4. Remove session from the list of inline sessions.
    m_xrSystem.sessionEnded(*this);

    // TODO: complete the implementation
    // 5. Reject any outstanding promises returned by session with an InvalidStateError, except for any promises returned by end().
    // 6. If no other features of the user agent are actively using them, perform the necessary platform-specific steps to shut down the device's tracking and rendering capabilities. This MUST include:
    //  6.1. Releasing exclusive access to the XR device if session is an immersive session.
    //  6.2. Deallocating any graphics resources acquired by session for presentation to the XR device.
    //  6.3. Putting the XR device in a state such that a different source may be able to initiate a session with the same device if session is an immersive session.
    // 7. Queue a task that fires an XRSessionEvent named end on session.
}

// https://immersive-web.github.io/webxr/#dom-xrsession-end
void WebXRSession::end(EndPromise&& promise)
{
    // The shutdown() call bellow might remove the sole reference to session
    // that could exist (the XRSystem owns the sessions) so let's protect this.
    Ref<WebXRSession> protectedThis(*this);
    // 1. Let promise be a new Promise.
    // 2. Shut down the target XRSession object.
    shutdown();

    // 3. Queue a task to perform the following steps:
    queueTaskKeepingObjectAlive(*this, TaskSource::WebXR, [promise = WTFMove(promise)] () mutable {
        // 3.1 Wait until any platform-specific steps related to shutting down the session have completed.
        // 3.2 Resolve promise.
        promise.resolve();
    });

    // 4. Return promise.
}

const char* WebXRSession::activeDOMObjectName() const
{
    return "XRSession";
}

void WebXRSession::stop()
{
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
