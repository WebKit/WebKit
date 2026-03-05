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

#if HAVE(AVEXPERIENCECONTROLLER)

#import "WKAVContentSource.h"
#import <WebCore/MediaSelectionOption.h>
#import <WebCore/NowPlayingInfo.h>
#import <WebCore/TimeRanges.h>
#import <wtf/TZoneMallocInlines.h>

#import <pal/cf/CoreMediaSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVInterfaceMediaSelectionOptionSource)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVInterfaceMetadata)
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVInterfaceTimelineSegment)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlaybackSessionInterfaceAVKit);

Ref<PlaybackSessionInterfaceAVKit> PlaybackSessionInterfaceAVKit::create(WebCore::PlaybackSessionModel& model)
{
    Ref interface = adoptRef(*new PlaybackSessionInterfaceAVKit(model));
    interface->initialize();
    return interface;
}

static Ref<WebCore::NowPlayingMetadataObserver> nowPlayingMetadataObserver(PlaybackSessionInterfaceAVKit& interface)
{
    return WebCore::NowPlayingMetadataObserver::create({
        [weakInterface = WeakPtr { interface }](auto& metadata) {
            if (RefPtr interface = weakInterface.get())
                interface->nowPlayingMetadataChanged(metadata);
        }
    });
}

PlaybackSessionInterfaceAVKit::PlaybackSessionInterfaceAVKit(WebCore::PlaybackSessionModel& model)
    : PlaybackSessionInterfaceIOS { model }
    , m_contentSource { adoptNS([[WKAVContentSource alloc] initWithModel:model]) }
    , m_nowPlayingMetadataObserver { nowPlayingMetadataObserver(*this) }
{
}

PlaybackSessionInterfaceAVKit::~PlaybackSessionInterfaceAVKit()
{
    invalidate();
}

void PlaybackSessionInterfaceAVKit::durationChanged(double duration)
{
    [m_contentSource setTimeRange:PAL::CMTimeRangeMake(PAL::kCMTimeZero, PAL::CMTimeMakeWithSeconds(duration, 1000))];
    [m_contentSource setSupportedSeekCapabilities:AVInterfaceSeekCapabilitiesSeek];
    [m_contentSource setHasAudio:YES];
    [m_contentSource setReady:YES];
}

void PlaybackSessionInterfaceAVKit::currentTimeChanged(double currentTime, double)
{
    [m_contentSource setCurrentPlaybackPositionInternal:PAL::CMTimeMakeWithSeconds(currentTime, 1000)];
}

void PlaybackSessionInterfaceAVKit::rateChanged(OptionSet<WebCore::PlaybackSessionModel::PlaybackState> playbackState, double playbackRate, double)
{
    [m_contentSource setBuffering:playbackState.contains(WebCore::PlaybackSessionModel::PlaybackState::Stalled)];
    [m_contentSource setPlayingInternal:playbackState.contains(WebCore::PlaybackSessionModel::PlaybackState::Playing)];
    if (![m_contentSource isBuffering])
        [m_contentSource setPlaybackSpeedInternal:[m_contentSource isPlaying] ? playbackRate : 0];
}

void PlaybackSessionInterfaceAVKit::seekableRangesChanged(const WebCore::PlatformTimeRanges& timeRanges, double, double)
{
    [m_contentSource setSeekableTimeRanges:makeNSArray(timeRanges).get()];
}

static RetainPtr<AVInterfaceMediaSelectionOptionSource> mediaSelectionOptionSource(const WebCore::MediaSelectionOption& option)
{
    return adoptNS([allocAVInterfaceMediaSelectionOptionSourceInstance() initWithDisplayName:option.displayName.createNSString().get() identifier:[NSUUID UUID].UUIDString extendedLanguageTag:option.languageTag.createNSString().get()]);
}

void PlaybackSessionInterfaceAVKit::audioMediaSelectionOptionsChanged(const Vector<WebCore::MediaSelectionOption>& options, uint64_t selectedIndex)
{
    RetainPtr audioOptions = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& option : options)
        [audioOptions addObject:mediaSelectionOptionSource(option).get()];

    [m_contentSource setAudioOptions:audioOptions.get()];
    audioMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionInterfaceAVKit::legibleMediaSelectionOptionsChanged(const Vector<WebCore::MediaSelectionOption>& options, uint64_t selectedIndex)
{
    RetainPtr legibleOptions = adoptNS([[NSMutableArray alloc] initWithCapacity:options.size()]);
    for (auto& option : options)
        [legibleOptions addObject:mediaSelectionOptionSource(option).get()];

    [m_contentSource setLegibleOptions:legibleOptions.get()];
    legibleMediaSelectionIndexChanged(selectedIndex);
}

void PlaybackSessionInterfaceAVKit::audioMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    if (selectedIndex < [m_contentSource audioOptions].count)
        [m_contentSource setCurrentAudioOptionIndex:selectedIndex];
    else
        [m_contentSource setCurrentAudioOptionIndex:NSNotFound];
}

void PlaybackSessionInterfaceAVKit::legibleMediaSelectionIndexChanged(uint64_t selectedIndex)
{
    if (selectedIndex < [m_contentSource legibleOptions].count)
        [m_contentSource setCurrentLegibleOptionIndex:selectedIndex];
    else
        [m_contentSource setCurrentLegibleOptionIndex:NSNotFound];
}

void PlaybackSessionInterfaceAVKit::mutedChanged(bool muted)
{
    [m_contentSource setMutedInternal:muted];
}

void PlaybackSessionInterfaceAVKit::volumeChanged(double volume)
{
    [m_contentSource setVolumeInternal:volume];
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

void PlaybackSessionInterfaceAVKit::nowPlayingMetadataChanged(const WebCore::NowPlayingMetadata& metadata)
{
    [m_contentSource setMetadata:createPlatformMetadata(metadata.title.createNSString().get(), metadata.artist.createNSString().get())];
}

#if !RELEASE_LOG_DISABLED
ASCIILiteral PlaybackSessionInterfaceAVKit::logClassName() const
{
    return "PlaybackSessionInterfaceAVKit"_s;
}
#endif

} // namespace WebKit

#endif // HAVE(AVEXPERIENCECONTROLLER)
