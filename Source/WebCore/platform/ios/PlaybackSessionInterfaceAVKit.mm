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
#import "PlaybackSessionInterfaceAVKit.h"

#if HAVE(AVKIT_CONTENT_SOURCE)

#import "MediaSelectionOption.h"
#import "NowPlayingInfo.h"
#import "TimeRanges.h"
#import <pal/spi/cocoa/AVKitSPI.h>
#import <wtf/TZoneMallocInlines.h>

@interface WebAVListItem : NSObject <AVListable>
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithLocalizedTitle:(NSString *)localizedTitle;

@property (nonatomic, strong) NSString *localizedTitle;

@end

@implementation WebAVListItem

- (instancetype)initWithLocalizedTitle:(NSString *)localizedTitle
{
    self = [super init];
    if (!self)
        return nil;

    self.localizedTitle = localizedTitle;
    return self;
}

@end

@interface WebAVContentSource : NSObject <AVMediaSource>
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithModel:(WebCore::PlaybackSessionModel&)model;

@property (nonatomic, strong, nullable) NSArray<AVListable> *audioOptions;
@property (nonatomic) BOOL canScanBackward;
@property (nonatomic) BOOL canScanForward;
@property (nonatomic) BOOL canSeek;
@property (nonatomic) BOOL canTogglePlayback;
@property (nonatomic, strong, nullable) CALayer *captionLayer;
@property (nonatomic, strong, nullable) NSArray<AVListable> *captionOptions;
@property (nonatomic, strong, nullable) id<AVListable> currentAudioOption;
@property (nonatomic, strong, nullable) id<AVListable> currentCaptionOption;
@property (nonatomic) float currentValue;
#if PLATFORM(VISION)
@property (nonatomic, nullable) REEntityRef entityRef;
#endif
@property (nonatomic) BOOL hasAudio;
@property (nonatomic) BOOL hasLiveStreamContent;
@property (nonatomic) BOOL isLoading;
@property (nonatomic) BOOL isSeeking;
@property (nonatomic) float maxValue;
@property (nonatomic) float minValue;
@property (nonatomic) BOOL muted;
@property (nonatomic, strong, nullable) NSError *playbackError;
@property (nonatomic) double rate;
@property (nonatomic) BOOL requiresLinearPlayback;
@property (nonatomic, strong, nullable) NSArray<NSValue *> *seekableTimeRanges;
@property (nonatomic, strong, nullable) NSString *subtitle;
@property (nonatomic, strong, nullable) NSString *title;
@property (nonatomic, strong, nullable) CALayer *videoLayer;
@property (nonatomic) CGSize videoSize;
@property (nonatomic) double volume;

@end

@implementation WebAVContentSource {
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

- (void)beginScanningBackward
{
    if (auto model = _model.get())
        model->beginScanningBackward();
}

- (void)beginScanningForward
{
    if (auto model = _model.get())
        model->beginScanningForward();
}

- (void)endScanningBackward
{
    if (auto model = _model.get())
        model->endScanning();
}

- (void)endScanningForward
{
    if (auto model = _model.get())
        model->endScanning();
}

- (void)beginScrubbing
{
    if (auto model = _model.get())
        model->beginScrubbing();
}

- (void)endScrubbing
{
    if (auto model = _model.get())
        model->endScrubbing();
}

- (void)pause
{
    if (auto model = _model.get())
        model->pause();
}

- (void)play
{
    if (auto model = _model.get())
        model->play();
}

- (void)seekTo:(double)time
{
    if (auto model = _model.get())
        model->seekToTime(time);
}

- (void)setCaptionContentInsets:(UIEdgeInsets)insets
{
    // FIXME: Implement caption content insets
}

- (void)updateCurrentAudioOption:(nonnull id<AVListable>)currentAudioOption
{
    auto model = _model.get();
    if (!model)
        return;

    NSUInteger index = currentAudioOption ? [self.audioOptions indexOfObject:currentAudioOption] : 0;
    if (index != NSNotFound)
        model->selectAudioMediaOption(index);
}

- (void)updateCurrentCaptionOption:(nonnull id<AVListable>)currentCaptionOption
{
    auto model = _model.get();
    if (!model)
        return;

    NSUInteger index = currentCaptionOption ? [self.captionOptions indexOfObject:currentCaptionOption] : 0;
    if (index != NSNotFound)
        model->selectLegibleMediaOption(index);
}

- (void)updateMuted:(BOOL)muted
{
    if (auto model = _model.get())
        model->setMuted(muted);
}

- (void)updateVolume:(double)volume
{
    if (auto model = _model.get())
        model->setVolume(volume);
}

@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaybackSessionInterfaceAVKit);

Ref<PlaybackSessionInterfaceAVKit> PlaybackSessionInterfaceAVKit::create(PlaybackSessionModel& model)
{
    Ref interface = adoptRef(*new PlaybackSessionInterfaceAVKit(model));
    interface->initialize();
    return interface;
}

static NowPlayingMetadataObserver nowPlayingMetadataObserver(PlaybackSessionInterfaceAVKit& interface)
{
    return {
        [weakInterface = WeakPtr { interface }](auto& metadata) {
            if (RefPtr interface = weakInterface.get())
                interface->nowPlayingMetadataChanged(metadata);
        }
    };
}

PlaybackSessionInterfaceAVKit::PlaybackSessionInterfaceAVKit(PlaybackSessionModel& model)
    : PlaybackSessionInterfaceIOS { model }
    , m_contentSource { adoptNS([[WebAVContentSource alloc] initWithModel:model]) }
    , m_nowPlayingMetadataObserver { nowPlayingMetadataObserver(*this) }
{
}

PlaybackSessionInterfaceAVKit::~PlaybackSessionInterfaceAVKit()
{
    invalidate();
}

void PlaybackSessionInterfaceAVKit::durationChanged(double duration)
{
    // FIXME: Is setting a min value of 0 correct for, e.g., live streams?
    [m_contentSource setMinValue:0];
    [m_contentSource setMaxValue:duration];
    [m_contentSource setCanTogglePlayback:YES];
}

void PlaybackSessionInterfaceAVKit::currentTimeChanged(double currentTime, double)
{
    [m_contentSource setCurrentValue:currentTime];
}

void PlaybackSessionInterfaceAVKit::rateChanged(OptionSet<PlaybackSessionModel::PlaybackState> playbackState, double playbackRate, double)
{
    if (!playbackState.contains(PlaybackSessionModel::PlaybackState::Stalled))
        [m_contentSource setRate:playbackState.contains(PlaybackSessionModel::PlaybackState::Playing) ? playbackRate : 0];
}

void PlaybackSessionInterfaceAVKit::seekableRangesChanged(const TimeRanges& timeRanges, double, double)
{
    [m_contentSource setSeekableTimeRanges:makeNSArray(timeRanges.ranges()).get()];
}

void PlaybackSessionInterfaceAVKit::canPlayFastReverseChanged(bool canPlayFastReverse)
{
    [m_contentSource setCanScanBackward:canPlayFastReverse];
}

void PlaybackSessionInterfaceAVKit::audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    RetainPtr audioOptions = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& option : options) {
        RetainPtr audioOption = adoptNS([[WebAVListItem alloc] initWithLocalizedTitle:option.displayName]);
        [audioOptions addObject:audioOption.get()];
    }

    [m_contentSource setAudioOptions:(NSArray<AVListable> *)audioOptions.get()];
    audioMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionInterfaceAVKit::legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex)
{
    RetainPtr captionOptions = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& option : options) {
        RetainPtr captionOption = adoptNS([[WebAVListItem alloc] initWithLocalizedTitle:option.displayName]);
        [captionOptions addObject:captionOption.get()];
    }

    [m_contentSource setCaptionOptions:(NSArray<AVListable> *)captionOptions.get()];
    legibleMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionInterfaceAVKit::audioMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    NSArray *audioOptions = [m_contentSource audioOptions];
    if (selectedIndex < audioOptions.count)
        [m_contentSource setCurrentAudioOption:audioOptions[selectedIndex]];
}

void PlaybackSessionInterfaceAVKit::legibleMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    NSArray *captionOptions = [m_contentSource captionOptions];
    if (selectedIndex < captionOptions.count)
        [m_contentSource setCurrentCaptionOption:captionOptions[selectedIndex]];
}

void PlaybackSessionInterfaceAVKit::mutedChanged(bool muted)
{
    [m_contentSource setMuted:muted];
}

void PlaybackSessionInterfaceAVKit::volumeChanged(double volume)
{
    [m_contentSource setVolume:volume];
}

void PlaybackSessionInterfaceAVKit::startObservingNowPlayingMetadata()
{
    if (m_playbackSessionModel)
        m_playbackSessionModel->addNowPlayingMetadataObserver(m_nowPlayingMetadataObserver);
}

void PlaybackSessionInterfaceAVKit::stopObservingNowPlayingMetadata()
{
    if (m_playbackSessionModel)
        m_playbackSessionModel->removeNowPlayingMetadataObserver(m_nowPlayingMetadataObserver);
}

void PlaybackSessionInterfaceAVKit::nowPlayingMetadataChanged(const NowPlayingMetadata& metadata)
{
    [m_contentSource setTitle:metadata.title];
    [m_contentSource setSubtitle:metadata.artist];
}

#if !RELEASE_LOG_DISABLED
ASCIILiteral PlaybackSessionInterfaceAVKit::logClassName() const
{
    return "PlaybackSessionInterfaceAVKit"_s;
}
#endif

} // namespace WebCore

#endif // HAVE(AVKIT_CONTENT_SOURCE)
