/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "MediaSessionCoordinator.h"

#if ENABLE(MEDIA_SESSION_COORDINATOR)

#include "EventNames.h"
#include "JSDOMException.h"
#include "JSDOMPromiseDeferred.h"
#include "JSMediaSessionCoordinatorState.h"
#include "Logging.h"
#include "MediaMetadata.h"
#include "MediaSession.h"
#include "MediaSessionCoordinatorPrivate.h"
#include <wtf/CompletionHandler.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/Logger.h>
#include <wtf/LoggerHelper.h>
#include <wtf/Seconds.h>

static const Seconds CommandTimeTolerance = 50_ms;

namespace WebCore {

static const void* nextCoordinatorLogIdentifier()
{
    static uint64_t logIdentifier = cryptographicallyRandomNumber<uint32_t>();
    return reinterpret_cast<const void*>(++logIdentifier);
}

Ref<MediaSessionCoordinator> MediaSessionCoordinator::create(ScriptExecutionContext* context)
{
    auto coordinator = adoptRef(*new MediaSessionCoordinator(context));
    coordinator->suspendIfNeeded();
    return coordinator;
}

MediaSessionCoordinator::MediaSessionCoordinator(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
    , m_logger(Document::sharedLogger())
    , m_logIdentifier(nextCoordinatorLogIdentifier())
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

void MediaSessionCoordinator::setMediaSessionCoordinatorPrivate(Ref<MediaSessionCoordinatorPrivate>&& privateCoordinator)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_privateCoordinator)
        m_privateCoordinator->leave();
    m_privateCoordinator = WTFMove(privateCoordinator);
    m_privateCoordinator->setLogger(m_logger.copyRef(), m_logIdentifier);
    m_privateCoordinator->setClient(*this);
    coordinatorStateChanged(MediaSessionCoordinatorState::Waiting);
}

MediaSessionCoordinator::~MediaSessionCoordinator() = default;

void MediaSessionCoordinator::eventListenersDidChange()
{
    m_hasCoordinatorsStateChangeEventListener = hasEventListeners(eventNames().coordinatorstatechangeEvent);
}

bool MediaSessionCoordinator::virtualHasPendingActivity() const
{
    // Need to keep the JS wrapper alive as long as it may still fire events in the future.
    return shouldFireEvents();
}

void MediaSessionCoordinator::join(DOMPromiseDeferred<void>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier, m_state);

    if (m_state != MediaSessionCoordinatorState::Waiting) {
        ERROR_LOG(identifier, "invalid state");
        promise.reject(Exception { ExceptionCode::InvalidStateError, makeString("Unable to join when state is ", convertEnumerationToString(m_state)) });
        return;
    }
    ASSERT(m_privateCoordinator, "We must be in Waiting state if no private coordinator is set");

    m_privateCoordinator->join([protectedThis = Ref { *this }, identifier, promise = WTFMove(promise)] (std::optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { ExceptionCode::InvalidStateError });
            return;
        }

        if (exception) {
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.join failed!");
            promise.reject(WTFMove(*exception));
            return;
        }

        protectedThis->coordinatorStateChanged(MediaSessionCoordinatorState::Joined);

        promise.resolve();
    });
}

ExceptionOr<void> MediaSessionCoordinator::leave()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_state != MediaSessionCoordinatorState::Joined)
        return Exception { ExceptionCode::InvalidStateError, makeString("Unable to leave when state is ", convertEnumerationToString(m_state)) };

    close();

    return { };
}

void MediaSessionCoordinator::close()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    coordinatorStateChanged(MediaSessionCoordinatorState::Closed);
    if (!m_privateCoordinator)
        return;

    m_privateCoordinator->leave();
    m_privateCoordinator = nullptr;
}

void MediaSessionCoordinator::seekTo(double time, DOMPromiseDeferred<void>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier, time);

    if (!m_session) {
        ERROR_LOG(identifier, "MediaSession is NULL!");
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    if (m_state != MediaSessionCoordinatorState::Joined) {
        ERROR_LOG(identifier, ".state is ", m_state);
        promise.reject(Exception { ExceptionCode::InvalidStateError, makeString("Unable to seekTo when state is ", convertEnumerationToString(m_state)) });
        return;
    }

    m_privateCoordinator->seekTo(time, [protectedThis = Ref { *this }, identifier, promise = WTFMove(promise)] (std::optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { ExceptionCode::InvalidStateError });
            return;
        }

        if (exception) {
            promise.reject(WTFMove(*exception));
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.seekTo failed!");
            return;
        }

        promise.resolve();
    });
}

void MediaSessionCoordinator::play(DOMPromiseDeferred<void>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    if (!m_session) {
        ERROR_LOG(identifier, "MediaSession is NULL!");
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    if (m_state != MediaSessionCoordinatorState::Joined) {
        ERROR_LOG(identifier, ".state is ", m_state);
        promise.reject(Exception { ExceptionCode::InvalidStateError, makeString("Unable to play when state is ", convertEnumerationToString(m_state)) });
        return;
    }

    m_privateCoordinator->play([protectedThis = Ref { *this }, identifier, promise = WTFMove(promise)] (std::optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { ExceptionCode::InvalidStateError });
            return;
        }

        if (exception) {
            promise.reject(WTFMove(*exception));
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.play failed!");
            return;
        }

        promise.resolve();
    });
}

void MediaSessionCoordinator::pause(DOMPromiseDeferred<void>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    if (!m_session) {
        ERROR_LOG(identifier, "MediaSession is NULL!");
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    if (m_state != MediaSessionCoordinatorState::Joined) {
        ERROR_LOG(identifier, ".state is ", m_state);
        promise.reject(Exception { ExceptionCode::InvalidStateError, makeString("Unable to pause when state is ", convertEnumerationToString(m_state)) });
        return;
    }

    m_privateCoordinator->pause([protectedThis = Ref { *this }, identifier, promise = WTFMove(promise)] (std::optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { ExceptionCode::InvalidStateError });
            return;
        }

        if (exception) {
            promise.reject(WTFMove(*exception));
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.pause failed!");
            return;
        }

        promise.resolve();
    });
}

void MediaSessionCoordinator::setTrack(const String& track, DOMPromiseDeferred<void>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    if (!m_session) {
        ERROR_LOG(identifier, "MediaSession is NULL!");
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    if (m_state != MediaSessionCoordinatorState::Joined) {
        ERROR_LOG(identifier, ".state is ", m_state);
        promise.reject(Exception { ExceptionCode::InvalidStateError, makeString("Unable to setTrack when state is ", convertEnumerationToString(m_state)) });
        return;
    }

    m_privateCoordinator->setTrack(track, [protectedThis = Ref { *this }, identifier, promise = WTFMove(promise)] (std::optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { ExceptionCode::InvalidStateError });
            return;
        }

        if (exception) {
            promise.reject(WTFMove(*exception));
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.setTrack failed!");
            return;
        }

        promise.resolve();
    });
}

void MediaSessionCoordinator::setMediaSession(MediaSession* session)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_session = session;

    if (m_session)
        m_session->addObserver(*this);
}

void MediaSessionCoordinator::metadataChanged(const RefPtr<MediaMetadata>& metadata)
{
#if ENABLE(MEDIA_SESSION_PLAYLIST)
    if (!m_privateCoordinator)
        return;

    auto identifier = metadata ? metadata->trackIdentifier() : emptyString();
    ALWAYS_LOG(LOGIDENTIFIER, m_state, ", trackIdentifier:", identifier);
    m_privateCoordinator->trackIdentifierChanged(identifier);
#endif
}

void MediaSessionCoordinator::positionStateChanged(const std::optional<MediaPositionState>& positionState)
{
    if (positionState)
        ALWAYS_LOG(LOGIDENTIFIER, positionState.value());
    else
        ALWAYS_LOG(LOGIDENTIFIER, "{ }");

    if (m_state != MediaSessionCoordinatorState::Joined)
        return;

    if (!m_privateCoordinator)
        return;

    if (!positionState) {
        m_privateCoordinator->positionStateChanged({ });
        return;
    }

    m_privateCoordinator->positionStateChanged(MediaPositionState { positionState->duration, positionState->playbackRate, positionState->position });
}

void MediaSessionCoordinator::playbackStateChanged(MediaSessionPlaybackState playbackState)
{
    ALWAYS_LOG(LOGIDENTIFIER, m_state, ", ", playbackState);

    if (m_state != MediaSessionCoordinatorState::Joined)
        return;

    if (!m_privateCoordinator)
        return;

    m_privateCoordinator->playbackStateChanged(playbackState);
}

void MediaSessionCoordinator::readyStateChanged(MediaSessionReadyState readyState)
{
    ALWAYS_LOG(LOGIDENTIFIER, m_state, ", ", readyState);

    if (m_state != MediaSessionCoordinatorState::Joined)
        return;

    if (!m_privateCoordinator)
        return;

    m_privateCoordinator->readyStateChanged(readyState);
}

void MediaSessionCoordinator::seekSessionToTime(double time, CompletionHandler<void(bool)>&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER, m_state, ", ", time);

    if (m_state != MediaSessionCoordinatorState::Joined) {
        completionHandler(false);
        return;
    }

    bool isPaused = m_session->playbackState() == MediaSessionPlaybackState::Paused;

    if (isPaused && currentPositionApproximatelyEqualTo(time)) {
        completionHandler(true);
        return;
    }

    if (!isPaused)
        m_session->callActionHandler({ .action = MediaSessionAction::Pause });

    m_session->callActionHandler({ .action = MediaSessionAction::Seekto, .seekTime = time });
    completionHandler(true);
}

void MediaSessionCoordinator::playSession(std::optional<double> atTime, std::optional<MonotonicTime> hostTime, CompletionHandler<void(bool)>&& completionHandler)
{
    auto now = MonotonicTime::now();
    auto delta = hostTime ? *hostTime - now : Seconds::nan();
    ALWAYS_LOG(LOGIDENTIFIER, m_state, " time: ", atTime ? *atTime : -1, " hostTime: ", (hostTime ? *hostTime : MonotonicTime::nan()).secondsSinceEpoch().value(), " delta: ", delta.value());

    if (m_state != MediaSessionCoordinatorState::Joined) {
        completionHandler(false);
        return;
    }

    if (atTime && !currentPositionApproximatelyEqualTo(*atTime))
        m_session->callActionHandler({ .action = MediaSessionAction::Seekto, .seekTime = *atTime });

    m_currentPlaySessionCommand = { atTime, hostTime };
    m_session->callActionHandler({ .action = MediaSessionAction::Play });
    completionHandler(true);
}

void MediaSessionCoordinator::pauseSession(CompletionHandler<void(bool)>&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER, m_state);

    if (m_state != MediaSessionCoordinatorState::Joined) {
        completionHandler(false);
        return;
    }

    m_session->callActionHandler({ .action = MediaSessionAction::Pause });
    completionHandler(true);
}

void MediaSessionCoordinator::setSessionTrack(const String& track, CompletionHandler<void(bool)>&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER, m_state, ", ", track);

    if (m_state != MediaSessionCoordinatorState::Joined) {
        completionHandler(false);
        return;
    }

    m_session->callActionHandler({ .action = MediaSessionAction::Settrack, .trackIdentifier = track });
    completionHandler(true);
}

bool MediaSessionCoordinator::shouldFireEvents() const
{
    return m_hasCoordinatorsStateChangeEventListener && m_session;
}


void MediaSessionCoordinator::coordinatorStateChanged(MediaSessionCoordinatorState state)
{
    if (m_state == state)
        return;
    m_state = state;
    ALWAYS_LOG(LOGIDENTIFIER, m_state);
    if (shouldFireEvents())
        queueTaskToDispatchEvent(*this, TaskSource::MediaElement, Event::create(eventNames().coordinatorstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

bool MediaSessionCoordinator::currentPositionApproximatelyEqualTo(double time) const
{
    if (!m_session)
        return false;

    auto currentPosition = m_session->currentPosition();
    if (!currentPosition)
        return false;

    auto delta = Seconds(std::abs(*currentPosition - time));
    return delta <= CommandTimeTolerance;
}

WTFLogChannel& MediaSessionCoordinator::logChannel()
{
    return LogMedia;
}

}

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
