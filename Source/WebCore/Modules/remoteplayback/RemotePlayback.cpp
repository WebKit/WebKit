/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "RemotePlayback.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#include "ContextDestructionObserverInlines.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLMediaElement.h"
#include "JSDOMPromiseDeferred.h"
#include "JSNodeCustom.h"
#include "Logging.h"
#include "MediaElementSession.h"
#include "MediaPlaybackTarget.h"
#include "RemotePlaybackAvailabilityCallback.h"
#include "UserGestureIndicator.h"
#include "WebCoreOpaqueRootInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RemotePlayback);

Ref<RemotePlayback> RemotePlayback::create(HTMLMediaElement& element)
{
    auto remotePlayback = adoptRef(*new RemotePlayback(element));
    remotePlayback->suspendIfNeeded();
    return remotePlayback;
}

RemotePlayback::RemotePlayback(HTMLMediaElement& element)
    : WebCore::ActiveDOMObject(element.scriptExecutionContext())
#if !RELEASE_LOG_DISABLED
    , m_logger(element.logger())
    , m_logIdentifier(element.logIdentifier())
#endif
    , m_mediaElement(element)
{
}

RemotePlayback::~RemotePlayback() = default;

WebCoreOpaqueRoot RemotePlayback::opaqueRootConcurrently() const
{
    return root(m_mediaElement.get());
}

Node* RemotePlayback::ownerNode() const
{
    return m_mediaElement.get();
}

void RemotePlayback::watchAvailability(Ref<RemotePlaybackAvailabilityCallback>&& callback, Ref<DeferredPromise>&& promise)
{
    // 6.2.1.3 Getting the remote playback devices availability information
    // https://w3c.github.io/remote-playback/#monitoring-the-list-of-available-remote-playback-devices
    // W3C Editor's Draft 15 July 2016

    // 1. Let promise be a new promise->
    // 2. Return promise, and run the following steps below:

    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [callback = WTFMove(callback), promise = WTFMove(promise), identifier = identifier](auto& playback) mutable {
        if (playback.isContextStopped())
            return;

        // 3. If the disableRemotePlayback attribute is present for the media element, reject the promise with
        //    InvalidStateError and abort all the remaining steps.
        if (!playback.m_mediaElement || playback.m_mediaElement->isWirelessPlaybackTargetDisabled()) {
            ERROR_LOG_WITH_THIS(&playback, identifier, "promise rejected, remote playback disabled");
            promise->reject(ExceptionCode::InvalidStateError);
            return;
        }

        // 4. If the user agent is unable to monitor the list of available remote playback devices for the entire
        //    lifetime of the browsing context (for instance, because the user has disabled this feature), then run
        //    the following steps in parallel:
        // 5. If the user agent is unable to continuously monitor the list of available remote playback devices but
        //    can do it for a short period of time when initiating remote playback, then:
        // NOTE: Unimplemented; all current ports can support continuous device monitoring

        // 6. Let callbackId be a number unique to the media element that will identify the callback.
        int32_t callbackId = ++playback.m_nextId;

        // 7. Create a tuple (callbackId, callback) and add it to the set of availability callbacks for this media element.
        ASSERT(!playback.m_callbackMap.contains(callbackId));
        playback.m_callbackMap.add(callbackId, WTFMove(callback));

        // 8. Fulfill promise with the callbackId and run the following steps in parallel:
        promise->whenSettled([protectedThis = Ref { playback }, callbackId] {
            // 8.1 Queue a task to invoke the callback with the current availability for the media element.
            queueTaskKeepingObjectAlive(protectedThis.get(), TaskSource::MediaElement, [callbackId, available = protectedThis->m_available](auto& playback) {
                if (playback.isContextStopped())
                    return;
                auto foundCallback = playback.m_callbackMap.find(callbackId);
                if (foundCallback == playback.m_callbackMap.end())
                    return;

                foundCallback->value->invoke(available);
            });

            // 8.2 Run the algorithm to monitor the list of available remote playback devices.
            if (protectedThis->m_mediaElement) {
                protectedThis->availabilityChanged(protectedThis->m_mediaElement->mediaSession().hasWirelessPlaybackTargets());
                protectedThis->m_mediaElement->remoteHasAvailabilityCallbacksChanged();
            }
        });
        promise->resolve<IDLLong>(callbackId);
    });
}

void RemotePlayback::cancelWatchAvailability(std::optional<int32_t> id, Ref<DeferredPromise>&& promise)
{
    // 6.2.1.5 Stop observing remote playback devices availability
    // https://w3c.github.io/remote-playback/#stop-observing-remote-playback-devices-availability
    // W3C Editor's Draft 15 July 2016

    // 1. Let promise be a new promise->
    // 2. Return promise, and run the following steps below:

    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [id = WTFMove(id), promise = WTFMove(promise), identifier = identifier](auto& playback) {
        if (playback.isContextStopped())
            return;
        // 3. If the disableRemotePlayback attribute is present for the media element, reject promise with
        //    InvalidStateError and abort all the remaining steps.
        if (!playback.m_mediaElement || playback.m_mediaElement->isWirelessPlaybackTargetDisabled()) {
            ERROR_LOG_WITH_THIS(&playback, identifier, "promise rejected, remote playback disabled");
            promise->reject(ExceptionCode::InvalidStateError);
            return;
        }

        // 4. If the parameter id is undefined, clear the set of availability callbacks.
        if (!id)
            playback.m_callbackMap.clear();
        else if (!playback.m_callbackMap.isValidKey(*id)) {
            ERROR_LOG_WITH_THIS(&playback, identifier, "promise rejected, invalid identifier");
            promise->reject(ExceptionCode::NotFoundError);
            return;
        } else {
            // 5. Otherwise, if id matches the callbackId for any entry in the set of availability callbacks,
            //    remove the entry from the set.
            if (!playback.m_callbackMap.remove(id.value())) {
                // 6. Otherwise, reject promise with NotFoundError and abort all the remaining steps.
                ERROR_LOG_WITH_THIS(&playback, identifier, "promise rejected, no matching callback");
                promise->reject(ExceptionCode::NotFoundError);
                return;
            }
        }
        // 7. If the set of availability callbacks is now empty and there is no pending request to initiate remote
        //    playback, cancel any pending task to monitor the list of available remote playback devices for power
        //    saving purposes.
        playback.m_mediaElement->remoteHasAvailabilityCallbacksChanged();

        // 8. Fulfill promise.
        promise->resolve();
    });
}

void RemotePlayback::prompt(Ref<DeferredPromise>&& promise)
{
    // 6.2.2 Prompt user for changing remote playback statee
    // https://w3c.github.io/remote-playback/#stop-observing-remote-playback-devices-availability
    // W3C Editor's Draft 15 July 2016

    // 1. Let promise be a new promise->
    // 2. Return promise, and run the following steps below:

    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [promise = WTFMove(promise), processingUserGesture = UserGestureIndicator::processingUserGesture(), identifier = identifier](auto& playback) mutable {
        if (playback.isContextStopped())
            return;

        // 3. If the disableRemotePlayback attribute is present for the media element, reject the promise with
        //    InvalidStateError and abort all the remaining steps.
        if (!playback.m_mediaElement || playback.m_mediaElement->isWirelessPlaybackTargetDisabled()) {
            ERROR_LOG_WITH_THIS(&playback, identifier, "promise rejected, remote playback disabled");
            promise->reject(ExceptionCode::InvalidStateError);
            return;
        }

        // 4. If there is already an unsettled promise from a previous call to prompt for the same media element
        //     or even for the same browsing context, the user agent may reject promise with an OperationError
        //     exception and abort all remaining steps.
        // NOTE: consider implementing

        // 5. OPTIONALLY, if the user agent knows a priori that showing the UI for this particular media element
        //    is not feasible, reject promise with a NotSupportedError and abort all remaining steps.
#if !(PLATFORM(IOS) || PLATFORM(VISION))
        if (playback.m_mediaElement->readyState() < HTMLMediaElementEnums::HAVE_METADATA) {
            ERROR_LOG_WITH_THIS(&playback, identifier, "promise rejected, readyState = ", playback.m_mediaElement->readyState());
            promise->reject(ExceptionCode::NotSupportedError);
            return;
        }
#endif

        // 6. If the algorithm isn't allowed to show a popup, reject promise with an InvalidAccessError exception
        //    and abort these steps.
        if (!processingUserGesture) {
            ERROR_LOG_WITH_THIS(&playback, identifier, "promise rejected, user gesture required");
            promise->reject(ExceptionCode::InvalidAccessError);
            return;
        }

        // 7. If the user agent needs to show the list of available remote playback devices and is not monitoring
        //    the list of available remote playback devices, run the steps to monitor the list of available remote
        //    playback devices in parallel.
        // NOTE: Monitoring enabled by adding to m_promptPromises and calling remoteHasAvailabilityCallbacksChanged().

        // 8. If the list of available remote playback devices is empty and will remain so before the request for
        //    user permission is completed, reject promise with a NotFoundError exception and abort all remaining steps.
        // NOTE: consider implementing (no network?)

        // 9. If the state is disconnected and availability for the media element is false, reject promise with a
        //    NotSupportedError exception and abort all remaining steps.
        if (playback.m_state == State::Disconnected && !playback.m_available) {
            ERROR_LOG_WITH_THIS(&playback, identifier, "promise rejected, state = ", playback.m_state, ", available = ", playback.m_available);
            promise->reject(ExceptionCode::NotSupportedError);
            return;
        }

        playback.m_promptPromises.append(WTFMove(promise));
        playback.availabilityChanged(playback.m_mediaElement->mediaSession().hasWirelessPlaybackTargets());
        playback.m_mediaElement->remoteHasAvailabilityCallbacksChanged();
        playback.m_mediaElement->webkitShowPlaybackTargetPicker();

        // NOTE: Steps 10-12 are implemented in the following methods:
    });
}

void RemotePlayback::shouldPlayToRemoteTargetChanged(bool shouldPlayToRemoteTarget)
{
    // 6.2.2 Prompt user for changing remote playback state [Ctd]
    // https://w3c.github.io/remote-playback/#prompt-user-for-changing-remote-playback-statee
    // W3C Editor's Draft 15 July 2016

    ALWAYS_LOG(LOGIDENTIFIER, "shouldPlay = ", shouldPlayToRemoteTarget, ", promise count = ", m_promptPromises.size());

    // 10. If the user picked a remote playback device device to initiate remote playback with, the user agent
    //     must run the following steps:
    if (shouldPlayToRemoteTarget) {
        // 10.1 Set the state of the remote object to connecting.
        // 10.3 Queue a task to fire a simple event with the name connecting at the remote property of the media element.
        //      The event must not bubble, must not be cancelable, and has no default action.
        setState(State::Connecting);
    }

    for (auto& promise : std::exchange(m_promptPromises, { })) {
        // 10.2 Fulfill promise.
        // 10.4 Establish a connection with the remote playback device device for the media element.
        // NOTE: Implemented in establishConnection().

        // 11. Otherwise, if the user chose to disconnect from the remote playback device device, the user agent
        //     must run the following steps:
        // 11.1. Fulfill promise.
        // 11.2. Run the disconnect from remote playback device algorithm for the device.
        // NOTE: Implemented in disconnect().

        promise->resolve();
    }

    if (shouldPlayToRemoteTarget)
        establishConnection();
    else
        disconnect();

    if (RefPtr mediaElement = m_mediaElement.get())
        mediaElement->remoteHasAvailabilityCallbacksChanged();
}

void RemotePlayback::setState(State state)
{
    if (m_state == state)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, state);
    m_state = state;

    auto eventName = [](State state) {
        switch (state) {
        case State::Connected:
            return eventNames().connectEvent;
        case State::Connecting:
            return eventNames().connectingEvent;
        case State::Disconnected:
            return eventNames().disconnectEvent;
        }
    };

    queueTaskToDispatchEvent(*this, TaskSource::MediaElement, Event::create(eventName(state), Event::CanBubble::No, Event::IsCancelable::No));
}

void RemotePlayback::establishConnection()
{
    // 6.2.4 Establishing a connection with a remote playback device
    // https://w3c.github.io/remote-playback/#establishing-a-connection-with-a-remote-playback-device
    // W3C Editor's Draft 15 July 2016

    // 1. If the state of remote is not equal to connecting, abort all the remaining steps.
    if (m_state != State::Connecting)
        return;

    // 2. Request connection of remote to device. The implementation of this step is specific to the user agent.
    // NOTE: Handled in MediaPlayer.

    // NOTE: Continued in isPlayingToRemoteTargetChanged()
}

void RemotePlayback::disconnect()
{
    // 6.2.6 Disconnecting from remote playback device
    // https://w3c.github.io/remote-playback/#dfn-disconnect-from-remote-playback-device
    // W3C Editor's Draft 15 July 2016

    // 1. If the state of remote is disconnected, abort all remaining steps.
    if (m_state == State::Disconnected)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    // 2. Queue a task to run the following steps:
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [](auto& playback) {
        if (playback.isContextStopped())
            return;

        // 2.1 Request disconnection of remote from the device. Implementation is user agent specific.
        // NOTE: Implemented by MediaPlayer::setWirelessPlaybackTarget()
        // 2.2 Change the remote's state to disconnected.
        // 2.3 Fire an event with the name disconnect at remote.
        playback.setState(State::Disconnected);

        // 2.4 Synchronize the current media element state with the local playback state. Implementation is
        //     specific to user agent.
        // NOTE: Handled by the MediaPlayer
    });
}

ScriptExecutionContext* RemotePlayback::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void RemotePlayback::stop()
{
    m_callbackMap.clear();
}

void RemotePlayback::playbackTargetPickerWasDismissed()
{
    if (m_promptPromises.isEmpty())
        return;

    // 6.2.2 Prompt user for changing remote playback state [Ctd]
    // https://w3c.github.io/remote-playback/#stop-observing-remote-playback-devices-availability
    // W3C Editor's Draft 15 July 2016

    // 12. Otherwise, the user is considered to deny permission to use the device, so reject promise with NotAllowedError
    // exception and hide the UI shown by the user agent
    ASSERT(!m_promptPromises.isEmpty());

    for (auto& promise : std::exchange(m_promptPromises, { }))
        promise->reject(ExceptionCode::NotAllowedError);

    if (RefPtr mediaElement = m_mediaElement.get())
        mediaElement->remoteHasAvailabilityCallbacksChanged();
}

void RemotePlayback::isPlayingToRemoteTargetChanged(bool isPlayingToTarget)
{
    // 6.2.4 Establishing a connection with a remote playback device [Ctd]
    // https://w3c.github.io/remote-playback/#establishing-a-connection-with-a-remote-playback-device
    // W3C Editor's Draft 15 July 2016

    // 3. If connection completes successfully, queue a task to run the following steps:
    if (isPlayingToTarget) {
        // 3.1. Set the state of remote to connected.
        // 3.2. Fire a simple event named connect at remote.
        setState(State::Connected);

        // 3.3 Synchronize the current media element state with the remote playback state. Implementation is
        //     specific to user agent.
        // NOTE: Implemented by MediaPlayer.
        return;
    }

    // 4. If connection fails, queue a task to run the following steps:
    // 4.1. Set the remote playback state of remote to disconnected.
    // 4.2. Fire a simple event named disconnect at remote.
    setState(State::Disconnected);
}

bool RemotePlayback::hasAvailabilityCallbacks() const
{
    return !m_callbackMap.isEmpty() || !m_promptPromises.isEmpty();
}

void RemotePlayback::availabilityChanged(bool available)
{
    if (available == m_available)
        return;
    m_available = available;

    ALWAYS_LOG(LOGIDENTIFIER);

    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [available](auto& playback) {
        if (playback.isContextStopped())
            return;

        // Protect m_callbackMap against mutation while it's being iterated over.
        for (auto& callback : copyToVector(playback.m_callbackMap.values()))
            callback->invoke(available);
    });
}

void RemotePlayback::invalidate()
{
    m_mediaElement = nullptr;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RemotePlayback::logChannel() const
{
    return LogMedia;
}
#endif

}

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
