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

#if HAVE(AVEXPERIENCECONTROLLER)

#include <WebCore/PlaybackSessionInterfaceIOS.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class PlaybackSessionInterfaceAVKit final : public WebCore::PlaybackSessionInterfaceIOS {
    WTF_MAKE_TZONE_ALLOCATED(PlaybackSessionInterfaceAVKit);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PlaybackSessionInterfaceAVKit);
public:
    static Ref<PlaybackSessionInterfaceAVKit> create(WebCore::PlaybackSessionModel&);
    ~PlaybackSessionInterfaceAVKit();

    void nowPlayingMetadataChanged(const WebCore::NowPlayingMetadata&);

    // PlaybackSessionInterfaceIOS overrides
    WebAVPlayerController *playerController() const final { return nullptr; }
    WKSLinearMediaPlayer *linearMediaPlayer() const final { return nullptr; }
    WKAVContentSource *contentSource() const { return m_contentSource.get(); }
    void durationChanged(double) final;
    void currentTimeChanged(double, double) final;
    void bufferedTimeChanged(double) final { }
    void rateChanged(OptionSet<WebCore::PlaybackSessionModel::PlaybackState>, double, double) final;
    void seekableRangesChanged(const WebCore::PlatformTimeRanges&, double, double) final;
    void canPlayFastReverseChanged(bool) final { }
    void audioMediaSelectionOptionsChanged(const Vector<WebCore::MediaSelectionOption>&, uint64_t) final;
    void legibleMediaSelectionOptionsChanged(const Vector<WebCore::MediaSelectionOption>&, uint64_t) final;
    void audioMediaSelectionIndexChanged(uint64_t) final;
    void legibleMediaSelectionIndexChanged(uint64_t) final;
    void externalPlaybackChanged(bool, WebCore::PlaybackSessionModel::ExternalPlaybackTargetType, const String&) final { }
    void wirelessVideoPlaybackDisabledChanged(bool) final { }
    void mutedChanged(bool) final;
    void volumeChanged(double) final;
    void startObservingNowPlayingMetadata() final;
    void stopObservingNowPlayingMetadata() final;
#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const final;
#endif

private:
    explicit PlaybackSessionInterfaceAVKit(WebCore::PlaybackSessionModel&);

    RetainPtr<WKAVContentSource> m_contentSource;
    const Ref<WebCore::NowPlayingMetadataObserver> m_nowPlayingMetadataObserver;
};

} // namespace WebKit

#endif // HAVE(AVEXPERIENCECONTROLLER)
