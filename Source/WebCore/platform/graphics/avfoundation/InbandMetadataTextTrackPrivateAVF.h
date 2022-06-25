/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

#ifndef InbandMetadataTextTrackPrivateAVF_h
#define InbandMetadataTextTrackPrivateAVF_h

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
#include "InbandTextTrackPrivate.h"

namespace WebCore {

#if ENABLE(DATACUE_VALUE)
struct IncompleteMetaDataCue {
    RefPtr<SerializedPlatformDataCue> cueData;
    MediaTime startTime;
};
#endif

class InbandMetadataTextTrackPrivateAVF : public InbandTextTrackPrivate {
public:
    static Ref<InbandMetadataTextTrackPrivateAVF> create(Kind, CueFormat, const AtomString& id = emptyAtom());

    ~InbandMetadataTextTrackPrivateAVF();

    Kind kind() const override { return m_kind; }
    AtomString id() const override { return m_id; }
    AtomString inBandMetadataTrackDispatchType() const override { return m_inBandMetadataTrackDispatchType; }
    void setInBandMetadataTrackDispatchType(const AtomString& value) { m_inBandMetadataTrackDispatchType = value; }

#if ENABLE(DATACUE_VALUE)
    void addDataCue(const MediaTime& start, const MediaTime& end, Ref<SerializedPlatformDataCue>&&, const String&);
    void updatePendingCueEndTimes(const MediaTime&);
#endif

    void flushPartialCues();

private:
    InbandMetadataTextTrackPrivateAVF(Kind, CueFormat, const AtomString&);

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "InbandMetadataTextTrackPrivateAVF"; }
#endif

    Kind m_kind;
    AtomString m_id;
    AtomString m_inBandMetadataTrackDispatchType;
    MediaTime m_currentCueStartTime;
#if ENABLE(DATACUE_VALUE)
    Vector<IncompleteMetaDataCue> m_incompleteCues;
#endif
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)

#endif // InbandMetadataTextTrackPrivateAVF_h
