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

SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS(AVKit, AVValueTiming)

namespace WebCore {

WebPlaybackSessionInterfaceAVKit::WebPlaybackSessionInterfaceAVKit()
    : m_playerController(adoptNS([[WebAVPlayerController alloc] init]))
{
    [m_playerController setPlaybackSessionInterface:this];
}

WebPlaybackSessionInterfaceAVKit::~WebPlaybackSessionInterfaceAVKit()
{
    WebAVPlayerController* playerController = m_playerController.get();
    if (playerController && playerController.externalPlaybackActive)
        setExternalPlayback(false, TargetTypeNone, "");
}

void WebPlaybackSessionInterfaceAVKit::resetMediaState()
{
    [m_playerController resetState];
}

void WebPlaybackSessionInterfaceAVKit::setWebPlaybackSessionModel(WebPlaybackSessionModel* model)
{
    m_playbackSessionModel = model;
    [m_playerController setDelegate:m_playbackSessionModel];
}

void WebPlaybackSessionInterfaceAVKit::setDuration(double duration)
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

void WebPlaybackSessionInterfaceAVKit::setCurrentTime(double currentTime, double anchorTime)
{
    NSTimeInterval anchorTimeStamp = ![m_playerController rate] ? NAN : anchorTime;
    AVValueTiming *timing = [getAVValueTimingClass() valueTimingWithAnchorValue:currentTime
        anchorTimeStamp:anchorTimeStamp rate:0];

    [m_playerController setTiming:timing];
}

void WebPlaybackSessionInterfaceAVKit::setBufferedTime(double bufferedTime)
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

void WebPlaybackSessionInterfaceAVKit::setRate(bool isPlaying, float playbackRate)
{
    [m_playerController setRate:isPlaying ? playbackRate : 0.];
}

void WebPlaybackSessionInterfaceAVKit::setSeekableRanges(const TimeRanges& timeRanges)
{
    RetainPtr<NSMutableArray> seekableRanges = adoptNS([[NSMutableArray alloc] init]);
    ExceptionCode exceptionCode;

    for (unsigned i = 0; i < timeRanges.length(); i++) {
        double start = timeRanges.start(i, exceptionCode);
        double end = timeRanges.end(i, exceptionCode);

        CMTimeRange range = CMTimeRangeMake(CMTimeMakeWithSeconds(start, 1000), CMTimeMakeWithSeconds(end-start, 1000));
        [seekableRanges addObject:[NSValue valueWithCMTimeRange:range]];
    }

    [m_playerController setSeekableTimeRanges:seekableRanges.get()];
}

void WebPlaybackSessionInterfaceAVKit::setCanPlayFastReverse(bool canPlayFastReverse)
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

void WebPlaybackSessionInterfaceAVKit::setAudioMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex)
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    [m_playerController setAudioMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [m_playerController setCurrentAudioMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
}

void WebPlaybackSessionInterfaceAVKit::setLegibleMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex)
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    [m_playerController setLegibleMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [m_playerController setCurrentLegibleMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
}

void WebPlaybackSessionInterfaceAVKit::setExternalPlayback(bool enabled, ExternalPlaybackTargetType targetType, String localizedDeviceName)
{
    AVPlayerControllerExternalPlaybackType externalPlaybackType = AVPlayerControllerExternalPlaybackTypeNone;
    if (targetType == TargetTypeAirPlay)
        externalPlaybackType = AVPlayerControllerExternalPlaybackTypeAirPlay;
    else if (targetType == TargetTypeTVOut)
        externalPlaybackType = AVPlayerControllerExternalPlaybackTypeTVOut;

    WebAVPlayerController* playerController = m_playerController.get();
    playerController.externalPlaybackAirPlayDeviceLocalizedName = localizedDeviceName;
    playerController.externalPlaybackType = externalPlaybackType;
    playerController.externalPlaybackActive = enabled;

    if (m_client)
        m_client->externalPlaybackEnabledChanged(enabled);
}

void WebPlaybackSessionInterfaceAVKit::setWirelessVideoPlaybackDisabled(bool disabled)
{
    [m_playerController setAllowsExternalPlayback:!disabled];
}

bool WebPlaybackSessionInterfaceAVKit::wirelessVideoPlaybackDisabled() const
{
    return [m_playerController allowsExternalPlayback];
}

void WebPlaybackSessionInterfaceAVKit::invalidate()
{
    m_playbackSessionModel = nil;
}

}

#endif // HAVE(AVKIT)
#endif // PLATFORM(IOS)
