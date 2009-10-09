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

namespace JSC {

static const UChar byteOrderMark = 0xFEFF;

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

inline const UChar* Lexer::currentCharacter() const
{
    return m_code - 4;
}

inline int Lexer::currentOffset() const
{
    return currentCharacter() - m_codeStart;
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

void Lexer::setCode(const SourceCode& source, ParserArena& arena)
{
    m_arena = &arena.identifierArena();

    m_lineNumber = source.firstLine();
    m_delimited = false;
    m_lastToken = -1;

    const UChar* data = source.provider()->data();

    m_source = &source;
    m_codeStart = data;
    m_code = data + source.startOffset();
    m_codeEnd = data + source.endOffset();
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

void Lexer::shiftLineTerminator()
{
    ASSERT(isLineTerminator(m_current));

    // Allow both CRLF and LFCR.
    if (m_current + m_next1 == '\n' + '\r')
        shift2();
    else
        shift1();

    ++m_lineNumber;
}

ALWAYS_INLINE const Identifier* Lexer::makeIdentifier(const UChar* characters, size_t length)
{
    return &m_arena->makeIdentifier(m_globalData, characters, length);
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

static inline int singleEscape(int c)
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
        default:
            return c;
    }
}

inline void Lexer::record8(int c)
{
    ASSERT(c >= 0);
    ASSERT(c <= 0xFF);
    m_buffer8.append(static_cast<char>(c));
}

inline void Lexer::record16(UChar c)
{
    m_buffer16.append(c);
}

inline void Lexer::record16(int c)
{
    ASSERT(c >= 0);
    ASSERT(c <= USHRT_MAX);
    record16(UChar(static_cast<unsigned short>(c)));
}

int Lexer::lex(void* p1, void* p2)
{
    ASSERT(!m_error);
    ASSERT(m_buffer8.isEmpty());
    ASSERT(m_buffer16.isEmpty());

    YYSTYPE* lvalp = static_cast<YYSTYPE*>(p1);
    YYLTYPE* llocp = static_cast<YYLTYPE*>(p2);
    int token = 0;
    m_terminator = false;

start:
    while (isWhiteSpace(m_current))
        shift1();

    int startOffset = currentOffset();

    if (m_current == -1) {
        if (!m_terminator && !m_delimited && !m_isReparsing) {
            // automatic semicolon insertion if program incomplete
            token = ';';
            goto doneSemicolon;
        }
        return 0;
    }

    m_delimited = false;
    switch (m_current) {
        case '>':
            if (m_next1 == '>' && m_next2 == '>') {
                if (m_next3 == '=') {
                    shift4();
                    token = URSHIFTEQUAL;
                    break;
                }
                shift3();
                token = URSHIFT;
                break;
            }
            if (m_next1 == '>') {
                if (m_next2 == '=') {
                    shift3();
                    token = RSHIFTEQUAL;
                    break;
                }
                shift2();
                token = RSHIFT;
                break;
            }
            if (m_next1 == '=') {
                shift2();
                token = GE;
                break;
            }
            shift1();
            token = '>';
            break;
        case '=':
            if (m_next1 == '=') {
                if (m_next2 == '=') {
                    shift3();
                    token = STREQ;
                    break;
                }
                shift2();
                token = EQEQ;
                break;
            }
            shift1();
            token = '=';
            break;
        case '!':
            if (m_next1 == '=') {
                if (m_next2 == '=') {
                    shift3();
                    token = STRNEQ;
                    break;
                }
                shift2();
                token = NE;
                break;
            }
            shift1();
            token = '!';
            break;
        case '<':
            if (m_next1 == '!' && m_next2 == '-' && m_next3 == '-') {
                // <!-- marks the beginning of a line comment (for www usage)
                shift4();
                goto inSingleLineComment;
            }
            if (m_next1 == '<') {
                if (m_next2 == '=') {
                    shift3();
                    token = LSHIFTEQUAL;
                    break;
                }
                shift2();
                token = LSHIFT;
                break;
            }
            if (m_next1 == '=') {
                shift2();
                token = LE;
                break;
            }
            shift1();
            token = '<';
            break;
        case '+':
            if (m_next1 == '+') {
                shift2();
                if (m_terminator) {
                    token = AUTOPLUSPLUS;
                    break;
                }
                token = PLUSPLUS;
                break;
            }
            if (m_next1 == '=') {
                shift2();
                token = PLUSEQUAL;
                break;
            }
            shift1();
            token = '+';
            break;
        case '-':
            if (m_next1 == '-') {
                if (m_atLineStart && m_next2 == '>') {
                    shift3();
                    goto inSingleLineComment;
                }
                shift2();
                if (m_terminator) {
                    token = AUTOMINUSMINUS;
                    break;
                }
                token = MINUSMINUS;
                break;
            }
            if (m_next1 == '=') {
                shift2();
                token = MINUSEQUAL;
                break;
            }
            shift1();
            token = '-';
            break;
        case '*':
            if (m_next1 == '=') {
                shift2();
                token = MULTEQUAL;
                break;
            }
            shift1();
            token = '*';
            break;
        case '/':
            if (m_next1 == '/') {
                shift2();
                goto inSingleLineComment;
            }
            if (m_next1 == '*')
                goto inMultiLineComment;
            if (m_next1 == '=') {
                shift2();
                token = DIVEQUAL;
                break;
            }
            shift1();
            token = '/';
            break;
        case '&':
            if (m_next1 == '&') {
                shift2();
                token = AND;
                break;
            }
            if (m_next1 == '=') {
                shift2();
                token = ANDEQUAL;
                break;
            }
            shift1();
            token = '&';
            break;
        case '^':
            if (m_next1 == '=') {
                shift2();
                token = XOREQUAL;
                break;
            }
            shift1();
            token = '^';
            break;
        case '%':
            if (m_next1 == '=') {
                shift2();
                token = MODEQUAL;
                break;
            }
            shift1();
            token = '%';
            break;
        case '|':
            if (m_next1 == '=') {
                shift2();
                token = OREQUAL;
                break;
            }
            if (m_next1 == '|') {
                shift2();
                token = OR;
                break;
            }
            shift1();
            token = '|';
            break;
        case '.':
            if (isASCIIDigit(m_next1)) {
                record8('.');
                shift1();
                goto inNumberAfterDecimalPoint;
            }
            token = '.';
            shift1();
            break;
        case ',':
        case '~':
        case '?':
        case ':':
        case '(':
        case ')':
        case '[':
        case ']':
            token = m_current;
            shift1();
            break;
        case ';':
            shift1();
            m_delimited = true;
            token = ';';
            break;
        case '{':
            lvalp->intValue = currentOffset();
            shift1();
            token = OPENBRACE;
            break;
        case '}':
            lvalp->intValue = currentOffset();
            shift1();
            m_delimited = true;
            token = CLOSEBRACE;
            break;
        case '\\':
            goto startIdentifierWithBackslash;
        case '0':
            goto startNumberWithZeroDigit;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            goto startNumber;
        case '"':
        case '\'':
            goto startString;
        default:
            if (isIdentStart(m_current))
                goto startIdentifierOrKeyword;
            if (isLineTerminator(m_current)) {
                shiftLineTerminator();
                m_atLineStart = true;
                m_terminator = true;
                if (lastTokenWasRestrKeyword()) {
                    token = ';';
                    goto doneSemicolon;
                }
                goto start;
            }
            goto returnError;
    }

    m_atLineStart = false;
    goto returnToken;

startString: {
    int stringQuoteCharacter = m_current;
    shift1();

    const UChar* stringStart = currentCharacter();
    while (m_current != stringQuoteCharacter) {
        // Fast check for characters that require special handling.
        // Catches -1, \n, \r, \, 0x2028, and 0x2029 as efficiently
        // as possible, and lets through all common ASCII characters.
        if (UNLIKELY(m_current == '\\') || UNLIKELY(((static_cast<unsigned>(m_current) - 0xE) & 0x2000))) {
            m_buffer16.append(stringStart, currentCharacter() - stringStart);
            goto inString;
        }
        shift1();
    }
    lvalp->ident = makeIdentifier(stringStart, currentCharacter() - stringStart);
    shift1();
    m_atLineStart = false;
    m_delimited = false;
    token = STRING;
    goto returnToken;

inString:
    while (m_current != stringQuoteCharacter) {
        if (m_current == '\\')
            goto inStringEscapeSequence;
        if (UNLIKELY(isLineTerminator(m_current)))
            goto returnError;
        if (UNLIKELY(m_current == -1))
            goto returnError;
        record16(m_current);
        shift1();
    }
    goto doneString;

inStringEscapeSequence:
    shift1();
    if (m_current == 'x') {
        shift1();
        if (isASCIIHexDigit(m_current) && isASCIIHexDigit(m_next1)) {
            record16(convertHex(m_current, m_next1));
            shift2();
            goto inString;
        }
        record16('x');
        if (m_current == stringQuoteCharacter)
            goto doneString;
        goto inString;
    }
    if (m_current == 'u') {
        shift1();
        if (isASCIIHexDigit(m_current) && isASCIIHexDigit(m_next1) && isASCIIHexDigit(m_next2) && isASCIIHexDigit(m_next3)) {
            record16(convertUnicode(m_current, m_next1, m_next2, m_next3));
            shift4();
            goto inString;
        }
        if (m_current == stringQuoteCharacter) {
            record16('u');
            goto doneString;
        }
        goto returnError;
    }
    if (isASCIIOctalDigit(m_current)) {
        if (m_current >= '0' && m_current <= '3' && isASCIIOctalDigit(m_next1) && isASCIIOctalDigit(m_next2)) {
            record16((m_current - '0') * 64 + (m_next1 - '0') * 8 + m_next2 - '0');
            shift3();
            goto inString;
        }
        if (isASCIIOctalDigit(m_next1)) {
            record16((m_current - '0') * 8 + m_next1 - '0');
            shift2();
            goto inString;
        }
        record16(m_current - '0');
        shift1();
        goto inString;
    }
    if (isLineTerminator(m_current)) {
        shiftLineTerminator();
        goto inString;
    }
    record16(singleEscape(m_current));
    shift1();
    goto inString;
}

startIdentifierWithBackslash:
    shift1();
    if (UNLIKELY(m_current != 'u'))
        goto returnError;
    shift1();
    if (UNLIKELY(!isASCIIHexDigit(m_current) || !isASCIIHexDigit(m_next1) || !isASCIIHexDigit(m_next2) || !isASCIIHexDigit(m_next3)))
        goto returnError;
    token = convertUnicode(m_current, m_next1, m_next2, m_next3);
    if (UNLIKELY(!isIdentStart(token)))
        goto returnError;
    goto inIdentifierAfterCharacterCheck;

startIdentifierOrKeyword: {
    const UChar* identifierStart = currentCharacter();
    shift1();
    while (isIdentPart(m_current))
        shift1();
    if (LIKELY(m_current != '\\')) {
        lvalp->ident = makeIdentifier(identifierStart, currentCharacter() - identifierStart);
        goto doneIdentifierOrKeyword;
    }
    m_buffer16.append(identifierStart, currentCharacter() - identifierStart);
}

    do {
        shift1();
        if (UNLIKELY(m_current != 'u'))
            goto returnError;
        shift1();
        if (UNLIKELY(!isASCIIHexDigit(m_current) || !isASCIIHexDigit(m_next1) || !isASCIIHexDigit(m_next2) || !isASCIIHexDigit(m_next3)))
            goto returnError;
        token = convertUnicode(m_current, m_next1, m_next2, m_next3);
        if (UNLIKELY(!isIdentPart(token)))
            goto returnError;
inIdentifierAfterCharacterCheck:
        record16(token);
        shift4();

        while (isIdentPart(m_current)) {
            record16(m_current);
            shift1();
        }
    } while (UNLIKELY(m_current == '\\'));
    goto doneIdentifier;

inSingleLineComment:
    while (!isLineTerminator(m_current)) {
        if (UNLIKELY(m_current == -1))
            return 0;
        shift1();
    }
    shiftLineTerminator();
    m_atLineStart = true;
    m_terminator = true;
    if (lastTokenWasRestrKeyword())
        goto doneSemicolon;
    goto start;

inMultiLineComment:
    shift2();
    while (m_current != '*' || m_next1 != '/') {
        if (isLineTerminator(m_current))
            shiftLineTerminator();
        else {
            shift1();
            if (UNLIKELY(m_current == -1))
                goto returnError;
        }
    }
    shift2();
    m_atLineStart = false;
    goto start;

startNumberWithZeroDigit:
    shift1();
    if ((m_current | 0x20) == 'x' && isASCIIHexDigit(m_next1)) {
        shift1();
        goto inHex;
    }
    if (m_current == '.') {
        record8('0');
        record8('.');
        shift1();
        goto inNumberAfterDecimalPoint;
    }
    if ((m_current | 0x20) == 'e') {
        record8('0');
        record8('e');
        shift1();
        goto inExponentIndicator;
    }
    if (isASCIIOctalDigit(m_current))
        goto inOctal;
    if (isASCIIDigit(m_current))
        goto startNumber;
    lvalp->doubleValue = 0;
    goto doneNumeric;

inNumberAfterDecimalPoint:
    while (isASCIIDigit(m_current)) {
        record8(m_current);
        shift1();
    }
    if ((m_current | 0x20) == 'e') {
        record8('e');
        shift1();
        goto inExponentIndicator;
    }
    goto doneNumber;

inExponentIndicator:
    if (m_current == '+' || m_current == '-') {
        record8(m_current);
        shift1();
    }
    if (!isASCIIDigit(m_current))
        goto returnError;
    do {
        record8(m_current);
        shift1();
    } while (isASCIIDigit(m_current));
    goto doneNumber;

inOctal: {
    do {
        record8(m_current);
        shift1();
    } while (isASCIIOctalDigit(m_current));
    if (isASCIIDigit(m_current))
        goto startNumber;

    double dval = 0;

    const char* end = m_buffer8.end();
    for (const char* p = m_buffer8.data(); p < end; ++p) {
        dval *= 8;
        dval += *p - '0';
    }
    if (dval >= mantissaOverflowLowerBound)
        dval = parseIntOverflow(m_buffer8.data(), end - m_buffer8.data(), 8);

    m_buffer8.resize(0);

    lvalp->doubleValue = dval;
    goto doneNumeric;
}

inHex: {
    do {
        record8(m_current);
        shift1();
    } while (isASCIIHexDigit(m_current));

    double dval = 0;

    const char* end = m_buffer8.end();
    for (const char* p = m_buffer8.data(); p < end; ++p) {
        dval *= 16;
        dval += toASCIIHexValue(*p);
    }
    if (dval >= mantissaOverflowLowerBound)
        dval = parseIntOverflow(m_buffer8.data(), end - m_buffer8.data(), 16);

    m_buffer8.resize(0);

    lvalp->doubleValue = dval;
    goto doneNumeric;
}

startNumber:
    record8(m_current);
    shift1();
    while (isASCIIDigit(m_current)) {
        record8(m_current);
        shift1();
    }
    if (m_current == '.') {
        record8('.');
        shift1();
        goto inNumberAfterDecimalPoint;
    }
    if ((m_current | 0x20) == 'e') {
        record8('e');
        shift1();
        goto inExponentIndicator;
    }

    // Fall through into doneNumber.

doneNumber:
    // Null-terminate string for strtod.
    m_buffer8.append('\0');
    lvalp->doubleValue = WTF::strtod(m_buffer8.data(), 0);
    m_buffer8.resize(0);

    // Fall through into doneNumeric.

doneNumeric:
    // No identifiers allowed directly after numeric literal, e.g. "3in" is bad.
    if (UNLIKELY(isIdentStart(m_current)))
        goto returnError;

    m_atLineStart = false;
    m_delimited = false;
    token = NUMBER;
    goto returnToken;

doneSemicolon:
    token = ';';
    m_delimited = true;
    goto returnToken;

doneIdentifier:
    m_atLineStart = false;
    m_delimited = false;
    lvalp->ident = makeIdentifier(m_buffer16.data(), m_buffer16.size());
    m_buffer16.resize(0);
    token = IDENT;
    goto returnToken;

doneIdentifierOrKeyword: {
    m_atLineStart = false;
    m_delimited = false;
    m_buffer16.resize(0);
    const HashEntry* entry = m_keywordTable.entry(m_globalData, *lvalp->ident);
    token = entry ? entry->lexerValue() : IDENT;
    goto returnToken;
}

doneString:
    // Atomize constant strings in case they're later used in property lookup.
    shift1();
    m_atLineStart = false;
    m_delimited = false;
    lvalp->ident = makeIdentifier(m_buffer16.data(), m_buffer16.size());
    m_buffer16.resize(0);
    token = STRING;

    // Fall through into returnToken.

returnToken: {
    int lineNumber = m_lineNumber;
    llocp->first_line = lineNumber;
    llocp->last_line = lineNumber;
    llocp->first_column = startOffset;
    llocp->last_column = currentOffset();

    m_lastToken = token;
    return token;
}

returnError:
    m_error = true;
    return -1;
}

bool Lexer::scanRegExp(const Identifier*& pattern, const Identifier*& flags, UChar patternPrefix)
{
    ASSERT(m_buffer16.isEmpty());

    bool lastWasEscape = false;
    bool inBrackets = false;

    if (patternPrefix) {
        ASSERT(!isLineTerminator(patternPrefix));
        ASSERT(patternPrefix != '/');
        ASSERT(patternPrefix != '[');
        record16(patternPrefix);
    }

    while (true) {
        int current = m_current;

        if (isLineTerminator(current) || current == -1) {
            m_buffer16.resize(0);
            return false;
        }

        shift1();

        if (current == '/' && !lastWasEscape && !inBrackets)
            break;

        record16(current);

        if (lastWasEscape) {
            lastWasEscape = false;
            continue;
        }

        switch (current) {
        case '[':
            inBrackets = true;
            break;
        case ']':
            inBrackets = false;
            break;
        case '\\':
            lastWasEscape = true;
            break;
        }
    }

    pattern = makeIdentifier(m_buffer16.data(), m_buffer16.size());
    m_buffer16.resize(0);

    while (isIdentPart(m_current)) {
        record16(m_current);
        shift1();
    }

    flags = makeIdentifier(m_buffer16.data(), m_buffer16.size());
    m_buffer16.resize(0);

    return true;
}

bool Lexer::skipRegExp()
{
    bool lastWasEscape = false;
    bool inBrackets = false;

    while (true) {
        int current = m_current;

        if (isLineTerminator(current) || current == -1)
            return false;

        shift1();

        if (current == '/' && !lastWasEscape && !inBrackets)
            break;

        if (lastWasEscape) {
            lastWasEscape = false;
            continue;
        }

        switch (current) {
        case '[':
            inBrackets = true;
            break;
        case ']':
            inBrackets = false;
            break;
        case '\\':
            lastWasEscape = true;
            break;
        }
    }

    while (isIdentPart(m_current))
        shift1();

    return true;
}

void Lexer::clear()
{
    m_arena = 0;
    m_codeWithoutBOMs.clear();

    Vector<char> newBuffer8;
    newBuffer8.reserveInitialCapacity(initialReadBufferCapacity);
    m_buffer8.swap(newBuffer8);

    Vector<UChar> newBuffer16;
    newBuffer16.reserveInitialCapacity(initialReadBufferCapacity);
    m_buffer16.swap(newBuffer16);

    m_isReparsing = false;
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
