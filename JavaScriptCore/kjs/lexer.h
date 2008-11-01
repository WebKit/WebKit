/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007 Apple Inc.
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

#include "lookup.h"
#include "ustring.h"
#include <wtf/Vector.h>
#include "SourceCode.h"

namespace JSC {

    class Identifier;
    class RegExp;

    class Lexer : Noncopyable {
    public:
        void setCode(const SourceCode&);
        int lex(void* lvalp, void* llocp);

        int lineNo() const { return yylineno; }

        bool prevTerminator() const { return m_terminator; }

        enum State {
            Start,
            IdentifierOrKeyword,
            Identifier,
            InIdentifierOrKeyword,
            InIdentifier,
            InIdentifierStartUnicodeEscapeStart,
            InIdentifierStartUnicodeEscape,
            InIdentifierPartUnicodeEscapeStart,
            InIdentifierPartUnicodeEscape,
            InSingleLineComment,
            InMultiLineComment,
            InNum,
            InNum0,
            InHex,
            InOctal,
            InDecimal,
            InExponentIndicator,
            InExponent,
            Hex,
            Octal,
            Number,
            String,
            Eof,
            InString,
            InEscapeSequence,
            InHexEscape,
            InUnicodeEscape,
            Other,
            Bad
        };

        bool scanRegExp();
        const UString& pattern() const { return m_pattern; }
        const UString& flags() const { return m_flags; }

        static unsigned char convertHex(int);
        static unsigned char convertHex(int c1, int c2);
        static UChar convertUnicode(int c1, int c2, int c3, int c4);
        static bool isIdentStart(int);
        static bool isIdentPart(int);
        static bool isHexDigit(int);

        bool sawError() const { return m_error; }

        void clear();
        SourceCode sourceCode(int openBrace, int closeBrace, int firstLine) { return SourceCode(m_source->provider(), m_source->startOffset() + openBrace + 1, m_source->startOffset() + closeBrace, firstLine); }

    private:
        friend class JSGlobalData;
        Lexer(JSGlobalData*);
        ~Lexer();

        void setDone(State);
        void shift(unsigned int p);
        void nextLine();
        int lookupKeyword(const char *);

        bool isWhiteSpace() const;
        bool isLineTerminator();
        static bool isOctalDigit(int);

        int matchPunctuator(int& charPos, int c1, int c2, int c3, int c4);
        static unsigned short singleEscape(unsigned short);
        static unsigned short convertOctal(int c1, int c2, int c3);

        void record8(int);
        void record16(int);
        void record16(UChar);

        JSC::Identifier* makeIdentifier(const Vector<UChar>& buffer);

        int yylineno;
        int yycolumn;

        bool m_done;
        Vector<char> m_buffer8;
        Vector<UChar> m_buffer16;
        bool m_terminator;
        bool m_restrKeyword;
        bool m_delimited; // encountered delimiter like "'" and "}" on last run
        bool m_skipLF;
        bool m_skipCR;
        bool m_eatNextIdentifier;
        int m_stackToken;
        int m_lastToken;

        State m_state;
        unsigned int m_position;
        const SourceCode* m_source;
        const UChar* m_code;
        unsigned int m_length;
        int m_atLineStart;
        bool m_error;

        // current and following unicode characters (int to allow for -1 for end-of-file marker)
        int m_current;
        int m_next1;
        int m_next2;
        int m_next3;
        
        int m_currentOffset;
        int m_nextOffset1;
        int m_nextOffset2;
        int m_nextOffset3;
        
        Vector<UString*> m_strings;
        Vector<JSC::Identifier*> m_identifiers;

        JSGlobalData* m_globalData;

        UString m_pattern;
        UString m_flags;

        const HashTable m_mainTable;
    };

} // namespace JSC

#endif // Lexer_h
