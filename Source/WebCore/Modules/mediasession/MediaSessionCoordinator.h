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

#pragma once

#if ENABLE(MEDIA_SESSION_COORDINATOR)

#include "MediaSession.h"
#include "MediaSessionCoordinatorPrivate.h"
#include "MediaSessionCoordinatorState.h"
#include <wtf/Logger.h>
#include <wtf/Optional.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

template<typename> class DOMPromiseDeferred;

class MediaSessionCoordinator
    : public RefCounted<MediaSessionCoordinator>
    , public MediaSessionCoordinatorClient
    , public MediaSession::Observer {
public:
    WEBCORE_EXPORT static Ref<MediaSessionCoordinator> create(Ref<MediaSessionCoordinatorPrivate>&&);
    WEBCORE_EXPORT ~MediaSessionCoordinator();

    void join(DOMPromiseDeferred<void>&&);
    ExceptionOr<void> leave();

    String identifier() const { return m_privateCoordinator->identifier(); }
    MediaSessionCoordinatorState state() const { return m_state; }

    void seekTo(double, DOMPromiseDeferred<void>&&);
    void play(DOMPromiseDeferred<void>&&);
    void pause(DOMPromiseDeferred<void>&&);
    void setTrack(const String&, DOMPromiseDeferred<void>&&);

    void setMediaSession(MediaSession*);

    using MediaSessionCoordinatorClient::weakPtrFactory;
    using WeakValueType = MediaSessionCoordinatorClient::WeakValueType;

private:
    explicit MediaSessionCoordinator(Ref<MediaSessionCoordinatorPrivate>&&);

    // MediaSession::Observer
    void positionStateChanged(const Optional<MediaPositionState>&) final;
    void playbackStateChanged(MediaSessionPlaybackState) final;
    void readyStateChanged(MediaSessionReadyState) final;

    // MediaSessionCoordinatorClient
    void seekSessionToTime(double, CompletionHandler<void(bool)>&&) final;
    void playSession(Optional<double> atTime, Optional<double> hostTime, CompletionHandler<void(bool)>&&) final;
    void pauseSession(CompletionHandler<void(bool)>&&) final;
    void setSessionTrack(const String&, CompletionHandler<void(bool)>&&) final;

    const Logger& logger() const { return m_logger; }
    const void* logIdentifier() const { return m_logIdentifier; }
    static WTFLogChannel& logChannel();
    static const char* logClassName() { return "MediaSessionCoordinator"; }

    WeakPtr<MediaSession> m_session;
    MediaSessionCoordinatorState m_state { MediaSessionCoordinatorState::Waiting };
    Ref<MediaSessionCoordinatorPrivate> m_privateCoordinator;
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
};

}

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
