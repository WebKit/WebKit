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

#include "config.h"
#include "AVTrackPrivateAVFObjCImpl.h"

#if ENABLE(VIDEO_TRACK)

#import "MediaSelectionGroupAVFObjC.h"
#import <AVFoundation/AVAssetTrack.h>
#import <AVFoundation/AVMediaSelectionGroup.h>
#import <AVFoundation/AVMetadataItem.h>
#import <AVFoundation/AVPlayerItem.h>
#import <AVFoundation/AVPlayerItemTrack.h>
#import <objc/runtime.h>
#import <wtf/SoftLinking.h>

@class AVMediaSelectionOption;
@interface AVMediaSelectionOption (WebKitInternal)
- (id)optionID;
@end

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS(AVFoundation, AVAssetTrack)
SOFT_LINK_CLASS(AVFoundation, AVPlayerItem)
SOFT_LINK_CLASS(AVFoundation, AVPlayerItemTrack)
SOFT_LINK_CLASS(AVFoundation, AVMediaSelectionGroup)
SOFT_LINK_CLASS(AVFoundation, AVMediaSelectionOption)
SOFT_LINK_CLASS(AVFoundation, AVMetadataItem)

SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVMediaCharacteristicIsMainProgramContent, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVMediaCharacteristicDescribesVideoForAccessibility, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVMediaCharacteristicIsAuxiliaryContent, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVMediaCharacteristicTranscribesSpokenDialogForAccessibility, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVMetadataCommonKeyTitle, NSString *)
SOFT_LINK_CONSTANT_MAY_FAIL(AVFoundation, AVMetadataKeySpaceCommon, NSString *)

#define AVMetadataItem getAVMetadataItemClass()

#define AVMediaCharacteristicIsMainProgramContent getAVMediaCharacteristicIsMainProgramContent()
#define AVMediaCharacteristicDescribesVideoForAccessibility getAVMediaCharacteristicDescribesVideoForAccessibility()
#define AVMediaCharacteristicIsAuxiliaryContent getAVMediaCharacteristicIsAuxiliaryContent()
#define AVMediaCharacteristicTranscribesSpokenDialogForAccessibility getAVMediaCharacteristicTranscribesSpokenDialogForAccessibility()
#define AVMetadataCommonKeyTitle getAVMetadataCommonKeyTitle()
#define AVMetadataKeySpaceCommon getAVMetadataKeySpaceCommon()

namespace WebCore {

AVTrackPrivateAVFObjCImpl::AVTrackPrivateAVFObjCImpl(AVPlayerItemTrack* track)
    : m_playerItemTrack(track)
    , m_assetTrack([track assetTrack])
{
}

AVTrackPrivateAVFObjCImpl::AVTrackPrivateAVFObjCImpl(AVAssetTrack* track)
    : m_assetTrack(track)
{
}

AVTrackPrivateAVFObjCImpl::AVTrackPrivateAVFObjCImpl(MediaSelectionOptionAVFObjC& option)
    : m_mediaSelectionOption(&option)
{
}

AVTrackPrivateAVFObjCImpl::~AVTrackPrivateAVFObjCImpl()
{
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
        if (canLoadAVMediaCharacteristicIsAuxiliaryContent() && [m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
            return AudioTrackPrivate::Alternative;
        if (canLoadAVMediaCharacteristicDescribesVideoForAccessibility() && [m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
            return AudioTrackPrivate::Description;
        if (canLoadAVMediaCharacteristicIsMainProgramContent() && [m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
            return AudioTrackPrivate::Main;
        return AudioTrackPrivate::None;
    }

    if (m_mediaSelectionOption) {
        AVMediaSelectionOption *option = m_mediaSelectionOption->avMediaSelectionOption();
        if (canLoadAVMediaCharacteristicIsAuxiliaryContent() && [option hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
            return AudioTrackPrivate::Alternative;
        if (canLoadAVMediaCharacteristicDescribesVideoForAccessibility() && [option hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
            return AudioTrackPrivate::Description;
        if (canLoadAVMediaCharacteristicIsMainProgramContent() && [option hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
            return AudioTrackPrivate::Main;
        return AudioTrackPrivate::None;
    }

    ASSERT_NOT_REACHED();
    return AudioTrackPrivate::None;
}

VideoTrackPrivate::Kind AVTrackPrivateAVFObjCImpl::videoKind() const
{
    if (m_assetTrack) {
        if (canLoadAVMediaCharacteristicDescribesVideoForAccessibility() && [m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
            return VideoTrackPrivate::Sign;
        if (canLoadAVMediaCharacteristicTranscribesSpokenDialogForAccessibility() && [m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicTranscribesSpokenDialogForAccessibility])
            return VideoTrackPrivate::Captions;
        if (canLoadAVMediaCharacteristicIsAuxiliaryContent() && [m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
            return VideoTrackPrivate::Alternative;
        if (canLoadAVMediaCharacteristicIsMainProgramContent() && [m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
            return VideoTrackPrivate::Main;
        return VideoTrackPrivate::None;
    }

    if (m_mediaSelectionOption) {
        AVMediaSelectionOption *option = m_mediaSelectionOption->avMediaSelectionOption();
        if (canLoadAVMediaCharacteristicDescribesVideoForAccessibility() && [option hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
            return VideoTrackPrivate::Sign;
        if (canLoadAVMediaCharacteristicTranscribesSpokenDialogForAccessibility() && [option hasMediaCharacteristic:AVMediaCharacteristicTranscribesSpokenDialogForAccessibility])
            return VideoTrackPrivate::Captions;
        if (canLoadAVMediaCharacteristicIsAuxiliaryContent() && [option hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
            return VideoTrackPrivate::Alternative;
        if (canLoadAVMediaCharacteristicIsMainProgramContent() && [option hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
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

AtomicString AVTrackPrivateAVFObjCImpl::id() const
{
    if (m_assetTrack)
        return String::format("%d", [m_assetTrack trackID]);
    if (m_mediaSelectionOption)
        return [[m_mediaSelectionOption->avMediaSelectionOption() optionID] stringValue];
    ASSERT_NOT_REACHED();
    return emptyAtom();
}

AtomicString AVTrackPrivateAVFObjCImpl::label() const
{
    if (!canLoadAVMetadataCommonKeyTitle() || !canLoadAVMetadataKeySpaceCommon())
        return emptyAtom();

    NSArray *commonMetadata = nil;
    if (m_assetTrack)
        commonMetadata = [m_assetTrack commonMetadata];
    else if (m_mediaSelectionOption)
        commonMetadata = [m_mediaSelectionOption->avMediaSelectionOption() commonMetadata];
    else
        ASSERT_NOT_REACHED();

    NSArray *titles = [AVMetadataItem metadataItemsFromArray:commonMetadata withKey:AVMetadataCommonKeyTitle keySpace:AVMetadataKeySpaceCommon];
    if (![titles count])
        return emptyAtom();

    // If possible, return a title in one of the user's preferred languages.
    NSArray *titlesForPreferredLanguages = [AVMetadataItem metadataItemsFromArray:titles filteredAndSortedAccordingToPreferredLanguages:[NSLocale preferredLanguages]];
    if ([titlesForPreferredLanguages count])
        return [[titlesForPreferredLanguages objectAtIndex:0] stringValue];
    return [[titles objectAtIndex:0] stringValue];
}

AtomicString AVTrackPrivateAVFObjCImpl::language() const
{
    if (m_assetTrack)
        return languageForAVAssetTrack(m_assetTrack.get());
    if (m_mediaSelectionOption)
        return languageForAVMediaSelectionOption(m_mediaSelectionOption->avMediaSelectionOption());

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

int AVTrackPrivateAVFObjCImpl::trackID() const
{
    if (m_assetTrack)
        return [m_assetTrack trackID];
    if (m_mediaSelectionOption)
        return [[m_mediaSelectionOption->avMediaSelectionOption() optionID] intValue];
    ASSERT_NOT_REACHED();
    return 0;
}

}

#endif // ENABLE(VIDEO_TRACK)
