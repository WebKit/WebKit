/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "InbandTextTrack.h"
#include "TextTrackCueGeneric.h"
#include "WebVTTParser.h"

namespace WebCore {

class GenericTextTrackCueMap {
public:
    void add(GenericCueData&, TextTrackCueGeneric&);

    void remove(TextTrackCue&);
    void remove(GenericCueData&);

    GenericCueData* find(TextTrackCue&);
    TextTrackCueGeneric* find(GenericCueData&);

private:
    using CueToDataMap = HashMap<RefPtr<TextTrackCue>, RefPtr<GenericCueData>>;
    using CueDataToCueMap = HashMap<RefPtr<GenericCueData>, RefPtr<TextTrackCueGeneric>>;

    CueToDataMap m_cueToDataMap;
    CueDataToCueMap m_dataToCueMap;
};

class InbandGenericTextTrack final : public InbandTextTrack, private WebVTTParserClient {
    WTF_MAKE_ISO_ALLOCATED(InbandGenericTextTrack);
public:
    static Ref<InbandGenericTextTrack> create(ScriptExecutionContext&, TextTrackClient&, InbandTextTrackPrivate&);
    virtual ~InbandGenericTextTrack();

private:
    InbandGenericTextTrack(ScriptExecutionContext&, TextTrackClient&, InbandTextTrackPrivate&);

    void addGenericCue(GenericCueData&) final;
    void updateGenericCue(GenericCueData&) final;
    void removeGenericCue(GenericCueData&) final;
    ExceptionOr<void> removeCue(TextTrackCue&) final;

    void updateCueFromCueData(TextTrackCueGeneric&, GenericCueData&);

    WebVTTParser& parser();
    void parseWebVTTCueData(const ISOWebVTTCue&) final;
    void parseWebVTTFileHeader(String&&) final;

    void newCuesParsed() final;
    void newRegionsParsed() final;
    void newStyleSheetsParsed() final;
    void fileFailedToParse() final;

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "InbandGenericTextTrack"; }
#endif

    GenericTextTrackCueMap m_cueMap;
    std::unique_ptr<WebVTTParser> m_webVTTParser;
};

} // namespace WebCore

#endif
