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

inline UChar legalEntityFor(unsigned value)
{
    // FIXME: There is a table with more exceptions in the HTML5 specification.
    if (value == 0 || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
        return 0xFFFD;
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
    m_emitPending = false;
    m_additionalAllowedCharacter = '\0';
}

UChar HTML5Lexer::consumeEntity(SegmentedString& source, bool& notEnoughCharacters)
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
                source.advance();
                return legalEntityFor(result);
            } else 
                return legalEntityFor(result);
            break;
        }
        case Decimal: {
            if (cc >= '0' && cc <= '9')
                result = result * 10 + cc - '0';
            else if (cc == ';') {
                source.advance();
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
                if (m_state == CharacterReferenceInAttributeValueState && (isAlphaNumeric(cc) || cc == '='))
                    break;
                if (!isAlphaNumeric(cc)) {
                    const Entity* entity = findEntity(entityName.data(), entityName.size());
                    if (entity) {
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

bool HTML5Lexer::nextToken(SegmentedString& source, HTML5Token& token)
{
    // If we have a token in progress, then we're supposed to be called back
    // with the same token so we can finish it.
    ASSERT(!m_token || m_token == &token || token.type() == HTML5Token::Uninitialized);
    m_token = &token;

    if (!m_bufferedEndTagName.isEmpty()) {
        // FIXME: This should call flushBufferedEndTag().
        // We started an end tag during our last iteration.
        m_token->beginEndTag(m_bufferedEndTagName);
        m_bufferedEndTagName.clear();
        if (m_state == DataState) {
            // We're back in the data state, so we must be done with the tag.
            return true;
        }
    }

    // Source: http://www.whatwg.org/specs/web-apps/current-work/#tokenisation0
    // FIXME: This while should stop as soon as we have a token to return.
    while (!source.isEmpty()) {
        UChar cc = *source;
        switch (m_state) {
        case DataState: {
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
        case CharacterReferenceInDataState: {
            bool notEnoughCharacters = false;
            UChar entity = consumeEntity(source, notEnoughCharacters);
            if (notEnoughCharacters)
                return haveBufferedCharacterToken();
            emitCharacter(entity ? entity : '&');
            m_state = DataState;
            continue;
        }
        case RCDATAState: {
            if (cc == '&')
                m_state = CharacterReferenceInRCDATAState;
            else if (cc == '<')
                m_state = RCDATALessThanSignState;
            else
                emitCharacter(cc);
            break;
        }
        case CharacterReferenceInRCDATAState: {
            bool notEnoughCharacters = false;
            UChar entity = consumeEntity(source, notEnoughCharacters);
            if (notEnoughCharacters)
                return haveBufferedCharacterToken();
            emitCharacter(entity ? entity : '&');
            m_state = RCDATAState;
            continue;
        }
        case RAWTEXTState: {
            if (cc == '<')
                m_state = RAWTEXTLessThanSignState;
            else
                emitCharacter(cc);
            break;
        }
        case ScriptDataState: {
            if (cc == '<')
                m_state = ScriptDataLessThanSignState;
            else
                emitCharacter(cc);
            break;
        }
        case PLAINTEXTState: {
            emitCharacter(cc);
            break;
        }
        case TagOpenState: {
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
            } else {
                emitParseError();
                m_state = DataState;
                emitCharacter('<');
                continue;
            }
            break;
        }
        case EndTagOpenState: {
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
                m_state = DataState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case TagNameState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BeforeAttributeNameState;
            else if (cc == '/')
                m_state = SelfClosingStartTagState;
            else if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
            } else if (cc >= 'A' && cc <= 'Z')
                m_token->appendToName(toLowerCase(cc));
            else
                m_token->appendToName(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        case RCDATALessThanSignState: {
            if (cc == '/') {
                m_temporaryBuffer.clear();
                ASSERT(m_bufferedEndTagName.isEmpty());
                m_state = RCDATAEndTagOpenState;
            } else {
                emitCharacter('<');
                m_state = RCDATAState;
                continue;
            }
            break;
        }
        case RCDATAEndTagOpenState: {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(toLowerCase(cc));
                m_state = RCDATAEndTagNameState;
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(cc);
                m_state = RCDATAEndTagNameState;
            } else {
                emitCharacter('<');
                emitCharacter('/');
                m_state = RCDATAState;
                continue;
            }
            break;
        }
        case RCDATAEndTagNameState: {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(toLowerCase(cc));
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(cc);
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
                        m_state = DataState;
                        maybeFlushBufferedEndTag();
                        break;
                    }
                }
                emitCharacter('<');
                emitCharacter('/');
                m_token->appendToCharacter(m_temporaryBuffer);
                m_bufferedEndTagName.clear();
                m_state = RCDATAState;
                continue;
            }
            break;
        }
        case RAWTEXTLessThanSignState: {
            if (cc == '/') {
                m_temporaryBuffer.clear();
                ASSERT(m_bufferedEndTagName.isEmpty());
                m_state = RAWTEXTEndTagOpenState;
            } else {
                emitCharacter('<');
                m_state = RAWTEXTState;
                continue;
            }
            break;
        }
        case RAWTEXTEndTagOpenState: {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(toLowerCase(cc));
                m_state = RAWTEXTEndTagNameState;
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(cc);
                m_state = RAWTEXTEndTagNameState;
            } else {
                emitCharacter('<');
                emitCharacter('/');
                m_state = RAWTEXTState;
                continue;
            }
            break;
        }
        case RAWTEXTEndTagNameState: {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(toLowerCase(cc));
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(cc);
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
                        m_state = DataState;
                        maybeFlushBufferedEndTag();
                        break;
                    }
                }
                emitCharacter('<');
                emitCharacter('/');
                m_token->appendToCharacter(m_temporaryBuffer);
                m_bufferedEndTagName.clear();
                m_state = RAWTEXTState;
                continue;
            }
            break;
        }
        case ScriptDataLessThanSignState: {
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
                m_state = ScriptDataState;
                continue;
            }
            break;
        }
        case ScriptDataEndTagOpenState: {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(toLowerCase(cc));
                m_state = ScriptDataEndTagNameState;
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(cc);
                m_state = ScriptDataEndTagNameState;
            } else {
                emitCharacter('<');
                emitCharacter('/');
                m_state = ScriptDataState;
                continue;
            }
            break;
        }
        case ScriptDataEndTagNameState: {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(toLowerCase(cc));
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(cc);
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
                        m_state = DataState;
                        maybeFlushBufferedEndTag();
                        break;
                    }
                }
                emitCharacter('<');
                emitCharacter('/');
                m_token->appendToCharacter(m_temporaryBuffer);
                m_bufferedEndTagName.clear();
                m_state = ScriptDataState;
                continue;
            }
            break;
        }
        case ScriptDataEscapeStartState: {
            if (cc == '-') {
                emitCharacter(cc);
                m_state = ScriptDataEscapeStartDashState;
            } else {
                m_state = ScriptDataState;
                continue;
            }
            break;
        }
        case ScriptDataEscapeStartDashState: {
            if (cc == '-') {
                emitCharacter(cc);
                m_state = ScriptDataEscapedDashDashState;
            } else {
                m_state = ScriptDataState;
                continue;
            }
            break;
        }
        case ScriptDataEscapedState: {
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
        case ScriptDataEscapedDashState: {
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
        case ScriptDataEscapedDashDashState: {
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
        case ScriptDataEscapedLessThanSignState: {
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
                m_state = ScriptDataEscapedState;
                continue;
            }
            break;
        }
        case ScriptDataEscapedEndTagOpenState: {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(toLowerCase(cc));
                m_state = ScriptDataEscapedEndTagNameState;
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(cc);
                m_state = ScriptDataEscapedEndTagNameState;
            } else {
                emitCharacter('<');
                emitCharacter('/');
                m_state = ScriptDataEscapedState;
                continue;
            }
            break;
        }
        case ScriptDataEscapedEndTagNameState: {
            if (cc >= 'A' && cc <= 'Z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(toLowerCase(cc));
            } else if (cc >= 'a' && cc <= 'z') {
                m_temporaryBuffer.append(cc);
                m_bufferedEndTagName.append(cc);
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
                        m_state = DataState;
                        maybeFlushBufferedEndTag();
                        break;
                    }
                }
                emitCharacter('<');
                emitCharacter('/');
                m_token->appendToCharacter(m_temporaryBuffer);
                m_bufferedEndTagName.clear();
                m_state = ScriptDataEscapedState;
                continue;
            }
            break;
        }
        case ScriptDataDoubleEscapeStartState: {
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
                m_state = ScriptDataEscapedState;
                continue;
            }
            break;
        }
        case ScriptDataDoubleEscapedState: {
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
        case ScriptDataDoubleEscapedDashState: {
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
        case ScriptDataDoubleEscapedDashDashState: {
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
        case ScriptDataDoubleEscapedLessThanSignState: {
            if (cc == '/') {
                emitCharacter(cc);
                m_temporaryBuffer.clear();
                m_state = ScriptDataDoubleEscapeEndState;
            } else {
                m_state = ScriptDataDoubleEscapedState;
                continue;
            }
            break;
        }
        case ScriptDataDoubleEscapeEndState: {
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
                m_state = ScriptDataDoubleEscapedState;
                continue;
            }
            break;
        }
        case BeforeAttributeNameState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            else if (cc == '/')
                m_state = SelfClosingStartTagState;
            else if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
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
        case AttributeNameState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = AfterAttributeNameState;
            else if (cc == '/')
                m_state = SelfClosingStartTagState;
            else if (cc == '=')
                m_state = BeforeAttributeValueState;
            else if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
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
        case AfterAttributeNameState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            else if (cc == '/')
                m_state = SelfClosingStartTagState;
            else if (cc == '=')
                m_state = BeforeAttributeValueState;
            else if (cc == '=') {
                emitCurrentToken();
                m_state = DataState;
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
        case BeforeAttributeValueState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            else if (cc == '"')
                m_state = AttributeValueDoubleQuotedState;
            else if (cc == '&') {
                m_state = AttributeValueUnquotedState;
                continue;
            } else if (cc == '\'')
                m_state = AttributeValueSingleQuotedState;
            else if (cc == '>') {
                emitParseError();
                emitCurrentToken();
                m_state = DataState;
            } else {
                if (cc == '<' || cc == '=' || cc == '`')
                    emitParseError();
                m_token->appendToAttributeValue(cc);
                m_state = AttributeValueUnquotedState;
            }
            break;
        }
        case AttributeValueDoubleQuotedState: {
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
        case AttributeValueSingleQuotedState: {
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
        case AttributeValueUnquotedState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BeforeAttributeNameState;
            else if (cc == '&') {
                m_state = CharacterReferenceInAttributeValueState;
                m_additionalAllowedCharacter = '>';
            } else if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
            } else {
                if (cc == '"' || cc == '\'' || cc == '<' || cc == '=' || cc == '`')
                    emitParseError();
                m_token->appendToAttributeValue(cc);
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case CharacterReferenceInAttributeValueState: {
            bool notEnoughCharacters = false;
            UChar entity = consumeEntity(source, notEnoughCharacters);
            if (notEnoughCharacters)
                return haveBufferedCharacterToken();
            m_token->appendToAttributeValue(entity ? entity : '&');
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
        case AfterAttributeValueQuotedState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BeforeAttributeNameState;
            else if (cc == '/')
                m_state = SelfClosingStartTagState;
            else if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
            } else {
                emitParseError();
                m_state = BeforeAttributeNameState;
                continue;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case SelfClosingStartTagState: {
            if (cc == '>') {
                notImplemented();
                emitCurrentToken();
                m_state = DataState;
            } else {
                emitParseError();
                m_state = BeforeAttributeNameState;
                continue;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case BogusCommentState: {
            notImplemented();
            m_state = DataState;
            break;
        }
        case MarkupDeclarationOpenState: {
            DEFINE_STATIC_LOCAL(String, dashDashString, ("--"));
            DEFINE_STATIC_LOCAL(String, doctypeString, ("doctype"));
            if (cc == '-') {
                SegmentedString::LookAheadResult result = source.lookAhead(dashDashString);
                if (result == SegmentedString::DidMatch) {
                    source.advanceAndASSERT('-');
                    source.advanceAndASSERT('-');
                    m_token->beginComment();
                    m_state = CommentStartState;
                    continue;
                } else if (result == SegmentedString::NotEnoughCharacters)
                    return haveBufferedCharacterToken();
            } else if (cc == 'D' || cc == 'd') {
                SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(doctypeString);
                if (result == SegmentedString::DidMatch) {
                    advanceStringAndASSERTIgnoringCase(source, "doctype");
                    m_state = DOCTYPEState;
                    continue;
                } else if (result == SegmentedString::NotEnoughCharacters)
                    return haveBufferedCharacterToken();
            }
            notImplemented();
            // FIXME: We're still missing the bits about the insertion mode being in foreign content:
            // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#markup-declaration-open-state
            emitParseError();
            m_state = BogusCommentState;
            continue;
        }
        case CommentStartState: {
            if (cc == '-')
                m_state = CommentStartDashState;
            else if (cc == '>') {
                emitParseError();
                emitCurrentToken();
                m_state = DataState;
            } else {
                m_token->appendToComment(cc);
                m_state = CommentState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case CommentStartDashState: {
            if (cc == '-')
                m_state = CommentEndState;
            else if (cc == '>') {
                emitParseError();
                emitCurrentToken();
                m_state = DataState;
            } else {
                m_token->appendToComment('-');
                m_token->appendToComment(cc);
                m_state = CommentState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case CommentState: {
            if (cc == '-')
                m_state = CommentEndDashState;
            else
                m_token->appendToComment(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        case CommentEndDashState: {
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
        case CommentEndState: {
            if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
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
        case CommentEndBangState: {
            if (cc == '-') {
                m_token->appendToComment('-');
                m_token->appendToComment('-');
                m_token->appendToComment('!');
                m_state = CommentEndDashState;
            } else if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
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
        case CommentEndSpaceState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_token->appendToComment(cc);
            else if (cc == '-')
                m_state = CommentEndDashState;
            else if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
            } else {
                m_token->appendToComment(cc);
                m_state = CommentState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case DOCTYPEState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BeforeDOCTYPENameState;
            else {
                emitParseError();
                m_state = BeforeDOCTYPENameState;
                continue;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case BeforeDOCTYPENameState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            else if (cc >= 'A' && cc <= 'Z') {
                m_token->beginDOCTYPE(toLowerCase(cc));
                m_state = DOCTYPENameState;
            } else if (cc == '>') {
                emitParseError();
                m_token->beginDOCTYPE();
                notImplemented();
                emitCurrentToken();
                m_state = DataState;
            } else {
                m_token->beginDOCTYPE(cc);
                m_state = DOCTYPENameState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case DOCTYPENameState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = AfterDOCTYPENameState;
            else if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
            } else if (cc >= 'A' && cc <= 'Z')
                m_token->appendToName(toLowerCase(cc));
            else
                m_token->appendToName(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        case AfterDOCTYPENameState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
            } else {
                DEFINE_STATIC_LOCAL(String, publicString, ("public"));
                DEFINE_STATIC_LOCAL(String, systemString, ("system"));
                if (cc == 'P' || cc == 'p') {
                    SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(publicString);
                    if (result == SegmentedString::DidMatch) {
                        advanceStringAndASSERTIgnoringCase(source, "public");
                        m_state = AfterDOCTYPEPublicKeywordState;
                        continue;
                    } else if (result == SegmentedString::NotEnoughCharacters)
                        return haveBufferedCharacterToken();
                } else if (cc == 'S' || cc == 's') {
                    SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(systemString);
                    if (result == SegmentedString::DidMatch) {
                        advanceStringAndASSERTIgnoringCase(source, "system");
                        m_state = AfterDOCTYPESystemKeywordState;
                        continue;
                    } else if (result == SegmentedString::NotEnoughCharacters)
                        return haveBufferedCharacterToken();
                }
                emitParseError();
                notImplemented();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case AfterDOCTYPEPublicKeywordState: {
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
                emitCurrentToken();
                m_state = DataState;
            } else {
                emitParseError();
                notImplemented();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case BeforeDOCTYPEPublicIdentifierState: {
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
                emitCurrentToken();
                m_state = DataState;
            } else {
                emitParseError();
                notImplemented();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case DOCTYPEPublicIdentifierDoubleQuotedState: {
            if (cc == '"')
                m_state = AfterDOCTYPEPublicIdentifierState;
            else if (cc == '>') {
                emitParseError();
                notImplemented();
                emitCurrentToken();
                m_state = DataState;
            } else
                m_token->appendToPublicIdentifier(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        case DOCTYPEPublicIdentifierSingleQuotedState: {
            if (cc == '\'')
                m_state = AfterDOCTYPEPublicIdentifierState;
            else if (cc == '>') {
                emitParseError();
                notImplemented();
                emitCurrentToken();
                m_state = DataState;
            } else
                m_token->appendToPublicIdentifier(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        case AfterDOCTYPEPublicIdentifierState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BetweenDOCTYPEPublicAndSystemIdentifiersState;
            else if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
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
        case BetweenDOCTYPEPublicAndSystemIdentifiersState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BetweenDOCTYPEPublicAndSystemIdentifiersState;
            else if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
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
        case AfterDOCTYPESystemKeywordState: {
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
                emitCurrentToken();
                m_state = DataState;
            } else {
                emitParseError();
                notImplemented();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case BeforeDOCTYPESystemIdentifierState: {
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
                emitCurrentToken();
                m_state = DataState;
            } else {
                emitParseError();
                notImplemented();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case DOCTYPESystemIdentifierDoubleQuotedState: {
            if (cc == '"')
                m_state = AfterDOCTYPESystemIdentifierState;
            else if (cc == '>') {
                emitParseError();
                notImplemented();
                emitCurrentToken();
                m_state = DataState;
            } else
                m_token->appendToSystemIdentifier(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        case DOCTYPESystemIdentifierSingleQuotedState: {
            if (cc == '\'')
                m_state = AfterDOCTYPESystemIdentifierState;
            else if (cc == '>') {
                emitParseError();
                notImplemented();
                emitCurrentToken();
                m_state = DataState;
            } else
                m_token->appendToSystemIdentifier(cc);
            // FIXME: Handle EOF properly.
            break;
        }
        case AfterDOCTYPESystemIdentifierState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                break;
            else if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
            } else {
                emitParseError();
                m_state = BogusDOCTYPEState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case BogusDOCTYPEState: {
            if (cc == '>') {
                emitCurrentToken();
                m_state = DataState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case CDATASectionState: {
            notImplemented();
            break;
        }
        }
        source.advance();
        if (m_emitPending) {
            m_emitPending = false;
            return true;
        }
    }
    // We've reached the end of the input stream.  If we have a character
    // token buffered, we should emit it.
    return haveBufferedCharacterToken();
}

inline bool HTML5Lexer::temporaryBufferIs(const String& expectedString)
{
    return vectorEqualsString(m_temporaryBuffer, expectedString);
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

inline bool HTML5Lexer::haveBufferedCharacterToken()
{
    return m_token->type() == HTML5Token::Character;
}

}
