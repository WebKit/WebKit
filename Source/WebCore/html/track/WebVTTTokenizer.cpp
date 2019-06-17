/*
 * Copyright (C) 2011, 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2014-2015 Apple Inc.  All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "MarkupTokenizerInlines.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

#define WEBVTT_ADVANCE_TO(stateName)                        \
    do {                                                    \
        ASSERT(!m_input.isEmpty());                         \
        m_preprocessor.advance(m_input);                    \
        character = m_preprocessor.nextInputCharacter();    \
        goto stateName;                                     \
    } while (false)

template<unsigned charactersCount> ALWAYS_INLINE bool equalLiteral(const StringBuilder& s, const char (&characters)[charactersCount])
{
    return WTF::equal(s, reinterpret_cast<const LChar*>(characters), charactersCount - 1);
}

static void addNewClass(StringBuilder& classes, const StringBuilder& newClass)
{
    if (!classes.isEmpty())
        classes.append(' ');
    classes.append(newClass);
}

inline bool emitToken(WebVTTToken& resultToken, const WebVTTToken& token)
{
    resultToken = token;
    return true;
}

inline bool advanceAndEmitToken(SegmentedString& source, WebVTTToken& resultToken, const WebVTTToken& token)
{
    source.advance();
    return emitToken(resultToken, token);
}

WebVTTTokenizer::WebVTTTokenizer(const String& input)
    : m_input(input)
    , m_preprocessor(*this)
{
    // Append an EOF marker and close the input "stream".
    ASSERT(!m_input.isClosed());
    m_input.append(String { &kEndOfFileMarker, 1 });
    m_input.close();
}

bool WebVTTTokenizer::nextToken(WebVTTToken& token)
{
    if (m_input.isEmpty() || !m_preprocessor.peek(m_input))
        return false;

    UChar character = m_preprocessor.nextInputCharacter();
    if (character == kEndOfFileMarker) {
        m_preprocessor.advance(m_input);
        return false;
    }

    StringBuilder buffer;
    StringBuilder result;
    StringBuilder classes;

// 4.8.10.13.4 WebVTT cue text tokenizer
DataState:
    if (character == '&') {
        buffer.append('&');
        WEBVTT_ADVANCE_TO(EscapeState);
    } else if (character == '<') {
        if (result.isEmpty())
            WEBVTT_ADVANCE_TO(TagState);
        else {
            // We don't want to advance input or perform a state transition - just return a (new) token.
            // (On the next call to nextToken we will see '<' again, but take the other branch in this if instead.)
            return emitToken(token, WebVTTToken::StringToken(result.toString()));
        }
    } else if (character == kEndOfFileMarker)
        return advanceAndEmitToken(m_input, token, WebVTTToken::StringToken(result.toString()));
    else {
        result.append(character);
        WEBVTT_ADVANCE_TO(DataState);
    }

EscapeState:
    if (character == ';') {
        if (equalLiteral(buffer, "&amp"))
            result.append('&');
        else if (equalLiteral(buffer, "&lt"))
            result.append('<');
        else if (equalLiteral(buffer, "&gt"))
            result.append('>');
        else if (equalLiteral(buffer, "&lrm"))
            result.append(leftToRightMark);
        else if (equalLiteral(buffer, "&rlm"))
            result.append(rightToLeftMark);
        else if (equalLiteral(buffer, "&nbsp"))
            result.append(noBreakSpace);
        else {
            buffer.append(character);
            result.append(buffer);
        }
        buffer.clear();
        WEBVTT_ADVANCE_TO(DataState);
    } else if (isASCIIAlphanumeric(character)) {
        buffer.append(character);
        WEBVTT_ADVANCE_TO(EscapeState);
    } else if (character == '<') {
        result.append(buffer);
        return emitToken(token, WebVTTToken::StringToken(result.toString()));
    } else if (character == kEndOfFileMarker) {
        result.append(buffer);
        return advanceAndEmitToken(m_input, token, WebVTTToken::StringToken(result.toString()));
    } else {
        result.append(buffer);
        buffer.clear();

        if (character == '&') {
            buffer.append('&');
            WEBVTT_ADVANCE_TO(EscapeState);
        }
        result.append(character);
        WEBVTT_ADVANCE_TO(DataState);
    }

TagState:
    if (isTokenizerWhitespace(character)) {
        ASSERT(result.isEmpty());
        WEBVTT_ADVANCE_TO(StartTagAnnotationState);
    } else if (character == '.') {
        ASSERT(result.isEmpty());
        WEBVTT_ADVANCE_TO(StartTagClassState);
    } else if (character == '/') {
        WEBVTT_ADVANCE_TO(EndTagState);
    } else if (WTF::isASCIIDigit(character)) {
        result.append(character);
        WEBVTT_ADVANCE_TO(TimestampTagState);
    } else if (character == '>' || character == kEndOfFileMarker) {
        ASSERT(result.isEmpty());
        return advanceAndEmitToken(m_input, token, WebVTTToken::StartTag(result.toString()));
    } else {
        result.append(character);
        WEBVTT_ADVANCE_TO(StartTagState);
    }

StartTagState:
    if (isTokenizerWhitespace(character))
        WEBVTT_ADVANCE_TO(StartTagAnnotationState);
    else if (character == '.')
        WEBVTT_ADVANCE_TO(StartTagClassState);
    else if (character == '>' || character == kEndOfFileMarker)
        return advanceAndEmitToken(m_input, token, WebVTTToken::StartTag(result.toString()));
    else {
        result.append(character);
        WEBVTT_ADVANCE_TO(StartTagState);
    }

StartTagClassState:
    if (isTokenizerWhitespace(character)) {
        addNewClass(classes, buffer);
        buffer.clear();
        WEBVTT_ADVANCE_TO(StartTagAnnotationState);
    } else if (character == '.') {
        addNewClass(classes, buffer);
        buffer.clear();
        WEBVTT_ADVANCE_TO(StartTagClassState);
    } else if (character == '>' || character == kEndOfFileMarker) {
        addNewClass(classes, buffer);
        buffer.clear();
        return advanceAndEmitToken(m_input, token, WebVTTToken::StartTag(result.toString(), classes.toAtomString()));
    } else {
        buffer.append(character);
        WEBVTT_ADVANCE_TO(StartTagClassState);
    }

StartTagAnnotationState:
    if (character == '>' || character == kEndOfFileMarker)
        return advanceAndEmitToken(m_input, token, WebVTTToken::StartTag(result.toString(), classes.toAtomString(), buffer.toAtomString()));
    buffer.append(character);
    WEBVTT_ADVANCE_TO(StartTagAnnotationState);

EndTagState:
    if (character == '>' || character == kEndOfFileMarker)
        return advanceAndEmitToken(m_input, token, WebVTTToken::EndTag(result.toString()));
    result.append(character);
    WEBVTT_ADVANCE_TO(EndTagState);

TimestampTagState:
    if (character == '>' || character == kEndOfFileMarker)
        return advanceAndEmitToken(m_input, token, WebVTTToken::TimestampTag(result.toString()));
    result.append(character);
    WEBVTT_ADVANCE_TO(TimestampTagState);
}

}

#endif
