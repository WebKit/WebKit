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

#include "GetVM.h"
#include "Identifier.h"
#include "JSCJSValue.h"
#include <array>
#include <wtf/Range.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace JSC {

enum ParserMode : uint8_t { StrictJSON, NonStrictJSON, JSONP };
enum class JSONReviverMode : uint8_t { Disabled, Enabled };

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

struct JSONPPathEntry {
    Identifier m_pathEntryName;
    int m_pathIndex;
    JSONPPathEntryType m_type;
};

struct JSONPData {
    Vector<JSONPPathEntry> m_path;
    Strong<Unknown> m_value;
};

class JSONRanges {
public:
    struct Entry;
    using Object = HashMap<RefPtr<UniquedStringImpl>, Entry, IdentifierRepHash>;
    using Array = Vector<Entry>;
    struct Entry {
        WTF::Range<unsigned> range;
        std::variant<std::monostate, Object, Array> properties;
    };

    JSONRanges() = default;

    JSONRanges(Entry entry)
        : m_root(WTFMove(entry))
    { }

    const Entry& root() const { return m_root; }

private:
    Entry m_root { };
};

template <typename CharType>
struct LiteralParserToken {
private:
WTF_MAKE_NONCOPYABLE(LiteralParserToken);

public:
    LiteralParserToken() = default;

    TokenType type;
    const CharType* start;
    const CharType* end;
    union {
        double numberToken;
        struct {
            union {
                const LChar* stringToken8;
                const UChar* stringToken16;
            };
            unsigned stringIs8Bit : 1;
            unsigned stringLength : 31;
        };
    };
};

template <typename CharType>
ALWAYS_INLINE void setParserTokenString(LiteralParserToken<CharType>&, const CharType* string);

template <typename CharType>
class LiteralParser {
public:
    LiteralParser(JSGlobalObject* globalObject, const CharType* characters, unsigned length, ParserMode mode, CodeBlock* nullOrCodeBlock = nullptr)
        : m_globalObject(globalObject)
        , m_nullOrCodeBlock(nullOrCodeBlock)
        , m_lexer(characters, length, mode)
        , m_mode(mode)
    {
    }
    
    String getErrorMessage()
    { 
        if (!m_lexer.getErrorMessage().isEmpty())
            return "JSON Parse error: " + m_lexer.getErrorMessage();
        if (!m_parseErrorMessage.isEmpty())
            return "JSON Parse error: " + m_parseErrorMessage;
        return "JSON Parse error: Unable to parse JSON string"_s;
    }
    
    JSValue tryLiteralParse()
    {
        return tryLiteralParseImpl<JSONReviverMode::Disabled>(nullptr);
    }

    JSValue tryLiteralParse(JSONRanges* sourceRanges)
    {
        return tryLiteralParseImpl<JSONReviverMode::Enabled>(sourceRanges);
    }

    JSValue tryLiteralParsePrimitiveValue()
    {
        ASSERT(m_mode == StrictJSON);
        m_lexer.next();
        JSValue result = parsePrimitiveValue(getVM(m_globalObject));
        if (result) {
            if (m_lexer.currentToken()->type != TokEnd) {
                m_parseErrorMessage = "Unexpected content at end of JSON literal"_s;
                return JSValue();
            }
        }
        return result;
    }
    
    bool tryJSONPParse(Vector<JSONPData>&, bool needsFullSourceInfo);

private:
    class Lexer {
    public:
        Lexer(const CharType* characters, unsigned length, ParserMode mode)
            : m_mode(mode)
            , m_start(characters)
            , m_ptr(characters)
            , m_end(characters + length)
        {
        }
        
        TokenType next();
        
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

        const CharType* start() const { return m_start; }
        
    private:
        TokenType lex(LiteralParserToken<CharType>&);
        ALWAYS_INLINE TokenType lexIdentifier(LiteralParserToken<CharType>&);
        ALWAYS_INLINE TokenType lexString(LiteralParserToken<CharType>&, CharType terminator);
        TokenType lexStringSlow(LiteralParserToken<CharType>&, const CharType* runStart, CharType terminator);
        ALWAYS_INLINE TokenType lexNumber(LiteralParserToken<CharType>&);

        String m_lexErrorMessage;
        LiteralParserToken<CharType> m_currentToken;
        ParserMode m_mode;
        const CharType* m_start;
        const CharType* m_ptr;
        const CharType* m_end;
        StringBuilder m_builder;
#if ASSERT_ENABLED
        unsigned m_currentTokenID { 0 };
#endif
    };
    
    template<JSONReviverMode reviverMode>
    inline JSValue tryLiteralParseImpl(JSONRanges* sourceRanges = nullptr)
    {
        m_lexer.next();
        JSValue result = parse<reviverMode>(m_mode == StrictJSON ? StartParseExpression : StartParseStatement, sourceRanges);
        if (m_lexer.currentToken()->type == TokSemi)
            m_lexer.next();
        if (m_lexer.currentToken()->type != TokEnd)
            return JSValue();
        return result;
    }

    class StackGuard;
    template<JSONReviverMode reviverMode> JSValue parse(ParserState, JSONRanges*);

    JSValue parsePrimitiveValue(VM&);

    ALWAYS_INLINE Identifier makeIdentifier(VM&, typename Lexer::LiteralParserTokenPtr);
    ALWAYS_INLINE JSString* makeJSString(VM&, typename Lexer::LiteralParserTokenPtr);

    void setErrorMessageForToken(TokenType);

    JSGlobalObject* const m_globalObject;
    CodeBlock* const m_nullOrCodeBlock;
    typename LiteralParser<CharType>::Lexer m_lexer;
    const ParserMode m_mode;
    String m_parseErrorMessage;
};

} // namespace JSC
