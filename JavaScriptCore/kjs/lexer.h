// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef _KJSLEXER_H_
#define _KJSLEXER_H_

#include "ustring.h"


namespace KJS {

  class Identifier;

  class RegExp;

  class Lexer {
  public:
    Lexer();
    ~Lexer();
    static Lexer *curr();

    void setCode(const UString &sourceURL, int startingLineNumber, const UChar *c, unsigned int len);
    int lex();

    int lineNo() const { return yylineno; }
    UString sourceURL() const { return m_sourceURL; }

    bool prevTerminator() const { return terminator; }

    enum State { Start,
                 Identifier,
                 InIdentifier,
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
                 Bad };

    bool scanRegExp();
    UString pattern, flags;

  private:
    int yylineno;
    UString m_sourceURL;
    bool done;
    char *buffer8;
    UChar *buffer16;
    unsigned int size8, size16;
    unsigned int pos8, pos16;
    bool terminator;
    bool restrKeyword;
    // encountered delimiter like "'" and "}" on last run
    bool delimited;
    bool skipLF;
    bool skipCR;
    bool eatNextIdentifier;
    int stackToken;
    int lastToken;

    State state;
    void setDone(State s);
    unsigned int pos;
    void shift(unsigned int p);
    void nextLine();
    int lookupKeyword(const char *);

    bool isWhiteSpace() const;
    bool isLineTerminator();
    bool isOctalDigit(unsigned short c) const;

    int matchPunctuator(unsigned short c1, unsigned short c2,
                        unsigned short c3, unsigned short c4);
    unsigned short singleEscape(unsigned short c) const;
    unsigned short convertOctal(unsigned short c1, unsigned short c2,
                                unsigned short c3) const;
  public:
    static unsigned char convertHex(unsigned short c1);
    static unsigned char convertHex(unsigned short c1, unsigned short c2);
    static UChar convertUnicode(unsigned short c1, unsigned short c2,
                                unsigned short c3, unsigned short c4);
    static bool isIdentLetter(unsigned short c);
    static bool isDecimalDigit(unsigned short c);
    static bool isHexDigit(unsigned short c);

#ifdef KJS_DEBUG_MEM
    /**
     * Clear statically allocated resources
     */
    static void globalClear();
#endif

    bool sawError() const { return error; }
    void doneParsing();

  private:

    void record8(unsigned short c);
    void record16(UChar c);

    KJS::Identifier *makeIdentifier(UChar *buffer, unsigned int pos);
    UString *makeUString(UChar *buffer, unsigned int pos);

    const UChar *code;
    unsigned int length;
    int yycolumn;
#ifndef KJS_PURE_ECMA
    int bol;     // begin of line
#endif
    bool error;

    // current and following unicode characters
    unsigned short current, next1, next2, next3;

    UString **strings;
    unsigned int numStrings;
    unsigned int stringsCapacity;

    KJS::Identifier **identifiers;
    unsigned int numIdentifiers;
    unsigned int identifiersCapacity;

    // for future extensions
    class LexerPrivate;
    LexerPrivate *priv;
  };

} // namespace

#endif
