/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef InbandWebVTTTextTrack_h
#define InbandWebVTTTextTrack_h

#if ENABLE(VIDEO_TRACK)

#include "InbandTextTrack.h"
#include "WebVTTParser.h"
#include <memory>
#include <wtf/RefPtr.h>

namespace WebCore {

class InbandWebVTTTextTrack : public InbandTextTrack, private WebVTTParserClient {
public:
    static PassRefPtr<InbandTextTrack> create(ScriptExecutionContext*, TextTrackClient*, PassRefPtr<InbandTextTrackPrivate>);
    virtual ~InbandWebVTTTextTrack();

private:
    InbandWebVTTTextTrack(ScriptExecutionContext*, TextTrackClient*, PassRefPtr<InbandTextTrackPrivate>);

    WebVTTParser& parser();
    virtual void parseWebVTTCueData(InbandTextTrackPrivate*, const char* data, unsigned length) override;
    virtual void parseWebVTTCueData(InbandTextTrackPrivate*, const ISOWebVTTCue&) override;

    virtual void newCuesParsed() override;
#if ENABLE(WEBVTT_REGIONS)
    virtual void newRegionsParsed() override;
#endif
    virtual void fileFailedToParse() override;

    std::unique_ptr<WebVTTParser> m_webVTTParser;
};

} // namespace WebCore

#endif
#endif
