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

#include "Exception.h"
#include "Logging.h"
#include <wtf/Logger.h>
#include <wtf/Optional.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class PlatformMediaSessionCoordinator : public RefCounted<PlatformMediaSessionCoordinator>, public CanMakeWeakPtr<PlatformMediaSessionCoordinator> {
public:
    virtual ~PlatformMediaSessionCoordinator() = default;

    using SeekCompletedCallback = CompletionHandler<void(Optional<Exception>&&)>;
    virtual void seekTo(double, SeekCompletedCallback&&) = 0;

    using PlayCompletedCallback = CompletionHandler<void(Optional<Exception>&&)>;
    virtual void play(PlayCompletedCallback&&) = 0;

    using PauseCompletedCallback = CompletionHandler<void(Optional<Exception>&&)>;
    virtual void pause(PauseCompletedCallback&&) = 0;

    using SetTrackCompletedCallback = CompletionHandler<void(Optional<Exception>&&)>;
    virtual void setTrack(const String, SetTrackCompletedCallback&&) = 0;

    void setLogger(const Logger& logger, const void* logIdentifier)
    {
        m_logger = makeRefPtr(logger);
        m_logIdentifier = logIdentifier;
    }

    struct MediaPositionState {
        double duration = std::numeric_limits<double>::infinity();
        double playbackRate = 1;
        double position = 0;
    };

    enum class MediaSessionReadyState : uint8_t {
        HaveNothing,
        HaveMetadata,
        HaveCurrentData,
        HaveFutureData,
        HaveEnoughData,
    };

    enum class MediaSessionPlaybackState : uint8_t {
        None,
        Paused,
        Playing,
    };

    virtual void positionStateChanged(Optional<MediaPositionState>) = 0;
    virtual void readyStateChanged(MediaSessionReadyState) = 0;
    virtual void playbackStateChanged(MediaSessionPlaybackState) = 0;

protected:
    explicit PlatformMediaSessionCoordinator() = default;

    const Logger* loggerPtr() const { return m_logger.get(); }
    const void* logIdentifier() const { return m_logIdentifier; }
    const char* logClassName() const { return "PlatformMediaSessionCoordinator"; }
    WTFLogChannel& logChannel() const { return LogMedia; }

private:
    RefPtr<const Logger> m_logger;
    const void* m_logIdentifier;
};

String convertEnumerationToString(PlatformMediaSessionCoordinator::MediaSessionReadyState);
String convertEnumerationToString(PlatformMediaSessionCoordinator::MediaSessionPlaybackState);

}

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
