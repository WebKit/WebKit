/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All Rights Reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2010 Zoltan Herczeg (zherczeg@inf.u-szeged.hu)
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
#include "Identifier.h"
#include "NodeInfo.h"
#include "Nodes.h"
#include "dtoa.h"
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <wtf/Assertions.h>

using namespace WTF;
using namespace Unicode;

#include "JSParser.h"
#include "Lookup.h"
#include "Lexer.lut.h"

namespace JSC {


enum CharacterTypes {
    // Types for the main switch
    CharacterInvalid,

    CharacterAlpha,
    CharacterZero,
    CharacterNumber,

    CharacterLineTerminator,
    CharacterExclamationMark,
    CharacterSimple,
    CharacterQuote,
    CharacterDot,
    CharacterSlash,
    CharacterBackSlash,
    CharacterSemicolon,
    CharacterOpenBrace,
    CharacterCloseBrace,

    CharacterAdd,
    CharacterSub,
    CharacterMultiply,
    CharacterModulo,
    CharacterAnd,
    CharacterXor,
    CharacterOr,
    CharacterLess,
    CharacterGreater,
    CharacterEqual,

    // Other types (only one so far)
    CharacterWhiteSpace,
};

// 128 ascii codes
static unsigned char AsciiCharacters[128] = {
/*   0 - Null               */ CharacterInvalid,
/*   1 - Start of Heading   */ CharacterInvalid,
/*   2 - Start of Text      */ CharacterInvalid,
/*   3 - End of Text        */ CharacterInvalid,
/*   4 - End of Transm.     */ CharacterInvalid,
/*   5 - Enquiry            */ CharacterInvalid,
/*   6 - Acknowledgment     */ CharacterInvalid,
/*   7 - Bell               */ CharacterInvalid,
/*   8 - Back Space         */ CharacterInvalid,
/*   9 - Horizontal Tab     */ CharacterWhiteSpace,
/*  10 - Line Feed          */ CharacterLineTerminator,
/*  11 - Vertical Tab       */ CharacterWhiteSpace,
/*  12 - Form Feed          */ CharacterWhiteSpace,
/*  13 - Carriage Return    */ CharacterLineTerminator,
/*  14 - Shift Out          */ CharacterInvalid,
/*  15 - Shift In           */ CharacterInvalid,
/*  16 - Data Line Escape   */ CharacterInvalid,
/*  17 - Device Control 1   */ CharacterInvalid,
/*  18 - Device Control 2   */ CharacterInvalid,
/*  19 - Device Control 3   */ CharacterInvalid,
/*  20 - Device Control 4   */ CharacterInvalid,
/*  21 - Negative Ack.      */ CharacterInvalid,
/*  22 - Synchronous Idle   */ CharacterInvalid,
/*  23 - End of Transmit    */ CharacterInvalid,
/*  24 - Cancel             */ CharacterInvalid,
/*  25 - End of Medium      */ CharacterInvalid,
/*  26 - Substitute         */ CharacterInvalid,
/*  27 - Escape             */ CharacterInvalid,
/*  28 - File Separator     */ CharacterInvalid,
/*  29 - Group Separator    */ CharacterInvalid,
/*  30 - Record Separator   */ CharacterInvalid,
/*  31 - Unit Separator     */ CharacterInvalid,
/*  32 - Space              */ CharacterWhiteSpace,
/*  33 - !                  */ CharacterExclamationMark,
/*  34 - "                  */ CharacterQuote,
/*  35 - #                  */ CharacterInvalid,
/*  36 - $                  */ CharacterAlpha,
/*  37 - %                  */ CharacterModulo,
/*  38 - &                  */ CharacterAnd,
/*  39 - '                  */ CharacterQuote,
/*  40 - (                  */ CharacterSimple,
/*  41 - )                  */ CharacterSimple,
/*  42 - *                  */ CharacterMultiply,
/*  43 - +                  */ CharacterAdd,
/*  44 - ,                  */ CharacterSimple,
/*  45 - -                  */ CharacterSub,
/*  46 - .                  */ CharacterDot,
/*  47 - /                  */ CharacterSlash,
/*  48 - 0                  */ CharacterZero,
/*  49 - 1                  */ CharacterNumber,
/*  50 - 2                  */ CharacterNumber,
/*  51 - 3                  */ CharacterNumber,
/*  52 - 4                  */ CharacterNumber,
/*  53 - 5                  */ CharacterNumber,
/*  54 - 6                  */ CharacterNumber,
/*  55 - 7                  */ CharacterNumber,
/*  56 - 8                  */ CharacterNumber,
/*  57 - 9                  */ CharacterNumber,
/*  58 - :                  */ CharacterSimple,
/*  59 - ;                  */ CharacterSemicolon,
/*  60 - <                  */ CharacterLess,
/*  61 - =                  */ CharacterEqual,
/*  62 - >                  */ CharacterGreater,
/*  63 - ?                  */ CharacterSimple,
/*  64 - @                  */ CharacterInvalid,
/*  65 - A                  */ CharacterAlpha,
/*  66 - B                  */ CharacterAlpha,
/*  67 - C                  */ CharacterAlpha,
/*  68 - D                  */ CharacterAlpha,
/*  69 - E                  */ CharacterAlpha,
/*  70 - F                  */ CharacterAlpha,
/*  71 - G                  */ CharacterAlpha,
/*  72 - H                  */ CharacterAlpha,
/*  73 - I                  */ CharacterAlpha,
/*  74 - J                  */ CharacterAlpha,
/*  75 - K                  */ CharacterAlpha,
/*  76 - L                  */ CharacterAlpha,
/*  77 - M                  */ CharacterAlpha,
/*  78 - N                  */ CharacterAlpha,
/*  79 - O                  */ CharacterAlpha,
/*  80 - P                  */ CharacterAlpha,
/*  81 - Q                  */ CharacterAlpha,
/*  82 - R                  */ CharacterAlpha,
/*  83 - S                  */ CharacterAlpha,
/*  84 - T                  */ CharacterAlpha,
/*  85 - U                  */ CharacterAlpha,
/*  86 - V                  */ CharacterAlpha,
/*  87 - W                  */ CharacterAlpha,
/*  88 - X                  */ CharacterAlpha,
/*  89 - Y                  */ CharacterAlpha,
/*  90 - Z                  */ CharacterAlpha,
/*  91 - [                  */ CharacterSimple,
/*  92 - \                  */ CharacterBackSlash,
/*  93 - ]                  */ CharacterSimple,
/*  94 - ^                  */ CharacterXor,
/*  95 - _                  */ CharacterAlpha,
/*  96 - `                  */ CharacterInvalid,
/*  97 - a                  */ CharacterAlpha,
/*  98 - b                  */ CharacterAlpha,
/*  99 - c                  */ CharacterAlpha,
/* 100 - d                  */ CharacterAlpha,
/* 101 - e                  */ CharacterAlpha,
/* 102 - f                  */ CharacterAlpha,
/* 103 - g                  */ CharacterAlpha,
/* 104 - h                  */ CharacterAlpha,
/* 105 - i                  */ CharacterAlpha,
/* 106 - j                  */ CharacterAlpha,
/* 107 - k                  */ CharacterAlpha,
/* 108 - l                  */ CharacterAlpha,
/* 109 - m                  */ CharacterAlpha,
/* 110 - n                  */ CharacterAlpha,
/* 111 - o                  */ CharacterAlpha,
/* 112 - p                  */ CharacterAlpha,
/* 113 - q                  */ CharacterAlpha,
/* 114 - r                  */ CharacterAlpha,
/* 115 - s                  */ CharacterAlpha,
/* 116 - t                  */ CharacterAlpha,
/* 117 - u                  */ CharacterAlpha,
/* 118 - v                  */ CharacterAlpha,
/* 119 - w                  */ CharacterAlpha,
/* 120 - x                  */ CharacterAlpha,
/* 121 - y                  */ CharacterAlpha,
/* 122 - z                  */ CharacterAlpha,
/* 123 - {                  */ CharacterOpenBrace,
/* 124 - |                  */ CharacterOr,
/* 125 - }                  */ CharacterCloseBrace,
/* 126 - ~                  */ CharacterSimple,
/* 127 - Delete             */ CharacterInvalid,
};

Lexer::Lexer(JSGlobalData* globalData)
    : m_isReparsing(false)
    , m_globalData(globalData)
    , m_keywordTable(JSC::mainTable)
{
}

Lexer::~Lexer()
{
    m_keywordTable.deleteTable();
}

ALWAYS_INLINE const UChar* Lexer::currentCharacter() const
{
    ASSERT(m_code <= m_codeEnd);
    return m_code;
}

ALWAYS_INLINE int Lexer::currentOffset() const
{
    return currentCharacter() - m_codeStart;
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

    m_buffer8.reserveInitialCapacity(initialReadBufferCapacity);
    m_buffer16.reserveInitialCapacity((m_codeEnd - m_code) / 2);

    if (LIKELY(m_code < m_codeEnd))
        m_current = *m_code;
    else
        m_current = -1;
    ASSERT(currentOffset() == source.startOffset());
}

ALWAYS_INLINE void Lexer::shift()
{
    // Faster than an if-else sequence
    ASSERT(m_current != -1);
    m_current = -1;
    ++m_code;
    if (LIKELY(m_code < m_codeEnd))
        m_current = *m_code;
}

ALWAYS_INLINE int Lexer::peek(int offset)
{
    // Only use if necessary
    ASSERT(offset > 0 && offset < 5);
    const UChar* code = m_code + offset;
    return (code < m_codeEnd) ? *code : -1;
}

int Lexer::getUnicodeCharacter()
{
    int char1 = peek(1);
    int char2 = peek(2);
    int char3 = peek(3);

    if (UNLIKELY(!isASCIIHexDigit(m_current) || !isASCIIHexDigit(char1) || !isASCIIHexDigit(char2) || !isASCIIHexDigit(char3)))
        return -1;

    int result = convertUnicode(m_current, char1, char2, char3);
    shift();
    shift();
    shift();
    shift();
    return result;
}

void Lexer::shiftLineTerminator()
{
    ASSERT(isLineTerminator(m_current));

    int m_prev = m_current;
    shift();

    // Allow both CRLF and LFCR.
    if (m_prev + m_current == '\n' + '\r')
        shift();

    ++m_lineNumber;
}

ALWAYS_INLINE const Identifier* Lexer::makeIdentifier(const UChar* characters, size_t length)
{
    return &m_arena->makeIdentifier(m_globalData, characters, length);
}

ALWAYS_INLINE bool Lexer::lastTokenWasRestrKeyword() const
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
        case '\\':
            return '\\';
        case '\'':
            return '\'';
        case '"':
            return '"';
        default:
            return 0;
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

ALWAYS_INLINE bool Lexer::parseString(void* lvalp)
{
    int stringQuoteCharacter = m_current;
    shift();

    const UChar* stringStart = currentCharacter();

    while (m_current != stringQuoteCharacter) {
        if (UNLIKELY(m_current == '\\')) {
            if (stringStart != currentCharacter())
                m_buffer16.append(stringStart, currentCharacter() - stringStart);
            shift();

            int escape = singleEscape(m_current);

            // Most common escape sequences first
            if (escape) {
                record16(escape);
                shift();
            } else if (UNLIKELY(isLineTerminator(m_current)))
                shiftLineTerminator();
            else if (m_current == 'x') {
                shift();
                if (isASCIIHexDigit(m_current) && isASCIIHexDigit(peek(1))) {
                    int prev = m_current;
                    shift();
                    record16(convertHex(prev, m_current));
                    shift();
                } else
                    record16('x');
            } else if (m_current == 'u') {
                shift();
                int character = getUnicodeCharacter();
                if (character != -1)
                    record16(character);
                else if (m_current == stringQuoteCharacter)
                    record16('u');
                else // Only stringQuoteCharacter allowed after \u
                    return false;
            } else if (isASCIIOctalDigit(m_current)) {
                // Octal character sequences
                int character1 = m_current;
                shift();
                if (isASCIIOctalDigit(m_current)) {
                    // Two octal characters
                    int character2 = m_current;
                    shift();
                    if (character1 >= '0' && character1 <= '3' && isASCIIOctalDigit(m_current)) {
                        record16((character1 - '0') * 64 + (character2 - '0') * 8 + m_current - '0');
                        shift();
                    } else
                        record16((character1 - '0') * 8 + character2 - '0');
                } else
                    record16(character1 - '0');
            } else if (m_current != -1) {
                record16(m_current);
                shift();
            } else
                return false;

            stringStart = currentCharacter();
            continue;
        } else if (UNLIKELY(((static_cast<unsigned>(m_current) - 0xE) & 0x2000))) {
            // New-line or end of input is not allowed
            if (UNLIKELY(isLineTerminator(m_current)) || UNLIKELY(m_current == -1))
                return false;
            // Anything else is just a normal character
        }
        shift();
    }

    if (currentCharacter() != stringStart)
        m_buffer16.append(stringStart, currentCharacter() - stringStart);
    reinterpret_cast<YYSTYPE*>(lvalp)->ident = makeIdentifier(m_buffer16.data(), m_buffer16.size());
    m_buffer16.resize(0);
    return true;
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
        shift();

    int startOffset = currentOffset();

    if (UNLIKELY(m_current == -1)) {
        if (!m_terminator && !m_delimited && !m_isReparsing) {
            // automatic semicolon insertion if program incomplete
            goto doneSemicolon;
        }
        return 0;
    }

    m_delimited = false;

    if (isASCII(m_current)) {
        ASSERT(m_current >= 0 && m_current < 128);

        switch (AsciiCharacters[m_current]) {
        case CharacterGreater:
            shift();
            if (m_current == '>') {
                shift();
                if (m_current == '>') {
                    shift();
                    if (m_current == '=') {
                        shift();
                        token = URSHIFTEQUAL;
                        break;
                    }
                    token = URSHIFT;
                    break;
                }
                if (m_current == '=') {
                    shift();
                    token = RSHIFTEQUAL;
                    break;
                }
                token = RSHIFT;
                break;
            }
            if (m_current == '=') {
                shift();
                token = GE;
                break;
            }
            token = '>';
            break;
        case CharacterEqual:
            shift();
            if (m_current == '=') {
                shift();
                if (m_current == '=') {
                    shift();
                    token = STREQ;
                    break;
                }
                token = EQEQ;
                break;
            }
            token = '=';
            break;
        case CharacterLess:
            shift();
            if (m_current == '!' && peek(1) == '-' && peek(2) == '-') {
                // <!-- marks the beginning of a line comment (for www usage)
                goto inSingleLineComment;
            }
            if (m_current == '<') {
                shift();
                if (m_current == '=') {
                    shift();
                    token = LSHIFTEQUAL;
                    break;
                }
                token = LSHIFT;
                break;
            }
            if (m_current == '=') {
                shift();
                token = LE;
                break;
            }
            token = '<';
            break;
        case CharacterExclamationMark:
            shift();
            if (m_current == '=') {
                shift();
                if (m_current == '=') {
                    shift();
                    token = STRNEQ;
                    break;
                }
                token = NE;
                break;
            }
            token = '!';
            break;
        case CharacterAdd:
            shift();
            if (m_current == '+') {
                shift();
                token = (!m_terminator) ? PLUSPLUS : AUTOPLUSPLUS;
                break;
            }
            if (m_current == '=') {
                shift();
                token = PLUSEQUAL;
                break;
            }
            token = '+';
            break;
        case CharacterSub:
            shift();
            if (m_current == '-') {
                shift();
                if (m_atLineStart && m_current == '>') {
                    shift();
                    goto inSingleLineComment;
                }
                token = (!m_terminator) ? MINUSMINUS : AUTOMINUSMINUS;
                break;
            }
            if (m_current == '=') {
                shift();
                token = MINUSEQUAL;
                break;
            }
            token = '-';
            break;
        case CharacterMultiply:
            shift();
            if (m_current == '=') {
                shift();
                token = MULTEQUAL;
                break;
            }
            token = '*';
            break;
        case CharacterSlash:
            shift();
            if (m_current == '/') {
                shift();
                goto inSingleLineComment;
            }
            if (m_current == '*') {
                shift();
                goto inMultiLineComment;
            }
            if (m_current == '=') {
                shift();
                token = DIVEQUAL;
                break;
            }
            token = '/';
            break;
        case CharacterAnd:
            shift();
            if (m_current == '&') {
                shift();
                token = AND;
                break;
            }
            if (m_current == '=') {
                shift();
                token = ANDEQUAL;
                break;
            }
            token = '&';
            break;
        case CharacterXor:
            shift();
            if (m_current == '=') {
                shift();
                token = XOREQUAL;
                break;
            }
            token = '^';
            break;
        case CharacterModulo:
            shift();
            if (m_current == '=') {
                shift();
                token = MODEQUAL;
                break;
            }
            token = '%';
            break;
        case CharacterOr:
            shift();
            if (m_current == '=') {
                shift();
                token = OREQUAL;
                break;
            }
            if (m_current == '|') {
                shift();
                token = OR;
                break;
            }
            token = '|';
            break;
        case CharacterDot:
            shift();
            if (isASCIIDigit(m_current)) {
                record8('.');
                goto inNumberAfterDecimalPoint;
            }
            token = '.';
            break;
        case CharacterSimple:
            token = m_current;
            shift();
            break;
        case CharacterSemicolon:
            m_delimited = true;
            shift();
            token = ';';
            break;
        case CharacterOpenBrace:
            lvalp->intValue = currentOffset();
            shift();
            token = OPENBRACE;
            break;
        case CharacterCloseBrace:
            lvalp->intValue = currentOffset();
            m_delimited = true;
            shift();
            token = CLOSEBRACE;
            break;
        case CharacterBackSlash:
            goto startIdentifierWithBackslash;
        case CharacterZero:
            goto startNumberWithZeroDigit;
        case CharacterNumber:
            goto startNumber;
        case CharacterQuote:
            if (UNLIKELY(!parseString(lvalp)))
                goto returnError;
            shift();
            m_delimited = false;
            token = STRING;
            break;
        case CharacterAlpha:
            ASSERT(isIdentStart(m_current));
            goto startIdentifierOrKeyword;
        case CharacterLineTerminator:
            ASSERT(isLineTerminator(m_current));
            shiftLineTerminator();
            m_atLineStart = true;
            m_terminator = true;
            if (lastTokenWasRestrKeyword()) {
                token = ';';
                goto doneSemicolon;
            }
            goto start;
        case CharacterInvalid:
            goto returnError;
        default:
            ASSERT_NOT_REACHED();
            goto returnError;
        }
    } else {
        // Rare characters

        if (isNonASCIIIdentStart(m_current))
            goto startIdentifierOrKeyword;
        if (isLineTerminator(m_current)) {
            shiftLineTerminator();
            m_atLineStart = true;
            m_terminator = true;
            if (lastTokenWasRestrKeyword())
                goto doneSemicolon;
            goto start;
        }
        goto returnError;
    }

    m_atLineStart = false;
    goto returnToken;

startIdentifierWithBackslash: {
    shift();
    if (UNLIKELY(m_current != 'u'))
        goto returnError;
    shift();

    token = getUnicodeCharacter();
    if (UNLIKELY(token == -1))
        goto returnError;
    if (UNLIKELY(!isIdentStart(token)))
        goto returnError;
    goto inIdentifierAfterCharacterCheck;
}

startIdentifierOrKeyword: {
    const UChar* identifierStart = currentCharacter();
    shift();
    while (isIdentPart(m_current))
        shift();
    if (LIKELY(m_current != '\\')) {
        // Fast case for idents which does not contain \uCCCC characters
        lvalp->ident = makeIdentifier(identifierStart, currentCharacter() - identifierStart);
        goto doneIdentifierOrKeyword;
    }
    m_buffer16.append(identifierStart, currentCharacter() - identifierStart);
}

    do {
        shift();
        if (UNLIKELY(m_current != 'u'))
            goto returnError;
        shift();
        token = getUnicodeCharacter();
        if (UNLIKELY(token == -1))
            goto returnError;
        if (UNLIKELY(!isIdentPart(token)))
            goto returnError;
inIdentifierAfterCharacterCheck:
        record16(token);

        while (isIdentPart(m_current)) {
            record16(m_current);
            shift();
        }
    } while (UNLIKELY(m_current == '\\'));
    goto doneIdentifier;

inSingleLineComment:
    while (!isLineTerminator(m_current)) {
        if (UNLIKELY(m_current == -1))
            return 0;
        shift();
    }
    shiftLineTerminator();
    m_atLineStart = true;
    m_terminator = true;
    if (lastTokenWasRestrKeyword())
        goto doneSemicolon;
    goto start;

inMultiLineComment:
    while (true) {
        if (UNLIKELY(m_current == '*')) {
            shift();
            if (m_current == '/')
                break;
            if (m_current == '*')
                continue;
        }

        if (UNLIKELY(m_current == -1))
            goto returnError;

        if (isLineTerminator(m_current))
            shiftLineTerminator();
        else
            shift();
    }
    shift();
    m_atLineStart = false;
    goto start;

startNumberWithZeroDigit:
    shift();
    if ((m_current | 0x20) == 'x' && isASCIIHexDigit(peek(1))) {
        shift();
        goto inHex;
    }
    if (m_current == '.') {
        record8('0');
        record8('.');
        shift();
        goto inNumberAfterDecimalPoint;
    }
    if ((m_current | 0x20) == 'e') {
        record8('0');
        record8('e');
        shift();
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
        shift();
    }
    if ((m_current | 0x20) == 'e') {
        record8('e');
        shift();
        goto inExponentIndicator;
    }
    goto doneNumber;

inExponentIndicator:
    if (m_current == '+' || m_current == '-') {
        record8(m_current);
        shift();
    }
    if (!isASCIIDigit(m_current))
        goto returnError;
    do {
        record8(m_current);
        shift();
    } while (isASCIIDigit(m_current));
    goto doneNumber;

inOctal: {
    do {
        record8(m_current);
        shift();
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
        shift();
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
    shift();
    while (isASCIIDigit(m_current)) {
        record8(m_current);
        shift();
    }
    if (m_current == '.') {
        record8('.');
        shift();
        goto inNumberAfterDecimalPoint;
    }
    if ((m_current | 0x20) == 'e') {
        record8('e');
        shift();
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
    token = entry ? entry->lexerValue() : static_cast<int>(IDENT);

    // Fall through into returnToken.
}

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

        shift();

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
        shift();
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

        shift();

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
        shift();

    return true;
}

void Lexer::clear()
{
    m_arena = 0;
    m_codeWithoutBOMs.clear();

    Vector<char> newBuffer8;
    m_buffer8.swap(newBuffer8);

    Vector<UChar> newBuffer16;
    m_buffer16.swap(newBuffer16);

    m_isReparsing = false;
}

SourceCode Lexer::sourceCode(int openBrace, int closeBrace, int firstLine)
{
    return SourceCode(m_source->provider(), openBrace, closeBrace + 1, firstLine);
}

} // namespace JSC
