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

#include "JSDOMException.h"
#include "JSDOMPromiseDeferred.h"
#include "Logging.h"
#include "MediaSession.h"
#include "PlatformMediaSessionCoordinator.h"

namespace WebCore {

static const void* nextCoordinatorLogIdentifier()
{
    static uint64_t logIdentifier = cryptographicallyRandomNumber();
    return reinterpret_cast<const void*>(++logIdentifier);
}

Ref<MediaSessionCoordinator> MediaSessionCoordinator::create(Ref<PlatformMediaSessionCoordinator>&& platformCoordinator)
{
    return adoptRef(*new MediaSessionCoordinator(WTFMove(platformCoordinator)));
}

MediaSessionCoordinator::MediaSessionCoordinator(Ref<PlatformMediaSessionCoordinator>&& platformCoordinator)
    : m_platformCoordinator(WTFMove(platformCoordinator))
    , m_logger(makeRef(Document::sharedLogger()))
    , m_logIdentifier(nextCoordinatorLogIdentifier())
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_platformCoordinator->setLogger(m_logger.copyRef(), m_logIdentifier);
}

MediaSessionCoordinator::~MediaSessionCoordinator() = default;

void MediaSessionCoordinator::seekTo(double time, DOMPromiseDeferred<void>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier, time);

    if (!m_session) {
        ERROR_LOG(identifier, "MediaSession is NULL!");
        promise.reject(Exception { InvalidStateError });
        return;
    }

    m_platformCoordinator->seekTo(time, [time, protectedThis = makeRefPtr(*this), identifier, promise = WTFMove(promise)] (Optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            promise.reject(WTFMove(*exception));
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.seekTo failed!");
            return;
        }

        MediaSessionActionDetails details {
            .action = MediaSessionAction::Seekto,
            .seekTime = time,
        };
        protectedThis->m_session->callActionHandler(details);

        // FIXME: should the promise reject if there isn't a registered 'seekTo' action handler?
        promise.resolve();
    });
}

void MediaSessionCoordinator::play(DOMPromiseDeferred<void>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    if (!m_session) {
        ERROR_LOG(identifier, "MediaSession is NULL!");
        promise.reject(Exception { InvalidStateError });
        return;
    }

    m_platformCoordinator->play([protectedThis = makeRefPtr(*this), identifier, promise = WTFMove(promise)] (Optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            promise.reject(WTFMove(*exception));
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.play failed!");
            return;
        }

        MediaSessionActionDetails details {
            .action = MediaSessionAction::Play,
        };
        protectedThis->m_session->callActionHandler(details);

        // FIXME: should the promise reject if there isn't a registered 'play' action handler?
        promise.resolve();
    });
}

void MediaSessionCoordinator::pause(DOMPromiseDeferred<void>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    if (!m_session) {
        ERROR_LOG(identifier, "MediaSession is NULL!");
        promise.reject(Exception { InvalidStateError });
        return;
    }

    m_platformCoordinator->pause([protectedThis = makeRefPtr(*this), identifier, promise = WTFMove(promise)] (Optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            promise.reject(WTFMove(*exception));
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.pause failed!");
            return;
        }

        MediaSessionActionDetails details {
            .action = MediaSessionAction::Pause,
        };
        protectedThis->m_session->callActionHandler(details);

        // FIXME: should the promise reject if there isn't a registered 'pause' action handler?
        promise.resolve();
    });
}

void MediaSessionCoordinator::setTrack(const String& track, DOMPromiseDeferred<void>&& promise)
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG(identifier);

    if (!m_session) {
        ERROR_LOG(identifier, "MediaSession is NULL!");
        promise.reject(Exception { InvalidStateError });
        return;
    }

    m_platformCoordinator->setTrack(track, [track, protectedThis = makeRefPtr(*this), identifier, promise = WTFMove(promise)] (Optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            promise.reject(WTFMove(*exception));
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.setTrack failed!");
            return;
        }

        MediaSessionActionDetails details {
            .action = MediaSessionAction::Settrack,
            .trackIdentifier = track,
        };
        protectedThis->m_session->callActionHandler(details);

        // FIXME: should the promise reject if there isn't a registered 'setTrack' action handler?
        promise.resolve();
    });
}

void MediaSessionCoordinator::setMediaSession(MediaSession* session)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_session = makeWeakPtr(session);

    if (m_session)
        m_session->addObserver(*this);
}

void MediaSessionCoordinator::positionStateChanged(Optional<MediaPositionState> state)
{
    if (!state) {
        m_platformCoordinator->positionStateChanged({ });
        return;
    }

    m_platformCoordinator->positionStateChanged(PlatformMediaSessionCoordinator::MediaPositionState { state->duration, state->playbackRate, state->position });
}

void MediaSessionCoordinator::playbackStateChanged(MediaSessionPlaybackState state)
{
    static_assert(static_cast<uint8_t>(MediaSessionPlaybackState::None) == static_cast<uint8_t>(PlatformMediaSessionCoordinator::MediaSessionPlaybackState::None), "MediaSessionPlaybackState::None is not PlatformMediaSessionCoordinator::MediaSessionPlaybackState::None as expected");
    static_assert(static_cast<uint8_t>(MediaSessionPlaybackState::Paused) == static_cast<uint8_t>(PlatformMediaSessionCoordinator::MediaSessionPlaybackState::Paused), "MediaSessionPlaybackState::Paused is not PlatformMediaSessionCoordinator::MediaSessionPlaybackState::Paused as expected");
    static_assert(static_cast<uint8_t>(MediaSessionPlaybackState::Playing) == static_cast<uint8_t>(PlatformMediaSessionCoordinator::MediaSessionPlaybackState::Playing), "MediaSessionPlaybackState::Playing is not PlatformMediaSessionCoordinator::MediaSessionPlaybackState::Playing as expected");

    m_platformCoordinator->playbackStateChanged(static_cast<PlatformMediaSessionCoordinator::MediaSessionPlaybackState>(state));
}

void MediaSessionCoordinator::readyStateChanged(MediaSessionReadyState state)
{
    static_assert(static_cast<uint8_t>(MediaSessionReadyState::HaveNothing) == static_cast<uint8_t>(PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveNothing), "MediaSessionReadyState::HaveNothing is not PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveNothing as expected");
    static_assert(static_cast<uint8_t>(MediaSessionReadyState::HaveMetadata) == static_cast<uint8_t>(PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveMetadata), "MediaSessionReadyState::HaveMetadata is not PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveMetadata as expected");
    static_assert(static_cast<uint8_t>(MediaSessionReadyState::HaveCurrentData) == static_cast<uint8_t>(PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveCurrentData), "MediaSessionReadyState::HaveCurrentData is not PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveCurrentData as expected");
    static_assert(static_cast<uint8_t>(MediaSessionReadyState::HaveFutureData) == static_cast<uint8_t>(PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveFutureData), "MediaSessionReadyState::HaveFutureData is not PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveFutureData as expected");
    static_assert(static_cast<uint8_t>(MediaSessionReadyState::HaveEnoughData) == static_cast<uint8_t>(PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveEnoughData), "MediaSessionReadyState::HaveEnoughData is not PlatformMediaSessionCoordinator::MediaSessionReadyState::HaveEnoughData as expected");

    m_platformCoordinator->readyStateChanged(static_cast<PlatformMediaSessionCoordinator::MediaSessionReadyState>(state));
}

WTFLogChannel& MediaSessionCoordinator::logChannel()
{
    return LogMedia;
}

}

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
