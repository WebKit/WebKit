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
#import "WebVideoFullscreenInterfaceMac.h"

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#import "AVKitSPI.h"
#import "CoreMediaSoftLink.h"
#import "IntRect.h"
#import "MediaTimeAVFoundation.h"
#import "TimeRanges.h"
#import "WebVideoFullscreenChangeObserver.h"
#import "WebVideoFullscreenModel.h"
#import <AVFoundation/AVTime.h>

#import "SoftLinking.h"

SOFT_LINK_FRAMEWORK(AVKit)
SOFT_LINK_CLASS(AVKit, AVValueTiming)

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebVideoFullscreenInterfaceMacAdditions.mm>
#endif

using namespace WebCore;

@interface WebPlaybackControlsManager : NSObject {
    NSTimeInterval _contentDuration;
    AVValueTiming *_timing;
    NSTimeInterval _seekToTime;
    NSArray *_seekableTimeRanges;
    BOOL _hasEnabledAudio;
    BOOL _hasEnabledVideo;
    float _rate;

@private
    WebCore::WebVideoFullscreenInterfaceMac* _webVideoFullscreenInterfaceMac;
}

@property (readwrite) NSTimeInterval contentDuration;
@property (nonatomic, retain, readwrite) AVValueTiming *timing;
@property NSTimeInterval seekToTime;
@property (nonatomic, retain, readwrite) NSArray *seekableTimeRanges;
@property (readwrite) BOOL hasEnabledAudio;
@property (readwrite) BOOL hasEnabledVideo;

@property (nonatomic) float rate;

- (instancetype)initWithWebVideoFullscreenInterfaceMac:(WebCore::WebVideoFullscreenInterfaceMac*)webVideoFullscreenInterfaceMac;

@end

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebPlaybackControlsControllerAdditions.mm>
#endif

@implementation WebPlaybackControlsManager

@synthesize contentDuration = _contentDuration;
@synthesize timing = _timing;
@synthesize seekToTime = _seekToTime;
@synthesize seekableTimeRanges = _seekableTimeRanges;
@synthesize hasEnabledAudio = _hasEnabledAudio;
@synthesize hasEnabledVideo = _hasEnabledVideo;
@synthesize rate = _rate;

- (instancetype)initWithWebVideoFullscreenInterfaceMac:(WebCore::WebVideoFullscreenInterfaceMac*)webVideoFullscreenInterfaceMac
{
    if (!(self = [super init]))
        return nil;

    _webVideoFullscreenInterfaceMac = webVideoFullscreenInterfaceMac;

    return self;
}

- (BOOL)isSeeking
{
    return NO;
}

- (void)seekToTime:(NSTimeInterval)time toleranceBefore:(NSTimeInterval)toleranceBefore toleranceAfter:(NSTimeInterval)toleranceAfter
{
    UNUSED_PARAM(toleranceBefore);
    UNUSED_PARAM(toleranceAfter);
    _webVideoFullscreenInterfaceMac->webVideoFullscreenModel()->seekToTime(time);
}

- (NSArray *)audioMediaSelectionOptions
{
    return @[];
}

- (AVMediaSelectionOption *)currentAudioMediaSelectionOption
{
    return nil;
}

- (void)setCurrentAudioMediaSelectionOption:(AVMediaSelectionOption *)audioMediaSelectionOption
{
    UNUSED_PARAM(audioMediaSelectionOption);
}

- (NSArray *)legibleMediaSelectionOptions
{
    return @[];
}

- (AVMediaSelectionOption *)currentLegibleMediaSelectionOption
{
    return nil;
}

- (void)setCurrentLegibleMediaSelectionOption:(AVMediaSelectionOption *)legibleMediaSelectionOption
{
    UNUSED_PARAM(legibleMediaSelectionOption);
}

- (void)cancelThumbnailAndAudioAmplitudeSampleGeneration
{
}

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WebPlaybackControlsControllerThumbnailAdditions.mm>
#endif

@end

namespace WebCore {

WebVideoFullscreenInterfaceMac::~WebVideoFullscreenInterfaceMac()
{
}

void WebVideoFullscreenInterfaceMac::setWebVideoFullscreenModel(WebVideoFullscreenModel* model)
{
    m_videoFullscreenModel = model;
}

void WebVideoFullscreenInterfaceMac::setWebVideoFullscreenChangeObserver(WebVideoFullscreenChangeObserver* observer)
{
    m_fullscreenChangeObserver = observer;
}

void WebVideoFullscreenInterfaceMac::setMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    HTMLMediaElementEnums::VideoFullscreenMode newMode = m_mode | mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_mode);
}

void WebVideoFullscreenInterfaceMac::clearMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    HTMLMediaElementEnums::VideoFullscreenMode newMode = m_mode & ~mode;
    if (m_mode == newMode)
        return;

    m_mode = newMode;
    if (m_videoFullscreenModel)
        m_videoFullscreenModel->fullscreenModeChanged(m_mode);
}

void WebVideoFullscreenInterfaceMac::setDuration(double duration)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();

    controlsManager.contentDuration = duration;

    // FIXME: We take this as an indication that playback is ready, but that is not necessarily true.
    controlsManager.hasEnabledAudio = YES;
    controlsManager.hasEnabledVideo = YES;
}

void WebVideoFullscreenInterfaceMac::setCurrentTime(double currentTime, double anchorTime)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();

    NSTimeInterval anchorTimeStamp = ![controlsManager rate] ? NAN : anchorTime;
    AVValueTiming *timing = [getAVValueTimingClass() valueTimingWithAnchorValue:currentTime
        anchorTimeStamp:anchorTimeStamp rate:0];
    
    [controlsManager setTiming:timing];
}

void WebVideoFullscreenInterfaceMac::setRate(bool isPlaying, float playbackRate)
{
    WebPlaybackControlsManager* controlsManager = playBackControlsManager();

    [controlsManager setRate:isPlaying ? playbackRate : 0.];
}

void WebVideoFullscreenInterfaceMac::setSeekableRanges(const TimeRanges& timeRanges)
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

void WebVideoFullscreenInterfaceMac::ensureControlsManager()
{
    playBackControlsManager();
}

WebPlaybackControlsManager *WebVideoFullscreenInterfaceMac::playBackControlsManager()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    if (!m_playbackControlsManager)
        m_playbackControlsManager = adoptNS([[WebPlaybackControlsManager alloc] initWithWebVideoFullscreenInterfaceMac:this]);
    return m_playbackControlsManager.get();
#else
    return nil;
#endif
}

#if !USE(APPLE_INTERNAL_SDK)
void WebVideoFullscreenInterfaceMac::setupFullscreen(NSView&, const IntRect&, NSWindow *, HTMLMediaElementEnums::VideoFullscreenMode, bool)
{
}

void WebVideoFullscreenInterfaceMac::enterFullscreen()
{
}

void WebVideoFullscreenInterfaceMac::exitFullscreen(const IntRect&, NSWindow *)
{
}

void WebVideoFullscreenInterfaceMac::cleanupFullscreen()
{
}

void WebVideoFullscreenInterfaceMac::invalidate()
{
}

void WebVideoFullscreenInterfaceMac::preparedToReturnToInline(bool, const IntRect&, NSWindow *)
{
}

bool supportsPictureInPicture()
{
    return false;
}
#endif

}

#endif
