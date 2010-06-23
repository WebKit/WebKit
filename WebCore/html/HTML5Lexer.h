/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef HTML5Lexer_h
#define HTML5Lexer_h

#include "AtomicString.h"
#include "SegmentedString.h"
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

class HTML5Token;

class HTML5Lexer : public Noncopyable {
public:
    enum State {
        DataState,
        CharacterReferenceInDataState,
        RCDATAState,
        CharacterReferenceInRCDATAState,
        RAWTEXTState,
        ScriptDataState,
        PLAINTEXTState,
        TagOpenState,
        EndTagOpenState,
        TagNameState,
        RCDATALessThanSignState,
        RCDATAEndTagOpenState,
        RCDATAEndTagNameState,
        RAWTEXTLessThanSignState,
        RAWTEXTEndTagOpenState,
        RAWTEXTEndTagNameState,
        ScriptDataLessThanSignState,
        ScriptDataEndTagOpenState,
        ScriptDataEndTagNameState,
        ScriptDataEscapeStartState,
        ScriptDataEscapeStartDashState,
        ScriptDataEscapedState,
        ScriptDataEscapedDashState,
        ScriptDataEscapedDashDashState,
        ScriptDataEscapedLessThanSignState,
        ScriptDataEscapedEndTagOpenState,
        ScriptDataEscapedEndTagNameState,
        ScriptDataDoubleEscapeStartState,
        ScriptDataDoubleEscapedState,
        ScriptDataDoubleEscapedDashState,
        ScriptDataDoubleEscapedDashDashState,
        ScriptDataDoubleEscapedLessThanSignState,
        ScriptDataDoubleEscapeEndState,
        BeforeAttributeNameState,
        AttributeNameState,
        AfterAttributeNameState,
        BeforeAttributeValueState,
        AttributeValueDoubleQuotedState,
        AttributeValueSingleQuotedState,
        AttributeValueUnquotedState,
        CharacterReferenceInAttributeValueState,
        AfterAttributeValueQuotedState,
        SelfClosingStartTagState,
        BogusCommentState,
        MarkupDeclarationOpenState,
        CommentStartState,
        CommentStartDashState,
        CommentState,
        CommentEndDashState,
        CommentEndState,
        CommentEndBangState,
        CommentEndSpaceState,
        DOCTYPEState,
        BeforeDOCTYPENameState,
        DOCTYPENameState,
        AfterDOCTYPENameState,
        AfterDOCTYPEPublicKeywordState,
        BeforeDOCTYPEPublicIdentifierState,
        DOCTYPEPublicIdentifierDoubleQuotedState,
        DOCTYPEPublicIdentifierSingleQuotedState,
        AfterDOCTYPEPublicIdentifierState,
        BetweenDOCTYPEPublicAndSystemIdentifiersState,
        AfterDOCTYPESystemKeywordState,
        BeforeDOCTYPESystemIdentifierState,
        DOCTYPESystemIdentifierDoubleQuotedState,
        DOCTYPESystemIdentifierSingleQuotedState,
        AfterDOCTYPESystemIdentifierState,
        BogusDOCTYPEState,
        CDATASectionState,
    };

    HTML5Lexer();
    ~HTML5Lexer();

    void reset();

    // This function returns true if it emits a token.  Otherwise, callers
    // must provide the same (in progress) token on the next call (unless
    // they call reset() first).
    bool nextToken(SegmentedString&, HTML5Token&);

    int lineNumber() const { return m_lineNumber; }
    int columnNumber() const { return 1; } // Matches LegacyHTMLDocumentParser.h behavior.

    State state() const { return m_state; }
    void setState(State state) { m_state = state; }

    // Hack to skip leading newline in <pre>/<listing> for authoring ease.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-inbody
    void skipLeadingNewLineForListing() { m_skipLeadingNewLineForListing = true; }

private:
    // http://www.whatwg.org/specs/web-apps/current-work/#preprocessing-the-input-stream
    class InputStreamPreprocessor : public Noncopyable {
    public:
        InputStreamPreprocessor()
            : m_nextInputCharacter('\0')
            , m_skipNextNewLine(false)
        {
        }

        UChar nextInputCharacter() const { return m_nextInputCharacter; }

        // Returns whether we succeeded in peeking at the next character.
        // The only way we can fail to peek is if there are no more
        // characters in |source| (after collapsing \r\n, etc).
        bool peek(SegmentedString& source, int& lineNumber)
        {
            m_nextInputCharacter = *source;
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
                if (m_nextInputCharacter == '\0')
                    m_nextInputCharacter = 0xFFFD;
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

    private:
        // http://www.whatwg.org/specs/web-apps/current-work/#next-input-character
        UChar m_nextInputCharacter;
        bool m_skipNextNewLine;
    };

    inline void emitCharacter(UChar);
    inline void emitParseError();
    inline void emitCurrentToken();
    inline void emitCodePoint(unsigned);

    inline bool processEntity(SegmentedString& source);

    inline bool temporaryBufferIs(const String&);

    // Sometimes we speculatively consume input characters and we don't
    // know whether they represent end tags or RCDATA, etc.  These
    // functions help manage these state.
    inline void addToPossibleEndTag(UChar cc);
    inline bool isAppropriateEndTag();

    inline bool shouldEmitBufferedCharacterToken(const SegmentedString&);

    State m_state;

    Vector<UChar, 32> m_appropriateEndTagName;

    // m_token is owned by the caller.  If nextToken is not on the stack,
    // this member might be pointing to unallocated memory.
    HTML5Token* m_token;
    int m_lineNumber;

    bool m_skipLeadingNewLineForListing;

    // http://www.whatwg.org/specs/web-apps/current-work/#temporary-buffer
    Vector<UChar, 32> m_temporaryBuffer;

    // We occationally want to emit both a character token and an end tag
    // token (e.g., when lexing script).  We buffer the name of the end tag
    // token here so we remember it next time we re-enter the lexer.
    Vector<UChar, 32> m_bufferedEndTagName;

    // http://www.whatwg.org/specs/web-apps/current-work/#additional-allowed-character
    UChar m_additionalAllowedCharacter;

    // http://www.whatwg.org/specs/web-apps/current-work/#preprocessing-the-input-stream
    InputStreamPreprocessor m_inputStreamPreprocessor;
};

}

#endif
