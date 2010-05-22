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
        HTML5Lexer();
        ~HTML5Lexer();

        void reset();

        // This function returns true if it emits a token.  Otherwise, callers
        // must provide the same (in progress) token on the next call (unless
        // they call reset() first).
        bool nextToken(SegmentedString&, HTML5Token&);

        static unsigned consumeEntity(SegmentedString&, bool& notEnoughCharacters);

    private:
        inline void emitCommentToken();
        inline void emitCharacter(UChar);
        inline void emitParseError();
        inline void emitCurrentTagToken();
        inline void emitCurrentDoctypeToken();

        inline bool temporaryBufferIs(const char*);

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
            TokenizingCharacterReferencesState,
        };

        State m_state;

        // m_token is owned by the caller.  If nextToken is not on the stack,
        // this member might be pointing to unallocated memory.
        HTML5Token* m_token;

        bool m_emitPending;

        Vector<UChar, 32> m_attributeName;
        Vector<UChar, 32> m_attributeValue;

        // http://www.whatwg.org/specs/web-apps/current-work/#temporary-buffer
        Vector<UChar, 1024> m_temporaryBuffer;

        // http://www.whatwg.org/specs/web-apps/current-work/#additional-allowed-character
        UChar m_additionalAllowedCharacter;
    };

}

#endif
