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

#pragma once

#if ENABLE(VIDEO) && (USE(AVFOUNDATION) || PLATFORM(IOS_FAMILY))

#include "InbandTextTrackPrivate.h"
#include <wtf/HashSet.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVTimedMetadataGroup;
OBJC_CLASS NSLocale;

namespace WebCore {

class InbandChapterTrackPrivateAVFObjC : public InbandTextTrackPrivate {
public:
    static Ref<InbandChapterTrackPrivateAVFObjC> create(RetainPtr<NSLocale> locale, TrackID trackID)
    {
        return adoptRef(*new InbandChapterTrackPrivateAVFObjC(WTFMove(locale), trackID));
    }

    virtual ~InbandChapterTrackPrivateAVFObjC() = default;

    TrackID id() const final { return m_id; }
    InbandTextTrackPrivate::Kind kind() const final { return InbandTextTrackPrivate::Kind::Chapters; }
    AtomString language() const final;

    int trackIndex() const final { return m_index; }
    void setTextTrackIndex(int index) { m_index = index; }

    void processChapters(RetainPtr<NSArray<AVTimedMetadataGroup *>>);

private:
    InbandChapterTrackPrivateAVFObjC(RetainPtr<NSLocale>, TrackID);

    AtomString inBandMetadataTrackDispatchType() const final { return "com.apple.chapters"_s; }
    const char* logClassName() const final { return "InbandChapterTrackPrivateAVFObjC"; }

    struct ChapterData {
        MediaTime m_startTime;
        MediaTime m_duration;
        String m_title;

        friend bool operator==(const ChapterData&, const ChapterData&) = default;
    };

    Vector<ChapterData> m_processedChapters;
    RetainPtr<NSLocale> m_locale;
    mutable AtomString m_language;
    const TrackID m_id;
    int m_index { 0 };
};

} // namespace WebCore

#endif //  ENABLE(VIDEO) && (USE(AVFOUNDATION) || PLATFORM(IOS_FAMILY))
