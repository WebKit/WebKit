/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)

#include "DestinationColorSpace.h"
#include "MediaPlayerPrivate.h"
#include <wtf/CanMakeWeakPtr.h>
#include <wtf/LoggerHelper.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/URL.h>

namespace WebCore {

class MediaPlaybackTarget;

class MediaPlayerPrivateWirelessPlayback final
    : public MediaPlayerPrivateInterface
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
    , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<MediaPlayerPrivateWirelessPlayback, WTF::DestructionThread::Main> {
    WTF_MAKE_TZONE_ALLOCATED(MediaPlayerPrivateWirelessPlayback);
public:
    ~MediaPlayerPrivateWirelessPlayback();

    static void registerMediaEngine(MediaEngineRegistrar);

    // AbstractRefCounted
    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

private:
    friend class MediaPlayerFactoryWirelessPlayback;

    explicit MediaPlayerPrivateWirelessPlayback(MediaPlayer&);

    void updateURLIfNeeded();

    void setNetworkState(MediaPlayer::NetworkState);
    void setReadyState(MediaPlayer::ReadyState);

    // MediaPlayerPrivateInterface
    constexpr MediaPlayerType mediaPlayerType() const final { return MediaPlayerType::WirelessPlayback; }
    void load(const String&) final;
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const LoadOptions&, MediaSourcePrivateClient&) final { }
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) final { }
#endif
    void cancelLoad() final { }
    void play() final { }
    void pause() final { }
    FloatSize naturalSize() const final { return { }; }
    bool hasVideo() const final { return false; }
    bool hasAudio() const final { return false; }
    void setPageIsVisible(bool) final { }
    void seekToTarget(const SeekTarget&) final { }
    bool seeking() const final { return false; }
    bool paused() const final { return true; }
    MediaPlayer::NetworkState networkState() const final { return m_networkState; }
    MediaPlayer::ReadyState readyState() const final { return m_readyState; }
    const PlatformTimeRanges& buffered() const final { return m_buffered; }
    bool didLoadingProgress() const final { return false; }
    void paint(GraphicsContext&, const FloatRect&) final { }
    DestinationColorSpace colorSpace() final { return DestinationColorSpace::SRGB(); }

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    static OptionSet<MediaPlaybackTargetType> playbackTargetTypes();
    String wirelessPlaybackTargetName() const final;
    MediaPlayer::WirelessPlaybackTargetType wirelessPlaybackTargetType() const final;
    bool wirelessVideoPlaybackDisabled() const final { return !m_allowsWirelessVideoPlayback; }
    void setWirelessVideoPlaybackDisabled(bool disabled) final { m_allowsWirelessVideoPlayback = !disabled; }
    OptionSet<MediaPlaybackTargetType> supportedPlaybackTargetTypes() const final;
    bool isCurrentPlaybackTargetWireless() const final;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) final;
    void setShouldPlayToPlaybackTarget(bool) final;
#endif

#if !RELEASE_LOG_DISABLED
    // LoggerHelper
    const Logger& logger() const final { return m_logger.get(); }
    ASCIILiteral logClassName() const final { return "MediaPlayerPrivateWirelessPlayback"_s; }
    WTFLogChannel& logChannel() const final;
    uint64_t logIdentifier() const final { return m_logIdentifier; }
#endif

    ThreadSafeWeakPtr<MediaPlayer> m_player;
    PlatformTimeRanges m_buffered;
    URL m_url;
    MediaPlayer::NetworkState m_networkState { MediaPlayer::NetworkState::Empty };
    MediaPlayer::ReadyState m_readyState { MediaPlayer::ReadyState::HaveNothing };
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    bool m_allowsWirelessVideoPlayback { true };
    bool m_shouldPlayToTarget { false };
    RefPtr<MediaPlaybackTarget> m_playbackTarget;
#endif
#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
