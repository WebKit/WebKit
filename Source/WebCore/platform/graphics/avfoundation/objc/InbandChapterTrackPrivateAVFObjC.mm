/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "InbandChapterTrackPrivateAVFObjC.h"

#if ENABLE(VIDEO) && (USE(AVFOUNDATION) || PLATFORM(IOS_FAMILY))

#import "ISOVTTCue.h"
#import "InbandTextTrackPrivateClient.h"
#import <AVFoundation/AVMetadataItem.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/WTFString.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

InbandChapterTrackPrivateAVFObjC::InbandChapterTrackPrivateAVFObjC(RetainPtr<NSLocale> locale)
    : InbandTextTrackPrivate(CueFormat::WebVTT)
    , m_locale(WTFMove(locale))
{
    setMode(Mode::Hidden);
}

void InbandChapterTrackPrivateAVFObjC::processChapters(RetainPtr<NSArray<AVTimedMetadataGroup *>> chapters)
{
    if (!client())
        return;

    auto identifier = LOGIDENTIFIER;
    auto createChapterCue = ([this, identifier] (AVMetadataItem *item, int chapterNumber) mutable {
        if (!client())
            return;
        ChapterData chapterData = { PAL::toMediaTime([item time]), PAL::toMediaTime([item duration]), [item stringValue] };
        if (m_processedChapters.contains(chapterData))
            return;
        m_processedChapters.append(chapterData);

        ISOWebVTTCue cueData = ISOWebVTTCue(PAL::toMediaTime([item time]), PAL::toMediaTime([item duration]), AtomString::number(chapterNumber), [item stringValue]);
        INFO_LOG(identifier, "created cue ", cueData);
        client()->parseWebVTTCueData(WTFMove(cueData));
    });

    int chapterNumber = 0;
    for (AVTimedMetadataGroup *chapter in chapters.get()) {
        for (AVMetadataItem *item in [chapter items]) {
            ++chapterNumber;
            if ([item statusOfValueForKey:@"value" error:nil] == AVKeyValueStatusLoaded)
                createChapterCue(item, chapterNumber);
            else {
                [item loadValuesAsynchronouslyForKeys:@[@"value"] completionHandler:[this, protectedThis = Ref { *this }, item = retainPtr(item), createChapterCue, chapterNumber, identifier] () mutable {

                    NSError *error = nil;
                    auto keyStatus = [item statusOfValueForKey:@"value" error:&error];
                    if (error)
                        ERROR_LOG(identifier, "@\"value\" failed failed to load, status is ", (int)keyStatus, ", error = ", error);

                    if (keyStatus == AVKeyValueStatusLoaded && !error) {
                        callOnMainThread([item = WTFMove(item), protectedThis = WTFMove(protectedThis), createChapterCue = WTFMove(createChapterCue), chapterNumber] () mutable {
                            createChapterCue(item.get(), chapterNumber);
                        });
                    }
                }];
            }
        }
    }
}

AtomString InbandChapterTrackPrivateAVFObjC::language() const
{
    if (!m_language.isEmpty())
        return m_language;

    m_language = [m_locale localeIdentifier];
    return m_language;
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && (USE(AVFOUNDATION) || PLATFORM(IOS_FAMILY))
