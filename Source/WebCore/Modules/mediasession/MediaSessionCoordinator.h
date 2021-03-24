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

#include <wtf/Logger.h>
#include <wtf/Optional.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

template<typename> class DOMPromiseDeferred;

class PlatformMediaSessionCoordinator;
class MediaSession;

class MediaSessionCoordinator : public RefCounted<MediaSessionCoordinator>, public MediaSession::Observer {
public:
    WEBCORE_EXPORT static Ref<MediaSessionCoordinator> create(Ref<PlatformMediaSessionCoordinator>&&);
    WEBCORE_EXPORT ~MediaSessionCoordinator();

    void seekTo(double, DOMPromiseDeferred<void>&&);
    void play(DOMPromiseDeferred<void>&&);
    void pause(DOMPromiseDeferred<void>&&);
    void setTrack(const String&, DOMPromiseDeferred<void>&&);

    void setMediaSession(MediaSession*);

    // MediaSession::Observer
    void positionStateChanged(Optional<MediaPositionState>) final;
    void playbackStateChanged(MediaSessionPlaybackState) final;
    void readyStateChanged(MediaSessionReadyState) final;

private:
    explicit MediaSessionCoordinator(Ref<PlatformMediaSessionCoordinator>&&);

    const Logger& logger() const { return m_logger; }
    const void* logIdentifier() const { return m_logIdentifier; }
    static WTFLogChannel& logChannel();
    static const char* logClassName() { return "MediaSessionCoordinator"; }

    WeakPtr<MediaSession> m_session;
    Ref<PlatformMediaSessionCoordinator> m_platformCoordinator;
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;

    std::unique_ptr<DOMPromiseDeferred<void>> m_seekToPromise;
    std::unique_ptr<DOMPromiseDeferred<void>> m_playPromise;
    std::unique_ptr<DOMPromiseDeferred<void>> m_pausePromise;
    std::unique_ptr<DOMPromiseDeferred<void>> m_setTrackPromise;
};

}

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
