/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef PreloadScanner_h
#define PreloadScanner_h

#include "AtomicString.h"
#include "SegmentedString.h"
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {
    
    class CachedResource;
    class CachedResourceClient;
    class Document;
    
    class PreloadScanner : public Noncopyable {
    public:
        PreloadScanner(Document*);
        ~PreloadScanner();
        void begin();
        void write(const SegmentedString&);
        void end();
        bool inProgress() const { return m_inProgress; }
        
        bool scanningBody() const;
        
        static unsigned consumeEntity(SegmentedString&, bool& notEnoughCharacters);
        
    private:
        void tokenize(const SegmentedString&);
        void reset();
        
        void emitTag();
        void emitCharacter(UChar);
        
        void tokenizeCSS(UChar);
        void emitCSSRule();
        
        void processAttribute();

        
        void clearLastCharacters();
        void rememberCharacter(UChar);
        bool lastCharactersMatch(const char*, unsigned count) const;
        
        bool m_inProgress;
        SegmentedString m_source;
        
        enum State {
            Data,
            EntityData,
            TagOpen,
            CloseTagOpen,
            TagName,
            BeforeAttributeName,
            AttributeName,
            AfterAttributeName,
            BeforeAttributeValue,
            AttributeValueDoubleQuoted,
            AttributeValueSingleQuoted,
            AttributeValueUnquoted,
            EntityInAttributeValue,
            BogusComment,
            MarkupDeclarationOpen,
            CommentStart,
            CommentStartDash,
            Comment,
            CommentEndDash,
            CommentEnd
        };
        State m_state;
        bool m_escape;
        enum ContentModel {
            PCDATA,
            RCDATA,
            CDATA,
            PLAINTEXT
        };
        ContentModel m_contentModel;
        unsigned m_commentPos;
        State m_stateBeforeEntityInAttributeValue;
        
        static const unsigned lastCharactersBufferSize = 8;
        UChar m_lastCharacters[lastCharactersBufferSize];
        unsigned m_lastCharacterIndex;
        
        bool m_closeTag;
        Vector<UChar, 32> m_tagName;
        Vector<UChar, 32> m_attributeName;
        Vector<UChar> m_attributeValue;
        AtomicString m_lastStartTag;
        
        String m_urlToLoad;
        String m_charset;
        bool m_linkIsStyleSheet;
        
        enum CSSState {
            CSSInitial,
            CSSMaybeComment,
            CSSComment,
            CSSMaybeCommentEnd,
            CSSRuleStart,
            CSSRule,
            CSSAfterRule,
            CSSRuleValue,
            CSSAfterRuleValue
        };
        CSSState m_cssState;
        Vector<UChar, 16> m_cssRule;
        Vector<UChar> m_cssRuleValue;
        
        double m_timeUsed;
        
        bool m_bodySeen;
        Document* m_document;
    };

}

#endif
