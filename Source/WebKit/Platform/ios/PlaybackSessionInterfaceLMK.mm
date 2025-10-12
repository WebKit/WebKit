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

#import "VideoPresentationInterfaceLMK.h"
#import "WKSLinearMediaPlayer.h"
#import "WKSLinearMediaTypes.h"
#import <WebCore/ExceptionOr.h>
#import <WebCore/MediaSelectionOption.h>
#import <WebCore/NowPlayingInfo.h>
#import <WebCore/PlaybackSessionModel.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SpatialVideoMetadata.h>
#import <WebCore/TimeRanges.h>
#import <WebCore/VideoProjectionMetadata.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakPtr.h>

#import "WebKitSwiftSoftLink.h"

@interface WKLinearMediaPlayerDelegate : NSObject <WKSLinearMediaPlayerDelegate>
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithModel:(WebCore::PlaybackSessionModel&)model;
@end

@implementation WKLinearMediaPlayerDelegate {
    WeakPtr<WebCore::PlaybackSessionModel> _model;
}

- (instancetype)initWithModel:(WebCore::PlaybackSessionModel&)model
{
    self = [super init];
    if (!self)
        return nil;

    _model = model;
    return self;
}

- (void)linearMediaPlayerPlay:(WKSLinearMediaPlayer *)player
{
    if (CheckedPtr model = _model.get())
        model->play();
}

- (void)linearMediaPlayerPause:(WKSLinearMediaPlayer *)player
{
    if (CheckedPtr model = _model.get())
        model->pause();
}

- (void)linearMediaPlayerTogglePlayback:(WKSLinearMediaPlayer *)player
{
    CheckedPtr model = _model.get();
    if (!model)
        return;

    if (model->isPlaying())
        model->pause();
    else
        model->play();
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setPlaybackRate:(double)playbackRate
{
    if (CheckedPtr model = _model.get())
        model->setPlaybackRate(playbackRate);
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player seekToTime:(NSTimeInterval)time
{
    if (auto model = _model.get())
        model->seekToTime(time);
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player seekByDelta:(NSTimeInterval)delta
{
    if (CheckedPtr model = _model.get())
        model->seekToTime(player.currentTime + delta);
}

- (NSTimeInterval)linearMediaPlayer:(WKSLinearMediaPlayer *)player seekToDestination:(NSTimeInterval)destination fromSource:(NSTimeInterval)source
{
    CheckedPtr model = _model.get();
    if (!model)
        return 0;

    model->seekToTime(destination);
    return destination;
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player seekThumbnailToTime:(NSTimeInterval)time
{
    // FIXME: The intent of this method is to seek the contents of LinearMediaPlayer's thumbnailLayer,
    // which LMPlayableViewController displays in a popover when scrubbing. Since we don't currently
    // provide a thumbnail layer, fast seek the main content instead.
    if (CheckedPtr model = _model.get())
        model->fastSeek(time);
}

- (void)linearMediaPlayerBeginScrubbing:(WKSLinearMediaPlayer *)player
{
    if (CheckedPtr model = _model.get())
        model->beginScrubbing();
}

- (void)linearMediaPlayerEndScrubbing:(WKSLinearMediaPlayer *)player
{
    if (CheckedPtr model = _model.get())
        model->endScrubbing();
}

- (void)linearMediaPlayerBeginScanningForward:(WKSLinearMediaPlayer *)player
{
    if (CheckedPtr model = _model.get())
        model->beginScanningForward();
}

- (void)linearMediaPlayerEndScanningForward:(WKSLinearMediaPlayer *)player
{
    if (CheckedPtr model = _model.get())
        model->endScanning();
}

- (void)linearMediaPlayerBeginScanningBackward:(WKSLinearMediaPlayer *)player
{
    if (CheckedPtr model = _model.get())
        model->beginScanningBackward();
}

- (void)linearMediaPlayerEndScanningBackward:(WKSLinearMediaPlayer *)player
{
    if (CheckedPtr model = _model.get())
        model->endScanning();
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setVolume:(double)volume
{
    if (CheckedPtr model = _model.get())
        model->setVolume(volume);
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setMuted:(BOOL)muted
{
    if (CheckedPtr model = _model.get())
        model->setMuted(muted);
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setAudioTrack:(WKSLinearMediaTrack * _Nullable)audioTrack
{
    CheckedPtr model = _model.get();
    if (!model)
        return;

    NSUInteger index = audioTrack ? [player.audioTracks indexOfObject:audioTrack] : 0;
    if (index == NSNotFound) {
        RetainPtr indexSet = [player.audioTracks indexesOfObjectsPassingTest:^(WKSLinearMediaTrack *track, NSUInteger, BOOL*) {
            return [track.localizedDisplayName isEqualToString:audioTrack.localizedDisplayName];
        }];
        ASSERT([indexSet count] < 2);
        index = [indexSet firstIndex];
        if (index == NSNotFound)
            return;
    }

    model->selectAudioMediaOption(index);
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setLegibleTrack:(WKSLinearMediaTrack * _Nullable)legibleTrack
{
    CheckedPtr model = _model.get();
    if (!model)
        return;

    NSUInteger index = legibleTrack ? [player.legibleTracks indexOfObject:legibleTrack] : 0;
    if (index == NSNotFound) {
        RetainPtr indexSet = [player.legibleTracks indexesOfObjectsPassingTest:^(WKSLinearMediaTrack *track, NSUInteger, BOOL*) {
            return [track.localizedDisplayName isEqualToString:legibleTrack.localizedDisplayName];
        }];
        ASSERT([indexSet count] < 2);
        index = [indexSet firstIndex];
        if (index == NSNotFound)
            return;
    }

    model->selectLegibleMediaOption(index);
}

- (void)linearMediaPlayerEnterFullscreen:(WKSLinearMediaPlayer *)player
{
    if (CheckedPtr model = _model.get())
        model->enterFullscreen();
}

- (void)linearMediaPlayerExitFullscreen:(WKSLinearMediaPlayer *)player
{
    CheckedPtr model = _model.get();
    if (!model)
        return;

    model->exitFullscreen();
    model->setVideoReceiverEndpoint(nullptr);
}

- (void)linearMediaPlayer:(WKSLinearMediaPlayer *)player setVideoReceiverEndpoint:(xpc_object_t)videoReceiverEndpoint
{
    if (CheckedPtr model = _model.get())
        model->setVideoReceiverEndpoint(videoReceiverEndpoint);
}

- (void)linearMediaPlayerClearVideoReceiverEndpoint:(WKSLinearMediaPlayer *)player
{
    if (CheckedPtr model = _model.get())
        model->setVideoReceiverEndpoint(nullptr);
}

- (uint64_t)linearMediaPlayerLogIdentifier:(WKSLinearMediaPlayer *)player
{
#if !RELEASE_LOG_DISABLED
    if (CheckedPtr model = _model.get())
        return model->logIdentifier();
#endif

    return 0;
}

@end

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaybackSessionInterfaceLMK);

Ref<PlaybackSessionInterfaceLMK> PlaybackSessionInterfaceLMK::create(WebCore::PlaybackSessionModel& model)
{
    Ref interface = adoptRef(*new PlaybackSessionInterfaceLMK(model));
    interface->initialize();
    return interface;
}

static WebCore::NowPlayingMetadataObserver nowPlayingMetadataObserver(PlaybackSessionInterfaceLMK& interface)
{
    return {
        [weakInterface = WeakPtr { interface }](auto& metadata) {
            if (RefPtr interface = weakInterface.get())
                interface->nowPlayingMetadataChanged(metadata);
        }
    };
}

PlaybackSessionInterfaceLMK::PlaybackSessionInterfaceLMK(WebCore::PlaybackSessionModel& model)
    : PlaybackSessionInterfaceIOS { model }
    , m_player { adoptNS([allocWKSLinearMediaPlayerInstance() init]) }
    , m_playerDelegate { adoptNS([[WKLinearMediaPlayerDelegate alloc] initWithModel:model]) }
    , m_nowPlayingMetadataObserver { nowPlayingMetadataObserver(*this) }
{
    [m_player setDelegate:m_playerDelegate.get()];
}

PlaybackSessionInterfaceLMK::~PlaybackSessionInterfaceLMK()
{
    invalidate();
}

WKSLinearMediaPlayer *PlaybackSessionInterfaceLMK::linearMediaPlayer() const
{
    return m_player.get();
}

void PlaybackSessionInterfaceLMK::setPlayerIdentifier(std::optional<WebCore::MediaPlayerIdentifier> identifier)
{
    PlaybackSessionInterfaceIOS::setPlayerIdentifier(identifier);

    if (RefPtr videoPresentationInterface = m_videoPresentationInterface.get())
        videoPresentationInterface->didSetPlayerIdentifier();
}

void PlaybackSessionInterfaceLMK::durationChanged(double duration)
{
    [m_player setStartTime:0];
    [m_player setEndTime:duration];
    [m_player setDuration:duration];
    [m_player setCanTogglePlayback:YES];
}

void PlaybackSessionInterfaceLMK::currentTimeChanged(double currentTime, double)
{
    [m_player setCurrentTime:currentTime];
    [m_player setRemainingTime:std::max([m_player duration] - currentTime, 0.0)];
}

void PlaybackSessionInterfaceLMK::rateChanged(OptionSet<WebCore::PlaybackSessionModel::PlaybackState> playbackState, double playbackRate, double)
{
    [m_player setSelectedPlaybackRate:playbackRate];
    [m_player setIsLoading:playbackState.contains(WebCore::PlaybackSessionModel::PlaybackState::Stalled)];
    if (!playbackState.contains(WebCore::PlaybackSessionModel::PlaybackState::Stalled))
        [m_player setPlaybackRate:playbackState.contains(WebCore::PlaybackSessionModel::PlaybackState::Playing) ? playbackRate : 0];
}

void PlaybackSessionInterfaceLMK::seekableRangesChanged(const WebCore::PlatformTimeRanges& timeRanges, double, double)
{
    RetainPtr seekableRanges = adoptNS([[NSMutableArray alloc] initWithCapacity:timeRanges.length()]);
    for (unsigned i = 0; i < timeRanges.length(); ++i) {
        RetainPtr timeRange = adoptNS([allocWKSLinearMediaTimeRangeInstance() initWithLowerBound:timeRanges.start(i).toDouble() upperBound:timeRanges.end(i).toDouble()]);
        [seekableRanges addObject:timeRange.get()];
    }

    [m_player setSeekableTimeRanges:seekableRanges.get()];
    [m_player setCanSeek:!![seekableRanges count]];
}

void PlaybackSessionInterfaceLMK::canPlayFastReverseChanged(bool canPlayFastReverse)
{
    [m_player setCanScanBackward:canPlayFastReverse];
}

void PlaybackSessionInterfaceLMK::audioMediaSelectionOptionsChanged(const Vector<WebCore::MediaSelectionOption>& options, uint64_t selectedIndex)
{
    RetainPtr audioTracks = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& option : options) {
        RetainPtr audioTrack = adoptNS([allocWKSLinearMediaTrackInstance() initWithLocalizedDisplayName:option.displayName.createNSString().get()]);
        [audioTracks addObject:audioTrack.get()];
    }

    [m_player setAudioTracks:audioTracks.get()];
    audioMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionInterfaceLMK::legibleMediaSelectionOptionsChanged(const Vector<WebCore::MediaSelectionOption>& options, uint64_t selectedIndex)
{
    RetainPtr legibleTracks = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& option : options) {
        RetainPtr legibleTrack = adoptNS([allocWKSLinearMediaTrackInstance() initWithLocalizedDisplayName:option.displayName.createNSString().get()]);
        [legibleTracks addObject:legibleTrack.get()];
    }

    [m_player setLegibleTracks:legibleTracks.get()];
    legibleMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionInterfaceLMK::audioMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    NSArray *audioTracks = [m_player audioTracks];
    if (selectedIndex < audioTracks.count)
        [m_player setCurrentAudioTrack:audioTracks[selectedIndex]];
}

void PlaybackSessionInterfaceLMK::legibleMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    NSArray *legibleTracks = [m_player legibleTracks];
    if (selectedIndex < legibleTracks.count)
        [m_player setCurrentLegibleTrack:legibleTracks[selectedIndex]];
}

void PlaybackSessionInterfaceLMK::mutedChanged(bool muted)
{
    [m_player setIsMuted:muted];
}

void PlaybackSessionInterfaceLMK::volumeChanged(double volume)
{
    [m_player setVolume:volume];
}

void PlaybackSessionInterfaceLMK::supportsLinearMediaPlayerChanged(bool supportsLinearMediaPlayer)
{
    if (supportsLinearMediaPlayer)
        return;

    switch ([m_player presentationState]) {
    case WKSLinearMediaPresentationStateEnteringFullscreen:
    case WKSLinearMediaPresentationStateFullscreen:
        // If the player is in (or is entering) fullscreen but the current media engine does not
        // support LinearMediaPlayer, exit fullscreen.
        if (m_playbackSessionModel)
            m_playbackSessionModel->exitFullscreen();
        break;
    case WKSLinearMediaPresentationStateEnteringExternal:
    case WKSLinearMediaPresentationStateExternal:
        // If the player is in (or entering) external presentation (which uses LinearMediaPlayer) but the current
        // media engine does not support it, exit external presentation.
        if (RefPtr videoPresentationInterface = m_videoPresentationInterface.get())
            videoPresentationInterface->exitExternalPlayback();
        break;
    case WKSLinearMediaPresentationStateInline:
    case WKSLinearMediaPresentationStateExitingFullscreen:
        break;
    }

    ASSERT_NOT_REACHED();
}

void PlaybackSessionInterfaceLMK::spatialVideoMetadataChanged(const std::optional<WebCore::SpatialVideoMetadata>& metadata)
{
    RetainPtr<WKSLinearMediaSpatialVideoMetadata> spatialVideoMetadata;
    if (metadata)
        spatialVideoMetadata = adoptNS([allocWKSLinearMediaSpatialVideoMetadataInstance() initWithWidth:metadata->size.width() height:metadata->size.height() horizontalFOVDegrees:metadata->horizontalFOVDegrees baseline:metadata->baseline disparityAdjustment:metadata->disparityAdjustment]);
    [m_player setSpatialVideoMetadata:spatialVideoMetadata.get()];
}

void PlaybackSessionInterfaceLMK::videoProjectionMetadataChanged(const std::optional<WebCore::VideoProjectionMetadata>& metadata)
{
    [m_player setIsImmersiveVideo:!!metadata];
}

void PlaybackSessionInterfaceLMK::startObservingNowPlayingMetadata()
{
    if (m_playbackSessionModel)
        m_playbackSessionModel->addNowPlayingMetadataObserver(m_nowPlayingMetadataObserver);
}

void PlaybackSessionInterfaceLMK::stopObservingNowPlayingMetadata()
{
    if (m_playbackSessionModel)
        m_playbackSessionModel->removeNowPlayingMetadataObserver(m_nowPlayingMetadataObserver);
}

static RetainPtr<NSData> artworkData(const WebCore::NowPlayingMetadata& metadata)
{
    if (!metadata.artwork)
        return nil;

    RefPtr image = metadata.artwork->image;
    if (!image)
        return nil;

    RefPtr fragmentedData = image->data();
    if (!fragmentedData || fragmentedData->isEmpty())
        return nil;

    RetainPtr artworkData = adoptNS([[NSMutableData alloc] initWithCapacity:fragmentedData->size()]);
    fragmentedData->forEachSegment([&](auto segment) {
        [artworkData appendBytes:segment.data() length:segment.size()];
    });

    return adoptNS([artworkData copy]);
}

void PlaybackSessionInterfaceLMK::nowPlayingMetadataChanged(const WebCore::NowPlayingMetadata& metadata)
{
    RetainPtr contentMetadata = adoptNS([allocWKSLinearMediaContentMetadataInstance() initWithTitle:metadata.title.createNSString().get() subtitle:metadata.artist.createNSString().get()]);
    [m_player setContentMetadata:contentMetadata.get()];
    [m_player setArtwork:artworkData(metadata).get()];
}

void PlaybackSessionInterfaceLMK::swapFullscreenModesWith(PlaybackSessionInterfaceIOS& otherInterfaceIOS)
{
    auto& otherInterface = static_cast<PlaybackSessionInterfaceLMK&>(otherInterfaceIOS);
    std::swap(m_player, otherInterface.m_player);
    [m_player setDelegate:m_playerDelegate.get()];
    [otherInterface.m_player setDelegate:otherInterface.m_playerDelegate.get()];
}

#if !RELEASE_LOG_DISABLED
ASCIILiteral PlaybackSessionInterfaceLMK::logClassName() const
{
    return "PlaybackSessionInterfaceLMK"_s;
}
#endif

} // namespace WebKit

#endif // ENABLE(LINEAR_MEDIA_PLAYER)
