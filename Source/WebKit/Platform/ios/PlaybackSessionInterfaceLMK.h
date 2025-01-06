/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(LINEAR_MEDIA_PLAYER)

#include <WebCore/NowPlayingMetadataObserver.h>
#include <WebCore/PlaybackSessionInterfaceIOS.h>
#include <wtf/Observer.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS WKLinearMediaPlayerDelegate;

namespace WebKit {

class PlaybackSessionInterfaceLMK final : public WebCore::PlaybackSessionInterfaceIOS {
    WTF_MAKE_TZONE_ALLOCATED(PlaybackSessionInterfaceLMK);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PlaybackSessionInterfaceLMK);
public:
    static Ref<PlaybackSessionInterfaceLMK> create(WebCore::PlaybackSessionModel&);
    ~PlaybackSessionInterfaceLMK();

    WebAVPlayerController *playerController() const final { return nullptr; }
    WKSLinearMediaPlayer *linearMediaPlayer() const final;
    void durationChanged(double) final;
    void currentTimeChanged(double, double) final;
    void bufferedTimeChanged(double) final { }
    void rateChanged(OptionSet<WebCore::PlaybackSessionModel::PlaybackState>, double, double) final;
    void seekableRangesChanged(const WebCore::TimeRanges&, double, double) final;
    void canPlayFastReverseChanged(bool) final;
    void audioMediaSelectionOptionsChanged(const Vector<WebCore::MediaSelectionOption>&, uint64_t) final;
    void legibleMediaSelectionOptionsChanged(const Vector<WebCore::MediaSelectionOption>&, uint64_t) final;
    void audioMediaSelectionIndexChanged(uint64_t) final;
    void legibleMediaSelectionIndexChanged(uint64_t) final;
    void externalPlaybackChanged(bool, WebCore::PlaybackSessionModel::ExternalPlaybackTargetType, const String&) final { }
    void wirelessVideoPlaybackDisabledChanged(bool) final { }
    void mutedChanged(bool) final;
    void volumeChanged(double) final;
    void supportsLinearMediaPlayerChanged(bool) final;
    void spatialVideoMetadataChanged(const std::optional<WebCore::SpatialVideoMetadata>&) final;
    void startObservingNowPlayingMetadata() final;
    void stopObservingNowPlayingMetadata() final;
#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const final;
#endif

    void nowPlayingMetadataChanged(const WebCore::NowPlayingMetadata&);

    void setSpatialVideoEnabled(bool enabled) { m_spatialVideoEnabled = enabled; }
    bool spatialVideoEnabled() const { return m_spatialVideoEnabled; }

private:
    PlaybackSessionInterfaceLMK(WebCore::PlaybackSessionModel&);

    RetainPtr<WKSLinearMediaPlayer> m_player;
    RetainPtr<WKLinearMediaPlayerDelegate> m_playerDelegate;
    WebCore::NowPlayingMetadataObserver m_nowPlayingMetadataObserver;
    bool m_spatialVideoEnabled { false };
};

} // namespace WebKit

#endif // ENABLE(LINEAR_MEDIA_PLAYER)
