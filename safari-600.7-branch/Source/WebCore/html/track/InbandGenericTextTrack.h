/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#ifndef InbandGenericTextTrack_h
#define InbandGenericTextTrack_h

#if ENABLE(VIDEO_TRACK)

#include "InbandTextTrack.h"
#include "TextTrackCueGeneric.h"
#include "WebVTTParser.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class Document;
class InbandTextTrackPrivate;
class TextTrackCue;

class GenericTextTrackCueMap {
public:
    GenericTextTrackCueMap();
    virtual ~GenericTextTrackCueMap();

    void add(GenericCueData*, TextTrackCueGeneric*);

    void remove(TextTrackCue*);
    void remove(GenericCueData*);

    PassRefPtr<GenericCueData> find(TextTrackCue*);
    PassRefPtr<TextTrackCueGeneric> find(GenericCueData*);

private:
    typedef HashMap<RefPtr<TextTrackCue>, RefPtr<GenericCueData>> CueToDataMap;
    typedef HashMap<RefPtr<GenericCueData>, RefPtr<TextTrackCueGeneric>> CueDataToCueMap;

    CueToDataMap m_cueToDataMap;
    CueDataToCueMap m_dataToCueMap;
};

class InbandGenericTextTrack : public InbandTextTrack, private WebVTTParserClient {
public:
    static PassRefPtr<InbandGenericTextTrack> create(ScriptExecutionContext*, TextTrackClient*, PassRefPtr<InbandTextTrackPrivate>);
    virtual ~InbandGenericTextTrack();

private:
    InbandGenericTextTrack(ScriptExecutionContext*, TextTrackClient*, PassRefPtr<InbandTextTrackPrivate>);

    virtual void addGenericCue(InbandTextTrackPrivate*, PassRefPtr<GenericCueData>) override;
    virtual void updateGenericCue(InbandTextTrackPrivate*, GenericCueData*) override;
    virtual void removeGenericCue(InbandTextTrackPrivate*, GenericCueData*) override;
    virtual void removeCue(TextTrackCue*, ExceptionCode&) override;

    PassRefPtr<TextTrackCueGeneric> createCue(PassRefPtr<GenericCueData>);
    void updateCueFromCueData(TextTrackCueGeneric*, GenericCueData*);

    WebVTTParser& parser();
    virtual void parseWebVTTCueData(InbandTextTrackPrivate*, const ISOWebVTTCue&) override;
    virtual void parseWebVTTFileHeader(InbandTextTrackPrivate*, String) override;

    virtual void newCuesParsed() override;
#if ENABLE(WEBVTT_REGIONS)
    virtual void newRegionsParsed() override;
#endif
    virtual void fileFailedToParse() override;

    GenericTextTrackCueMap m_cueMap;
    std::unique_ptr<WebVTTParser> m_webVTTParser;
};

} // namespace WebCore

#endif
#endif
