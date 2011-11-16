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

#include "KeywordLookup.h"
#include "Lexer.lut.h"
#include "Parser.h"

namespace JSC {

Keywords::Keywords(JSGlobalData* globalData)
    : m_globalData(globalData)
    , m_keywordTable(JSC::mainTable)
{
}

enum CharacterType {
    // Types for the main switch

    // The first three types are fixed, and also used for identifying
    // ASCII alpha and alphanumeric characters (see isIdentStart and isIdentPart).
    CharacterIdentifierStart,
    CharacterZero,
    CharacterNumber,

    CharacterInvalid,
    CharacterLineTerminator,
    CharacterExclamationMark,
    CharacterOpenParen,
    CharacterCloseParen,
    CharacterOpenBracket,
    CharacterCloseBracket,
    CharacterComma,
    CharacterColon,
    CharacterQuestion,
    CharacterTilde,
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

// 128 ASCII codes
static const unsigned short typesOfASCIICharacters[128] = {
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
/*  36 - $                  */ CharacterIdentifierStart,
/*  37 - %                  */ CharacterModulo,
/*  38 - &                  */ CharacterAnd,
/*  39 - '                  */ CharacterQuote,
/*  40 - (                  */ CharacterOpenParen,
/*  41 - )                  */ CharacterCloseParen,
/*  42 - *                  */ CharacterMultiply,
/*  43 - +                  */ CharacterAdd,
/*  44 - ,                  */ CharacterComma,
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
/*  58 - :                  */ CharacterColon,
/*  59 - ;                  */ CharacterSemicolon,
/*  60 - <                  */ CharacterLess,
/*  61 - =                  */ CharacterEqual,
/*  62 - >                  */ CharacterGreater,
/*  63 - ?                  */ CharacterQuestion,
/*  64 - @                  */ CharacterInvalid,
/*  65 - A                  */ CharacterIdentifierStart,
/*  66 - B                  */ CharacterIdentifierStart,
/*  67 - C                  */ CharacterIdentifierStart,
/*  68 - D                  */ CharacterIdentifierStart,
/*  69 - E                  */ CharacterIdentifierStart,
/*  70 - F                  */ CharacterIdentifierStart,
/*  71 - G                  */ CharacterIdentifierStart,
/*  72 - H                  */ CharacterIdentifierStart,
/*  73 - I                  */ CharacterIdentifierStart,
/*  74 - J                  */ CharacterIdentifierStart,
/*  75 - K                  */ CharacterIdentifierStart,
/*  76 - L                  */ CharacterIdentifierStart,
/*  77 - M                  */ CharacterIdentifierStart,
/*  78 - N                  */ CharacterIdentifierStart,
/*  79 - O                  */ CharacterIdentifierStart,
/*  80 - P                  */ CharacterIdentifierStart,
/*  81 - Q                  */ CharacterIdentifierStart,
/*  82 - R                  */ CharacterIdentifierStart,
/*  83 - S                  */ CharacterIdentifierStart,
/*  84 - T                  */ CharacterIdentifierStart,
/*  85 - U                  */ CharacterIdentifierStart,
/*  86 - V                  */ CharacterIdentifierStart,
/*  87 - W                  */ CharacterIdentifierStart,
/*  88 - X                  */ CharacterIdentifierStart,
/*  89 - Y                  */ CharacterIdentifierStart,
/*  90 - Z                  */ CharacterIdentifierStart,
/*  91 - [                  */ CharacterOpenBracket,
/*  92 - \                  */ CharacterBackSlash,
/*  93 - ]                  */ CharacterCloseBracket,
/*  94 - ^                  */ CharacterXor,
/*  95 - _                  */ CharacterIdentifierStart,
/*  96 - `                  */ CharacterInvalid,
/*  97 - a                  */ CharacterIdentifierStart,
/*  98 - b                  */ CharacterIdentifierStart,
/*  99 - c                  */ CharacterIdentifierStart,
/* 100 - d                  */ CharacterIdentifierStart,
/* 101 - e                  */ CharacterIdentifierStart,
/* 102 - f                  */ CharacterIdentifierStart,
/* 103 - g                  */ CharacterIdentifierStart,
/* 104 - h                  */ CharacterIdentifierStart,
/* 105 - i                  */ CharacterIdentifierStart,
/* 106 - j                  */ CharacterIdentifierStart,
/* 107 - k                  */ CharacterIdentifierStart,
/* 108 - l                  */ CharacterIdentifierStart,
/* 109 - m                  */ CharacterIdentifierStart,
/* 110 - n                  */ CharacterIdentifierStart,
/* 111 - o                  */ CharacterIdentifierStart,
/* 112 - p                  */ CharacterIdentifierStart,
/* 113 - q                  */ CharacterIdentifierStart,
/* 114 - r                  */ CharacterIdentifierStart,
/* 115 - s                  */ CharacterIdentifierStart,
/* 116 - t                  */ CharacterIdentifierStart,
/* 117 - u                  */ CharacterIdentifierStart,
/* 118 - v                  */ CharacterIdentifierStart,
/* 119 - w                  */ CharacterIdentifierStart,
/* 120 - x                  */ CharacterIdentifierStart,
/* 121 - y                  */ CharacterIdentifierStart,
/* 122 - z                  */ CharacterIdentifierStart,
/* 123 - {                  */ CharacterOpenBrace,
/* 124 - |                  */ CharacterOr,
/* 125 - }                  */ CharacterCloseBrace,
/* 126 - ~                  */ CharacterTilde,
/* 127 - Delete             */ CharacterInvalid,
};

template <typename T>
Lexer<T>::Lexer(JSGlobalData* globalData)
    : m_isReparsing(false)
    , m_globalData(globalData)
{
}

template <typename T>
Lexer<T>::~Lexer()
{
}

template <typename T>
UString Lexer<T>::getInvalidCharMessage()
{
    switch (m_current) {
    case 0:
        return "Invalid character: '\\0'";
    case 10:
        return "Invalid character: '\\n'";
    case 11:
        return "Invalid character: '\\v'";
    case 13:
        return "Invalid character: '\\r'";
    case 35:
        return "Invalid character: '#'";
    case 64:
        return "Invalid character: '@'";
    case 96:
        return "Invalid character: '`'";
    default:
        return String::format("Invalid character '\\u%04u'", m_current).impl();
    }
}

template <typename T>
ALWAYS_INLINE const T* Lexer<T>::currentCharacter() const
{
    ASSERT(m_code <= m_codeEnd);
    return m_code;
}

template <typename T>
void Lexer<T>::setCode(const SourceCode& source, ParserArena* arena)
{
    m_arena = &arena->identifierArena();
    
    m_lineNumber = source.firstLine();
    m_delimited = false;
    m_lastToken = -1;
    
    const StringImpl* sourceString = source.provider()->data();

    if (sourceString)
        setCodeStart(sourceString);
    else
        m_codeStart = 0;

    m_source = &source;
    m_code = m_codeStart + source.startOffset();
    m_codeEnd = m_codeStart + source.endOffset();
    m_error = false;
    m_atLineStart = true;
    m_lexErrorMessage = UString();
    
    m_buffer8.reserveInitialCapacity(initialReadBufferCapacity);
    m_buffer16.reserveInitialCapacity((m_codeEnd - m_code) / 2);
    
    if (LIKELY(m_code < m_codeEnd))
        m_current = *m_code;
    else
        m_current = -1;
    ASSERT(currentOffset() == source.startOffset());
}

template <typename T>
template <int shiftAmount> ALWAYS_INLINE void Lexer<T>::internalShift()
{
    m_code += shiftAmount;
    m_current = *m_code;
}

template <typename T>
ALWAYS_INLINE void Lexer<T>::shift()
{
    // Faster than an if-else sequence
    ASSERT(m_current != -1);
    m_current = -1;
    m_code++;
    if (LIKELY(m_code < m_codeEnd))
        m_current = *m_code;
}

template <typename T>
ALWAYS_INLINE int Lexer<T>::peek(int offset)
{
    // Only use if necessary
    ASSERT(offset > 0 && offset < 5);
    const T* code = m_code + offset;
    return (code < m_codeEnd) ? *code : -1;
}

template <typename T>
int Lexer<T>::getUnicodeCharacter()
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

template <typename T>
void Lexer<T>::shiftLineTerminator()
{
    ASSERT(isLineTerminator(m_current));

    int m_prev = m_current;
    shift();

    // Allow both CRLF and LFCR.
    if (m_prev + m_current == '\n' + '\r')
        shift();

    ++m_lineNumber;
}

template <typename T>
ALWAYS_INLINE bool Lexer<T>::lastTokenWasRestrKeyword() const
{
    return m_lastToken == CONTINUE || m_lastToken == BREAK || m_lastToken == RETURN || m_lastToken == THROW;
}

static NEVER_INLINE bool isNonASCIIIdentStart(int c)
{
    return category(c) & (Letter_Uppercase | Letter_Lowercase | Letter_Titlecase | Letter_Modifier | Letter_Other);
}

static inline bool isIdentStart(int c)
{
    return isASCII(c) ? typesOfASCIICharacters[c] == CharacterIdentifierStart : isNonASCIIIdentStart(c);
}

static NEVER_INLINE bool isNonASCIIIdentPart(int c)
{
    return category(c) & (Letter_Uppercase | Letter_Lowercase | Letter_Titlecase | Letter_Modifier | Letter_Other
        | Mark_NonSpacing | Mark_SpacingCombining | Number_DecimalDigit | Punctuation_Connector);
}

static ALWAYS_INLINE bool isIdentPart(int c)
{
    // Character types are divided into two groups depending on whether they can be part of an
    // identifier or not. Those whose type value is less or equal than CharacterNumber can be
    // part of an identifier. (See the CharacterType definition for more details.)
    return isASCII(c) ? typesOfASCIICharacters[c] <= CharacterNumber : isNonASCIIIdentPart(c);
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

template <typename T>
inline void Lexer<T>::record8(int c)
{
    ASSERT(c >= 0);
    ASSERT(c <= 0xFF);
    m_buffer8.append(static_cast<LChar>(c));
}

template <typename T>
inline void assertCharIsIn8BitRange(T c)
{
    UNUSED_PARAM(c);
    ASSERT(c >= 0);
    ASSERT(c <= 0xFF);
}

template <>
inline void assertCharIsIn8BitRange(UChar c)
{
    UNUSED_PARAM(c);
    ASSERT(c <= 0xFF);
}

template <>
inline void assertCharIsIn8BitRange(LChar)
{
}

template <typename T>
inline void Lexer<T>::append8(const T* p, size_t length)
{
    // FIXME: Change three occurrances of m_buffer16 to m_buffer8 and
    // UChar to LChar when 8 bit strings are turned on.
    size_t currentSize = m_buffer16.size();
    m_buffer16.grow(currentSize + length);
    UChar* rawBuffer = m_buffer16.data() + currentSize;

    for (size_t i = 0; i < length; i++) {
        T c = p[i];
        assertCharIsIn8BitRange(c);
        rawBuffer[i] = c;
    }
}

template <typename T>
inline void Lexer<T>::append16(const LChar* p, size_t length)
{
    size_t currentSize = m_buffer16.size();
    m_buffer16.grow(currentSize + length);
    UChar* rawBuffer = m_buffer16.data() + currentSize;

    for (size_t i = 0; i < length; i++)
        rawBuffer[i] = p[i];
}

template <typename T>
inline void Lexer<T>::record16(T c)
{
    m_buffer16.append(c);
}

template <typename T>
inline void Lexer<T>::record16(int c)
{
    ASSERT(c >= 0);
    ASSERT(c <= USHRT_MAX);
    m_buffer16.append(static_cast<UChar>(c));
}

template <>
    template <bool shouldCreateIdentifier> ALWAYS_INLINE JSTokenType Lexer<LChar>::parseIdentifier(JSTokenData* tokenData, unsigned lexerFlags, bool strictMode)
{
    const ptrdiff_t remaining = m_codeEnd - m_code;
    if ((remaining >= maxTokenLength) && !(lexerFlags & LexerFlagsIgnoreReservedWords)) {
        JSTokenType keyword = parseKeyword<shouldCreateIdentifier>(tokenData);
        if (keyword != IDENT) {
            ASSERT((!shouldCreateIdentifier) || tokenData->ident);
            return keyword == RESERVED_IF_STRICT && !strictMode ? IDENT : keyword;
        }
    }

    const LChar* identifierStart = currentCharacter();
    
    while (isIdentPart(m_current))
        shift();
    
    if (UNLIKELY(m_current == '\\')) {
        setOffsetFromCharOffset(identifierStart);
        return parseIdentifierSlowCase<shouldCreateIdentifier>(tokenData, lexerFlags, strictMode);
    }

    const Identifier* ident = 0;
    
    if (shouldCreateIdentifier) {
        int identifierLength = currentCharacter() - identifierStart;
        ident = makeIdentifier(identifierStart, identifierLength);

        tokenData->ident = ident;
    } else
        tokenData->ident = 0;

    m_delimited = false;

    if (UNLIKELY((remaining < maxTokenLength) && !(lexerFlags & LexerFlagsIgnoreReservedWords))) {
        ASSERT(shouldCreateIdentifier);
        if (remaining < maxTokenLength) {
            const HashEntry* entry = m_globalData->keywords->getKeyword(*ident);
            ASSERT((remaining < maxTokenLength) || !entry);
            if (!entry)
                return IDENT;
            JSTokenType token = static_cast<JSTokenType>(entry->lexerValue());
            return (token != RESERVED_IF_STRICT) || strictMode ? token : IDENT;
        }
        return IDENT;
    }

    return IDENT;
}

template <>
template <bool shouldCreateIdentifier> ALWAYS_INLINE JSTokenType Lexer<UChar>::parseIdentifier(JSTokenData* tokenData, unsigned lexerFlags, bool strictMode)
{
    const ptrdiff_t remaining = m_codeEnd - m_code;
    if ((remaining >= maxTokenLength) && !(lexerFlags & LexerFlagsIgnoreReservedWords)) {
        JSTokenType keyword = parseKeyword<shouldCreateIdentifier>(tokenData);
        if (keyword != IDENT) {
            ASSERT((!shouldCreateIdentifier) || tokenData->ident);
            return keyword == RESERVED_IF_STRICT && !strictMode ? IDENT : keyword;
        }
    }
    const UChar* identifierStart = currentCharacter();

    UChar orAllChars = 0;
    
    while (isIdentPart(m_current)) {
        orAllChars |= m_current;
        shift();
    }
    
    if (UNLIKELY(m_current == '\\')) {
        setOffsetFromCharOffset(identifierStart);
        return parseIdentifierSlowCase<shouldCreateIdentifier>(tokenData, lexerFlags, strictMode);
    }

    bool isAll8Bit = false;

#if 0 // FIXME: Remove this #if when 8 bit strings are turned on.
    if (!(orAllChars & ~0xff))
        isAll8Bit = true;
#endif

    const Identifier* ident = 0;
    
    if (shouldCreateIdentifier) {
        int identifierLength = currentCharacter() - identifierStart;
        if (isAll8Bit)
            ident = makeIdentifierLCharFromUChar(identifierStart, identifierLength);
        else
            ident = makeIdentifier(identifierStart, identifierLength);
        
        tokenData->ident = ident;
    } else
        tokenData->ident = 0;
    
    m_delimited = false;
    
    if (UNLIKELY((remaining < maxTokenLength) && !(lexerFlags & LexerFlagsIgnoreReservedWords))) {
        ASSERT(shouldCreateIdentifier);
        if (remaining < maxTokenLength) {
            const HashEntry* entry = m_globalData->keywords->getKeyword(*ident);
            ASSERT((remaining < maxTokenLength) || !entry);
            if (!entry)
                return IDENT;
            JSTokenType token = static_cast<JSTokenType>(entry->lexerValue());
            return (token != RESERVED_IF_STRICT) || strictMode ? token : IDENT;
        }
        return IDENT;
    }

    return IDENT;
}

template <typename T>
template <bool shouldCreateIdentifier> JSTokenType Lexer<T>::parseIdentifierSlowCase(JSTokenData* tokenData, unsigned lexerFlags, bool strictMode)
{
    const ptrdiff_t remaining = m_codeEnd - m_code;
    const T* identifierStart = currentCharacter();
    bool bufferRequired = false;

    while (true) {
        if (LIKELY(isIdentPart(m_current))) {
            shift();
            continue;
        }
        if (LIKELY(m_current != '\\'))
            break;

        // \uXXXX unicode characters.
        bufferRequired = true;
        if (identifierStart != currentCharacter())
            m_buffer16.append(identifierStart, currentCharacter() - identifierStart);
        shift();
        if (UNLIKELY(m_current != 'u'))
            return ERRORTOK;
        shift();
        int character = getUnicodeCharacter();
        if (UNLIKELY(character == -1))
            return ERRORTOK;
        if (UNLIKELY(m_buffer16.size() ? !isIdentPart(character) : !isIdentStart(character)))
            return ERRORTOK;
        if (shouldCreateIdentifier)
            record16(character);
        identifierStart = currentCharacter();
    }

    int identifierLength;
    const Identifier* ident = 0;
    if (shouldCreateIdentifier) {
        if (!bufferRequired) {
            identifierLength = currentCharacter() - identifierStart;
            ident = makeIdentifier(identifierStart, identifierLength);
        } else {
            if (identifierStart != currentCharacter())
                m_buffer16.append(identifierStart, currentCharacter() - identifierStart);
            ident = makeIdentifier(m_buffer16.data(), m_buffer16.size());
        }

        tokenData->ident = ident;
    } else
        tokenData->ident = 0;

    m_delimited = false;

    if (LIKELY(!bufferRequired && !(lexerFlags & LexerFlagsIgnoreReservedWords))) {
        ASSERT(shouldCreateIdentifier);
        // Keywords must not be recognized if there was an \uXXXX in the identifier.
        if (remaining < maxTokenLength) {
            const HashEntry* entry = m_globalData->keywords->getKeyword(*ident);
            ASSERT((remaining < maxTokenLength) || !entry);
            if (!entry)
                return IDENT;
            JSTokenType token = static_cast<JSTokenType>(entry->lexerValue());
            return (token != RESERVED_IF_STRICT) || strictMode ? token : IDENT;
        }
        return IDENT;
    }

    m_buffer16.resize(0);
    return IDENT;
}

template <typename T>
template <bool shouldBuildStrings> ALWAYS_INLINE bool Lexer<T>::parseString(JSTokenData* tokenData, bool strictMode)
{
    // FIXME: Change record16 and m_buffer16 to record8 and m_buffer8 below when
    // 8 bit strings are turned on.
    int startingOffset = currentOffset();
    int startingLineNumber = lineNumber();
    int stringQuoteCharacter = m_current;
    shift();

    const T* stringStart = currentCharacter();

    while (m_current != stringQuoteCharacter) {
        if (UNLIKELY((m_current == '\\'))) {
            if (stringStart != currentCharacter() && shouldBuildStrings)
                append8(stringStart, currentCharacter() - stringStart);
            shift();

            int escape = singleEscape(m_current);

            // Most common escape sequences first
            if (escape) {
                if (shouldBuildStrings)
                    record16(escape); // FIXME: Change to record8
                shift();
            } else if (UNLIKELY(isLineTerminator(m_current)))
                shiftLineTerminator();
            else if (m_current == 'x') {
                shift();
                if (isASCIIHexDigit(m_current) && isASCIIHexDigit(peek(1))) {
                    int prev = m_current;
                    shift();
                    if (shouldBuildStrings)
                        record16(convertHex(prev, m_current)); // FIXME: Change to record8
                    shift();
                } else if (shouldBuildStrings)
                    record16('x'); // FIXME: Change to record8
            } else {
                setOffset(startingOffset);
                setLineNumber(startingLineNumber);
                m_buffer16.resize(0); // FIXME: Change to m_buffer8
                return parseStringSlowCase<shouldBuildStrings>(tokenData, strictMode);
            }
            stringStart = currentCharacter();
            continue;
        }

        if (UNLIKELY(((m_current > 0xff) || (m_current < 0xe)))) {
            setOffset(startingOffset);
            setLineNumber(startingLineNumber);
            m_buffer16.resize(0); // FIXME: Change to m_buffer8
            return parseStringSlowCase<shouldBuildStrings>(tokenData, strictMode);
        }

        shift();
    }

    if (currentCharacter() != stringStart && shouldBuildStrings)
        append8(stringStart, currentCharacter() - stringStart);
    if (shouldBuildStrings) {
        // FIXME: Change to m_buffer8
        tokenData->ident = makeIdentifier(m_buffer16.data(), m_buffer16.size());
        // FIXME: Change to m_buffer8
        m_buffer16.resize(0);
    } else
        tokenData->ident = 0;

    return true;
}

template <typename T>
template <bool shouldBuildStrings> bool Lexer<T>::parseStringSlowCase(JSTokenData* tokenData, bool strictMode)
{
    int stringQuoteCharacter = m_current;
    shift();

    const T* stringStart = currentCharacter();

    while (m_current != stringQuoteCharacter) {
        if (UNLIKELY(m_current == '\\')) {
            if (stringStart != currentCharacter() && shouldBuildStrings)
                append16(stringStart, currentCharacter() - stringStart);
            shift();

            int escape = singleEscape(m_current);

            // Most common escape sequences first
            if (escape) {
                if (shouldBuildStrings)
                    record16(escape);
                shift();
            } else if (UNLIKELY(isLineTerminator(m_current)))
                shiftLineTerminator();
            else if (m_current == 'x') {
                shift();
                if (isASCIIHexDigit(m_current) && isASCIIHexDigit(peek(1))) {
                    int prev = m_current;
                    shift();
                    if (shouldBuildStrings)
                        record16(convertHex(prev, m_current));
                    shift();
                } else if (shouldBuildStrings)
                    record16('x');
            } else if (m_current == 'u') {
                shift();
                int character = getUnicodeCharacter();
                if (character != -1) {
                    if (shouldBuildStrings)
                        record16(character);
                } else if (m_current == stringQuoteCharacter) {
                    if (shouldBuildStrings)
                        record16('u');
                } else {
                    m_lexErrorMessage = "\\u can only be followed by a Unicode character sequence";
                    return false;
                }
            } else if (strictMode && isASCIIDigit(m_current)) {
                // The only valid numeric escape in strict mode is '\0', and this must not be followed by a decimal digit.
                int character1 = m_current;
                shift();
                if (character1 != '0' || isASCIIDigit(m_current)) {
                    m_lexErrorMessage = "The only valid numeric escape in strict mode is '\\0'";
                    return false;
                }
                if (shouldBuildStrings)
                    record16(0);
            } else if (!strictMode && isASCIIOctalDigit(m_current)) {
                // Octal character sequences
                int character1 = m_current;
                shift();
                if (isASCIIOctalDigit(m_current)) {
                    // Two octal characters
                    int character2 = m_current;
                    shift();
                    if (character1 >= '0' && character1 <= '3' && isASCIIOctalDigit(m_current)) {
                        if (shouldBuildStrings)
                            record16((character1 - '0') * 64 + (character2 - '0') * 8 + m_current - '0');
                        shift();
                    } else {
                        if (shouldBuildStrings)
                            record16((character1 - '0') * 8 + character2 - '0');
                    }
                } else {
                    if (shouldBuildStrings)
                        record16(character1 - '0');
                }
            } else if (m_current != -1) {
                if (shouldBuildStrings)
                    record16(m_current);
                shift();
            } else {
                m_lexErrorMessage = "Unterminated string constant";
                return false;
            }

            stringStart = currentCharacter();
            continue;
        }
        // Fast check for characters that require special handling.
        // Catches -1, \n, \r, 0x2028, and 0x2029 as efficiently
        // as possible, and lets through all common ASCII characters.
        if (UNLIKELY(((static_cast<unsigned>(m_current) - 0xE) & 0x2000))) {
            // New-line or end of input is not allowed
            if (UNLIKELY(isLineTerminator(m_current)) || UNLIKELY(m_current == -1)) {
                m_lexErrorMessage = "Unexpected EOF";
                return false;
            }
            // Anything else is just a normal character
        }
        shift();
    }

    if (currentCharacter() != stringStart && shouldBuildStrings)
        append16(stringStart, currentCharacter() - stringStart);
    if (shouldBuildStrings)
        tokenData->ident = makeIdentifier(m_buffer16.data(), m_buffer16.size());
    else
        tokenData->ident = 0;

    m_buffer16.resize(0);
    return true;
}

template <typename T>
ALWAYS_INLINE void Lexer<T>::parseHex(double& returnValue)
{
    // Optimization: most hexadecimal values fit into 4 bytes.
    uint32_t hexValue = 0;
    int maximumDigits = 7;

    // Shift out the 'x' prefix.
    shift();

    do {
        hexValue = (hexValue << 4) + toASCIIHexValue(m_current);
        shift();
        --maximumDigits;
    } while (isASCIIHexDigit(m_current) && maximumDigits >= 0);

    if (maximumDigits >= 0) {
        returnValue = hexValue;
        return;
    }

    // No more place in the hexValue buffer.
    // The values are shifted out and placed into the m_buffer8 vector.
    for (int i = 0; i < 8; ++i) {
         int digit = hexValue >> 28;
         if (digit < 10)
             record8(digit + '0');
         else
             record8(digit - 10 + 'a');
         hexValue <<= 4;
    }

    while (isASCIIHexDigit(m_current)) {
        record8(m_current);
        shift();
    }

    returnValue = parseIntOverflow(m_buffer8.data(), m_buffer8.size(), 16);
}

template <typename T>
ALWAYS_INLINE bool Lexer<T>::parseOctal(double& returnValue)
{
    // Optimization: most octal values fit into 4 bytes.
    uint32_t octalValue = 0;
    int maximumDigits = 9;
    // Temporary buffer for the digits. Makes easier
    // to reconstruct the input characters when needed.
    LChar digits[10];

    do {
        octalValue = octalValue * 8 + (m_current - '0');
        digits[maximumDigits] = m_current;
        shift();
        --maximumDigits;
    } while (isASCIIOctalDigit(m_current) && maximumDigits >= 0);

    if (!isASCIIDigit(m_current) && maximumDigits >= 0) {
        returnValue = octalValue;
        return true;
    }

    for (int i = 9; i > maximumDigits; --i)
         record8(digits[i]);

    while (isASCIIOctalDigit(m_current)) {
        record8(m_current);
        shift();
    }

    if (isASCIIDigit(m_current))
        return false;

    returnValue = parseIntOverflow(m_buffer8.data(), m_buffer8.size(), 8);
    return true;
}

template <typename T>
ALWAYS_INLINE bool Lexer<T>::parseDecimal(double& returnValue)
{
    // Optimization: most decimal values fit into 4 bytes.
    uint32_t decimalValue = 0;

    // Since parseOctal may be executed before parseDecimal,
    // the m_buffer8 may hold ascii digits.
    if (!m_buffer8.size()) {
        int maximumDigits = 9;
        // Temporary buffer for the digits. Makes easier
        // to reconstruct the input characters when needed.
        LChar digits[10];

        do {
            decimalValue = decimalValue * 10 + (m_current - '0');
            digits[maximumDigits] = m_current;
            shift();
            --maximumDigits;
        } while (isASCIIDigit(m_current) && maximumDigits >= 0);

        if (maximumDigits >= 0 && m_current != '.' && (m_current | 0x20) != 'e') {
            returnValue = decimalValue;
            return true;
        }

        for (int i = 9; i > maximumDigits; --i)
            record8(digits[i]);
    }

    while (isASCIIDigit(m_current)) {
        record8(m_current);
        shift();
    }

    return false;
}

template <typename T>
ALWAYS_INLINE void Lexer<T>::parseNumberAfterDecimalPoint()
{
    record8('.');
    while (isASCIIDigit(m_current)) {
        record8(m_current);
        shift();
    }
}

template <typename T>
ALWAYS_INLINE bool Lexer<T>::parseNumberAfterExponentIndicator()
{
    record8('e');
    shift();
    if (m_current == '+' || m_current == '-') {
        record8(m_current);
        shift();
    }

    if (!isASCIIDigit(m_current))
        return false;

    do {
        record8(m_current);
        shift();
    } while (isASCIIDigit(m_current));
    return true;
}

template <typename T>
ALWAYS_INLINE bool Lexer<T>::parseMultilineComment()
{
    while (true) {
        while (UNLIKELY(m_current == '*')) {
            shift();
            if (m_current == '/') {
                shift();
                return true;
            }
        }

        if (UNLIKELY(m_current == -1))
            return false;

        if (isLineTerminator(m_current)) {
            shiftLineTerminator();
            m_terminator = true;
        } else
            shift();
    }
}

template <typename T>
bool Lexer<T>::nextTokenIsColon()
{
    const T* code = m_code;
    while (code < m_codeEnd && (isWhiteSpace(*code) || isLineTerminator(*code)))
        code++;
    
    return code < m_codeEnd && *code == ':';
}

template <typename T>
JSTokenType Lexer<T>::lex(JSTokenData* tokenData, JSTokenInfo* tokenInfo, unsigned lexerFlags, bool strictMode)
{
    ASSERT(!m_error);
    ASSERT(m_buffer8.isEmpty());
    ASSERT(m_buffer16.isEmpty());

    JSTokenType token = ERRORTOK;
    m_terminator = false;

start:
    while (isWhiteSpace(m_current))
        shift();

    int startOffset = currentOffset();

    if (UNLIKELY(m_current == -1))
        return EOFTOK;

    m_delimited = false;

    CharacterType type;
    if (LIKELY(isASCII(m_current)))
        type = static_cast<CharacterType>(typesOfASCIICharacters[m_current]);
    else if (isNonASCIIIdentStart(m_current))
        type = CharacterIdentifierStart;
    else if (isLineTerminator(m_current))
        type = CharacterLineTerminator;
    else
        type = CharacterInvalid;

    switch (type) {
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
        token = GT;
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
        token = EQUAL;
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
        token = LT;
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
        token = EXCLAMATION;
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
        token = PLUS;
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
        token = MINUS;
        break;
    case CharacterMultiply:
        shift();
        if (m_current == '=') {
            shift();
            token = MULTEQUAL;
            break;
        }
        token = TIMES;
        break;
    case CharacterSlash:
        shift();
        if (m_current == '/') {
            shift();
            goto inSingleLineComment;
        }
        if (m_current == '*') {
            shift();
            if (parseMultilineComment())
                goto start;
            m_lexErrorMessage = "Multiline comment was not closed properly";
            goto returnError;
        }
        if (m_current == '=') {
            shift();
            token = DIVEQUAL;
            break;
        }
        token = DIVIDE;
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
        token = BITAND;
        break;
    case CharacterXor:
        shift();
        if (m_current == '=') {
            shift();
            token = XOREQUAL;
            break;
        }
        token = BITXOR;
        break;
    case CharacterModulo:
        shift();
        if (m_current == '=') {
            shift();
            token = MODEQUAL;
            break;
        }
        token = MOD;
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
        token = BITOR;
        break;
    case CharacterOpenParen:
        token = OPENPAREN;
        shift();
        break;
    case CharacterCloseParen:
        token = CLOSEPAREN;
        shift();
        break;
    case CharacterOpenBracket:
        token = OPENBRACKET;
        shift();
        break;
    case CharacterCloseBracket:
        token = CLOSEBRACKET;
        shift();
        break;
    case CharacterComma:
        token = COMMA;
        shift();
        break;
    case CharacterColon:
        token = COLON;
        shift();
        break;
    case CharacterQuestion:
        token = QUESTION;
        shift();
        break;
    case CharacterTilde:
        token = TILDE;
        shift();
        break;
    case CharacterSemicolon:
        m_delimited = true;
        shift();
        token = SEMICOLON;
        break;
    case CharacterOpenBrace:
        tokenData->intValue = currentOffset();
        shift();
        token = OPENBRACE;
        break;
    case CharacterCloseBrace:
        tokenData->intValue = currentOffset();
        m_delimited = true;
        shift();
        token = CLOSEBRACE;
        break;
    case CharacterDot:
        shift();
        if (!isASCIIDigit(m_current)) {
            token = DOT;
            break;
        }
        goto inNumberAfterDecimalPoint;
    case CharacterZero:
        shift();
        if ((m_current | 0x20) == 'x' && isASCIIHexDigit(peek(1))) {
            parseHex(tokenData->doubleValue);
            token = NUMBER;
        } else {
            record8('0');
            if (isASCIIOctalDigit(m_current)) {
                if (parseOctal(tokenData->doubleValue)) {
                    if (strictMode) {
                        m_lexErrorMessage = "Octal escapes are forbidden in strict mode";
                        goto returnError;
                    }
                    token = NUMBER;
                }
            }
        }
        // Fall through into CharacterNumber
    case CharacterNumber:
        if (LIKELY(token != NUMBER)) {
            if (!parseDecimal(tokenData->doubleValue)) {
                if (m_current == '.') {
                    shift();
inNumberAfterDecimalPoint:
                    parseNumberAfterDecimalPoint();
                }
                if ((m_current | 0x20) == 'e')
                    if (!parseNumberAfterExponentIndicator()) {
                        m_lexErrorMessage = "Non-number found after exponent indicator";
                        goto returnError;
                    }
                // Null-terminate string for strtod.
                m_buffer8.append('\0');
                tokenData->doubleValue = WTF::strtod(reinterpret_cast<const char*>(m_buffer8.data()), 0);
            }
            token = NUMBER;
        }

        // No identifiers allowed directly after numeric literal, e.g. "3in" is bad.
        if (UNLIKELY(isIdentStart(m_current))) {
            m_lexErrorMessage = "At least one digit must occur after a decimal point";
            goto returnError;
        }
        m_buffer8.resize(0);
        m_delimited = false;
        break;
    case CharacterQuote:
        if (lexerFlags & LexerFlagsDontBuildStrings) {
            if (UNLIKELY(!parseString<false>(tokenData, strictMode)))
                goto returnError;
        } else {
            if (UNLIKELY(!parseString<true>(tokenData, strictMode)))
                goto returnError;
        }
        shift();
        m_delimited = false;
        token = STRING;
        break;
    case CharacterIdentifierStart:
        ASSERT(isIdentStart(m_current));
        // Fall through into CharacterBackSlash.
    case CharacterBackSlash:
        if (lexerFlags & LexexFlagsDontBuildKeywords)
            token = parseIdentifier<false>(tokenData, lexerFlags, strictMode);
        else
            token = parseIdentifier<true>(tokenData, lexerFlags, strictMode);
        break;
    case CharacterLineTerminator:
        ASSERT(isLineTerminator(m_current));
        shiftLineTerminator();
        m_atLineStart = true;
        m_terminator = true;
        goto start;
    case CharacterInvalid:
        m_lexErrorMessage = getInvalidCharMessage();
        goto returnError;
    default:
        ASSERT_NOT_REACHED();
        m_lexErrorMessage = "Internal Error";
        goto returnError;
    }

    m_atLineStart = false;
    goto returnToken;

inSingleLineComment:
    while (!isLineTerminator(m_current)) {
        if (UNLIKELY(m_current == -1))
            return EOFTOK;
        shift();
    }
    shiftLineTerminator();
    m_atLineStart = true;
    m_terminator = true;
    if (!lastTokenWasRestrKeyword())
        goto start;

    token = SEMICOLON;
    m_delimited = true;
    // Fall through into returnToken.

returnToken:
    tokenInfo->line = m_lineNumber;
    tokenInfo->startOffset = startOffset;
    tokenInfo->endOffset = currentOffset();
    m_lastToken = token;
    return token;

returnError:
    m_error = true;
    tokenInfo->line = m_lineNumber;
    tokenInfo->startOffset = startOffset;
    tokenInfo->endOffset = currentOffset();
    return ERRORTOK;
}

template <typename T>
bool Lexer<T>::scanRegExp(const Identifier*& pattern, const Identifier*& flags, UChar patternPrefix)
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

template <typename T>
bool Lexer<T>::skipRegExp()
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

template <typename T>
void Lexer<T>::clear()
{
    m_arena = 0;

    Vector<LChar> newBuffer8;
    m_buffer8.swap(newBuffer8);

    Vector<UChar> newBuffer16;
    m_buffer16.swap(newBuffer16);

    m_isReparsing = false;
}

template <typename T>
SourceCode Lexer<T>::sourceCode(int openBrace, int closeBrace, int firstLine)
{
    ASSERT((*m_source->provider()->data())[openBrace] == '{');
    ASSERT((*m_source->provider()->data())[closeBrace] == '}');
    return SourceCode(m_source->provider(), openBrace, closeBrace + 1, firstLine);
}

// Instantiate the two flavors of Lexer we need instead of putting most of this file in Lexer.h
template class Lexer<LChar>;
template class Lexer<UChar>;

} // namespace JSC
