/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006, 2007, 2008 Apple Inc. All Rights Reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "Lexer.h"

#include "dtoa.h"
#include "JSFunction.h"
#include "Nodes.h"
#include "NodeInfo.h"
#include "JSGlobalObjectFunctions.h"
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <wtf/Assertions.h>
#include <wtf/unicode/Unicode.h>

using namespace WTF;
using namespace Unicode;

// we can't specify the namespace in yacc's C output, so do it here
using namespace JSC;

#ifndef KDE_USE_FINAL
#include "Grammar.h"
#endif

#include "Lookup.h"
#include "Lexer.lut.h"

// a bridge for yacc from the C world to C++
int kjsyylex(void* lvalp, void* llocp, void* globalData)
{
    return static_cast<JSGlobalData*>(globalData)->lexer->lex(lvalp, llocp);
}

namespace JSC {

static bool isDecimalDigit(int);

Lexer::Lexer(JSGlobalData* globalData)
    : yylineno(1)
    , m_restrKeyword(false)
    , m_eatNextIdentifier(false)
    , m_stackToken(-1)
    , m_lastToken(-1)
    , m_position(0)
    , m_code(0)
    , m_length(0)
    , m_atLineStart(true)
    , m_current(0)
    , m_next1(0)
    , m_next2(0)
    , m_next3(0)
    , m_currentOffset(0)
    , m_nextOffset1(0)
    , m_nextOffset2(0)
    , m_nextOffset3(0)
    , m_globalData(globalData)
    , m_mainTable(JSC::mainTable)
{
    m_buffer8.reserveCapacity(initialReadBufferCapacity);
    m_buffer16.reserveCapacity(initialReadBufferCapacity);
}

Lexer::~Lexer()
{
    m_mainTable.deleteTable();
}

void Lexer::setCode(const SourceCode& source)
{
    yylineno = source.firstLine();
    m_restrKeyword = false;
    m_delimited = false;
    m_eatNextIdentifier = false;
    m_stackToken = -1;
    m_lastToken = -1;

    m_position = source.startOffset();
    m_source = &source;
    m_code = source.provider()->data();
    m_length = source.endOffset();
    m_skipLF = false;
    m_skipCR = false;
    m_error = false;
    m_atLineStart = true;

    // read first characters
    shift(4);
}

void Lexer::shift(unsigned p)
{
    // ECMA-262 calls for stripping Cf characters here, but we only do this for BOM,
    // see <https://bugs.webkit.org/show_bug.cgi?id=4931>.

    while (p--) {
        m_current = m_next1;
        m_next1 = m_next2;
        m_next2 = m_next3;
        m_currentOffset = m_nextOffset1;
        m_nextOffset1 = m_nextOffset2;
        m_nextOffset2 = m_nextOffset3;
        do {
            if (m_position >= m_length) {
                m_nextOffset3 = m_position;
                m_position++;
                m_next3 = -1;
                break;
            }
            m_nextOffset3 = m_position;
            m_next3 = m_code[m_position++];
        } while (m_next3 == 0xFEFF);
    }
}

// called on each new line
void Lexer::nextLine()
{
    yylineno++;
    m_atLineStart = true;
}

void Lexer::setDone(State s)
{
    m_state = s;
    m_done = true;
}

int Lexer::lex(void* p1, void* p2)
{
    YYSTYPE* lvalp = static_cast<YYSTYPE*>(p1);
    YYLTYPE* llocp = static_cast<YYLTYPE*>(p2);
    int token = 0;
    m_state = Start;
    unsigned short stringType = 0; // either single or double quotes
    m_buffer8.clear();
    m_buffer16.clear();
    m_done = false;
    m_terminator = false;
    m_skipLF = false;
    m_skipCR = false;

    // did we push a token on the stack previously ?
    // (after an automatic semicolon insertion)
    if (m_stackToken >= 0) {
        setDone(Other);
        token = m_stackToken;
        m_stackToken = 0;
    }
    int startOffset = m_currentOffset;
    while (!m_done) {
        if (m_skipLF && m_current != '\n') // found \r but not \n afterwards
            m_skipLF = false;
        if (m_skipCR && m_current != '\r') // found \n but not \r afterwards
            m_skipCR = false;
        if (m_skipLF || m_skipCR) { // found \r\n or \n\r -> eat the second one
            m_skipLF = false;
            m_skipCR = false;
            shift(1);
        }
        switch (m_state) {
            case Start:
                startOffset = m_currentOffset;
                if (isWhiteSpace()) {
                    // do nothing
                } else if (m_current == '/' && m_next1 == '/') {
                    shift(1);
                    m_state = InSingleLineComment;
                } else if (m_current == '/' && m_next1 == '*') {
                    shift(1);
                    m_state = InMultiLineComment;
                } else if (m_current == -1) {
                    if (!m_terminator && !m_delimited) {
                        // automatic semicolon insertion if program incomplete
                        token = ';';
                        m_stackToken = 0;
                        setDone(Other);
                    } else
                        setDone(Eof);
                } else if (isLineTerminator()) {
                    nextLine();
                    m_terminator = true;
                    if (m_restrKeyword) {
                        token = ';';
                        setDone(Other);
                    }
                } else if (m_current == '"' || m_current == '\'') {
                    m_state = InString;
                    stringType = static_cast<unsigned short>(m_current);
                } else if (isIdentStart(m_current)) {
                    record16(m_current);
                    m_state = InIdentifierOrKeyword;
                } else if (m_current == '\\')
                    m_state = InIdentifierStartUnicodeEscapeStart;
                else if (m_current == '0') {
                    record8(m_current);
                    m_state = InNum0;
                } else if (isDecimalDigit(m_current)) {
                    record8(m_current);
                    m_state = InNum;
                } else if (m_current == '.' && isDecimalDigit(m_next1)) {
                    record8(m_current);
                    m_state = InDecimal;
                    // <!-- marks the beginning of a line comment (for www usage)
                } else if (m_current == '<' && m_next1 == '!' && m_next2 == '-' && m_next3 == '-') {
                    shift(3);
                    m_state = InSingleLineComment;
                    // same for -->
                } else if (m_atLineStart && m_current == '-' && m_next1 == '-' &&  m_next2 == '>') {
                    shift(2);
                    m_state = InSingleLineComment;
                } else {
                    token = matchPunctuator(lvalp->intValue, m_current, m_next1, m_next2, m_next3);
                    if (token != -1)
                        setDone(Other);
                    else
                        setDone(Bad);
                }
                break;
            case InString:
                if (m_current == stringType) {
                    shift(1);
                    setDone(String);
                } else if (isLineTerminator() || m_current == -1)
                    setDone(Bad);
                else if (m_current == '\\')
                    m_state = InEscapeSequence;
                else
                    record16(m_current);
                break;
            // Escape Sequences inside of strings
            case InEscapeSequence:
                if (isOctalDigit(m_current)) {
                    if (m_current >= '0' && m_current <= '3' &&
                        isOctalDigit(m_next1) && isOctalDigit(m_next2)) {
                        record16(convertOctal(m_current, m_next1, m_next2));
                        shift(2);
                        m_state = InString;
                    } else if (isOctalDigit(m_current) && isOctalDigit(m_next1)) {
                        record16(convertOctal('0', m_current, m_next1));
                        shift(1);
                        m_state = InString;
                    } else if (isOctalDigit(m_current)) {
                        record16(convertOctal('0', '0', m_current));
                        m_state = InString;
                    } else
                        setDone(Bad);
                } else if (m_current == 'x')
                    m_state = InHexEscape;
                else if (m_current == 'u')
                    m_state = InUnicodeEscape;
                else if (isLineTerminator()) {
                    nextLine();
                    m_state = InString;
                } else {
                    record16(singleEscape(static_cast<unsigned short>(m_current)));
                    m_state = InString;
                }
                break;
            case InHexEscape:
                if (isHexDigit(m_current) && isHexDigit(m_next1)) {
                    m_state = InString;
                    record16(convertHex(m_current, m_next1));
                    shift(1);
                } else if (m_current == stringType) {
                    record16('x');
                    shift(1);
                    setDone(String);
                } else {
                    record16('x');
                    record16(m_current);
                    m_state = InString;
                }
                break;
            case InUnicodeEscape:
                if (isHexDigit(m_current) && isHexDigit(m_next1) && isHexDigit(m_next2) && isHexDigit(m_next3)) {
                    record16(convertUnicode(m_current, m_next1, m_next2, m_next3));
                    shift(3);
                    m_state = InString;
                } else if (m_current == stringType) {
                    record16('u');
                    shift(1);
                    setDone(String);
                } else
                    setDone(Bad);
                break;
            case InSingleLineComment:
                if (isLineTerminator()) {
                    nextLine();
                    m_terminator = true;
                    if (m_restrKeyword) {
                        token = ';';
                        setDone(Other);
                    } else
                        m_state = Start;
                } else if (m_current == -1)
                    setDone(Eof);
                break;
            case InMultiLineComment:
                if (m_current == -1)
                    setDone(Bad);
                else if (isLineTerminator())
                    nextLine();
                else if (m_current == '*' && m_next1 == '/') {
                    m_state = Start;
                    shift(1);
                }
                break;
            case InIdentifierOrKeyword:
            case InIdentifier:
                if (isIdentPart(m_current))
                    record16(m_current);
                else if (m_current == '\\')
                    m_state = InIdentifierPartUnicodeEscapeStart;
                else
                    setDone(m_state == InIdentifierOrKeyword ? IdentifierOrKeyword : Identifier);
                break;
            case InNum0:
                if (m_current == 'x' || m_current == 'X') {
                    record8(m_current);
                    m_state = InHex;
                } else if (m_current == '.') {
                    record8(m_current);
                    m_state = InDecimal;
                } else if (m_current == 'e' || m_current == 'E') {
                    record8(m_current);
                    m_state = InExponentIndicator;
                } else if (isOctalDigit(m_current)) {
                    record8(m_current);
                    m_state = InOctal;
                } else if (isDecimalDigit(m_current)) {
                    record8(m_current);
                    m_state = InDecimal;
                } else
                    setDone(Number);
                break;
            case InHex:
                if (isHexDigit(m_current))
                    record8(m_current);
                else
                    setDone(Hex);
                break;
            case InOctal:
                if (isOctalDigit(m_current))
                    record8(m_current);
                else if (isDecimalDigit(m_current)) {
                    record8(m_current);
                    m_state = InDecimal;
                } else
                    setDone(Octal);
                break;
            case InNum:
                if (isDecimalDigit(m_current))
                    record8(m_current);
                else if (m_current == '.') {
                    record8(m_current);
                    m_state = InDecimal;
                } else if (m_current == 'e' || m_current == 'E') {
                    record8(m_current);
                    m_state = InExponentIndicator;
                } else
                    setDone(Number);
                break;
            case InDecimal:
                if (isDecimalDigit(m_current))
                    record8(m_current);
                else if (m_current == 'e' || m_current == 'E') {
                    record8(m_current);
                    m_state = InExponentIndicator;
                } else
                    setDone(Number);
                break;
            case InExponentIndicator:
                if (m_current == '+' || m_current == '-')
                    record8(m_current);
                else if (isDecimalDigit(m_current)) {
                    record8(m_current);
                    m_state = InExponent;
                } else
                    setDone(Bad);
                break;
            case InExponent:
                if (isDecimalDigit(m_current))
                    record8(m_current);
                else
                    setDone(Number);
                break;
            case InIdentifierStartUnicodeEscapeStart:
                if (m_current == 'u')
                    m_state = InIdentifierStartUnicodeEscape;
                else
                    setDone(Bad);
                break;
            case InIdentifierPartUnicodeEscapeStart:
                if (m_current == 'u')
                    m_state = InIdentifierPartUnicodeEscape;
                else
                    setDone(Bad);
                break;
            case InIdentifierStartUnicodeEscape:
                if (!isHexDigit(m_current) || !isHexDigit(m_next1) || !isHexDigit(m_next2) || !isHexDigit(m_next3)) {
                    setDone(Bad);
                    break;
                }
                token = convertUnicode(m_current, m_next1, m_next2, m_next3);
                shift(3);
                if (!isIdentStart(token)) {
                    setDone(Bad);
                    break;
                }
                record16(token);
                m_state = InIdentifier;
                break;
            case InIdentifierPartUnicodeEscape:
                if (!isHexDigit(m_current) || !isHexDigit(m_next1) || !isHexDigit(m_next2) || !isHexDigit(m_next3)) {
                    setDone(Bad);
                    break;
                }
                token = convertUnicode(m_current, m_next1, m_next2, m_next3);
                shift(3);
                if (!isIdentPart(token)) {
                    setDone(Bad);
                    break;
                }
                record16(token);
                m_state = InIdentifier;
                break;
            default:
                ASSERT(!"Unhandled state in switch statement");
        }

        // move on to the next character
        if (!m_done)
            shift(1);
        if (m_state != Start && m_state != InSingleLineComment)
            m_atLineStart = false;
    }

    // no identifiers allowed directly after numeric literal, e.g. "3in" is bad
    if ((m_state == Number || m_state == Octal || m_state == Hex) && isIdentStart(m_current))
        m_state = Bad;

    // terminate string
    m_buffer8.append('\0');

#ifdef KJS_DEBUG_LEX
    fprintf(stderr, "line: %d ", lineNo());
    fprintf(stderr, "yytext (%x): ", m_buffer8[0]);
    fprintf(stderr, "%s ", m_buffer8.data());
#endif

    double dval = 0;
    if (m_state == Number)
        dval = WTF::strtod(m_buffer8.data(), 0L);
    else if (m_state == Hex) { // scan hex numbers
        const char* p = m_buffer8.data() + 2;
        while (char c = *p++) {
            dval *= 16;
            dval += convertHex(c);
        }

        if (dval >= mantissaOverflowLowerBound)
            dval = parseIntOverflow(m_buffer8.data() + 2, p - (m_buffer8.data() + 3), 16);

        m_state = Number;
    } else if (m_state == Octal) {   // scan octal number
        const char* p = m_buffer8.data() + 1;
        while (char c = *p++) {
            dval *= 8;
            dval += c - '0';
        }

        if (dval >= mantissaOverflowLowerBound)
            dval = parseIntOverflow(m_buffer8.data() + 1, p - (m_buffer8.data() + 2), 8);

        m_state = Number;
    }

#ifdef KJS_DEBUG_LEX
    switch (m_state) {
        case Eof:
            printf("(EOF)\n");
            break;
        case Other:
            printf("(Other)\n");
            break;
        case Identifier:
            printf("(Identifier)/(Keyword)\n");
            break;
        case String:
            printf("(String)\n");
            break;
        case Number:
            printf("(Number)\n");
            break;
        default:
            printf("(unknown)");
    }
#endif

    if (m_state != Identifier)
        m_eatNextIdentifier = false;

    m_restrKeyword = false;
    m_delimited = false;
    llocp->first_line = yylineno;
    llocp->last_line = yylineno;
    llocp->first_column = startOffset;
    llocp->last_column = m_currentOffset;
    switch (m_state) {
        case Eof:
            token = 0;
            break;
        case Other:
            if (token == '}' || token == ';')
                m_delimited = true;
            break;
        case Identifier:
            // Apply anonymous-function hack below (eat the identifier).
            if (m_eatNextIdentifier) {
                m_eatNextIdentifier = false;
                token = lex(lvalp, llocp);
                break;
            }
            lvalp->ident = makeIdentifier(m_buffer16);
            token = IDENT;
            break;
        case IdentifierOrKeyword: {
            lvalp->ident = makeIdentifier(m_buffer16);
            const HashEntry* entry = m_mainTable.entry(m_globalData, *lvalp->ident);
            if (!entry) {
                // Lookup for keyword failed, means this is an identifier.
                token = IDENT;
                break;
            }
            token = entry->lexerValue();
            // Hack for "f = function somename() { ... }"; too hard to get into the grammar.
            m_eatNextIdentifier = token == FUNCTION && m_lastToken == '=';
            if (token == CONTINUE || token == BREAK || token == RETURN || token == THROW)
                m_restrKeyword = true;
            break;
        }
        case String:
            // Atomize constant strings in case they're later used in property lookup.
            lvalp->ident = makeIdentifier(m_buffer16);
            token = STRING;
            break;
        case Number:
            lvalp->doubleValue = dval;
            token = NUMBER;
            break;
        case Bad:
#ifdef KJS_DEBUG_LEX
            fprintf(stderr, "yylex: ERROR.\n");
#endif
            m_error = true;
            return -1;
        default:
            ASSERT(!"unhandled numeration value in switch");
            m_error = true;
            return -1;
    }
    m_lastToken = token;
    return token;
}

bool Lexer::isWhiteSpace() const
{
    return m_current == '\t' || m_current == 0x0b || m_current == 0x0c || isSeparatorSpace(m_current);
}

bool Lexer::isLineTerminator()
{
    bool cr = (m_current == '\r');
    bool lf = (m_current == '\n');
    if (cr)
        m_skipLF = true;
    else if (lf)
        m_skipCR = true;
    return cr || lf || m_current == 0x2028 || m_current == 0x2029;
}

bool Lexer::isIdentStart(int c)
{
    return isASCIIAlpha(c) || c == '$' || c == '_' || (!isASCII(c) && (category(c) & (Letter_Uppercase | Letter_Lowercase | Letter_Titlecase | Letter_Modifier | Letter_Other)));
}

bool Lexer::isIdentPart(int c)
{
    return isASCIIAlphanumeric(c) || c == '$' || c == '_' || (!isASCII(c) && (category(c) & (Letter_Uppercase | Letter_Lowercase | Letter_Titlecase | Letter_Modifier | Letter_Other
                            | Mark_NonSpacing | Mark_SpacingCombining | Number_DecimalDigit | Punctuation_Connector)));
}

static bool isDecimalDigit(int c)
{
    return isASCIIDigit(c);
}

bool Lexer::isHexDigit(int c)
{
    return isASCIIHexDigit(c); 
}

bool Lexer::isOctalDigit(int c)
{
    return isASCIIOctalDigit(c);
}

int Lexer::matchPunctuator(int& charPos, int c1, int c2, int c3, int c4)
{
    if (c1 == '>' && c2 == '>' && c3 == '>' && c4 == '=') {
        shift(4);
        return URSHIFTEQUAL;
    }
    if (c1 == '=' && c2 == '=' && c3 == '=') {
        shift(3);
        return STREQ;
    }
    if (c1 == '!' && c2 == '=' && c3 == '=') {
        shift(3);
        return STRNEQ;
    }
    if (c1 == '>' && c2 == '>' && c3 == '>') {
        shift(3);
        return URSHIFT;
    }
    if (c1 == '<' && c2 == '<' && c3 == '=') {
        shift(3);
        return LSHIFTEQUAL;
    }
    if (c1 == '>' && c2 == '>' && c3 == '=') {
        shift(3);
        return RSHIFTEQUAL;
    }
    if (c1 == '<' && c2 == '=') {
        shift(2);
        return LE;
    }
    if (c1 == '>' && c2 == '=') {
        shift(2);
        return GE;
    }
    if (c1 == '!' && c2 == '=') {
        shift(2);
        return NE;
    }
    if (c1 == '+' && c2 == '+') {
        shift(2);
        if (m_terminator)
            return AUTOPLUSPLUS;
        return PLUSPLUS;
    }
    if (c1 == '-' && c2 == '-') {
        shift(2);
        if (m_terminator)
            return AUTOMINUSMINUS;
        return MINUSMINUS;
    }
    if (c1 == '=' && c2 == '=') {
        shift(2);
        return EQEQ;
    }
    if (c1 == '+' && c2 == '=') {
        shift(2);
        return PLUSEQUAL;
    }
    if (c1 == '-' && c2 == '=') {
        shift(2);
        return MINUSEQUAL;
    }
    if (c1 == '*' && c2 == '=') {
        shift(2);
        return MULTEQUAL;
    }
    if (c1 == '/' && c2 == '=') {
        shift(2);
        return DIVEQUAL;
    }
    if (c1 == '&' && c2 == '=') {
        shift(2);
        return ANDEQUAL;
    }
    if (c1 == '^' && c2 == '=') {
        shift(2);
        return XOREQUAL;
    }
    if (c1 == '%' && c2 == '=') {
        shift(2);
        return MODEQUAL;
    }
    if (c1 == '|' && c2 == '=') {
        shift(2);
        return OREQUAL;
    }
    if (c1 == '<' && c2 == '<') {
        shift(2);
        return LSHIFT;
    }
    if (c1 == '>' && c2 == '>') {
        shift(2);
        return RSHIFT;
    }
    if (c1 == '&' && c2 == '&') {
        shift(2);
        return AND;
    }
    if (c1 == '|' && c2 == '|') {
        shift(2);
        return OR;
    }

    switch (c1) {
        case '=':
        case '>':
        case '<':
        case ',':
        case '!':
        case '~':
        case '?':
        case ':':
        case '.':
        case '+':
        case '-':
        case '*':
        case '/':
        case '&':
        case '|':
        case '^':
        case '%':
        case '(':
        case ')':
        case '[':
        case ']':
        case ';':
            shift(1);
            return static_cast<int>(c1);
        case '{':
            charPos = m_position - 4;
            shift(1);
            return OPENBRACE;
        case '}':
            charPos = m_position - 4;
            shift(1);
            return CLOSEBRACE;
        default:
            return -1;
    }
}

unsigned short Lexer::singleEscape(unsigned short c)
{
    switch (c) {
        case 'b':
            return 0x08;
        case 't':
            return 0x09;
        case 'n':
            return 0x0A;
        case 'v':
            return 0x0B;
        case 'f':
            return 0x0C;
        case 'r':
            return 0x0D;
        case '"':
            return 0x22;
        case '\'':
            return 0x27;
        case '\\':
            return 0x5C;
        default:
            return c;
    }
}

unsigned short Lexer::convertOctal(int c1, int c2, int c3)
{
    return static_cast<unsigned short>((c1 - '0') * 64 + (c2 - '0') * 8 + c3 - '0');
}

unsigned char Lexer::convertHex(int c)
{
    if (c >= '0' && c <= '9')
        return static_cast<unsigned char>(c - '0');
    if (c >= 'a' && c <= 'f')
        return static_cast<unsigned char>(c - 'a' + 10);
    return static_cast<unsigned char>(c - 'A' + 10);
}

unsigned char Lexer::convertHex(int c1, int c2)
{
    return ((convertHex(c1) << 4) + convertHex(c2));
}

UChar Lexer::convertUnicode(int c1, int c2, int c3, int c4)
{
    unsigned char highByte = (convertHex(c1) << 4) + convertHex(c2);
    unsigned char lowByte = (convertHex(c3) << 4) + convertHex(c4);
    return (highByte << 8 | lowByte);
}

void Lexer::record8(int c)
{
    ASSERT(c >= 0);
    ASSERT(c <= 0xff);
    m_buffer8.append(static_cast<char>(c));
}

void Lexer::record16(int c)
{
    ASSERT(c >= 0);
    ASSERT(c <= USHRT_MAX);
    record16(UChar(static_cast<unsigned short>(c)));
}

void Lexer::record16(UChar c)
{
    m_buffer16.append(c);
}

bool Lexer::scanRegExp()
{
    m_buffer16.clear();
    bool lastWasEscape = false;
    bool inBrackets = false;

    while (1) {
        if (isLineTerminator() || m_current == -1)
            return false;
        else if (m_current != '/' || lastWasEscape == true || inBrackets == true) {
            // keep track of '[' and ']'
            if (!lastWasEscape) {
                if ( m_current == '[' && !inBrackets )
                    inBrackets = true;
                if ( m_current == ']' && inBrackets )
                    inBrackets = false;
            }
            record16(m_current);
            lastWasEscape =
            !lastWasEscape && (m_current == '\\');
        } else { // end of regexp
            m_pattern = UString(m_buffer16);
            m_buffer16.clear();
            shift(1);
            break;
        }
        shift(1);
    }

    while (isIdentPart(m_current)) {
        record16(m_current);
        shift(1);
    }
    m_flags = UString(m_buffer16);

    return true;
}

void Lexer::clear()
{
    m_identifiers.resize(0);

    Vector<char> newBuffer8;
    newBuffer8.reserveCapacity(initialReadBufferCapacity);
    m_buffer8.swap(newBuffer8);

    Vector<UChar> newBuffer16;
    newBuffer16.reserveCapacity(initialReadBufferCapacity);
    m_buffer16.swap(newBuffer16);

    m_pattern = 0;
    m_flags = 0;
}

} // namespace JSC
