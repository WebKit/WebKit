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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#import "SoftLinking.h"
#import <objc/runtime.h>
#import <AVFoundation/AVAssetTrack.h>
#import <AVFoundation/AVPlayerItemTrack.h>
#import <AVFoundation/AVMetadataItem.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS(AVFoundation, AVAssetTrack)
SOFT_LINK_CLASS(AVFoundation, AVPlayerItemTrack)
SOFT_LINK_CLASS(AVFoundation, AVMetadataItem)

SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVMediaCharacteristicIsMainProgramContent, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVMediaCharacteristicDescribesVideoForAccessibility, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVMediaCharacteristicIsAuxiliaryContent, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVMediaCharacteristicTranscribesSpokenDialogForAccessibility, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVMetadataCommonKeyTitle, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVMetadataKeySpaceCommon, NSString *)

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

bool AVTrackPrivateAVFObjCImpl::enabled() const
{
    ASSERT(m_playerItemTrack);
    return [m_playerItemTrack isEnabled];
}

void AVTrackPrivateAVFObjCImpl::setEnabled(bool enabled)
{
    ASSERT(m_playerItemTrack);
    [m_playerItemTrack setEnabled:enabled];
}

AudioTrackPrivate::Kind AVTrackPrivateAVFObjCImpl::audioKind() const
{
    if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
        return AudioTrackPrivate::Alternative;
    else if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
        return AudioTrackPrivate::Main;
    return AudioTrackPrivate::None;
}

VideoTrackPrivate::Kind AVTrackPrivateAVFObjCImpl::videoKind() const
{
    if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
        return VideoTrackPrivate::Sign;
    else if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicTranscribesSpokenDialogForAccessibility])
        return VideoTrackPrivate::Captions;
    else if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
        return VideoTrackPrivate::Alternative;
    else if ([m_assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
        return VideoTrackPrivate::Main;
    return VideoTrackPrivate::None;
}

AtomicString AVTrackPrivateAVFObjCImpl::id() const
{
    return String::format("%d", [m_assetTrack trackID]);
}

AtomicString AVTrackPrivateAVFObjCImpl::label() const
{
    NSArray *titles = [AVMetadataItem metadataItemsFromArray:[m_assetTrack commonMetadata] withKey:AVMetadataCommonKeyTitle keySpace:AVMetadataKeySpaceCommon];
    if (![titles count])
        return emptyAtom;

    // If possible, return a title in one of the user's preferred languages.
    NSArray *titlesForPreferredLanguages = [AVMetadataItem metadataItemsFromArray:titles filteredAndSortedAccordingToPreferredLanguages:[NSLocale preferredLanguages]];
    if ([titlesForPreferredLanguages count])
        return [[titlesForPreferredLanguages objectAtIndex:0] stringValue];
    return [[titles objectAtIndex:0] stringValue];
}

AtomicString AVTrackPrivateAVFObjCImpl::language() const
{
    return languageForAVAssetTrack(m_assetTrack.get());
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

int AVTrackPrivateAVFObjCImpl::trackID() const
{
    return [m_assetTrack trackID];
}

}

#endif // ENABLE(VIDEO_TRACK)
