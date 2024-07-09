/*
 * Copyright (C) 2009-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "ArgList.h"
#include "GetVM.h"
#include "Identifier.h"
#include "JSCJSValue.h"
#include <array>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace JSC {

enum ParserMode : uint8_t { StrictJSON, SloppyJSON, JSONP };

enum JSONPPathEntryType : uint8_t {
    JSONPPathEntryTypeDeclareVar, // var pathEntryName = JSON
    JSONPPathEntryTypeDot, // <prior entries>.pathEntryName = JSON
    JSONPPathEntryTypeLookup, // <prior entries>[pathIndex] = JSON
    JSONPPathEntryTypeCall // <prior entries>(JSON)
};

enum ParserState : uint8_t {
    StartParseObject, StartParseArray, StartParseExpression,
    StartParseStatement, StartParseStatementEndStatement,
    DoParseObjectStartExpression, DoParseObjectEndExpression,
    DoParseArrayStartExpression, DoParseArrayEndExpression };

enum TokenType : uint8_t {
    TokLBracket, TokRBracket, TokLBrace, TokRBrace,
    TokString, TokIdentifier, TokNumber, TokColon,
    TokLParen, TokRParen, TokComma, TokTrue, TokFalse,
    TokNull, TokEnd, TokDot, TokAssign, TokSemi, TokError, TokErrorSpace };

enum class JSONIdentifierHint : uint8_t { MaybeIdentifier, Unknown };

struct JSONPPathEntry {
    Identifier m_pathEntryName;
    int m_pathIndex;
    JSONPPathEntryType m_type;
};

struct JSONPData {
    Vector<JSONPPathEntry> m_path;
    Strong<Unknown> m_value;
};

template<typename CharacterType> struct LiteralParserToken {
    WTF_MAKE_NONCOPYABLE(LiteralParserToken);

    LiteralParserToken() = default;

    TokenType type;
    unsigned stringIs8Bit : 1; // Only used for TokString.
    unsigned stringOrIdentifierLength : 31;
    union {
        double numberToken; // Only used for TokNumber.
        const CharacterType* identifierStart;
        const LChar* stringStart8;
        const UChar* stringStart16;
    };

    std::span<const CharacterType> identifier() const { return { identifierStart, stringOrIdentifierLength }; }
    std::span<const LChar> string8() const { return { stringStart8, stringOrIdentifierLength }; }
    std::span<const UChar> string16() const { return { stringStart16, stringOrIdentifierLength }; }
};

template <typename CharType>
ALWAYS_INLINE void setParserTokenString(LiteralParserToken<CharType>&, const CharType* string);

template <typename CharType>
class LiteralParser {
public:
    LiteralParser(JSGlobalObject* globalObject, std::span<const CharType> characters, ParserMode mode, CodeBlock* nullOrCodeBlock = nullptr)
        : m_globalObject(globalObject)
        , m_nullOrCodeBlock(nullOrCodeBlock)
        , m_lexer(characters, mode)
        , m_mode(mode)
    {
    }
    
    String getErrorMessage()
    { 
        if (!m_lexer.getErrorMessage().isEmpty())
            return makeString("JSON Parse error: "_s, m_lexer.getErrorMessage());
        if (!m_parseErrorMessage.isEmpty())
            return makeString("JSON Parse error: "_s, m_parseErrorMessage);
        return "JSON Parse error: Unable to parse JSON string"_s;
    }
    
    JSValue tryLiteralParse()
    {
        m_lexer.next();
        JSValue result;
        VM& vm = getVM(m_globalObject);
        if (m_mode == StrictJSON)
            result = parseRecursivelyEntry(vm);
        else {
            result = parse(vm, StartParseStatement);
            if (m_lexer.currentToken()->type == TokSemi)
                m_lexer.next();
        }
        if (m_lexer.currentToken()->type != TokEnd)
            return JSValue();
        return result;
    }
    
    bool tryJSONPParse(Vector<JSONPData>&, bool needsFullSourceInfo);

private:
    class Lexer {
    public:
        Lexer(std::span<const CharType> characters, ParserMode mode)
            : m_mode(mode)
            , m_ptr(characters.data())
            , m_end(characters.data() + characters.size())
        {
        }
        
        TokenType next();
        TokenType nextMaybeIdentifier();
        
#if !ASSERT_ENABLED
        using LiteralParserTokenPtr = const LiteralParserToken<CharType>*;

        LiteralParserTokenPtr currentToken()
        {
            return &m_currentToken;
        }
#else
        class LiteralParserTokenPtr;
        friend class LiteralParserTokenPtr;
        class LiteralParserTokenPtr {
        public:
            LiteralParserTokenPtr(Lexer& lexer)
                : m_lexer(lexer)
                , m_tokenID(lexer.m_currentTokenID)
            {
            }

            ALWAYS_INLINE const LiteralParserToken<CharType>* operator->() const
            {
                ASSERT(m_tokenID == m_lexer.m_currentTokenID);
                return &m_lexer.m_currentToken;
            }

        private:
            Lexer& m_lexer;
            unsigned m_tokenID;
        };

        LiteralParserTokenPtr currentToken()
        {
            return LiteralParserTokenPtr(*this);
        }
#endif // ASSERT_ENABLED
        
        String getErrorMessage() { return m_lexErrorMessage; }
        
    private:
        template<JSONIdentifierHint>
        TokenType lex(LiteralParserToken<CharType>&);
        ALWAYS_INLINE TokenType lexIdentifier(LiteralParserToken<CharType>&);
        template<JSONIdentifierHint>
        ALWAYS_INLINE TokenType lexString(LiteralParserToken<CharType>&, CharType terminator);
        TokenType lexStringSlow(LiteralParserToken<CharType>&, const CharType* runStart, CharType terminator);
        ALWAYS_INLINE TokenType lexNumber(LiteralParserToken<CharType>&);

        String m_lexErrorMessage;
        LiteralParserToken<CharType> m_currentToken;
        ParserMode m_mode;
        const CharType* m_ptr;
        const CharType* m_end;
        StringBuilder m_builder;
#if ASSERT_ENABLED
        unsigned m_currentTokenID { 0 };
#endif
    };
    
    class StackGuard;
    JSValue parseRecursivelyEntry(VM&);
    JSValue parseRecursively(VM&, uint8_t* stackLimit);
    JSValue parse(VM&, ParserState);

    JSValue parsePrimitiveValue(VM&);

    ALWAYS_INLINE Identifier makeIdentifier(VM&, typename Lexer::LiteralParserTokenPtr);
    ALWAYS_INLINE JSString* makeJSString(VM&, typename Lexer::LiteralParserTokenPtr);

    void setErrorMessageForToken(TokenType);

    JSGlobalObject* const m_globalObject;
    CodeBlock* const m_nullOrCodeBlock;
    typename LiteralParser<CharType>::Lexer m_lexer;
    const ParserMode m_mode;
    String m_parseErrorMessage;
    HashSet<JSObject*> m_visitedUnderscoreProto;
    MarkedArgumentBuffer m_objectStack;
    Vector<ParserState, 16, UnsafeVectorOverflow> m_stateStack;
    Vector<Identifier, 16, UnsafeVectorOverflow> m_identifierStack;
};

} // namespace JSC
