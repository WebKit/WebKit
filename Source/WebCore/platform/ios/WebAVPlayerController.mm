/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY) && HAVE(AVKIT)

#import "Logging.h"
#import "PlaybackSessionInterfaceAVKit.h"
#import "PlaybackSessionModel.h"
#import "TimeRanges.h"
#import <AVFoundation/AVTime.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#import <pal/cf/CoreMediaSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPlayerController)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVValueTiming)

using namespace WebCore;

static void * WebAVPlayerControllerSeekableTimeRangesObserverContext = &WebAVPlayerControllerSeekableTimeRangesObserverContext;
static void * WebAVPlayerControllerHasLiveStreamingContentObserverContext = &WebAVPlayerControllerHasLiveStreamingContentObserverContext;
static void * WebAVPlayerControllerIsPlayingOnSecondScreenObserverContext = &WebAVPlayerControllerIsPlayingOnSecondScreenObserverContext;

static double WebAVPlayerControllerLiveStreamSeekableTimeRangeDurationHysteresisDelta = 3.0; // Minimum delta of time required to change the duration of the seekable time range.
static double WebAVPlayerControllerLiveStreamMinimumTargetDuration = 1.0; // Minimum segment duration to be considered valid.
static double WebAVPlayerControllerLiveStreamSeekableTimeRangeMinimumDuration = 30.0;

@implementation WebAVPlayerController {
    BOOL _liveStreamEventModePossible;
    BOOL _isScrubbing;
}

- (instancetype)init
{
    if (!getAVPlayerControllerClass())
        return nil;

    if (!(self = [super init]))
        return self;

    initAVPlayerController();
    self.playerControllerProxy = [[allocAVPlayerControllerInstance() init] autorelease];
    _liveStreamEventModePossible = YES;

    [self addObserver:self forKeyPath:@"seekableTimeRanges" options:(NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew | NSKeyValueObservingOptionInitial) context:WebAVPlayerControllerSeekableTimeRangesObserverContext];
    [self addObserver:self forKeyPath:@"hasLiveStreamingContent" options:NSKeyValueObservingOptionInitial context:WebAVPlayerControllerHasLiveStreamingContentObserverContext];
    [self addObserver:self forKeyPath:@"playingOnSecondScreen" options:NSKeyValueObservingOptionNew context:WebAVPlayerControllerIsPlayingOnSecondScreenObserverContext];

    return self;
}

- (void)dealloc
{
    [self removeObserver:self forKeyPath:@"seekableTimeRanges" context:WebAVPlayerControllerSeekableTimeRangesObserverContext];
    [self removeObserver:self forKeyPath:@"hasLiveStreamingContent" context:WebAVPlayerControllerHasLiveStreamingContentObserverContext];
    [self removeObserver:self forKeyPath:@"playingOnSecondScreen" context:WebAVPlayerControllerIsPlayingOnSecondScreenObserverContext];

    [_playerControllerProxy release];
    [_loadedTimeRanges release];
    [_seekableTimeRanges release];
    [_timing release];
    [_audioMediaSelectionOptions release];
    [_legibleMediaSelectionOptions release];
    [_currentAudioMediaSelectionOption release];
    [_currentLegibleMediaSelectionOption release];
    [_externalPlaybackAirPlayDeviceLocalizedName release];
    [_minTiming release];
    [_maxTiming release];
    [super dealloc];
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
    if (self.delegate)
        self.delegate->play();
}

- (void)pause:(id)sender
{
    UNUSED_PARAM(sender);
    if (self.delegate)
        self.delegate->pause();
}

- (void)togglePlayback:(id)sender
{
    UNUSED_PARAM(sender);
    if (self.delegate)
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
    _isScrubbing = YES;
    if (self.delegate)
        self.delegate->beginScrubbing();
}

- (void)endScrubbing:(id)sender
{
    UNUSED_PARAM(sender);
    _isScrubbing = NO;
    if (self.delegate)
        self.delegate->endScrubbing();
}

- (void)seekToTime:(NSTimeInterval)time
{
    if (self.delegate)
        self.delegate->seekToTime(time);
}

- (void)seekToTime:(NSTimeInterval)time toleranceBefore:(NSTimeInterval)before toleranceAfter:(NSTimeInterval)after
{
    self.delegate->seekToTime(time, before, after);
}

- (void)seekByTimeInterval:(NSTimeInterval)interval
{
    [self seekByTimeInterval:interval toleranceBefore:0. toleranceAfter:0.];
}

- (void)seekByTimeInterval:(NSTimeInterval)interval toleranceBefore:(NSTimeInterval)before toleranceAfter:(NSTimeInterval)after
{
    NSTimeInterval targetTime = [[self timing] currentValue] + interval;
    [self seekToTime:targetTime toleranceBefore:before toleranceAfter:after];
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

- (NSTimeInterval)maxTime
{
    NSTimeInterval maxTime = NAN;

    NSTimeInterval duration = [self contentDuration];
    if (!isnan(duration) && !isinf(duration))
        maxTime = duration;
    else if ([self hasSeekableLiveStreamingContent] && [self maxTiming])
        maxTime = [[self maxTiming] currentValue];

    return maxTime;
}

+ (NSSet *)keyPathsForValuesAffectingMaxTime
{
    return [NSSet setWithObjects:@"contentDuration", @"hasSeekableLiveStreamingContent", @"maxTiming", nil];
}

- (NSTimeInterval)minTime
{
    NSTimeInterval minTime = 0.0;

    if ([self hasSeekableLiveStreamingContent] && [self minTiming])
        minTime = [[self minTiming] currentValue];

    return minTime;
}

+ (NSSet *)keyPathsForValuesAffectingMinTime
{
    return [NSSet setWithObjects:@"hasSeekableLiveStreamingContent", @"minTiming", nil];
}

- (void)skipBackwardThirtySeconds:(id)sender
{
    using namespace PAL;

    UNUSED_PARAM(sender);
    BOOL isTimeWithinSeekableTimeRanges = NO;
    CMTime currentTime = CMTimeMakeWithSeconds([[self timing] currentValue], 1000);
    CMTime thirtySecondsBeforeCurrentTime = CMTimeSubtract(currentTime, PAL::CMTimeMake(30, 1));

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
    using namespace PAL;

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

- (BOOL)isScrubbing
{
    return _isScrubbing;
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
    if (self.delegate)
        self.delegate->beginScanningForward();
}

- (void)endScanningForward:(id)sender
{
    UNUSED_PARAM(sender);
    if (self.delegate)
        self.delegate->endScanning();
}

- (void)beginScanningBackward:(id)sender
{
    UNUSED_PARAM(sender);
    if (self.delegate)
        self.delegate->beginScanningBackward();
}

- (void)endScanningBackward:(id)sender
{
    UNUSED_PARAM(sender);
    if (self.delegate)
        self.delegate->endScanning();
}

- (BOOL)canSeekToBeginning
{
    using namespace PAL;

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
    if (self.delegate)
        self.delegate->seekToTime(-INFINITY);
}

- (void)seekChapterBackward:(id)sender
{
    [self seekToBeginning:sender];
}

- (BOOL)canSeekToEnd
{
    using namespace PAL;

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
    if (self.delegate)
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

    if (index == NSNotFound)
        return;

    self.delegate->selectAudioMediaOption(index);
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

    if (index == NSNotFound)
        return;

    self.delegate->selectLegibleMediaOption(index);
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

- (BOOL)isMuted
{
    return _muted;
}

- (void)setMuted:(BOOL)muted
{
    if (_muted == muted)
        return;
    _muted = muted;

    if (self.delegate)
        self.delegate->setMuted(muted);
}

- (void)toggleMuted:(id)sender
{
    UNUSED_PARAM(sender);
    if (self.delegate)
        self.delegate->toggleMuted();
}

- (double)volume
{
    return self.delegate ? self.delegate->volume() : 0;
}

- (void)setVolume:(double)volume
{
    if (self.delegate)
        self.delegate->setVolume(volume);
}

- (void)volumeChanged:(double)volume
{
    UNUSED_PARAM(volume);
    [self willChangeValueForKey:@"volume"];
    [self didChangeValueForKey:@"volume"];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    using namespace PAL;

    UNUSED_PARAM(object);
    UNUSED_PARAM(keyPath);

    if (WebAVPlayerControllerSeekableTimeRangesObserverContext == context) {
        NSArray *oldArray = change[NSKeyValueChangeOldKey];
        NSArray *newArray = change[NSKeyValueChangeNewKey];
        if ([oldArray isKindOfClass:[NSArray class]] && [newArray isKindOfClass:[NSArray class]]) {
            CMTimeRange oldSeekableTimeRange = [oldArray count] > 0 ? [[oldArray firstObject] CMTimeRangeValue] : kCMTimeRangeInvalid;
            CMTimeRange newSeekableTimeRange = [newArray count] > 0 ? [[newArray firstObject] CMTimeRangeValue] : kCMTimeRangeInvalid;
            if (!CMTimeRangeEqual(oldSeekableTimeRange, newSeekableTimeRange)) {
                if (CMTIMERANGE_IS_VALID(newSeekableTimeRange) && CMTIMERANGE_IS_VALID(oldSeekableTimeRange) && _liveStreamEventModePossible && !CMTimeRangeContainsTime(newSeekableTimeRange, oldSeekableTimeRange.start))
                    _liveStreamEventModePossible = NO;

                [self updateMinMaxTiming];
            }
        }
    } else if (WebAVPlayerControllerHasLiveStreamingContentObserverContext == context)
        [self updateMinMaxTiming];
    else if (WebAVPlayerControllerIsPlayingOnSecondScreenObserverContext == context) {
        if (auto* delegate = self.delegate)
            delegate->setPlayingOnSecondScreen(_playingOnSecondScreen);
    }
}

- (void)updateMinMaxTiming
{
    using namespace PAL;

    AVValueTiming *newMinTiming = nil;
    AVValueTiming *newMaxTiming = nil;

    // For live streams value timings for the min and max times are needed.
    if ([self hasLiveStreamingContent] && ([[self seekableTimeRanges] count] > 0)) {
        CMTimeRange seekableTimeRange = [[[self seekableTimeRanges] firstObject] CMTimeRangeValue];
        if (CMTIMERANGE_IS_VALID(seekableTimeRange)) {
            NSTimeInterval oldMinTime = [[self minTiming] currentValue];
            NSTimeInterval oldMaxTime = [[self maxTiming] currentValue];
            NSTimeInterval newMinTime = CMTimeGetSeconds(seekableTimeRange.start);
            NSTimeInterval newMaxTime = CMTimeGetSeconds(seekableTimeRange.start) + CMTimeGetSeconds(seekableTimeRange.duration) + (CACurrentMediaTime() - [self seekableTimeRangesLastModifiedTime]);
            double newMinTimingRate = _liveStreamEventModePossible ? 0.0 : 1.0;
            BOOL minTimingNeedsUpdate = YES;
            BOOL maxTimingNeedsUpdate = YES;

            if (isfinite([self liveUpdateInterval]) && [self liveUpdateInterval] > WebAVPlayerControllerLiveStreamMinimumTargetDuration) {
                // Only update the timing if the new time differs by one segment duration plus the hysteresis delta.
                minTimingNeedsUpdate = isnan(oldMinTime) || (fabs(oldMinTime - newMinTime) > [self liveUpdateInterval] + WebAVPlayerControllerLiveStreamSeekableTimeRangeDurationHysteresisDelta) || ([[self minTiming] rate] != newMinTimingRate);
                maxTimingNeedsUpdate = isnan(oldMaxTime) || (fabs(oldMaxTime - newMaxTime) > [self liveUpdateInterval] + WebAVPlayerControllerLiveStreamSeekableTimeRangeDurationHysteresisDelta);
            }

            if (minTimingNeedsUpdate || maxTimingNeedsUpdate) {
                newMinTiming = [getAVValueTimingClass() valueTimingWithAnchorValue:newMinTime anchorTimeStamp:[getAVValueTimingClass() currentTimeStamp] rate:newMinTimingRate];
                newMaxTiming = [getAVValueTimingClass() valueTimingWithAnchorValue:newMaxTime anchorTimeStamp:[getAVValueTimingClass() currentTimeStamp] rate:1.0];
            } else {
                newMinTiming = [self minTiming];
                newMaxTiming = [self maxTiming];
            }
        }
    }

    if (!newMinTiming)
        newMinTiming = [getAVValueTimingClass() valueTimingWithAnchorValue:[self minTime] anchorTimeStamp:NAN rate:0.0];

    if (!newMaxTiming)
        newMaxTiming = [getAVValueTimingClass() valueTimingWithAnchorValue:[self maxTime] anchorTimeStamp:NAN rate:0.0];

    [self setMinTiming:newMinTiming];
    [self setMaxTiming:newMaxTiming];
}

- (BOOL)hasSeekableLiveStreamingContent
{
    BOOL hasSeekableLiveStreamingContent = NO;

    if ([self hasLiveStreamingContent] && [self minTiming] && [self maxTiming] && isfinite([self liveUpdateInterval]) && [self liveUpdateInterval] > WebAVPlayerControllerLiveStreamMinimumTargetDuration && ([self seekableTimeRangesLastModifiedTime] != 0.0)) {
        NSTimeInterval timeStamp = [getAVValueTimingClass() currentTimeStamp];
        NSTimeInterval minTime = [[self minTiming] valueForTimeStamp:timeStamp];
        NSTimeInterval maxTime = [[self maxTiming] valueForTimeStamp:timeStamp];
        hasSeekableLiveStreamingContent = ((maxTime - minTime) > WebAVPlayerControllerLiveStreamSeekableTimeRangeMinimumDuration);
    }

    return hasSeekableLiveStreamingContent;
}

+ (NSSet *)keyPathsForValuesAffectingHasSeekableLiveStreamingContent
{
    return [NSSet setWithObjects:@"hasLiveStreamingContent", @"minTiming", @"maxTiming", @"seekableTimeRangesLastModifiedTime", nil];
}

@end

@implementation WebAVMediaSelectionOption

- (void)dealloc
{
    [_localizedDisplayName release];
    [super dealloc];
}

@end

#endif // PLATFORM(IOS_FAMILY)

