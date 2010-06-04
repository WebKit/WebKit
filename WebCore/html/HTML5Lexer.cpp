/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "HTML5Lexer.h"

#include "AtomicString.h"
#include "HTML5Token.h"
#include "HTMLNames.h"
#include "NotImplemented.h"
#include <wtf/CurrentTime.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>
#include <wtf/unicode/Unicode.h>


// Use __GNUC__ instead of PLATFORM(GCC) to stay consistent with the gperf generated c file
#ifdef __GNUC__
// The main tokenizer includes this too so we are getting two copies of the data. However, this way the code gets inlined.
#include "HTMLEntityNames.c"
#else
// Not inlined for non-GCC compilers
struct Entity {
    const char* name;
    int code;
};
const struct Entity* findEntity(register const char* str, register unsigned int len);
#endif

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

namespace {

static const UChar windowsLatin1ExtensionArray[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, // 80-87
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F, // 88-8F
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 90-97
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178, // 98-9F
};

inline UChar toLowerCase(UChar cc)
{
    ASSERT(cc >= 'A' && cc <= 'Z');
    const int lowerCaseOffset = 0x20;
    return cc + lowerCaseOffset;
}

inline void advanceStringAndASSERTIgnoringCase(SegmentedString& source, const char* expectedCharacters)
{
    while (*expectedCharacters)
        source.advanceAndASSERTIgnoringCase(*expectedCharacters++);
}

inline bool vectorEqualsString(const Vector<UChar, 32>& vector, const String& string)
{
    if (vector.size() != string.length())
        return false;
    const UChar* stringData = string.characters();
    const UChar* vectorData = vector.data();
    // FIXME: Is there a higher-level function we should be calling here?
    return !memcmp(stringData, vectorData, vector.size() * sizeof(UChar));
}

inline UChar adjustEntity(unsigned value)
{
    if ((value & ~0x1F) != 0x0080)
        return value;
    return windowsLatin1ExtensionArray[value - 0x80];
}

inline unsigned legalEntityFor(unsigned value)
{
    // FIXME: A number of specific entity values generate parse errors.
    if (value == 0 || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
        return 0xFFFD;
    if (value < 0xFFFF)
        return adjustEntity(value);
    return value;
}

inline bool isHexDigit(UChar cc)
{
    return (cc >= '0' && cc <= '9') || (cc >= 'a' && cc <= 'f') || (cc >= 'A' && cc <= 'F');
}

inline bool isAlphaNumeric(UChar cc)
{
    return (cc >= '0' && cc <= '9') || (cc >= 'a' && cc <= 'z') || (cc >= 'A' && cc <= 'Z');
}

void unconsumeCharacters(SegmentedString& source, const Vector<UChar, 10>& consumedCharacters)
{
    if (consumedCharacters.size() == 1)
        source.push(consumedCharacters[0]);
    else if (consumedCharacters.size() == 2) {
        source.push(consumedCharacters[0]);
        source.push(consumedCharacters[1]);
    } else
        source.prepend(SegmentedString(String(consumedCharacters.data(), consumedCharacters.size())));
}

inline bool isEndTagBufferingState(HTML5Lexer::State state)
{
    return state == HTML5Lexer::RCDATAEndTagOpenState
        || state == HTML5Lexer::RCDATAEndTagNameState
        || state == HTML5Lexer::RAWTEXTEndTagOpenState
        || state == HTML5Lexer::RAWTEXTEndTagNameState
        || state == HTML5Lexer::ScriptDataEndTagOpenState
        || state == HTML5Lexer::ScriptDataEndTagNameState
        || state == HTML5Lexer::ScriptDataEscapedEndTagOpenState
        || state == HTML5Lexer::ScriptDataEscapedEndTagNameState;
}

}

HTML5Lexer::HTML5Lexer()
{
    reset();
}

HTML5Lexer::~HTML5Lexer()
{
}

void HTML5Lexer::reset()
{
    m_state = DataState;
    m_token = 0;
    m_lineNumber = 0;
    m_skipLeadingNewLineForListing = false;
    m_emitPending = false;
    m_additionalAllowedCharacter = '\0';
}

unsigned HTML5Lexer::consumeEntity(SegmentedString& source, bool& notEnoughCharacters)
{
    ASSERT(m_state != CharacterReferenceInAttributeValueState || m_additionalAllowedCharacter == '"' || m_additionalAllowedCharacter == '\'' || m_additionalAllowedCharacter == '>');
    ASSERT(!notEnoughCharacters);

    enum EntityState {
        Initial,
        NumberType,
        MaybeHexLowerCaseX,
        MaybeHexUpperCaseX,
        Hex,
        Decimal,
        Named
    };
    EntityState entityState = Initial;
    unsigned result = 0;
    Vector<UChar, 10> consumedCharacters;
    Vector<char, 10> entityName;

    while (!source.isEmpty()) {
        UChar cc = *source;
        switch (entityState) {
        case Initial: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ' || cc == '<' || cc == '&')
                return 0;
            if (m_state == CharacterReferenceInAttributeValueState && cc == m_additionalAllowedCharacter)
                return 0;
            if (cc == '#') {
                entityState = NumberType;
                break;
            }
            if ((cc >= 'a' && cc <= 'z') || (cc >= 'A' && cc <= 'Z')) {
                entityState = Named;
                continue;
            }
            return 0;
        }
        case NumberType: {
            if (cc == 'x') {
                entityState = MaybeHexLowerCaseX;
                break;
            }
            if (cc == 'X') {
                entityState = MaybeHexUpperCaseX;
                break;
            }
            if (cc >= '0' && cc <= '9') {
                entityState = Decimal;
                continue;
            }
            source.push('#');
            return 0;
        }
        case MaybeHexLowerCaseX: {
            if (isHexDigit(cc)) {
                entityState = Hex;
                continue;
            }
            source.push('#');
            source.push('x');
            return 0;
        }
        case MaybeHexUpperCaseX: {
            if (isHexDigit(cc)) {
                entityState = Hex;
                continue;
            }
            source.push('#');
            source.push('X');
            return 0;
        }
        case Hex: {
            if (cc >= '0' && cc <= '9')
                result = result * 16 + cc - '0';
            else if (cc >= 'a' && cc <= 'f')
                result = result * 16 + 10 + cc - 'a';
            else if (cc >= 'A' && cc <= 'F')
                result = result * 16 + 10 + cc - 'A';
            else if (cc == ';') {
                source.advancePastNonNewline();
                return legalEntityFor(result);
            } else 
                return legalEntityFor(result);
            break;
        }
        case Decimal: {
            if (cc >= '0' && cc <= '9')
                result = result * 10 + cc - '0';
            else if (cc == ';') {
                source.advancePastNonNewline();
                return legalEntityFor(result);
            } else
                return legalEntityFor(result);
            break;
        }
        case Named: {
            // FIXME: This code is wrong. We need to find the longest matching entity.
            //        The examples from the spec are:
            //            I'm &notit; I tell you
            //            I'm &notin; I tell you
            //        In the first case, "&not" is the entity.  In the second
            //        case, "&notin;" is the entity.
            // FIXME: Our list of HTML entities is incomplete.
            // FIXME: The number 8 below is bogus.
            while (!source.isEmpty() && entityName.size() <= 8) {
                cc = *source;
                if (cc == ';') {
                    const Entity* entity = findEntity(entityName.data(), entityName.size());
                    if (entity) {
                        source.advanceAndASSERT(';');
                        return entity->code;
                    }
                    emitParseError();
                    break;
                }
                if (!isAlphaNumeric(cc)) {
                    const Entity* entity = findEntity(entityName.data(), entityName.size());
                    if (entity) {
                        // HTML5 tells us to ignore this entity, for historical reasons,
                        // if the lookhead character is '='.
                        if (m_state == CharacterReferenceInAttributeValueState && cc == '=')
                            break;
                        emitParseError();
                        return entity->code;
                    }
                    break;
                }
                entityName.append(cc);
                consumedCharacters.append(cc);
                source.advanceAndASSERT(cc);
            }
            notEnoughCharacters = source.isEmpty();
            unconsumeCharacters(source, consumedCharacters);
            return 0;
        }
        }
        consumedCharacters.append(cc);
        source.advanceAndASSERT(cc);
    }
    ASSERT(source.isEmpty());
    notEnoughCharacters = true;
    unconsumeCharacters(source, consumedCharacters);
    return 0;
}

inline bool HTML5Lexer::processEntity(SegmentedString& source)
{
    bool notEnoughCharacters = false;
    unsigned value = consumeEntity(source, notEnoughCharacters);
    if (notEnoughCharacters)
        return false;
    if (!value)
        emitCharacter('&');
    else
        emitCodePoint(value);
    return true;
}

#define BEGIN_STATE(stateName) case stateName:
#define END_STATE() ASSERT_NOT_REACHED(); break;

#define EMIT_AND_RESUME_IN(stateName)                                       \
    do {                                                                    \
        emitCurrentToken();                                                 \
        m_state = DataState;                                                \
    } while (false)

// We'd like to use the standard do { } while (false) pattern here, but it
// doesn't play nicely with continue.
#define RECONSUME_IN(stateName)                                             \
    {                                                                       \
        m_state = stateName;                                                \
        continue;                                                           \
    }

#define FLUSH_EMIT_AND_RESUME_IN(stateName)                                 \
    {                                                                       \
        m_state = stateName;                                                \
        maybeFlushBufferedEndTag();                                         \
        break;                                                              \
    }

bool HTML5Lexer::nextToken(SegmentedString& source, HTML5Token& token)
{
    // If we have a token in progress, then we're supposed to be called back
    // with the same token so we can finish it.
    ASSERT(!m_token || m_token == &token || token.type() == HTML5Token::Uninitialized);
    m_token = &token;

    if (!m_bufferedEndTagName.isEmpty() && !isEndTagBufferingState(m_state)) {
        // FIXME: This should call flushBufferedEndTag().
        // We started an end tag during our last iteration.
        m_token->beginEndTag(m_bufferedEndTagName);
        m_bufferedEndTagName.clear();
        if (m_state == DataState) {
            // We're back in the data state, so we must be done with the tag.
            return true;
        }
    }

    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-inbody
    if (m_skipLeadingNewLineForListing && m_state == DataState && !source.isEmpty() && *source == '\x0A')
        source.advanceAndASSERT('\x0A');
    m_skipLeadingNewLineForListing = false;

    // Source: http://www.whatwg.org/specs/web-apps/current-work/#tokenisation0
    // FIXME: This while should stop as soon as we have a token to return.
    while (!source.isEmpty()) {
        UChar cc = *source;
        switch (m_state) {
        BEGIN_STATE(DataState) {
            if (cc == '&')
                m_state = CharacterReferenceInDataState;
            else if (cc == '<') {
                if (m_token->type() == HTML5Token::Character) {
                    // We have a bunch of character tokens queued up that we
                    // are emitting lazily here.
                    return true;
                }
                m_state = TagOpenState;
            } else
                emitCharacter(cc);
            break;
        }
        END_STATE()

        BEGIN_STATE(CharacterReferenceInDataState) {
            if (!processEntity(source))
                return shouldEmitBufferedCharacterToken(source);
            RECONSUME_IN(DataState);
        }
        END_STATE()

        BEGIN_STATE(RCDATAState) {
            if (cc == '&')
                m_state = CharacterReferenceInRCDATAState;
            else if (cc == '<')
                m_state = RCDATALessThanSignState;
            else
                emitCharacter(cc);
            break;
        }
        END_STATE()

        BEGIN_STATE(CharacterReferenceInRCDATAState) {
            if (!processEntity(source))
                return shouldEmitBufferedCharacterToken(source);
            RECONSUME_IN(RCDATAState);
        }
        END_STATE()

        BEGIN_STATE(RAWTEXTState) {
            if (cc == '<')
                m_state = RAWTEXTLessThanSignState;
            else
                emitCharacter(cc);
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataState) {
            if (cc == '<')
                m_state = ScriptDataLessThanSignState;
            else
                emitCharacter(cc);
            break;
        }
        END_STATE()

        BEGIN_STATE(PLAINTEXTState) {
            emitCharacter(cc);
            break;
        }
        END_STATE()

        BEGIN_STATE(TagOpenState) {
            if (cc == '!')
                m_state = MarkupDeclarationOpenState;
            else if (cc == '/')
                m_state = EndTagOpenState;
            else if (cc >= 'A' && cc <= 'Z') {
                m_token->beginStartTag(toLowerCase(cc));
                m_state = TagNameState;
            } else if (cc >= 'a' && cc <= 'z') {
                m_token->beginStartTag(cc);
                m_state = TagNameState;
            } else if (cc == '?') {
                emitParseError();
                m_state = BogusCommentState;
                // The spec consumes the current character before switching
                // to the bogus comment state, but it's easier to implement
                // if we reconsume the current character.
                continue;
            } else {
                emitParseError();
                m_state = DataState;
                emitCharacter('<');
                continue;
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(EndTagOpenState) {
            if (cc >= 'A' && cc <= 'Z') {
                m_token->beginEndTag(toLowerCase(cc));
                m_state = TagNameState;
            } else if (cc >= 'a' && cc <= 'z') {
                m_token->beginEndTag(cc);
                m_state = TagNameState;
            } else if (cc == '>') {
                emitParseError();
                m_state = DataState;
            } else {
                emitParseError();
                RECONSUME_IN(BogusCommentState);
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(TagNameState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BeforeAttributeNameState;
            else if (cc == '/')
                m_state = SelfClosingStartTagState;
            else if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else if (cc >= 'A' && cc <= 'Z')
                m_token->appendToName(toLowerCase(cc));
            else
                m_token->appendToName(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(RCDATALessThanSignState) {
            if (cc == '/') {
                m_temporaryBuffer.clear();
                ASSERT(m_bufferedEndTagName.isEmpty());
                m_state = RCDATAEndTagOpenState;
            } else {
                emitCharacter('<');
                RECONSUME_IN(RCDATAState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(RCDATAEndTagOpenState) {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(toLowerCase(cc));
                m_state = RCDATAEndTagNameState;
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(cc);
                m_state = RCDATAEndTagNameState;
            } else {
                emitCharacter('<');
                emitCharacter('/');
                RECONSUME_IN(RCDATAState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(RCDATAEndTagNameState) {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(toLowerCase(cc));
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(cc);
            } else {
                if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ') {
                    if (isAppropriateEndTag()) {
                        m_state = BeforeAttributeNameState;
                        maybeFlushBufferedEndTag();
                        break;
                    }
                } else if (cc == '/') {
                    if (isAppropriateEndTag()) {
                        m_state = SelfClosingStartTagState;
                        maybeFlushBufferedEndTag();
                        break;
                    }
                } else if (cc == '>') {
                    if (isAppropriateEndTag()) {
                        FLUSH_EMIT_AND_RESUME_IN(DataState);
                    }
                }
                emitCharacter('<');
                emitCharacter('/');
                m_token->appendToCharacter(m_temporaryBuffer);
                m_bufferedEndTagName.clear();
                RECONSUME_IN(RCDATAState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(RAWTEXTLessThanSignState) {
            if (cc == '/') {
                m_temporaryBuffer.clear();
                ASSERT(m_bufferedEndTagName.isEmpty());
                m_state = RAWTEXTEndTagOpenState;
            } else {
                emitCharacter('<');
                RECONSUME_IN(RAWTEXTState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(RAWTEXTEndTagOpenState) {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(toLowerCase(cc));
                m_state = RAWTEXTEndTagNameState;
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(cc);
                m_state = RAWTEXTEndTagNameState;
            } else {
                emitCharacter('<');
                emitCharacter('/');
                RECONSUME_IN(RAWTEXTState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(RAWTEXTEndTagNameState) {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(toLowerCase(cc));
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(cc);
            } else {
                if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ') {
                    if (isAppropriateEndTag()) {
                        m_state = BeforeAttributeNameState;
                        maybeFlushBufferedEndTag();
                        break;
                    }
                } else if (cc == '/') {
                    if (isAppropriateEndTag()) {
                        m_state = SelfClosingStartTagState;
                        maybeFlushBufferedEndTag();
                        break;
                    }
                } else if (cc == '>') {
                    if (isAppropriateEndTag()) {
                        FLUSH_EMIT_AND_RESUME_IN(DataState);
                    }
                }
                emitCharacter('<');
                emitCharacter('/');
                m_token->appendToCharacter(m_temporaryBuffer);
                m_bufferedEndTagName.clear();
                RECONSUME_IN(RAWTEXTState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataLessThanSignState) {
            if (cc == '/') {
                m_temporaryBuffer.clear();
                ASSERT(m_bufferedEndTagName.isEmpty());
                m_state = ScriptDataEndTagOpenState;
            } else if (cc == '!') {
                emitCharacter('<');
                emitCharacter('!');
                m_state = ScriptDataEscapeStartState;
            } else {
                emitCharacter('<');
                RECONSUME_IN(ScriptDataState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataEndTagOpenState) {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(toLowerCase(cc));
                m_state = ScriptDataEndTagNameState;
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(cc);
                m_state = ScriptDataEndTagNameState;
            } else {
                emitCharacter('<');
                emitCharacter('/');
                RECONSUME_IN(ScriptDataState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataEndTagNameState) {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(toLowerCase(cc));
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(cc);
            } else {
                if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ') {
                    if (isAppropriateEndTag()) {
                        m_state = BeforeAttributeNameState;
                        maybeFlushBufferedEndTag();
                        break;
                    }
                } else if (cc == '/') {
                    if (isAppropriateEndTag()) {
                        m_state = SelfClosingStartTagState;
                        maybeFlushBufferedEndTag();
                        break;
                    }
                } else if (cc == '>') {
                    if (isAppropriateEndTag()) {
                        FLUSH_EMIT_AND_RESUME_IN(DataState);
                    }
                }
                emitCharacter('<');
                emitCharacter('/');
                m_token->appendToCharacter(m_temporaryBuffer);
                m_bufferedEndTagName.clear();
                RECONSUME_IN(ScriptDataState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataEscapeStartState) {
            if (cc == '-') {
                emitCharacter(cc);
                m_state = ScriptDataEscapeStartDashState;
            } else {
                RECONSUME_IN(ScriptDataState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataEscapeStartDashState) {
            if (cc == '-') {
                emitCharacter(cc);
                m_state = ScriptDataEscapedDashDashState;
            } else {
                RECONSUME_IN(ScriptDataState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataEscapedState) {
            if (cc == '-') {
                emitCharacter(cc);
                m_state = ScriptDataEscapedDashState;
            } else if (cc == '<')
                m_state = ScriptDataEscapedLessThanSignState;
            else
                emitCharacter(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataEscapedDashState) {
            if (cc == '-') {
                emitCharacter(cc);
                m_state = ScriptDataEscapedDashDashState;
            } else if (cc == '<')
                m_state = ScriptDataEscapedLessThanSignState;
            else {
                emitCharacter(cc);
                m_state = ScriptDataEscapedState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataEscapedDashDashState) {
            if (cc == '-')
                emitCharacter(cc);
            else if (cc == '<')
                m_state = ScriptDataEscapedLessThanSignState;
            else if (cc == '>') {
                emitCharacter(cc);
                m_state = ScriptDataState;
            } else {
                emitCharacter(cc);
                m_state = ScriptDataEscapedState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataEscapedLessThanSignState) {
            if (cc == '/') {
                m_temporaryBuffer.clear();
                ASSERT(m_bufferedEndTagName.isEmpty());
                m_state = ScriptDataEscapedEndTagOpenState;
            } else if (cc >= 'A' && cc <= 'Z') {
                emitCharacter('<');
                emitCharacter(cc);
                m_temporaryBuffer.clear();
                m_temporaryBuffer.append(toLowerCase(cc));
                m_state = ScriptDataDoubleEscapeStartState;
            } else if (cc >= 'a' && cc <= 'z') {
                emitCharacter('<');
                emitCharacter(cc);
                m_temporaryBuffer.clear();
                m_temporaryBuffer.append(cc);
                m_state = ScriptDataDoubleEscapeStartState;
            } else {
                emitCharacter('<');
                RECONSUME_IN(ScriptDataEscapedState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataEscapedEndTagOpenState) {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(toLowerCase(cc));
                m_state = ScriptDataEscapedEndTagNameState;
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(cc);
                m_state = ScriptDataEscapedEndTagNameState;
            } else {
                emitCharacter('<');
                emitCharacter('/');
                RECONSUME_IN(ScriptDataEscapedState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataEscapedEndTagNameState) {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(toLowerCase(cc));
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                addToPossibleEndTag(cc);
            } else {
                if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ') {
                    if (isAppropriateEndTag()) {
                        m_state = BeforeAttributeNameState;
                        maybeFlushBufferedEndTag();
                        break;
                    }
                } else if (cc == '/') {
                    if (isAppropriateEndTag()) {
                        m_state = SelfClosingStartTagState;
                        maybeFlushBufferedEndTag();
                        break;
                    }
                } else if (cc == '>') {
                    if (isAppropriateEndTag()) {
                        FLUSH_EMIT_AND_RESUME_IN(DataState);
                    }
                }
                emitCharacter('<');
                emitCharacter('/');
                m_token->appendToCharacter(m_temporaryBuffer);
                m_bufferedEndTagName.clear();
                RECONSUME_IN(ScriptDataEscapedState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataDoubleEscapeStartState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ' || cc == '/' || cc == '>') {
                emitCharacter(cc);
                if (temporaryBufferIs(scriptTag.localName()))
                    m_state = ScriptDataDoubleEscapedState;
                else
                    m_state = ScriptDataEscapedState;
            } else if (cc >= 'A' && cc <= 'Z') {
                emitCharacter(cc);
                m_temporaryBuffer.append(toLowerCase(cc));
            } else if (cc >= 'a' && cc <= 'z') {
                emitCharacter(cc);
                m_temporaryBuffer.append(cc);
            } else {
                RECONSUME_IN(ScriptDataEscapedState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataDoubleEscapedState) {
            if (cc == '-') {
                emitCharacter(cc);
                m_state = ScriptDataDoubleEscapedDashState;
            } else if (cc == '<') {
                emitCharacter(cc);
                m_state = ScriptDataDoubleEscapedLessThanSignState;
            } else
                emitCharacter(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataDoubleEscapedDashState) {
            if (cc == '-') {
                emitCharacter(cc);
                m_state = ScriptDataDoubleEscapedDashDashState;
            } else if (cc == '<') {
                emitCharacter(cc);
                m_state = ScriptDataDoubleEscapedLessThanSignState;
            } else {
                emitCharacter(cc);
                m_state = ScriptDataDoubleEscapedState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataDoubleEscapedDashDashState) {
            if (cc == '-')
                emitCharacter(cc);
            else if (cc == '<') {
                emitCharacter(cc);
                m_state = ScriptDataDoubleEscapedLessThanSignState;
            } else if (cc == '>') {
                emitCharacter(cc);
                m_state = ScriptDataState;
            } else {
                emitCharacter(cc);
                m_state = ScriptDataDoubleEscapedState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataDoubleEscapedLessThanSignState) {
            if (cc == '/') {
                emitCharacter(cc);
                m_temporaryBuffer.clear();
                m_state = ScriptDataDoubleEscapeEndState;
            } else {
                RECONSUME_IN(ScriptDataDoubleEscapedState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(ScriptDataDoubleEscapeEndState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ' || cc == '/' || cc == '>') {
                emitCharacter(cc);
                if (temporaryBufferIs(scriptTag.localName()))
                    m_state = ScriptDataEscapedState;
                else
                    m_state = ScriptDataDoubleEscapedState;
            } else if (cc >= 'A' && cc <= 'Z') {
                emitCharacter(cc);
                m_temporaryBuffer.append(toLowerCase(cc));
            } else if (cc >= 'a' && cc <= 'z') {
                emitCharacter(cc);
                m_temporaryBuffer.append(cc);
            } else {
                RECONSUME_IN(ScriptDataDoubleEscapedState);
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(BeforeAttributeNameState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            else if (cc == '/')
                m_state = SelfClosingStartTagState;
            else if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else if (cc >= 'A' && cc <= 'Z') {
                m_token->addNewAttribute();
                m_token->appendToAttributeName(toLowerCase(cc));
                m_state = AttributeNameState;
            } else {
                if (cc == '"' || cc == '\'' || cc == '<' || cc == '=')
                    emitParseError();
                m_token->addNewAttribute();
                m_token->appendToAttributeName(cc);
                m_state = AttributeNameState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(AttributeNameState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = AfterAttributeNameState;
            else if (cc == '/')
                m_state = SelfClosingStartTagState;
            else if (cc == '=')
                m_state = BeforeAttributeValueState;
            else if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else if (cc >= 'A' && cc <= 'Z')
                m_token->appendToAttributeName(toLowerCase(cc));
            else {
                if (cc == '"' || cc == '\'' || cc == '<' || cc == '=')
                    emitParseError();
                m_token->appendToAttributeName(cc);
                m_state = AttributeNameState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(AfterAttributeNameState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            else if (cc == '/')
                m_state = SelfClosingStartTagState;
            else if (cc == '=')
                m_state = BeforeAttributeValueState;
            else if (cc == '=') {
                EMIT_AND_RESUME_IN(DataState);
            } else if (cc >= 'A' && cc <= 'Z') {
                m_token->addNewAttribute();
                m_token->appendToAttributeName(toLowerCase(cc));
                m_state = AttributeNameState;
            } else {
                if (cc == '"' || cc == '\'' || cc == '<')
                    emitParseError();
                m_token->addNewAttribute();
                m_token->appendToAttributeName(cc);
                m_state = AttributeNameState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(BeforeAttributeValueState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            else if (cc == '"')
                m_state = AttributeValueDoubleQuotedState;
            else if (cc == '&') {
                RECONSUME_IN(AttributeValueUnquotedState);
            } else if (cc == '\'')
                m_state = AttributeValueSingleQuotedState;
            else if (cc == '>') {
                emitParseError();
                EMIT_AND_RESUME_IN(DataState);
            } else {
                if (cc == '<' || cc == '=' || cc == '`')
                    emitParseError();
                m_token->appendToAttributeValue(cc);
                m_state = AttributeValueUnquotedState;
            }
            break;
        }
        END_STATE()

        BEGIN_STATE(AttributeValueDoubleQuotedState) {
            if (cc == '"')
                m_state = AfterAttributeValueQuotedState;
            else if (cc == '&') {
                m_state = CharacterReferenceInAttributeValueState;
                m_additionalAllowedCharacter = '"';
            } else
                m_token->appendToAttributeValue(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(AttributeValueSingleQuotedState) {
            if (cc == '\'')
                m_state = AfterAttributeValueQuotedState;
            else if (cc == '&') {
                m_state = CharacterReferenceInAttributeValueState;
                m_additionalAllowedCharacter = '\'';
            } else
                m_token->appendToAttributeValue(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(AttributeValueUnquotedState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BeforeAttributeNameState;
            else if (cc == '&') {
                m_state = CharacterReferenceInAttributeValueState;
                m_additionalAllowedCharacter = '>';
            } else if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else {
                if (cc == '"' || cc == '\'' || cc == '<' || cc == '=' || cc == '`')
                    emitParseError();
                m_token->appendToAttributeValue(cc);
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(CharacterReferenceInAttributeValueState) {
            bool notEnoughCharacters = false;
            unsigned value = consumeEntity(source, notEnoughCharacters);
            if (notEnoughCharacters)
                return shouldEmitBufferedCharacterToken(source);
            if (!value)
                m_token->appendToAttributeValue('&');
            else if (value < 0xFFFF)
                m_token->appendToAttributeValue(value);
            else {
                m_token->appendToAttributeValue(U16_LEAD(value));
                m_token->appendToAttributeValue(U16_TRAIL(value));
            }
            // We're supposed to switch back to the attribute value state that
            // we were in when we were switched into this state.  Rather than
            // keeping track of this explictly, we observe that the previous
            // state can be determined by m_additionalAllowedCharacter.
            if (m_additionalAllowedCharacter == '"')
                m_state = AttributeValueDoubleQuotedState;
            else if (m_additionalAllowedCharacter == '\'')
                m_state = AttributeValueSingleQuotedState;
            else if (m_additionalAllowedCharacter == '>')
                m_state = AttributeValueUnquotedState;
            else
                ASSERT_NOT_REACHED();
            continue;
        }
        END_STATE()

        BEGIN_STATE(AfterAttributeValueQuotedState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BeforeAttributeNameState;
            else if (cc == '/')
                m_state = SelfClosingStartTagState;
            else if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else {
                emitParseError();
                RECONSUME_IN(BeforeAttributeNameState);
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(SelfClosingStartTagState) {
            if (cc == '>') {
                notImplemented();
                EMIT_AND_RESUME_IN(DataState);
            } else {
                emitParseError();
                RECONSUME_IN(BeforeAttributeNameState);
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(BogusCommentState) {
            m_token->beginComment();
            while (!source.isEmpty()) {
                cc = *source;
                if (cc == '>')
                    break;
                m_token->appendToComment(cc);
                source.advance(m_lineNumber);
            }
            EMIT_AND_RESUME_IN(DataState);
            if (source.isEmpty())
                return true;
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(MarkupDeclarationOpenState) {
            DEFINE_STATIC_LOCAL(String, dashDashString, ("--"));
            DEFINE_STATIC_LOCAL(String, doctypeString, ("doctype"));
            if (cc == '-') {
                SegmentedString::LookAheadResult result = source.lookAhead(dashDashString);
                if (result == SegmentedString::DidMatch) {
                    source.advanceAndASSERT('-');
                    source.advanceAndASSERT('-');
                    m_token->beginComment();
                    RECONSUME_IN(CommentStartState);
                } else if (result == SegmentedString::NotEnoughCharacters)
                    return shouldEmitBufferedCharacterToken(source);
            } else if (cc == 'D' || cc == 'd') {
                SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(doctypeString);
                if (result == SegmentedString::DidMatch) {
                    advanceStringAndASSERTIgnoringCase(source, "doctype");
                    RECONSUME_IN(DOCTYPEState);
                } else if (result == SegmentedString::NotEnoughCharacters)
                    return shouldEmitBufferedCharacterToken(source);
            }
            notImplemented();
            // FIXME: We're still missing the bits about the insertion mode being in foreign content:
            // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#markup-declaration-open-state
            emitParseError();
            RECONSUME_IN(BogusCommentState);
        }
        END_STATE()

        BEGIN_STATE(CommentStartState) {
            if (cc == '-')
                m_state = CommentStartDashState;
            else if (cc == '>') {
                emitParseError();
                EMIT_AND_RESUME_IN(DataState);
            } else {
                m_token->appendToComment(cc);
                m_state = CommentState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(CommentStartDashState) {
            if (cc == '-')
                m_state = CommentEndState;
            else if (cc == '>') {
                emitParseError();
                EMIT_AND_RESUME_IN(DataState);
            } else {
                m_token->appendToComment('-');
                m_token->appendToComment(cc);
                m_state = CommentState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(CommentState) {
            if (cc == '-')
                m_state = CommentEndDashState;
            else
                m_token->appendToComment(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(CommentEndDashState) {
            if (cc == '-')
                m_state = CommentEndState;
            else {
                m_token->appendToComment('-');
                m_token->appendToComment(cc);
                m_state = CommentState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(CommentEndState) {
            if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ') {
                emitParseError();
                m_token->appendToComment('-');
                m_token->appendToComment('-');
                m_token->appendToComment(cc);
                m_state = CommentEndSpaceState;
            } else if (cc == '!') {
                emitParseError();
                m_state = CommentEndBangState;
            } else if (cc == '-') {
                emitParseError();
                m_token->appendToComment('-');
                m_token->appendToComment(cc);
            } else {
                emitParseError();
                m_token->appendToComment('-');
                m_token->appendToComment('-');
                m_token->appendToComment(cc);
                m_state = CommentState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(CommentEndBangState) {
            if (cc == '-') {
                m_token->appendToComment('-');
                m_token->appendToComment('-');
                m_token->appendToComment('!');
                m_state = CommentEndDashState;
            } else if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else {
                m_token->appendToComment('-');
                m_token->appendToComment('-');
                m_token->appendToComment('!');
                m_token->appendToComment(cc);
                m_state = CommentState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(CommentEndSpaceState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_token->appendToComment(cc);
            else if (cc == '-')
                m_state = CommentEndDashState;
            else if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else {
                m_token->appendToComment(cc);
                m_state = CommentState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(DOCTYPEState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BeforeDOCTYPENameState;
            else {
                emitParseError();
                RECONSUME_IN(BeforeDOCTYPENameState);
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(BeforeDOCTYPENameState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            else if (cc >= 'A' && cc <= 'Z') {
                m_token->beginDOCTYPE(toLowerCase(cc));
                m_state = DOCTYPENameState;
            } else if (cc == '>') {
                emitParseError();
                m_token->beginDOCTYPE();
                notImplemented();
                EMIT_AND_RESUME_IN(DataState);
            } else {
                m_token->beginDOCTYPE(cc);
                m_state = DOCTYPENameState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(DOCTYPENameState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = AfterDOCTYPENameState;
            else if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else if (cc >= 'A' && cc <= 'Z')
                m_token->appendToName(toLowerCase(cc));
            else
                m_token->appendToName(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(AfterDOCTYPENameState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else {
                DEFINE_STATIC_LOCAL(String, publicString, ("public"));
                DEFINE_STATIC_LOCAL(String, systemString, ("system"));
                if (cc == 'P' || cc == 'p') {
                    SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(publicString);
                    if (result == SegmentedString::DidMatch) {
                        advanceStringAndASSERTIgnoringCase(source, "public");
                        RECONSUME_IN(AfterDOCTYPEPublicKeywordState);
                    } else if (result == SegmentedString::NotEnoughCharacters)
                        return shouldEmitBufferedCharacterToken(source);
                } else if (cc == 'S' || cc == 's') {
                    SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(systemString);
                    if (result == SegmentedString::DidMatch) {
                        advanceStringAndASSERTIgnoringCase(source, "system");
                        RECONSUME_IN(AfterDOCTYPESystemKeywordState);
                    } else if (result == SegmentedString::NotEnoughCharacters)
                        return shouldEmitBufferedCharacterToken(source);
                }
                emitParseError();
                notImplemented();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(AfterDOCTYPEPublicKeywordState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BeforeDOCTYPEPublicIdentifierState;
            else if (cc == '"') {
                emitParseError();
                m_token->setPublicIdentifierToEmptyString();
                m_state = DOCTYPEPublicIdentifierDoubleQuotedState;
            } else if (cc == '\'') {
                emitParseError();
                m_token->setPublicIdentifierToEmptyString();
                m_state = DOCTYPEPublicIdentifierSingleQuotedState;
            } else if (cc == '>') {
                emitParseError();
                notImplemented();
                EMIT_AND_RESUME_IN(DataState);
            } else {
                emitParseError();
                notImplemented();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(BeforeDOCTYPEPublicIdentifierState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            else if (cc == '"') {
                m_token->setPublicIdentifierToEmptyString();
                m_state = DOCTYPEPublicIdentifierDoubleQuotedState;
            } else if (cc == '\'') {
                m_token->setPublicIdentifierToEmptyString();
                m_state = DOCTYPEPublicIdentifierSingleQuotedState;
            } else if (cc == '>') {
                emitParseError();
                notImplemented();
                EMIT_AND_RESUME_IN(DataState);
            } else {
                emitParseError();
                notImplemented();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(DOCTYPEPublicIdentifierDoubleQuotedState) {
            if (cc == '"')
                m_state = AfterDOCTYPEPublicIdentifierState;
            else if (cc == '>') {
                emitParseError();
                notImplemented();
                EMIT_AND_RESUME_IN(DataState);
            } else
                m_token->appendToPublicIdentifier(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(DOCTYPEPublicIdentifierSingleQuotedState) {
            if (cc == '\'')
                m_state = AfterDOCTYPEPublicIdentifierState;
            else if (cc == '>') {
                emitParseError();
                notImplemented();
                EMIT_AND_RESUME_IN(DataState);
            } else
                m_token->appendToPublicIdentifier(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(AfterDOCTYPEPublicIdentifierState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BetweenDOCTYPEPublicAndSystemIdentifiersState;
            else if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else if (cc == '"') {
                emitParseError();
                m_token->setPublicIdentifierToEmptyString();
                m_state = DOCTYPESystemIdentifierDoubleQuotedState;
            } else if (cc == '\'') {
                emitParseError();
                m_token->setPublicIdentifierToEmptyString();
                m_state = DOCTYPESystemIdentifierSingleQuotedState;
            } else {
                emitParseError();
                notImplemented();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(BetweenDOCTYPEPublicAndSystemIdentifiersState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BetweenDOCTYPEPublicAndSystemIdentifiersState;
            else if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else if (cc == '"') {
                m_token->setSystemIdentifierToEmptyString();
                m_state = DOCTYPESystemIdentifierDoubleQuotedState;
            } else if (cc == '\'') {
                m_token->setSystemIdentifierToEmptyString();
                m_state = DOCTYPESystemIdentifierSingleQuotedState;
            } else {
                emitParseError();
                notImplemented();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(AfterDOCTYPESystemKeywordState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BeforeDOCTYPESystemIdentifierState;
            else if (cc == '"') {
                emitParseError();
                m_token->setSystemIdentifierToEmptyString();
                m_state = DOCTYPESystemIdentifierDoubleQuotedState;
            } else if (cc == '\'') {
                emitParseError();
                m_token->setSystemIdentifierToEmptyString();
                m_state = DOCTYPESystemIdentifierSingleQuotedState;
            } else if (cc == '>') {
                emitParseError();
                notImplemented();
                EMIT_AND_RESUME_IN(DataState);
            } else {
                emitParseError();
                notImplemented();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(BeforeDOCTYPESystemIdentifierState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            if (cc == '"') {
                m_token->setSystemIdentifierToEmptyString();
                m_state = DOCTYPESystemIdentifierDoubleQuotedState;
            } else if (cc == '\'') {
                m_token->setSystemIdentifierToEmptyString();
                m_state = DOCTYPESystemIdentifierSingleQuotedState;
            } else if (cc == '>') {
                emitParseError();
                notImplemented();
                EMIT_AND_RESUME_IN(DataState);
            } else {
                emitParseError();
                notImplemented();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(DOCTYPESystemIdentifierDoubleQuotedState) {
            if (cc == '"')
                m_state = AfterDOCTYPESystemIdentifierState;
            else if (cc == '>') {
                emitParseError();
                notImplemented();
                EMIT_AND_RESUME_IN(DataState);
            } else
                m_token->appendToSystemIdentifier(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(DOCTYPESystemIdentifierSingleQuotedState) {
            if (cc == '\'')
                m_state = AfterDOCTYPESystemIdentifierState;
            else if (cc == '>') {
                emitParseError();
                notImplemented();
                EMIT_AND_RESUME_IN(DataState);
            } else
                m_token->appendToSystemIdentifier(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(AfterDOCTYPESystemIdentifierState) {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            else if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            } else {
                emitParseError();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(BogusDOCTYPEState) {
            if (cc == '>') {
                EMIT_AND_RESUME_IN(DataState);
            }
            // FIXME: Handle EOF properly.
            break;
        }
        END_STATE()

        BEGIN_STATE(CDATASectionState) {
            notImplemented();
            break;
        }
        END_STATE()

        }
        source.advance(m_lineNumber);
        if (m_emitPending) {
            m_emitPending = false;
            return true;
        }
    }
    // We've reached the end of the input stream.  If we have a character
    // token buffered, we should emit it.
    return shouldEmitBufferedCharacterToken(source);
}

inline bool HTML5Lexer::temporaryBufferIs(const String& expectedString)
{
    return vectorEqualsString(m_temporaryBuffer, expectedString);
}

inline void HTML5Lexer::addToPossibleEndTag(UChar cc)
{
    ASSERT(isEndTagBufferingState(m_state));
    m_bufferedEndTagName.append(cc);
}

inline bool HTML5Lexer::isAppropriateEndTag()
{
    return vectorEqualsString(m_bufferedEndTagName, m_appropriateEndTagName);
}

inline void HTML5Lexer::emitCharacter(UChar character)
{
    if (m_token->type() != HTML5Token::Character) {
        m_token->beginCharacter(character);
        return;
    }
    m_token->appendToCharacter(character);
}

inline void HTML5Lexer::emitCodePoint(unsigned value)
{
    if (value < 0xFFFF) {
        emitCharacter(value);
        return;
    }
    emitCharacter(U16_LEAD(value));
    emitCharacter(U16_TRAIL(value));
}

inline void HTML5Lexer::emitParseError()
{
    notImplemented();
}

inline void HTML5Lexer::maybeFlushBufferedEndTag()
{
    ASSERT(m_token->type() == HTML5Token::Character || m_token->type() == HTML5Token::Uninitialized);
    if (m_token->type() == HTML5Token::Character) {
        // We have a character token queued up.  We need to emit it before we
        // can start begin the buffered end tag token.
        emitCurrentToken();
        return;
    }
    flushBufferedEndTag();
}

inline void HTML5Lexer::flushBufferedEndTag()
{
    m_token->beginEndTag(m_bufferedEndTagName);
    m_bufferedEndTagName.clear();
    if (m_state == DataState)
        emitCurrentToken();
}

inline void HTML5Lexer::emitCurrentToken()
{
    ASSERT(m_token->type() != HTML5Token::Uninitialized);
    m_emitPending = true;
    if (m_token->type() == HTML5Token::StartTag)
        m_appropriateEndTagName = m_token->name();
}

inline bool HTML5Lexer::shouldEmitBufferedCharacterToken(const SegmentedString& source)
{
    return source.isClosed() && m_token->type() == HTML5Token::Character;
}

}

