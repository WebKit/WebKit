/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSParser_h
#define JSParser_h

namespace JSC {

class Identifier;
class JSGlobalData;

enum JSTokenType {
    NULLTOKEN = 258,
    TRUETOKEN = 259,
    FALSETOKEN = 260,
    BREAK = 261,
    CASE = 262,
    DEFAULT = 263,
    FOR = 264,
    NEW = 265,
    VAR = 266,
    CONSTTOKEN = 267,
    CONTINUE = 268,
    FUNCTION = 269,
    RETURN = 270,
    VOIDTOKEN = 271,
    DELETETOKEN = 272,
    IF = 273,
    THISTOKEN = 274,
    DO = 275,
    WHILE = 276,
    INTOKEN = 277,
    INSTANCEOF = 278,
    TYPEOF = 279,
    SWITCH = 280,
    WITH = 281,
    RESERVED = 282,
    THROW = 283,
    TRY = 284,
    CATCH = 285,
    FINALLY = 286,
    DEBUGGER = 287,
    IF_WITHOUT_ELSE = 288,
    ELSE = 289,
    EQEQ = 290,
    NE = 291,
    STREQ = 292,
    STRNEQ = 293,
    LE = 294,
    GE = 295,
    OR = 296,
    AND = 297,
    PLUSPLUS = 298,
    MINUSMINUS = 299,
    LSHIFT = 300,
    RSHIFT = 301,
    URSHIFT = 302,
    PLUSEQUAL = 303,
    MINUSEQUAL = 304,
    MULTEQUAL = 305,
    DIVEQUAL = 306,
    LSHIFTEQUAL = 307,
    RSHIFTEQUAL = 308,
    URSHIFTEQUAL = 309,
    ANDEQUAL = 310,
    MODEQUAL = 311,
    XOREQUAL = 312,
    OREQUAL = 313,
    OPENBRACE = 314,
    CLOSEBRACE = 315,
    NUMBER = 316,
    IDENT = 317,
    STRING = 318,
    AUTOPLUSPLUS = 319,
    AUTOMINUSMINUS = 320
};

union JSTokenData {
    int intValue;
    double doubleValue;
    const Identifier* ident;
};
typedef JSTokenData YYSTYPE;

struct JSTokenInfo {
    JSTokenInfo() : last_line(0) {}
    int first_line;
    int last_line;
    int first_column;
    int last_column;
};
typedef JSTokenInfo YYLTYPE;
struct JSToken {
    int m_type;
    JSTokenData m_data;
    JSTokenInfo m_info;
};

int jsParse(JSGlobalData*);
}
#endif // JSParser_h
