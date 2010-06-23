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
#include "HTMLTokenizer.h"

#include "AtomicString.h"
#include "HTMLEntityParser.h"
#include "HTMLToken.h"
#include "HTMLNames.h"
#include "NotImplemented.h"
#include <wtf/CurrentTime.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>
#include <wtf/unicode/Unicode.h>

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

inline bool isEndTagBufferingState(HTMLTokenizer::State state)
{
    switch (state) {
    case HTMLTokenizer::RCDATAEndTagOpenState:
    case HTMLTokenizer::RCDATAEndTagNameState:
    case HTMLTokenizer::RAWTEXTEndTagOpenState:
    case HTMLTokenizer::RAWTEXTEndTagNameState:
    case HTMLTokenizer::ScriptDataEndTagOpenState:
    case HTMLTokenizer::ScriptDataEndTagNameState:
    case HTMLTokenizer::ScriptDataEscapedEndTagOpenState:
    case HTMLTokenizer::ScriptDataEscapedEndTagNameState:
        return true;
    default:
        return false;
    }
}

}

HTMLTokenizer::HTMLTokenizer()
{
    reset();
}

HTMLTokenizer::~HTMLTokenizer()
{
}

void HTMLTokenizer::reset()
{
    m_state = DataState;
    m_token = 0;
    m_lineNumber = 0;
    m_skipLeadingNewLineForListing = false;
    m_additionalAllowedCharacter = '\0';
}

inline bool HTMLTokenizer::processEntity(SegmentedString& source)
{
    bool notEnoughCharacters = false;
    unsigned value = consumeHTMLEntity(source, notEnoughCharacters);
    if (notEnoughCharacters)
        return false;
    if (!value)
        emitCharacter('&');
    else
        emitCodePoint(value);
    return true;
}

#if COMPILER(MSVC)
// We need to disable the "unreachable code" warning because we want to assert
// that some code points aren't reached in the state machine.
#pragma warning(disable: 4702)
#endif

#define BEGIN_STATE(stateName) case stateName: stateName:
#define END_STATE() ASSERT_NOT_REACHED(); break;

// We use this macro when the HTML5 spec says "reconsume the current input
// character in the <mumble> state."
#define RECONSUME_IN(stateName)                                            \
    do {                                                                   \
        m_state = stateName;                                               \
        goto stateName;                                                    \
    } while (false)

// We use this macro when the HTML5 spec says "consume the next input
// character ... and switch to the <mumble> state."
#define ADVANCE_TO(stateName)                                              \
    do {                                                                   \
        m_state = stateName;                                               \
        if (!m_inputStreamPreprocessor.advance(source, m_lineNumber))      \
            return shouldEmitBufferedCharacterToken(source);               \
        cc = m_inputStreamPreprocessor.nextInputCharacter();               \
        goto stateName;                                                    \
    } while (false)

// Sometimes there's more complicated logic in the spec that separates when
// we consume the next input character and when we switch to a particular
// state.  We handle those cases by advancing the source directly and using
// this macro to switch to the indicated state.
#define SWITCH_TO(stateName)                                               \
    do {                                                                   \
        m_state = stateName;                                               \
        if (source.isEmpty() || !m_inputStreamPreprocessor.peek(source, m_lineNumber)) \
            return shouldEmitBufferedCharacterToken(source);               \
        cc = m_inputStreamPreprocessor.nextInputCharacter();               \
        goto stateName;                                                    \
    } while (false)

// We use this macro when the HTML5 spec says "Emit the current <mumble>
// token. Switch to the <mumble> state."  We use the word "resume" instead of
// switch to indicate that this macro actually returns and that we'll end up
// in the state when we "resume" (i.e., are called again).
#define EMIT_AND_RESUME_IN(stateName)                                      \
    do {                                                                   \
        m_state = stateName;                                               \
        source.advance(m_lineNumber);                                      \
        emitCurrentToken();                                                \
        return true;                                                       \
    } while (false)

#define _FLUSH_BUFFERED_END_TAG()                                          \
    do {                                                                   \
        ASSERT(m_token->type() == HTMLToken::Character ||                  \
               m_token->type() == HTMLToken::Uninitialized);               \
        source.advance(m_lineNumber);                                      \
        if (m_token->type() == HTMLToken::Character)                       \
            return true;                                                   \
        m_token->beginEndTag(m_bufferedEndTagName);                        \
        m_bufferedEndTagName.clear();                                      \
    } while (false)

#define FLUSH_AND_ADVANCE_TO(stateName)                                    \
    do {                                                                   \
        m_state = stateName;                                               \
        _FLUSH_BUFFERED_END_TAG();                                         \
        if (source.isEmpty()                                               \
            || !m_inputStreamPreprocessor.peek(source, m_lineNumber))      \
            return shouldEmitBufferedCharacterToken(source);               \
        cc = m_inputStreamPreprocessor.nextInputCharacter();               \
        goto stateName;                                                    \
    } while (false)

#define FLUSH_EMIT_AND_RESUME_IN(stateName)                                \
    do {                                                                   \
        m_state = stateName;                                               \
        _FLUSH_BUFFERED_END_TAG();                                         \
        return true;                                                       \
    } while (false)

bool HTMLTokenizer::nextToken(SegmentedString& source, HTMLToken& token)
{
    // If we have a token in progress, then we're supposed to be called back
    // with the same token so we can finish it.
    ASSERT(!m_token || m_token == &token || token.type() == HTMLToken::Uninitialized);
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

    if (source.isEmpty() || !m_inputStreamPreprocessor.peek(source, m_lineNumber))
        return shouldEmitBufferedCharacterToken(source);
    UChar cc = m_inputStreamPreprocessor.nextInputCharacter();

    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-inbody
    // Note that this logic is different than the generic \r\n collapsing
    // handled in the input stream preprocessor.  This logic is here as an
    // "authoring convenience" so folks can write:
    //
    // <pre>
    // lorem ipsum
    // lorem ipsum
    // </pre>
    //
    // without getting an extra newline at the start of their <pre> element.
    if (m_skipLeadingNewLineForListing) {
        m_skipLeadingNewLineForListing = false;
        if (m_state == DataState && cc == '\n')
            ADVANCE_TO(DataState);
    }

    // Source: http://www.whatwg.org/specs/web-apps/current-work/#tokenisation0
    switch (m_state) {
    BEGIN_STATE(DataState) {
        if (cc == '&')
            ADVANCE_TO(CharacterReferenceInDataState);
        else if (cc == '<') {
            if (m_token->type() == HTMLToken::Character) {
                // We have a bunch of character tokens queued up that we
                // are emitting lazily here.
                return true;
            }
            ADVANCE_TO(TagOpenState);
        } else {
            emitCharacter(cc);
            ADVANCE_TO(DataState);
        }
    }
    END_STATE()

    BEGIN_STATE(CharacterReferenceInDataState) {
        if (!processEntity(source))
            return shouldEmitBufferedCharacterToken(source);
        SWITCH_TO(DataState);
    }
    END_STATE()

    BEGIN_STATE(RCDATAState) {
        if (cc == '&')
            ADVANCE_TO(CharacterReferenceInRCDATAState);
        else if (cc == '<')
            ADVANCE_TO(RCDATALessThanSignState);
        else {
            emitCharacter(cc);
            ADVANCE_TO(RCDATAState);
        }
    }
    END_STATE()

    BEGIN_STATE(CharacterReferenceInRCDATAState) {
        if (!processEntity(source))
            return shouldEmitBufferedCharacterToken(source);
        SWITCH_TO(RCDATAState);
    }
    END_STATE()

    BEGIN_STATE(RAWTEXTState) {
        if (cc == '<')
            ADVANCE_TO(RAWTEXTLessThanSignState);
        else {
            emitCharacter(cc);
            ADVANCE_TO(RAWTEXTState);
        }
    }
    END_STATE()

    BEGIN_STATE(ScriptDataState) {
        if (cc == '<')
            ADVANCE_TO(ScriptDataLessThanSignState);
        else {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataState);
        }
    }
    END_STATE()

    BEGIN_STATE(PLAINTEXTState) {
        emitCharacter(cc);
        ADVANCE_TO(PLAINTEXTState);
    }
    END_STATE()

    BEGIN_STATE(TagOpenState) {
        if (cc == '!')
            ADVANCE_TO(MarkupDeclarationOpenState);
        else if (cc == '/')
            ADVANCE_TO(EndTagOpenState);
        else if (cc >= 'A' && cc <= 'Z') {
            m_token->beginStartTag(toLowerCase(cc));
            ADVANCE_TO(TagNameState);
        } else if (cc >= 'a' && cc <= 'z') {
            m_token->beginStartTag(cc);
            ADVANCE_TO(TagNameState);
        } else if (cc == '?') {
            emitParseError();
            // The spec consumes the current character before switching
            // to the bogus comment state, but it's easier to implement
            // if we reconsume the current character.
            RECONSUME_IN(BogusCommentState);
        } else {
            emitParseError();
            emitCharacter('<');
            RECONSUME_IN(DataState);
        }
    }
    END_STATE()

    BEGIN_STATE(EndTagOpenState) {
        if (cc >= 'A' && cc <= 'Z') {
            m_token->beginEndTag(toLowerCase(cc));
            ADVANCE_TO(TagNameState);
        } else if (cc >= 'a' && cc <= 'z') {
            m_token->beginEndTag(cc);
            ADVANCE_TO(TagNameState);
        } else if (cc == '>') {
            emitParseError();
            ADVANCE_TO(DataState);
        } else {
            emitParseError();
            RECONSUME_IN(BogusCommentState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(TagNameState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BeforeAttributeNameState);
        else if (cc == '/')
            ADVANCE_TO(SelfClosingStartTagState);
        else if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else if (cc >= 'A' && cc <= 'Z') {
            m_token->appendToName(toLowerCase(cc));
            ADVANCE_TO(TagNameState);
        } else {
            m_token->appendToName(cc);
            ADVANCE_TO(TagNameState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(RCDATALessThanSignState) {
        if (cc == '/') {
            m_temporaryBuffer.clear();
            ASSERT(m_bufferedEndTagName.isEmpty());
            ADVANCE_TO(RCDATAEndTagOpenState);
        } else {
            emitCharacter('<');
            RECONSUME_IN(RCDATAState);
        }
    }
    END_STATE()

    BEGIN_STATE(RCDATAEndTagOpenState) {
        if (cc >= 'A' && cc <= 'Z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(toLowerCase(cc));
            ADVANCE_TO(RCDATAEndTagNameState);
        } else if (cc >= 'a' && cc <= 'z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(cc);
            ADVANCE_TO(RCDATAEndTagNameState);
        } else {
            emitCharacter('<');
            emitCharacter('/');
            RECONSUME_IN(RCDATAState);
        }
    }
    END_STATE()

    BEGIN_STATE(RCDATAEndTagNameState) {
        if (cc >= 'A' && cc <= 'Z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(toLowerCase(cc));
            ADVANCE_TO(RCDATAEndTagNameState);
        } else if (cc >= 'a' && cc <= 'z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(cc);
            ADVANCE_TO(RCDATAEndTagNameState);
        } else {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ') {
                if (isAppropriateEndTag())
                    FLUSH_AND_ADVANCE_TO(BeforeAttributeNameState);
            } else if (cc == '/') {
                if (isAppropriateEndTag())
                    FLUSH_AND_ADVANCE_TO(SelfClosingStartTagState);
            } else if (cc == '>') {
                if (isAppropriateEndTag())
                    FLUSH_EMIT_AND_RESUME_IN(DataState);
            }
            emitCharacter('<');
            emitCharacter('/');
            m_token->appendToCharacter(m_temporaryBuffer);
            m_bufferedEndTagName.clear();
            RECONSUME_IN(RCDATAState);
        }
    }
    END_STATE()

    BEGIN_STATE(RAWTEXTLessThanSignState) {
        if (cc == '/') {
            m_temporaryBuffer.clear();
            ASSERT(m_bufferedEndTagName.isEmpty());
            ADVANCE_TO(RAWTEXTEndTagOpenState);
        } else {
            emitCharacter('<');
            RECONSUME_IN(RAWTEXTState);
        }
    }
    END_STATE()

    BEGIN_STATE(RAWTEXTEndTagOpenState) {
        if (cc >= 'A' && cc <= 'Z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(toLowerCase(cc));
            ADVANCE_TO(RAWTEXTEndTagNameState);
        } else if (cc >= 'a' && cc <= 'z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(cc);
            ADVANCE_TO(RAWTEXTEndTagNameState);
        } else {
            emitCharacter('<');
            emitCharacter('/');
            RECONSUME_IN(RAWTEXTState);
        }
    }
    END_STATE()

    BEGIN_STATE(RAWTEXTEndTagNameState) {
        if (cc >= 'A' && cc <= 'Z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(toLowerCase(cc));
            ADVANCE_TO(RAWTEXTEndTagNameState);
        } else if (cc >= 'a' && cc <= 'z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(cc);
            ADVANCE_TO(RAWTEXTEndTagNameState);
        } else {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ') {
                if (isAppropriateEndTag())
                    FLUSH_AND_ADVANCE_TO(BeforeAttributeNameState);
            } else if (cc == '/') {
                if (isAppropriateEndTag())
                    FLUSH_AND_ADVANCE_TO(SelfClosingStartTagState);
            } else if (cc == '>') {
                if (isAppropriateEndTag())
                    FLUSH_EMIT_AND_RESUME_IN(DataState);
            }
            emitCharacter('<');
            emitCharacter('/');
            m_token->appendToCharacter(m_temporaryBuffer);
            m_bufferedEndTagName.clear();
            RECONSUME_IN(RAWTEXTState);
        }
    }
    END_STATE()

    BEGIN_STATE(ScriptDataLessThanSignState) {
        if (cc == '/') {
            m_temporaryBuffer.clear();
            ASSERT(m_bufferedEndTagName.isEmpty());
            ADVANCE_TO(ScriptDataEndTagOpenState);
        } else if (cc == '!') {
            emitCharacter('<');
            emitCharacter('!');
            ADVANCE_TO(ScriptDataEscapeStartState);
        } else {
            emitCharacter('<');
            RECONSUME_IN(ScriptDataState);
        }
    }
    END_STATE()

    BEGIN_STATE(ScriptDataEndTagOpenState) {
        if (cc >= 'A' && cc <= 'Z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(toLowerCase(cc));
            ADVANCE_TO(ScriptDataEndTagNameState);
        } else if (cc >= 'a' && cc <= 'z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(cc);
            ADVANCE_TO(ScriptDataEndTagNameState);
        } else {
            emitCharacter('<');
            emitCharacter('/');
            RECONSUME_IN(ScriptDataState);
        }
    }
    END_STATE()

    BEGIN_STATE(ScriptDataEndTagNameState) {
        if (cc >= 'A' && cc <= 'Z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(toLowerCase(cc));
            ADVANCE_TO(ScriptDataEndTagNameState);
        } else if (cc >= 'a' && cc <= 'z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(cc);
            ADVANCE_TO(ScriptDataEndTagNameState);
        } else {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ') {
                if (isAppropriateEndTag())
                    FLUSH_AND_ADVANCE_TO(BeforeAttributeNameState);
            } else if (cc == '/') {
                if (isAppropriateEndTag())
                    FLUSH_AND_ADVANCE_TO(SelfClosingStartTagState);
            } else if (cc == '>') {
                if (isAppropriateEndTag())
                    FLUSH_EMIT_AND_RESUME_IN(DataState);
            }
            emitCharacter('<');
            emitCharacter('/');
            m_token->appendToCharacter(m_temporaryBuffer);
            m_bufferedEndTagName.clear();
            RECONSUME_IN(ScriptDataState);
        }
    }
    END_STATE()

    BEGIN_STATE(ScriptDataEscapeStartState) {
        if (cc == '-') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataEscapeStartDashState);
        } else
            RECONSUME_IN(ScriptDataState);
    }
    END_STATE()

    BEGIN_STATE(ScriptDataEscapeStartDashState) {
        if (cc == '-') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataEscapedDashDashState);
        } else
            RECONSUME_IN(ScriptDataState);
    }
    END_STATE()

    BEGIN_STATE(ScriptDataEscapedState) {
        if (cc == '-') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataEscapedDashState);
        } else if (cc == '<')
            ADVANCE_TO(ScriptDataEscapedLessThanSignState);
        else {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataEscapedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(ScriptDataEscapedDashState) {
        if (cc == '-') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataEscapedDashDashState);
        } else if (cc == '<')
            ADVANCE_TO(ScriptDataEscapedLessThanSignState);
        else {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataEscapedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(ScriptDataEscapedDashDashState) {
        if (cc == '-') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataEscapedDashDashState);
        } else if (cc == '<')
            ADVANCE_TO(ScriptDataEscapedLessThanSignState);
        else if (cc == '>') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataState);
        } else {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataEscapedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(ScriptDataEscapedLessThanSignState) {
        if (cc == '/') {
            m_temporaryBuffer.clear();
            ASSERT(m_bufferedEndTagName.isEmpty());
            ADVANCE_TO(ScriptDataEscapedEndTagOpenState);
        } else if (cc >= 'A' && cc <= 'Z') {
            emitCharacter('<');
            emitCharacter(cc);
            m_temporaryBuffer.clear();
            m_temporaryBuffer.append(toLowerCase(cc));
            ADVANCE_TO(ScriptDataDoubleEscapeStartState);
        } else if (cc >= 'a' && cc <= 'z') {
            emitCharacter('<');
            emitCharacter(cc);
            m_temporaryBuffer.clear();
            m_temporaryBuffer.append(cc);
            ADVANCE_TO(ScriptDataDoubleEscapeStartState);
        } else {
            emitCharacter('<');
            RECONSUME_IN(ScriptDataEscapedState);
        }
    }
    END_STATE()

    BEGIN_STATE(ScriptDataEscapedEndTagOpenState) {
        if (cc >= 'A' && cc <= 'Z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(toLowerCase(cc));
            ADVANCE_TO(ScriptDataEscapedEndTagNameState);
        } else if (cc >= 'a' && cc <= 'z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(cc);
            ADVANCE_TO(ScriptDataEscapedEndTagNameState);
        } else {
            emitCharacter('<');
            emitCharacter('/');
            RECONSUME_IN(ScriptDataEscapedState);
        }
    }
    END_STATE()

    BEGIN_STATE(ScriptDataEscapedEndTagNameState) {
        if (cc >= 'A' && cc <= 'Z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(toLowerCase(cc));
            ADVANCE_TO(ScriptDataEscapedEndTagNameState);
        } else if (cc >= 'a' && cc <= 'z') {
            m_temporaryBuffer.append(cc);
            addToPossibleEndTag(cc);
            ADVANCE_TO(ScriptDataEscapedEndTagNameState);
        } else {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ') {
                if (isAppropriateEndTag())
                    FLUSH_AND_ADVANCE_TO(BeforeAttributeNameState);
            } else if (cc == '/') {
                if (isAppropriateEndTag())
                    FLUSH_AND_ADVANCE_TO(SelfClosingStartTagState);
            } else if (cc == '>') {
                if (isAppropriateEndTag())
                    FLUSH_EMIT_AND_RESUME_IN(DataState);
            }
            emitCharacter('<');
            emitCharacter('/');
            m_token->appendToCharacter(m_temporaryBuffer);
            m_bufferedEndTagName.clear();
            RECONSUME_IN(ScriptDataEscapedState);
        }
    }
    END_STATE()

    BEGIN_STATE(ScriptDataDoubleEscapeStartState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ' || cc == '/' || cc == '>') {
            emitCharacter(cc);
            if (temporaryBufferIs(scriptTag.localName()))
                ADVANCE_TO(ScriptDataDoubleEscapedState);
            else
                ADVANCE_TO(ScriptDataEscapedState);
        } else if (cc >= 'A' && cc <= 'Z') {
            emitCharacter(cc);
            m_temporaryBuffer.append(toLowerCase(cc));
            ADVANCE_TO(ScriptDataDoubleEscapeStartState);
        } else if (cc >= 'a' && cc <= 'z') {
            emitCharacter(cc);
            m_temporaryBuffer.append(cc);
            ADVANCE_TO(ScriptDataDoubleEscapeStartState);
        } else
            RECONSUME_IN(ScriptDataEscapedState);
    }
    END_STATE()

    BEGIN_STATE(ScriptDataDoubleEscapedState) {
        if (cc == '-') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataDoubleEscapedDashState);
        } else if (cc == '<') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataDoubleEscapedLessThanSignState);
        } else {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataDoubleEscapedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(ScriptDataDoubleEscapedDashState) {
        if (cc == '-') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataDoubleEscapedDashDashState);
        } else if (cc == '<') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataDoubleEscapedLessThanSignState);
        } else {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataDoubleEscapedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(ScriptDataDoubleEscapedDashDashState) {
        if (cc == '-') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataDoubleEscapedDashDashState);
        } else if (cc == '<') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataDoubleEscapedLessThanSignState);
        } else if (cc == '>') {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataState);
        } else {
            emitCharacter(cc);
            ADVANCE_TO(ScriptDataDoubleEscapedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(ScriptDataDoubleEscapedLessThanSignState) {
        if (cc == '/') {
            emitCharacter(cc);
            m_temporaryBuffer.clear();
            ADVANCE_TO(ScriptDataDoubleEscapeEndState);
        } else
            RECONSUME_IN(ScriptDataDoubleEscapedState);
    }
    END_STATE()

    BEGIN_STATE(ScriptDataDoubleEscapeEndState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ' || cc == '/' || cc == '>') {
            emitCharacter(cc);
            if (temporaryBufferIs(scriptTag.localName()))
                ADVANCE_TO(ScriptDataEscapedState);
            else
                ADVANCE_TO(ScriptDataDoubleEscapedState);
        } else if (cc >= 'A' && cc <= 'Z') {
            emitCharacter(cc);
            m_temporaryBuffer.append(toLowerCase(cc));
            ADVANCE_TO(ScriptDataDoubleEscapeEndState);
        } else if (cc >= 'a' && cc <= 'z') {
            emitCharacter(cc);
            m_temporaryBuffer.append(cc);
            ADVANCE_TO(ScriptDataDoubleEscapeEndState);
        } else
            RECONSUME_IN(ScriptDataDoubleEscapedState);
    }
    END_STATE()

    BEGIN_STATE(BeforeAttributeNameState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BeforeAttributeNameState);
        else if (cc == '/')
            ADVANCE_TO(SelfClosingStartTagState);
        else if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else if (cc >= 'A' && cc <= 'Z') {
            m_token->addNewAttribute();
            m_token->appendToAttributeName(toLowerCase(cc));
            ADVANCE_TO(AttributeNameState);
        } else {
            if (cc == '"' || cc == '\'' || cc == '<' || cc == '=')
                emitParseError();
            m_token->addNewAttribute();
            m_token->appendToAttributeName(cc);
            ADVANCE_TO(AttributeNameState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(AttributeNameState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(AfterAttributeNameState);
        else if (cc == '/')
            ADVANCE_TO(SelfClosingStartTagState);
        else if (cc == '=')
            ADVANCE_TO(BeforeAttributeValueState);
        else if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else if (cc >= 'A' && cc <= 'Z') {
            m_token->appendToAttributeName(toLowerCase(cc));
            ADVANCE_TO(AttributeNameState);
        } else {
            if (cc == '"' || cc == '\'' || cc == '<' || cc == '=')
                emitParseError();
            m_token->appendToAttributeName(cc);
            ADVANCE_TO(AttributeNameState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(AfterAttributeNameState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(AfterAttributeNameState);
        else if (cc == '/')
            ADVANCE_TO(SelfClosingStartTagState);
        else if (cc == '=')
            ADVANCE_TO(BeforeAttributeValueState);
        else if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else if (cc >= 'A' && cc <= 'Z') {
            m_token->addNewAttribute();
            m_token->appendToAttributeName(toLowerCase(cc));
            ADVANCE_TO(AttributeNameState);
        } else {
            if (cc == '"' || cc == '\'' || cc == '<')
                emitParseError();
            m_token->addNewAttribute();
            m_token->appendToAttributeName(cc);
            ADVANCE_TO(AttributeNameState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(BeforeAttributeValueState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BeforeAttributeValueState);
        else if (cc == '"')
            ADVANCE_TO(AttributeValueDoubleQuotedState);
        else if (cc == '&')
            RECONSUME_IN(AttributeValueUnquotedState);
        else if (cc == '\'')
            ADVANCE_TO(AttributeValueSingleQuotedState);
        else if (cc == '>') {
            emitParseError();
            EMIT_AND_RESUME_IN(DataState);
        } else {
            if (cc == '<' || cc == '=' || cc == '`')
                emitParseError();
            m_token->appendToAttributeValue(cc);
            ADVANCE_TO(AttributeValueUnquotedState);
        }
    }
    END_STATE()

    BEGIN_STATE(AttributeValueDoubleQuotedState) {
        if (cc == '"')
            ADVANCE_TO(AfterAttributeValueQuotedState);
        else if (cc == '&') {
            m_additionalAllowedCharacter = '"';
            ADVANCE_TO(CharacterReferenceInAttributeValueState);
        } else {
            m_token->appendToAttributeValue(cc);
            ADVANCE_TO(AttributeValueDoubleQuotedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(AttributeValueSingleQuotedState) {
        if (cc == '\'')
            ADVANCE_TO(AfterAttributeValueQuotedState);
        else if (cc == '&') {
            m_additionalAllowedCharacter = '\'';
            ADVANCE_TO(CharacterReferenceInAttributeValueState);
        } else {
            m_token->appendToAttributeValue(cc);
            ADVANCE_TO(AttributeValueSingleQuotedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(AttributeValueUnquotedState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BeforeAttributeNameState);
        else if (cc == '&') {
            m_additionalAllowedCharacter = '>';
            ADVANCE_TO(CharacterReferenceInAttributeValueState);
        } else if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else {
            if (cc == '"' || cc == '\'' || cc == '<' || cc == '=' || cc == '`')
                emitParseError();
            m_token->appendToAttributeValue(cc);
            ADVANCE_TO(AttributeValueUnquotedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(CharacterReferenceInAttributeValueState) {
        bool notEnoughCharacters = false;
        unsigned value = consumeHTMLEntity(source, notEnoughCharacters, m_additionalAllowedCharacter);
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
            SWITCH_TO(AttributeValueDoubleQuotedState);
        else if (m_additionalAllowedCharacter == '\'')
            SWITCH_TO(AttributeValueSingleQuotedState);
        else if (m_additionalAllowedCharacter == '>')
            SWITCH_TO(AttributeValueUnquotedState);
        else
            ASSERT_NOT_REACHED();
    }
    END_STATE()

    BEGIN_STATE(AfterAttributeValueQuotedState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BeforeAttributeNameState);
        else if (cc == '/')
            ADVANCE_TO(SelfClosingStartTagState);
        else if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else {
            emitParseError();
            RECONSUME_IN(BeforeAttributeNameState);
        }
        // FIXME: Handle EOF properly.
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
    }
    END_STATE()

    BEGIN_STATE(BogusCommentState) {
        // FIXME: This state isn't correct because we'll terminate the
        // comment early if we don't have the whole input stream available.
        m_token->beginComment();
        while (!source.isEmpty()) {
            cc = m_inputStreamPreprocessor.nextInputCharacter();
            if (cc == '>')
                EMIT_AND_RESUME_IN(DataState);
            m_token->appendToComment(cc);
            m_inputStreamPreprocessor.advance(source, m_lineNumber);
            // We ignore the return value (which indicates that |source| is
            // empty) because it's checked by the loop condition above.
        }
        m_state = DataState;
        return true;
        // FIXME: Handle EOF properly.
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
                SWITCH_TO(CommentStartState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return shouldEmitBufferedCharacterToken(source);
        } else if (cc == 'D' || cc == 'd') {
            SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(doctypeString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERTIgnoringCase(source, "doctype");
                SWITCH_TO(DOCTYPEState);
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
            ADVANCE_TO(CommentStartDashState);
        else if (cc == '>') {
            emitParseError();
            EMIT_AND_RESUME_IN(DataState);
        } else {
            m_token->appendToComment(cc);
            ADVANCE_TO(CommentState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(CommentStartDashState) {
        if (cc == '-')
            ADVANCE_TO(CommentEndState);
        else if (cc == '>') {
            emitParseError();
            EMIT_AND_RESUME_IN(DataState);
        } else {
            m_token->appendToComment('-');
            m_token->appendToComment(cc);
            ADVANCE_TO(CommentState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(CommentState) {
        if (cc == '-')
            ADVANCE_TO(CommentEndDashState);
        else {
            m_token->appendToComment(cc);
            ADVANCE_TO(CommentState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(CommentEndDashState) {
        if (cc == '-')
            ADVANCE_TO(CommentEndState);
        else {
            m_token->appendToComment('-');
            m_token->appendToComment(cc);
            ADVANCE_TO(CommentState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(CommentEndState) {
        if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ') {
            emitParseError();
            m_token->appendToComment('-');
            m_token->appendToComment('-');
            m_token->appendToComment(cc);
            ADVANCE_TO(CommentEndSpaceState);
        } else if (cc == '!') {
            emitParseError();
            ADVANCE_TO(CommentEndBangState);
        } else if (cc == '-') {
            emitParseError();
            m_token->appendToComment('-');
            m_token->appendToComment(cc);
            ADVANCE_TO(CommentEndState);
        } else {
            emitParseError();
            m_token->appendToComment('-');
            m_token->appendToComment('-');
            m_token->appendToComment(cc);
            ADVANCE_TO(CommentState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(CommentEndBangState) {
        if (cc == '-') {
            m_token->appendToComment('-');
            m_token->appendToComment('-');
            m_token->appendToComment('!');
            ADVANCE_TO(CommentEndDashState);
        } else if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else {
            m_token->appendToComment('-');
            m_token->appendToComment('-');
            m_token->appendToComment('!');
            m_token->appendToComment(cc);
            ADVANCE_TO(CommentState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(CommentEndSpaceState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ') {
            m_token->appendToComment(cc);
            ADVANCE_TO(CommentEndSpaceState);
        } else if (cc == '-')
            ADVANCE_TO(CommentEndDashState);
        else if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else {
            m_token->appendToComment(cc);
            ADVANCE_TO(CommentState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(DOCTYPEState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BeforeDOCTYPENameState);
        else {
            emitParseError();
            RECONSUME_IN(BeforeDOCTYPENameState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(BeforeDOCTYPENameState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BeforeDOCTYPENameState);
        else if (cc >= 'A' && cc <= 'Z') {
            m_token->beginDOCTYPE(toLowerCase(cc));
            ADVANCE_TO(DOCTYPENameState);
        } else if (cc == '>') {
            emitParseError();
            m_token->beginDOCTYPE();
            m_token->setForceQuirks();
            EMIT_AND_RESUME_IN(DataState);
        } else {
            m_token->beginDOCTYPE(cc);
            ADVANCE_TO(DOCTYPENameState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(DOCTYPENameState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(AfterDOCTYPENameState);
        else if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else if (cc >= 'A' && cc <= 'Z') {
            m_token->appendToName(toLowerCase(cc));
            ADVANCE_TO(DOCTYPENameState);
        } else {
            m_token->appendToName(cc);
            ADVANCE_TO(DOCTYPENameState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(AfterDOCTYPENameState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(AfterDOCTYPENameState);
        if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else {
            DEFINE_STATIC_LOCAL(String, publicString, ("public"));
            DEFINE_STATIC_LOCAL(String, systemString, ("system"));
            if (cc == 'P' || cc == 'p') {
                SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(publicString);
                if (result == SegmentedString::DidMatch) {
                    advanceStringAndASSERTIgnoringCase(source, "public");
                    SWITCH_TO(AfterDOCTYPEPublicKeywordState);
                } else if (result == SegmentedString::NotEnoughCharacters)
                    return shouldEmitBufferedCharacterToken(source);
            } else if (cc == 'S' || cc == 's') {
                SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(systemString);
                if (result == SegmentedString::DidMatch) {
                    advanceStringAndASSERTIgnoringCase(source, "system");
                    SWITCH_TO(AfterDOCTYPESystemKeywordState);
                } else if (result == SegmentedString::NotEnoughCharacters)
                    return shouldEmitBufferedCharacterToken(source);
            }
            emitParseError();
            m_token->setForceQuirks();
            ADVANCE_TO(BogusDOCTYPEState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(AfterDOCTYPEPublicKeywordState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BeforeDOCTYPEPublicIdentifierState);
        else if (cc == '"') {
            emitParseError();
            m_token->setPublicIdentifierToEmptyString();
            ADVANCE_TO(DOCTYPEPublicIdentifierDoubleQuotedState);
        } else if (cc == '\'') {
            emitParseError();
            m_token->setPublicIdentifierToEmptyString();
            ADVANCE_TO(DOCTYPEPublicIdentifierSingleQuotedState);
        } else if (cc == '>') {
            emitParseError();
            m_token->setForceQuirks();
            EMIT_AND_RESUME_IN(DataState);
        } else {
            emitParseError();
            m_token->setForceQuirks();
            ADVANCE_TO(BogusDOCTYPEState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(BeforeDOCTYPEPublicIdentifierState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BeforeDOCTYPEPublicIdentifierState);
        else if (cc == '"') {
            m_token->setPublicIdentifierToEmptyString();
            ADVANCE_TO(DOCTYPEPublicIdentifierDoubleQuotedState);
        } else if (cc == '\'') {
            m_token->setPublicIdentifierToEmptyString();
            ADVANCE_TO(DOCTYPEPublicIdentifierSingleQuotedState);
        } else if (cc == '>') {
            emitParseError();
            m_token->setForceQuirks();
            EMIT_AND_RESUME_IN(DataState);
        } else {
            emitParseError();
            m_token->setForceQuirks();
            ADVANCE_TO(BogusDOCTYPEState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(DOCTYPEPublicIdentifierDoubleQuotedState) {
        if (cc == '"')
            ADVANCE_TO(AfterDOCTYPEPublicIdentifierState);
        else if (cc == '>') {
            emitParseError();
            m_token->setForceQuirks();
            EMIT_AND_RESUME_IN(DataState);
        } else {
            m_token->appendToPublicIdentifier(cc);
            ADVANCE_TO(DOCTYPEPublicIdentifierDoubleQuotedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(DOCTYPEPublicIdentifierSingleQuotedState) {
        if (cc == '\'')
            ADVANCE_TO(AfterDOCTYPEPublicIdentifierState);
        else if (cc == '>') {
            emitParseError();
            m_token->setForceQuirks();
            EMIT_AND_RESUME_IN(DataState);
        } else {
            m_token->appendToPublicIdentifier(cc);
            ADVANCE_TO(DOCTYPEPublicIdentifierSingleQuotedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(AfterDOCTYPEPublicIdentifierState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BetweenDOCTYPEPublicAndSystemIdentifiersState);
        else if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else if (cc == '"') {
            emitParseError();
            m_token->setSystemIdentifierToEmptyString();
            ADVANCE_TO(DOCTYPESystemIdentifierDoubleQuotedState);
        } else if (cc == '\'') {
            emitParseError();
            m_token->setSystemIdentifierToEmptyString();
            ADVANCE_TO(DOCTYPESystemIdentifierSingleQuotedState);
        } else {
            emitParseError();
            m_token->setForceQuirks();
            ADVANCE_TO(BogusDOCTYPEState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(BetweenDOCTYPEPublicAndSystemIdentifiersState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BetweenDOCTYPEPublicAndSystemIdentifiersState);
        else if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else if (cc == '"') {
            m_token->setSystemIdentifierToEmptyString();
            ADVANCE_TO(DOCTYPESystemIdentifierDoubleQuotedState);
        } else if (cc == '\'') {
            m_token->setSystemIdentifierToEmptyString();
            ADVANCE_TO(DOCTYPESystemIdentifierSingleQuotedState);
        } else {
            emitParseError();
            m_token->setForceQuirks();
            ADVANCE_TO(BogusDOCTYPEState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(AfterDOCTYPESystemKeywordState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BeforeDOCTYPESystemIdentifierState);
        else if (cc == '"') {
            emitParseError();
            m_token->setSystemIdentifierToEmptyString();
            ADVANCE_TO(DOCTYPESystemIdentifierDoubleQuotedState);
        } else if (cc == '\'') {
            emitParseError();
            m_token->setSystemIdentifierToEmptyString();
            ADVANCE_TO(DOCTYPESystemIdentifierSingleQuotedState);
        } else if (cc == '>') {
            emitParseError();
            m_token->setForceQuirks();
            EMIT_AND_RESUME_IN(DataState);
        } else {
            emitParseError();
            m_token->setForceQuirks();
            ADVANCE_TO(BogusDOCTYPEState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(BeforeDOCTYPESystemIdentifierState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(BeforeDOCTYPESystemIdentifierState);
        if (cc == '"') {
            m_token->setSystemIdentifierToEmptyString();
            ADVANCE_TO(DOCTYPESystemIdentifierDoubleQuotedState);
        } else if (cc == '\'') {
            m_token->setSystemIdentifierToEmptyString();
            ADVANCE_TO(DOCTYPESystemIdentifierSingleQuotedState);
        } else if (cc == '>') {
            emitParseError();
            m_token->setForceQuirks();
            EMIT_AND_RESUME_IN(DataState);
        } else {
            emitParseError();
            m_token->setForceQuirks();
            ADVANCE_TO(BogusDOCTYPEState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(DOCTYPESystemIdentifierDoubleQuotedState) {
        if (cc == '"')
            ADVANCE_TO(AfterDOCTYPESystemIdentifierState);
        else if (cc == '>') {
            emitParseError();
            m_token->setForceQuirks();
            EMIT_AND_RESUME_IN(DataState);
        } else {
            m_token->appendToSystemIdentifier(cc);
            ADVANCE_TO(DOCTYPESystemIdentifierDoubleQuotedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(DOCTYPESystemIdentifierSingleQuotedState) {
        if (cc == '\'')
            ADVANCE_TO(AfterDOCTYPESystemIdentifierState);
        else if (cc == '>') {
            emitParseError();
            m_token->setForceQuirks();
            EMIT_AND_RESUME_IN(DataState);
        } else {
            m_token->appendToSystemIdentifier(cc);
            ADVANCE_TO(DOCTYPESystemIdentifierSingleQuotedState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(AfterDOCTYPESystemIdentifierState) {
        if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
            ADVANCE_TO(AfterDOCTYPESystemIdentifierState);
        else if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        else {
            emitParseError();
            ADVANCE_TO(BogusDOCTYPEState);
        }
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(BogusDOCTYPEState) {
        if (cc == '>')
            EMIT_AND_RESUME_IN(DataState);
        ADVANCE_TO(BogusDOCTYPEState);
        // FIXME: Handle EOF properly.
    }
    END_STATE()

    BEGIN_STATE(CDATASectionState) {
        notImplemented();
        ADVANCE_TO(CDATASectionState);
    }
    END_STATE()

    }

    ASSERT_NOT_REACHED();
    return false;
}

inline bool HTMLTokenizer::temporaryBufferIs(const String& expectedString)
{
    return vectorEqualsString(m_temporaryBuffer, expectedString);
}

inline void HTMLTokenizer::addToPossibleEndTag(UChar cc)
{
    ASSERT(isEndTagBufferingState(m_state));
    m_bufferedEndTagName.append(cc);
}

inline bool HTMLTokenizer::isAppropriateEndTag()
{
    return m_bufferedEndTagName == m_appropriateEndTagName;
}

inline void HTMLTokenizer::emitCharacter(UChar character)
{
    if (m_token->type() != HTMLToken::Character) {
        m_token->beginCharacter(character);
        return;
    }
    m_token->appendToCharacter(character);
}

inline void HTMLTokenizer::emitCodePoint(unsigned value)
{
    if (value < 0xFFFF) {
        emitCharacter(value);
        return;
    }
    emitCharacter(U16_LEAD(value));
    emitCharacter(U16_TRAIL(value));
}

inline void HTMLTokenizer::emitParseError()
{
    notImplemented();
}

inline void HTMLTokenizer::emitCurrentToken()
{
    ASSERT(m_token->type() != HTMLToken::Uninitialized);
    if (m_token->type() == HTMLToken::StartTag)
        m_appropriateEndTagName = m_token->name();
}

inline bool HTMLTokenizer::shouldEmitBufferedCharacterToken(const SegmentedString& source)
{
    return source.isClosed() && m_token->type() == HTMLToken::Character;
}

}

