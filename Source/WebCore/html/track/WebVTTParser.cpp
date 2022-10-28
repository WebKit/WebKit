/*
 * Copyright (C) 2011, 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2013 Cable Television Labs, Inc.
 * Copyright (C) 2011-2020 Apple Inc.  All rights reserved.
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
#include "WebVTTParser.h"

#if ENABLE(VIDEO)

#include "CommonAtomStrings.h"
#include "Document.h"
#include "HTMLParserIdioms.h"
#include "ISOVTTCue.h"
#include "ProcessingInstruction.h"
#include "StyleProperties.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"
#include "Text.h"
#include "VTTScanner.h"
#include "WebVTTElement.h"
#include "WebVTTTokenizer.h"

namespace WebCore {

constexpr double secondsPerHour = 3600;
constexpr double secondsPerMinute = 60;
constexpr double secondsPerMillisecond = 0.001;
constexpr auto fileIdentifier = "WEBVTT"_s;
constexpr unsigned fileIdentifierLength = 6;
constexpr unsigned regionIdentifierLength = 6;
constexpr unsigned styleIdentifierLength = 5;

bool WebVTTParser::parseFloatPercentageValue(VTTScanner& valueScanner, float& percentage)
{
    float number;
    if (!valueScanner.scanFloat(number))
        return false;
    // '%' must be present and at the end of the setting value.
    if (!valueScanner.scan('%'))
        return false;

    if (number < 0 || number > 100)
        return false;

    percentage = number;
    return true;
}

bool WebVTTParser::parseFloatPercentageValuePair(VTTScanner& valueScanner, char delimiter, FloatPoint& valuePair)
{
    float firstCoord;
    if (!parseFloatPercentageValue(valueScanner, firstCoord))
        return false;

    if (!valueScanner.scan(delimiter))
        return false;

    float secondCoord;
    if (!parseFloatPercentageValue(valueScanner, secondCoord))
        return false;

    valuePair = FloatPoint(firstCoord, secondCoord);
    return true;
}

WebVTTParser::WebVTTParser(WebVTTParserClient& client, Document& document)
    : m_document(document)
    , m_decoder(TextResourceDecoder::create(textPlainContentTypeAtom(), PAL::UTF8Encoding()))
    , m_client(client)
{
}

Vector<Ref<WebVTTCueData>> WebVTTParser::takeCues()
{
    return WTFMove(m_cuelist);
}

Vector<Ref<VTTRegion>> WebVTTParser::takeRegions()
{
    return WTFMove(m_regionList);
}

Vector<String> WebVTTParser::takeStyleSheets()
{
    return WTFMove(m_styleSheets);
}

void WebVTTParser::parseFileHeader(String&& data)
{
    m_state = Initial;
    m_lineReader.reset();
    m_lineReader.append(WTFMove(data));
    parse();
    if (!m_regionList.isEmpty())
        m_client.newRegionsParsed();
    if (!m_styleSheets.isEmpty())
        m_client.newStyleSheetsParsed();
}

void WebVTTParser::parseBytes(const uint8_t* data, unsigned length)
{
    m_lineReader.append(m_decoder->decode(data, length));
    parse();
}

void WebVTTParser::parseCueData(const ISOWebVTTCue& data)
{
    auto cue = WebVTTCueData::create();

    MediaTime startTime = data.presentationTime();
    cue->setStartTime(startTime);
    cue->setEndTime(startTime + data.duration());

    cue->setContent(data.cueText());
    cue->setId(data.id());
    cue->setSettings(data.settings());

    MediaTime originalStartTime;
    if (WebVTTParser::collectTimeStamp(data.originalStartTime(), originalStartTime))
        cue->setOriginalStartTime(originalStartTime);

    m_cuelist.append(WTFMove(cue));
    m_client.newCuesParsed();
}

void WebVTTParser::flush()
{
    m_lineReader.append(m_decoder->flush());
    m_lineReader.appendEndOfStream();
    parse();
    flushPendingCue();
}

void WebVTTParser::parse()
{    
    // WebVTT parser algorithm. (5.1 WebVTT file parsing.)
    // Steps 1 - 3 - Initial setup.
    while (auto line = m_lineReader.nextLine()) {
        switch (m_state) {
        case Initial:
            // Steps 4 - 9 - Check for a valid WebVTT signature.
            if (!hasRequiredFileIdentifier(*line)) {
                m_client.fileFailedToParse();
                return;
            }

            m_state = Header;
            break;

        case Header:
            // Steps 11 - 14 - Collect WebVTT block
            m_state = collectWebVTTBlock(*line);
            break;

        case Region:
            m_state = collectRegionSettings(*line);
            break;

        case Style:
            m_state = collectStyleSheet(*line);
            break;

        case Id:
            // Steps 17 - 20 - Allow any number of line terminators, then initialize new cue values.
            if (line->isEmpty())
                break;

            // Step 21 - Cue creation (start a new cue).
            resetCueValues();

            // Steps 22 - 25 - Check if this line contains an optional identifier or timing data.
            m_state = collectCueId(*line);
            break;

        case TimingsAndSettings:
            // Steps 26 - 27 - Discard current cue if the line is empty.
            if (line->isEmpty()) {
                m_state = Id;
                break;
            }

            // Steps 28 - 29 - Collect cue timings and settings.
            m_state = collectTimingsAndSettings(*line);
            break;

        case CueText:
            // Steps 31 - 41 - Collect the cue text, create a cue, and add it to the output.
            m_state = collectCueText(*line);
            break;

        case BadCue:
            // Steps 42 - 48 - Discard lines until an empty line or a potential timing line is seen.
            m_state = ignoreBadCue(*line);
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
    constexpr uint8_t endLines[] = { '\n', '\n' };
    parseBytes(endLines, 2);
    m_state = Finished;
}

void WebVTTParser::flushPendingCue()
{
    ASSERT(m_lineReader.isAtEndOfStream());
    // If we're in the CueText state when we run out of data, we emit the pending cue.
    if (m_state == CueText)
        createNewCue();
}

bool WebVTTParser::hasRequiredFileIdentifier(const String& line)
{
    // A WebVTT file identifier consists of an optional BOM character,
    // the string "WEBVTT" followed by an optional space or tab character,
    // and any number of characters that are not line terminators ...
    if (!line.startsWith(fileIdentifier))
        return false;
    if (line.length() > fileIdentifierLength && !isHTMLSpace(line[fileIdentifierLength]))
        return false;
    return true;
}

WebVTTParser::ParseState WebVTTParser::collectRegionSettings(const String& line)
{
    // End of region block
    if (checkAndStoreRegion(line))
        return checkAndRecoverCue(line);
    
    m_currentRegion->setRegionSettings(line);
    return Region;
}

WebVTTParser::ParseState WebVTTParser::collectWebVTTBlock(const String& line)
{
    // collect a WebVTT block parsing. (WebVTT parser algorithm step 14)
    
    if (checkAndCreateRegion(line))
        return Region;
    
    if (checkStyleSheet(line))
        return Style;

    // Handle cue block.
    ParseState state = checkAndRecoverCue(line);
    if (state != Header) {
        if (!m_regionList.isEmpty())
            m_client.newRegionsParsed();
        if (!m_styleSheets.isEmpty())
            m_client.newStyleSheetsParsed();
        if (!m_previousLine.isEmpty() && !m_previousLine.contains("-->"_s))
            m_currentId = AtomString { m_previousLine };
        
        return state;
    }
    
    // store previous line for cue id.
    // length is more than 1 line clear m_previousLine and ignore line.
    if (m_previousLine.isEmpty())
        m_previousLine = line;
    else
        m_previousLine = emptyString();

    return state;
}

WebVTTParser::ParseState WebVTTParser::checkAndRecoverCue(const String& line)
{
    // parse cue timings and settings
    if (line.contains("-->"_s)) {
        ParseState state = recoverCue(line);
        if (state != BadCue)
            return state;
    }
    return Header;
}

WebVTTParser::ParseState WebVTTParser::collectStyleSheet(const String& line)
{
    // End of style block
    if (checkAndStoreStyleSheet(line))
        return checkAndRecoverCue(line);

    m_currentSourceStyleSheet.append(line);
    return Style;
}

bool WebVTTParser::checkAndCreateRegion(StringView line)
{
    if (m_previousLine.contains("-->"_s))
        return false;
    // line starts with the substring "REGION" and remaining characters
    // zero or more U+0020 SPACE characters or U+0009 CHARACTER TABULATION
    // (tab) characters expected other than these characters it is invalid.
    if (line.startsWith("REGION"_s) && line.substring(regionIdentifierLength).isAllSpecialCharacters<isASpace>()) {
        m_currentRegion = VTTRegion::create(m_document);
        return true;
    }
    return false;
}

bool WebVTTParser::checkAndStoreRegion(StringView line)
{
    if (!line.isEmpty() && !line.contains("-->"_s))
        return false;

    if (!m_currentRegion->id().isEmpty()) {
        m_regionList.removeFirstMatching([this] (auto& region) {
            return region->id() == m_currentRegion->id();
        });
        m_regionList.append(m_currentRegion.releaseNonNull());
    }
    m_currentRegion = nullptr;
    return true;
}

bool WebVTTParser::checkStyleSheet(StringView line)
{
    if (m_previousLine.contains("-->"_s))
        return false;
    // line starts with the substring "STYLE" and remaining characters
    // zero or more U+0020 SPACE characters or U+0009 CHARACTER TABULATION
    // (tab) characters expected other than these characters it is invalid.
    if (line.startsWith("STYLE"_s) && line.substring(styleIdentifierLength).isAllSpecialCharacters<isASpace>())
        return true;

    return false;
}

bool WebVTTParser::checkAndStoreStyleSheet(StringView line)
{
    if (!line.isEmpty() && !line.contains("-->"_s))
        return false;
    
    auto styleSheetText = m_currentSourceStyleSheet.toString();
    m_currentSourceStyleSheet.clear();

    // WebVTTMode disallows non-data URLs.
    auto contents = StyleSheetContents::create(CSSParserContext(WebVTTMode));
    if (!contents->parseString(styleSheetText))
        return true;

    auto& namespaceRules = contents->namespaceRules();
    if (namespaceRules.size())
        return true;

    auto& importRules = contents->importRules();
    if (importRules.size())
        return true;

    auto& childRules = contents->childRules();
    if (!childRules.size())
        return true;

    StringBuilder sanitizedStyleSheetBuilder;
    
    for (const auto& rule : childRules) {
        if (!rule->isStyleRule())
            return true;
        const auto& styleRule = downcast<StyleRule>(*rule);

        const auto& selectorList = styleRule.selectorList();
        if (selectorList.listSize() != 1)
            return true;
        auto selector = selectorList.selectorAt(0);
        auto selectorText = selector->selectorText();
        
        bool isCue = selectorText == "::cue"_s || selectorText.startsWith("::cue("_s);
        if (!isCue)
            return true;

        if (styleRule.properties().isEmpty())
            continue;

        sanitizedStyleSheetBuilder.append(selectorText, " { ", styleRule.properties().asText(), "  }\n");
    }

    // It would be more stylish to parse the stylesheet only once instead of serializing a sanitized version.
    if (!sanitizedStyleSheetBuilder.isEmpty())
        m_styleSheets.append(sanitizedStyleSheetBuilder.toString());

    return true;
}

WebVTTParser::ParseState WebVTTParser::collectCueId(const String& line)
{
    if (line.contains("-->"_s))
        return collectTimingsAndSettings(line);
    m_currentId = AtomString { line };
    return TimingsAndSettings;
}

WebVTTParser::ParseState WebVTTParser::collectTimingsAndSettings(const String& line)
{
    if (line.isEmpty())
        return BadCue;

    VTTScanner input(line);

    // Collect WebVTT cue timings and settings. (5.3 WebVTT cue timings and settings parsing.)
    // Steps 1 - 3 - Let input be the string being parsed and position be a pointer into input
    input.skipWhile<isHTMLSpace<UChar>>();

    // Steps 4 - 5 - Collect a WebVTT timestamp. If that fails, then abort and return failure. Otherwise, let cue's text track cue start time be the collected time.
    if (!collectTimeStamp(input, m_currentStartTime))
        return BadCue;
    
    input.skipWhile<isHTMLSpace<UChar>>();

    // Steps 6 - 9 - If the next three characters are not "-->", abort and return failure.
    if (!input.scan("-->"))
        return BadCue;
    
    input.skipWhile<isHTMLSpace<UChar>>();

    // Steps 10 - 11 - Collect a WebVTT timestamp. If that fails, then abort and return failure. Otherwise, let cue's text track cue end time be the collected time.
    if (!collectTimeStamp(input, m_currentEndTime))
        return BadCue;

    input.skipWhile<isHTMLSpace<UChar>>();

    // Step 12 - Parse the WebVTT settings for the cue (conducted in TextTrackCue).
    m_currentSettings = input.restOfInputAsString();
    return CueText;
}

WebVTTParser::ParseState WebVTTParser::collectCueText(const String& line)
{
    // Step 34.
    if (line.isEmpty()) {
        createNewCue();
        return Id;
    }
    // Step 35.
    if (line.contains("-->"_s)) {
        // Step 39-40.
        createNewCue();

        // Step 41 - New iteration of the cue loop.
        return recoverCue(line);
    }
    if (!m_currentContent.isEmpty())
        m_currentContent.append('\n');
    m_currentContent.append(line);

    return CueText;
}

WebVTTParser::ParseState WebVTTParser::recoverCue(const String& line)
{
    // Step 17 and 21.
    resetCueValues();

    // Step 22.
    return collectTimingsAndSettings(line);
}

WebVTTParser::ParseState WebVTTParser::ignoreBadCue(const String& line)
{
    if (line.isEmpty())
        return Id;
    if (line.contains("-->"_s))
        return recoverCue(line);
    return BadCue;
}

// A helper class for the construction of a "cue fragment" from the cue text.
class WebVTTTreeBuilder {
public:
    WebVTTTreeBuilder(Document& document)
        : m_document(document) { }

    Ref<DocumentFragment> buildFromString(const String& cueText);

private:
    void constructTreeFromToken(Document&);

    WebVTTToken m_token;
    RefPtr<ContainerNode> m_currentNode;
    Vector<AtomString> m_languageStack;
    Document& m_document;
};

Ref<DocumentFragment> WebVTTTreeBuilder::buildFromString(const String& cueText)
{
    // Cue text processing based on
    // 5.4 WebVTT cue text parsing rules, and
    // 5.5 WebVTT cue text DOM construction rules.
    auto fragment = DocumentFragment::create(m_document);

    if (cueText.isEmpty()) {
        fragment->parserAppendChild(Text::create(m_document, String { emptyString() }));
        return fragment;
    }

    m_currentNode = fragment.ptr();

    WebVTTTokenizer tokenizer(cueText);
    m_languageStack.clear();

    while (tokenizer.nextToken(m_token))
        constructTreeFromToken(m_document);
    
    return fragment;
}

Ref<DocumentFragment> WebVTTParser::createDocumentFragmentFromCueText(Document& document, const String& cueText)
{
    WebVTTTreeBuilder treeBuilder(document);
    return treeBuilder.buildFromString(cueText);
}

void WebVTTParser::createNewCue()
{
    auto cue = WebVTTCueData::create();
    cue->setStartTime(m_currentStartTime);
    cue->setEndTime(m_currentEndTime);
    cue->setContent(m_currentContent.toString());
    cue->setId(m_currentId);
    cue->setSettings(m_currentSettings);

    m_cuelist.append(WTFMove(cue));
    m_client.newCuesParsed();
}

void WebVTTParser::resetCueValues()
{
    m_currentId = emptyAtom();
    m_currentSettings = emptyString();
    m_currentStartTime = MediaTime::zeroTime();
    m_currentEndTime = MediaTime::zeroTime();
    m_currentContent.clear();
}

bool WebVTTParser::collectTimeStamp(const String& line, MediaTime& timeStamp)
{
    if (line.isEmpty())
        return false;

    VTTScanner input(line);
    return collectTimeStamp(input, timeStamp);
}

bool WebVTTParser::collectTimeStamp(VTTScanner& input, MediaTime& timeStamp)
{
    // Collect a WebVTT timestamp (5.3 WebVTT cue timings and settings parsing.)
    // Steps 1 - 4 - Initial checks, let most significant units be minutes.
    enum Mode { minutes, hours };
    Mode mode = minutes;

    // Steps 5 - 7 - Collect a sequence of characters that are 0-9.
    // If not 2 characters or value is greater than 59, interpret as hours.
    int value1;
    unsigned value1Digits = input.scanDigits(value1);
    if (!value1Digits)
        return false;
    if (value1Digits != 2 || value1 > 59)
        mode = hours;

    // Steps 8 - 11 - Collect the next sequence of 0-9 after ':' (must be 2 chars).
    int value2;
    if (!input.scan(':') || input.scanDigits(value2) != 2)
        return false;

    // Step 12 - Detect whether this timestamp includes hours.
    int value3;
    if (mode == hours || input.match(':')) {
        if (!input.scan(':') || input.scanDigits(value3) != 2)
            return false;
    } else {
        value3 = value2;
        value2 = value1;
        value1 = 0;
    }

    // Steps 13 - 17 - Collect next sequence of 0-9 after '.' (must be 3 chars).
    int value4;
    if (!input.scan('.') || input.scanDigits(value4) != 3)
        return false;
    if (value2 > 59 || value3 > 59)
        return false;

    // Steps 18 - 19 - Calculate result.
    timeStamp = MediaTime::createWithDouble((value1 * secondsPerHour) + (value2 * secondsPerMinute) + value3 + (value4 * secondsPerMillisecond));
    return true;
}

static WebVTTNodeType tokenToNodeType(WebVTTToken& token)
{
    switch (token.name().length()) {
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

void WebVTTTreeBuilder::constructTreeFromToken(Document& document)
{
    // http://dev.w3.org/html5/webvtt/#webvtt-cue-text-dom-construction-rules

    switch (m_token.type()) {
    case WebVTTTokenTypes::Character: {
        m_currentNode->parserAppendChild(Text::create(document, String { m_token.characters() }));
        break;
    }
    case WebVTTTokenTypes::StartTag: {
        WebVTTNodeType nodeType = tokenToNodeType(m_token);
        if (nodeType == WebVTTNodeTypeNone)
            break;

        WebVTTNodeType currentType = is<WebVTTElement>(*m_currentNode) ? downcast<WebVTTElement>(*m_currentNode).webVTTNodeType() : WebVTTNodeTypeNone;
        // <rt> is only allowed if the current node is <ruby>.
        if (nodeType == WebVTTNodeTypeRubyText && currentType != WebVTTNodeTypeRuby)
            break;

        auto child = WebVTTElement::create(nodeType, document);
        if (!m_token.classes().isEmpty())
            child->setAttributeWithoutSynchronization(classAttr, m_token.classes());

        if (nodeType == WebVTTNodeTypeVoice)
            child->setAttributeWithoutSynchronization(WebVTTElement::voiceAttributeName(), m_token.annotation());
        else if (nodeType == WebVTTNodeTypeLanguage) {
            m_languageStack.append(m_token.annotation());
            child->setAttributeWithoutSynchronization(WebVTTElement::langAttributeName(), m_languageStack.last());
        }
        if (!m_languageStack.isEmpty())
            child->setLanguage(m_languageStack.last());
        m_currentNode->parserAppendChild(child);
        m_currentNode = WTFMove(child);
        break;
    }
    case WebVTTTokenTypes::EndTag: {
        WebVTTNodeType nodeType = tokenToNodeType(m_token);
        if (nodeType == WebVTTNodeTypeNone)
            break;
        
        // The only non-VTTElement would be the DocumentFragment root. (Text
        // nodes and PIs will never appear as m_currentNode.)
        if (!is<WebVTTElement>(*m_currentNode))
            break;

        WebVTTNodeType currentType = downcast<WebVTTElement>(*m_currentNode).webVTTNodeType();
        bool matchesCurrent = nodeType == currentType;
        if (!matchesCurrent) {
            // </ruby> auto-closes <rt>
            if (currentType == WebVTTNodeTypeRubyText && nodeType == WebVTTNodeTypeRuby) {
                if (m_currentNode->parentNode())
                    m_currentNode = m_currentNode->parentNode();
            } else
                break;
        }
        if (nodeType == WebVTTNodeTypeLanguage)
            m_languageStack.removeLast();
        if (m_currentNode->parentNode())
            m_currentNode = m_currentNode->parentNode();
        break;
    }
    case WebVTTTokenTypes::TimestampTag: {
        String charactersString = m_token.characters();
        MediaTime parsedTimeStamp;
        if (WebVTTParser::collectTimeStamp(charactersString, parsedTimeStamp))
            m_currentNode->parserAppendChild(ProcessingInstruction::create(document, "timestamp"_s, WTFMove(charactersString)));
        break;
    }
    default:
        break;
    }
}

}

#endif
