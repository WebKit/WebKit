/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "AVTrackPrivateAVFObjCImpl.h"

#if ENABLE(VIDEO)

#import "FormatDescriptionUtilities.h"
#import "MediaSelectionGroupAVFObjC.h"
#import "PlatformAudioTrackConfiguration.h"
#import "PlatformVideoTrackConfiguration.h"
#import "SharedBuffer.h"
#import <AVFoundation/AVAssetTrack.h>
#import <AVFoundation/AVMediaSelectionGroup.h>
#import <AVFoundation/AVMetadataItem.h>
#import <AVFoundation/AVPlayerItem.h>
#import <AVFoundation/AVPlayerItemTrack.h>
#import <objc/runtime.h>
#import <wtf/RunLoop.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@class AVMediaSelectionOption;
@interface AVMediaSelectionOption (WebKitInternal)
- (id)optionID;
@end

namespace WebCore {

static NSArray* assetTrackConfigurationKeyNames()
{
    static NSArray* keys = [[NSArray alloc] initWithObjects:@"formatDescriptions", @"estimatedDataRate", @"nominalFrameRate", nil];
    return keys;
}

AVTrackPrivateAVFObjCImpl::AVTrackPrivateAVFObjCImpl(AVPlayerItemTrack* track)
    : m_playerItemTrack(track)
    , m_assetTrack([track assetTrack])
{
    initializeAssetTrack();
}

AVTrackPrivateAVFObjCImpl::AVTrackPrivateAVFObjCImpl(AVAssetTrack* track)
    : m_assetTrack(track)
{
    initializeAssetTrack();
}

AVTrackPrivateAVFObjCImpl::AVTrackPrivateAVFObjCImpl(MediaSelectionOptionAVFObjC& option)
    : m_mediaSelectionOption(&option)
    , m_assetTrack(option.assetTrack())
{
    initializeAssetTrack();
}

AVTrackPrivateAVFObjCImpl::~AVTrackPrivateAVFObjCImpl()
{
}

void AVTrackPrivateAVFObjCImpl::initializeAssetTrack()
{
    if (!m_assetTrack)
        return;

    [m_assetTrack loadValuesAsynchronouslyForKeys:assetTrackConfigurationKeyNames() completionHandler:[weakThis = WeakPtr(this)] () mutable {
        callOnMainThread([weakThis = WTFMove(weakThis)] {
            if (weakThis && weakThis->m_audioTrackConfigurationObserver)
                (*weakThis->m_audioTrackConfigurationObserver)();
            if (weakThis && weakThis->m_videoTrackConfigurationObserver)
                (*weakThis->m_videoTrackConfigurationObserver)();
        });
    }];
}
    
bool AVTrackPrivateAVFObjCImpl::enabled() const
{
    if (m_playerItemTrack)
        return [m_playerItemTrack isEnabled];
    if (m_mediaSelectionOption)
        return m_mediaSelectionOption->selected();
    ASSERT_NOT_REACHED();
    return false;
}

void AVTrackPrivateAVFObjCImpl::setEnabled(bool enabled)
{
    if (m_playerItemTrack)
        [m_playerItemTrack setEnabled:enabled];
    else if (m_mediaSelectionOption)
        m_mediaSelectionOption->setSelected(enabled);
    else
        ASSERT_NOT_REACHED();
}

AudioTrackPrivate::Kind AVTrackPrivateAVFObjCImpl::audioKind() const
{
    if (m_assetTrack) {
        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
            return AudioTrackPrivate::Alternative;
        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
            return AudioTrackPrivate::Description;
        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
            return AudioTrackPrivate::Main;
        return AudioTrackPrivate::None;
    }

    if (m_mediaSelectionOption) {
        AVMediaSelectionOption *option = m_mediaSelectionOption->avMediaSelectionOption();
        if ([option hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
            return AudioTrackPrivate::Alternative;
        if ([option hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
            return AudioTrackPrivate::Description;
        if ([option hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
            return AudioTrackPrivate::Main;
        return AudioTrackPrivate::None;
    }

    ASSERT_NOT_REACHED();
    return AudioTrackPrivate::None;
}

VideoTrackPrivate::Kind AVTrackPrivateAVFObjCImpl::videoKind() const
{
    if (m_assetTrack) {
        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
            return VideoTrackPrivate::Sign;
        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicTranscribesSpokenDialogForAccessibility])
            return VideoTrackPrivate::Captions;
        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
            return VideoTrackPrivate::Alternative;
        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
            return VideoTrackPrivate::Main;
        return VideoTrackPrivate::None;
    }

    if (m_mediaSelectionOption) {
        AVMediaSelectionOption *option = m_mediaSelectionOption->avMediaSelectionOption();
        if ([option hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
            return VideoTrackPrivate::Sign;
        if ([option hasMediaCharacteristic:AVMediaCharacteristicTranscribesSpokenDialogForAccessibility])
            return VideoTrackPrivate::Captions;
        if ([option hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
            return VideoTrackPrivate::Alternative;
        if ([option hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
            return VideoTrackPrivate::Main;
        return VideoTrackPrivate::None;
    }

    ASSERT_NOT_REACHED();
    return VideoTrackPrivate::None;
}

int AVTrackPrivateAVFObjCImpl::index() const
{
    if (m_assetTrack)
        return [[[m_assetTrack asset] tracks] indexOfObject:m_assetTrack.get()];
    if (m_mediaSelectionOption)
        return [[[m_playerItem asset] tracks] count] + m_mediaSelectionOption->index();
    ASSERT_NOT_REACHED();
    return 0;
}

TrackID AVTrackPrivateAVFObjCImpl::id() const
{
    if (m_assetTrack)
        return [m_assetTrack trackID];
    if (m_mediaSelectionOption)
        return [[m_mediaSelectionOption->avMediaSelectionOption() optionID] unsignedLongLongValue];
    ASSERT_NOT_REACHED();
    return 0;
}

AtomString AVTrackPrivateAVFObjCImpl::label() const
{
    NSArray *commonMetadata = nil;
    if (m_assetTrack)
        commonMetadata = [m_assetTrack commonMetadata];
    else if (m_mediaSelectionOption)
        commonMetadata = [m_mediaSelectionOption->avMediaSelectionOption() commonMetadata];
    else
        ASSERT_NOT_REACHED();

    NSArray *titles = [PAL::getAVMetadataItemClass() metadataItemsFromArray:commonMetadata withKey:AVMetadataCommonKeyTitle keySpace:AVMetadataKeySpaceCommon];
    if (![titles count])
        return emptyAtom();

    // If possible, return a title in one of the user's preferred languages.
    NSArray *titlesForPreferredLanguages = [PAL::getAVMetadataItemClass() metadataItemsFromArray:titles filteredAndSortedAccordingToPreferredLanguages:[NSLocale preferredLanguages]];
    if ([titlesForPreferredLanguages count])
        return [[titlesForPreferredLanguages objectAtIndex:0] stringValue];
    return [[titles objectAtIndex:0] stringValue];
}

AtomString AVTrackPrivateAVFObjCImpl::language() const
{
    if (m_assetTrack)
        return AtomString { languageForAVAssetTrack(m_assetTrack.get()) };
    if (m_mediaSelectionOption)
        return AtomString { languageForAVMediaSelectionOption(m_mediaSelectionOption->avMediaSelectionOption()) };

    ASSERT_NOT_REACHED();
    return emptyAtom();
}

String AVTrackPrivateAVFObjCImpl::languageForAVAssetTrack(AVAssetTrack* track)
{
    NSString *language = [track extendedLanguageTag];

    // If the language code is stored as a QuickTime 5-bit packed code there aren't enough bits for a full
    // RFC 4646 language tag so extendedLanguageTag returns NULL. In this case languageCode will return the
    // ISO 639-2/T language code so check it.
    if (!language)
        language = [track languageCode];

    // Some legacy tracks have "und" as a language, treat that the same as no language at all.
    if (!language || [language isEqualToString:@"und"])
        return emptyString();

    return language;
}

String AVTrackPrivateAVFObjCImpl::languageForAVMediaSelectionOption(AVMediaSelectionOption* option)
{
    NSString *language = [option extendedLanguageTag];

    // If the language code is stored as a QuickTime 5-bit packed code there aren't enough bits for a full
    // RFC 4646 language tag so extendedLanguageTag returns NULL. In this case languageCode will return the
    // ISO 639-2/T language code so check it.
    if (!language)
        language = [[option locale] objectForKey:NSLocaleLanguageCode];

    // Some legacy tracks have "und" as a language, treat that the same as no language at all.
    if (!language || [language isEqualToString:@"und"])
        return emptyString();
    
    return language;
}

PlatformVideoTrackConfiguration AVTrackPrivateAVFObjCImpl::videoTrackConfiguration() const
{
    return {
        { codec() },
        width(),
        height(),
        colorSpace(),
        framerate(),
        bitrate(),
    };
}

PlatformAudioTrackConfiguration AVTrackPrivateAVFObjCImpl::audioTrackConfiguration() const
{
    return {
        { codec() },
        sampleRate(),
        numberOfChannels(),
        bitrate(),
    };
}

static AVAssetTrack* assetTrackFor(const AVTrackPrivateAVFObjCImpl& impl)
{
    if (impl.playerItemTrack() && impl.playerItemTrack().assetTrack)
        return impl.playerItemTrack().assetTrack;
    if (impl.assetTrack())
        return impl.assetTrack();
    if (impl.mediaSelectionOption() && impl.mediaSelectionOption()->assetTrack())
        return impl.mediaSelectionOption()->assetTrack();
    return nil;
}

static RetainPtr<CMFormatDescriptionRef> formatDescriptionFor(const AVTrackPrivateAVFObjCImpl& impl)
{
    auto assetTrack = assetTrackFor(impl);
    if (!assetTrack || [assetTrack statusOfValueForKey:@"formatDescriptions" error:nil] != AVKeyValueStatusLoaded)
        return nullptr;

    return static_cast<CMFormatDescriptionRef>(assetTrack.formatDescriptions.firstObject);
}

String AVTrackPrivateAVFObjCImpl::codec() const
{
    return codecFromFormatDescription(formatDescriptionFor(*this).get());
}

uint32_t AVTrackPrivateAVFObjCImpl::width() const
{
    if (auto assetTrack = assetTrackFor(*this))
        return assetTrack.naturalSize.width;
    ASSERT_NOT_REACHED();
    return 0;
}

uint32_t AVTrackPrivateAVFObjCImpl::height() const
{
    if (auto assetTrack = assetTrackFor(*this))
        return assetTrack.naturalSize.height;
    ASSERT_NOT_REACHED();
    return 0;
}

PlatformVideoColorSpace AVTrackPrivateAVFObjCImpl::colorSpace() const
{
    if (auto colorSpace = colorSpaceFromFormatDescription(formatDescriptionFor(*this).get()))
        return *colorSpace;
    return { };
}

double AVTrackPrivateAVFObjCImpl::framerate() const
{
    auto assetTrack = assetTrackFor(*this);
    if (!assetTrack)
        return 0;
    if ([assetTrack statusOfValueForKey:@"nominalFrameRate" error:nil] != AVKeyValueStatusLoaded)
        return 0;
    return assetTrack.nominalFrameRate;
}

uint32_t AVTrackPrivateAVFObjCImpl::sampleRate() const
{
    auto formatDescription = formatDescriptionFor(*this);
    if (!formatDescription)
        return 0;

    const AudioStreamBasicDescription* const asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription.get());
    if (!asbd)
        return 0;

    return asbd->mSampleRate;
}

uint32_t AVTrackPrivateAVFObjCImpl::numberOfChannels() const
{
    auto formatDescription = formatDescriptionFor(*this);
    if (!formatDescription)
        return 0;

    const AudioStreamBasicDescription* const asbd = PAL::CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription.get());
    if (!asbd)
        return 0;

    return asbd->mChannelsPerFrame;
}

uint64_t AVTrackPrivateAVFObjCImpl::bitrate() const
{
    auto assetTrack = assetTrackFor(*this);
    if (!assetTrack)
        return 0;
    if ([assetTrack statusOfValueForKey:@"estimatedDataRate" error:nil] != AVKeyValueStatusLoaded)
        return 0;
    if (!std::isfinite(assetTrack.estimatedDataRate))
        return 0;
    return assetTrack.estimatedDataRate;
}

}

#endif // ENABLE(VIDEO)
