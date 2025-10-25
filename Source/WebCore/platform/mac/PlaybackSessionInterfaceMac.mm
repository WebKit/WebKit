/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PlaybackSessionInterfaceMac.h"

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#import "IntRect.h"
#import "Logging.h"
#import "MediaSelectionOption.h"
#import "PlaybackSessionModel.h"
#import "TimeRanges.h"
#import "WebPlaybackControlsManager.h"
#import <AVFoundation/AVTime.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <wtf/TZoneMallocInlines.h>

#import <pal/cf/CoreMediaSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVValueTiming)

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaybackSessionInterfaceMac);

Ref<PlaybackSessionInterfaceMac> PlaybackSessionInterfaceMac::create(PlaybackSessionModel& model)
{
    auto interface = adoptRef(*new PlaybackSessionInterfaceMac(model));
    model.addClient(interface);
    return interface;
}

PlaybackSessionInterfaceMac::PlaybackSessionInterfaceMac(PlaybackSessionModel& model)
    : m_playbackSessionModel(model)
{
}

PlaybackSessionInterfaceMac::~PlaybackSessionInterfaceMac()
{
    invalidate();
}

PlaybackSessionModel* PlaybackSessionInterfaceMac::playbackSessionModel() const
{
    return m_playbackSessionModel.get();
}

bool PlaybackSessionInterfaceMac::isInWindowFullscreenActive() const
{
    CheckedPtr model = m_playbackSessionModel.get();
    return model && model->isInWindowFullscreenActive();
}

void PlaybackSessionInterfaceMac::enterInWindowFullscreen()
{
    if (CheckedPtr model = m_playbackSessionModel.get())
        model->enterInWindowFullscreen();
}

void PlaybackSessionInterfaceMac::exitInWindowFullscreen()
{
    if (CheckedPtr model = m_playbackSessionModel.get())
        model->exitInWindowFullscreen();
}

void PlaybackSessionInterfaceMac::durationChanged(double duration)
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    RetainPtr controlsManager = playBackControlsManager();

    controlsManager.get().contentDuration = duration;

    // FIXME: We take this as an indication that playback is ready, but that is not necessarily true.
    controlsManager.get().hasEnabledAudio = YES;
    controlsManager.get().hasEnabledVideo = YES;
#else
    UNUSED_PARAM(duration);
#endif
}

void PlaybackSessionInterfaceMac::currentTimeChanged(double currentTime, double anchorTime)
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    RetainPtr controlsManager = playBackControlsManager();
    updatePlaybackControlsManagerTiming(currentTime, anchorTime, controlsManager.get().rate, controlsManager.get().playing);
#else
    UNUSED_PARAM(currentTime);
    UNUSED_PARAM(anchorTime);
#endif
}

void PlaybackSessionInterfaceMac::rateChanged(OptionSet<PlaybackSessionModel::PlaybackState> playbackState, double playbackRate, double defaultPlaybackRate)
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, "playbackState: ", playbackState, ", playbackRate: ", playbackRate);

    auto isPlaying = playbackState.contains(PlaybackSessionModel::PlaybackState::Playing);
    RetainPtr controlsManager = playBackControlsManager();
    [controlsManager setDefaultPlaybackRate:defaultPlaybackRate fromJavaScript:YES];
    [controlsManager setRate:isPlaying ? playbackRate : 0. fromJavaScript:YES];
    [controlsManager setPlaying:isPlaying];
    CheckedPtr model = m_playbackSessionModel.get();
    updatePlaybackControlsManagerTiming(model ? model->currentTime() : 0, [[NSProcessInfo processInfo] systemUptime], playbackRate, isPlaying);
#else
    UNUSED_PARAM(isPlaying);
    UNUSED_PARAM(playbackRate);
#endif
}

void PlaybackSessionInterfaceMac::willBeginScrubbing()
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    CheckedPtr model = m_playbackSessionModel.get();
    updatePlaybackControlsManagerTiming(model ? model->currentTime() : 0, [[NSProcessInfo processInfo] systemUptime], 0, false);
#endif
}

void PlaybackSessionInterfaceMac::beginScrubbing()
{
    willBeginScrubbing();
    if (CheckedPtr model = playbackSessionModel())
        model->beginScrubbing();
}

void PlaybackSessionInterfaceMac::endScrubbing()
{
    if (CheckedPtr model = playbackSessionModel())
        model->endScrubbing();
}
#if HAVE(PIP_SKIP_PREROLL)
void PlaybackSessionInterfaceMac::skipAd()
{
    if (CheckedPtr model = playbackSessionModel())
        model->skipAd();
}
#endif

void PlaybackSessionInterfaceMac::seekableRangesChanged(const PlatformTimeRanges& timeRanges, double, double)
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    [protectedPlayBackControlsManager() setSeekableTimeRanges:makeNSArray(timeRanges).get()];
#else
    UNUSED_PARAM(timeRanges);
#endif
}

void PlaybackSessionInterfaceMac::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    [protectedPlayBackControlsManager() setAudioMediaSelectionOptions:options withSelectedIndex:static_cast<NSUInteger>(selectedIndex)];
#else
    UNUSED_PARAM(options);
    UNUSED_PARAM(selectedIndex);
#endif
}

void PlaybackSessionInterfaceMac::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    [protectedPlayBackControlsManager() setLegibleMediaSelectionOptions:options withSelectedIndex:static_cast<NSUInteger>(selectedIndex)];
#else
    UNUSED_PARAM(options);
    UNUSED_PARAM(selectedIndex);
#endif
}

void PlaybackSessionInterfaceMac::audioMediaSelectionIndexChanged(uint64_t selectedIndex)
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    [protectedPlayBackControlsManager() setAudioMediaSelectionIndex:selectedIndex];
#else
    UNUSED_PARAM(selectedIndex);
#endif
}

void PlaybackSessionInterfaceMac::legibleMediaSelectionIndexChanged(uint64_t selectedIndex)
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    [protectedPlayBackControlsManager() setLegibleMediaSelectionIndex:selectedIndex];
#else
    UNUSED_PARAM(selectedIndex);
#endif
}

void PlaybackSessionInterfaceMac::isPictureInPictureSupportedChanged(bool)
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    updatePlaybackControlsManagerCanTogglePictureInPicture();
#endif
}

void PlaybackSessionInterfaceMac::externalPlaybackChanged(bool, PlaybackSessionModel::ExternalPlaybackTargetType, const String&)
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    updatePlaybackControlsManagerCanTogglePictureInPicture();
#endif
}

void PlaybackSessionInterfaceMac::invalidate()
{
    if (CheckedPtr model = m_playbackSessionModel.get())
        model->removeClient(*this);
    m_playbackSessionModel = nullptr;
}

void PlaybackSessionInterfaceMac::ensureControlsManager()
{
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    playBackControlsManager();
#endif
}

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

WebPlaybackControlsManager *PlaybackSessionInterfaceMac::playBackControlsManager()
{
    return m_playbackControlsManager.getAutoreleased();
}

void PlaybackSessionInterfaceMac::setPlayBackControlsManager(WebPlaybackControlsManager *manager)
{
    m_playbackControlsManager = manager;

    CheckedPtr model = m_playbackSessionModel.get();
    if (!manager || !model)
        return;

    NSTimeInterval anchorTimeStamp = ![manager rate] ? NAN : [[NSProcessInfo processInfo] systemUptime];
    manager.timing = [getAVValueTimingClassSingleton() valueTimingWithAnchorValue:model->currentTime() anchorTimeStamp:anchorTimeStamp rate:0];
    double duration = model->duration();
    manager.contentDuration = duration;
    manager.hasEnabledAudio = duration > 0;
    manager.hasEnabledVideo = duration > 0;
    manager.defaultPlaybackRate = model->defaultPlaybackRate();
    manager.rate = model->isPlaying() ? model->playbackRate() : 0.;
    manager.seekableTimeRanges = makeNSArray(model->seekableRanges()).get();
    manager.canTogglePlayback = YES;
    manager.playing = model->isPlaying();
    [manager setAudioMediaSelectionOptions:model->audioMediaSelectionOptions() withSelectedIndex:static_cast<NSUInteger>(model->audioMediaSelectedIndex())];
    [manager setLegibleMediaSelectionOptions:model->legibleMediaSelectionOptions() withSelectedIndex:static_cast<NSUInteger>(model->legibleMediaSelectedIndex())];

    updatePlaybackControlsManagerCanTogglePictureInPicture();
}

void PlaybackSessionInterfaceMac::updatePlaybackControlsManagerCanTogglePictureInPicture()
{
    CheckedPtr model = playbackSessionModel();
    if (!model) {
        [protectedPlayBackControlsManager() setCanTogglePictureInPicture:NO];
        return;
    }

    [protectedPlayBackControlsManager() setCanTogglePictureInPicture:model->isPictureInPictureSupported() && !model->externalPlaybackEnabled()];
}

void PlaybackSessionInterfaceMac::updatePlaybackControlsManagerTiming(double currentTime, double anchorTime, double playbackRate, bool isPlaying)
{
    RetainPtr manager = playBackControlsManager();
    if (!manager)
        return;

    CheckedPtr model = playbackSessionModel();
    if (!model)
        return;

    double effectiveAnchorTime = playbackRate ? anchorTime : NAN;
    double effectivePlaybackRate = playbackRate;
    if (!isPlaying
        || model->isScrubbing()
        || (manager.get().rate > 0 && model->playbackStartedTime() >= currentTime)
        || (manager.get().rate < 0 && model->playbackStartedTime() <= currentTime))
        effectivePlaybackRate = 0;

    manager.get().timing = [getAVValueTimingClassSingleton() valueTimingWithAnchorValue:currentTime anchorTimeStamp:effectiveAnchorTime rate:effectivePlaybackRate];
}

RetainPtr<WebPlaybackControlsManager> PlaybackSessionInterfaceMac::protectedPlayBackControlsManager()
{
    return playBackControlsManager();
}

#endif // ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

#if !RELEASE_LOG_DISABLED
uint64_t PlaybackSessionInterfaceMac::logIdentifier() const
{
    CheckedPtr model = m_playbackSessionModel.get();
    return model ? model->logIdentifier() : 0;
}

const Logger* PlaybackSessionInterfaceMac::loggerPtr() const
{
    CheckedPtr model = m_playbackSessionModel.get();
    return model ? model->loggerPtr() : nullptr;
}

WTFLogChannel& PlaybackSessionInterfaceMac::logChannel() const
{
    return LogMedia;
}

#endif

}

#endif // PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
