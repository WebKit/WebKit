/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
#import "PlaybackSessionInterfaceAVKitLegacy.h"

#if PLATFORM(COCOA) && HAVE(AVKIT)

#import "MediaSelectionOption.h"
#import "PlaybackSessionModel.h"
#import "TimeRanges.h"
#import "WebAVPlayerController.h"
#import <AVFoundation/AVTime.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/VectorCocoa.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVValueTiming)

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaybackSessionInterfaceAVKitLegacy);

Ref<PlaybackSessionInterfaceAVKitLegacy> PlaybackSessionInterfaceAVKitLegacy::create(PlaybackSessionModel& model)
{
    Ref interface = adoptRef(*new PlaybackSessionInterfaceAVKitLegacy(model));
    interface->initialize();
    return interface;
}

PlaybackSessionInterfaceAVKitLegacy::PlaybackSessionInterfaceAVKitLegacy(PlaybackSessionModel& model)
    : PlaybackSessionInterfaceIOS(model)
    , m_playerController(createWebAVPlayerController())
{
    ASSERT(isUIThread());
    [m_playerController setPlaybackSessionInterface:this];
    [m_playerController setDelegate:&model];
}

PlaybackSessionInterfaceAVKitLegacy::~PlaybackSessionInterfaceAVKitLegacy()
{
    ASSERT(isUIThread());
    invalidate();
}

WebAVPlayerController *PlaybackSessionInterfaceAVKitLegacy::playerController() const
{
    return m_playerController.get();
}

WKSLinearMediaPlayer *PlaybackSessionInterfaceAVKitLegacy::linearMediaPlayer() const
{
    return nullptr;
}

void PlaybackSessionInterfaceAVKitLegacy::invalidate()
{
    if (!m_playbackSessionModel)
        return;

    [m_playerController setPlaybackSessionInterface:nullptr];
    [m_playerController setExternalPlaybackActive:false];
    [m_playerController setDelegate:nullptr];
    PlaybackSessionInterfaceIOS::invalidate();
}

void PlaybackSessionInterfaceAVKitLegacy::durationChanged(double duration)
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

void PlaybackSessionInterfaceAVKitLegacy::currentTimeChanged(double currentTime, double anchorTime)
{
    if ([m_playerController isScrubbing])
        return;

    NSTimeInterval anchorTimeStamp = ![m_playerController rate] ? NAN : anchorTime;
    AVValueTiming *timing = [getAVValueTimingClassSingleton() valueTimingWithAnchorValue:currentTime
        anchorTimeStamp:anchorTimeStamp rate:0];

    [m_playerController setTiming:timing];
}

void PlaybackSessionInterfaceAVKitLegacy::bufferedTimeChanged(double bufferedTime)
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

void PlaybackSessionInterfaceAVKitLegacy::rateChanged(OptionSet<PlaybackSessionModel::PlaybackState> playbackState, double playbackRate, double defaultPlaybackRate)
{
    [m_playerController setDefaultPlaybackRate:defaultPlaybackRate fromJavaScript:YES];
    if (!playbackState.contains(PlaybackSessionModel::PlaybackState::Stalled))
        [m_playerController setRate:playbackState.contains(PlaybackSessionModel::PlaybackState::Playing) ? playbackRate : 0. fromJavaScript:YES];
}

void PlaybackSessionInterfaceAVKitLegacy::seekableRangesChanged(const PlatformTimeRanges& timeRanges, double lastModifiedTime, double liveUpdateInterval)
{
    [m_playerController setSeekableTimeRanges:makeNSArray(timeRanges).get()];
    [m_playerController setSeekableTimeRangesLastModifiedTime:lastModifiedTime];
    [m_playerController setLiveUpdateInterval:liveUpdateInterval];
}

void PlaybackSessionInterfaceAVKitLegacy::canPlayFastReverseChanged(bool canPlayFastReverse)
{
    [m_playerController setCanScanBackward:canPlayFastReverse];
}

static AVMediaType toAVMediaType(MediaSelectionOption::MediaType type)
{
    switch (type) {
    case MediaSelectionOption::MediaType::Audio:
        return AVMediaTypeAudio;
        break;
    case MediaSelectionOption::MediaType::Subtitles:
        return AVMediaTypeSubtitle;
        break;
    case MediaSelectionOption::MediaType::Captions:
        return AVMediaTypeClosedCaption;
        break;
    case MediaSelectionOption::MediaType::Metadata:
        return AVMediaTypeMetadata;
        break;
    case MediaSelectionOption::MediaType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    return AVMediaTypeMetadata;
}

static RetainPtr<NSArray> mediaSelectionOptions(const Vector<MediaSelectionOption>& options)
{
    return createNSArray(options, [] (auto& option) {
        return adoptNS([[WebAVMediaSelectionOption alloc] initWithMediaType:toAVMediaType(option.mediaType) displayName:option.displayName.createNSString().get()]);
    });
}

void PlaybackSessionInterfaceAVKitLegacy::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    auto webOptions = mediaSelectionOptions(options);
    [m_playerController setAudioMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [m_playerController setCurrentAudioMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
}

void PlaybackSessionInterfaceAVKitLegacy::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    auto webOptions = mediaSelectionOptions(options);
    [m_playerController setLegibleMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [m_playerController setCurrentLegibleMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
}

void PlaybackSessionInterfaceAVKitLegacy::externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType targetType, const String& localizedDeviceName)
{
    AVPlayerControllerExternalPlaybackType externalPlaybackType = AVPlayerControllerExternalPlaybackTypeNone;
    if (enabled && targetType == PlaybackSessionModel::ExternalPlaybackTargetType::TargetTypeAirPlay)
        externalPlaybackType = AVPlayerControllerExternalPlaybackTypeAirPlay;
    else if (enabled && targetType == PlaybackSessionModel::ExternalPlaybackTargetType::TargetTypeTVOut)
        externalPlaybackType = AVPlayerControllerExternalPlaybackTypeTVOut;

    WebAVPlayerController* playerController = m_playerController.get();
    playerController.externalPlaybackAirPlayDeviceLocalizedName = localizedDeviceName.createNSString().get();
    playerController.externalPlaybackType = externalPlaybackType;
    playerController.externalPlaybackActive = enabled;
}

void PlaybackSessionInterfaceAVKitLegacy::wirelessVideoPlaybackDisabledChanged(bool disabled)
{
    [m_playerController setAllowsExternalPlayback:!disabled];
}

void PlaybackSessionInterfaceAVKitLegacy::mutedChanged(bool muted)
{
    [m_playerController setMuted:muted];
}

void PlaybackSessionInterfaceAVKitLegacy::volumeChanged(double volume)
{
    [m_playerController volumeChanged:volume];
}

#if !RELEASE_LOG_DISABLED
ASCIILiteral PlaybackSessionInterfaceAVKitLegacy::logClassName() const
{
    return "PlaybackSessionInterfaceAVKitLegacy"_s;
}
#endif
} // namespace WebCore

#endif // PLATFORM(COCOA) && HAVE(AVKIT)
