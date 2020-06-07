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

#import "MediaSelectionGroupAVFObjC.h"
#import <AVFoundation/AVAssetTrack.h>
#import <AVFoundation/AVMediaSelectionGroup.h>
#import <AVFoundation/AVMetadataItem.h>
#import <AVFoundation/AVPlayerItem.h>
#import <AVFoundation/AVPlayerItemTrack.h>
#import <objc/runtime.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

@class AVMediaSelectionOption;
@interface AVMediaSelectionOption (WebKitInternal)
- (id)optionID;
@end

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

AtomString AVTrackPrivateAVFObjCImpl::id() const
{
    if (m_assetTrack)
        return AtomString::number([m_assetTrack trackID]);
    if (m_mediaSelectionOption)
        return [[m_mediaSelectionOption->avMediaSelectionOption() optionID] stringValue];
    ASSERT_NOT_REACHED();
    return emptyAtom();
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

#endif // ENABLE(VIDEO)
