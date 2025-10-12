/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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

#import "AVTrackPrivateAVFObjCImpl.h"
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
#import <wtf/TZoneMallocInlines.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

@class AVMediaSelectionOption;
@interface AVMediaSelectionOption (WebKitInternal)
- (id)optionID;
@end

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(InbandTextTrackPrivateAVFObjC);

InbandTextTrackPrivateAVFObjC::InbandTextTrackPrivateAVFObjC(AVMediaSelectionGroup *group, AVMediaSelectionOption *selection, TrackID trackID, InbandTextTrackPrivate::CueFormat format, ModeChangedCallback&& callback)
    : InbandTextTrackPrivateAVF(trackID, format, WTFMove(callback))
    , m_mediaSelectionGroup(group)
    , m_mediaSelectionOption(selection)
{
}

InbandTextTrackPrivateAVFObjC::InbandTextTrackPrivateAVFObjC(AVAssetTrack* track, TrackID trackID, InbandTextTrackPrivate::CueFormat format)
    : InbandTextTrackPrivateAVF(trackID, format, [] { })
    , m_assetTrack(track)
{
}

void InbandTextTrackPrivateAVFObjC::disconnect()
{
    InbandTextTrackPrivateAVF::disconnect();
    m_mediaSelectionGroup = 0;
    m_mediaSelectionOption = 0;
    m_assetTrack = nullptr;
}

String InbandTextTrackPrivateAVFObjC::mediaType() const
{
    if (m_mediaSelectionOption)
        return [m_mediaSelectionOption mediaType];

    if (m_assetTrack)
        return [m_assetTrack mediaType];

    return emptyString();
}

bool InbandTextTrackPrivateAVFObjC::hasMediaCharacteristic(const String& type) const
{
    if (m_mediaSelectionOption)
        return [m_mediaSelectionOption hasMediaCharacteristic:type.createNSString().get()];

    if (m_assetTrack)
        return [m_assetTrack hasMediaCharacteristic:type.createNSString().get()];

    return false;
}

InbandTextTrackPrivate::Kind InbandTextTrackPrivateAVFObjC::kind() const
{
    if (m_mediaSelectionOption)
        return AVTrackPrivateAVFObjCImpl::textKindForAVMediaSelectionOption(m_mediaSelectionOption.get());

    if (m_assetTrack)
        return AVTrackPrivateAVFObjCImpl::textKindForAVAssetTrack(m_assetTrack.get());

    return Kind::None;
}

bool InbandTextTrackPrivateAVFObjC::isClosedCaptions() const
{
    return mediaType() == String(AVMediaTypeClosedCaption);
}

bool InbandTextTrackPrivateAVFObjC::isSDH() const
{
    if (mediaType() != String(AVMediaTypeSubtitle))
        return false;

    if (hasMediaCharacteristic(AVMediaCharacteristicTranscribesSpokenDialogForAccessibility) && hasMediaCharacteristic(AVMediaCharacteristicDescribesMusicAndSoundForAccessibility))
        return true;

    return false;
}
    
bool InbandTextTrackPrivateAVFObjC::containsOnlyForcedSubtitles() const
{
    return hasMediaCharacteristic(AVMediaCharacteristicContainsOnlyForcedSubtitles);
}

bool InbandTextTrackPrivateAVFObjC::isMainProgramContent() const
{
    return hasMediaCharacteristic(AVMediaCharacteristicIsMainProgramContent);
}

bool InbandTextTrackPrivateAVFObjC::isEasyToRead() const
{
    return hasMediaCharacteristic(AVMediaCharacteristicEasyToRead);
}

AtomString InbandTextTrackPrivateAVFObjC::label() const
{
    NSArray<AVMetadataItem*>* commonMetadata = nil;
    if (m_mediaSelectionOption)
        commonMetadata = [m_mediaSelectionOption commonMetadata];
    else if (m_assetTrack)
        commonMetadata = [m_assetTrack commonMetadata];
    if (!commonMetadata)
        return emptyAtom();

    NSString *title = 0;
    NSArray *titles = [PAL::getAVMetadataItemClassSingleton() metadataItemsFromArray:commonMetadata withKey:AVMetadataCommonKeyTitle keySpace:AVMetadataKeySpaceCommon];
    if ([titles count]) {
        // If possible, return a title in one of the user's preferred languages.
        NSArray *titlesForPreferredLanguages = [PAL::getAVMetadataItemClassSingleton() metadataItemsFromArray:titles filteredAndSortedAccordingToPreferredLanguages:[NSLocale preferredLanguages]];
        if ([titlesForPreferredLanguages count])
            title = [[titlesForPreferredLanguages objectAtIndex:0] stringValue];

        if (!title)
            title = [[titles objectAtIndex:0] stringValue];
    }

    return title ? AtomString(title) : emptyAtom();
}

AtomString InbandTextTrackPrivateAVFObjC::language() const
{
    if (m_mediaSelectionOption)
        return AtomString(AVTrackPrivateAVFObjCImpl::languageForAVMediaSelectionOption(m_mediaSelectionOption.get()));

    if (m_assetTrack)
        return AtomString(AVTrackPrivateAVFObjCImpl::languageForAVAssetTrack(m_assetTrack.get()));

    return emptyAtom();
}

bool InbandTextTrackPrivateAVFObjC::isDefault() const
{
    if (m_mediaSelectionGroup && m_mediaSelectionOption)
        return [m_mediaSelectionGroup defaultOption] == m_mediaSelectionOption.get();

    return false;
}

} // namespace WebCore

#endif
