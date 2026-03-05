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

#if HAVE(AVEXPERIENCECONTROLLER)

#import <AVKit/AVInterfaceControllable_Private.h>
#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>
#import <wtf/Forward.h>

namespace WebCore {
class PlaybackSessionModel;
}

NS_ASSUME_NONNULL_BEGIN

@interface WKAVContentSource : NSObject <AVInterfaceVideoPlaybackControllable>
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithModel:(WebCore::PlaybackSessionModel&)model;

- (void)setCurrentAudioOptionIndex:(NSUInteger)currentAudioOptionIndex;
- (void)setCurrentLegibleOptionIndex:(NSUInteger)currentLegibleOptionIndex;
- (void)setCurrentPlaybackPositionInternal:(CMTime)currentPlaybackPosition;
- (void)setPlayingInternal:(BOOL)playing;
- (void)setPlaybackSpeedInternal:(float)playbackSpeed;
- (void)setMutedInternal:(BOOL)muted;
- (void)setVolumeInternal:(float)volume;

@property (nonatomic) CMTimeRange timeRange;
@property (nonatomic, copy, nullable) NSArray<NSValue *> *seekableTimeRanges;
@property (nonatomic, getter=isReady) BOOL ready;
@property (nonatomic, getter=isBuffering) BOOL buffering;
@property (nonatomic) AVInterfaceSeekCapabilities supportedSeekCapabilities;
@property (nonatomic, copy) NSArray<AVInterfaceMediaSelectionOptionSource *> *audioOptions;
@property (nonatomic, copy) NSArray<AVInterfaceMediaSelectionOptionSource *> *legibleOptions;
@property (nonatomic) BOOL hasAudio;
@property (nonatomic, strong) AVInterfaceMetadata *metadata;
@property (nonatomic, strong, nullable) CALayer *videoLayer;
@property (nonatomic) CGSize videoSize;
@end

RetainPtr<AVInterfaceMetadata> createPlatformMetadata(NSString * _Nullable title, NSString * _Nullable subtitle);

NS_ASSUME_NONNULL_END

#endif // HAVE(AVEXPERIENCECONTROLLER)
