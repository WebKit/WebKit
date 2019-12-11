/*
 * Copyright (C) 2008-2016 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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

#include "config.h"
#include "HTMLTokenizer.h"

#include "HTMLEntityParser.h"
#include "HTMLNames.h"
#include "MarkupTokenizerInlines.h"
#include <wtf/text/StringBuilder.h>


namespace WebCore {

using namespace HTMLNames;

static inline LChar convertASCIIAlphaToLower(UChar character)
{
    ASSERT(isASCIIAlpha(character));
    return toASCIILowerUnchecked(character);
}

static inline bool vectorEqualsString(const Vector<LChar, 32>& vector, const char* string)
{
    unsigned size = vector.size();
    for (unsigned i = 0; i < size; ++i) {
        if (!string[i] || vector[i] != string[i])
            return false;
    }
    return !string[size];
}

inline bool HTMLTokenizer::inEndTagBufferingState() const
{
    switch (m_state) {
    case RCDATAEndTagOpenState:
    case RCDATAEndTagNameState:
    case RAWTEXTEndTagOpenState:
    case RAWTEXTEndTagNameState:
    case ScriptDataEndTagOpenState:
    case ScriptDataEndTagNameState:
    case ScriptDataEscapedEndTagOpenState:
    case ScriptDataEscapedEndTagNameState:
        return true;
    default:
        return false;
    }
}

HTMLTokenizer::HTMLTokenizer(const HTMLParserOptions& options)
    : m_preprocessor(*this)
    , m_options(options)
{
}

inline void HTMLTokenizer::bufferASCIICharacter(UChar character)
{
    ASSERT(character != kEndOfFileMarker);
    ASSERT(isASCII(character));
    LChar narrowedCharacter = character;
    m_token.appendToCharacter(narrowedCharacter);
}

inline void HTMLTokenizer::bufferCharacter(UChar character)
{
    ASSERT(character != kEndOfFileMarker);
    m_token.appendToCharacter(character);
}

inline bool HTMLTokenizer::emitAndResumeInDataState(SegmentedString& source)
{
    saveEndTagNameIfNeeded();
    m_state = DataState;
    source.advancePastNonNewline();
    return true;
}

inline bool HTMLTokenizer::emitAndReconsumeInDataState()
{
    saveEndTagNameIfNeeded();
    m_state = DataState;
    return true;
}

inline bool HTMLTokenizer::emitEndOfFile(SegmentedString& source)
{
    m_state = DataState;
    if (haveBufferedCharacterToken())
        return true;
    source.advance();
    m_token.clear();
    m_token.makeEndOfFile();
    return true;
}

inline void HTMLTokenizer::saveEndTagNameIfNeeded()
{
    ASSERT(m_token.type() != HTMLToken::Uninitialized);
    if (m_token.type() == HTMLToken::StartTag)
        m_appropriateEndTagName = m_token.name();
}

inline bool HTMLTokenizer::haveBufferedCharacterToken() const
{
    return m_token.type() == HTMLToken::Character;
}

inline bool HTMLTokenizer::processEntity(SegmentedString& source)
{
    bool notEnoughCharacters = false;
    StringBuilder decodedEntity;
    bool success = consumeHTMLEntity(source, decodedEntity, notEnoughCharacters);
    if (notEnoughCharacters)
        return false;
    if (!success) {
        ASSERT(decodedEntity.isEmpty());
        bufferASCIICharacter('&');
    } else {
        for (unsigned i = 0; i < decodedEntity.length(); ++i)
            bufferCharacter(decodedEntity[i]);
    }
    return true;
}

void HTMLTokenizer::flushBufferedEndTag()
{
    m_token.beginEndTag(m_bufferedEndTagName);
    m_bufferedEndTagName.clear();
    m_appropriateEndTagName.clear();
    m_temporaryBuffer.clear();
}

bool HTMLTokenizer::commitToPartialEndTag(SegmentedString& source, UChar character, State state)
{
    ASSERT(source.currentCharacter() == character);
    appendToTemporaryBuffer(character);
    source.advance();

    if (haveBufferedCharacterToken()) {
        // Emit the buffered character token.
        // The next call to processToken will flush the buffered end tag and continue parsing it.
        m_state = state;
        return true;
    }

    flushBufferedEndTag();
    return false;
}

bool HTMLTokenizer::commitToCompleteEndTag(SegmentedString& source)
{
    ASSERT(source.currentCharacter() == '>');
    appendToTemporaryBuffer('>');
    source.advancePastNonNewline();

    m_state = DataState;

    if (haveBufferedCharacterToken()) {
        // Emit the character token we already have.
        // The next call to processToken will flush the buffered end tag and emit it.
        return true;
    }

    flushBufferedEndTag();
    return true;
}

bool HTMLTokenizer::processToken(SegmentedString& source)
{
    if (!m_bufferedEndTagName.isEmpty() && !inEndTagBufferingState()) {
        // We are back here after emitting a character token that came just before an end tag.
        // To continue parsing the end tag we need to move the buffered tag name into the token.
        flushBufferedEndTag();

        // If we are in the data state, the end tag is already complete and we should emit it
        // now, otherwise, we want to resume parsing the partial end tag.
        if (m_state == DataState)
            return true;
    }

    if (!m_preprocessor.peek(source, isNullCharacterSkippingState(m_state)))
        return haveBufferedCharacterToken();
    UChar character = m_preprocessor.nextInputCharacter();

    // https://html.spec.whatwg.org/#tokenization
    switch (m_state) {

    BEGIN_STATE(DataState)
        if (character == '&')
            ADVANCE_PAST_NON_NEWLINE_TO(CharacterReferenceInDataState);
        if (character == '<') {
            if (haveBufferedCharacterToken())
                RETURN_IN_CURRENT_STATE(true);
            ADVANCE_PAST_NON_NEWLINE_TO(TagOpenState);
        }
        if (character == kEndOfFileMarker)
            return emitEndOfFile(source);
        bufferCharacter(character);
        ADVANCE_TO(DataState);
    END_STATE()

    BEGIN_STATE(CharacterReferenceInDataState)
        if (!processEntity(source))
            RETURN_IN_CURRENT_STATE(haveBufferedCharacterToken());
        SWITCH_TO(DataState);
    END_STATE()

    BEGIN_STATE(RCDATAState)
        if (character == '&')
            ADVANCE_PAST_NON_NEWLINE_TO(CharacterReferenceInRCDATAState);
        if (character == '<')
            ADVANCE_PAST_NON_NEWLINE_TO(RCDATALessThanSignState);
        if (character == kEndOfFileMarker)
            RECONSUME_IN(DataState);
        bufferCharacter(character);
        ADVANCE_TO(RCDATAState);
    END_STATE()

    BEGIN_STATE(CharacterReferenceInRCDATAState)
        if (!processEntity(source))
            RETURN_IN_CURRENT_STATE(haveBufferedCharacterToken());
        SWITCH_TO(RCDATAState);
    END_STATE()

    BEGIN_STATE(RAWTEXTState)
        if (character == '<')
            ADVANCE_PAST_NON_NEWLINE_TO(RAWTEXTLessThanSignState);
        if (character == kEndOfFileMarker)
            RECONSUME_IN(DataState);
        bufferCharacter(character);
        ADVANCE_TO(RAWTEXTState);
    END_STATE()

    BEGIN_STATE(ScriptDataState)
        if (character == '<')
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataLessThanSignState);
        if (character == kEndOfFileMarker)
            RECONSUME_IN(DataState);
        bufferCharacter(character);
        ADVANCE_TO(ScriptDataState);
    END_STATE()

    BEGIN_STATE(PLAINTEXTState)
        if (character == kEndOfFileMarker)
            RECONSUME_IN(DataState);
        bufferCharacter(character);
        ADVANCE_TO(PLAINTEXTState);
    END_STATE()

    BEGIN_STATE(TagOpenState)
        if (character == '!')
            ADVANCE_PAST_NON_NEWLINE_TO(MarkupDeclarationOpenState);
        if (character == '/')
            ADVANCE_PAST_NON_NEWLINE_TO(EndTagOpenState);
        if (isASCIIAlpha(character)) {
            m_token.beginStartTag(convertASCIIAlphaToLower(character));
            ADVANCE_PAST_NON_NEWLINE_TO(TagNameState);
        }
        if (character == '?') {
            parseError();
            // The spec consumes the current character before switching
            // to the bogus comment state, but it's easier to implement
            // if we reconsume the current character.
            RECONSUME_IN(BogusCommentState);
        }
        parseError();
        bufferASCIICharacter('<');
        RECONSUME_IN(DataState);
    END_STATE()

    BEGIN_STATE(EndTagOpenState)
        if (isASCIIAlpha(character)) {
            m_token.beginEndTag(convertASCIIAlphaToLower(character));
            m_appropriateEndTagName.clear();
            ADVANCE_PAST_NON_NEWLINE_TO(TagNameState);
        }
        if (character == '>') {
            parseError();
            ADVANCE_PAST_NON_NEWLINE_TO(DataState);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            bufferASCIICharacter('<');
            bufferASCIICharacter('/');
            RECONSUME_IN(DataState);
        }
        parseError();
        RECONSUME_IN(BogusCommentState);
    END_STATE()

    BEGIN_STATE(TagNameState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(BeforeAttributeNameState);
        if (character == '/')
            ADVANCE_PAST_NON_NEWLINE_TO(SelfClosingStartTagState);
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (m_options.usePreHTML5ParserQuirks && character == '<')
            return emitAndReconsumeInDataState();
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        m_token.appendToName(toASCIILower(character));
        ADVANCE_PAST_NON_NEWLINE_TO(TagNameState);
    END_STATE()

    BEGIN_STATE(RCDATALessThanSignState)
        if (character == '/') {
            m_temporaryBuffer.clear();
            ASSERT(m_bufferedEndTagName.isEmpty());
            ADVANCE_PAST_NON_NEWLINE_TO(RCDATAEndTagOpenState);
        }
        bufferASCIICharacter('<');
        RECONSUME_IN(RCDATAState);
    END_STATE()

    BEGIN_STATE(RCDATAEndTagOpenState)
        if (isASCIIAlpha(character)) {
            appendToTemporaryBuffer(character);
            appendToPossibleEndTag(convertASCIIAlphaToLower(character));
            ADVANCE_PAST_NON_NEWLINE_TO(RCDATAEndTagNameState);
        }
        bufferASCIICharacter('<');
        bufferASCIICharacter('/');
        RECONSUME_IN(RCDATAState);
    END_STATE()

    BEGIN_STATE(RCDATAEndTagNameState)
        if (isASCIIAlpha(character)) {
            appendToTemporaryBuffer(character);
            appendToPossibleEndTag(convertASCIIAlphaToLower(character));
            ADVANCE_PAST_NON_NEWLINE_TO(RCDATAEndTagNameState);
        }
        if (isTokenizerWhitespace(character)) {
            if (isAppropriateEndTag()) {
                if (commitToPartialEndTag(source, character, BeforeAttributeNameState))
                    return true;
                SWITCH_TO(BeforeAttributeNameState);
            }
        } else if (character == '/') {
            if (isAppropriateEndTag()) {
                if (commitToPartialEndTag(source, '/', SelfClosingStartTagState))
                    return true;
                SWITCH_TO(SelfClosingStartTagState);
            }
        } else if (character == '>') {
            if (isAppropriateEndTag())
                return commitToCompleteEndTag(source);
        }
        bufferASCIICharacter('<');
        bufferASCIICharacter('/');
        m_token.appendToCharacter(m_temporaryBuffer);
        m_bufferedEndTagName.clear();
        m_temporaryBuffer.clear();
        RECONSUME_IN(RCDATAState);
    END_STATE()

    BEGIN_STATE(RAWTEXTLessThanSignState)
        if (character == '/') {
            m_temporaryBuffer.clear();
            ASSERT(m_bufferedEndTagName.isEmpty());
            ADVANCE_PAST_NON_NEWLINE_TO(RAWTEXTEndTagOpenState);
        }
        bufferASCIICharacter('<');
        RECONSUME_IN(RAWTEXTState);
    END_STATE()

    BEGIN_STATE(RAWTEXTEndTagOpenState)
        if (isASCIIAlpha(character)) {
            appendToTemporaryBuffer(character);
            appendToPossibleEndTag(convertASCIIAlphaToLower(character));
            ADVANCE_PAST_NON_NEWLINE_TO(RAWTEXTEndTagNameState);
        }
        bufferASCIICharacter('<');
        bufferASCIICharacter('/');
        RECONSUME_IN(RAWTEXTState);
    END_STATE()

    BEGIN_STATE(RAWTEXTEndTagNameState)
        if (isASCIIAlpha(character)) {
            appendToTemporaryBuffer(character);
            appendToPossibleEndTag(convertASCIIAlphaToLower(character));
            ADVANCE_PAST_NON_NEWLINE_TO(RAWTEXTEndTagNameState);
        }
        if (isTokenizerWhitespace(character)) {
            if (isAppropriateEndTag()) {
                if (commitToPartialEndTag(source, character, BeforeAttributeNameState))
                    return true;
                SWITCH_TO(BeforeAttributeNameState);
            }
        } else if (character == '/') {
            if (isAppropriateEndTag()) {
                if (commitToPartialEndTag(source, '/', SelfClosingStartTagState))
                    return true;
                SWITCH_TO(SelfClosingStartTagState);
            }
        } else if (character == '>') {
            if (isAppropriateEndTag())
                return commitToCompleteEndTag(source);
        }
        bufferASCIICharacter('<');
        bufferASCIICharacter('/');
        m_token.appendToCharacter(m_temporaryBuffer);
        m_bufferedEndTagName.clear();
        m_temporaryBuffer.clear();
        RECONSUME_IN(RAWTEXTState);
    END_STATE()

    BEGIN_STATE(ScriptDataLessThanSignState)
        if (character == '/') {
            m_temporaryBuffer.clear();
            ASSERT(m_bufferedEndTagName.isEmpty());
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEndTagOpenState);
        }
        if (character == '!') {
            bufferASCIICharacter('<');
            bufferASCIICharacter('!');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEscapeStartState);
        }
        bufferASCIICharacter('<');
        RECONSUME_IN(ScriptDataState);
    END_STATE()

    BEGIN_STATE(ScriptDataEndTagOpenState)
        if (isASCIIAlpha(character)) {
            appendToTemporaryBuffer(character);
            appendToPossibleEndTag(convertASCIIAlphaToLower(character));
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEndTagNameState);
        }
        bufferASCIICharacter('<');
        bufferASCIICharacter('/');
        RECONSUME_IN(ScriptDataState);
    END_STATE()

    BEGIN_STATE(ScriptDataEndTagNameState)
        if (isASCIIAlpha(character)) {
            appendToTemporaryBuffer(character);
            appendToPossibleEndTag(convertASCIIAlphaToLower(character));
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEndTagNameState);
        }
        if (isTokenizerWhitespace(character)) {
            if (isAppropriateEndTag()) {
                if (commitToPartialEndTag(source, character, BeforeAttributeNameState))
                    return true;
                SWITCH_TO(BeforeAttributeNameState);
            }
        } else if (character == '/') {
            if (isAppropriateEndTag()) {
                if (commitToPartialEndTag(source, '/', SelfClosingStartTagState))
                    return true;
                SWITCH_TO(SelfClosingStartTagState);
            }
        } else if (character == '>') {
            if (isAppropriateEndTag())
                return commitToCompleteEndTag(source);
        }
        bufferASCIICharacter('<');
        bufferASCIICharacter('/');
        m_token.appendToCharacter(m_temporaryBuffer);
        m_bufferedEndTagName.clear();
        m_temporaryBuffer.clear();
        RECONSUME_IN(ScriptDataState);
    END_STATE()

    BEGIN_STATE(ScriptDataEscapeStartState)
        if (character == '-') {
            bufferASCIICharacter('-');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEscapeStartDashState);
        } else
            RECONSUME_IN(ScriptDataState);
    END_STATE()

    BEGIN_STATE(ScriptDataEscapeStartDashState)
        if (character == '-') {
            bufferASCIICharacter('-');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEscapedDashDashState);
        } else
            RECONSUME_IN(ScriptDataState);
    END_STATE()

    BEGIN_STATE(ScriptDataEscapedState)
        if (character == '-') {
            bufferASCIICharacter('-');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEscapedDashState);
        }
        if (character == '<')
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEscapedLessThanSignState);
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        bufferCharacter(character);
        ADVANCE_TO(ScriptDataEscapedState);
    END_STATE()

    BEGIN_STATE(ScriptDataEscapedDashState)
        if (character == '-') {
            bufferASCIICharacter('-');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEscapedDashDashState);
        }
        if (character == '<')
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEscapedLessThanSignState);
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        bufferCharacter(character);
        ADVANCE_TO(ScriptDataEscapedState);
    END_STATE()

    BEGIN_STATE(ScriptDataEscapedDashDashState)
        if (character == '-') {
            bufferASCIICharacter('-');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEscapedDashDashState);
        }
        if (character == '<')
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEscapedLessThanSignState);
        if (character == '>') {
            bufferASCIICharacter('>');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataState);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        bufferCharacter(character);
        ADVANCE_TO(ScriptDataEscapedState);
    END_STATE()

    BEGIN_STATE(ScriptDataEscapedLessThanSignState)
        if (character == '/') {
            m_temporaryBuffer.clear();
            ASSERT(m_bufferedEndTagName.isEmpty());
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEscapedEndTagOpenState);
        }
        if (isASCIIAlpha(character)) {
            bufferASCIICharacter('<');
            bufferASCIICharacter(character);
            m_temporaryBuffer.clear();
            appendToTemporaryBuffer(convertASCIIAlphaToLower(character));
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataDoubleEscapeStartState);
        }
        bufferASCIICharacter('<');
        RECONSUME_IN(ScriptDataEscapedState);
    END_STATE()

    BEGIN_STATE(ScriptDataEscapedEndTagOpenState)
        if (isASCIIAlpha(character)) {
            appendToTemporaryBuffer(character);
            appendToPossibleEndTag(convertASCIIAlphaToLower(character));
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEscapedEndTagNameState);
        }
        bufferASCIICharacter('<');
        bufferASCIICharacter('/');
        RECONSUME_IN(ScriptDataEscapedState);
    END_STATE()

    BEGIN_STATE(ScriptDataEscapedEndTagNameState)
        if (isASCIIAlpha(character)) {
            appendToTemporaryBuffer(character);
            appendToPossibleEndTag(convertASCIIAlphaToLower(character));
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataEscapedEndTagNameState);
        }
        if (isTokenizerWhitespace(character)) {
            if (isAppropriateEndTag()) {
                if (commitToPartialEndTag(source, character, BeforeAttributeNameState))
                    return true;
                SWITCH_TO(BeforeAttributeNameState);
            }
        } else if (character == '/') {
            if (isAppropriateEndTag()) {
                if (commitToPartialEndTag(source, '/', SelfClosingStartTagState))
                    return true;
                SWITCH_TO(SelfClosingStartTagState);
            }
        } else if (character == '>') {
            if (isAppropriateEndTag())
                return commitToCompleteEndTag(source);
        }
        bufferASCIICharacter('<');
        bufferASCIICharacter('/');
        m_token.appendToCharacter(m_temporaryBuffer);
        m_bufferedEndTagName.clear();
        m_temporaryBuffer.clear();
        RECONSUME_IN(ScriptDataEscapedState);
    END_STATE()

    BEGIN_STATE(ScriptDataDoubleEscapeStartState)
        if (isTokenizerWhitespace(character) || character == '/' || character == '>') {
            bufferASCIICharacter(character);
            if (temporaryBufferIs("script"))
                ADVANCE_TO(ScriptDataDoubleEscapedState);
            else
                ADVANCE_TO(ScriptDataEscapedState);
        }
        if (isASCIIAlpha(character)) {
            bufferASCIICharacter(character);
            appendToTemporaryBuffer(convertASCIIAlphaToLower(character));
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataDoubleEscapeStartState);
        }
        RECONSUME_IN(ScriptDataEscapedState);
    END_STATE()

    BEGIN_STATE(ScriptDataDoubleEscapedState)
        if (character == '-') {
            bufferASCIICharacter('-');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataDoubleEscapedDashState);
        }
        if (character == '<') {
            bufferASCIICharacter('<');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataDoubleEscapedLessThanSignState);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        bufferCharacter(character);
        ADVANCE_TO(ScriptDataDoubleEscapedState);
    END_STATE()

    BEGIN_STATE(ScriptDataDoubleEscapedDashState)
        if (character == '-') {
            bufferASCIICharacter('-');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataDoubleEscapedDashDashState);
        }
        if (character == '<') {
            bufferASCIICharacter('<');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataDoubleEscapedLessThanSignState);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        bufferCharacter(character);
        ADVANCE_TO(ScriptDataDoubleEscapedState);
    END_STATE()

    BEGIN_STATE(ScriptDataDoubleEscapedDashDashState)
        if (character == '-') {
            bufferASCIICharacter('-');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataDoubleEscapedDashDashState);
        }
        if (character == '<') {
            bufferASCIICharacter('<');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataDoubleEscapedLessThanSignState);
        }
        if (character == '>') {
            bufferASCIICharacter('>');
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataState);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        bufferCharacter(character);
        ADVANCE_TO(ScriptDataDoubleEscapedState);
    END_STATE()

    BEGIN_STATE(ScriptDataDoubleEscapedLessThanSignState)
        if (character == '/') {
            bufferASCIICharacter('/');
            m_temporaryBuffer.clear();
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataDoubleEscapeEndState);
        }
        RECONSUME_IN(ScriptDataDoubleEscapedState);
    END_STATE()

    BEGIN_STATE(ScriptDataDoubleEscapeEndState)
        if (isTokenizerWhitespace(character) || character == '/' || character == '>') {
            bufferASCIICharacter(character);
            if (temporaryBufferIs("script"))
                ADVANCE_TO(ScriptDataEscapedState);
            else
                ADVANCE_TO(ScriptDataDoubleEscapedState);
        }
        if (isASCIIAlpha(character)) {
            bufferASCIICharacter(character);
            appendToTemporaryBuffer(convertASCIIAlphaToLower(character));
            ADVANCE_PAST_NON_NEWLINE_TO(ScriptDataDoubleEscapeEndState);
        }
        RECONSUME_IN(ScriptDataDoubleEscapedState);
    END_STATE()

    BEGIN_STATE(BeforeAttributeNameState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(BeforeAttributeNameState);
        if (character == '/')
            ADVANCE_PAST_NON_NEWLINE_TO(SelfClosingStartTagState);
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (m_options.usePreHTML5ParserQuirks && character == '<')
            return emitAndReconsumeInDataState();
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        if (character == '"' || character == '\'' || character == '<' || character == '=')
            parseError();
        m_token.beginAttribute(source.numberOfCharactersConsumed());
        m_token.appendToAttributeName(toASCIILower(character));
        ADVANCE_PAST_NON_NEWLINE_TO(AttributeNameState);
    END_STATE()

    BEGIN_STATE(AttributeNameState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(AfterAttributeNameState);
        if (character == '/')
            ADVANCE_PAST_NON_NEWLINE_TO(SelfClosingStartTagState);
        if (character == '=')
            ADVANCE_PAST_NON_NEWLINE_TO(BeforeAttributeValueState);
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (m_options.usePreHTML5ParserQuirks && character == '<')
            return emitAndReconsumeInDataState();
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        if (character == '"' || character == '\'' || character == '<' || character == '=')
            parseError();
        m_token.appendToAttributeName(toASCIILower(character));
        ADVANCE_PAST_NON_NEWLINE_TO(AttributeNameState);
    END_STATE()

    BEGIN_STATE(AfterAttributeNameState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(AfterAttributeNameState);
        if (character == '/')
            ADVANCE_PAST_NON_NEWLINE_TO(SelfClosingStartTagState);
        if (character == '=')
            ADVANCE_PAST_NON_NEWLINE_TO(BeforeAttributeValueState);
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (m_options.usePreHTML5ParserQuirks && character == '<')
            return emitAndReconsumeInDataState();
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        if (character == '"' || character == '\'' || character == '<')
            parseError();
        m_token.beginAttribute(source.numberOfCharactersConsumed());
        m_token.appendToAttributeName(toASCIILower(character));
        ADVANCE_PAST_NON_NEWLINE_TO(AttributeNameState);
    END_STATE()

    BEGIN_STATE(BeforeAttributeValueState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(BeforeAttributeValueState);
        if (character == '"')
            ADVANCE_PAST_NON_NEWLINE_TO(AttributeValueDoubleQuotedState);
        if (character == '&')
            RECONSUME_IN(AttributeValueUnquotedState);
        if (character == '\'')
            ADVANCE_PAST_NON_NEWLINE_TO(AttributeValueSingleQuotedState);
        if (character == '>') {
            parseError();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        if (character == '<' || character == '=' || character == '`')
            parseError();
        m_token.appendToAttributeValue(character);
        ADVANCE_PAST_NON_NEWLINE_TO(AttributeValueUnquotedState);
    END_STATE()

    BEGIN_STATE(AttributeValueDoubleQuotedState)
        if (character == '"') {
            m_token.endAttribute(source.numberOfCharactersConsumed());
            ADVANCE_PAST_NON_NEWLINE_TO(AfterAttributeValueQuotedState);
        }
        if (character == '&') {
            m_additionalAllowedCharacter = '"';
            ADVANCE_PAST_NON_NEWLINE_TO(CharacterReferenceInAttributeValueState);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.endAttribute(source.numberOfCharactersConsumed());
            RECONSUME_IN(DataState);
        }
        m_token.appendToAttributeValue(character);
        ADVANCE_TO(AttributeValueDoubleQuotedState);
    END_STATE()

    BEGIN_STATE(AttributeValueSingleQuotedState)
        if (character == '\'') {
            m_token.endAttribute(source.numberOfCharactersConsumed());
            ADVANCE_PAST_NON_NEWLINE_TO(AfterAttributeValueQuotedState);
        }
        if (character == '&') {
            m_additionalAllowedCharacter = '\'';
            ADVANCE_PAST_NON_NEWLINE_TO(CharacterReferenceInAttributeValueState);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.endAttribute(source.numberOfCharactersConsumed());
            RECONSUME_IN(DataState);
        }
        m_token.appendToAttributeValue(character);
        ADVANCE_TO(AttributeValueSingleQuotedState);
    END_STATE()

    BEGIN_STATE(AttributeValueUnquotedState)
        if (isTokenizerWhitespace(character)) {
            m_token.endAttribute(source.numberOfCharactersConsumed());
            ADVANCE_TO(BeforeAttributeNameState);
        }
        if (character == '&') {
            m_additionalAllowedCharacter = '>';
            ADVANCE_PAST_NON_NEWLINE_TO(CharacterReferenceInAttributeValueState);
        }
        if (character == '>') {
            m_token.endAttribute(source.numberOfCharactersConsumed());
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.endAttribute(source.numberOfCharactersConsumed());
            RECONSUME_IN(DataState);
        }
        if (character == '"' || character == '\'' || character == '<' || character == '=' || character == '`')
            parseError();
        m_token.appendToAttributeValue(character);
        ADVANCE_PAST_NON_NEWLINE_TO(AttributeValueUnquotedState);
    END_STATE()

    BEGIN_STATE(CharacterReferenceInAttributeValueState)
        bool notEnoughCharacters = false;
        StringBuilder decodedEntity;
        bool success = consumeHTMLEntity(source, decodedEntity, notEnoughCharacters, m_additionalAllowedCharacter);
        if (notEnoughCharacters)
            RETURN_IN_CURRENT_STATE(haveBufferedCharacterToken());
        if (!success) {
            ASSERT(decodedEntity.isEmpty());
            m_token.appendToAttributeValue('&');
        } else {
            for (unsigned i = 0; i < decodedEntity.length(); ++i)
                m_token.appendToAttributeValue(decodedEntity[i]);
        }
        // We're supposed to switch back to the attribute value state that
        // we were in when we were switched into this state. Rather than
        // keeping track of this explictly, we observe that the previous
        // state can be determined by m_additionalAllowedCharacter.
        if (m_additionalAllowedCharacter == '"')
            SWITCH_TO(AttributeValueDoubleQuotedState);
        if (m_additionalAllowedCharacter == '\'')
            SWITCH_TO(AttributeValueSingleQuotedState);
        ASSERT(m_additionalAllowedCharacter == '>');
        SWITCH_TO(AttributeValueUnquotedState);
    END_STATE()

    BEGIN_STATE(AfterAttributeValueQuotedState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(BeforeAttributeNameState);
        if (character == '/')
            ADVANCE_PAST_NON_NEWLINE_TO(SelfClosingStartTagState);
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (m_options.usePreHTML5ParserQuirks && character == '<')
            return emitAndReconsumeInDataState();
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        parseError();
        RECONSUME_IN(BeforeAttributeNameState);
    END_STATE()

    BEGIN_STATE(SelfClosingStartTagState)
        if (character == '>') {
            m_token.setSelfClosing();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            RECONSUME_IN(DataState);
        }
        parseError();
        RECONSUME_IN(BeforeAttributeNameState);
    END_STATE()

    BEGIN_STATE(BogusCommentState)
        m_token.beginComment();
        RECONSUME_IN(ContinueBogusCommentState);
    END_STATE()

    BEGIN_STATE(ContinueBogusCommentState)
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (character == kEndOfFileMarker)
            return emitAndReconsumeInDataState();
        m_token.appendToComment(character);
        ADVANCE_TO(ContinueBogusCommentState);
    END_STATE()

    BEGIN_STATE(MarkupDeclarationOpenState)
        if (character == '-') {
            auto result = source.advancePast("--");
            if (result == SegmentedString::DidMatch) {
                m_token.beginComment();
                SWITCH_TO(CommentStartState);
            }
            if (result == SegmentedString::NotEnoughCharacters)
                RETURN_IN_CURRENT_STATE(haveBufferedCharacterToken());
        } else if (isASCIIAlphaCaselessEqual(character, 'd')) {
            auto result = source.advancePastLettersIgnoringASCIICase("doctype");
            if (result == SegmentedString::DidMatch)
                SWITCH_TO(DOCTYPEState);
            if (result == SegmentedString::NotEnoughCharacters)
                RETURN_IN_CURRENT_STATE(haveBufferedCharacterToken());
        } else if (character == '[' && shouldAllowCDATA()) {
            auto result = source.advancePast("[CDATA[");
            if (result == SegmentedString::DidMatch)
                SWITCH_TO(CDATASectionState);
            if (result == SegmentedString::NotEnoughCharacters)
                RETURN_IN_CURRENT_STATE(haveBufferedCharacterToken());
        }
        parseError();
        RECONSUME_IN(BogusCommentState);
    END_STATE()

    BEGIN_STATE(CommentStartState)
        if (character == '-')
            ADVANCE_PAST_NON_NEWLINE_TO(CommentStartDashState);
        if (character == '>') {
            parseError();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            return emitAndReconsumeInDataState();
        }
        m_token.appendToComment(character);
        ADVANCE_TO(CommentState);
    END_STATE()

    BEGIN_STATE(CommentStartDashState)
        if (character == '-')
            ADVANCE_PAST_NON_NEWLINE_TO(CommentEndState);
        if (character == '>') {
            parseError();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            return emitAndReconsumeInDataState();
        }
        m_token.appendToComment('-');
        m_token.appendToComment(character);
        ADVANCE_TO(CommentState);
    END_STATE()

    BEGIN_STATE(CommentState)
        if (character == '-')
            ADVANCE_PAST_NON_NEWLINE_TO(CommentEndDashState);
        if (character == kEndOfFileMarker) {
            parseError();
            return emitAndReconsumeInDataState();
        }
        m_token.appendToComment(character);
        ADVANCE_TO(CommentState);
    END_STATE()

    BEGIN_STATE(CommentEndDashState)
        if (character == '-')
            ADVANCE_PAST_NON_NEWLINE_TO(CommentEndState);
        if (character == kEndOfFileMarker) {
            parseError();
            return emitAndReconsumeInDataState();
        }
        m_token.appendToComment('-');
        m_token.appendToComment(character);
        ADVANCE_TO(CommentState);
    END_STATE()

    BEGIN_STATE(CommentEndState)
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (character == '!') {
            parseError();
            ADVANCE_PAST_NON_NEWLINE_TO(CommentEndBangState);
        }
        if (character == '-') {
            parseError();
            m_token.appendToComment('-');
            ADVANCE_PAST_NON_NEWLINE_TO(CommentEndState);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            return emitAndReconsumeInDataState();
        }
        parseError();
        m_token.appendToComment('-');
        m_token.appendToComment('-');
        m_token.appendToComment(character);
        ADVANCE_TO(CommentState);
    END_STATE()

    BEGIN_STATE(CommentEndBangState)
        if (character == '-') {
            m_token.appendToComment('-');
            m_token.appendToComment('-');
            m_token.appendToComment('!');
            ADVANCE_PAST_NON_NEWLINE_TO(CommentEndDashState);
        }
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (character == kEndOfFileMarker) {
            parseError();
            return emitAndReconsumeInDataState();
        }
        m_token.appendToComment('-');
        m_token.appendToComment('-');
        m_token.appendToComment('!');
        m_token.appendToComment(character);
        ADVANCE_TO(CommentState);
    END_STATE()

    BEGIN_STATE(DOCTYPEState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(BeforeDOCTYPENameState);
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.beginDOCTYPE();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        parseError();
        RECONSUME_IN(BeforeDOCTYPENameState);
    END_STATE()

    BEGIN_STATE(BeforeDOCTYPENameState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(BeforeDOCTYPENameState);
        if (character == '>') {
            parseError();
            m_token.beginDOCTYPE();
            m_token.setForceQuirks();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.beginDOCTYPE();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        m_token.beginDOCTYPE(toASCIILower(character));
        ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPENameState);
    END_STATE()

    BEGIN_STATE(DOCTYPENameState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(AfterDOCTYPENameState);
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        m_token.appendToName(toASCIILower(character));
        ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPENameState);
    END_STATE()

    BEGIN_STATE(AfterDOCTYPENameState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(AfterDOCTYPENameState);
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        if (isASCIIAlphaCaselessEqual(character, 'p')) {
            auto result = source.advancePastLettersIgnoringASCIICase("public");
            if (result == SegmentedString::DidMatch)
                SWITCH_TO(AfterDOCTYPEPublicKeywordState);
            if (result == SegmentedString::NotEnoughCharacters)
                RETURN_IN_CURRENT_STATE(haveBufferedCharacterToken());
        } else if (isASCIIAlphaCaselessEqual(character, 's')) {
            auto result = source.advancePastLettersIgnoringASCIICase("system");
            if (result == SegmentedString::DidMatch)
                SWITCH_TO(AfterDOCTYPESystemKeywordState);
            if (result == SegmentedString::NotEnoughCharacters)
                RETURN_IN_CURRENT_STATE(haveBufferedCharacterToken());
        }
        parseError();
        m_token.setForceQuirks();
        ADVANCE_PAST_NON_NEWLINE_TO(BogusDOCTYPEState);
    END_STATE()

    BEGIN_STATE(AfterDOCTYPEPublicKeywordState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(BeforeDOCTYPEPublicIdentifierState);
        if (character == '"') {
            parseError();
            m_token.setPublicIdentifierToEmptyString();
            ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPEPublicIdentifierDoubleQuotedState);
        }
        if (character == '\'') {
            parseError();
            m_token.setPublicIdentifierToEmptyString();
            ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPEPublicIdentifierSingleQuotedState);
        }
        if (character == '>') {
            parseError();
            m_token.setForceQuirks();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        parseError();
        m_token.setForceQuirks();
        ADVANCE_PAST_NON_NEWLINE_TO(BogusDOCTYPEState);
    END_STATE()

    BEGIN_STATE(BeforeDOCTYPEPublicIdentifierState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(BeforeDOCTYPEPublicIdentifierState);
        if (character == '"') {
            m_token.setPublicIdentifierToEmptyString();
            ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPEPublicIdentifierDoubleQuotedState);
        }
        if (character == '\'') {
            m_token.setPublicIdentifierToEmptyString();
            ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPEPublicIdentifierSingleQuotedState);
        }
        if (character == '>') {
            parseError();
            m_token.setForceQuirks();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        parseError();
        m_token.setForceQuirks();
        ADVANCE_PAST_NON_NEWLINE_TO(BogusDOCTYPEState);
    END_STATE()

    BEGIN_STATE(DOCTYPEPublicIdentifierDoubleQuotedState)
        if (character == '"')
            ADVANCE_PAST_NON_NEWLINE_TO(AfterDOCTYPEPublicIdentifierState);
        if (character == '>') {
            parseError();
            m_token.setForceQuirks();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        m_token.appendToPublicIdentifier(character);
        ADVANCE_TO(DOCTYPEPublicIdentifierDoubleQuotedState);
    END_STATE()

    BEGIN_STATE(DOCTYPEPublicIdentifierSingleQuotedState)
        if (character == '\'')
            ADVANCE_PAST_NON_NEWLINE_TO(AfterDOCTYPEPublicIdentifierState);
        if (character == '>') {
            parseError();
            m_token.setForceQuirks();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        m_token.appendToPublicIdentifier(character);
        ADVANCE_TO(DOCTYPEPublicIdentifierSingleQuotedState);
    END_STATE()

    BEGIN_STATE(AfterDOCTYPEPublicIdentifierState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(BetweenDOCTYPEPublicAndSystemIdentifiersState);
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (character == '"') {
            parseError();
            m_token.setSystemIdentifierToEmptyString();
            ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPESystemIdentifierDoubleQuotedState);
        }
        if (character == '\'') {
            parseError();
            m_token.setSystemIdentifierToEmptyString();
            ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPESystemIdentifierSingleQuotedState);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        parseError();
        m_token.setForceQuirks();
        ADVANCE_PAST_NON_NEWLINE_TO(BogusDOCTYPEState);
    END_STATE()

    BEGIN_STATE(BetweenDOCTYPEPublicAndSystemIdentifiersState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(BetweenDOCTYPEPublicAndSystemIdentifiersState);
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (character == '"') {
            m_token.setSystemIdentifierToEmptyString();
            ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPESystemIdentifierDoubleQuotedState);
        }
        if (character == '\'') {
            m_token.setSystemIdentifierToEmptyString();
            ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPESystemIdentifierSingleQuotedState);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        parseError();
        m_token.setForceQuirks();
        ADVANCE_PAST_NON_NEWLINE_TO(BogusDOCTYPEState);
    END_STATE()

    BEGIN_STATE(AfterDOCTYPESystemKeywordState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(BeforeDOCTYPESystemIdentifierState);
        if (character == '"') {
            parseError();
            m_token.setSystemIdentifierToEmptyString();
            ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPESystemIdentifierDoubleQuotedState);
        }
        if (character == '\'') {
            parseError();
            m_token.setSystemIdentifierToEmptyString();
            ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPESystemIdentifierSingleQuotedState);
        }
        if (character == '>') {
            parseError();
            m_token.setForceQuirks();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        parseError();
        m_token.setForceQuirks();
        ADVANCE_PAST_NON_NEWLINE_TO(BogusDOCTYPEState);
    END_STATE()

    BEGIN_STATE(BeforeDOCTYPESystemIdentifierState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(BeforeDOCTYPESystemIdentifierState);
        if (character == '"') {
            m_token.setSystemIdentifierToEmptyString();
            ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPESystemIdentifierDoubleQuotedState);
        }
        if (character == '\'') {
            m_token.setSystemIdentifierToEmptyString();
            ADVANCE_PAST_NON_NEWLINE_TO(DOCTYPESystemIdentifierSingleQuotedState);
        }
        if (character == '>') {
            parseError();
            m_token.setForceQuirks();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        parseError();
        m_token.setForceQuirks();
        ADVANCE_PAST_NON_NEWLINE_TO(BogusDOCTYPEState);
    END_STATE()

    BEGIN_STATE(DOCTYPESystemIdentifierDoubleQuotedState)
        if (character == '"')
            ADVANCE_PAST_NON_NEWLINE_TO(AfterDOCTYPESystemIdentifierState);
        if (character == '>') {
            parseError();
            m_token.setForceQuirks();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        m_token.appendToSystemIdentifier(character);
        ADVANCE_TO(DOCTYPESystemIdentifierDoubleQuotedState);
    END_STATE()

    BEGIN_STATE(DOCTYPESystemIdentifierSingleQuotedState)
        if (character == '\'')
            ADVANCE_PAST_NON_NEWLINE_TO(AfterDOCTYPESystemIdentifierState);
        if (character == '>') {
            parseError();
            m_token.setForceQuirks();
            return emitAndResumeInDataState(source);
        }
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        m_token.appendToSystemIdentifier(character);
        ADVANCE_TO(DOCTYPESystemIdentifierSingleQuotedState);
    END_STATE()

    BEGIN_STATE(AfterDOCTYPESystemIdentifierState)
        if (isTokenizerWhitespace(character))
            ADVANCE_TO(AfterDOCTYPESystemIdentifierState);
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (character == kEndOfFileMarker) {
            parseError();
            m_token.setForceQuirks();
            return emitAndReconsumeInDataState();
        }
        parseError();
        ADVANCE_PAST_NON_NEWLINE_TO(BogusDOCTYPEState);
    END_STATE()

    BEGIN_STATE(BogusDOCTYPEState)
        if (character == '>')
            return emitAndResumeInDataState(source);
        if (character == kEndOfFileMarker)
            return emitAndReconsumeInDataState();
        ADVANCE_TO(BogusDOCTYPEState);
    END_STATE()

    BEGIN_STATE(CDATASectionState)
        if (character == ']')
            ADVANCE_PAST_NON_NEWLINE_TO(CDATASectionRightSquareBracketState);
        if (character == kEndOfFileMarker)
            RECONSUME_IN(DataState);
        bufferCharacter(character);
        ADVANCE_TO(CDATASectionState);
    END_STATE()

    BEGIN_STATE(CDATASectionRightSquareBracketState)
        if (character == ']')
            ADVANCE_PAST_NON_NEWLINE_TO(CDATASectionDoubleRightSquareBracketState);
        bufferASCIICharacter(']');
        RECONSUME_IN(CDATASectionState);
    END_STATE()

    BEGIN_STATE(CDATASectionDoubleRightSquareBracketState)
        if (character == '>')
            ADVANCE_PAST_NON_NEWLINE_TO(DataState);
        bufferASCIICharacter(']');
        bufferASCIICharacter(']');
        RECONSUME_IN(CDATASectionState);
    END_STATE()

    }

    ASSERT_NOT_REACHED();
    return false;
}

String HTMLTokenizer::bufferedCharacters() const
{
    // FIXME: Add an assert about m_state.
    StringBuilder characters;
    characters.reserveCapacity(numberOfBufferedCharacters());
    characters.append('<');
    characters.append('/');
    characters.appendCharacters(m_temporaryBuffer.data(), m_temporaryBuffer.size());
    return characters.toString();
}

void HTMLTokenizer::updateStateFor(const AtomString& tagName)
{
    if (tagName == textareaTag || tagName == titleTag)
        m_state = RCDATAState;
    else if (tagName == plaintextTag)
        m_state = PLAINTEXTState;
    else if (tagName == scriptTag)
        m_state = ScriptDataState;
    else if (tagName == styleTag
        || tagName == iframeTag
        || tagName == xmpTag
        || (tagName == noembedTag)
        || tagName == noframesTag
        || (tagName == noscriptTag && m_options.scriptEnabled))
        m_state = RAWTEXTState;
}

inline void HTMLTokenizer::appendToTemporaryBuffer(UChar character)
{
    ASSERT(isASCII(character));
    m_temporaryBuffer.append(character);
}

inline bool HTMLTokenizer::temporaryBufferIs(const char* expectedString)
{
    return vectorEqualsString(m_temporaryBuffer, expectedString);
}

inline void HTMLTokenizer::appendToPossibleEndTag(UChar character)
{
    ASSERT(isASCII(character));
    m_bufferedEndTagName.append(character);
}

inline bool HTMLTokenizer::isAppropriateEndTag() const
{
    if (m_bufferedEndTagName.size() != m_appropriateEndTagName.size())
        return false;

    unsigned size = m_bufferedEndTagName.size();

    for (unsigned i = 0; i < size; i++) {
        if (m_bufferedEndTagName[i] != m_appropriateEndTagName[i])
            return false;
    }

    return true;
}

inline void HTMLTokenizer::parseError()
{
}

}
