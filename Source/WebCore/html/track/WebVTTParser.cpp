/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#include "config.h"

#if ENABLE(VIDEO_TRACK)

#include "WebVTTParser.h"

#include "HTMLElement.h"
#include "ProcessingInstruction.h"
#include "SegmentedString.h"
#include "Text.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

const double secondsPerHour = 3600;
const double secondsPerMinute = 60;
const double secondsPerMillisecond = 0.001;
const double malformedTime = -1;
const unsigned bomLength = 3;
const unsigned fileIdentifierLength = 6;

String WebVTTParser::collectDigits(const String& input, unsigned* position)
{
    StringBuilder digits;
    while (*position < input.length() && isASCIIDigit(input[*position]))
        digits.append(input[(*position)++]);
    return digits.toString();
}

String WebVTTParser::collectWord(const String& input, unsigned* position)
{
    StringBuilder string;
    while (*position < input.length() && !isASpace(input[*position]))
        string.append(input[(*position)++]);
    return string.toString();
}

WebVTTParser::WebVTTParser(WebVTTParserClient* client, ScriptExecutionContext* context)
    : m_scriptExecutionContext(context)
    , m_state(Initial)
    , m_currentStartTime(0)
    , m_currentEndTime(0)
    , m_tokenizer(WebVTTTokenizer::create())
    , m_client(client)
{
}

void WebVTTParser::getNewCues(Vector<RefPtr<TextTrackCue> >& outputCues)
{
    outputCues = m_cuelist;
    m_cuelist.clear();
}

void WebVTTParser::parseBytes(const char* data, unsigned length)
{
    // 4.8.10.13.3 WHATWG WebVTT Parser algorithm.
    // 1-3 - Initial setup.
    unsigned position = 0;

    while (position < length) {
        String line = collectNextLine(data, length, &position);
        
        switch (m_state) {
        case Initial:
            // Buffer up at least 9 bytes before proceeding with checking for the file identifier.
            m_identifierData.append(data, length);
            if (m_identifierData.size() < bomLength + fileIdentifierLength)
                return;

            // 4-12 - Collect the first line and check for "WEBVTT".
            if (!hasRequiredFileIdentifier()) {
                if (m_client)
                    m_client->fileFailedToParse();
                return;
            }

            m_state = Header;
            m_identifierData.clear();
            break;
        
        case Header:
            // 13-18 - Allow a header (comment area) under the WEBVTT line.
            if (line.isEmpty())
                m_state = Id;
            break;
        
        case Id:
            // 19-29 - Allow any number of line terminators, then initialize new cue values.
            if (line.isEmpty())
                break;
            resetCueValues();
            
            // 30-39 - Check if this line contains an optional identifier or timing data.
            m_state = collectCueId(line);
            break;
        
        case TimingsAndSettings:
            // 40 - Collect cue timings and settings.
            m_state = collectTimingsAndSettings(line);
            break;
        
        case CueText:
            // 41-53 - Collect the cue text, create a cue, and add it to the output.
            m_state = collectCueText(line, length, position);
            break;

        case BadCue:
            // 54-62 - Collect and discard the remaining cue.
            m_state = ignoreBadCue(line);
            break;
        }
    }
}

bool WebVTTParser::hasRequiredFileIdentifier()
{
    // A WebVTT file identifier consists of an optional BOM character,
    // the string "WEBVTT" followed by an optional space or tab character,
    // and any number of characters that are not line terminators ...
    unsigned position = 0;
    if (m_identifierData.size() >= bomLength && m_identifierData[0] == '\xEF' && m_identifierData[1] == '\xBB' && m_identifierData[2] == '\xBF')
        position += bomLength;
    String line = collectNextLine(m_identifierData.data(), m_identifierData.size(), &position);

    if (line.length() < fileIdentifierLength)
        return false;
    if (line.substring(0, fileIdentifierLength) != "WEBVTT")
        return false;
    if (line.length() > fileIdentifierLength && line[fileIdentifierLength] != ' ' && line[fileIdentifierLength] != '\t')
        return false;

    return true;
}

WebVTTParser::ParseState WebVTTParser::collectCueId(const String& line)
{
    if (line.contains("-->"))
        return collectTimingsAndSettings(line);
    m_currentId = line;
    return TimingsAndSettings;
}

WebVTTParser::ParseState WebVTTParser::collectTimingsAndSettings(const String& line)
{
    // 4.8.10.13.3 Collect WebVTT cue timings and settings.
    // 1-3 - Let input be the string being parsed and position be a pointer into input
    unsigned position = 0;
    skipWhiteSpace(line, &position);

    // 4-5 - Collect a WebVTT timestamp. If that fails, then abort and return failure. Otherwise, let cue's text track cue start time be the collected time.
    m_currentStartTime = collectTimeStamp(line, &position);
    if (m_currentStartTime == malformedTime)
        return BadCue;
    if (position >= line.length())
        return BadCue;
    char nextChar = line[position++];
    if (nextChar != ' ' && nextChar != '\t')
        return BadCue;
    skipWhiteSpace(line, &position);

    // 6-9 - If the next three characters are not "-->", abort and return failure.
    if (line.find("-->", position) == notFound)
        return BadCue;
    position += 3;
    if (position >= line.length())
        return BadCue;
    nextChar = line[position++];
    if (nextChar != ' ' && nextChar != '\t')
        return BadCue;
    skipWhiteSpace(line, &position);

    // 10-11 - Collect a WebVTT timestamp. If that fails, then abort and return failure. Otherwise, let cue's text track cue end time be the collected time.
    m_currentEndTime = collectTimeStamp(line, &position);
    if (m_currentEndTime == malformedTime)
        return BadCue;
    skipWhiteSpace(line, &position);

    // 12 - Parse the WebVTT settings for the cue (conducted in TextTrackCue).
    m_currentSettings = line.substring(position, line.length()-1);
    return CueText;
}

WebVTTParser::ParseState WebVTTParser::collectCueText(const String& line, unsigned length, unsigned position)
{
    if (line.isEmpty()) {
        createNewCue();
        return Id;
    }
    if (!m_currentContent.isEmpty())
        m_currentContent.append("\n");
    m_currentContent.append(line);

    if (position >= length)
        createNewCue();
                
    return CueText;
}

WebVTTParser::ParseState WebVTTParser::ignoreBadCue(const String& line)
{
    if (!line.isEmpty())
        return BadCue;
    return Id;
}

PassRefPtr<DocumentFragment>  WebVTTParser::createDocumentFragmentFromCueText(const String& text)
{
    // Cue text processing based on
    // 4.8.10.13.4 WebVTT cue text parsing rules and
    // 4.8.10.13.5 WebVTT cue text DOM construction rules.

    if (!text.length())
        return 0;

    ASSERT(m_scriptExecutionContext->isDocument());
    Document* document = static_cast<Document*>(m_scriptExecutionContext);
    
    RefPtr<DocumentFragment> fragment = DocumentFragment::create(document);
    m_currentNode = fragment;
    m_tokenizer->reset();
    m_token.clear();
    
    SegmentedString content(text);
    while (m_tokenizer->nextToken(content, m_token))
        constructTreeFromToken(document);
    
    return fragment.release();
}

void WebVTTParser::createNewCue()
{
    if (!m_currentContent.length())
        return;

    RefPtr<TextTrackCue> cue = TextTrackCue::create(m_scriptExecutionContext, m_currentStartTime, m_currentEndTime, m_currentContent.toString());
    cue->setId(m_currentId);
    cue->setCueSettings(m_currentSettings);

    m_cuelist.append(cue);
    if (m_client)
        m_client->newCuesParsed();
}

void WebVTTParser::resetCueValues()
{
    m_currentId = emptyString();
    m_currentSettings = emptyString();
    m_currentStartTime = 0;
    m_currentEndTime = 0;
    m_currentContent.clear();
}

double WebVTTParser::collectTimeStamp(const String& line, unsigned* position)
{
    // 4.8.10.13.3 Collect a WebVTT timestamp.
    // 1-4 - Initial checks, let most significant units be minutes.
    enum Mode { minutes, hours };
    Mode mode = minutes;
    if (*position >= line.length() || !isASCIIDigit(line[*position]))
        return malformedTime;

    // 5-6 - Collect a sequence of characters that are 0-9.
    String digits1 = collectDigits(line, position);
    int value1 = digits1.toInt();

    // 7 - If not 2 characters or value is greater than 59, interpret as hours.
    if (digits1.length() != 2 || value1 > 59)
        mode = hours;

    // 8-12 - Collect the next sequence of 0-9 after ':' (must be 2 chars).
    if (*position >= line.length() || line[(*position)++] != ':')
        return malformedTime;
    if (*position >= line.length() || !isASCIIDigit(line[(*position)]))
        return malformedTime;
    String digits2 = collectDigits(line, position);
    int value2 = digits2.toInt();
    if (digits2.length() != 2) 
        return malformedTime;

    // 13 - Detect whether this timestamp includes hours.
    int value3;
    if (mode == hours || (*position < line.length() && line[*position] == ':')) {
        if (*position >= line.length() || line[(*position)++] != ':')
            return malformedTime;
        if (*position >= line.length() || !isASCIIDigit(line[*position]))
            return malformedTime;
        String digits3 = collectDigits(line, position);
        if (digits3.length() != 2)
            return malformedTime;
        value3 = digits3.toInt();
    } else {
        value3 = value2;
        value2 = value1;
        value1 = 0;
    }

    // 14-19 - Collect next sequence of 0-9 after '.' (must be 3 chars).
    if (*position >= line.length() || line[(*position)++] != '.')
        return malformedTime;
    if (*position >= line.length() || !isASCIIDigit(line[*position]))
        return malformedTime;
    String digits4 = collectDigits(line, position);
    if (digits4.length() != 3)
        return malformedTime;
    int value4 = digits4.toInt();
    if (value2 > 59 || value3 > 59)
        return malformedTime;

    // 20-21 - Calculate result.
    return (value1 * secondsPerHour) + (value2 * secondsPerMinute) + value3 + (value4 * secondsPerMillisecond);
}

void WebVTTParser::constructTreeFromToken(Document* document)
{
    AtomicString tokenTagName(m_token.name().data(), m_token.name().size());
    QualifiedName tagName(nullAtom, tokenTagName, xhtmlNamespaceURI);

    // http://dev.w3.org/html5/webvtt/#webvtt-cue-text-dom-construction-rules
    
    switch (m_token.type()) {
    case WebVTTTokenTypes::Character: {
        String content(m_token.characters().data(), m_token.characters().size());
        RefPtr<Text> child = Text::create(document, content);
        m_currentNode->parserAppendChild(child);
        break;
    }
    case WebVTTTokenTypes::StartTag: {
        RefPtr<HTMLElement> child;
        if (isRecognizedTag(tokenTagName))
            child = HTMLElement::create(tagName, document);
        else if (m_token.name().size() == 1 && m_token.name()[0] == 'c')
            child = HTMLElement::create(spanTag, document);
        else if (m_token.name().size() == 1 && m_token.name()[0] == 'v')
            child = HTMLElement::create(qTag, document);

        if (child) {
            if (m_token.classes().size() > 0)
                child->setAttribute(classAttr, AtomicString(m_token.classes().data(), m_token.classes().size()));
            if (child->hasTagName(qTag))
                child->setAttribute(titleAttr, AtomicString(m_token.annotation().data(), m_token.annotation().size()));
            m_currentNode->parserAppendChild(child);
            m_currentNode = child;
        }
        break;
    }
    case WebVTTTokenTypes::EndTag:
        if (isRecognizedTag(tokenTagName)
            || (m_token.name().size() == 1 && m_token.name()[0] == 'c')
            || (m_token.name().size() == 1 && m_token.name()[0] == 'v')) {
            if (m_currentNode->parentNode())
                m_currentNode = m_currentNode->parentNode();
        }
        break;
    case WebVTTTokenTypes::TimestampTag: {
        unsigned position = 0;
        double time = collectTimeStamp(m_token.characters().data(), &position);
        if (time != malformedTime)
            m_currentNode->parserAppendChild(ProcessingInstruction::create(document, "timestamp", String(m_token.characters().data(), m_token.characters().size())));
        break;
    }
    default:
        break;
    }
    m_token.clear();
}

void WebVTTParser::skipWhiteSpace(const String& line, unsigned* position)
{
    while (*position < line.length() && isASpace(line[*position]))
        (*position)++;
}

void WebVTTParser::skipLineTerminator(const char* data, unsigned length, unsigned* position)
{
    if (*position >= length)
        return;
    if (data[*position] == '\r')
        (*position)++;
    if (*position >= length)
        return;
    if (data[*position] == '\n')
        (*position)++;
}

String WebVTTParser::collectNextLine(const char* data, unsigned length, unsigned* position)
{
    unsigned oldPosition = *position;
    while (*position < length && data[*position] != '\r' && data[*position] != '\n')
        (*position)++;
    String line = String::fromUTF8(data + oldPosition, *position - oldPosition);
    skipLineTerminator(data, length, position);
    return line;
}

}

#endif
