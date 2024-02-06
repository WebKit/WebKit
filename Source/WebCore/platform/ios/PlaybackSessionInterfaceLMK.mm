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

#import "config.h"
#import "PlaybackSessionInterfaceLMK.h"

#if PLATFORM(VISION)

#import "MediaSelectionOption.h"
#import "TimeRanges.h"

namespace WebCore {

Ref<PlaybackSessionInterfaceLMK> PlaybackSessionInterfaceLMK::create(PlaybackSessionModel& model)
{
    return adoptRef(*new PlaybackSessionInterfaceLMK(model));
}

PlaybackSessionInterfaceLMK::PlaybackSessionInterfaceLMK(PlaybackSessionModel& model)
    : PlaybackSessionInterfaceIOS(model)
{
    durationChanged(model.duration());
    currentTimeChanged(model.currentTime(), [[NSProcessInfo processInfo] systemUptime]);
    bufferedTimeChanged(model.bufferedTime());
    OptionSet<PlaybackSessionModel::PlaybackState> playbackState;
    if (model.isPlaying())
        playbackState.add(PlaybackSessionModel::PlaybackState::Playing);
    if (model.isStalled())
        playbackState.add(PlaybackSessionModel::PlaybackState::Stalled);
    rateChanged(playbackState, model.playbackRate(), model.defaultPlaybackRate());
    seekableRangesChanged(model.seekableRanges(), model.seekableTimeRangesLastModifiedTime(), model.liveUpdateInterval());
    canPlayFastReverseChanged(model.canPlayFastReverse());
    audioMediaSelectionOptionsChanged(model.audioMediaSelectionOptions(), model.audioMediaSelectedIndex());
    legibleMediaSelectionOptionsChanged(model.legibleMediaSelectionOptions(), model.legibleMediaSelectedIndex());
    externalPlaybackChanged(model.externalPlaybackEnabled(), model.externalPlaybackTargetType(), model.externalPlaybackLocalizedDeviceName());
    wirelessVideoPlaybackDisabledChanged(model.wirelessVideoPlaybackDisabled());
}

WebAVPlayerController *PlaybackSessionInterfaceLMK::playerController() const
{
    return nullptr;
}

PlaybackSessionInterfaceLMK::~PlaybackSessionInterfaceLMK()
{

}

void PlaybackSessionInterfaceLMK::durationChanged(double)
{

}

void PlaybackSessionInterfaceLMK::currentTimeChanged(double, double)
{

}

void PlaybackSessionInterfaceLMK::bufferedTimeChanged(double)
{

}

void PlaybackSessionInterfaceLMK::rateChanged(OptionSet<PlaybackSessionModel::PlaybackState>, double, double)
{

}

void PlaybackSessionInterfaceLMK::seekableRangesChanged(const TimeRanges&, double, double)
{

}

void PlaybackSessionInterfaceLMK::canPlayFastReverseChanged(bool)
{

}

void PlaybackSessionInterfaceLMK::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>&, uint64_t)
{

}

void PlaybackSessionInterfaceLMK::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>&, uint64_t)
{

}

void PlaybackSessionInterfaceLMK::externalPlaybackChanged(bool, PlaybackSessionModel::ExternalPlaybackTargetType, const String&)
{

}

void PlaybackSessionInterfaceLMK::wirelessVideoPlaybackDisabledChanged(bool)
{

}

void PlaybackSessionInterfaceLMK::mutedChanged(bool)
{

}

void PlaybackSessionInterfaceLMK::volumeChanged(double)
{

}

PlaybackSessionInterfaceLMK::~PlaybackSessionInterfaceLMK()
{
    ASSERT(isUIThread());
    invalidate();
}

void PlaybackSessionInterfaceLMK::invalidate()
{
    if (!m_playbackSessionModel)
        return;

    m_playbackSessionModel->removeClient(*this);
    m_playbackSessionModel = nullptr;
}

#if !RELEASE_LOG_DISABLED
const char* PlaybackSessionInterfaceLMK::logClassName() const
{
    return "PlaybackSessionInterfaceLMK";
}
#endif

} // namespace WebCore

#endif // PLATFORM(VISION)
