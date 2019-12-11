/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#import "PlaybackSessionInterfaceAVKit.h"

#if PLATFORM(IOS_FAMILY)
#if HAVE(AVKIT)

#import "Logging.h"
#import "MediaSelectionOption.h"
#import "PlaybackSessionModel.h"
#import "TimeRanges.h"
#import "WebAVPlayerController.h"
#import <AVFoundation/AVTime.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#import <pal/cf/CoreMediaSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVValueTiming)

namespace WebCore {
using namespace PAL;

PlaybackSessionInterfaceAVKit::PlaybackSessionInterfaceAVKit(PlaybackSessionModel& model)
    : m_playerController(adoptNS([[WebAVPlayerController alloc] init]))
    , m_playbackSessionModel(&model)
{
    ASSERT(isUIThread());
    model.addClient(*this);
    [m_playerController setPlaybackSessionInterface:this];
    [m_playerController setDelegate:&model];

    durationChanged(model.duration());
    currentTimeChanged(model.currentTime(), [[NSProcessInfo processInfo] systemUptime]);
    bufferedTimeChanged(model.bufferedTime());
    rateChanged(model.isPlaying(), model.playbackRate());
    seekableRangesChanged(model.seekableRanges(), model.seekableTimeRangesLastModifiedTime(), model.liveUpdateInterval());
    canPlayFastReverseChanged(model.canPlayFastReverse());
    audioMediaSelectionOptionsChanged(model.audioMediaSelectionOptions(), model.audioMediaSelectedIndex());
    legibleMediaSelectionOptionsChanged(model.legibleMediaSelectionOptions(), model.legibleMediaSelectedIndex());
    externalPlaybackChanged(model.externalPlaybackEnabled(), model.externalPlaybackTargetType(), model.externalPlaybackLocalizedDeviceName());
    wirelessVideoPlaybackDisabledChanged(model.wirelessVideoPlaybackDisabled());
}

PlaybackSessionInterfaceAVKit::~PlaybackSessionInterfaceAVKit()
{
    ASSERT(isUIThread());
    [m_playerController setPlaybackSessionInterface:nullptr];
    [m_playerController setExternalPlaybackActive:false];

    invalidate();
}

PlaybackSessionModel* PlaybackSessionInterfaceAVKit::playbackSessionModel() const
{
    return m_playbackSessionModel;
}

void PlaybackSessionInterfaceAVKit::durationChanged(double duration)
{
    WebAVPlayerController* playerController = m_playerController.get();

    playerController.contentDuration = duration;
    playerController.contentDurationWithinEndTimes = duration;

    // FIXME: we take this as an indication that playback is ready.
    playerController.canPlay = YES;
    playerController.canPause = YES;
    playerController.canTogglePlayback = YES;
    playerController.hasEnabledAudio = YES;
    playerController.canSeek = YES;
    playerController.status = AVPlayerControllerStatusReadyToPlay;
}

void PlaybackSessionInterfaceAVKit::currentTimeChanged(double currentTime, double anchorTime)
{
    if ([m_playerController isScrubbing])
        return;

    NSTimeInterval anchorTimeStamp = ![m_playerController rate] ? NAN : anchorTime;
    AVValueTiming *timing = [getAVValueTimingClass() valueTimingWithAnchorValue:currentTime
        anchorTimeStamp:anchorTimeStamp rate:0];

    [m_playerController setTiming:timing];
}

void PlaybackSessionInterfaceAVKit::bufferedTimeChanged(double bufferedTime)
{
    WebAVPlayerController* playerController = m_playerController.get();
    double duration = playerController.contentDuration;
    double normalizedBufferedTime;
    if (!duration)
        normalizedBufferedTime = 0;
    else
        normalizedBufferedTime = bufferedTime / duration;
    playerController.loadedTimeRanges = @[@0, @(normalizedBufferedTime)];
}

void PlaybackSessionInterfaceAVKit::rateChanged(bool isPlaying, float playbackRate)
{
    [m_playerController setRate:isPlaying ? playbackRate : 0.];
}

void PlaybackSessionInterfaceAVKit::seekableRangesChanged(const TimeRanges& timeRanges, double lastModifiedTime, double liveUpdateInterval)
{
    RetainPtr<NSMutableArray> seekableRanges = adoptNS([[NSMutableArray alloc] init]);

#if !PLATFORM(WATCHOS)
    for (unsigned i = 0; i < timeRanges.length(); i++) {
        double start = timeRanges.start(i).releaseReturnValue();
        double end = timeRanges.end(i).releaseReturnValue();

        CMTimeRange range = CMTimeRangeMake(CMTimeMakeWithSeconds(start, 1000), CMTimeMakeWithSeconds(end-start, 1000));
        [seekableRanges addObject:[NSValue valueWithCMTimeRange:range]];
    }
#else
    UNUSED_PARAM(timeRanges);
#endif

    [m_playerController setSeekableTimeRanges:seekableRanges.get()];
    [m_playerController setSeekableTimeRangesLastModifiedTime: lastModifiedTime];
    [m_playerController setLiveUpdateInterval:liveUpdateInterval];
}

void PlaybackSessionInterfaceAVKit::canPlayFastReverseChanged(bool canPlayFastReverse)
{
    [m_playerController setCanScanBackward:canPlayFastReverse];
}

static RetainPtr<NSMutableArray> mediaSelectionOptions(const Vector<MediaSelectionOption>& options)
{
    RetainPtr<NSMutableArray> webOptions = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& option : options) {
        RetainPtr<WebAVMediaSelectionOption> webOption = adoptNS([[WebAVMediaSelectionOption alloc] init]);
        [webOption setLocalizedDisplayName:option.displayName];
        [webOptions addObject:webOption.get()];
    }
    return webOptions;
}

void PlaybackSessionInterfaceAVKit::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    [m_playerController setAudioMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [m_playerController setCurrentAudioMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
}

void PlaybackSessionInterfaceAVKit::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    [m_playerController setLegibleMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [m_playerController setCurrentLegibleMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
}

void PlaybackSessionInterfaceAVKit::externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType targetType, const String& localizedDeviceName)
{
    AVPlayerControllerExternalPlaybackType externalPlaybackType = AVPlayerControllerExternalPlaybackTypeNone;
    if (targetType == PlaybackSessionModel::TargetTypeAirPlay)
        externalPlaybackType = AVPlayerControllerExternalPlaybackTypeAirPlay;
    else if (targetType == PlaybackSessionModel::TargetTypeTVOut)
        externalPlaybackType = AVPlayerControllerExternalPlaybackTypeTVOut;

    WebAVPlayerController* playerController = m_playerController.get();
    playerController.externalPlaybackAirPlayDeviceLocalizedName = localizedDeviceName;
    playerController.externalPlaybackType = externalPlaybackType;
    playerController.externalPlaybackActive = enabled;
}

void PlaybackSessionInterfaceAVKit::wirelessVideoPlaybackDisabledChanged(bool disabled)
{
    [m_playerController setAllowsExternalPlayback:!disabled];
}

void PlaybackSessionInterfaceAVKit::mutedChanged(bool muted)
{
    [m_playerController setMuted:muted];
}

void PlaybackSessionInterfaceAVKit::volumeChanged(double volume)
{
    [m_playerController volumeChanged:volume];
}

void PlaybackSessionInterfaceAVKit::invalidate()
{
    if (!m_playbackSessionModel)
        return;

    [m_playerController setDelegate:nullptr];
    m_playbackSessionModel->removeClient(*this);
    m_playbackSessionModel = nullptr;
}

void PlaybackSessionInterfaceAVKit::modelDestroyed()
{
    ASSERT(isUIThread());
    invalidate();
    ASSERT(!m_playbackSessionModel);
}

}

#endif // HAVE(AVKIT)
#endif // PLATFORM(IOS_FAMILY)
