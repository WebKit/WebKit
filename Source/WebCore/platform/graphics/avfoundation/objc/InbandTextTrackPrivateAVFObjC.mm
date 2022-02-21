/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#import "InbandTextTrackPrivateAVFObjC.h"

#import "FloatConversion.h"
#import "InbandTextTrackPrivate.h"
#import "InbandTextTrackPrivateAVF.h"
#import "Logging.h"
#import <AVFoundation/AVMediaSelectionGroup.h>
#import <AVFoundation/AVMetadataItem.h>
#import <AVFoundation/AVPlayer.h>
#import <AVFoundation/AVPlayerItem.h>
#import <AVFoundation/AVPlayerItemOutput.h>
#import <objc/runtime.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

InbandTextTrackPrivateAVFObjC::InbandTextTrackPrivateAVFObjC(AVFInbandTrackParent* player, AVMediaSelectionOption *selection, InbandTextTrackPrivate::CueFormat format)
    : InbandTextTrackPrivateAVF(player, format)
    , m_mediaSelectionOption(selection)
{
}

void InbandTextTrackPrivateAVFObjC::disconnect()
{
    m_mediaSelectionOption = 0;
    InbandTextTrackPrivateAVF::disconnect();
}

InbandTextTrackPrivate::Kind InbandTextTrackPrivateAVFObjC::kind() const
{
    if (!m_mediaSelectionOption)
        return Kind::None;

    NSString *mediaType = [m_mediaSelectionOption mediaType];
    
    if ([mediaType isEqualToString:AVMediaTypeClosedCaption])
        return Kind::Captions;
    if ([mediaType isEqualToString:AVMediaTypeSubtitle]) {

        if ([m_mediaSelectionOption hasMediaCharacteristic:AVMediaCharacteristicContainsOnlyForcedSubtitles])
            return Kind::Forced;

        // An "SDH" track is a subtitle track created for the deaf or hard-of-hearing. "captions" in WebVTT are
        // "labeled as appropriate for the hard-of-hearing", so tag SDH sutitles as "captions".
        if ([m_mediaSelectionOption hasMediaCharacteristic:AVMediaCharacteristicTranscribesSpokenDialogForAccessibility])
            return Kind::Captions;
        if ([m_mediaSelectionOption hasMediaCharacteristic:AVMediaCharacteristicDescribesMusicAndSoundForAccessibility])
            return Kind::Captions;
        
        return Kind::Subtitles;
    }

    return Kind::Captions;
}

bool InbandTextTrackPrivateAVFObjC::isClosedCaptions() const
{
    if (!m_mediaSelectionOption)
        return false;
    
    return [[m_mediaSelectionOption mediaType] isEqualToString:AVMediaTypeClosedCaption];
}

bool InbandTextTrackPrivateAVFObjC::isSDH() const
{
    if (!m_mediaSelectionOption)
        return false;
    
    if (![[m_mediaSelectionOption mediaType] isEqualToString:AVMediaTypeSubtitle])
        return false;

    if ([m_mediaSelectionOption hasMediaCharacteristic:AVMediaCharacteristicTranscribesSpokenDialogForAccessibility] && [m_mediaSelectionOption hasMediaCharacteristic:AVMediaCharacteristicDescribesMusicAndSoundForAccessibility])
        return true;

    return false;
}
    
bool InbandTextTrackPrivateAVFObjC::containsOnlyForcedSubtitles() const
{
    if (!m_mediaSelectionOption)
        return false;
    
    return [m_mediaSelectionOption hasMediaCharacteristic:AVMediaCharacteristicContainsOnlyForcedSubtitles];
}

bool InbandTextTrackPrivateAVFObjC::isMainProgramContent() const
{
    if (!m_mediaSelectionOption)
        return false;
    
    return [m_mediaSelectionOption hasMediaCharacteristic:AVMediaCharacteristicIsMainProgramContent];
}

bool InbandTextTrackPrivateAVFObjC::isEasyToRead() const
{
    if (!m_mediaSelectionOption)
        return false;

    return [m_mediaSelectionOption hasMediaCharacteristic:AVMediaCharacteristicEasyToRead];
}

AtomString InbandTextTrackPrivateAVFObjC::label() const
{
    if (!m_mediaSelectionOption)
        return emptyAtom();

    NSString *title = 0;

    NSArray *titles = [PAL::getAVMetadataItemClass() metadataItemsFromArray:[m_mediaSelectionOption commonMetadata] withKey:AVMetadataCommonKeyTitle keySpace:AVMetadataKeySpaceCommon];
    if ([titles count]) {
        // If possible, return a title in one of the user's preferred languages.
        NSArray *titlesForPreferredLanguages = [PAL::getAVMetadataItemClass() metadataItemsFromArray:titles filteredAndSortedAccordingToPreferredLanguages:[NSLocale preferredLanguages]];
        if ([titlesForPreferredLanguages count])
            title = [[titlesForPreferredLanguages objectAtIndex:0] stringValue];

        if (!title)
            title = [[titles objectAtIndex:0] stringValue];
    }

    return title ? AtomString(title) : emptyAtom();
}

AtomString InbandTextTrackPrivateAVFObjC::language() const
{
    if (!m_mediaSelectionOption)
        return emptyAtom();

    return [[m_mediaSelectionOption locale] localeIdentifier];
}

bool InbandTextTrackPrivateAVFObjC::isDefault() const
{
    return false;
}

} // namespace WebCore

#endif
