/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "WKAVContentSource.h"

#if HAVE(AVEXPERIENCECONTROLLER)

#import <WebCore/PlaybackSessionModel.h>
#import <wtf/RetainPtr.h>

#import <pal/cf/CoreMediaSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPlaybackUserInterfaceContentMetadata)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPlaybackUserInterfaceTimelineSegment)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVPlaybackUserInterfacePlaybackPosition)

NS_ASSUME_NONNULL_BEGIN

@implementation WKAVContentSource {
    WeakPtr<WebCore::PlaybackSessionModel> _model;

    CMTimeRange _timeRange;
    RetainPtr<AVPlaybackUserInterfacePlaybackPosition> _playbackPosition;
    RetainPtr<NSArray<AVPlaybackUserInterfaceTimelineSegment *>> _segments;
    NSUInteger _currentSegmentIndex;
    RetainPtr<NSArray<NSValue *>> _seekableTimeRanges;
    BOOL _ready;
    BOOL _playing;
    BOOL _buffering;
    float _playbackSpeed;
    float _scanSpeed;
    AVPlaybackUserInterfacePlaybackState _state;
    AVPlaybackUserInterfaceSeekCapabilities _supportedSeekCapabilities;
    BOOL _containsLiveStreamingContent;
    RetainPtr<NSError> _error;
    float _defaultPlaybackSpeed;
    NSUInteger _currentAudioOptionIndex;
    NSUInteger _currentAudioDescriptionOptionIndex;
    NSUInteger _currentLegibleOptionIndex;
    RetainPtr<NSArray<AVPlaybackUserInterfaceMediaSelectionOption *>> _audioOptions;
    RetainPtr<NSArray<AVPlaybackUserInterfaceMediaSelectionOption *>> _audioDescriptionOptions;
    RetainPtr<NSArray<AVPlaybackUserInterfaceMediaSelectionOption *>> _legibleOptions;
    BOOL _hasAudio;
    BOOL _muted;
    float _volume;
    RetainPtr<AVPlaybackUserInterfaceContentMetadata> _metadata;
    RetainPtr<CALayer> _videoLayer;
    CGSize _videoSize;
    RetainPtr<CALayer> _captionLayer;
}

static RetainPtr<AVPlaybackUserInterfaceTimelineSegment> emptyTimelineSegment()
{
    using namespace PAL;

    return adoptNS([allocAVPlaybackUserInterfaceTimelineSegmentInstance() initWithTimeRange:kCMTimeRangeZero segmentType:AVPlaybackUserInterfaceTimelineSegmentTypePrimary marked:NO requiresLinearPlayback:NO identifier:nil]);
}

static RetainPtr<AVPlaybackUserInterfacePlaybackPosition> playbackPosition(CMTime position, CMTime hostTime, float rate)
{
    using namespace PAL;

    return adoptNS([allocAVPlaybackUserInterfacePlaybackPositionInstance() initWithPosition:position hostTime:hostTime rate:rate]);
}

- (instancetype)initWithModel:(WebCore::PlaybackSessionModel&)model
{
    using namespace PAL;

    self = [super init];
    if (!self)
        return nil;

    _model = model;

    _timeRange = kCMTimeRangeZero;
    _playbackPosition = playbackPosition(kCMTimeZero, CMClockGetTime(CMClockGetHostTimeClock()), 0);
    _segments = [NSArray arrayWithObject:emptyTimelineSegment().get()];
    _currentAudioOptionIndex = NSNotFound;
    _currentAudioDescriptionOptionIndex = NSNotFound;
    _currentLegibleOptionIndex = NSNotFound;
    _audioOptions = [NSArray array];
    _audioDescriptionOptions = [NSArray array];
    _legibleOptions = [NSArray array];
    _metadata = createPlatformMetadata(nil, nil);
    _videoSize = CGSizeZero;

    return self;
}

- (void)setTimeRange:(CMTimeRange)timeRange
{
    _timeRange = timeRange;
}

- (void)setSeekableTimeRanges:(NSArray<NSValue *> * _Nullable)seekableTimeRanges
{
    _seekableTimeRanges = adoptNS([seekableTimeRanges copy]);
}

- (void)setReady:(BOOL)ready
{
    _ready = ready;
}

- (void)setBuffering:(BOOL)buffering
{
    _buffering = buffering;
}

- (void)setSupportedSeekCapabilities:(AVPlaybackUserInterfaceSeekCapabilities)supportedSeekCapabilities
{
    _supportedSeekCapabilities = supportedSeekCapabilities;
}

- (void)setCurrentAudioOptionIndex:(NSUInteger)currentAudioOptionIndex
{
    [self willChangeValueForKey:@"currentAudioOption"];
    _currentAudioOptionIndex = currentAudioOptionIndex;
    [self didChangeValueForKey:@"currentAudioOption"];
}

- (void)setCurrentLegibleOptionIndex:(NSUInteger)currentLegibleOptionIndex
{
    [self willChangeValueForKey:@"currentLegibleOption"];
    _currentLegibleOptionIndex = currentLegibleOptionIndex;
    [self didChangeValueForKey:@"currentLegibleOption"];
}

- (void)setAudioOptions:(NSArray<AVPlaybackUserInterfaceMediaSelectionOption *> *)audioOptions
{
    _audioOptions = adoptNS([audioOptions copy]);
}

- (void)setLegibleOptions:(NSArray<AVPlaybackUserInterfaceMediaSelectionOption *> *)legibleOptions
{
    _legibleOptions = adoptNS([legibleOptions copy]);
}

- (void)setHasAudio:(BOOL)hasAudio
{
    _hasAudio = hasAudio;
}

- (void)setMetadata:(AVPlaybackUserInterfaceContentMetadata *)metadata
{
    _metadata = metadata;
}

- (void)setVideoLayer:(CALayer * _Nullable)videoLayer
{
    _videoLayer = videoLayer;
}

- (void)setVideoSize:(CGSize)videoSize
{
    _videoSize = videoSize;
}

- (void)setCaptionLayer:(CALayer * _Nullable)captionLayer
{
    _captionLayer = captionLayer;
}

- (void)setPlaybackPositionInternal:(CMTime)position hostTime:(CMTime)hostTime
{
    [self willChangeValueForKey:@"playbackPosition"];
    _playbackPosition = playbackPosition(position, hostTime, _playing ? _playbackSpeed : 0);
    [self didChangeValueForKey:@"playbackPosition"];
}

- (void)setPlayingInternal:(BOOL)playing
{
    [self willChangeValueForKey:@"playing"];
    _playing = playing;
    [self didChangeValueForKey:@"playing"];
}

- (void)setPlaybackSpeedInternal:(float)playbackSpeed
{
    [self willChangeValueForKey:@"playbackSpeed"];
    _playbackSpeed = playbackSpeed;
    [self didChangeValueForKey:@"playbackSpeed"];
}

- (void)setMutedInternal:(BOOL)muted
{
    [self willChangeValueForKey:@"muted"];
    _muted = muted;
    [self didChangeValueForKey:@"muted"];
}

- (void)setVolumeInternal:(float)volume
{
    [self willChangeValueForKey:@"volume"];
    _volume = volume;
    [self didChangeValueForKey:@"volume"];
}

#pragma mark - AVPlaybackUserInterfaceVideoControllable conformance

- (CMTimeRange)timeRange
{
    return _timeRange;
}

- (AVPlaybackUserInterfacePlaybackPosition *)playbackPosition
{
    return _playbackPosition.get();
}

- (void)seekToPosition:(CMTime)position tolerance:(CMTime)tolerance
{
    if (CheckedPtr model = _model.get()) {
        double toleranceSeconds = PAL::CMTimeGetSeconds(tolerance);
        model->seekToTime(PAL::CMTimeGetSeconds(position), toleranceSeconds, toleranceSeconds);
    }
}

- (NSArray<AVPlaybackUserInterfaceTimelineSegment *> *)segments
{
    return _segments.get();
}

- (AVPlaybackUserInterfaceTimelineSegment *)currentSegment
{
    return [_segments objectAtIndex:_currentSegmentIndex];
}

- (NSArray<NSValue *> * _Nullable)seekableTimeRanges
{
    return _seekableTimeRanges.get();
}

- (BOOL)isReady
{
    return _ready;
}

- (BOOL)isPlaying
{
    return _playing;
}

- (void)setPlaying:(BOOL)playing
{
    CheckedPtr model = _model.get();
    if (!model)
        return;

    if (playing)
        model->play();
    else
        model->pause();
}

- (BOOL)isBuffering
{
    return _buffering;
}

- (float)playbackSpeed
{
    return _playbackSpeed;
}

- (void)setPlaybackSpeed:(float)playbackSpeed
{
    if (CheckedPtr model = _model.get())
        model->setPlaybackRate(playbackSpeed);
}

- (float)scanSpeed
{
    return _scanSpeed;
}

- (void)setScanSpeed:(float)scanSpeed
{
    _scanSpeed = scanSpeed;
}

- (AVPlaybackUserInterfacePlaybackState)state
{
    return _state;
}

- (void)setState:(AVPlaybackUserInterfacePlaybackState)state
{
    _state = state;
}

- (AVPlaybackUserInterfaceSeekCapabilities)supportedSeekCapabilities
{
    return _supportedSeekCapabilities;
}

- (BOOL)containsLiveStreamingContent
{
    return _containsLiveStreamingContent;
}

- (NSError * _Nullable)error
{
    return _error;
}

- (float)defaultPlaybackSpeed
{
    return _defaultPlaybackSpeed;
}

- (void)setDefaultPlaybackSpeed:(float)defaultPlaybackSpeed
{
    _defaultPlaybackSpeed = defaultPlaybackSpeed;
}

- (AVPlaybackUserInterfaceMediaSelectionOption * _Nullable)currentAudioOption
{
    if (_currentAudioOptionIndex == NSNotFound)
        return nil;

    return [_audioOptions objectAtIndex:_currentAudioOptionIndex];
}

- (void)setCurrentAudioOption:(AVPlaybackUserInterfaceMediaSelectionOption * _Nullable)currentAudioOption
{
    CheckedPtr model = _model.get();
    if (!model)
        return;

    if (!currentAudioOption) {
        model->selectAudioMediaOption(0);
        return;
    }

    NSUInteger index = [_audioOptions indexOfObjectPassingTest:^BOOL(AVPlaybackUserInterfaceMediaSelectionOption *option, NSUInteger, BOOL*) {
        if (option == currentAudioOption)
            return YES;
        return [option.identifier isEqualToString:currentAudioOption.identifier];
    }];

    if (index != NSNotFound)
        model->selectAudioMediaOption(index);
}

- (AVPlaybackUserInterfaceMediaSelectionOption * _Nullable)currentAudioDescriptionOption
{
    if (_currentAudioDescriptionOptionIndex == NSNotFound)
        return nil;

    return [_audioDescriptionOptions objectAtIndex:_currentAudioDescriptionOptionIndex];
}

- (void)setCurrentAudioDescriptionOption:(AVPlaybackUserInterfaceMediaSelectionOption * _Nullable)currentAudioDescriptionOption
{
    if (!currentAudioDescriptionOption) {
        _currentAudioDescriptionOptionIndex = NSNotFound;
        return;
    }

    _currentAudioDescriptionOptionIndex = [_audioDescriptionOptions indexOfObjectPassingTest:^BOOL(AVPlaybackUserInterfaceMediaSelectionOption *option, NSUInteger, BOOL*) {
        if (option == currentAudioDescriptionOption)
            return YES;
        return [option.identifier isEqualToString:currentAudioDescriptionOption.identifier];
    }];
}

- (AVPlaybackUserInterfaceMediaSelectionOption * _Nullable)currentLegibleOption
{
    if (_currentLegibleOptionIndex == NSNotFound)
        return nil;

    return [_legibleOptions objectAtIndex:_currentLegibleOptionIndex];
}

- (void)setCurrentLegibleOption:(AVPlaybackUserInterfaceMediaSelectionOption * _Nullable)currentLegibleOption
{
    CheckedPtr model = _model.get();
    if (!model)
        return;

    if (!currentLegibleOption) {
        model->selectLegibleMediaOption(0);
        return;
    }

    NSUInteger index = [_legibleOptions indexOfObjectPassingTest:^BOOL(AVPlaybackUserInterfaceMediaSelectionOption *option, NSUInteger, BOOL*) {
        if (option == currentLegibleOption)
            return YES;
        return [option.identifier isEqualToString:currentLegibleOption.identifier];
    }];

    if (index != NSNotFound)
        model->selectLegibleMediaOption(index);
}

- (NSArray<AVPlaybackUserInterfaceMediaSelectionOption *> *)audioOptions
{
    return _audioOptions.get();
}

- (NSArray<AVPlaybackUserInterfaceMediaSelectionOption *> *)audioDescriptionOptions
{
    return _audioDescriptionOptions.get();
}

- (NSArray<AVPlaybackUserInterfaceMediaSelectionOption *> *)legibleOptions
{
    return _legibleOptions.get();
}

- (BOOL)hasAudio
{
    return _hasAudio;
}

- (BOOL)isMuted
{
    return _muted;
}

- (void)setMuted:(BOOL)muted
{
    if (CheckedPtr model = _model.get())
        model->setMuted(muted);
}

- (float)volume
{
    return _volume;
}

- (void)setVolume:(float)volume
{
    if (CheckedPtr model = _model.get())
        model->setVolume(volume);
}

- (AVPlaybackUserInterfaceContentMetadata *)metadata
{
    return _metadata;
}

- (CALayer * _Nullable)videoLayer
{
    return _videoLayer;
}

- (CGSize)videoSize
{
    return _videoSize;
}

- (CALayer * _Nullable)captionLayer
{
    return _captionLayer;
}

@end

RetainPtr<AVPlaybackUserInterfaceContentMetadata> createPlatformMetadata(NSString * _Nullable title, NSString * _Nullable subtitle)
{
    using namespace PAL;
    return adoptNS([allocAVPlaybackUserInterfaceContentMetadataInstance() initWithVideoProperties:nil title:title subtitle:subtitle artworkRepresentations:[NSArray array]]);
}

NS_ASSUME_NONNULL_END

#endif // HAVE(AVEXPERIENCECONTROLLER)
