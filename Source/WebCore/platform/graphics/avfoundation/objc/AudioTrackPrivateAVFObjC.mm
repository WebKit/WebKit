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
#import "AudioTrackPrivateAVFObjC.h"

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

SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicIsMainProgramContent, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicDescribesVideoForAccessibility, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicIsAuxiliaryContent, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicTranscribesSpokenDialogForAccessibility, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMetadataCommonKeyTitle, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMetadataKeySpaceCommon, NSString *)

#define AVMetadataItem getAVMetadataItemClass()

#define AVMediaCharacteristicIsMainProgramContent getAVMediaCharacteristicIsMainProgramContent()
#define AVMediaCharacteristicDescribesVideoForAccessibility getAVMediaCharacteristicDescribesVideoForAccessibility()
#define AVMediaCharacteristicIsAuxiliaryContent getAVMediaCharacteristicIsAuxiliaryContent()
#define AVMediaCharacteristicTranscribesSpokenDialogForAccessibility getAVMediaCharacteristicTranscribesSpokenDialogForAccessibility()
#define AVMetadataCommonKeyTitle getAVMetadataCommonKeyTitle()
#define AVMetadataKeySpaceCommon getAVMetadataKeySpaceCommon()

namespace WebCore {

AudioTrackPrivateAVFObjC::AudioTrackPrivateAVFObjC(AVPlayerItemTrack* track)
    : m_playerItemTrack(track)
{
    resetPropertiesFromTrack();
}

void AudioTrackPrivateAVFObjC::resetPropertiesFromTrack()
{
    AVAssetTrack* assetTrack = [m_playerItemTrack.get() assetTrack];
    if (!assetTrack) {
        setEnabled(false);
        setKind(None);
        setId(emptyAtom);
        setLabel(emptyAtom);
        setLanguage(emptyAtom);
        return;
    }

    setEnabled([m_playerItemTrack.get() isEnabled]);
    setId(String::format("%d", [assetTrack trackID]));

    if ([assetTrack hasMediaCharacteristic:AVMediaCharacteristicDescribesVideoForAccessibility])
        setKind(Description);
    else if ([assetTrack hasMediaCharacteristic:AVMediaCharacteristicTranscribesSpokenDialogForAccessibility])
        setKind(Translation);
    else if ([assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsAuxiliaryContent])
        setKind(Alternative);
    else if ([assetTrack hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent])
        setKind(Main);
    else
        setKind(None);

    NSArray *titles = [AVMetadataItem metadataItemsFromArray:[assetTrack commonMetadata] withKey:AVMetadataCommonKeyTitle keySpace:AVMetadataKeySpaceCommon];
    if ([titles count]) {
        // If possible, return a title in one of the user's preferred languages.
        NSArray *titlesForPreferredLanguages = [AVMetadataItem metadataItemsFromArray:titles filteredAndSortedAccordingToPreferredLanguages:[NSLocale preferredLanguages]];
        if ([titlesForPreferredLanguages count])
            setLabel([[titlesForPreferredLanguages objectAtIndex:0] stringValue]);
        else
            setLabel([[titles objectAtIndex:0] stringValue]);
    }

    setLanguage(languageForAVAssetTrack(assetTrack));
}

void AudioTrackPrivateAVFObjC::setPlayerItemTrack(AVPlayerItemTrack *track)
{
    m_playerItemTrack = track;
    resetPropertiesFromTrack();
}

void AudioTrackPrivateAVFObjC::setEnabled(bool enabled)
{
    AudioTrackPrivateAVF::setEnabled(enabled);
    [m_playerItemTrack.get() setEnabled:enabled];
}

String AudioTrackPrivateAVFObjC::languageForAVAssetTrack(AVAssetTrack* track)
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

}

#endif

