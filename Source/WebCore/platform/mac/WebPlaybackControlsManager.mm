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
#import "WebPlaybackControlsManager.h"

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

#import "MediaSelectionOption.h"
#import "PlaybackSessionInterfaceMac.h"
#import "PlaybackSessionModel.h"
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

IGNORE_WARNINGS_BEGIN("nullability-completeness")

SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVTouchBarMediaSelectionOption)

using WebCore::MediaSelectionOption;
using WebCore::PlaybackSessionInterfaceMac;

@implementation WebPlaybackControlsManager

@synthesize contentDuration=_contentDuration;
@synthesize seekToTime=_seekToTime;
@synthesize hasEnabledAudio=_hasEnabledAudio;
@synthesize hasEnabledVideo=_hasEnabledVideo;
@synthesize rate=_rate;
@synthesize canTogglePlayback=_canTogglePlayback;
@synthesize allowsPictureInPicturePlayback;
@synthesize pictureInPictureActive;
@synthesize canTogglePictureInPicture;

- (void)dealloc
{
    if (_playbackSessionInterfaceMac)
        _playbackSessionInterfaceMac->setPlayBackControlsManager(nullptr);
    [super dealloc];
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
}

- (BOOL)isSeeking
{
    return NO;
}

- (void)seekToTime:(NSTimeInterval)time toleranceBefore:(NSTimeInterval)toleranceBefore toleranceAfter:(NSTimeInterval)toleranceAfter
{
    UNUSED_PARAM(toleranceBefore);
    UNUSED_PARAM(toleranceAfter);
    if (auto* model = _playbackSessionInterfaceMac->playbackSessionModel())
        model->seekToTime(time);
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

- (BOOL)canBeginTouchBarScrubbing
{
    // At this time, we return YES for all media that is not a live stream and media that is not Netflix. (A Netflix
    // quirk means we pretend Netflix is a live stream for Touch Bar.) It's not ideal to return YES all the time for
    // other media. The intent of the API is that we return NO when the media is being scrubbed via the on-screen scrubber.
    // But we can only possibly get the right answer for media that uses the default controls.
    return std::isfinite(_contentDuration) && [_seekableTimeRanges count];
}

- (void)beginTouchBarScrubbing
{
    _playbackSessionInterfaceMac->beginScrubbing();
}

- (void)endTouchBarScrubbing
{
    _playbackSessionInterfaceMac->endScrubbing();
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

    if (auto* model = _playbackSessionInterfaceMac->playbackSessionModel())
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

    if (auto* model = _playbackSessionInterfaceMac->playbackSessionModel())
        model->selectLegibleMediaOption(index != NSNotFound ? index : UINT64_MAX);
}

static AVTouchBarMediaSelectionOptionType toAVTouchBarMediaSelectionOptionType(MediaSelectionOption::Type type)
{
    switch (type) {
    case MediaSelectionOption::Type::Regular:
        return AVTouchBarMediaSelectionOptionTypeRegular;
    case MediaSelectionOption::Type::LegibleOff:
        return AVTouchBarMediaSelectionOptionTypeLegibleOff;
    case MediaSelectionOption::Type::LegibleAuto:
        return AVTouchBarMediaSelectionOptionTypeLegibleAuto;
    }

    ASSERT_NOT_REACHED();
    return AVTouchBarMediaSelectionOptionTypeRegular;
}

static RetainPtr<NSArray> mediaSelectionOptions(const Vector<MediaSelectionOption>& options)
{
    return createNSArray(options, [] (auto& option) {
        return adoptNS([allocAVTouchBarMediaSelectionOptionInstance() initWithTitle:option.displayName type:toAVTouchBarMediaSelectionOptionType(option.type)]);
    });
}

- (void)setAudioMediaSelectionOptions:(const Vector<MediaSelectionOption>&)options withSelectedIndex:(NSUInteger)selectedIndex
{
    auto webOptions = mediaSelectionOptions(options);
    [self setAudioTouchBarMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [self setCurrentAudioTouchBarMediaSelectionOption:[webOptions objectAtIndex:selectedIndex]];
}

- (void)setLegibleMediaSelectionOptions:(const Vector<MediaSelectionOption>&)options withSelectedIndex:(NSUInteger)selectedIndex
{
    auto webOptions = mediaSelectionOptions(options);
    [self setLegibleTouchBarMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [self setCurrentLegibleTouchBarMediaSelectionOption:[webOptions objectAtIndex:selectedIndex]];
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

    if (_playbackSessionInterfaceMac)
        _playbackSessionInterfaceMac->setPlayBackControlsManager(nullptr);

    _playbackSessionInterfaceMac = playbackSessionInterfaceMac;

    if (_playbackSessionInterfaceMac)
        _playbackSessionInterfaceMac->setPlayBackControlsManager(self);
}

- (void)togglePlayback
{
    if (_playbackSessionInterfaceMac && _playbackSessionInterfaceMac->playbackSessionModel())
        _playbackSessionInterfaceMac->playbackSessionModel()->togglePlayState();
}

- (void)setPlaying:(BOOL)playing
{
    if (!_playbackSessionInterfaceMac || !_playbackSessionInterfaceMac->playbackSessionModel())
        return;

    BOOL isCurrentlyPlaying = self.playing;
    if (!isCurrentlyPlaying && playing)
        _playbackSessionInterfaceMac->playbackSessionModel()->play();
    else if (isCurrentlyPlaying && !playing)
        _playbackSessionInterfaceMac->playbackSessionModel()->pause();
}

- (BOOL)isPlaying
{
    if (_playbackSessionInterfaceMac && _playbackSessionInterfaceMac->playbackSessionModel())
        return _playbackSessionInterfaceMac->playbackSessionModel()->isPlaying();

    return NO;
}

- (void)togglePictureInPicture
{
    if (_playbackSessionInterfaceMac && _playbackSessionInterfaceMac->playbackSessionModel())
        _playbackSessionInterfaceMac->playbackSessionModel()->togglePictureInPicture();
}

IGNORE_WARNINGS_END

@end

#endif // PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
