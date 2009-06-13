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
            return lexString(token);

        case '-':
        case '0':
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
    ++m_ptr;
    while (m_ptr < m_end && isSafeStringCharacter(*m_ptr) && *m_ptr != '"')
        ++m_ptr;
    if (m_ptr >= m_end || *m_ptr != '"') {
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
    // ES5 and json.org define numbers as
    // number
    //     int
    //     int frac? exp?
    //
    // int
    //     -? 0
    //     -? digit1-9 digits?
    //
    // digits
    //     digit digits?
    //
    // -?(0 | [1-9][0-9]*) ('.' [0-9]+)? ([eE][+-]? [0-9]+)?

    if (m_ptr < m_end && *m_ptr == '-') // -?
        ++m_ptr;
    
    // (0 | [1-9][0-9]*)
    if (m_ptr < m_end && *m_ptr == '0') // 0
        ++m_ptr;
    else if (m_ptr < m_end && *m_ptr >= '1' && *m_ptr <= '9') { // [1-9]
        ++m_ptr;
        // [0-9]*
        while (m_ptr < m_end && isASCIIDigit(*m_ptr))
            ++m_ptr;
    } else
        return TokError;

    // ('.' [0-9]+)?
    if (m_ptr < m_end && *m_ptr == '.') {
        ++m_ptr;
        // [0-9]+
        if (m_ptr >= m_end && !isASCIIDigit(*m_ptr))
            return TokError;

        ++m_ptr;
        while (m_ptr < m_end && isASCIIDigit(*m_ptr))
            ++m_ptr;
    }

    //  ([eE][+-]? [0-9]+)?
    if (m_ptr < m_end && (*m_ptr == 'e' || *m_ptr == 'E')) { // [eE]
        ++m_ptr;

        // [-+]?
        if (m_ptr < m_end && (*m_ptr == '-' || *m_ptr == '+'))
            ++m_ptr;

        // [0-9]+
        if (m_ptr >= m_end && !isASCIIDigit(*m_ptr))
            return TokError;
        
        ++m_ptr;
        while (m_ptr < m_end && isASCIIDigit(*m_ptr))
            ++m_ptr;
    }
    
    token.type = TokNumber;
    token.end = m_ptr;
    return TokNumber;
}

JSValue LiteralParser::parse(ParserState initialState)
{
    ParserState state = initialState;
    MarkedArgumentBuffer objectStack;
    JSValue lastValue;
    Vector<ParserState, 16> stateStack;
    Vector<Identifier, 16> identifierStack;
    while (1) {
        switch(state) {
            startParseArray:
            case StartParseArray: {
                JSArray* array = constructEmptyArray(m_exec);
                objectStack.append(array);
                // fallthrough
            }
            doParseArrayStartExpression:
            case DoParseArrayStartExpression: {
                if (m_lexer.next() == TokRBracket) {
                    m_lexer.next();
                    lastValue = objectStack.last();
                    objectStack.removeLast();
                    break;
                }

                stateStack.append(DoParseArrayEndExpression);
                goto startParseExpression;
            }
            case DoParseArrayEndExpression: {
                 asArray(objectStack.last())->push(m_exec, lastValue);
                
                if (m_lexer.currentToken().type == TokComma)
                    goto doParseArrayStartExpression;

                if (m_lexer.currentToken().type != TokRBracket)
                    return JSValue();
                
                m_lexer.next();
                lastValue = objectStack.last();
                objectStack.removeLast();
                break;
            }
            startParseObject:
            case StartParseObject: {
                JSObject* object = constructEmptyObject(m_exec);
                objectStack.append(object);
                // fallthrough
            }
            doParseObjectStartExpression:
            case DoParseObjectStartExpression: {
                TokenType type = m_lexer.next();
                if (type == TokString) {
                    Lexer::LiteralParserToken identifierToken = m_lexer.currentToken();

                    // Check for colon
                    if (m_lexer.next() != TokColon)
                        return JSValue();

                    m_lexer.next();
                    identifierStack.append(Identifier(m_exec, identifierToken.start + 1, identifierToken.end - identifierToken.start - 2));
                    stateStack.append(DoParseObjectEndExpression);
                    goto startParseExpression;
                } else if (type != TokRBrace) 
                    return JSValue();
                m_lexer.next();
                lastValue = objectStack.last();
                objectStack.removeLast();
                break;
            }
            case DoParseObjectEndExpression:
            {
                asObject(objectStack.last())->putDirect(identifierStack.last(), lastValue);
                identifierStack.removeLast();
                if (m_lexer.currentToken().type == TokComma)
                    goto doParseObjectStartExpression;
                if (m_lexer.currentToken().type != TokRBrace)
                    return JSValue();
                m_lexer.next();
                lastValue = objectStack.last();
                objectStack.removeLast();
                break;
            }
            startParseExpression:
            case StartParseExpression: {
                switch (m_lexer.currentToken().type) {
                    case TokLBracket:
                        goto startParseArray;
                    case TokLBrace:
                        goto startParseObject;
                    case TokString: {
                        Lexer::LiteralParserToken stringToken = m_lexer.currentToken();
                        m_lexer.next();
                        lastValue = jsString(m_exec, UString(stringToken.start + 1, stringToken.end - stringToken.start - 2));
                        break;
                    }
                    case TokNumber: {
                        Lexer::LiteralParserToken numberToken = m_lexer.currentToken();
                        m_lexer.next();
                        lastValue = jsNumber(m_exec, UString(numberToken.start, numberToken.end - numberToken.start).toDouble());
                        break;
                    }
                    default:
                        // Error
                        return JSValue();
                }
                break;
            }
            case StartParseStatement: {
                switch (m_lexer.currentToken().type) {
                    case TokLBracket:
                    case TokNumber:
                    case TokString:
                        goto startParseExpression;

                    case TokLParen: {
                        m_lexer.next();
                        stateStack.append(StartParseStatementEndStatement);
                        goto startParseExpression;
                    }
                    default:
                        return JSValue();
                }
            }
            case StartParseStatementEndStatement: {
                ASSERT(stateStack.isEmpty());
                if (m_lexer.currentToken().type != TokRParen)
                    return JSValue();
                if (m_lexer.next() == TokEnd)
                    return lastValue;
                return JSValue();
            }
            default:
                ASSERT_NOT_REACHED();
        }
        if (stateStack.isEmpty())
            return lastValue;
        state = stateStack.last();
        stateStack.removeLast();
        continue;
    }
}

}
