/*
 * Copyright (C) 2014 Cable Television Labs Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef InbandDataTextTrack_h
#define InbandDataTextTrack_h

#if ENABLE(VIDEO_TRACK)

#include "InbandTextTrack.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class DataCue;
class Document;
class InbandTextTrackPrivate;

#if ENABLE(DATACUE_VALUE)
class SerializedPlatformRepresentation;
#endif

class InbandDataTextTrack : public InbandTextTrack {
public:
    static PassRefPtr<InbandDataTextTrack> create(ScriptExecutionContext*, TextTrackClient*, PassRefPtr<InbandTextTrackPrivate>);
    virtual ~InbandDataTextTrack();

private:
    InbandDataTextTrack(ScriptExecutionContext*, TextTrackClient*, PassRefPtr<InbandTextTrackPrivate>);

    virtual void addDataCue(InbandTextTrackPrivate*, const MediaTime& start, const MediaTime& end, const void*, unsigned) override;

#if ENABLE(DATACUE_VALUE)
    virtual void addDataCue(InbandTextTrackPrivate*, const MediaTime& start, const MediaTime& end, PassRefPtr<SerializedPlatformRepresentation>, const String&) override;
    virtual void updateDataCue(InbandTextTrackPrivate*, const MediaTime& start, const MediaTime& end, PassRefPtr<SerializedPlatformRepresentation>) override;
    virtual void removeDataCue(InbandTextTrackPrivate*, const MediaTime& start, const MediaTime& end, PassRefPtr<SerializedPlatformRepresentation>) override;
    virtual void removeCue(TextTrackCue*, ExceptionCode&) override;

    HashMap<RefPtr<SerializedPlatformRepresentation>, RefPtr<DataCue>> m_incompleteCueMap;
#endif
};

} // namespace WebCore

#endif
#endif
