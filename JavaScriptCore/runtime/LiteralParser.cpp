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

#include "config.h"
#include "LiteralParser.h"

#include "JSArray.h"
#include "JSString.h"
#include <wtf/ASCIICType.h>

namespace JSC {

class LiteralParser::StackGuard {
public:
    StackGuard(LiteralParser* parser) 
        : m_parser(parser)
    {
        m_parser->m_depth++;
    }
    ~StackGuard()
    {
        m_parser->m_depth--;
    }
    bool isSafe() { return m_parser->m_depth < 10; }
private:
    LiteralParser* m_parser;
};

static bool isSafeStringCharacter(UChar c)
{
    return (c >= ' ' && c <= 0xff && c != '\\') || c == '\t';
}

LiteralParser::TokenType LiteralParser::Lexer::lex(LiteralParserToken& token)
{
    while (m_ptr < m_end && isASCIISpace(*m_ptr))
        ++m_ptr;

    ASSERT(m_ptr <= m_end);
    if (m_ptr >= m_end) {
        token.type = TokEnd;
        token.start = token.end = m_ptr;
        return TokEnd;
    }
    token.type = TokError;
    token.start = m_ptr;
    switch (*m_ptr) {
        case '[':
            token.type = TokLBracket;
            token.end = ++m_ptr;
            return TokLBracket;
        case ']':
            token.type = TokRBracket;
            token.end = ++m_ptr;
            return TokRBracket;
        case '(':
            token.type = TokLParen;
            token.end = ++m_ptr;
            return TokLBracket;
        case ')':
            token.type = TokRParen;
            token.end = ++m_ptr;
            return TokRBracket;
        case '{':
            token.type = TokLBrace;
            token.end = ++m_ptr;
            return TokLBrace;
        case '}':
            token.type = TokRBrace;
            token.end = ++m_ptr;
            return TokRBrace;
        case ',':
            token.type = TokComma;
            token.end = ++m_ptr;
            return TokComma;
        case ':':
            token.type = TokColon;
            token.end = ++m_ptr;
            return TokColon;
        case '"':
        case '\'':
            return lexString(token);

        // Numbers are trickier so we only allow the most basic form, basically
        // * [1-9][0-9]*(\.[0-9]*)?
        // * \.[0-9]*
        // * 0(\.[0-9]*)?
        case '0':
            // If a number starts with 0 it's expected to be octal.  It seems silly
            // to attempt to handle this case, so we abort
            if (m_ptr < m_end - 1 && isASCIIDigit(m_ptr[1]))
                return TokError;
            return lexNumber(token);
        case '.':
            // If a number starts with a '.' it must be followed by a digit
            if (!(m_ptr < m_end - 1 && isASCIIDigit(m_ptr[1])))
                return TokError;
            return lexNumber(token);
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return lexNumber(token);
    }
    return TokError;
}

LiteralParser::TokenType LiteralParser::Lexer::lexString(LiteralParserToken& token)
{
    UChar terminator = *m_ptr;
    ++m_ptr;
    while (m_ptr < m_end && isSafeStringCharacter(*m_ptr) && *m_ptr != terminator)
        ++m_ptr;
    if (m_ptr >= m_end || *m_ptr != terminator) {
        token.type = TokError;
        token.end = ++m_ptr;
        return TokError;
    }
    token.type = TokString;
    token.end = ++m_ptr;
    return TokString;
}

LiteralParser::TokenType LiteralParser::Lexer::lexNumber(LiteralParserToken& token)
{
    bool beginsWithDot = *m_ptr == '.';
    ++m_ptr;
    while (m_ptr < m_end && isASCIIDigit(*m_ptr))
        ++m_ptr;

    if (!beginsWithDot && m_ptr < m_end - 1 && *m_ptr == '.') {
        ++m_ptr;
        while (m_ptr < m_end && isASCIIDigit(*m_ptr))
            ++m_ptr;
    }

    if (m_ptr < m_end) {
        if (*m_ptr == 'x' || *m_ptr == 'X' || *m_ptr == 'e' || *m_ptr == 'E') {
            token.type = TokError;
            return TokError;
        }
    }
    token.type = TokNumber;
    token.end = m_ptr;
    return TokNumber;
}

JSValue LiteralParser::parseStatement()
{
    StackGuard guard(this);
    if (!guard.isSafe())
        return abortParse();

    switch (m_lexer.currentToken().type) {
        case TokLBracket:
        case TokNumber:
        case TokString:
            return parseExpression();
        case TokLParen: {
            m_lexer.next();
            JSValue result = parseExpression();
            if (m_aborted || m_lexer.currentToken().type != TokRParen)
                return abortParse();
            m_lexer.next();
            return result;
        }
        default:
            return abortParse();
    }
}

JSValue LiteralParser::parseExpression()
{
    StackGuard guard(this);
    if (!guard.isSafe())
        return abortParse();
    switch (m_lexer.currentToken().type) {
        case TokLBracket:
            return parseArray();
        case TokLBrace:
            return parseObject();
        case TokString: {
            Lexer::LiteralParserToken stringToken = m_lexer.currentToken();
            m_lexer.next();
            return jsString(m_exec, UString(stringToken.start + 1, stringToken.end - stringToken.start - 2));
        }
        case TokNumber: {
            Lexer::LiteralParserToken numberToken = m_lexer.currentToken();
            m_lexer.next();
            return jsNumber(m_exec, UString(numberToken.start, numberToken.end - numberToken.start).toDouble());
        }
        default:
            return JSValue();
    }
}

JSValue LiteralParser::parseArray()
{
    StackGuard guard(this);
    if (!guard.isSafe())
        return abortParse();
    JSArray* array = constructEmptyArray(m_exec);
    while (true) {
        m_lexer.next();
        JSValue value = parseExpression();
        if (m_aborted)
            return JSValue();
        if (!value)
            break;
        array->push(m_exec, value);

        if (m_lexer.currentToken().type != TokComma)
            break;
    }
    if (m_lexer.currentToken().type != TokRBracket)
        return abortParse();

    m_lexer.next();
    return array;
}

JSValue LiteralParser::parseObject()
{
    StackGuard guard(this);
    if (!guard.isSafe())
        return abortParse();
    JSObject* object = constructEmptyObject(m_exec);
    
    while (m_lexer.next() == TokString) {
        Lexer::LiteralParserToken identifierToken = m_lexer.currentToken();
        
        // Check for colon
        if (m_lexer.next() != TokColon)
            return abortParse();
        m_lexer.next();
        
        JSValue value = parseExpression();
        if (!value || m_aborted)
            return abortParse();
        
        Identifier ident(m_exec, identifierToken.start + 1, identifierToken.end - identifierToken.start - 2);
        object->putDirect(ident, value);
        
        if (m_lexer.currentToken().type != TokComma)
            break;
    }
    
    if (m_lexer.currentToken().type != TokRBrace)
        return abortParse();
    m_lexer.next();
    return object;
}

}
