/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#include "Document.h"
#include "EventNames.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebXRReferenceSpace.h"
#include "SecurityOrigin.h"
#include "WebCoreOpaqueRoot.h"
#include "WebXRBoundedReferenceSpace.h"
#include "WebXRFrame.h"
#include "WebXRSystem.h"
#include "WebXRView.h"
#include "XRFrameRequestCallback.h"
#include "XRRenderStateInit.h"
#include "XRSessionEvent.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/RefPtr.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRSession);

Ref<WebXRSession> WebXRSession::create(Document& document, WebXRSystem& system, XRSessionMode mode, PlatformXR::Device& device, FeatureList&& requestedFeatures)
{
    auto session = adoptRef(*new WebXRSession(document, system, mode, device, WTFMove(requestedFeatures)));
    session->suspendIfNeeded();
    return session;
}

WebXRSession::WebXRSession(Document& document, WebXRSystem& system, XRSessionMode mode, PlatformXR::Device& device, FeatureList&& requestedFeatures)
    : ActiveDOMObject(&document)
    , m_inputSources(WebXRInputSourceArray::create(*this))
    , m_xrSystem(system)
    , m_mode(mode)
    , m_device(device)
    , m_requestedFeatures(WTFMove(requestedFeatures))
    , m_activeRenderState(WebXRRenderState::create(mode))
    , m_viewerReferenceSpace(WebXRViewerSpace::create(document, *this))
    , m_timeOrigin(MonotonicTime::now())
    , m_views(device.views(mode))
{
    device.setTrackingAndRenderingClient(*this);
    device.initializeTrackingAndRendering(document.securityOrigin().data(), mode, m_requestedFeatures);

    // https://immersive-web.github.io/webxr/#ref-for-dom-xrreferencespacetype-viewer%E2%91%A2
    // Every session MUST support viewer XRReferenceSpaces.
    device.initializeReferenceSpace(XRReferenceSpaceType::Viewer);
}

WebXRSession::~WebXRSession()
{
    auto device = m_device.get();
    if (!m_ended && device)
        device->shutDownTrackingAndRendering();
}

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

// https://immersive-web.github.io/webxr/#dom-xrsession-updaterenderstate
ExceptionOr<void> WebXRSession::updateRenderState(const XRRenderStateInit& newState)
{
    // 1. Let session be this.
    // 2. If session's ended value is true, throw an InvalidStateError and abort these steps.
    if (m_ended)
        return Exception { ExceptionCode::InvalidStateError };

    // 3. If newState's baseLayer was created with an XRSession other than session,
    //    throw an InvalidStateError and abort these steps.
    if (newState.baseLayer && &newState.baseLayer->session() != this)
        return Exception { ExceptionCode::InvalidStateError };

    // 4. If newState's inlineVerticalFieldOfView is set and session is an immersive session,
    //    throw an InvalidStateError and abort these steps.
    if (newState.inlineVerticalFieldOfView && isImmersive(m_mode))
        return Exception { ExceptionCode::InvalidStateError };

    // 5. If none of newState's depthNear, depthFar, inlineVerticalFieldOfView, baseLayer,
    //    layers are set, abort these steps.
    if (!newState.depthNear && !newState.depthFar && !newState.inlineVerticalFieldOfView && !newState.baseLayer && !newState.layers)
        return { };

    // 6. Run update the pending layers state with session and newState.
    // https://immersive-web.github.io/webxr/#update-the-pending-layers-state
    if (newState.layers)
        return Exception { ExceptionCode::NotSupportedError };

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
    if (!m_requestedFeatures.contains(sessionFeatureFromReferenceSpaceType(type)))
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
        auto device = m_device.get();
        if (device && device->supportsOrientationTracking())
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
    if (!scriptExecutionContext()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    // 1. Let promise be a new Promise.
    // 2. Run the following steps in parallel:
    scriptExecutionContext()->postTask([this, weakThis = WeakPtr { *this }, promise = WTFMove(promise), type](auto&) mutable {
        if (!weakThis)
            return;
        // 2.1. If the result of running reference space is supported for type and session is false, queue a task to reject promise
        // with a NotSupportedError and abort these steps.
        if (!referenceSpaceIsSupported(type)) {
            queueTaskKeepingObjectAlive(*this, TaskSource::WebXR, [promise = WTFMove(promise)]() mutable {
                promise.reject(Exception { ExceptionCode::NotSupportedError });
            });
            return;
        }
        // 2.2. Set up any platform resources required to track reference spaces of type type.
        if (auto device = m_device.get())
            device->initializeReferenceSpace(type);

        // 2.3. Queue a task to run the following steps:
        queueTaskKeepingObjectAlive(*this, TaskSource::WebXR, [this, type, promise = WTFMove(promise)]() mutable {
            if (!scriptExecutionContext()) {
                promise.reject(Exception { ExceptionCode::InvalidStateError });
                return;
            }
            auto& document = downcast<Document>(*scriptExecutionContext());
            // 2.4. Create a reference space, referenceSpace, with type and session.
            // https://immersive-web.github.io/webxr/#create-a-reference-space
            RefPtr<WebXRReferenceSpace> referenceSpace;
            if (type == XRReferenceSpaceType::BoundedFloor)
                referenceSpace = WebXRBoundedReferenceSpace::create(document, Ref { *this }, type);
            else
                referenceSpace = WebXRReferenceSpace::create(document, Ref { *this }, type);

            // 2.5. Resolve promise with referenceSpace.
            promise.resolve(referenceSpace.releaseNonNull());
        });
    });
}

// https://immersive-web.github.io/webxr/#dom-xrsession-requestanimationframe
unsigned WebXRSession::requestAnimationFrame(Ref<XRFrameRequestCallback>&& callback)
{
    // Ignore any new frame requests once the session is ended.
    if (m_ended)
        return 0;

    // 1. Let session be the target XRSession object.
    // 2. Increment session's animation frame callback identifier by one.
    unsigned newId = m_nextCallbackId++;

    // 3. Append callback to session's list of animation frame callbacks, associated with session's
    // animation frame callback identifier's current value.
    callback->setCallbackId(newId);
    m_callbacks.append(WTFMove(callback));

    // Script can add multiple requestAnimationFrame callbacks but we should only request a device frame once.
    // When requestAnimationFrame is called during processing RAF callbacks the next requestFrame is scheduled
    // at the end of WebXRSession::onFrame() to prevent requesting a new frame before the current one has ended.
    requestFrameIfNeeded();

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
    size_t position = m_callbacks.findIf([callbackId] (auto& item) {
        return item->callbackId() == callbackId;
    });

    if (position != notFound) {
        m_callbacks[position]->setFiredOrCancelled();
        return;
    }
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
    auto device = m_device.get();
    ASSERT(device);
    return device ? device->recommendedResolution(m_mode) : IntSize { };
}

// https://immersive-web.github.io/webxr/#view-viewport-modifiable
bool WebXRSession::supportsViewportScaling() const
{
    auto device = m_device.get();
    ASSERT(device);
    // Only immersive sessions support viewport scaling.
    return isImmersive(m_mode) && device && device->supportsViewportScaling();
}

bool WebXRSession::isPositionEmulated() const
{
    return m_frameData.isPositionEmulated || !m_frameData.isPositionValid;
}

// https://immersive-web.github.io/webxr/#shut-down-the-session
void WebXRSession::shutdown(InitiatedBySystem initiatedBySystem)
{
    Ref protectedThis { *this };

    if (m_ended) {
        // This method was called earlier with initiatedBySystem=No when the
        // session termination was requested manually via XRSession.end(). When
        // the system has completed the shutdown, this method is now called again
        // with initiatedBySystem=Yes to do the final cleanup.
        if (initiatedBySystem == InitiatedBySystem::Yes)
            didCompleteShutdown();
        return;
    }

    // 1. Let session be the target XRSession object.
    // 2. Set session's ended value to true.
    m_ended = true;

    // 3. If the active immersive session is equal to session, set the active immersive session to null.
    // 4. Remove session from the list of inline sessions.
    m_xrSystem.sessionEnded(*this);

    m_inputSources->clear();

    if (initiatedBySystem == InitiatedBySystem::Yes) {
        // If we get here, the session termination was triggered by the system rather than
        // via XRSession.end(). Since the system has completed the session shutdown, we can
        // immediately do the final cleanup.
        didCompleteShutdown();
        return;
    }

    // TODO: complete the implementation
    // 5. Reject any outstanding promises returned by session with an InvalidStateError, except for any promises returned by end().
    // 6. If no other features of the user agent are actively using them, perform the necessary platform-specific steps to shut down the device's tracking and rendering capabilities. This MUST include:
    //  6.1. Releasing exclusive access to the XR device if session is an immersive session.
    //  6.2. Deallocating any graphics resources acquired by session for presentation to the XR device.
    //  6.3. Putting the XR device in a state such that a different source may be able to initiate a session with the same device if session is an immersive session.
    auto device = m_device.get();
    if (device)
        device->shutDownTrackingAndRendering();

    // If device will not report shutdown completion via the TrackingAndRenderingClient,
    // complete the shutdown cleanup here.
    if (!device || !device->supportsSessionShutdownNotification())
        didCompleteShutdown();
}

void WebXRSession::didCompleteShutdown()
{
    if (auto device = m_device.get())
        device->setTrackingAndRenderingClient(nullptr);

    // Resolve end promise from XRSession::end()
    if (m_endPromise) {
        m_endPromise->resolve();
        m_endPromise = nullptr;
    }

    // From https://immersive-web.github.io/webxr/#shut-down-the-session
    // 7. Queue a task that fires an XRSessionEvent named end on session.
    auto event = XRSessionEvent::create(eventNames().endEvent, { RefPtr { this } });
    queueTaskToDispatchEvent(*this, TaskSource::WebXR, WTFMove(event));
}

// https://immersive-web.github.io/webxr/#dom-xrsession-end
ExceptionOr<void> WebXRSession::end(EndPromise&& promise)
{
    // The shutdown() call below might remove the sole reference to session
    // that could exist (the XRSystem owns the sessions) so let's protect this.
    Ref protectedThis { *this };

    if (m_ended)
        return Exception { ExceptionCode::InvalidStateError, "Cannot end a session more than once"_s };

    ASSERT(!m_endPromise);
    m_endPromise = makeUnique<EndPromise>(WTFMove(promise));

    // 1. Let promise be a new Promise.
    // 2. Shut down the target XRSession object.
    shutdown(InitiatedBySystem::No);

    // 3. Queue a task to perform the following steps:
    // 3.1 Wait until any platform-specific steps related to shutting down the session have completed.
    // 3.2 Resolve promise.
    // 4. Return promise.
    return { };
}

const char* WebXRSession::activeDOMObjectName() const
{
    return "XRSession";
}

void WebXRSession::stop()
{
}

void WebXRSession::sessionDidInitializeInputSources(Vector<PlatformXR::FrameData::InputSource>&& inputSources)
{
    // https://immersive-web.github.io/webxr/#dom-xrsystem-requestsession
    // 5.4.11 Queue a task to perform the following steps: NOTE: These steps ensure that initial inputsourceschange
    // events occur after the initial session is resolved.
    queueTaskKeepingObjectAlive(*this, TaskSource::WebXR, [this, inputSources = WTFMove(inputSources)]() mutable {
        //  1. Set session's promise resolved flag to true.
        m_inputInitialized = true;
        //  2. Let sources be any existing input sources attached to session.
        //  3. If sources is non-empty, perform the following steps:
        if (!inputSources.isEmpty()) {
            auto timestamp = (MonotonicTime::now() - m_timeOrigin).milliseconds();
            //  3.1. Set session's list of active XR input sources to sources.
            //  3.2. Fire an XRInputSourcesChangeEvent named inputsourceschange on session with added set to sources.
            //  Note: 3.1 and 3.2 steps are handled inside the update() call.
            m_inputSources->update(timestamp, inputSources);
        }
    });
}

void WebXRSession::sessionDidEnd()
{
    // This can be called as a result of finishing the shutdown initiated
    // from XRSession::end(), or session termination triggered by the system.
    shutdown(InitiatedBySystem::Yes);
}

void WebXRSession::updateSessionVisibilityState(PlatformXR::VisibilityState visibilityState)
{
    if (m_visibilityState == visibilityState)
        return;

    m_visibilityState = visibilityState;

    requestFrameIfNeeded();

    // From https://immersive-web.github.io/webxr/#event-types
    // A user agent MUST dispatch a visibilitychange event on an XRSession each time the
    // visibility state of the XRSession has changed. The event MUST be of type XRSessionEvent.
    auto event = XRSessionEvent::create(eventNames().visibilitychangeEvent, { RefPtr { this } });
    queueTaskToDispatchEvent(*this, TaskSource::WebXR, WTFMove(event));
}

void WebXRSession::applyPendingRenderState()
{
    // https: //immersive-web.github.io/webxr/#apply-the-pending-render-state
    // 1. Let activeState be session’s active render state.
    // 2. Let newState be session’s pending render state.
    // 3. Set session’s pending render state to null.
    auto newState = m_pendingRenderState;
    ASSERT(newState);

    // 4. Let oldBaseLayer be activeState’s baseLayer.
    // 5. Let oldLayers be activeState’s layers.
    // FIXME: those are only needed for step 6.2.

    // 6.1 Set activeState to newState.
    m_activeRenderState = newState;

    // 6.2 If oldBaseLayer is not equal to activeState’s baseLayer, oldLayers is not equal to activeState’s layers, or the dimensions of any of the layers have changed, update the viewports for session.
    // FIXME: implement this.

    // 6.3 If activeState’s inlineVerticalFieldOfView is less than session’s minimum inline field of view set activeState’s inlineVerticalFieldOfView to session’s minimum inline field of view.
    if (m_activeRenderState->inlineVerticalFieldOfView() < m_minimumInlineFOV)
        m_activeRenderState->setInlineVerticalFieldOfView(m_minimumInlineFOV);

    // 6.4 If activeState’s inlineVerticalFieldOfView is greater than session’s maximum inline field of view set activeState’s inlineVerticalFieldOfView to session’s maximum inline field of view.
    if (m_activeRenderState->inlineVerticalFieldOfView() > m_maximumInlineFOV)
        m_activeRenderState->setInlineVerticalFieldOfView(m_maximumInlineFOV);

    // 6.5 If activeState’s depthNear is less than session’s minimum near clip plane set activeState’s depthNear to session’s minimum near clip plane.
    if (m_activeRenderState->depthNear() < m_minimumNearClipPlane)
        m_activeRenderState->setDepthNear(m_minimumNearClipPlane);

    // 6.6 If activeState’s depthFar is greater than session’s maximum far clip plane set activeState’s depthFar to session’s maximum far clip plane.
    if (m_activeRenderState->depthFar() > m_maximumFarClipPlane)
        m_activeRenderState->setDepthFar(m_maximumFarClipPlane);

    // 6.7 Let baseLayer be activeState’s baseLayer.
    auto baseLayer = m_activeRenderState->baseLayer();

    // 6.8 Set activeState’s composition enabled and output canvas as follows:
    if (m_mode == XRSessionMode::Inline && is<WebXRWebGLLayer>(baseLayer) && !baseLayer->isCompositionEnabled()) {
        m_activeRenderState->setCompositionEnabled(false);
        m_activeRenderState->setOutputCanvas(baseLayer->canvas());
    } else {
        m_activeRenderState->setCompositionEnabled(true);
        m_activeRenderState->setOutputCanvas(nullptr);
    }
}

// https://immersive-web.github.io/webxr/#should-be-rendered
bool WebXRSession::frameShouldBeRendered() const
{
    if (!m_activeRenderState->baseLayer())
        return false;
    if (m_mode == XRSessionMode::Inline && !m_activeRenderState->outputCanvas())
        return false;
    return true;
}

void WebXRSession::requestFrameIfNeeded()
{
    ASSERT(isMainThread());

    if (m_ended)
        return;

    if (m_visibilityState == XRVisibilityState::Hidden)
        return;

    if (m_callbacks.isEmpty() || m_isDeviceFrameRequestPending)
        return;

    auto device = m_device.get();
    if (!device)
        return;
    m_isDeviceFrameRequestPending = true;
    device->requestFrame([this, protectedThis = Ref { *this }](auto&& frameData) {
        m_isDeviceFrameRequestPending = false;
        onFrame(WTFMove(frameData));
    });
}

void WebXRSession::onFrame(PlatformXR::FrameData&& frameData)
{
    ASSERT(isMainThread());

    if (m_ended)
        return;

    // From https://immersive-web.github.io/webxr/#xrsession-visibility-state
    // A state of hidden indicates that imagery rendered by the XRSession cannot be seen by the user.
    // requestAnimationFrame() callbacks will not be processed until the visibility state changes.
    // Input is not processed by the XRSession.
    if (m_visibilityState == XRVisibilityState::Hidden)
        return;

    // Queue a task to perform the following steps.
    queueTaskKeepingObjectAlive(*this, TaskSource::WebXR, [this, frameData = WTFMove(frameData)]() mutable {
        if (m_ended || m_visibilityState == XRVisibilityState::Hidden)
            return;

        m_frameData = WTFMove(frameData);
        //  1.Let now be the current high resolution time.
        auto now = (MonotonicTime::now() - m_timeOrigin).milliseconds();

        auto frame = WebXRFrame::create(*this, WebXRFrame::IsAnimationFrame::Yes);
        //  2.Let frame be session’s animation frame.
        //  3.Set frame’s time to frameTime.
        frame->setTime(static_cast<DOMHighResTimeStamp>(m_frameData.predictedDisplayTime));

        // 4. For each view in list of views, set view’s viewport modifiable flag to true.
        // 5. If the active flag of any view in the list of views has changed since the last XR animation frame, update the viewports.
        // FIXME: implement.

        // FIXME: I moved step 7 before 6 because of https://github.com/immersive-web/webxr/issues/1164
        // 7.If session’s pending render state is not null, apply the pending render state.
        if (m_pendingRenderState)
            applyPendingRenderState();

        // 6. If the frame should be rendered for session:
        if (frameShouldBeRendered() && m_frameData.shouldRender) {
            // Prepare all layers for render
            if (isImmersive(m_mode) && m_activeRenderState->baseLayer())
                m_activeRenderState->baseLayer()->startFrame(m_frameData);

            // 6.1.Set session’s list of currently running animation frame callbacks to be session’s list of animation frame callbacks.
            // 6.2.Set session’s list of animation frame callbacks to the empty list.
            auto callbacks = m_callbacks;

            // 6.3.Set frame’s active boolean to true.
            frame->setActive(true);

            // 6.4.Apply frame updates for frame.
            if (m_inputInitialized)
                m_inputSources->update(now, m_frameData.inputSources);

            // 6.5.For each entry in session’s list of currently running animation frame callbacks, in order:
            for (auto& callback : callbacks) {
                //  6.6.If the entry’s cancelled boolean is true, continue to the next entry.
                if (callback->isFiredOrCancelled())
                    continue;
                callback->setFiredOrCancelled();
                //  6.7.Invoke the Web IDL callback function for entry, passing now and frame as the arguments
                callback->handleEvent(now, frame.get());

                //  6.8.If an exception is thrown, report the exception.
            }
            // 6.9.Set session’s list of currently running animation frame callbacks to the empty list.
            m_callbacks.removeAllMatching([](auto& callback) {
                return callback->isFiredOrCancelled();
            });

            // 6.10.Set frame’s active boolean to false.
            // If the session is ended, m_animationFrame->setActive false is set in shutdown().
            frame->setActive(false);


            // Submit current frame layers to the device.
            Vector<PlatformXR::Device::Layer> frameLayers;
            if (isImmersive(m_mode) && m_activeRenderState->baseLayer())
                frameLayers.append(m_activeRenderState->baseLayer()->endFrame());

            if (auto device = m_device.get())
                device->submitFrame(WTFMove(frameLayers));
        }

        requestFrameIfNeeded();
    });
}

// https://immersive-web.github.io/webxr/#poses-may-be-reported
bool WebXRSession::posesCanBeReported(const Document& document) const
{
    // 1. If session’s relevant global object is not the current global object, return false.
    RefPtr sessionDocument = downcast<Document>(scriptExecutionContext());
    if (!sessionDocument || sessionDocument->domWindow() != document.domWindow())
        return false;

    // 2. If session's visibilityState is "hidden", return false.
    if (m_visibilityState == XRVisibilityState::Hidden)
        return false;

    // 5. Determine if the pose data can be returned as follows:
    // The procedure in the specs tries to ensure that we apply measures to
    // prevent fingerprintint in pose data and return false in case we don't.
    // We're going to apply them so let's just return true.
    return true;
}

#if ENABLE(WEBXR_HANDS)
bool WebXRSession::isHandTrackingEnabled() const
{
    return m_requestedFeatures.contains(PlatformXR::SessionFeature::HandTracking);
}
#endif

WebCoreOpaqueRoot root(WebXRSession* session)
{
    return WebCoreOpaqueRoot { session };
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
