/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "PlaybackSessionInterfaceLMK.h"

#if ENABLE(LINEAR_MEDIA_PLAYER)

#import <WebCore/MediaSelectionOption.h>
#import <WebCore/PlaybackSessionModel.h>
#import <WebCore/TimeRanges.h>
#import <WebKitSwift/WebKitSwift.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/WeakPtr.h>

#import "WebKitSwiftSoftLink.h"

@interface WKLinearMediaPlayerDelegate : NSObject <WKSLinearMediaPlayerDelegate>
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithModel:(WebKit::PlaybackSessionModel&)model;
@end

@implementation WKLinearMediaPlayerDelegate {
    WeakPtr<WebKit::PlaybackSessionModel> _model;
}

- (instancetype)initWithModel:(WebKit::PlaybackSessionModel&)model
{
    self = [super init];
    if (!self)
        return nil;

    _model = model;
    return self;
}

- (void)linearMediaPlayerPlay:(WKSLinearMediaPlayer *)player
{
    if (auto model = _model.get())
        model->play();
}

- (void)linearMediaPlayerPause:(WKSLinearMediaPlayer *)player
{
    if (auto model = _model.get())
        model->pause();
}

- (void)linearMediaPlayerTogglePlayback:(WKSLinearMediaPlayer *)player
{
    auto model = _model.get();
    if (!model)
        return;

    if (model->isPlaying())
        model->pause();
    else
        model->play();
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setPlaybackRate:(double)playbackRate
{
    if (auto model = _model.get())
        model->setPlaybackRate(playbackRate);
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player seekToTime:(NSTimeInterval)time
{
    if (auto model = _model.get())
        model->seekToTime(time);
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player seekByDelta:(NSTimeInterval)delta
{
    if (auto model = _model.get())
        model->seekToTime(player.currentTime + delta);
}

- (NSTimeInterval)linearMediaPlayer:(WKSLinearMediaPlayer *)player seekToDestination:(NSTimeInterval)destination fromSource:(NSTimeInterval)source
{
    auto model = _model.get();
    if (!model)
        return 0;

    model->seekToTime(destination);
    return destination;
}

- (void)linearMediaPlayerBeginScrubbing:(WKSLinearMediaPlayer *)player
{
    if (auto model = _model.get())
        model->beginScrubbing();
}

- (void)linearMediaPlayerEndScrubbing:(WKSLinearMediaPlayer *)player
{
    if (auto model = _model.get())
        model->endScrubbing();
}

- (void)linearMediaPlayerBeginScanningForward:(WKSLinearMediaPlayer *)player
{
    if (auto model = _model.get())
        model->beginScanningForward();
}

- (void)linearMediaPlayerEndScanningForward:(WKSLinearMediaPlayer *)player
{
    if (auto model = _model.get())
        model->endScanning();
}

- (void)linearMediaPlayerBeginScanningBackward:(WKSLinearMediaPlayer *)player
{
    if (auto model = _model.get())
        model->beginScanningBackward();
}

- (void)linearMediaPlayerEndScanningBackward:(WKSLinearMediaPlayer *)player
{
    if (auto model = _model.get())
        model->endScanning();
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setVolume:(double)volume
{
    if (auto model = _model.get())
        model->setVolume(volume);
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setMuted:(BOOL)muted
{
    if (auto model = _model.get())
        model->setMuted(muted);
}

- (void)linearMediaPlayerToggleInlineMode:(WKSLinearMediaPlayer *)player
{
    if (auto model = _model.get())
        model->toggleFullscreen();
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setVideoReceiverEndpoint:(xpc_object_t)videoReceiverEndpoint
{
    if (auto model = _model.get())
        model->setVideoReceiverEndpoint(videoReceiverEndpoint);
}

@end

namespace WebKit {

Ref<PlaybackSessionInterfaceLMK> PlaybackSessionInterfaceLMK::create(PlaybackSessionModel& model)
{
    Ref interface = adoptRef(*new PlaybackSessionInterfaceLMK(model));
    interface->initialize();
    return interface;
}

PlaybackSessionInterfaceLMK::PlaybackSessionInterfaceLMK(PlaybackSessionModel& model)
    : PlaybackSessionInterfaceIOS { model }
    , m_player { adoptNS([allocWKSLinearMediaPlayerInstance() init]) }
    , m_playerDelegate { adoptNS([[WKLinearMediaPlayerDelegate alloc] initWithModel:model]) }
{
}

PlaybackSessionInterfaceLMK::~PlaybackSessionInterfaceLMK()
{
    ASSERT(isUIThread());
    invalidate();
}

WKSLinearMediaPlayer *PlaybackSessionInterfaceLMK::linearMediaPlayer() const
{
    return m_player.get();
}

void PlaybackSessionInterfaceLMK::durationChanged(double duration)
{
    [m_player setDuration:duration];
}

void PlaybackSessionInterfaceLMK::currentTimeChanged(double currentTime, double)
{
    [m_player setCurrentTime:currentTime];
}

void PlaybackSessionInterfaceLMK::rateChanged(OptionSet<PlaybackSessionModel::PlaybackState> playbackState, double playbackRate, double)
{
    if (playbackState.contains(PlaybackSessionModel::PlaybackState::Stalled))
        return;

    [m_player setPlaybackRate:playbackState.contains(PlaybackSessionModel::PlaybackState::Playing) ? playbackRate : 0];
}

void PlaybackSessionInterfaceLMK::seekableRangesChanged(const TimeRanges& timeRanges, double, double)
{
    RetainPtr seekableRanges = adoptNS([[NSMutableArray alloc] initWithCapacity:timeRanges.length()]);
    for (unsigned i = 0; i < timeRanges.length(); ++i) {
        double lowerBound = timeRanges.start(i).releaseReturnValue();
        double upperBound = timeRanges.end(i).releaseReturnValue();
        RetainPtr timeRange = adoptNS([allocWKSLinearMediaTimeRangeInstance() initWithLowerBound:lowerBound upperBound:upperBound]);
        [seekableRanges addObject:timeRange.get()];
    }

    [m_player setSeekableTimeRanges:seekableRanges.get()];
    [m_player setCanSeek:!![seekableRanges count]];
}

void PlaybackSessionInterfaceLMK::canPlayFastReverseChanged(bool canPlayFastReverse)
{
    [m_player setCanScanBackward:canPlayFastReverse];
}

void PlaybackSessionInterfaceLMK::mutedChanged(bool muted)
{
    [m_player setIsMuted:muted];
}

void PlaybackSessionInterfaceLMK::volumeChanged(double volume)
{
    [m_player setVolume:volume];
}

#if !RELEASE_LOG_DISABLED
const char* PlaybackSessionInterfaceLMK::logClassName() const
{
    return "PlaybackSessionInterfaceLMK";
}
#endif

} // namespace WebKit

#endif // ENABLE(LINEAR_MEDIA_PLAYER)
