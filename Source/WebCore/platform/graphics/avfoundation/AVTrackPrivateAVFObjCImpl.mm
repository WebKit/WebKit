/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#import "AVAssetTrackUtilities.h"
#import "FormatDescriptionUtilities.h"
#import "FourCC.h"
#import "ImmersiveVideoMetadata.h"
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
#import <wtf/TZoneMallocInlines.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@class AVMediaSelectionOption;
@interface AVMediaSelectionOption (WebKitInternal)
- (id)optionID;
@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AVTrackPrivateAVFObjCImpl);

static NSArray* assetTrackConfigurationKeyNames()
{
    static NSArray* keys = [[NSArray alloc] initWithObjects:@"formatDescriptions", @"estimatedDataRate", @"nominalFrameRate", nil];
    return keys;
}

static AVAssetTrack* assetTrackFor(const AVTrackPrivateAVFObjCImpl& impl)
{
    if (impl.playerItemTrack() && impl.playerItemTrack().assetTrack)
        return impl.playerItemTrack().assetTrack;
    if (impl.assetTrack())
        return impl.assetTrack();
    if (RefPtr mediaSelectionOption = impl.mediaSelectionOption())
        return mediaSelectionOption->assetTrack();
    return nil;
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

AVTrackPrivateAVFObjCImpl::~AVTrackPrivateAVFObjCImpl() = default;

void AVTrackPrivateAVFObjCImpl::initializeAssetTrack()
{
    if (!m_assetTrack)
        return;

    [m_assetTrack loadValuesAsynchronouslyForKeys:assetTrackConfigurationKeyNames() completionHandler:[weakThis = ThreadSafeWeakPtr { *this }]() mutable {
        callOnMainThread([weakThis = WTF::move(weakThis)] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->initializationCompleted();
        });
    }];
}

void AVTrackPrivateAVFObjCImpl::initializationCompleted()
{
    assertIsMainThread();

    if (m_audioTrackConfigurationObserver)
        (*m_audioTrackConfigurationObserver)();
    if (m_videoTrackConfigurationObserver)
        (*m_videoTrackConfigurationObserver)();
}

bool AVTrackPrivateAVFObjCImpl::enabled() const
{
    if (m_playerItemTrack)
        return [m_playerItemTrack isEnabled];
    if (RefPtr mediaSelectionOption = m_mediaSelectionOption)
        return mediaSelectionOption->selected();
    ASSERT_NOT_REACHED();
    return false;
}

void AVTrackPrivateAVFObjCImpl::setEnabled(bool enabled)
{
    if (m_playerItemTrack)
        [m_playerItemTrack setEnabled:enabled];
    else if (RefPtr mediaSelectionOption = m_mediaSelectionOption)
        mediaSelectionOption->setSelected(enabled);
    else
        ASSERT_NOT_REACHED();
}

AudioTrackPrivate::Kind AVTrackPrivateAVFObjCImpl::audioKind() const
{
    if (m_assetTrack) {
        // Check if accessibility characteristics are missing from AVAssetTrack but might be preserved in MediaSelectionOption
        bool hasDescribes = [m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility];
        if (!hasDescribes && m_mediaSelectionOption) {
            RetainPtr option = m_mediaSelectionOption->avMediaSelectionOption();
            if ([option hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
                return AudioTrackPrivate::Kind::Description;
        }

        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
            return AudioTrackPrivate::Kind::Alternative;
        if (hasDescribes)
            return AudioTrackPrivate::Kind::Description;
        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
            return AudioTrackPrivate::Kind::Main;
        return AudioTrackPrivate::Kind::None;
    }

    if (m_mediaSelectionOption) {
        RetainPtr option = m_mediaSelectionOption->avMediaSelectionOption();
        if ([option hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
            return AudioTrackPrivate::Kind::Alternative;
        if ([option hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
            return AudioTrackPrivate::Kind::Description;
        if ([option hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
            return AudioTrackPrivate::Kind::Main;
        return AudioTrackPrivate::Kind::None;
    }

    ASSERT_NOT_REACHED();
    return AudioTrackPrivate::Kind::None;
}

VideoTrackPrivate::Kind AVTrackPrivateAVFObjCImpl::videoKind() const
{
    if (m_assetTrack) {
        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
            return VideoTrackPrivate::Kind::Sign;
        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicTranscribesSpokenDialogForAccessibility])
            return VideoTrackPrivate::Kind::Captions;
        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
            return VideoTrackPrivate::Kind::Alternative;
        if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
            return VideoTrackPrivate::Kind::Main;
        return VideoTrackPrivate::Kind::None;
    }

    if (m_mediaSelectionOption) {
        RetainPtr option = m_mediaSelectionOption->avMediaSelectionOption();
        if ([option hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
            return VideoTrackPrivate::Kind::Sign;
        if ([option hasMediaCharacteristic:AVMediaCharacteristicTranscribesSpokenDialogForAccessibility])
            return VideoTrackPrivate::Kind::Captions;
        if ([option hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
            return VideoTrackPrivate::Kind::Alternative;
        if ([option hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
            return VideoTrackPrivate::Kind::Main;
        return VideoTrackPrivate::Kind::None;
    }

    ASSERT_NOT_REACHED();
    return VideoTrackPrivate::Kind::None;
}

InbandTextTrackPrivate::Kind AVTrackPrivateAVFObjCImpl::textKindForAVAssetTrack(const AVAssetTrack *track)
{
    RetainPtr mediaType = [track mediaType];
    if ([mediaType isEqualToString:AVMediaTypeClosedCaption])
        return InbandTextTrackPrivate::Kind::Captions;
    if ([mediaType isEqualToString:AVMediaTypeSubtitle]) {

        if ([track hasMediaCharacteristic:AVMediaCharacteristicContainsOnlyForcedSubtitles])
            return InbandTextTrackPrivate::Kind::Forced;

        // An "SDH" track is a subtitle track created for the deaf or hard-of-hearing. "captions" in WebVTT are
        // "labeled as appropriate for the hard-of-hearing", so tag SDH sutitles as "captions".
        if ([track hasMediaCharacteristic:AVMediaCharacteristicTranscribesSpokenDialogForAccessibility])
            return InbandTextTrackPrivate::Kind::Captions;
        if ([track hasMediaCharacteristic:AVMediaCharacteristicDescribesMusicAndSoundForAccessibility])
            return InbandTextTrackPrivate::Kind::Captions;

        return InbandTextTrackPrivate::Kind::Subtitles;
    }

    RetainPtr formatDescriptions = [track formatDescriptions];
    if ([formatDescriptions count]) {
        FourCC codec = PAL::softLink_CoreMedia_CMFormatDescriptionGetMediaSubType((__bridge CMFormatDescriptionRef)[formatDescriptions objectAtIndex:0]);
        if (codec == kCMSubtitleFormatType_WebVTT)
            return InbandTextTrackPrivate::Kind::Captions;
    }

    return InbandTextTrackPrivate::Kind::Captions;
}

InbandTextTrackPrivate::Kind AVTrackPrivateAVFObjCImpl::textKindForAVMediaSelectionOption(const AVMediaSelectionOption *option)
{
    RetainPtr mediaType = [option mediaType];
    if ([mediaType isEqualToString:AVMediaTypeClosedCaption])
        return InbandTextTrackPrivate::Kind::Captions;
    if ([mediaType isEqualToString:AVMediaTypeSubtitle]) {

        if ([option hasMediaCharacteristic:AVMediaCharacteristicContainsOnlyForcedSubtitles])
            return InbandTextTrackPrivate::Kind::Forced;

        // An "SDH" track is a subtitle track created for the deaf or hard-of-hearing. "captions" in WebVTT are
        // "labeled as appropriate for the hard-of-hearing", so tag SDH sutitles as "captions".
        if ([option hasMediaCharacteristic:AVMediaCharacteristicTranscribesSpokenDialogForAccessibility])
            return InbandTextTrackPrivate::Kind::Captions;
        if ([option hasMediaCharacteristic:AVMediaCharacteristicDescribesMusicAndSoundForAccessibility])
            return InbandTextTrackPrivate::Kind::Captions;

        return InbandTextTrackPrivate::Kind::Subtitles;
    }

    return InbandTextTrackPrivate::Kind::Captions;
}

InbandTextTrackPrivate::Kind AVTrackPrivateAVFObjCImpl::textKind() const
{
    if (m_assetTrack)
        return textKindForAVAssetTrack(m_assetTrack.get());

    if (m_mediaSelectionOption)
        return textKindForAVMediaSelectionOption(m_mediaSelectionOption->avMediaSelectionOption().get());

    return InbandTextTrackPrivate::Kind::None;
}

int AVTrackPrivateAVFObjCImpl::index() const
{
    if (m_assetTrack)
        return [[[m_assetTrack asset] tracks] indexOfObject:m_assetTrack.get()];
    if (RefPtr mediaSelectionOption = m_mediaSelectionOption)
        return mediaSelectionOption->index();
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

String AVTrackPrivateAVFObjCImpl::label() const
{
    ASSERT(m_assetTrack || m_mediaSelectionOption);

    RetainPtr<NSArray> commonMetadata = nil;
    if (m_assetTrack)
        commonMetadata = [m_assetTrack commonMetadata];
    if (![commonMetadata count] && m_mediaSelectionOption)
        commonMetadata = [m_mediaSelectionOption->avMediaSelectionOption() commonMetadata];

    RetainPtr titles = [PAL::getAVMetadataItemClassSingleton() metadataItemsFromArray:commonMetadata.get() withKey:AVMetadataCommonKeyTitle keySpace:AVMetadataKeySpaceCommon];
    if (![titles count])
        return emptyString();

    // If possible, return a title in one of the user's preferred languages.
    RetainPtr titlesForPreferredLanguages = [PAL::getAVMetadataItemClassSingleton() metadataItemsFromArray:titles.get() filteredAndSortedAccordingToPreferredLanguages:[NSLocale preferredLanguages]];
    if ([titlesForPreferredLanguages count])
        return [[titlesForPreferredLanguages objectAtIndex:0] stringValue];
    return [[titles objectAtIndex:0] stringValue];
}

String AVTrackPrivateAVFObjCImpl::language() const
{
    if (m_assetTrack) {
        auto language = languageForAVAssetTrack(m_assetTrack.get());
        if (!language.isEmpty())
            return language;
    }

    if (m_mediaSelectionOption) {
        auto language = languageForAVMediaSelectionOption(m_mediaSelectionOption->avMediaSelectionOption().get());
        if (!language.isEmpty())
            return language;
    }

    return emptyString();
}

String AVTrackPrivateAVFObjCImpl::languageForAVAssetTrack(const AVAssetTrack *track)
{
    RetainPtr language = [track extendedLanguageTag];

    // If the language code is stored as a QuickTime 5-bit packed code there aren't enough bits for a full
    // RFC 4646 language tag so extendedLanguageTag returns NULL. In this case languageCode will return the
    // ISO 639-2/T language code so check it.
    if (!language)
        language = [track languageCode];

    // Some legacy tracks have "und" as a language, treat that the same as no language at all.
    if (!language || [language isEqualToString:@"und"])
        return emptyString();

    return language.get();
}

String AVTrackPrivateAVFObjCImpl::languageForAVMediaSelectionOption(const AVMediaSelectionOption *option)
{
    RetainPtr language = [option extendedLanguageTag];

    // If the language code is stored as a QuickTime 5-bit packed code there aren't enough bits for a full
    // RFC 4646 language tag so extendedLanguageTag returns NULL. In this case languageCode will return the
    // ISO 639-2/T language code so check it.
    if (!language)
        language = [[option locale] objectForKey:NSLocaleLanguageCode];

    // Some legacy tracks have "und" as a language, treat that the same as no language at all.
    if (!language || [language isEqualToString:@"und"])
        return emptyString();

    return language.get();
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
        immersiveVideoMetadata(),
        isProtected(),
    };
}

PlatformAudioTrackConfiguration AVTrackPrivateAVFObjCImpl::audioTrackConfiguration() const
{
    return {
        { codec() },
        sampleRate(),
        numberOfChannels(),
        bitrate(),
        isProtected(),
    };
}

void AVTrackPrivateAVFObjCImpl::setVideoTrackConfigurationObserver(VideoTrackConfigurationObserver& observer)
{
    assertIsMainThread();

    m_videoTrackConfigurationObserver = observer;
}

void AVTrackPrivateAVFObjCImpl::setAudioTrackConfigurationObserver(AudioTrackConfigurationObserver& observer)
{
    assertIsMainThread();

    m_audioTrackConfigurationObserver = observer;
}

static RetainPtr<CMFormatDescriptionRef> formatDescriptionFor(const AVTrackPrivateAVFObjCImpl& impl)
{
    RetainPtr assetTrack = assetTrackFor(impl);
    if (!assetTrack || [assetTrack.get() statusOfValueForKey:@"formatDescriptions" error:nil] != AVKeyValueStatusLoaded)
        return nullptr;

    return static_cast<CMFormatDescriptionRef>(assetTrack.get().formatDescriptions.firstObject);
}

String AVTrackPrivateAVFObjCImpl::codec() const
{
    return codecFromFormatDescription(formatDescriptionFor(*this).get());
}

uint32_t AVTrackPrivateAVFObjCImpl::width() const
{
    if (RetainPtr assetTrack = assetTrackFor(*this))
        return assetTrack.get().naturalSize.width;
    ASSERT_NOT_REACHED();
    return 0;
}

uint32_t AVTrackPrivateAVFObjCImpl::height() const
{
    if (RetainPtr assetTrack = assetTrackFor(*this))
        return assetTrack.get().naturalSize.height;
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
    RetainPtr assetTrack = assetTrackFor(*this);
    if (!assetTrack)
        return 0;
    if ([assetTrack.get() statusOfValueForKey:@"nominalFrameRate" error:nil] != AVKeyValueStatusLoaded)
        return 0;
    return assetTrack.get().nominalFrameRate;
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
    RetainPtr assetTrack = assetTrackFor(*this);
    if (!assetTrack)
        return 0;
    if ([assetTrack.get() statusOfValueForKey:@"estimatedDataRate" error:nil] != AVKeyValueStatusLoaded)
        return 0;
    if (!std::isfinite(assetTrack.get().estimatedDataRate))
        return 0;
    return assetTrack.get().estimatedDataRate;
}

bool AVTrackPrivateAVFObjCImpl::isProtected() const
{
    return formatDescriptionIsProtected(formatDescriptionFor(*this).get());
}

std::optional<ImmersiveVideoMetadata> AVTrackPrivateAVFObjCImpl::immersiveVideoMetadata() const
{
    return immersiveVideoMetadataFromFormatDescription(formatDescriptionFor(*this).get());
}

}

#endif // ENABLE(VIDEO)
