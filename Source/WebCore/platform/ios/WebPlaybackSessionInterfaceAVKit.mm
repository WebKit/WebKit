/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
#import "WebPlaybackSessionInterfaceAVKit.h"

#if PLATFORM(IOS)
#if HAVE(AVKIT)

#import "AVKitSPI.h"
#import "Logging.h"
#import "TimeRanges.h"
#import "WebAVPlayerController.h"
#import "WebPlaybackSessionModel.h"
#import <AVFoundation/AVTime.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK_OPTIONAL(AVKit)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVValueTiming)

namespace WebCore {

WebPlaybackSessionInterfaceAVKit::WebPlaybackSessionInterfaceAVKit(WebPlaybackSessionModel& model)
    : m_playerController(adoptNS([[WebAVPlayerController alloc] init]))
    , m_playbackSessionModel(&model)
{
    model.addClient(*this);
    [m_playerController setPlaybackSessionInterface:this];
    [m_playerController setDelegate:&model];

    durationChanged(model.duration());
    currentTimeChanged(model.currentTime(), [[NSProcessInfo processInfo] systemUptime]);
    bufferedTimeChanged(model.bufferedTime());
    rateChanged(model.isPlaying(), model.playbackRate());
    seekableRangesChanged(model.seekableRanges());
    canPlayFastReverseChanged(model.canPlayFastReverse());
    audioMediaSelectionOptionsChanged(model.audioMediaSelectionOptions(), model.audioMediaSelectedIndex());
    legibleMediaSelectionOptionsChanged(model.legibleMediaSelectionOptions(), model.legibleMediaSelectedIndex());
    externalPlaybackChanged(model.externalPlaybackEnabled(), model.externalPlaybackTargetType(), model.externalPlaybackLocalizedDeviceName());
    wirelessVideoPlaybackDisabledChanged(model.wirelessVideoPlaybackDisabled());
}

WebPlaybackSessionInterfaceAVKit::~WebPlaybackSessionInterfaceAVKit()
{
    [m_playerController setPlaybackSessionInterface:nullptr];
    [m_playerController setExternalPlaybackActive:false];

    invalidate();
}

void WebPlaybackSessionInterfaceAVKit::resetMediaState()
{
    WebAVPlayerController* playerController = m_playerController.get();

    playerController.contentDuration = 0;
    playerController.maxTime = 0;
    playerController.contentDurationWithinEndTimes = 0;
    playerController.loadedTimeRanges = @[];

    playerController.canPlay = NO;
    playerController.canPause = NO;
    playerController.canTogglePlayback = NO;
    playerController.hasEnabledAudio = NO;
    playerController.canSeek = NO;
    playerController.minTime = 0;
    playerController.status = AVPlayerControllerStatusUnknown;

    playerController.timing = nil;
    playerController.rate = 0;

    playerController.seekableTimeRanges = [NSMutableArray array];

    playerController.canScanBackward = NO;

    playerController.audioMediaSelectionOptions = nil;
    playerController.currentAudioMediaSelectionOption = nil;

    playerController.legibleMediaSelectionOptions = nil;
    playerController.currentLegibleMediaSelectionOption = nil;
}

void WebPlaybackSessionInterfaceAVKit::durationChanged(double duration)
{
    WebAVPlayerController* playerController = m_playerController.get();

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=127017 use correct values instead of duration for all these
    playerController.contentDuration = duration;
    playerController.maxTime = duration;
    playerController.contentDurationWithinEndTimes = duration;

    // FIXME: we take this as an indication that playback is ready.
    playerController.canPlay = YES;
    playerController.canPause = YES;
    playerController.canTogglePlayback = YES;
    playerController.hasEnabledAudio = YES;
    playerController.canSeek = YES;
    playerController.minTime = 0;
    playerController.status = AVPlayerControllerStatusReadyToPlay;
}

void WebPlaybackSessionInterfaceAVKit::currentTimeChanged(double currentTime, double anchorTime)
{
    NSTimeInterval anchorTimeStamp = ![m_playerController rate] ? NAN : anchorTime;
    AVValueTiming *timing = [getAVValueTimingClass() valueTimingWithAnchorValue:currentTime
        anchorTimeStamp:anchorTimeStamp rate:0];

    [m_playerController setTiming:timing];
}

void WebPlaybackSessionInterfaceAVKit::bufferedTimeChanged(double bufferedTime)
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

void WebPlaybackSessionInterfaceAVKit::rateChanged(bool isPlaying, float playbackRate)
{
    [m_playerController setRate:isPlaying ? playbackRate : 0.];
}

void WebPlaybackSessionInterfaceAVKit::seekableRangesChanged(const TimeRanges& timeRanges)
{
    RetainPtr<NSMutableArray> seekableRanges = adoptNS([[NSMutableArray alloc] init]);

    for (unsigned i = 0; i < timeRanges.length(); i++) {
        double start = timeRanges.start(i).releaseReturnValue();
        double end = timeRanges.end(i).releaseReturnValue();

        CMTimeRange range = CMTimeRangeMake(CMTimeMakeWithSeconds(start, 1000), CMTimeMakeWithSeconds(end-start, 1000));
        [seekableRanges addObject:[NSValue valueWithCMTimeRange:range]];
    }

    [m_playerController setSeekableTimeRanges:seekableRanges.get()];
}

void WebPlaybackSessionInterfaceAVKit::canPlayFastReverseChanged(bool canPlayFastReverse)
{
    [m_playerController setCanScanBackward:canPlayFastReverse];
}

static RetainPtr<NSMutableArray> mediaSelectionOptions(const Vector<String>& options)
{
    RetainPtr<NSMutableArray> webOptions = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& name : options) {
        RetainPtr<WebAVMediaSelectionOption> webOption = adoptNS([[WebAVMediaSelectionOption alloc] init]);
        [webOption setLocalizedDisplayName:name];
        [webOptions addObject:webOption.get()];
    }
    return webOptions;
}

void WebPlaybackSessionInterfaceAVKit::audioMediaSelectionOptionsChanged(const Vector<String>& options, uint64_t selectedIndex)
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    [m_playerController setAudioMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [m_playerController setCurrentAudioMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
}

void WebPlaybackSessionInterfaceAVKit::legibleMediaSelectionOptionsChanged(const Vector<String>& options, uint64_t selectedIndex)
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    [m_playerController setLegibleMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [m_playerController setCurrentLegibleMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
}

void WebPlaybackSessionInterfaceAVKit::externalPlaybackChanged(bool enabled, WebPlaybackSessionModel::ExternalPlaybackTargetType targetType, const String& localizedDeviceName)
{
    AVPlayerControllerExternalPlaybackType externalPlaybackType = AVPlayerControllerExternalPlaybackTypeNone;
    if (targetType == WebPlaybackSessionModel::TargetTypeAirPlay)
        externalPlaybackType = AVPlayerControllerExternalPlaybackTypeAirPlay;
    else if (targetType == WebPlaybackSessionModel::TargetTypeTVOut)
        externalPlaybackType = AVPlayerControllerExternalPlaybackTypeTVOut;

    WebAVPlayerController* playerController = m_playerController.get();
    playerController.externalPlaybackAirPlayDeviceLocalizedName = localizedDeviceName;
    playerController.externalPlaybackType = externalPlaybackType;
    playerController.externalPlaybackActive = enabled;
}

void WebPlaybackSessionInterfaceAVKit::wirelessVideoPlaybackDisabledChanged(bool disabled)
{
    [m_playerController setAllowsExternalPlayback:!disabled];
}

void WebPlaybackSessionInterfaceAVKit::invalidate()
{
    if (!m_playbackSessionModel)
        return;

    [m_playerController setDelegate:nullptr];
    m_playbackSessionModel->removeClient(*this);
    m_playbackSessionModel = nullptr;
}

}

#endif // HAVE(AVKIT)
#endif // PLATFORM(IOS)
