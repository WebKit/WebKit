/*
 * Copyright (C) 2011, 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2013 Cable Television Labs, Inc.
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO_TRACK)

#include "BufferedLineReader.h"
#include "DocumentFragment.h"
#include "HTMLNames.h"
#include "TextResourceDecoder.h"
#include "VTTRegion.h"
#include <memory>
#include <wtf/MediaTime.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

class Document;
class ISOWebVTTCue;
class VTTScanner;

class WebVTTParserClient {
public:
    virtual ~WebVTTParserClient() = default;

    virtual void newCuesParsed() = 0;
    virtual void newRegionsParsed() = 0;
    virtual void newStyleSheetsParsed() = 0;
    virtual void fileFailedToParse() = 0;
};

class WebVTTCueData final : public RefCounted<WebVTTCueData> {
public:

    static Ref<WebVTTCueData> create() { return adoptRef(*new WebVTTCueData()); }
    ~WebVTTCueData() = default;

    MediaTime startTime() const { return m_startTime; }
    void setStartTime(const MediaTime& startTime) { m_startTime = startTime; }

    MediaTime endTime() const { return m_endTime; }
    void setEndTime(const MediaTime& endTime) { m_endTime = endTime; }

    String id() const { return m_id; }
    void setId(String id) { m_id = id; }

    String content() const { return m_content; }
    void setContent(String content) { m_content = content; }

    String settings() const { return m_settings; }
    void setSettings(String settings) { m_settings = settings; }

    MediaTime originalStartTime() const { return m_originalStartTime; }
    void setOriginalStartTime(const MediaTime& time) { m_originalStartTime = time; }

private:
    WebVTTCueData() = default;

    MediaTime m_startTime;
    MediaTime m_endTime;
    MediaTime m_originalStartTime;
    String m_id;
    String m_content;
    String m_settings;
};

class WebVTTParser final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum ParseState {
        Initial,
        Header,
        Id,
        TimingsAndSettings,
        CueText,
        Region,
        Style,
        BadCue,
        Finished
    };

    WebVTTParser(WebVTTParserClient*, ScriptExecutionContext*);

    static inline bool isRecognizedTag(const AtomString& tagName)
    {
        return tagName == iTag
            || tagName == bTag
            || tagName == uTag
            || tagName == rubyTag
            || tagName == rtTag;
    }

    static inline bool isASpace(UChar c)
    {
        // WebVTT space characters are U+0020 SPACE, U+0009 CHARACTER
        // TABULATION (tab), U+000A LINE FEED (LF), U+000C FORM FEED (FF), and
        // U+000D CARRIAGE RETURN (CR).
        return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r';
    }

    static inline bool isValidSettingDelimiter(UChar c)
    {
        // ... a WebVTT cue consists of zero or more of the following components, in any order, separated from each other by one or more 
        // U+0020 SPACE characters or U+0009 CHARACTER TABULATION (tab) characters.
        return c == ' ' || c == '\t';
    }
    static bool collectTimeStamp(const String&, MediaTime&);

    // Useful functions for parsing percentage settings.
    static bool parseFloatPercentageValue(VTTScanner& valueScanner, float&);
    static bool parseFloatPercentageValuePair(VTTScanner& valueScanner, char, FloatPoint&);

    // Input data to the parser to parse.
    void parseBytes(const char*, unsigned);
    void parseFileHeader(String&&);
    void parseCueData(const ISOWebVTTCue&);
    void flush();
    void fileFinished();

    // Transfers ownership of last parsed cues to caller.
    void getNewCues(Vector<RefPtr<WebVTTCueData>>&);
    void getNewRegions(Vector<RefPtr<VTTRegion>>&);

    Vector<String> getStyleSheets();
    
    // Create the DocumentFragment representation of the WebVTT cue text.
    static Ref<DocumentFragment> createDocumentFragmentFromCueText(Document&, const String&);

protected:
    ScriptExecutionContext* m_scriptExecutionContext;
    ParseState m_state;

private:
    void parse();
    void flushPendingCue();
    bool hasRequiredFileIdentifier(const String&);
    ParseState collectCueId(const String&);
    ParseState collectTimingsAndSettings(const String&);
    ParseState collectCueText(const String&);
    ParseState recoverCue(const String&);
    ParseState ignoreBadCue(const String&);
    ParseState collectRegionSettings(const String&);
    ParseState collectWebVTTBlock(const String&);
    ParseState checkAndRecoverCue(const String& line);
    ParseState collectStyleSheet(const String&);
    bool checkAndCreateRegion(const String& line);
    bool checkAndStoreRegion(const String& line);
    bool checkStyleSheet(const String& line);
    bool checkAndStoreStyleSheet(const String& line);

    void createNewCue();
    void resetCueValues();

    static bool collectTimeStamp(VTTScanner& input, MediaTime& timeStamp);

    BufferedLineReader m_lineReader;
    RefPtr<TextResourceDecoder> m_decoder;
    String m_currentId;
    MediaTime m_currentStartTime;
    MediaTime m_currentEndTime;
    StringBuilder m_currentContent;
    String m_previousLine;
    String m_currentSettings;
    RefPtr<VTTRegion> m_currentRegion;
    String m_currentSourceStyleSheet;
    
    WebVTTParserClient* m_client;

    Vector<RefPtr<WebVTTCueData>> m_cuelist;
    Vector<RefPtr<VTTRegion>> m_regionList;
    Vector<String> m_styleSheets;
};

} // namespace WebCore

#endif // ENABLE(VIDEO_TRACK)
