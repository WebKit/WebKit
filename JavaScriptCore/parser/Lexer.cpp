/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All Rights Reserved.
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

#include "JSFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "NodeInfo.h"
#include "Nodes.h"
#include "dtoa.h"
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <wtf/Assertions.h>

using namespace WTF;
using namespace Unicode;

// We can't specify the namespace in yacc's C output, so do it here instead.
using namespace JSC;

#ifndef KDE_USE_FINAL
#include "Grammar.h"
#endif

#include "Lookup.h"
#include "Lexer.lut.h"

// A bridge for yacc from the C world to the C++ world.
int jscyylex(void* lvalp, void* llocp, void* globalData)
{
    return static_cast<JSGlobalData*>(globalData)->lexer->lex(lvalp, llocp);
}

namespace JSC {

static const UChar byteOrderMark = 0xFEFF;

// Values for m_skipLineEnd.
static const unsigned char SkipLFShift = 0;
static const unsigned char SkipCRShift = 1;
static const unsigned char SkipLF = 1 << SkipLFShift;
static const unsigned char SkipCR = 1 << SkipCRShift;

Lexer::Lexer(JSGlobalData* globalData)
    : m_isReparsing(false)
    , m_globalData(globalData)
    , m_keywordTable(JSC::mainTable)
{
    m_buffer8.reserveInitialCapacity(initialReadBufferCapacity);
    m_buffer16.reserveInitialCapacity(initialReadBufferCapacity);
}

Lexer::~Lexer()
{
    m_keywordTable.deleteTable();
}

inline int Lexer::currentOffset() const
{
    return m_code - 4 - m_codeStart;
}

ALWAYS_INLINE void Lexer::shift1()
{
    m_current = m_next1;
    m_next1 = m_next2;
    m_next2 = m_next3;
    if (LIKELY(m_code < m_codeEnd))
        m_next3 = m_code[0];
    else
        m_next3 = -1;

    ++m_code;
}

ALWAYS_INLINE void Lexer::shift2()
{
    m_current = m_next2;
    m_next1 = m_next3;
    if (LIKELY(m_code + 1 < m_codeEnd)) {
        m_next2 = m_code[0];
        m_next3 = m_code[1];
    } else {
        m_next2 = m_code < m_codeEnd ? m_code[0] : -1;
        m_next3 = -1;
    }

    m_code += 2;
}

ALWAYS_INLINE void Lexer::shift3()
{
    m_current = m_next3;
    if (LIKELY(m_code + 2 < m_codeEnd)) {
        m_next1 = m_code[0];
        m_next2 = m_code[1];
        m_next3 = m_code[2];
    } else {
        m_next1 = m_code < m_codeEnd ? m_code[0] : -1;
        m_next2 = m_code + 1 < m_codeEnd ? m_code[1] : -1;
        m_next3 = -1;
    }

    m_code += 3;
}

ALWAYS_INLINE void Lexer::shift4()
{
    if (LIKELY(m_code + 3 < m_codeEnd)) {
        m_current = m_code[0];
        m_next1 = m_code[1];
        m_next2 = m_code[2];
        m_next3 = m_code[3];
    } else {
        m_current = m_code < m_codeEnd ? m_code[0] : -1;
        m_next1 = m_code + 1 < m_codeEnd ? m_code[1] : -1;
        m_next2 = m_code + 2 < m_codeEnd ? m_code[2] : -1;
        m_next3 = -1;
    }

    m_code += 4;
}

void Lexer::setCode(const SourceCode& source)
{
    m_lineNumber = source.firstLine();
    m_delimited = false;
    m_lastToken = -1;

    const UChar* data = source.provider()->data();

    m_source = &source;
    m_codeStart = data;
    m_code = data + source.startOffset();
    m_codeEnd = data + source.endOffset();
    m_skipLineEnd = 0;
    m_error = false;
    m_atLineStart = true;

    // ECMA-262 calls for stripping all Cf characters, but we only strip BOM characters.
    // See <https://bugs.webkit.org/show_bug.cgi?id=4931> for details.
    if (source.provider()->hasBOMs()) {
        for (const UChar* p = m_codeStart; p < m_codeEnd; ++p) {
            if (UNLIKELY(*p == byteOrderMark)) {
                copyCodeWithoutBOMs();
                break;
            }
        }
    }

    // Read the first characters into the 4-character buffer.
    shift4();
    ASSERT(currentOffset() == source.startOffset());
}

void Lexer::copyCodeWithoutBOMs()
{
    // Note: In this case, the character offset data for debugging will be incorrect.
    // If it's important to correctly debug code with extraneous BOMs, then the caller
    // should strip the BOMs when creating the SourceProvider object and do its own
    // mapping of offsets within the stripped text to original text offset.

    m_codeWithoutBOMs.reserveCapacity(m_codeEnd - m_code);
    for (const UChar* p = m_code; p < m_codeEnd; ++p) {
        UChar c = *p;
        if (c != byteOrderMark)
            m_codeWithoutBOMs.append(c);
    }
    ptrdiff_t startDelta = m_codeStart - m_code;
    m_code = m_codeWithoutBOMs.data();
    m_codeStart = m_code + startDelta;
    m_codeEnd = m_codeWithoutBOMs.data() + m_codeWithoutBOMs.size();
}

// called on each new line
void Lexer::nextLine()
{
    ++m_lineNumber;
    m_atLineStart = true;
}

void Lexer::setDone(State s)
{
    m_state = s;
    m_done = true;
}

ALWAYS_INLINE JSC::Identifier* Lexer::makeIdentifier(const Vector<UChar>& buffer)
{
    m_identifiers.append(JSC::Identifier(m_globalData, buffer.data(), buffer.size()));
    return &m_identifiers.last();
}

ALWAYS_INLINE int Lexer::matchPunctuator(int& charPos)
{
    switch (m_current) {
        case '>':
            if (m_next1 == '>' && m_next2 == '>') {
                if (m_next3 == '=') {
                    shift4();
                    return URSHIFTEQUAL;
                }
                shift3();
                return URSHIFT;
            }
            if (m_next1 == '>') {
                if (m_next2 == '=') {
                    shift3();
                    return RSHIFTEQUAL;
                }
                shift2();
                return RSHIFT;
            }
            if (m_next1 == '=') {
                shift2();
                return GE;
            }
            shift1();
            return '>';
        case '=':
            if (m_next1 == '=') {
                if (m_next2 == '=') {
                    shift3();
                    return STREQ;
                }
                shift2();
                return EQEQ;
            }
            shift1();
            return '=';
        case '!':
            if (m_next1 == '=') {
                if (m_next2 == '=') {
                    shift3();
                    return STRNEQ;
                }
                shift2();
                return NE;
            }
            shift1();
            return '!';
        case '<':
            if (m_next1 == '<') {
                if (m_next2 == '=') {
                    shift3();
                    return LSHIFTEQUAL;
                }
                shift2();
                return LSHIFT;
            }
            if (m_next1 == '=') {
                shift2();
                return LE;
            }
            shift1();
            return '<';
        case '+':
            if (m_next1 == '+') {
                shift2();
                if (m_terminator)
                    return AUTOPLUSPLUS;
                return PLUSPLUS;
            }
            if (m_next1 == '=') {
                shift2();
                return PLUSEQUAL;
            }
            shift1();
            return '+';
        case '-':
            if (m_next1 == '-') {
                shift2();
                if (m_terminator)
                    return AUTOMINUSMINUS;
                return MINUSMINUS;
            }
            if (m_next1 == '=') {
                shift2();
                return MINUSEQUAL;
            }
            shift1();
            return '-';
        case '*':
            if (m_next1 == '=') {
                shift2();
                return MULTEQUAL;
            }
            shift1();
            return '*';
        case '/':
            if (m_next1 == '=') {
                shift2();
                return DIVEQUAL;
            }
            shift1();
            return '/';
        case '&':
            if (m_next1 == '&') {
                shift2();
                return AND;
            }
            if (m_next1 == '=') {
                shift2();
                return ANDEQUAL;
            }
            shift1();
            return '&';
        case '^':
            if (m_next1 == '=') {
                shift2();
                return XOREQUAL;
            }
            shift1();
            return '^';
        case '%':
            if (m_next1 == '=') {
                shift2();
                return MODEQUAL;
            }
            shift1();
            return '%';
        case '|':
            if (m_next1 == '=') {
                shift2();
                return OREQUAL;
            }
            if (m_next1 == '|') {
                shift2();
                return OR;
            }
            shift1();
            return '|';
        case ',':
            shift1();
            return ',';
        case '~':
            shift1();
            return '~';
        case '?':
            shift1();
            return '?';
        case ':':
            shift1();
            return ':';
        case '.':
            shift1();
            return '.';
        case '(':
            shift1();
            return '(';
        case ')':
            shift1();
            return ')';
        case '[':
            shift1();
            return '[';
        case ']':
            shift1();
            return ']';
        case ';':
            shift1();
            return ';';
        case '{':
            charPos = currentOffset();
            shift1();
            return OPENBRACE;
        case '}':
            charPos = currentOffset();
            shift1();
            return CLOSEBRACE;
    }

    return -1;
}

ALWAYS_INLINE bool Lexer::isLineTerminator()
{
    bool cr = m_current == '\r';
    bool lf = m_current == '\n';
    m_skipLineEnd |= (cr << SkipLFShift) | (lf << SkipCRShift);
    return cr | lf | ((m_current & ~1) == 0x2028);
}

inline bool Lexer::lastTokenWasRestrKeyword() const
{
    return m_lastToken == CONTINUE || m_lastToken == BREAK || m_lastToken == RETURN || m_lastToken == THROW;
}

static NEVER_INLINE bool isNonASCIIIdentStart(int c)
{
    return category(c) & (Letter_Uppercase | Letter_Lowercase | Letter_Titlecase | Letter_Modifier | Letter_Other);
}

static inline bool isIdentStart(int c)
{
    return isASCII(c) ? isASCIIAlpha(c) || c == '$' || c == '_' : isNonASCIIIdentStart(c);
}

static NEVER_INLINE bool isNonASCIIIdentPart(int c)
{
    return category(c) & (Letter_Uppercase | Letter_Lowercase | Letter_Titlecase | Letter_Modifier | Letter_Other
        | Mark_NonSpacing | Mark_SpacingCombining | Number_DecimalDigit | Punctuation_Connector);
}

static inline bool isIdentPart(int c)
{
    return isASCII(c) ? isASCIIAlphanumeric(c) || c == '$' || c == '_' : isNonASCIIIdentPart(c);
}

static int singleEscape(int c)
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

static inline int convertOctal(int c1, int c2, int c3)
{
    return (c1 - '0') * 64 + (c2 - '0') * 8 + c3 - '0';
}

inline void Lexer::record8(int c)
{
    ASSERT(c >= 0);
    ASSERT(c <= 0xFF);
    m_buffer8.append(static_cast<char>(c));
}

inline void Lexer::record16(int c)
{
    ASSERT(c >= 0);
    ASSERT(c <= USHRT_MAX);
    record16(UChar(static_cast<unsigned short>(c)));
}

inline void Lexer::record16(UChar c)
{
    m_buffer16.append(c);
}

int Lexer::lex(void* p1, void* p2)
{
    YYSTYPE* lvalp = static_cast<YYSTYPE*>(p1);
    YYLTYPE* llocp = static_cast<YYLTYPE*>(p2);
    int token = 0;
    m_state = Start;
    int stringType = 0; // either single or double quotes
    m_buffer8.resize(0);
    m_buffer16.resize(0);
    m_done = false;
    m_terminator = false;
    m_skipLineEnd = 0;

    int startOffset = currentOffset();

    while (true) {
        if (m_skipLineEnd) {
            if (m_current != '\n') // found \r but not \n afterwards
                m_skipLineEnd &= ~SkipLF;
            if (m_current != '\r') // found \n but not \r afterwards
                m_skipLineEnd &= ~SkipCR;
            if (m_skipLineEnd) { // found \r\n or \n\r -> eat the second one
                m_skipLineEnd = 0;
                shift1();
            }
        }
        switch (m_state) {
            case Start:
                startOffset = currentOffset();
                if (isWhiteSpace(m_current)) {
                    // do nothing
                } else if (m_current == '/' && m_next1 == '/') {
                    shift1();
                    m_state = InSingleLineComment;
                } else if (m_current == '/' && m_next1 == '*') {
                    shift1();
                    m_state = InMultiLineComment;
                } else if (m_current == -1) {
                    if (!m_terminator && !m_delimited && !m_isReparsing) {
                        // automatic semicolon insertion if program incomplete
                        token = ';';
                        setDone(Other);
                    } else
                        setDone(Eof);
                } else if (isLineTerminator()) {
                    nextLine();
                    m_terminator = true;
                    if (lastTokenWasRestrKeyword()) {
                        token = ';';
                        setDone(Other);
                    }
                } else if (m_current == '"' || m_current == '\'') {
                    m_state = InString;
                    stringType = m_current;
                } else if (isIdentStart(m_current)) {
                    record16(m_current);
                    m_state = InIdentifierOrKeyword;
                } else if (m_current == '\\')
                    m_state = InIdentifierStartUnicodeEscapeStart;
                else if (m_current == '0') {
                    record8(m_current);
                    m_state = InNum0;
                } else if (isASCIIDigit(m_current)) {
                    record8(m_current);
                    m_state = InNum;
                } else if (m_current == '.' && isASCIIDigit(m_next1)) {
                    record8(m_current);
                    m_state = InDecimal;
                } else if (m_current == '<' && m_next1 == '!' && m_next2 == '-' && m_next3 == '-') {
                    // <!-- marks the beginning of a line comment (for www usage)
                    shift3();
                    m_state = InSingleLineComment;
                } else if (m_atLineStart && m_current == '-' && m_next1 == '-' && m_next2 == '>') {
                    // same for -->
                    shift2();
                    m_state = InSingleLineComment;
                } else {
                    token = matchPunctuator(lvalp->intValue);
                    if (token != -1)
                        setDone(Other);
                    else
                        setDone(Bad);
                }
                goto stillAtLineStart;
            case InString:
                if (m_current == stringType) {
                    shift1();
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
                if (isASCIIOctalDigit(m_current)) {
                    if (m_current >= '0' && m_current <= '3' && isASCIIOctalDigit(m_next1) && isASCIIOctalDigit(m_next2)) {
                        record16(convertOctal(m_current, m_next1, m_next2));
                        shift2();
                        m_state = InString;
                    } else if (isASCIIOctalDigit(m_current) && isASCIIOctalDigit(m_next1)) {
                        record16(convertOctal('0', m_current, m_next1));
                        shift1();
                        m_state = InString;
                    } else if (isASCIIOctalDigit(m_current)) {
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
                    record16(singleEscape(m_current));
                    m_state = InString;
                }
                break;
            case InHexEscape:
                if (isASCIIHexDigit(m_current) && isASCIIHexDigit(m_next1)) {
                    m_state = InString;
                    record16(convertHex(m_current, m_next1));
                    shift1();
                } else if (m_current == stringType) {
                    record16('x');
                    shift1();
                    setDone(String);
                } else {
                    record16('x');
                    record16(m_current);
                    m_state = InString;
                }
                break;
            case InUnicodeEscape:
                if (isASCIIHexDigit(m_current) && isASCIIHexDigit(m_next1) && isASCIIHexDigit(m_next2) && isASCIIHexDigit(m_next3)) {
                    record16(convertUnicode(m_current, m_next1, m_next2, m_next3));
                    shift3();
                    m_state = InString;
                } else if (m_current == stringType) {
                    record16('u');
                    shift1();
                    setDone(String);
                } else
                    setDone(Bad);
                break;
            case InSingleLineComment:
                if (isLineTerminator()) {
                    nextLine();
                    m_terminator = true;
                    if (lastTokenWasRestrKeyword()) {
                        token = ';';
                        setDone(Other);
                    } else
                        m_state = Start;
                } else if (m_current == -1)
                    setDone(Eof);
                goto stillAtLineStart;
            case InMultiLineComment:
                if (isLineTerminator())
                    nextLine();
                else if (m_current == '*' && m_next1 == '/') {
                    m_state = Start;
                    shift1();
                } else if (m_current == -1)
                    setDone(Bad);
                break;
            case InIdentifierOrKeyword:
                if (isIdentPart(m_current)) {
                    record16(m_current);
                    while (isIdentPart(m_next1)) {
                        shift1();
                        record16(m_current);
                    }
                } else if (m_current == '\\')
                    m_state = InIdentifierPartUnicodeEscapeStart;
                else
                    setDone(IdentifierOrKeyword);
                break;
            case InIdentifier:
                if (isIdentPart(m_current)) {
                    record16(m_current);
                    while (isIdentPart(m_next1)) {
                        shift1();
                        record16(m_current);
                    }
                } else if (m_current == '\\')
                    m_state = InIdentifierPartUnicodeEscapeStart;
                else
                    setDone(Identifier);
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
                } else if (isASCIIOctalDigit(m_current)) {
                    record8(m_current);
                    m_state = InOctal;
                } else if (isASCIIDigit(m_current)) {
                    record8(m_current);
                    m_state = InDecimal;
                } else
                    setDone(Number);
                break;
            case InHex:
                if (isASCIIHexDigit(m_current)) {
                    record8(m_current);
                    while (isASCIIHexDigit(m_next1)) {
                        shift1();
                        record8(m_current);
                    }
                } else
                    setDone(Hex);
                break;
            case InOctal:
                if (isASCIIOctalDigit(m_current)) {
                    record8(m_current);
                    while (isASCIIOctalDigit(m_next1)) {
                        shift1();
                        record8(m_current);
                    }
                } else if (isASCIIDigit(m_current)) {
                    record8(m_current);
                    m_state = InDecimal;
                } else
                    setDone(Octal);
                break;
            case InNum:
                if (isASCIIDigit(m_current)) {
                    record8(m_current);
                    while (isASCIIDigit(m_next1)) {
                        shift1();
                        record8(m_current);
                    }
                } else if (m_current == '.') {
                    record8(m_current);
                    m_state = InDecimal;
                } else if (m_current == 'e' || m_current == 'E') {
                    record8(m_current);
                    m_state = InExponentIndicator;
                } else
                    setDone(Number);
                break;
            case InDecimal:
                if (isASCIIDigit(m_current)) {
                    record8(m_current);
                    while (isASCIIDigit(m_next1)) {
                        shift1();
                        record8(m_current);
                    }
                } else if (m_current == 'e' || m_current == 'E') {
                    record8(m_current);
                    m_state = InExponentIndicator;
                } else
                    setDone(Number);
                break;
            case InExponentIndicator:
                if (m_current == '+' || m_current == '-')
                    record8(m_current);
                else if (isASCIIDigit(m_current)) {
                    record8(m_current);
                    m_state = InExponent;
                } else
                    setDone(Bad);
                break;
            case InExponent:
                if (isASCIIDigit(m_current))
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
                if (!isASCIIHexDigit(m_current) || !isASCIIHexDigit(m_next1) || !isASCIIHexDigit(m_next2) || !isASCIIHexDigit(m_next3)) {
                    setDone(Bad);
                    break;
                }
                token = convertUnicode(m_current, m_next1, m_next2, m_next3);
                shift3();
                if (!isIdentStart(token)) {
                    setDone(Bad);
                    break;
                }
                record16(token);
                m_state = InIdentifier;
                break;
            case InIdentifierPartUnicodeEscape:
                if (!isASCIIHexDigit(m_current) || !isASCIIHexDigit(m_next1) || !isASCIIHexDigit(m_next2) || !isASCIIHexDigit(m_next3)) {
                    setDone(Bad);
                    break;
                }
                token = convertUnicode(m_current, m_next1, m_next2, m_next3);
                shift3();
                if (!isIdentPart(token)) {
                    setDone(Bad);
                    break;
                }
                record16(token);
                m_state = InIdentifier;
                break;
            default:
                ASSERT_NOT_REACHED();
        }

        m_atLineStart = false;

stillAtLineStart:
        if (m_done)
            break;

        shift1();
    }

    if (m_state == Number || m_state == Octal || m_state == Hex) {
        // No identifiers allowed directly after numeric literal, e.g. "3in" is bad.
        if (isIdentStart(m_current))
            m_state = Bad;
        else {
            // terminate string
            m_buffer8.append('\0');

            if (m_state == Number)
                lvalp->doubleValue = WTF::strtod(m_buffer8.data(), 0L);
            else if (m_state == Hex) { // scan hex numbers
                double dval = 0;

                const char* p = m_buffer8.data() + 2;
                while (char c = *p++) {
                    dval *= 16;
                    dval += toASCIIHexValue(c);
                }

                if (dval >= mantissaOverflowLowerBound)
                    dval = parseIntOverflow(m_buffer8.data() + 2, p - (m_buffer8.data() + 3), 16);

                m_state = Number;
                lvalp->doubleValue = dval;
            } else {   // scan octal number
                double dval = 0;

                const char* p = m_buffer8.data() + 1;
                while (char c = *p++) {
                    dval *= 8;
                    dval += c - '0';
                }

                if (dval >= mantissaOverflowLowerBound)
                    dval = parseIntOverflow(m_buffer8.data() + 1, p - (m_buffer8.data() + 2), 8);

                m_state = Number;
                lvalp->doubleValue = dval;
            }
        }
    }

    m_delimited = false;

    int lineNumber = m_lineNumber;
    llocp->first_line = lineNumber;
    llocp->last_line = lineNumber;
    llocp->first_column = startOffset;
    llocp->last_column = currentOffset();

    switch (m_state) {
        case Eof:
            token = 0;
            break;
        case Other:
            m_delimited = (token == '}') | (token == ';');
            break;
        case Identifier:
            lvalp->ident = makeIdentifier(m_buffer16);
            token = IDENT;
            break;
        case IdentifierOrKeyword: {
            lvalp->ident = makeIdentifier(m_buffer16);
            const HashEntry* entry = m_keywordTable.entry(m_globalData, *lvalp->ident);
            if (!entry) {
                // Lookup for keyword failed, means this is an identifier.
                token = IDENT;
                break;
            }
            token = entry->lexerValue();
            break;
        }
        case String:
            // Atomize constant strings in case they're later used in property lookup.
            lvalp->ident = makeIdentifier(m_buffer16);
            token = STRING;
            break;
        case Number:
            token = NUMBER;
            break;
        default:
            ASSERT_NOT_REACHED();
            // Fall through.
        case Bad:
            m_error = true;
            return -1;
    }

    m_lastToken = token;
    return token;
}

bool Lexer::scanRegExp()
{
    m_buffer16.resize(0);
    bool lastWasEscape = false;
    bool inBrackets = false;

    while (1) {
        if (isLineTerminator() || m_current == -1)
            return false;
        else if (m_current != '/' || lastWasEscape || inBrackets) {
            // keep track of '[' and ']'
            if (!lastWasEscape) {
                if (m_current == '[' && !inBrackets)
                    inBrackets = true;
                if (m_current == ']' && inBrackets)
                    inBrackets = false;
            }
            record16(m_current);
            lastWasEscape = !lastWasEscape && m_current == '\\';
        } else { // end of regexp
            m_pattern = UString(m_buffer16);
            m_buffer16.resize(0);
            shift1();
            break;
        }
        shift1();
    }

    while (isIdentPart(m_current)) {
        record16(m_current);
        shift1();
    }
    m_flags = UString(m_buffer16);

    return true;
}

void Lexer::clear()
{
    m_identifiers.clear();
    m_codeWithoutBOMs.clear();

    Vector<char> newBuffer8;
    newBuffer8.reserveInitialCapacity(initialReadBufferCapacity);
    m_buffer8.swap(newBuffer8);

    Vector<UChar> newBuffer16;
    newBuffer16.reserveInitialCapacity(initialReadBufferCapacity);
    m_buffer16.swap(newBuffer16);

    m_isReparsing = false;

    m_pattern = UString();
    m_flags = UString();
}

SourceCode Lexer::sourceCode(int openBrace, int closeBrace, int firstLine)
{
    if (m_codeWithoutBOMs.isEmpty())
        return SourceCode(m_source->provider(), openBrace, closeBrace + 1, firstLine);

    const UChar* data = m_source->provider()->data();

    ASSERT(openBrace < closeBrace);

    int numBOMsBeforeOpenBrace = 0;
    int numBOMsBetweenBraces = 0;

    int i;
    for (i = m_source->startOffset(); i < openBrace; ++i)
        numBOMsBeforeOpenBrace += data[i] == byteOrderMark;
    for (; i < closeBrace; ++i)
        numBOMsBetweenBraces += data[i] == byteOrderMark;

    return SourceCode(m_source->provider(), openBrace + numBOMsBeforeOpenBrace,
        closeBrace + numBOMsBeforeOpenBrace + numBOMsBetweenBraces + 1, firstLine);
}

} // namespace JSC
