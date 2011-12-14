/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef Lexer_h
#define Lexer_h

#include "JSParser.h"
#include "Lookup.h"
#include "ParserArena.h"
#include "SourceCode.h"
#include <wtf/ASCIICType.h>
#include <wtf/AlwaysInline.h>
#include <wtf/SegmentedVector.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

namespace JSC {

    class RegExp;

    class Lexer {
        WTF_MAKE_NONCOPYABLE(Lexer); WTF_MAKE_FAST_ALLOCATED;
    public:
        // Character manipulation functions.
        static bool isWhiteSpace(int character);
        static bool isLineTerminator(int character);
        static unsigned char convertHex(int c1, int c2);
        static UChar convertUnicode(int c1, int c2, int c3, int c4);

        // Functions to set up parsing.
        void setCode(const SourceCode&, ParserArena&);
        void setIsReparsing() { m_isReparsing = true; }
        bool isReparsing() const { return m_isReparsing; }

        // Functions for the parser itself.
        enum LexType {
            IgnoreReservedWords = 1, 
            DontBuildStrings = 2,
            DontBuildKeywords = 4
        };
        JSTokenType lex(JSTokenData*, JSTokenInfo*, unsigned, bool strictMode);
        bool nextTokenIsColon();
        int lineNumber() const { return m_lineNumber; }
        void setLastLineNumber(int lastLineNumber) { m_lastLineNumber = lastLineNumber; }
        int lastLineNumber() const { return m_lastLineNumber; }
        bool prevTerminator() const { return m_terminator; }
        SourceCode sourceCode(int openBrace, int closeBrace, int firstLine);
        bool scanRegExp(const Identifier*& pattern, const Identifier*& flags, UChar patternPrefix = 0);
        bool skipRegExp();

        // Functions for use after parsing.
        bool sawError() const { return m_error; }
        UString getErrorMessage() const { return m_lexErrorMessage; }
        void clear();
        int currentOffset() { return m_code - m_codeStart; }
        void setOffset(int offset)
        {
            m_error = 0;
            m_lexErrorMessage = UString();
            m_code = m_codeStart + offset;
            m_buffer8.resize(0);
            m_buffer16.resize(0);
            // Faster than an if-else sequence
            m_current = -1;
            if (LIKELY(m_code < m_codeEnd))
                m_current = *m_code;
        }
        void setLineNumber(int line)
        {
            m_lineNumber = line;
        }

        SourceProvider* sourceProvider() const { return m_source->provider(); }
        
        JSTokenType lexExpectIdentifier(JSTokenData*, JSTokenInfo*, unsigned, bool strictMode);

        bool isKeyword(const Identifier&);

    private:
        friend class JSGlobalData;

        Lexer(JSGlobalData*);
        ~Lexer();

        void record8(int);
        void record16(int);
        void record16(UChar);

        void copyCodeWithoutBOMs();

        ALWAYS_INLINE void shift();
        ALWAYS_INLINE int peek(int offset);
        int getUnicodeCharacter();
        void shiftLineTerminator();

        UString getInvalidCharMessage();
        ALWAYS_INLINE const UChar* currentCharacter() const;
        ALWAYS_INLINE int currentOffset() const;

        ALWAYS_INLINE const Identifier* makeIdentifier(const UChar* characters, size_t length);

        ALWAYS_INLINE bool lastTokenWasRestrKeyword() const;

        enum ShiftType { DoBoundsCheck, DoNotBoundsCheck };
        template <int shiftAmount, ShiftType shouldBoundsCheck> void internalShift();
        template <bool shouldCreateIdentifier> ALWAYS_INLINE JSTokenType parseKeyword(JSTokenData*);
        template <bool shouldBuildIdentifiers> ALWAYS_INLINE JSTokenType parseIdentifier(JSTokenData*, unsigned, bool strictMode);
        template <bool shouldBuildStrings> ALWAYS_INLINE bool parseString(JSTokenData*, bool strictMode);
        ALWAYS_INLINE void parseHex(double& returnValue);
        ALWAYS_INLINE bool parseOctal(double& returnValue);
        ALWAYS_INLINE bool parseDecimal(double& returnValue);
        ALWAYS_INLINE void parseNumberAfterDecimalPoint();
        ALWAYS_INLINE bool parseNumberAfterExponentIndicator();
        ALWAYS_INLINE bool parseMultilineComment();

        static const size_t initialReadBufferCapacity = 32;

        int m_lineNumber;
        int m_lastLineNumber;

        Vector<char> m_buffer8;
        Vector<UChar> m_buffer16;
        bool m_terminator;
        bool m_delimited; // encountered delimiter like "'" and "}" on last run
        int m_lastToken;

        const SourceCode* m_source;
        const UChar* m_code;
        const UChar* m_codeStart;
        const UChar* m_codeEnd;
        bool m_isReparsing;
        bool m_atLineStart;
        bool m_error;
        UString m_lexErrorMessage;

        // current and following unicode characters (int to allow for -1 for end-of-file marker)
        int m_current;

        IdentifierArena* m_arena;

        JSGlobalData* m_globalData;

        const HashTable m_keywordTable;
    };

    ALWAYS_INLINE bool Lexer::isWhiteSpace(int ch)
    {
        return isASCII(ch) ? (ch == ' ' || ch == '\t' || ch == 0xB || ch == 0xC) : (WTF::Unicode::isSeparatorSpace(ch) || ch == 0xFEFF);
    }

    ALWAYS_INLINE bool Lexer::isLineTerminator(int ch)
    {
        return ch == '\r' || ch == '\n' || (ch & ~1) == 0x2028;
    }

    inline unsigned char Lexer::convertHex(int c1, int c2)
    {
        return (toASCIIHexValue(c1) << 4) | toASCIIHexValue(c2);
    }

    inline UChar Lexer::convertUnicode(int c1, int c2, int c3, int c4)
    {
        return (convertHex(c1, c2) << 8) | convertHex(c3, c4);
    }
    
    ALWAYS_INLINE const Identifier* Lexer::makeIdentifier(const UChar* characters, size_t length)
    {
        return &m_arena->makeIdentifier(m_globalData, characters, length);
    }

    ALWAYS_INLINE JSTokenType Lexer::lexExpectIdentifier(JSTokenData* tokenData, JSTokenInfo* tokenInfo, unsigned lexType, bool strictMode)
    {
        ASSERT((lexType & IgnoreReservedWords));
        const UChar* start = m_code;
        const UChar* ptr = start;
        const UChar* end = m_codeEnd;
        if (ptr >= end) {
            ASSERT(ptr == end);
            goto slowCase;
        }
        if (!WTF::isASCIIAlpha(*ptr))
            goto slowCase;
        ++ptr;
        while (ptr < end) {
            if (!WTF::isASCIIAlphanumeric(*ptr))
                break;
            ++ptr;
        }

        // Here's the shift
        if (ptr < end) {
            if ((!WTF::isASCII(*ptr)) || (*ptr == '\\') || (*ptr == '_') || (*ptr == '$'))
                goto slowCase;
            m_current = *ptr;
        } else
            m_current = -1;

        m_code = ptr;

        // Create the identifier if needed
        if (lexType & DontBuildKeywords)
            tokenData->ident = 0;
        else
            tokenData->ident = makeIdentifier(start, ptr - start);
        tokenInfo->line = m_lineNumber;
        tokenInfo->startOffset = start - m_codeStart;
        tokenInfo->endOffset = currentOffset();
        m_lastToken = IDENT;
        return IDENT;
        
    slowCase:
        return lex(tokenData, tokenInfo, lexType, strictMode);
    }

} // namespace JSC

#endif // Lexer_h
