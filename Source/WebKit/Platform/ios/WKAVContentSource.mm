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
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVInterfaceMetadata)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVInterfaceTimelineSegment)

NS_ASSUME_NONNULL_BEGIN

@interface AVInterfaceMetadata (Staging_169033633)
- (instancetype)initWithAudioOnly:(BOOL)audioOnly title:(nullable NSString *)title subtitle:(nullable NSString *)subtitle albumArtworkRepresentations:(NSArray<AVInterfaceAlbumArtwork *> *)albumArtworkRepresentations;
- (instancetype)initWithAudioOnly:(BOOL)audioOnly title:(nullable NSString *)title subtitle:(nullable NSString *)subtitle albumArtworks:(nullable NSArray<AVInterfaceAlbumArtwork *> *)albumArtworks;
@end

@implementation WKAVContentSource {
    WeakPtr<WebCore::PlaybackSessionModel> _model;

    CMTimeRange _timeRange;
    CMTime _currentPlaybackPosition;
    RetainPtr<NSArray<AVInterfaceTimelineSegment *>> _segments;
    NSUInteger _currentSegmentIndex;
    RetainPtr<NSArray<NSValue *>> _seekableTimeRanges;
    BOOL _ready;
    BOOL _playing;
    BOOL _buffering;
    float _playbackSpeed;
    float _scanSpeed;
    AVInterfacePlaybackState _state;
    AVInterfaceSeekCapabilities _supportedSeekCapabilities;
    BOOL _containsLiveStreamingContent;
    RetainPtr<NSError> _playbackError;
    float _defaultPlaybackSpeed;
    NSUInteger _currentAudioOptionIndex;
    NSUInteger _currentLegibleOptionIndex;
    RetainPtr<NSArray<AVInterfaceMediaSelectionOptionSource *>> _audioOptions;
    RetainPtr<NSArray<AVInterfaceMediaSelectionOptionSource *>> _legibleOptions;
    BOOL _hasAudio;
    BOOL _muted;
    float _volume;
    RetainPtr<AVInterfaceMetadata> _metadata;
    RetainPtr<CALayer> _videoLayer;
    CGSize _videoSize;
}

static RetainPtr<AVInterfaceTimelineSegment> emptyTimelineSegment()
{
    using namespace PAL;

    return adoptNS([allocAVInterfaceTimelineSegmentInstance() initWithTimeRange:kCMTimeRangeZero auxiliaryContent:NO marked:NO requiresLinearPlayback:NO identifier:nil]);
}

- (instancetype)initWithModel:(WebCore::PlaybackSessionModel&)model
{
    using namespace PAL;

    self = [super init];
    if (!self)
        return nil;

    _model = model;

    _timeRange = kCMTimeRangeZero;
    _currentPlaybackPosition = kCMTimeZero;
    _segments = [NSArray arrayWithObject:emptyTimelineSegment().get()];
    _currentAudioOptionIndex = NSNotFound;
    _currentLegibleOptionIndex = NSNotFound;
    _audioOptions = [NSArray array];
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

- (void)setSupportedSeekCapabilities:(AVInterfaceSeekCapabilities)supportedSeekCapabilities
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

- (void)setAudioOptions:(NSArray<AVInterfaceMediaSelectionOptionSource *> *)audioOptions
{
    _audioOptions = adoptNS([audioOptions copy]);
}

- (void)setLegibleOptions:(NSArray<AVInterfaceMediaSelectionOptionSource *> *)legibleOptions
{
    _legibleOptions = adoptNS([legibleOptions copy]);
}

- (void)setHasAudio:(BOOL)hasAudio
{
    _hasAudio = hasAudio;
}

- (void)setMetadata:(AVInterfaceMetadata *)metadata
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

- (void)setCurrentPlaybackPositionInternal:(CMTime)currentPlaybackPosition
{
    [self willChangeValueForKey:@"currentValue"];
    [self willChangeValueForKey:@"currentPlaybackPosition"];
    _currentPlaybackPosition = currentPlaybackPosition;
    [self didChangeValueForKey:@"currentValue"];
    [self didChangeValueForKey:@"currentPlaybackPosition"];
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

#pragma mark - AVInterfaceVideoPlaybackControllable conformance

- (CMTimeRange)timeRange
{
    return _timeRange;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
- (CMTime)currentValue
{
    return [self currentPlaybackPosition];
}

- (void)setCurrentValue:(CMTime)currentValue
{
    [self setCurrentPlaybackPosition:currentValue];
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (CMTime)currentPlaybackPosition
{
    return _currentPlaybackPosition;
}

- (void)setCurrentPlaybackPosition:(CMTime)currentPlaybackPosition
{
    if (CheckedPtr model = _model.get())
        model->seekToTime(PAL::CMTimeGetSeconds(currentPlaybackPosition));
}

- (NSArray<AVInterfaceTimelineSegment *> *)segments
{
    return _segments.get();
}

- (AVInterfaceTimelineSegment *)currentSegment
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

- (AVInterfacePlaybackState)state
{
    return _state;
}

- (void)setState:(AVInterfacePlaybackState)state
{
    _state = state;
}

- (AVInterfaceSeekCapabilities)supportedSeekCapabilities
{
    return _supportedSeekCapabilities;
}

- (BOOL)containsLiveStreamingContent
{
    return _containsLiveStreamingContent;
}

- (NSError * _Nullable)playbackError
{
    return _playbackError;
}

- (float)defaultPlaybackSpeed
{
    return _defaultPlaybackSpeed;
}

- (void)setDefaultPlaybackSpeed:(float)defaultPlaybackSpeed
{
    _defaultPlaybackSpeed = defaultPlaybackSpeed;
}

- (AVInterfaceMediaSelectionOptionSource * _Nullable)currentAudioOption
{
    if (_currentAudioOptionIndex == NSNotFound)
        return nil;

    return [_audioOptions objectAtIndex:_currentAudioOptionIndex];
}

- (void)setCurrentAudioOption:(AVInterfaceMediaSelectionOptionSource * _Nullable)currentAudioOption
{
    CheckedPtr model = _model.get();
    if (!model)
        return;

    if (!currentAudioOption) {
        model->selectAudioMediaOption(0);
        return;
    }

    NSUInteger index = [_audioOptions indexOfObjectPassingTest:^BOOL(AVInterfaceMediaSelectionOptionSource *option, NSUInteger, BOOL*) {
        if (option == currentAudioOption)
            return YES;
        return [option.identifier isEqualToString:currentAudioOption.identifier];
    }];

    if (index != NSNotFound)
        model->selectAudioMediaOption(index);
}

- (AVInterfaceMediaSelectionOptionSource * _Nullable)currentLegibleOption
{
    if (_currentLegibleOptionIndex == NSNotFound)
        return nil;

    return [_legibleOptions objectAtIndex:_currentLegibleOptionIndex];
}

- (void)setCurrentLegibleOption:(AVInterfaceMediaSelectionOptionSource * _Nullable)currentLegibleOption
{
    CheckedPtr model = _model.get();
    if (!model)
        return;

    if (!currentLegibleOption) {
        model->selectLegibleMediaOption(0);
        return;
    }

    NSUInteger index = [_legibleOptions indexOfObjectPassingTest:^BOOL(AVInterfaceMediaSelectionOptionSource *option, NSUInteger, BOOL*) {
        if (option == currentLegibleOption)
            return YES;
        return [option.identifier isEqualToString:currentLegibleOption.identifier];
    }];

    if (index != NSNotFound)
        model->selectLegibleMediaOption(index);
}

- (NSArray<AVInterfaceMediaSelectionOptionSource *> *)audioOptions
{
    return _audioOptions.get();
}

- (NSArray<AVInterfaceMediaSelectionOptionSource *> *)legibleOptions
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

- (AVInterfaceMetadata *)metadata
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

@end

RetainPtr<AVInterfaceMetadata> createPlatformMetadata(NSString * _Nullable title, NSString * _Nullable subtitle)
{
    using namespace PAL;

    if ([getAVInterfaceMetadataClassSingleton() respondsToSelector:@selector(initWithAudioOnly:title:subtitle:albumArtworkRepresentations:)])
        return adoptNS([allocAVInterfaceMetadataInstance() initWithAudioOnly:NO title:title subtitle:subtitle albumArtworkRepresentations:[NSArray array]]).get();

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return adoptNS([allocAVInterfaceMetadataInstance() initWithAudioOnly:NO title:title subtitle:subtitle albumArtworks:nil]).get();
ALLOW_DEPRECATED_DECLARATIONS_END
}

NS_ASSUME_NONNULL_END


#endif // HAVE(AVEXPERIENCECONTROLLER)
