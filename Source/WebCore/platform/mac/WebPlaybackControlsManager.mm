/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#import "SoftLinking.h"
#import "WebPlaybackSessionInterfaceMac.h"
#import "WebPlaybackSessionModel.h"
#import <wtf/text/WTFString.h>

using namespace WebCore;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
SOFT_LINK_FRAMEWORK(AVKit)
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVTouchBarMediaSelectionOption)
#else
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVFunctionBarMediaSelectionOption)
#endif // __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300

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
    if (_webPlaybackSessionInterfaceMac)
        _webPlaybackSessionInterfaceMac->setPlayBackControlsManager(nullptr);
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
    _webPlaybackSessionInterfaceMac->webPlaybackSessionModel()->seekToTime(time);
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
    // It's not ideal to return YES all the time here. The intent of the API is that we return NO when the
    // media is being scrubbed via the on-screen scrubber. But we can only possibly get the right answer for
    // media that uses the default controls.
    return YES;
}

- (void)beginTouchBarScrubbing
{
    _webPlaybackSessionInterfaceMac->beginScrubbing();
}

- (void)endTouchBarScrubbing
{
    _webPlaybackSessionInterfaceMac->endScrubbing();
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101300

- (void)generateFunctionBarThumbnailsForTimes:(NSArray<NSNumber *> *)thumbnailTimes size:(NSSize)size completionHandler:(void (^)(NSArray<AVThumbnail *> *thumbnails, NSError *error))completionHandler
{
    UNUSED_PARAM(thumbnailTimes);
    UNUSED_PARAM(size);
    completionHandler(@[ ], nil);
}

- (void)generateFunctionBarAudioAmplitudeSamples:(NSInteger)numberOfSamples completionHandler:(void (^)(NSArray<NSNumber *> *audioAmplitudeSamples,  NSError *error))completionHandler
{
    UNUSED_PARAM(numberOfSamples);
    completionHandler(@[ ], nil);
}

- (BOOL)canBeginFunctionBarScrubbing
{
    return [self canBeginTouchBarScrubbing];
}

- (void)beginFunctionBarScrubbing
{
    [self beginTouchBarScrubbing];
}

- (void)endFunctionBarScrubbing
{
    [self endTouchBarScrubbing];
}

#endif

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

    _webPlaybackSessionInterfaceMac->webPlaybackSessionModel()->selectAudioMediaOption(index != NSNotFound ? index : UINT64_MAX);
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

    _webPlaybackSessionInterfaceMac->webPlaybackSessionModel()->selectLegibleMediaOption(index != NSNotFound ? index : UINT64_MAX);
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
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
#endif

static RetainPtr<NSMutableArray> mediaSelectionOptions(const Vector<MediaSelectionOption>& options)
{
    RetainPtr<NSMutableArray> webOptions = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& option : options) {
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
        if (auto webOption = adoptNS([allocAVTouchBarMediaSelectionOptionInstance() initWithTitle:option.displayName type:toAVTouchBarMediaSelectionOptionType(option.type)]))
#else
        if (auto webOption = adoptNS([allocAVFunctionBarMediaSelectionOptionInstance() initWithTitle:option.displayName]))
#endif // __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300
            [webOptions addObject:webOption.get()];
    }
    return webOptions;
}

- (void)setAudioMediaSelectionOptions:(const Vector<MediaSelectionOption>&)options withSelectedIndex:(NSUInteger)selectedIndex
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    [self setAudioTouchBarMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [self setCurrentAudioTouchBarMediaSelectionOption:[webOptions objectAtIndex:selectedIndex]];
}

- (void)setLegibleMediaSelectionOptions:(const Vector<MediaSelectionOption>&)options withSelectedIndex:(NSUInteger)selectedIndex
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
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

- (WebPlaybackSessionInterfaceMac*)webPlaybackSessionInterfaceMac
{
    return _webPlaybackSessionInterfaceMac.get();
}

- (void)setWebPlaybackSessionInterfaceMac:(WebPlaybackSessionInterfaceMac*)webPlaybackSessionInterfaceMac
{
    if (_webPlaybackSessionInterfaceMac == webPlaybackSessionInterfaceMac)
        return;

    if (_webPlaybackSessionInterfaceMac)
        _webPlaybackSessionInterfaceMac->setPlayBackControlsManager(nullptr);

    _webPlaybackSessionInterfaceMac = webPlaybackSessionInterfaceMac;

    if (_webPlaybackSessionInterfaceMac)
        _webPlaybackSessionInterfaceMac->setPlayBackControlsManager(self);
}

- (void)togglePlayback
{
    if (_webPlaybackSessionInterfaceMac && _webPlaybackSessionInterfaceMac->webPlaybackSessionModel())
        _webPlaybackSessionInterfaceMac->webPlaybackSessionModel()->togglePlayState();
}

- (void)setPlaying:(BOOL)playing
{
    if (!_webPlaybackSessionInterfaceMac || !_webPlaybackSessionInterfaceMac->webPlaybackSessionModel())
        return;

    BOOL isCurrentlyPlaying = self.playing;
    if (!isCurrentlyPlaying && playing)
        _webPlaybackSessionInterfaceMac->webPlaybackSessionModel()->play();
    else if (isCurrentlyPlaying && !playing)
        _webPlaybackSessionInterfaceMac->webPlaybackSessionModel()->pause();
}

- (BOOL)isPlaying
{
    if (_webPlaybackSessionInterfaceMac && _webPlaybackSessionInterfaceMac->webPlaybackSessionModel())
        return _webPlaybackSessionInterfaceMac->webPlaybackSessionModel()->isPlaying();

    return NO;
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300

- (void)togglePictureInPicture
{
    if (_webPlaybackSessionInterfaceMac && _webPlaybackSessionInterfaceMac->webPlaybackSessionModel())
        _webPlaybackSessionInterfaceMac->webPlaybackSessionModel()->togglePictureInPicture();
}

#endif // __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300

#pragma clang diagnostic pop

@end

#endif // PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

