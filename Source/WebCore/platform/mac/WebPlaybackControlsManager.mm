/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#import "WebPlaybackControlsManager.h"

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

#import "MediaSelectionOption.h"
#import "PlaybackSessionInterfaceMac.h"
#import "PlaybackSessionModel.h"
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

#import <pal/cf/CoreMediaSoftLink.h>

IGNORE_WARNINGS_BEGIN("nullability-completeness")

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVTouchBarMediaSelectionOption)

using WebCore::MediaSelectionOption;
using WebCore::PlaybackSessionInterfaceMac;

@implementation WebPlaybackControlsManager

@synthesize seekToTime = _seekToTime;
@synthesize hasEnabledAudio = _hasEnabledAudio;
@synthesize hasEnabledVideo = _hasEnabledVideo;
@synthesize canTogglePlayback = _canTogglePlayback;
@synthesize allowsPictureInPicturePlayback;
@synthesize pictureInPictureActive;
@synthesize canTogglePictureInPicture;

- (void)dealloc
{
    if (RefPtr playbackSessionInterfaceMac = _playbackSessionInterfaceMac)
        playbackSessionInterfaceMac->setPlayBackControlsManager(nullptr);
    [super dealloc];
}

- (BOOL)canSeek
{
    return _canSeek;
}

- (void)setCanSeek:(BOOL)canSeek
{
    _canSeek = canSeek;
}

+ (NSSet<NSString *> *)keyPathsForValuesAffectingContentDuration
{
    return [NSSet setWithObject:@"seekableTimeRanges"];
}

- (NSTimeInterval)contentDuration
{
    return [_seekableTimeRanges count] ? _contentDuration : std::numeric_limits<double>::infinity();
}

- (void)setContentDuration:(NSTimeInterval)duration
{
    bool needCanSeekUpdate = std::isfinite(_contentDuration) != std::isfinite(duration);
    _contentDuration = duration;
    // Workaround rdar://82275552. We do so by toggling the canSeek property to force
    // a content refresh that will make the scrubber appear/disappear accordingly.
    if (needCanSeekUpdate) {
        bool canSeek = _canSeek;
        self.canSeek = !canSeek;
        self.canSeek = canSeek;
    }
}

- (AVValueTiming *)timing
{
    return _timing.get();
}

- (void)setTiming:(AVValueTiming *)timing
{
    _timing = timing;
}

- (NSArray *)seekableTimeRanges
{
    return _seekableTimeRanges.get();
}

- (void)setSeekableTimeRanges:(NSArray *)timeRanges
{
    _seekableTimeRanges = timeRanges;
    self.canSeek = timeRanges.count;
}

- (BOOL)isSeeking
{
    return NO;
}

- (void)seekToTime:(NSTimeInterval)time toleranceBefore:(NSTimeInterval)toleranceBefore toleranceAfter:(NSTimeInterval)toleranceAfter
{
    RefPtr playbackSessionInterfaceMac = _playbackSessionInterfaceMac;
    if (!playbackSessionInterfaceMac)
        return;

    if (CheckedPtr model = playbackSessionInterfaceMac->playbackSessionModel())
        model->sendRemoteCommand(WebCore::PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand, { time, toleranceBefore || toleranceAfter });
}

- (void)cancelThumbnailAndAudioAmplitudeSampleGeneration
{
}

- (void)generateTouchBarThumbnailsForTimes:(NSArray<NSNumber *> *)thumbnailTimes tolerance:(NSTimeInterval)tolerance size:(NSSize)size thumbnailHandler:(void (^)(NSArray<AVThumbnail *> *thumbnails, BOOL thumbnailGenerationFailed))thumbnailHandler
{
    UNUSED_PARAM(thumbnailTimes);
    UNUSED_PARAM(tolerance);
    UNUSED_PARAM(size);
    thumbnailHandler(@[ ], YES);
}

- (void)generateTouchBarAudioAmplitudeSamples:(NSInteger)numberOfSamples completionHandler:(void (^)(NSArray<NSNumber *> *audioAmplitudeSamples))completionHandler
{
    UNUSED_PARAM(numberOfSamples);
    completionHandler(@[ ]);
}

+ (NSSet<NSString *> *)keyPathsForValuesAffectingCanBeginTouchBarScrubbing
{
    return [NSSet setWithObjects:@"canSeek", @"contentDuration", nil];
}

- (BOOL)canBeginTouchBarScrubbing
{
    // At this time, we return YES for all media that is not a live stream and media that is not Netflix. (A Netflix
    // quirk means we pretend Netflix is a live stream for Touch Bar.) It's not ideal to return YES all the time for
    // other media. The intent of the API is that we return NO when the media is being scrubbed via the on-screen scrubber.
    // But we can only possibly get the right answer for media that uses the default controls.
    return _canSeek && std::isfinite(_contentDuration);
}

- (void)beginTouchBarScrubbing
{
    RefPtr playbackSessionInterfaceMac = _playbackSessionInterfaceMac;
    if (!playbackSessionInterfaceMac)
        return;

    CheckedPtr model = playbackSessionInterfaceMac->playbackSessionModel();
    if (!model)
        return;
        
    playbackSessionInterfaceMac->willBeginScrubbing();
    model->sendRemoteCommand(WebCore::PlatformMediaSession::RemoteControlCommandType::BeginScrubbingCommand, { });
}

- (void)endTouchBarScrubbing
{
    RefPtr playbackSessionInterfaceMac = _playbackSessionInterfaceMac;
    if (!playbackSessionInterfaceMac)
        return;

    if (CheckedPtr model = playbackSessionInterfaceMac->playbackSessionModel())
        model->sendRemoteCommand(WebCore::PlatformMediaSession::RemoteControlCommandType::EndScrubbingCommand, { });
}

- (NSArray<AVTouchBarMediaSelectionOption *> *)audioTouchBarMediaSelectionOptions
{
    return _audioTouchBarMediaSelectionOptions.get();
}

- (void)setAudioTouchBarMediaSelectionOptions:(NSArray<AVTouchBarMediaSelectionOption *> *)audioOptions
{
    _audioTouchBarMediaSelectionOptions = audioOptions;
}

- (AVTouchBarMediaSelectionOption *)currentAudioTouchBarMediaSelectionOption
{
    return _currentAudioTouchBarMediaSelectionOption.get();
}

- (void)setCurrentAudioTouchBarMediaSelectionOption:(AVTouchBarMediaSelectionOption *)audioMediaSelectionOption
{
    if (audioMediaSelectionOption == _currentAudioTouchBarMediaSelectionOption)
        return;

    _currentAudioTouchBarMediaSelectionOption = audioMediaSelectionOption;

    NSInteger index = NSNotFound;

    if (audioMediaSelectionOption && _audioTouchBarMediaSelectionOptions)
        index = [_audioTouchBarMediaSelectionOptions indexOfObject:audioMediaSelectionOption];

    if (CheckedPtr model = Ref { *_playbackSessionInterfaceMac }->playbackSessionModel())
        model->selectAudioMediaOption(index != NSNotFound ? index : UINT64_MAX);
}

- (NSArray<AVTouchBarMediaSelectionOption *> *)legibleTouchBarMediaSelectionOptions
{
    return _legibleTouchBarMediaSelectionOptions.get();
}

- (void)setLegibleTouchBarMediaSelectionOptions:(NSArray<AVTouchBarMediaSelectionOption *> *)legibleOptions
{
    _legibleTouchBarMediaSelectionOptions = legibleOptions;
}

- (AVTouchBarMediaSelectionOption *)currentLegibleTouchBarMediaSelectionOption
{
    return _currentLegibleTouchBarMediaSelectionOption.get();
}

- (void)setCurrentLegibleTouchBarMediaSelectionOption:(AVTouchBarMediaSelectionOption *)legibleMediaSelectionOption
{
    if (legibleMediaSelectionOption == _currentLegibleTouchBarMediaSelectionOption)
        return;

    _currentLegibleTouchBarMediaSelectionOption = legibleMediaSelectionOption;

    NSInteger index = NSNotFound;

    if (legibleMediaSelectionOption && _legibleTouchBarMediaSelectionOptions)
        index = [_legibleTouchBarMediaSelectionOptions indexOfObject:legibleMediaSelectionOption];

    if (CheckedPtr model = Ref { *_playbackSessionInterfaceMac }->playbackSessionModel())
        model->selectLegibleMediaOption(index != NSNotFound ? index : UINT64_MAX);
}

static AVTouchBarMediaSelectionOptionType toAVTouchBarMediaSelectionOptionType(MediaSelectionOption::LegibleType type)
{
    switch (type) {
    case MediaSelectionOption::LegibleType::LegibleOn:
    case MediaSelectionOption::LegibleType::Regular:
        return AVTouchBarMediaSelectionOptionTypeRegular;
    case MediaSelectionOption::LegibleType::LegibleOff:
        return AVTouchBarMediaSelectionOptionTypeLegibleOff;
    case MediaSelectionOption::LegibleType::LegibleAuto:
        return AVTouchBarMediaSelectionOptionTypeLegibleAuto;
    }

    ASSERT_NOT_REACHED();
    return AVTouchBarMediaSelectionOptionTypeRegular;
}

static RetainPtr<NSArray> mediaSelectionOptions(const Vector<MediaSelectionOption>& options)
{
    return createNSArray(options, [] (auto& option) {
        return adoptNS([allocAVTouchBarMediaSelectionOptionInstance() initWithTitle:option.displayName.createNSString().get() type:toAVTouchBarMediaSelectionOptionType(option.legibleType)]);
    });
}

- (void)setAudioMediaSelectionOptions:(const Vector<MediaSelectionOption>&)options withSelectedIndex:(NSUInteger)selectedIndex
{
    auto webOptions = mediaSelectionOptions(options);
    [self setAudioTouchBarMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [self setCurrentAudioTouchBarMediaSelectionOption:retainPtr([webOptions objectAtIndex:selectedIndex]).get()];
}

- (void)setLegibleMediaSelectionOptions:(const Vector<MediaSelectionOption>&)options withSelectedIndex:(NSUInteger)selectedIndex
{
    auto webOptions = mediaSelectionOptions(options);
    [self setLegibleTouchBarMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [self setCurrentLegibleTouchBarMediaSelectionOption:retainPtr([webOptions objectAtIndex:selectedIndex]).get()];
}

- (void)setAudioMediaSelectionIndex:(NSUInteger)selectedIndex
{
    if (selectedIndex >= [_audioTouchBarMediaSelectionOptions count])
        return;

    [self willChangeValueForKey:@"currentAudioTouchBarMediaSelectionOption"];
    _currentAudioTouchBarMediaSelectionOption = [_audioTouchBarMediaSelectionOptions objectAtIndex:selectedIndex];
    [self didChangeValueForKey:@"currentAudioTouchBarMediaSelectionOption"];
}

- (void)setLegibleMediaSelectionIndex:(NSUInteger)selectedIndex
{
    if (selectedIndex >= [_legibleTouchBarMediaSelectionOptions count])
        return;

    [self willChangeValueForKey:@"currentLegibleTouchBarMediaSelectionOption"];
    _currentLegibleTouchBarMediaSelectionOption = [_legibleTouchBarMediaSelectionOptions objectAtIndex:selectedIndex];
    [self didChangeValueForKey:@"currentLegibleTouchBarMediaSelectionOption"];
}

- (PlaybackSessionInterfaceMac*)playbackSessionInterfaceMac
{
    return _playbackSessionInterfaceMac.get();
}

- (void)setPlaybackSessionInterfaceMac:(PlaybackSessionInterfaceMac*)playbackSessionInterfaceMac
{
    if (_playbackSessionInterfaceMac == playbackSessionInterfaceMac)
        return;

    if (RefPtr playbackSessionInterfaceMac = _playbackSessionInterfaceMac)
        playbackSessionInterfaceMac->setPlayBackControlsManager(nullptr);

    _playbackSessionInterfaceMac = playbackSessionInterfaceMac;

    if (RefPtr playbackSessionInterfaceMac = _playbackSessionInterfaceMac)
        playbackSessionInterfaceMac->setPlayBackControlsManager(self);
}

- (void)togglePlayback
{
    RefPtr playbackSessionInterfaceMac = _playbackSessionInterfaceMac;
    if (!playbackSessionInterfaceMac)
        return;

    if (CheckedPtr model = playbackSessionInterfaceMac->playbackSessionModel())
        model->sendRemoteCommand(WebCore::PlatformMediaSession::RemoteControlCommandType::TogglePlayPauseCommand, { });
}

- (void)setPlaying:(BOOL)playing
{
    if (playing != _playing) {
        [self willChangeValueForKey:@"playing"];
        _playing = playing;
        [self didChangeValueForKey:@"playing"];
    }

    RefPtr playbackSessionInterfaceMac = _playbackSessionInterfaceMac;
    if (!playbackSessionInterfaceMac)
        return;

    if (CheckedPtr model = playbackSessionInterfaceMac->playbackSessionModel(); model && model->isPlaying() != _playing)
        model->sendRemoteCommand(_playing ? WebCore::PlatformMediaSession::RemoteControlCommandType::PlayCommand : WebCore::PlatformMediaSession::RemoteControlCommandType::PauseCommand, { });
}

- (BOOL)isPlaying
{
    return _playing;
}

- (double)defaultPlaybackRate
{
    return _defaultPlaybackRate;
}

- (void)setDefaultPlaybackRate:(double)defaultPlaybackRate
{
    [self setDefaultPlaybackRate:defaultPlaybackRate fromJavaScript:NO];
}

- (void)setDefaultPlaybackRate:(double)defaultPlaybackRate fromJavaScript:(BOOL)fromJavaScript
{
    if (defaultPlaybackRate == _defaultPlaybackRate)
        return;

    _defaultPlaybackRate = defaultPlaybackRate;

    if (!fromJavaScript) {
        if (RefPtr playbackSessionInterfaceMac = _playbackSessionInterfaceMac) {
            if (CheckedPtr model = playbackSessionInterfaceMac->playbackSessionModel(); model && model->defaultPlaybackRate() != _defaultPlaybackRate)
                model->setDefaultPlaybackRate(_defaultPlaybackRate);
        }
    }

    if ([self isPlaying])
        [self setRate:_defaultPlaybackRate fromJavaScript:fromJavaScript];
}

- (float)rate
{
    return _rate;
}

- (void)setRate:(float)rate
{
    [self setRate:rate fromJavaScript:NO];
}

- (void)setRate:(double)rate fromJavaScript:(BOOL)fromJavaScript
{
    if (rate == _rate)
        return;

    _rate = rate;

    // AVKit doesn't have a separate variable for "paused", instead representing it by a `rate` of
    // `0`. Unfortunately, `HTMLMediaElement::play` doesn't call `HTMLMediaElement::setPlaybackRate`
    // so if we propagate a `rate` of `0` along to the `HTMLMediaElement` then any attempt to
    // `HTMLMediaElement::play` will effectively be a no-op since the `playbackRate` will be `0`.
    if (!_rate)
        return;

    // In AVKit, the `defaultPlaybackRate` is used when playback starts, such as resuming after
    // pausing. In WebKit, however, `defaultPlaybackRate` is only used when first loading and after
    // ending scanning, with the `playbackRate` being used in all other cases, including when
    // resuming after pausing. As such, WebKit should return the `playbackRate` instead of the
    // `defaultPlaybackRate` in these cases when communicating with AVKit.
    [self setDefaultPlaybackRate:_rate fromJavaScript:fromJavaScript];

    if (!fromJavaScript) {
        if (RefPtr playbackSessionInterfaceMac = _playbackSessionInterfaceMac) {
            if (CheckedPtr model = playbackSessionInterfaceMac->playbackSessionModel(); model && model->playbackRate() != _rate)
                model->setPlaybackRate(_rate);
        }
    }
}

- (void)togglePictureInPicture
{
    if (CheckedPtr model = Ref { *_playbackSessionInterfaceMac }->playbackSessionModel())
        model->togglePictureInPicture();
}

- (void)enterInWindow
{
    if (CheckedPtr model = Ref { *_playbackSessionInterfaceMac }->playbackSessionModel())
        model->enterInWindowFullscreen();
}

- (void)exitInWindow
{
    if (CheckedPtr model = Ref { *_playbackSessionInterfaceMac }->playbackSessionModel())
        model->exitInWindowFullscreen();
}


IGNORE_WARNINGS_END

@end

#endif // PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
