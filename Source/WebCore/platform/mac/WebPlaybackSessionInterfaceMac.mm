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
#include "WebPlaybackSessionInterfaceMac.h"

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#import "AVKitSPI.h"
#import "IntRect.h"
#import "MediaTimeAVFoundation.h"
#import "TimeRanges.h"
#import "WebPlaybackSessionModel.h"
#import <AVFoundation/AVFoundation.h>

#import "CoreMediaSoftLink.h"

SOFT_LINK_FRAMEWORK_OPTIONAL(AVKit)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVValueTiming)

using namespace WebCore;

@interface WebAVMediaSelectionOptionMac : NSObject {
    RetainPtr<NSString> _localizedDisplayName;
}
@property (retain) NSString *localizedDisplayName;
@end

@implementation WebAVMediaSelectionOptionMac

- (NSString *)localizedDisplayName
{
    return _localizedDisplayName.get();
}

- (void)setLocalizedDisplayName:(NSString *)name
{
    _localizedDisplayName = name;
}

@end

@interface WebPlaybackControlsManager : NSObject {
    NSTimeInterval _contentDuration;
    RetainPtr<AVValueTiming> _timing;
    NSTimeInterval _seekToTime;
    RetainPtr<NSArray> _seekableTimeRanges;
    BOOL _hasEnabledAudio;
    BOOL _hasEnabledVideo;
    RetainPtr<NSArray<AVMediaSelectionOption *>> _audioMediaSelectionOptions;
    RetainPtr<AVMediaSelectionOption> _currentAudioMediaSelectionOption;
    RetainPtr<NSArray<AVMediaSelectionOption *>> _legibleMediaSelectionOptions;
    RetainPtr<AVMediaSelectionOption> _currentLegibleMediaSelectionOption;

    float _rate;

@private
    WebCore::WebPlaybackSessionInterfaceMac* _webPlaybackSessionInterfaceMac;
}

@property (readwrite) NSTimeInterval contentDuration;
@property (nonatomic, retain, readwrite) AVValueTiming *timing;
@property NSTimeInterval seekToTime;
@property (nonatomic, retain, readwrite) NSArray *seekableTimeRanges;
@property (readwrite) BOOL hasEnabledAudio;
@property (readwrite) BOOL hasEnabledVideo;
@property (nonatomic, retain, readwrite) NSArray<AVMediaSelectionOption *> *audioMediaSelectionOptions;
@property (nonatomic, retain, readwrite) AVMediaSelectionOption *currentAudioMediaSelectionOption;
@property (nonatomic, retain, readwrite) NSArray<AVMediaSelectionOption *> *legibleMediaSelectionOptions;
@property (nonatomic, retain, readwrite) AVMediaSelectionOption *currentLegibleMediaSelectionOption;

@property (nonatomic) float rate;

- (instancetype)initWithWebPlaybackSessionInterfaceMac:(WebCore::WebPlaybackSessionInterfaceMac*)webPlaybackSessionInterfaceMac;

@end

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebPlaybackControlsControllerAdditions.mm>
#endif

@implementation WebPlaybackControlsManager

@synthesize contentDuration=_contentDuration;
@synthesize seekToTime=_seekToTime;
@synthesize hasEnabledAudio=_hasEnabledAudio;
@synthesize hasEnabledVideo=_hasEnabledVideo;
@synthesize rate=_rate;

- (instancetype)initWithWebPlaybackSessionInterfaceMac:(WebCore::WebPlaybackSessionInterfaceMac*)webPlaybackSessionInterfaceMac
{
    if (!(self = [super init]))
        return nil;

    _webPlaybackSessionInterfaceMac = webPlaybackSessionInterfaceMac;

    return self;
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

- (NSArray<AVMediaSelectionOption *> *)audioMediaSelectionOptions
{
    return _audioMediaSelectionOptions.get();
}

- (void)setAudioMediaSelectionOptions:(NSArray<AVMediaSelectionOption *> *)audioOptions
{
    _audioMediaSelectionOptions = audioOptions;
}

- (AVMediaSelectionOption *)currentAudioMediaSelectionOption
{
    return _currentAudioMediaSelectionOption.get();
}

- (void)setCurrentAudioMediaSelectionOption:(AVMediaSelectionOption *)audioMediaSelectionOption
{
    if (audioMediaSelectionOption == _currentAudioMediaSelectionOption)
        return;

    _currentAudioMediaSelectionOption = audioMediaSelectionOption;

    NSInteger index = NSNotFound;

    if (audioMediaSelectionOption && self.audioMediaSelectionOptions)
        index = [self.audioMediaSelectionOptions indexOfObject:audioMediaSelectionOption];

    _webPlaybackSessionInterfaceMac->webPlaybackSessionModel()->selectAudioMediaOption(index != NSNotFound ? index : UINT64_MAX);
}

- (NSArray<AVMediaSelectionOption *> *)legibleMediaSelectionOptions
{
    return _legibleMediaSelectionOptions.get();
}

- (void)setLegibleMediaSelectionOptions:(NSArray<AVMediaSelectionOption *> *)legibleOptions
{
    _legibleMediaSelectionOptions = legibleOptions;
}

- (AVMediaSelectionOption *)currentLegibleMediaSelectionOption
{
    return _currentLegibleMediaSelectionOption.get();
}

- (void)setCurrentLegibleMediaSelectionOption:(AVMediaSelectionOption *)legibleMediaSelectionOption
{
    if (legibleMediaSelectionOption == _currentLegibleMediaSelectionOption)
        return;

    _currentLegibleMediaSelectionOption = legibleMediaSelectionOption;

    NSInteger index = NSNotFound;

    if (legibleMediaSelectionOption && self.legibleMediaSelectionOptions)
        index = [self.legibleMediaSelectionOptions indexOfObject:legibleMediaSelectionOption];

    _webPlaybackSessionInterfaceMac->webPlaybackSessionModel()->selectLegibleMediaOption(index != NSNotFound ? index : UINT64_MAX);
}

- (void)cancelThumbnailAndAudioAmplitudeSampleGeneration
{
}

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebPlaybackControlsControllerThumbnailAdditions.mm>
#endif

@end

namespace WebCore {

WebPlaybackSessionInterfaceMac::~WebPlaybackSessionInterfaceMac()
{
}

void WebPlaybackSessionInterfaceMac::setWebPlaybackSessionModel(WebPlaybackSessionModel* model)
{
    m_playbackSessionModel = model;
}

void WebPlaybackSessionInterfaceMac::setDuration(double duration)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();

    controlsManager.contentDuration = duration;

    // FIXME: We take this as an indication that playback is ready, but that is not necessarily true.
    controlsManager.hasEnabledAudio = YES;
    controlsManager.hasEnabledVideo = YES;
}

void WebPlaybackSessionInterfaceMac::setCurrentTime(double currentTime, double anchorTime)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();

    NSTimeInterval anchorTimeStamp = ![controlsManager rate] ? NAN : anchorTime;
    AVValueTiming *timing = [getAVValueTimingClass() valueTimingWithAnchorValue:currentTime
        anchorTimeStamp:anchorTimeStamp rate:0];

    [controlsManager setTiming:timing];
}

void WebPlaybackSessionInterfaceMac::setRate(bool isPlaying, float playbackRate)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();

    [controlsManager setRate:isPlaying ? playbackRate : 0.];
}

void WebPlaybackSessionInterfaceMac::setSeekableRanges(const TimeRanges& timeRanges)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();

    RetainPtr<NSMutableArray> seekableRanges = adoptNS([[NSMutableArray alloc] init]);

    for (unsigned i = 0; i < timeRanges.length(); i++) {
        const PlatformTimeRanges& ranges = timeRanges.ranges();
        CMTimeRange range = CMTimeRangeMake(toCMTime(ranges.start(i)), toCMTime(ranges.end(i)));
        [seekableRanges addObject:[NSValue valueWithCMTimeRange:range]];
    }

    [controlsManager setSeekableTimeRanges:seekableRanges.get()];
}

static RetainPtr<NSMutableArray> mediaSelectionOptions(const Vector<String>& options)
{
    RetainPtr<NSMutableArray> webOptions = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& name : options) {
        RetainPtr<WebAVMediaSelectionOptionMac> webOption = adoptNS([[WebAVMediaSelectionOptionMac alloc] init]);
        [webOption setLocalizedDisplayName:name];
        [webOptions addObject:webOption.get()];
    }
    return webOptions;
}

void WebPlaybackSessionInterfaceMac::setAudioMediaSelectionOptions(const Vector<WTF::String>& options, uint64_t selectedIndex)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();

    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    [controlsManager setAudioMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [controlsManager setCurrentAudioMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
}

void WebPlaybackSessionInterfaceMac::setLegibleMediaSelectionOptions(const Vector<WTF::String>& options, uint64_t selectedIndex)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();

    RetainPtr<NSMutableArray> webOptions = mediaSelectionOptions(options);
    [controlsManager setLegibleMediaSelectionOptions:webOptions.get()];
    if (selectedIndex < [webOptions count])
        [controlsManager setCurrentLegibleMediaSelectionOption:[webOptions objectAtIndex:static_cast<NSUInteger>(selectedIndex)]];
}

void WebPlaybackSessionInterfaceMac::invalidate()
{
}

void WebPlaybackSessionInterfaceMac::ensureControlsManager()
{
    playBackControlsManager();
}

WebPlaybackControlsManager *WebPlaybackSessionInterfaceMac::playBackControlsManager()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    if (!m_playbackControlsManager)
        m_playbackControlsManager = adoptNS([[WebPlaybackControlsManager alloc] initWithWebPlaybackSessionInterfaceMac:this]);
    return m_playbackControlsManager.get();
#else
    return nil;
#endif
}
    
}

#endif // PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
