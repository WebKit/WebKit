/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebPlaybackControlsManager.h"

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

#import "SoftLinking.h"
#import "WebPlaybackSessionInterfaceMac.h"
#import "WebPlaybackSessionModel.h"
#import <wtf/text/WTFString.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVFunctionBarMediaSelectionOption)

@implementation WebPlaybackControlsManager

@synthesize contentDuration=_contentDuration;
@synthesize seekToTime=_seekToTime;
@synthesize hasEnabledAudio=_hasEnabledAudio;
@synthesize hasEnabledVideo=_hasEnabledVideo;
@synthesize rate=_rate;
@synthesize canTogglePlayback=_canTogglePlayback;

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

-(BOOL)canBeginFunctionBarScrubbing
{
    // It's not ideal to return YES all the time here. The intent of the API is that we return NO when the
    // media is being scrubbed via the on-screen scrubber. But we can only possibly get the right answer for
    // media that uses the default controls.
    return YES;
}

- (void)beginFunctionBarScrubbing
{
    _webPlaybackSessionInterfaceMac->beginScrubbing();
}

- (void)endFunctionBarScrubbing
{
    _webPlaybackSessionInterfaceMac->endScrubbing();
}

- (NSArray<AVFunctionBarMediaSelectionOption *> *)audioFunctionBarMediaSelectionOptions
{
    return _audioFunctionBarMediaSelectionOptions.get();
}

- (void)setAudioFunctionBarMediaSelectionOptions:(NSArray<AVFunctionBarMediaSelectionOption *> *)audioOptions
{
    _audioFunctionBarMediaSelectionOptions = audioOptions;
}

- (AVFunctionBarMediaSelectionOption *)currentAudioFunctionBarMediaSelectionOption
{
    return _currentAudioFunctionBarMediaSelectionOption.get();
}

- (void)setCurrentAudioFunctionBarMediaSelectionOption:(AVFunctionBarMediaSelectionOption *)audioMediaSelectionOption
{
    if (audioMediaSelectionOption == _currentAudioFunctionBarMediaSelectionOption)
        return;

    _currentAudioFunctionBarMediaSelectionOption = audioMediaSelectionOption;

    NSInteger index = NSNotFound;

    if (audioMediaSelectionOption && _audioFunctionBarMediaSelectionOptions)
        index = [_audioFunctionBarMediaSelectionOptions indexOfObject:audioMediaSelectionOption];

    _webPlaybackSessionInterfaceMac->webPlaybackSessionModel()->selectAudioMediaOption(index != NSNotFound ? index : UINT64_MAX);
}

- (NSArray<AVFunctionBarMediaSelectionOption *> *)legibleFunctionBarMediaSelectionOptions
{
    return _legibleFunctionBarMediaSelectionOptions.get();
}

- (void)setLegibleFunctionBarMediaSelectionOptions:(NSArray<AVFunctionBarMediaSelectionOption *> *)legibleOptions
{
    _legibleFunctionBarMediaSelectionOptions = legibleOptions;
}

- (AVFunctionBarMediaSelectionOption *)currentLegibleFunctionBarMediaSelectionOption
{
    return _currentLegibleFunctionBarMediaSelectionOption.get();
}

- (void)setCurrentLegibleFunctionBarMediaSelectionOption:(AVFunctionBarMediaSelectionOption *)legibleMediaSelectionOption
{
    if (legibleMediaSelectionOption == _currentLegibleFunctionBarMediaSelectionOption)
        return;

    _currentLegibleFunctionBarMediaSelectionOption = legibleMediaSelectionOption;

    NSInteger index = NSNotFound;

    if (legibleMediaSelectionOption && _legibleFunctionBarMediaSelectionOptions)
        index = [_legibleFunctionBarMediaSelectionOptions indexOfObject:legibleMediaSelectionOption];

    _webPlaybackSessionInterfaceMac->webPlaybackSessionModel()->selectLegibleMediaOption(index != NSNotFound ? index : UINT64_MAX);
}

static RetainPtr<NSMutableArray> mediaSelectionOptions(const Vector<String>& options)
{
    RetainPtr<NSMutableArray> webOptions = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& name : options) {
        if (RetainPtr<AVFunctionBarMediaSelectionOption> webOption = adoptNS([allocAVFunctionBarMediaSelectionOptionInstance() initWithTitle:name]))
            [webOptions addObject:webOption.get()];
    }
    return webOptions;
}

- (void)setAudioMediaSelectionOptions:(const Vector<WTF::String>&)options withSelectedIndex:(NSUInteger)selectedIndex
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    [self setAudioFunctionBarMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [self setCurrentAudioFunctionBarMediaSelectionOption:[webOptions objectAtIndex:selectedIndex]];
}

- (void)setLegibleMediaSelectionOptions:(const Vector<WTF::String>&)options withSelectedIndex:(NSUInteger)selectedIndex
{
    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    [self setLegibleFunctionBarMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [self setCurrentLegibleFunctionBarMediaSelectionOption:[webOptions objectAtIndex:selectedIndex]];
}

- (WebCore::WebPlaybackSessionInterfaceMac*)webPlaybackSessionInterfaceMac
{
    return _webPlaybackSessionInterfaceMac.get();
}

- (void)setWebPlaybackSessionInterfaceMac:(WebCore::WebPlaybackSessionInterfaceMac*)webPlaybackSessionInterfaceMac
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

#pragma clang diagnostic pop

@end

#endif // PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE) && ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)

