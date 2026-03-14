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
#include "MediaDeviceRoute.h"
#include "MediaPlayerPrivate.h"
#include <wtf/CanMakeWeakPtr.h>
#include <wtf/LoggerHelper.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/URL.h>

namespace WebCore {

class MediaDeviceRoute;
class MediaPlaybackTarget;
class MediaPlaybackTargetWirelessPlayback;

class MediaPlayerPrivateWirelessPlayback final
    : public MediaPlayerPrivateInterface
    , private MediaDeviceRouteClient
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

    MediaPlaybackTargetWirelessPlayback* wirelessPlaybackTarget() const;
    MediaDeviceRoute* route() const;

    void updateURLIfNeeded();

    void setNetworkState(MediaPlayer::NetworkState);
    void setReadyState(MediaPlayer::ReadyState);

    // MediaPlayerPrivateInterface
    constexpr MediaPlayerType mediaPlayerType() const final { return MediaPlayerType::WirelessPlayback; }
    void load(const URL&, const LoadOptions&) final;
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const LoadOptions&, MediaSourcePrivateClient&) final { }
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) final { }
#endif
    void cancelLoad() final { }
    void play() final;
    void pause() final;
    FloatSize naturalSize() const final { return { }; }
    bool hasVideo() const final { return true; }
    bool hasAudio() const final;
    void setPageIsVisible(bool) final { }
    void seekToTarget(const SeekTarget&) final;
    bool seeking() const final { return false; }
    bool paused() const final;
    MediaPlayer::NetworkState networkState() const final { return m_networkState; }
    MediaPlayer::ReadyState readyState() const final { return m_readyState; }
    const PlatformTimeRanges& buffered() const LIFETIME_BOUND final { return m_buffered; }
    bool didLoadingProgress() const final { return m_didLoadingProgress; }
    void paint(GraphicsContext&, const FloatRect&) final { }
    DestinationColorSpace colorSpace() final { return DestinationColorSpace::SRGB(); }
    static OptionSet<MediaPlaybackTargetType> playbackTargetTypes();
    String wirelessPlaybackTargetName() const final;
    MediaPlayer::WirelessPlaybackTargetType wirelessPlaybackTargetType() const final;
    bool wirelessVideoPlaybackDisabled() const final { return !m_allowsWirelessVideoPlayback; }
    void setWirelessVideoPlaybackDisabled(bool disabled) final { m_allowsWirelessVideoPlayback = !disabled; }
    OptionSet<MediaPlaybackTargetType> supportedPlaybackTargetTypes() const final;
    bool isCurrentPlaybackTargetWireless() const final;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) final;
    void setShouldPlayToPlaybackTarget(bool) final;
    MediaTime startTime() const final;
    MediaTime duration() const final;
    MediaTime currentTime() const final;
    MediaTime maxTimeSeekable() const final;
    MediaTime minTimeSeekable() const final;
    bool setCurrentTimeDidChangeCallback(MediaPlayer::CurrentTimeDidChangeCallback&&) final;
    void setRate(float) final;
    double rate() const final;
    void setVolume(float) final { }
    float volume() const final { return 0; }
    void setMuted(bool) final { }
    String engineDescription() const final;

    // MediaDeviceRouteClient
    void timeRangeDidChange(MediaDeviceRoute&) final;
    void readyDidChange(MediaDeviceRoute&) final;
    void playbackErrorDidChange(MediaDeviceRoute&) final;
    void currentPlaybackPositionDidChange(MediaDeviceRoute&) final;

#if !RELEASE_LOG_DISABLED
    // LoggerHelper
    const Logger& logger() const final { return m_logger.get(); }
    ASCIILiteral logClassName() const final { return "MediaPlayerPrivateWirelessPlayback"_s; }
    WTFLogChannel& logChannel() const final;
    uint64_t logIdentifier() const final { return m_logIdentifier; }
#endif

    enum class ShouldPlayToTarget : uint8_t { Unknown, No, Yes };

    ThreadSafeWeakPtr<MediaPlayer> m_player;
    PlatformTimeRanges m_buffered;
    URL m_url;
    MediaPlayer::NetworkState m_networkState { MediaPlayer::NetworkState::Empty };
    MediaPlayer::ReadyState m_readyState { MediaPlayer::ReadyState::HaveNothing };
    bool m_didLoadingProgress { false };
    bool m_allowsWirelessVideoPlayback { true };
    ShouldPlayToTarget m_shouldPlayToTarget { ShouldPlayToTarget::Unknown };
    RefPtr<MediaPlaybackTarget> m_playbackTarget;
    MediaPlayer::CurrentTimeDidChangeCallback m_currentTimeDidChangeCallback;
    std::optional<SeekTarget> m_pendingSeekTarget;
#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
