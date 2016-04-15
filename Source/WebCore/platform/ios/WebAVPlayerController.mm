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
#import "WebAVPlayerController.h"

#if PLATFORM(IOS)

#import "AVKitSPI.h"
#import "Logging.h"
#import "TimeRanges.h"
#import "WebPlaybackSessionInterfaceAVKit.h"
#import "WebPlaybackSessionModel.h"
#import <AVFoundation/AVTime.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS(AVKit, AVPlayerController)

using namespace WebCore;

@implementation WebAVPlayerController

- (instancetype)init
{
    if (!(self = [super init]))
        return self;

    initAVPlayerController();
    self.playerControllerProxy = [[allocAVPlayerControllerInstance() init] autorelease];
    return self;
}

- (void)dealloc
{
    [_playerControllerProxy release];
    [_loadedTimeRanges release];
    [_seekableTimeRanges release];
    [_timing release];
    [_audioMediaSelectionOptions release];
    [_legibleMediaSelectionOptions release];
    [_currentAudioMediaSelectionOption release];
    [_currentLegibleMediaSelectionOption release];
    [super dealloc];
}

- (void)resetState
{
    self.contentDuration = 0;
    self.maxTime = 0;
    self.contentDurationWithinEndTimes = 0;
    self.loadedTimeRanges = @[];

    self.canPlay = NO;
    self.canPause = NO;
    self.canTogglePlayback = NO;
    self.hasEnabledAudio = NO;
    self.canSeek = NO;
    self.minTime = 0;
    self.status = AVPlayerControllerStatusUnknown;

    self.timing = nil;
    self.rate = 0;

    self.hasEnabledVideo = NO;
    self.contentDimensions = CGSizeMake(0, 0);

    self.seekableTimeRanges = [NSMutableArray array];

    self.canScanBackward = NO;

    self.audioMediaSelectionOptions = nil;
    self.currentAudioMediaSelectionOption = nil;

    self.legibleMediaSelectionOptions = nil;
    self.currentLegibleMediaSelectionOption = nil;
}

- (AVPlayer *)player
{
    return nil;
}

- (id)forwardingTargetForSelector:(SEL)selector
{
    UNUSED_PARAM(selector);
    return self.playerControllerProxy;
}

- (void)play:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->play();
}

- (void)pause:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->pause();
}

- (void)togglePlayback:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->togglePlayState();
}

- (void)togglePlaybackEvenWhenInBackground:(id)sender
{
    [self togglePlayback:sender];
}

- (BOOL)isPlaying
{
    return [self rate];
}

- (void)setPlaying:(BOOL)playing
{
    if (!self.delegate)
        return;
    if (playing)
        self.delegate->play();
    else
        self.delegate->pause();
}

+ (NSSet *)keyPathsForValuesAffectingPlaying
{
    return [NSSet setWithObject:@"rate"];
}

- (void)beginScrubbing:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->beginScrubbing();
}

- (void)endScrubbing:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->endScrubbing();
}

- (void)seekToTime:(NSTimeInterval)time
{
    if (!self.delegate)
        return;
    self.delegate->fastSeek(time);
}

- (NSTimeInterval)currentTimeWithinEndTimes
{
    return self.timing.currentValue;
}

- (void)setCurrentTimeWithinEndTimes:(NSTimeInterval)currentTimeWithinEndTimes
{
    [self seekToTime:currentTimeWithinEndTimes];
}

+ (NSSet *)keyPathsForValuesAffectingCurrentTimeWithinEndTimes
{
    return [NSSet setWithObject:@"timing"];
}

- (BOOL)hasLiveStreamingContent
{
    if ([self status] == AVPlayerControllerStatusReadyToPlay)
        return [self contentDuration] == std::numeric_limits<float>::infinity();
    return NO;
}

+ (NSSet *)keyPathsForValuesAffectingHasLiveStreamingContent
{
    return [NSSet setWithObjects:@"contentDuration", @"status", nil];
}

- (void)skipBackwardThirtySeconds:(id)sender
{
    UNUSED_PARAM(sender);
    BOOL isTimeWithinSeekableTimeRanges = NO;
    CMTime currentTime = CMTimeMakeWithSeconds([[self timing] currentValue], 1000);
    CMTime thirtySecondsBeforeCurrentTime = CMTimeSubtract(currentTime, CMTimeMake(30, 1));

    for (NSValue *seekableTimeRangeValue in [self seekableTimeRanges]) {
        if (CMTimeRangeContainsTime([seekableTimeRangeValue CMTimeRangeValue], thirtySecondsBeforeCurrentTime)) {
            isTimeWithinSeekableTimeRanges = YES;
            break;
        }
    }

    if (isTimeWithinSeekableTimeRanges)
        [self seekToTime:CMTimeGetSeconds(thirtySecondsBeforeCurrentTime)];
}

- (void)gotoEndOfSeekableRanges:(id)sender
{
    UNUSED_PARAM(sender);
    NSTimeInterval timeAtEndOfSeekableTimeRanges = NAN;

    for (NSValue *seekableTimeRangeValue in [self seekableTimeRanges]) {
        CMTimeRange seekableTimeRange = [seekableTimeRangeValue CMTimeRangeValue];
        NSTimeInterval endOfSeekableTimeRange = CMTimeGetSeconds(CMTimeRangeGetEnd(seekableTimeRange));
        if (isnan(timeAtEndOfSeekableTimeRanges) || endOfSeekableTimeRange > timeAtEndOfSeekableTimeRanges)
            timeAtEndOfSeekableTimeRanges = endOfSeekableTimeRange;
    }

    if (!isnan(timeAtEndOfSeekableTimeRanges))
        [self seekToTime:timeAtEndOfSeekableTimeRanges];
}

- (BOOL)canScanForward
{
    return [self canPlay];
}

+ (NSSet *)keyPathsForValuesAffectingCanScanForward
{
    return [NSSet setWithObject:@"canPlay"];
}

- (void)beginScanningForward:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->beginScanningForward();
}

- (void)endScanningForward:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->endScanning();
}

- (void)beginScanningBackward:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->beginScanningBackward();
}

- (void)endScanningBackward:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->endScanning();
}

- (BOOL)canSeekToBeginning
{
    CMTime minimumTime = kCMTimeIndefinite;

    for (NSValue *value in [self seekableTimeRanges])
        minimumTime = CMTimeMinimum([value CMTimeRangeValue].start, minimumTime);

    return CMTIME_IS_NUMERIC(minimumTime);
}

+ (NSSet *)keyPathsForValuesAffectingCanSeekToBeginning
{
    return [NSSet setWithObject:@"seekableTimeRanges"];
}

- (void)seekToBeginning:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->seekToTime(-INFINITY);
}

- (void)seekChapterBackward:(id)sender
{
    [self seekToBeginning:sender];
}

- (BOOL)canSeekToEnd
{
    CMTime maximumTime = kCMTimeIndefinite;

    for (NSValue *value in [self seekableTimeRanges])
        maximumTime = CMTimeMaximum(CMTimeRangeGetEnd([value CMTimeRangeValue]), maximumTime);

    return CMTIME_IS_NUMERIC(maximumTime);
}

+ (NSSet *)keyPathsForValuesAffectingCanSeekToEnd
{
    return [NSSet setWithObject:@"seekableTimeRanges"];
}

- (void)seekToEnd:(id)sender
{
    UNUSED_PARAM(sender);
    if (!self.delegate)
        return;
    self.delegate->seekToTime(INFINITY);
}

- (void)seekChapterForward:(id)sender
{
    [self seekToEnd:sender];
}

- (BOOL)hasMediaSelectionOptions
{
    return [self hasAudioMediaSelectionOptions] || [self hasLegibleMediaSelectionOptions];
}

+ (NSSet *)keyPathsForValuesAffectingHasMediaSelectionOptions
{
    return [NSSet setWithObjects:@"hasAudioMediaSelectionOptions", @"hasLegibleMediaSelectionOptions", nil];
}

- (BOOL)hasAudioMediaSelectionOptions
{
    return [[self audioMediaSelectionOptions] count] > 1;
}

+ (NSSet *)keyPathsForValuesAffectingHasAudioMediaSelectionOptions
{
    return [NSSet setWithObject:@"audioMediaSelectionOptions"];
}

- (BOOL)hasLegibleMediaSelectionOptions
{
    const NSUInteger numDefaultLegibleOptions = 2;
    return [[self legibleMediaSelectionOptions] count] > numDefaultLegibleOptions;
}

+ (NSSet *)keyPathsForValuesAffectingHasLegibleMediaSelectionOptions
{
    return [NSSet setWithObject:@"legibleMediaSelectionOptions"];
}

- (WebAVMediaSelectionOption *)currentAudioMediaSelectionOption
{
    return _currentAudioMediaSelectionOption;
}

- (void)setCurrentAudioMediaSelectionOption:(WebAVMediaSelectionOption *)option
{
    if (option == _currentAudioMediaSelectionOption)
        return;

    [_currentAudioMediaSelectionOption release];
    _currentAudioMediaSelectionOption = [option retain];

    if (!self.delegate)
        return;

    NSInteger index = NSNotFound;

    if (option && self.audioMediaSelectionOptions)
        index = [self.audioMediaSelectionOptions indexOfObject:option];

    self.delegate->selectAudioMediaOption(index != NSNotFound ? index : UINT64_MAX);
}

- (WebAVMediaSelectionOption *)currentLegibleMediaSelectionOption
{
    return _currentLegibleMediaSelectionOption;
}

- (void)setCurrentLegibleMediaSelectionOption:(WebAVMediaSelectionOption *)option
{
    if (option == _currentLegibleMediaSelectionOption)
        return;

    [_currentLegibleMediaSelectionOption release];
    _currentLegibleMediaSelectionOption = [option retain];

    if (!self.delegate)
        return;

    NSInteger index = NSNotFound;

    if (option && self.legibleMediaSelectionOptions)
        index = [self.legibleMediaSelectionOptions indexOfObject:option];

    self.delegate->selectLegibleMediaOption(index != NSNotFound ? index : UINT64_MAX);
}

- (BOOL)isPlayingOnExternalScreen
{
    return [self isExternalPlaybackActive] || [self isPlayingOnSecondScreen];
}

+ (NSSet *)keyPathsForValuesAffectingPlayingOnExternalScreen
{
    return [NSSet setWithObjects:@"externalPlaybackActive", @"playingOnSecondScreen", nil];
}

- (BOOL)isPictureInPictureInterrupted
{
    return _pictureInPictureInterrupted;
}

- (void)setPictureInPictureInterrupted:(BOOL)pictureInPictureInterrupted
{
    if (_pictureInPictureInterrupted != pictureInPictureInterrupted) {
        _pictureInPictureInterrupted = pictureInPictureInterrupted;
        if (pictureInPictureInterrupted)
            [self setPlaying:NO];
    }
}
@end

@implementation WebAVMediaSelectionOption
@end

#endif // PLATFORM(IOS)

