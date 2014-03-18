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

#include "config.h"

#if ENABLE(VIDEO_TRACK)

#include "WebVTTParser.h"

#include "ProcessingInstruction.h"
#include "Text.h"
#include "WebVTTElement.h"

namespace WebCore {

const double secondsPerHour = 3600;
const double secondsPerMinute = 60;
const double secondsPerMillisecond = 0.001;
const double malformedTime = -1;
const UChar bom = 0xFEFF;
const char* fileIdentifier = "WEBVTT";
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

#if ENABLE(WEBVTT_REGIONS)
float WebVTTParser::parseFloatPercentageValue(const String& value, bool& isValidSetting)
{
    // '%' must be present and at the end of the setting value.
    if (value.find('%', 1) != value.length() - 1) {
        isValidSetting = false;
        return 0;
    }

    unsigned position = 0;

    StringBuilder floatNumberAsString;
    floatNumberAsString.append(WebVTTParser::collectDigits(value, &position));

    if (value[position] == '.') {
        floatNumberAsString.append(".");
        position++;

        floatNumberAsString.append(WebVTTParser::collectDigits(value, &position));
    }
    float number = floatNumberAsString.toString().toFloat(&isValidSetting);

    if (isValidSetting && (number <= 0 || number >= 100))
        isValidSetting = false;

    return number;
}

FloatPoint WebVTTParser::parseFloatPercentageValuePair(const String& value, char delimiter, bool& isValidSetting)
{
    // The delimiter can't be the first or second value because a pair of
    // percentages (x%,y%) implies that at least the first two characters
    // are the first percentage value.
    size_t delimiterOffset = value.find(delimiter, 2);
    if (delimiterOffset == notFound || delimiterOffset == value.length() - 1) {
        isValidSetting = false;
        return FloatPoint(0, 0);
    }

    bool isFirstValueValid;
    float firstCoord = parseFloatPercentageValue(value.substring(0, delimiterOffset), isFirstValueValid);

    bool isSecondValueValid;
    float secondCoord = parseFloatPercentageValue(value.substring(delimiterOffset + 1, value.length() - 1), isSecondValueValid);

    isValidSetting = isFirstValueValid && isSecondValueValid;
    return FloatPoint(firstCoord, secondCoord);
}
#endif

WebVTTParser::WebVTTParser(WebVTTParserClient* client, ScriptExecutionContext* context)
    : m_scriptExecutionContext(context)
    , m_state(Initial)
    , m_currentStartTime(0)
    , m_currentEndTime(0)
    , m_tokenizer(std::make_unique<WebVTTTokenizer>())
    , m_client(client)
{
}

void WebVTTParser::getNewCues(Vector<RefPtr<WebVTTCueData>>& outputCues)
{
    outputCues = m_cuelist;
    m_cuelist.clear();
}

#if ENABLE(WEBVTT_REGIONS)
void WebVTTParser::getNewRegions(Vector<RefPtr<TextTrackRegion>>& outputRegions)
{
    outputRegions = m_regionList;
    m_regionList.clear();
}
#endif

void WebVTTParser::parseBytes(const char* data, unsigned length)
{
    // 4.8.10.13.3 WHATWG WebVTT Parser algorithm.
    // 1-3 - Initial setup.
    unsigned position = 0;

    while (position < length) {
        String line = collectNextLine(data, length, &position);
        if (line.isNull()) {
            m_buffer.append(data + position, length - position);
            return;
        }

        switch (m_state) {
        case Initial:

            // 4-12 - Collect the first line and check for "WEBVTT".
            if (!hasRequiredFileIdentifier(line)) {
                if (m_client)
                    m_client->fileFailedToParse();
                return;
            }

            m_state = Header;
            break;

        case Header:
#if ENABLE(WEBVTT_REGIONS)
            collectMetadataHeader(line);

            // 13-18 - Allow a header (comment area) under the WEBVTT line.
            if (line.isEmpty()) {
                if (m_client && m_regionList.size())
                    m_client->newRegionsParsed();

                m_state = Id;
                break;
            }
#endif
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
            m_state = collectCueText(line);
            break;

        case BadCue:
            // 54-62 - Collect and discard the remaining cue.
            m_state = ignoreBadCue(line);
            break;

        case Finished:
            ASSERT_NOT_REACHED();
            break;
        }
    }
}

void WebVTTParser::fileFinished()
{
    ASSERT(m_state != Finished);
    parseBytes("\n\n", 2);
    m_state = Finished;
}

bool WebVTTParser::hasRequiredFileIdentifier(const String& line)
{
    // A WebVTT file identifier consists of an optional BOM character,
    // the string "WEBVTT" followed by an optional space or tab character,
    // and any number of characters that are not line terminators ...
    unsigned linePos = 0;

    if (line.isEmpty())
        return false;

    if (line[0] == bom)
        ++linePos;

    if (line.length() < fileIdentifierLength + linePos)
        return false;

    for (unsigned i = 0; i < fileIdentifierLength; ++i, ++linePos) {
        if (line[linePos] != fileIdentifier[i])
            return false;
    }

    if (linePos < line.length() && line[linePos] != ' ' && line[linePos] != '\t')
        return false;
    return true;
}

#if ENABLE(WEBVTT_REGIONS)
void WebVTTParser::collectMetadataHeader(const String& line)
{
    // 4.1 Extension of WebVTT header parsing (11 - 15)
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, regionHeaderName, ("Region", AtomicString::ConstructFromLiteral));

    // 15.4 If line contains the character ":" (A U+003A COLON), then set metadata's
    // name to the substring of line before the first ":" character and
    // metadata's value to the substring after this character.
    if (!line.contains(":"))
        return;

    unsigned colonPosition = line.find(":");
    m_currentHeaderName = line.substring(0, colonPosition);

    // 15.5 If metadata's name equals "Region":
    if (m_currentHeaderName == regionHeaderName) {
        m_currentHeaderValue = line.substring(colonPosition + 1, line.length() - 1);
        // 15.5.1 - 15.5.8 Region creation: Let region be a new text track region [...]
        createNewRegion();
    }
}
#endif

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
    
    skipWhiteSpace(line, &position);

    // 6-9 - If the next three characters are not "-->", abort and return failure.
    if (line.find("-->", position) == notFound)
        return BadCue;
    position += 3;
    if (position >= line.length())
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

WebVTTParser::ParseState WebVTTParser::collectCueText(const String& line)
{
    if (line.isEmpty()) {
        createNewCue();
        return Id;
    }
    if (!m_currentContent.isEmpty())
        m_currentContent.append("\n");
    m_currentContent.append(line);
                
    return CueText;
}

WebVTTParser::ParseState WebVTTParser::ignoreBadCue(const String& line)
{
    if (!line.isEmpty())
        return BadCue;
    return Id;
}

PassRefPtr<DocumentFragment> WebVTTParser::createDocumentFragmentFromCueText(const String& text)
{
    // Cue text processing based on
    // 4.8.10.13.4 WebVTT cue text parsing rules and
    // 4.8.10.13.5 WebVTT cue text DOM construction rules.

    ASSERT(m_scriptExecutionContext->isDocument());
    Document* document = toDocument(m_scriptExecutionContext);
    
    RefPtr<DocumentFragment> fragment = DocumentFragment::create(*document);

    if (text.isEmpty()) {
        fragment->parserAppendChild(Text::create(*document, emptyString()));
        return fragment.release();
    }

    m_currentNode = fragment;
    m_tokenizer->reset();
    m_token.clear();
    
    m_languageStack.clear();
    SegmentedString content(text);
    while (m_tokenizer->nextToken(content, m_token))
        constructTreeFromToken(document);
    
    return fragment.release();
}

void WebVTTParser::createNewCue()
{
    if (!m_currentContent.length())
        return;

    RefPtr<WebVTTCueData> cue = WebVTTCueData::create();
    cue->setStartTime(m_currentStartTime);
    cue->setEndTime(m_currentEndTime);
    cue->setContent(m_currentContent.toString());
    cue->setId(m_currentId);
    cue->setSettings(m_currentSettings);

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

#if ENABLE(WEBVTT_REGIONS)
void WebVTTParser::createNewRegion()
{
    if (!m_currentHeaderValue.length())
        return;

    RefPtr<TextTrackRegion> region = TextTrackRegion::create(*m_scriptExecutionContext);
    region->setRegionSettings(m_currentHeaderValue);

    // 15.5.10 If the text track list of regions regions contains a region
    // with the same region identifier value as region, remove that region.
    for (size_t i = 0; i < m_regionList.size(); ++i)
        if (m_regionList[i]->id() == region->id()) {
            m_regionList.remove(i);
            break;
        }

    m_regionList.append(region);
}
#endif

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

static WebVTTNodeType tokenToNodeType(WebVTTToken& token)
{
    switch (token.name().size()) {
    case 1:
        if (token.name()[0] == 'c')
            return WebVTTNodeTypeClass;
        if (token.name()[0] == 'v')
            return WebVTTNodeTypeVoice;
        if (token.name()[0] == 'b')
            return WebVTTNodeTypeBold;
        if (token.name()[0] == 'i')
            return WebVTTNodeTypeItalic;
        if (token.name()[0] == 'u')
            return WebVTTNodeTypeUnderline;
        break;
    case 2:
        if (token.name()[0] == 'r' && token.name()[1] == 't')
            return WebVTTNodeTypeRubyText;
        break;
    case 4:
        if (token.name()[0] == 'r' && token.name()[1] == 'u' && token.name()[2] == 'b' && token.name()[3] == 'y')
            return WebVTTNodeTypeRuby;
        if (token.name()[0] == 'l' && token.name()[1] == 'a' && token.name()[2] == 'n' && token.name()[3] == 'g')
            return WebVTTNodeTypeLanguage;
        break;
    }
    return WebVTTNodeTypeNone;
}

void WebVTTParser::constructTreeFromToken(Document* document)
{
    QualifiedName tagName(nullAtom, AtomicString(m_token.name()), xhtmlNamespaceURI);

    // http://dev.w3.org/html5/webvtt/#webvtt-cue-text-dom-construction-rules

    switch (m_token.type()) {
    case WebVTTTokenTypes::Character: {
        String content(m_token.characters()); // FIXME: This should be 8bit if possible.
        RefPtr<Text> child = Text::create(*document, content);
        m_currentNode->parserAppendChild(child);
        break;
    }
    case WebVTTTokenTypes::StartTag: {
        RefPtr<WebVTTElement> child;
        WebVTTNodeType nodeType = tokenToNodeType(m_token);
        if (nodeType != WebVTTNodeTypeNone)
            child = WebVTTElement::create(nodeType, *document);
        if (child) {
            if (m_token.classes().size() > 0)
                child->setAttribute(classAttr, AtomicString(m_token.classes()));

            if (child->webVTTNodeType() == WebVTTNodeTypeVoice)
                child->setAttribute(WebVTTElement::voiceAttributeName(), AtomicString(m_token.annotation()));
            else if (child->webVTTNodeType() == WebVTTNodeTypeLanguage) {
                m_languageStack.append(AtomicString(m_token.annotation()));
                child->setAttribute(WebVTTElement::langAttributeName(), m_languageStack.last());
            }
            if (!m_languageStack.isEmpty())
                child->setLanguage(m_languageStack.last());
            m_currentNode->parserAppendChild(child);
            m_currentNode = child;
        }
        break;
    }
    case WebVTTTokenTypes::EndTag: {
        WebVTTNodeType nodeType = tokenToNodeType(m_token);
        if (nodeType != WebVTTNodeTypeNone) {
            if (nodeType == WebVTTNodeTypeLanguage && m_currentNode->isWebVTTElement() && toWebVTTElement(m_currentNode.get())->webVTTNodeType() == WebVTTNodeTypeLanguage)
                m_languageStack.removeLast();
            if (m_currentNode->parentNode())
                m_currentNode = m_currentNode->parentNode();
        }
        break;
    }
    case WebVTTTokenTypes::TimestampTag: {
        unsigned position = 0;
        String charactersString(StringImpl::create8BitIfPossible(m_token.characters()));
        double time = collectTimeStamp(charactersString, &position);
        if (time != malformedTime)
            m_currentNode->parserAppendChild(ProcessingInstruction::create(*document, "timestamp", charactersString));
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

String WebVTTParser::collectNextLine(const char* data, unsigned length, unsigned* position)
{
    unsigned currentPosition = *position;
    while (currentPosition < length && data[currentPosition] != '\r' && data[currentPosition] != '\n')
        currentPosition++;
    if (currentPosition >= length)
        return String();
    String line = String::fromUTF8(data + *position , currentPosition - *position);
    if (data[currentPosition] == '\r')
        currentPosition++;
    if (currentPosition < length && data[currentPosition] == '\n')
        currentPosition++;
    *position = currentPosition;
    if (m_buffer.isEmpty())
        return line;

    String lineWithBuffer = String::fromUTF8(m_buffer.data(), m_buffer.size());
    lineWithBuffer.append(line);
    m_buffer.clear();
    return lineWithBuffer;
}

}

#endif
