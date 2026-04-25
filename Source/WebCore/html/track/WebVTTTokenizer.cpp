/*
 * Copyright (C) 2011, 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "WebVTTTokenizer.h"

#if ENABLE(VIDEO)

#include "HTMLEntityParser.h"
#include "MarkupTokenizerInlines.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

#define WEBVTT_ADVANCE_TO(stateName)                        \
    do {                                                    \
        ASSERT(input.hasCharactersRemaining());             \
        input.advance();                                    \
        if (!input.atEnd())                                 \
            character = *input;                             \
        goto stateName;                                     \
    } while (false)
#define WEBVTT_SWITCH_TO(stateName)                         \
    do {                                                    \
        if (!input.atEnd())                                 \
            character = *input;                             \
        goto stateName;                                     \
    } while (false)

static void addNewClass(StringBuilder& classes, const StringBuilder& newClass)
{
    if (!classes.isEmpty())
        classes.append(' ');
    classes.append(newClass);
}

static bool emitToken(WebVTTToken& resultToken, const WebVTTToken& token)
{
    resultToken = token;
    return true;
}

template<typename CharacterType>
static void processEntity(StringParsingBuffer<CharacterType>& source, StringBuilder& result, char16_t additionalAllowedCharacter = 0)
{
    auto decoded = consumeHTMLEntity(source, additionalAllowedCharacter);
    if (decoded.failed())
        result.append('&');
    else {
        for (auto character : decoded.span())
            result.append(character);
    }
}

WebVTTTokenizer::WebVTTTokenizer(const String& input)
    : m_input(input)
    , m_buffer(input.is8Bit()
        ? decltype(m_buffer) { StringParsingBuffer { input.span8() } }
        : decltype(m_buffer) { StringParsingBuffer { input.span16() } })
{
}

bool WebVTTTokenizer::nextToken(WebVTTToken& token)
{
    return WTF::switchOn(m_buffer, [&](auto& buffer) {
        return nextTokenImpl(buffer, token);
    });
}

template<typename CharacterType>
bool WebVTTTokenizer::nextTokenImpl(StringParsingBuffer<CharacterType>& input, WebVTTToken& token)
{
    if (input.atEnd())
        return false;

    char16_t character = *input;

    StringBuilder buffer;
    StringBuilder result;
    StringBuilder classes;

// 4.8.10.13.4 WebVTT cue text tokenizer
DataState:
    if (input.atEnd())
        return emitToken(token, WebVTTToken::StringToken(result.toString()));
    if (character == '&') {
        WEBVTT_ADVANCE_TO(HTMLCharacterReferenceInDataState);
    } else if (character == '<') {
        if (result.isEmpty())
            WEBVTT_ADVANCE_TO(TagState);
        else {
            // We don't want to advance input or perform a state transition - just return a (new) token.
            // (On the next call to nextToken we will see '<' again, but take the other branch in this if instead.)
            return emitToken(token, WebVTTToken::StringToken(result.toString()));
        }
    } else {
        result.append(character);
        WEBVTT_ADVANCE_TO(DataState);
    }

TagState:
    if (input.atEnd()) {
        ASSERT(result.isEmpty());
        return emitToken(token, WebVTTToken::StartTag(result.toString()));
    }
    if (isTokenizerWhitespace(character)) {
        ASSERT(result.isEmpty());
        WEBVTT_ADVANCE_TO(StartTagAnnotationState);
    } else if (character == '.') {
        ASSERT(result.isEmpty());
        WEBVTT_ADVANCE_TO(StartTagClassState);
    } else if (character == '/') {
        WEBVTT_ADVANCE_TO(EndTagState);
    } else if (isASCIIDigit(character)) {
        result.append(character);
        WEBVTT_ADVANCE_TO(TimestampTagState);
    } else if (character == '>') {
        ASSERT(result.isEmpty());
        input.advance();
        return emitToken(token, WebVTTToken::StartTag(result.toString()));
    } else {
        result.append(character);
        WEBVTT_ADVANCE_TO(StartTagState);
    }

StartTagState:
    if (input.atEnd())
        return emitToken(token, WebVTTToken::StartTag(result.toString()));
    if (isTokenizerWhitespace(character))
        WEBVTT_ADVANCE_TO(StartTagAnnotationState);
    else if (character == '.')
        WEBVTT_ADVANCE_TO(StartTagClassState);
    else if (character == '>') {
        input.advance();
        return emitToken(token, WebVTTToken::StartTag(result.toString()));
    } else {
        result.append(character);
        WEBVTT_ADVANCE_TO(StartTagState);
    }

StartTagClassState:
    if (input.atEnd()) {
        addNewClass(classes, buffer);
        buffer.clear();
        return emitToken(token, WebVTTToken::StartTag(result.toString(), classes.toAtomString()));
    }
    if (isTokenizerWhitespace(character)) {
        addNewClass(classes, buffer);
        buffer.clear();
        WEBVTT_ADVANCE_TO(StartTagAnnotationState);
    } else if (character == '.') {
        addNewClass(classes, buffer);
        buffer.clear();
        WEBVTT_ADVANCE_TO(StartTagClassState);
    } else if (character == '>') {
        addNewClass(classes, buffer);
        buffer.clear();
        input.advance();
        return emitToken(token, WebVTTToken::StartTag(result.toString(), classes.toAtomString()));
    } else {
        buffer.append(character);
        WEBVTT_ADVANCE_TO(StartTagClassState);
    }

StartTagAnnotationState:
    if (input.atEnd())
        return emitToken(token, WebVTTToken::StartTag(result.toString(), classes.toAtomString(), buffer.toAtomString()));
    if (character == '&')
        WEBVTT_ADVANCE_TO(HTMLCharacterReferenceInAnnotationState);
    else if (character == '>') {
        input.advance();
        return emitToken(token, WebVTTToken::StartTag(result.toString(), classes.toAtomString(), buffer.toAtomString()));
    }
    buffer.append(character);
    WEBVTT_ADVANCE_TO(StartTagAnnotationState);

EndTagState:
    if (input.atEnd())
        return emitToken(token, WebVTTToken::EndTag(result.toString()));
    if (character == '>') {
        input.advance();
        return emitToken(token, WebVTTToken::EndTag(result.toString()));
    }
    result.append(character);
    WEBVTT_ADVANCE_TO(EndTagState);

TimestampTagState:
    if (input.atEnd())
        return emitToken(token, WebVTTToken::TimestampTag(result.toString()));
    if (character == '>') {
        input.advance();
        return emitToken(token, WebVTTToken::TimestampTag(result.toString()));
    }
    result.append(character);
    WEBVTT_ADVANCE_TO(TimestampTagState);

HTMLCharacterReferenceInDataState:
    processEntity(input, result);
    WEBVTT_SWITCH_TO(DataState);

HTMLCharacterReferenceInAnnotationState:
    processEntity(input, buffer, '>');
    WEBVTT_SWITCH_TO(StartTagAnnotationState);
}

} // namespace WebCore

#endif
