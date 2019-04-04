/*
 * Copyright (C) 2014 Cable Television Labs Inc. All rights reserved.
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(VIDEO_TRACK)

#include "InbandTextTrack.h"

namespace WebCore {

class DataCue;

class InbandDataTextTrack final : public InbandTextTrack {
    WTF_MAKE_ISO_ALLOCATED(InbandDataTextTrack);
public:
    static Ref<InbandDataTextTrack> create(ScriptExecutionContext&, TextTrackClient&, InbandTextTrackPrivate&);
    virtual ~InbandDataTextTrack();

private:
    InbandDataTextTrack(ScriptExecutionContext&, TextTrackClient&, InbandTextTrackPrivate&);

    void addDataCue(const MediaTime& start, const MediaTime& end, const void*, unsigned) final;

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "DataCue"; }
#endif

#if ENABLE(DATACUE_VALUE)
    void addDataCue(const MediaTime& start, const MediaTime& end, Ref<SerializedPlatformRepresentation>&&, const String&) final;
    void updateDataCue(const MediaTime& start, const MediaTime& end, SerializedPlatformRepresentation&) final;
    void removeDataCue(const MediaTime& start, const MediaTime& end, SerializedPlatformRepresentation&) final;
    ExceptionOr<void> removeCue(TextTrackCue&) final;

    HashMap<RefPtr<SerializedPlatformRepresentation>, RefPtr<DataCue>> m_incompleteCueMap;
#endif
};

} // namespace WebCore

#endif
