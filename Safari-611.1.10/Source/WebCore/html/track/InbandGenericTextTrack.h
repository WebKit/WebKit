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

#pragma once

#if ENABLE(VIDEO)

#include "InbandTextTrack.h"
#include "TextTrackCueGeneric.h"
#include "WebVTTParser.h"

namespace WebCore {

class GenericTextTrackCueMap {
public:
    void add(InbandGenericCueIdentifier, TextTrackCueGeneric&);

    void remove(TextTrackCue&);
    void remove(InbandGenericCueIdentifier);

    TextTrackCueGeneric* find(InbandGenericCueIdentifier);

private:
    using CueToDataMap = HashMap<TextTrackCue*, InbandGenericCueIdentifier>;
    using CueDataToCueMap = HashMap<InbandGenericCueIdentifier, RefPtr<TextTrackCueGeneric>>;

    CueToDataMap m_cueToDataMap;
    CueDataToCueMap m_dataToCueMap;
};

class InbandGenericTextTrack final : public InbandTextTrack, private WebVTTParserClient {
    WTF_MAKE_ISO_ALLOCATED(InbandGenericTextTrack);
public:
    static Ref<InbandGenericTextTrack> create(Document&, TextTrackClient&, InbandTextTrackPrivate&);
    virtual ~InbandGenericTextTrack();

private:
    InbandGenericTextTrack(Document&, TextTrackClient&, InbandTextTrackPrivate&);

    void addGenericCue(InbandGenericCue&) final;
    void updateGenericCue(InbandGenericCue&) final;
    void removeGenericCue(InbandGenericCue&) final;
    ExceptionOr<void> removeCue(TextTrackCue&) final;

    void updateCueFromCueData(TextTrackCueGeneric&, InbandGenericCue&);

    WebVTTParser& parser();
    void parseWebVTTCueData(ISOWebVTTCue&&) final;
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
