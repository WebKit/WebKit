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
#include "MediaSessionCoordinatorPrivate.h"
#include <wtf/Logger.h>

namespace WebCore {

static const void* nextCoordinatorLogIdentifier()
{
    static uint64_t logIdentifier = cryptographicallyRandomNumber();
    return reinterpret_cast<const void*>(++logIdentifier);
}

Ref<MediaSessionCoordinator> MediaSessionCoordinator::create(Ref<MediaSessionCoordinatorPrivate>&& privateCoordinator)
{
    return adoptRef(*new MediaSessionCoordinator(WTFMove(privateCoordinator)));
}

MediaSessionCoordinator::MediaSessionCoordinator(Ref<MediaSessionCoordinatorPrivate>&& privateCoordinator)
    : m_privateCoordinator(WTFMove(privateCoordinator))
    , m_logger(makeRef(Document::sharedLogger()))
    , m_logIdentifier(nextCoordinatorLogIdentifier())
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_privateCoordinator->setLogger(m_logger.copyRef(), m_logIdentifier);
    m_privateCoordinator->setClient(makeWeakPtr(this));
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

    m_privateCoordinator->seekTo(time, [time, protectedThis = makeRefPtr(*this), identifier, promise = WTFMove(promise)] (Optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            promise.reject(WTFMove(*exception));
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.seekTo failed!");
            return;
        }

        // FIXME: should the promise reject if there isn't a registered 'seekTo' action handler?
        protectedThis->internalSeekTo(time);

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

    m_privateCoordinator->play([protectedThis = makeRefPtr(*this), identifier, promise = WTFMove(promise)] (Optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            promise.reject(WTFMove(*exception));
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.play failed!");
            return;
        }

        // FIXME: should the promise reject if there isn't a registered 'play' action handler?
        protectedThis->internalPlay();

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

    m_privateCoordinator->pause([protectedThis = makeRefPtr(*this), identifier, promise = WTFMove(promise)] (Optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            promise.reject(WTFMove(*exception));
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.pause failed!");
            return;
        }

        // FIXME: should the promise reject if there isn't a registered 'pause' action handler?
        protectedThis->internalPause();

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

    m_privateCoordinator->setTrack(track, [track, protectedThis = makeRefPtr(*this), identifier, promise = WTFMove(promise)] (Optional<Exception>&& exception) mutable {
        if (!protectedThis->m_session) {
            promise.reject(Exception { InvalidStateError });
            return;
        }

        if (exception) {
            promise.reject(WTFMove(*exception));
            protectedThis->logger().error(protectedThis->logChannel(), identifier, "coordinator.setTrack failed!");
            return;
        }

        // FIXME: should the promise reject if there isn't a registered 'setTrack' action handler?
        protectedThis->internalSetTrack(track);

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
        m_privateCoordinator->positionStateChanged({ });
        return;
    }

    m_privateCoordinator->positionStateChanged(MediaPositionState { state->duration, state->playbackRate, state->position });
}

void MediaSessionCoordinator::playbackStateChanged(MediaSessionPlaybackState state)
{
    m_privateCoordinator->playbackStateChanged(state);
}

void MediaSessionCoordinator::readyStateChanged(MediaSessionReadyState state)
{
    m_privateCoordinator->readyStateChanged(state);
}

void MediaSessionCoordinator::seekSessionToTime(double time, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(internalSeekTo(time));
}

void MediaSessionCoordinator::playSession(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(internalPlay());
}

void MediaSessionCoordinator::pauseSession(CompletionHandler<void(bool)> completionHandler)
{
    completionHandler(internalPause());
}

void MediaSessionCoordinator::setSessionTrack(const String& track, CompletionHandler<void(bool)> completionHandler)
{
    completionHandler(internalSetTrack(track));
}

bool MediaSessionCoordinator::internalSeekTo(double time)
{
    if (!m_session)
        return false;

    MediaSessionActionDetails details {
        .action = MediaSessionAction::Seekto,
        .seekTime = time,
    };
    m_session->callActionHandler(details);

    return true;
}

bool MediaSessionCoordinator::internalPlay()
{
    if (!m_session)
        return false;

    MediaSessionActionDetails details {
        .action = MediaSessionAction::Play,
    };
    m_session->callActionHandler(details);

    return true;
}

bool MediaSessionCoordinator::internalPause()
{
    if (!m_session)
        return false;

    MediaSessionActionDetails details {
        .action = MediaSessionAction::Pause,
    };
    m_session->callActionHandler(details);

    return true;
}

bool MediaSessionCoordinator::internalSetTrack(const String& track)
{
    if (!m_session)
        return false;

    MediaSessionActionDetails details {
        .action = MediaSessionAction::Settrack,
        .trackIdentifier = track,
    };
    m_session->callActionHandler(details);

    return true;
}

WTFLogChannel& MediaSessionCoordinator::logChannel()
{
    return LogMedia;
}

}

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
