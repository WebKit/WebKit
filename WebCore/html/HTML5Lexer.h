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

        void setState(State state) { m_state = state; }

    private:
        inline void emitCharacter(UChar);
        inline void emitParseError();
        inline void emitCurrentToken();

        UChar consumeEntity(SegmentedString&, bool& notEnoughCharacters);

        inline bool temporaryBufferIs(const String&);
        inline bool isAppropriateEndTag();

        inline void maybeFlushBufferedEndTag();
        inline void flushBufferedEndTag();

        inline bool haveBufferedCharacterToken();

        State m_state;

        AtomicString m_appropriateEndTagName;

        // m_token is owned by the caller.  If nextToken is not on the stack,
        // this member might be pointing to unallocated memory.
        HTML5Token* m_token;

        bool m_emitPending;

        // http://www.whatwg.org/specs/web-apps/current-work/#temporary-buffer
        Vector<UChar, 32> m_temporaryBuffer;

        // We occationally want to emit both a character token and an end tag
        // token (e.g., when lexing script).  We buffer the name of the end tag
        // token here so we remember it next time we re-enter the lexer.
        Vector<UChar, 32> m_bufferedEndTagName;

        // http://www.whatwg.org/specs/web-apps/current-work/#additional-allowed-character
        UChar m_additionalAllowedCharacter;
    };

}

#endif
