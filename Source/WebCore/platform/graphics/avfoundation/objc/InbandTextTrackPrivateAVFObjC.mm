/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"

#if ENABLE(VIDEO) && USE(AVFOUNDATION) && HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)

#import "InbandTextTrackPrivateAVFObjC.h"

#import "BlockExceptions.h"
#import "FloatConversion.h"
#import "InbandTextTrackPrivate.h"
#import "InbandTextTrackPrivateAVF.h"
#import "Logging.h"
#import "MediaPlayerPrivateAVFoundationObjC.h"
#import "SoftLinking.h"
#import <AVFoundation/AVFoundation.h>
#import <objc/runtime.h>
#import <wtf/UnusedParam.h>


SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
#define AVPlayer getAVPlayerClass()
#define AVPlayerItem getAVPlayerItemClass()

SOFT_LINK_CLASS(AVFoundation, AVPlayer)
SOFT_LINK_CLASS(AVFoundation, AVPlayerItem)
SOFT_LINK_CLASS(AVFoundation, AVMetadataItem)
SOFT_LINK_CLASS(AVFoundation, AVPlayerItemLegibleOutput)
#define AVMediaCharacteristicVisual getAVMediaCharacteristicVisual()
#define AVMediaCharacteristicAudible getAVMediaCharacteristicAudible()
#define AVMediaTypeClosedCaption getAVMediaTypeClosedCaption()
#define AVMediaTypeVideo getAVMediaTypeVideo()
#define AVMediaTypeAudio getAVMediaTypeAudio()

SOFT_LINK_POINTER(AVFoundation, AVMediaTypeClosedCaption, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicLegible, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMetadataCommonKeyTitle, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMetadataKeySpaceCommon, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeSubtitle, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaCharacteristicContainsOnlyForcedSubtitles, NSString *)
#define AVMetadataItem getAVMetadataItemClass()
#define AVPlayerItemLegibleOutput getAVPlayerItemLegibleOutputClass()
#define AVMediaCharacteristicLegible getAVMediaCharacteristicLegible()
#define AVMetadataCommonKeyTitle getAVMetadataCommonKeyTitle()
#define AVMetadataKeySpaceCommon getAVMetadataKeySpaceCommon()
#define AVMediaTypeSubtitle getAVMediaTypeSubtitle()
#define AVMediaCharacteristicContainsOnlyForcedSubtitles getAVMediaCharacteristicContainsOnlyForcedSubtitles()

using namespace WebCore;
using namespace std;

namespace WebCore {

InbandTextTrackPrivateAVFObjC::InbandTextTrackPrivateAVFObjC(MediaPlayerPrivateAVFoundationObjC* player, AVMediaSelectionOption* selection)
    : InbandTextTrackPrivateAVF(player)
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
        return None;

    NSString *mediaType = [m_mediaSelectionOption mediaType];
    if ([mediaType isEqualToString:AVMediaTypeClosedCaption])
        return Captions;
    if ([mediaType isEqualToString:AVMediaTypeSubtitle])
        return Subtitles;

    return Captions;
}

AtomicString InbandTextTrackPrivateAVFObjC::label() const
{
    if (!m_mediaSelectionOption)
        return emptyAtom;

    NSArray *titles = [AVMetadataItem metadataItemsFromArray:[m_mediaSelectionOption.get() commonMetadata] withKey:AVMetadataCommonKeyTitle keySpace:AVMetadataKeySpaceCommon];
    if ([titles count]) {
        // If possible, return a title in one of the user's preferred languages.
        NSArray *titlesForPreferredLanguages = [AVMetadataItem metadataItemsFromArray:titles filteredAndSortedAccordingToPreferredLanguages:[NSLocale preferredLanguages]];
        if ([titlesForPreferredLanguages count])
            return [[titlesForPreferredLanguages objectAtIndex:0] stringValue];

        return [[titles objectAtIndex:0] stringValue];
    }

    return emptyAtom;
}

AtomicString InbandTextTrackPrivateAVFObjC::language() const
{
    if (!m_mediaSelectionOption)
        return emptyAtom;

    return [[m_mediaSelectionOption.get() locale] localeIdentifier];
}

bool InbandTextTrackPrivateAVFObjC::isDefault() const
{
    return false;
}
   

} // namespace WebCore

#endif
