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

#ifndef MarkupTokenizerBase_h
#define MarkupTokenizerBase_h

#include "SegmentedString.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/TextPosition.h>

namespace WebCore {

// Never use this type for a variable, as it contains several non-virtual functions.
template<typename Token, typename State>
class MarkupTokenizerBase {
    WTF_MAKE_NONCOPYABLE(MarkupTokenizerBase);
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~MarkupTokenizerBase() { }

    OrdinalNumber lineNumber() const { return OrdinalNumber::fromZeroBasedInt(m_lineNumber); }

    typename State::State state() const { return m_state; }
    void setState(typename State::State state) { m_state = state; }

    bool forceNullCharacterReplacement() const { return m_forceNullCharacterReplacement; }
    void setForceNullCharacterReplacement(bool value) { m_forceNullCharacterReplacement = value; }

    // This method needs to be defined in a template specialization when subclassing this template
    inline bool shouldSkipNullCharacters() const;

protected:
    // http://www.whatwg.org/specs/web-apps/current-work/#preprocessing-the-input-stream
    class InputStreamPreprocessor {
        WTF_MAKE_NONCOPYABLE(InputStreamPreprocessor);
    public:
        InputStreamPreprocessor(MarkupTokenizerBase<Token, State>* tokenizer)
            : m_tokenizer(tokenizer)
            , m_nextInputCharacter('\0')
            , m_skipNextNewLine(false)
        {
        }

        UChar nextInputCharacter() const { return m_nextInputCharacter; }

        // Returns whether we succeeded in peeking at the next character.
        // The only way we can fail to peek is if there are no more
        // characters in |source| (after collapsing \r\n, etc).
        ALWAYS_INLINE bool peek(SegmentedString& source, int& lineNumber)
        {
        PeekAgain:
            m_nextInputCharacter = *source;

            // Every branch in this function is expensive, so we have a
            // fast-reject branch for characters that don't require special
            // handling. Please run the parser benchmark whenever you touch
            // this function. It's very hot.
            static const UChar specialCharacterMask = '\n' | '\r' | '\0';
            if (m_nextInputCharacter & ~specialCharacterMask) {
                m_skipNextNewLine = false;
                return true;
            }

            if (m_nextInputCharacter == '\n' && m_skipNextNewLine) {
                m_skipNextNewLine = false;
                source.advancePastNewline(lineNumber);
                if (source.isEmpty())
                    return false;
                m_nextInputCharacter = *source;
            }
            if (m_nextInputCharacter == '\r') {
                m_nextInputCharacter = '\n';
                m_skipNextNewLine = true;
            } else {
                m_skipNextNewLine = false;
                // FIXME: The spec indicates that the surrogate pair range as well as
                // a number of specific character values are parse errors and should be replaced
                // by the replacement character. We suspect this is a problem with the spec as doing
                // that filtering breaks surrogate pair handling and causes us not to match Minefield.
                if (m_nextInputCharacter == '\0' && !shouldTreatNullAsEndOfFileMarker(source)) {
                    if (m_tokenizer->shouldSkipNullCharacters()) {
                        source.advancePastNonNewline();
                        if (source.isEmpty())
                            return false;
                        goto PeekAgain;
                    }
                    m_nextInputCharacter = 0xFFFD;
                }
            }
            return true;
        }

        // Returns whether there are more characters in |source| after advancing.
        bool advance(SegmentedString& source, int& lineNumber)
        {
            source.advance(lineNumber);
            if (source.isEmpty())
                return false;
            return peek(source, lineNumber);
        }

        static const UChar endOfFileMarker = 0;

    private:
        bool shouldTreatNullAsEndOfFileMarker(SegmentedString& source) const
        {
            return source.isClosed() && source.length() == 1;
        }

        MarkupTokenizerBase<Token, State>* m_tokenizer;

        // http://www.whatwg.org/specs/web-apps/current-work/#next-input-character
        UChar m_nextInputCharacter;
        bool m_skipNextNewLine;
    };

    MarkupTokenizerBase() : m_inputStreamPreprocessor(this) { reset(); }

    inline void bufferCharacter(UChar character)
    {
        ASSERT(character != InputStreamPreprocessor::endOfFileMarker);
        m_token->ensureIsCharacterToken();
        m_token->appendToCharacter(character);
    }

    inline void bufferCodePoint(unsigned);

    // This method can get hidden in subclasses
    inline bool emitAndResumeIn(SegmentedString& source, typename State::State state)
    {
        m_state = state;
        source.advance(m_lineNumber);
        return true;
    }
    
    // This method can get hidden in subclasses
    inline bool emitAndReconsumeIn(SegmentedString&, typename State::State state)
    {
        m_state = state;
        return true;
    }

    inline bool emitEndOfFile(SegmentedString& source)
    {
        if (haveBufferedCharacterToken())
            return true;
        m_state = State::DataState;
        source.advance(m_lineNumber);
        m_token->clear();
        m_token->makeEndOfFile();
        return true;
    }

    void reset()
    {
        m_state = State::DataState;
        m_token = 0;
        m_lineNumber = 0;
    }

    inline bool haveBufferedCharacterToken()
    {
        return m_token->type() == Token::Type::Character;
    }
    
    typename State::State m_state;

    // m_token is owned by the caller. If nextToken is not on the stack,
    // this member might be pointing to unallocated memory.
    Token* m_token;
    int m_lineNumber;

    bool m_skipLeadingNewLineForListing;
    bool m_forceNullCharacterReplacement;

    // http://www.whatwg.org/specs/web-apps/current-work/#additional-allowed-character
    UChar m_additionalAllowedCharacter;

    // http://www.whatwg.org/specs/web-apps/current-work/#preprocessing-the-input-stream
    InputStreamPreprocessor m_inputStreamPreprocessor;
};

}

#endif // MarkupTokenizerBase_h
