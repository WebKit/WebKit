/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef LiteralParser_h
#define LiteralParser_h

#include "Identifier.h"
#include "JSGlobalObjectFunctions.h"
#include "JSValue.h"
#include "UString.h"

namespace JSC {

    class LiteralParser {
    public:
        typedef enum { StrictJSON, NonStrictJSON, JSONP } ParserMode;
        LiteralParser(ExecState* exec, const UChar* characters, unsigned length, ParserMode mode)
            : m_exec(exec)
            , m_lexer(characters, length, mode)
            , m_mode(mode)
        {
        }
        
        UString getErrorMessage() 
        { 
            if (!m_lexer.getErrorMessage().isEmpty())
                return String::format("JSON Parse error: %s", m_lexer.getErrorMessage().ascii().data()).impl();
            if (!m_parseErrorMessage.isEmpty())
                return String::format("JSON Parse error: %s", m_parseErrorMessage.ascii().data()).impl();
            return "JSON Parse error: Unable to parse JSON string";
        }
        
        JSValue tryLiteralParse()
        {
            m_lexer.next();
            JSValue result = parse(m_mode == StrictJSON ? StartParseExpression : StartParseStatement);
            if (m_lexer.currentToken().type == TokSemi)
                m_lexer.next();
            if (m_lexer.currentToken().type != TokEnd)
                return JSValue();
            return result;
        }
        
        enum JSONPPathEntryType {
            JSONPPathEntryTypeDeclare, // var pathEntryName = JSON
            JSONPPathEntryTypeDot, // <prior entries>.pathEntryName = JSON
            JSONPPathEntryTypeLookup, // <prior entries>[pathIndex] = JSON
            JSONPPathEntryTypeCall // <prior entries>(JSON)
        };

        struct JSONPPathEntry {
            JSONPPathEntryType m_type;
            Identifier m_pathEntryName;
            int m_pathIndex;
        };

        struct JSONPData {
            Vector<JSONPPathEntry> m_path;
            Strong<Unknown> m_value;
        };

        bool tryJSONPParse(Vector<JSONPData>&, bool needsFullSourceInfo);

    private:
        enum ParserState { StartParseObject, StartParseArray, StartParseExpression, 
                           StartParseStatement, StartParseStatementEndStatement, 
                           DoParseObjectStartExpression, DoParseObjectEndExpression,
                           DoParseArrayStartExpression, DoParseArrayEndExpression };
        enum TokenType { TokLBracket, TokRBracket, TokLBrace, TokRBrace, 
                         TokString, TokIdentifier, TokNumber, TokColon, 
                         TokLParen, TokRParen, TokComma, TokTrue, TokFalse,
                         TokNull, TokEnd, TokDot, TokAssign, TokSemi, TokError };
        
        class Lexer {
        public:
            struct LiteralParserToken {
                TokenType type;
                const UChar* start;
                const UChar* end;
                UString stringBuffer;
                union {
                    double numberToken;
                    struct {
                        const UChar* stringToken;
                        int stringLength;
                    };
                };
            };
            Lexer(const UChar* characters, unsigned length, ParserMode mode)
                : m_mode(mode)
                , m_ptr(characters)
                , m_end(characters + length)
            {
            }
            
            TokenType next();
            
            const LiteralParserToken& currentToken()
            {
                return m_currentToken;
            }
            
            UString getErrorMessage() { return m_lexErrorMessage; }
            
        private:
            UString m_lexErrorMessage;
            template <ParserMode mode> TokenType lex(LiteralParserToken&);
            template <ParserMode mode, UChar terminator> ALWAYS_INLINE TokenType lexString(LiteralParserToken&);
            ALWAYS_INLINE TokenType lexNumber(LiteralParserToken&);
            LiteralParserToken m_currentToken;
            UString m_string;
            ParserMode m_mode;
            const UChar* m_ptr;
            const UChar* m_end;
        };
        
        class StackGuard;
        JSValue parse(ParserState);

        ExecState* m_exec;
        LiteralParser::Lexer m_lexer;
        ParserMode m_mode;
        UString m_parseErrorMessage;
        static unsigned const MaximumCachableCharacter = 128;
        FixedArray<Identifier, MaximumCachableCharacter> m_shortIdentifiers;
        FixedArray<Identifier, MaximumCachableCharacter> m_recentIdentifiers;
        ALWAYS_INLINE const Identifier makeIdentifier(const UChar* characters, size_t length);
    };

}

#endif
