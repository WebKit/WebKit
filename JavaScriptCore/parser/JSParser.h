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
class SourceCode;

enum JSTokenType {
    NULLTOKEN,
    TRUETOKEN,
    FALSETOKEN,
    BREAK,
    CASE,
    DEFAULT,
    FOR,
    NEW,
    VAR,
    CONSTTOKEN,
    CONTINUE,
    FUNCTION,
    RETURN,
    VOIDTOKEN,
    DELETETOKEN,
    IF,
    THISTOKEN,
    DO,
    WHILE,
    INTOKEN,
    INSTANCEOF,
    TYPEOF,
    SWITCH,
    WITH,
    RESERVED,
    THROW,
    TRY,
    CATCH,
    FINALLY,
    DEBUGGER,
    IF_WITHOUT_ELSE,
    ELSE,
    EQUAL,
    EQEQ,
    NE,
    STREQ,
    STRNEQ,
    LT,
    GT,
    LE,
    GE,
    OR,
    AND,
    PLUSPLUS,
    MINUSMINUS,
    LSHIFT,
    RSHIFT,
    URSHIFT,
    PLUSEQUAL,
    MINUSEQUAL,
    MULTEQUAL,
    DIVEQUAL,
    LSHIFTEQUAL,
    RSHIFTEQUAL,
    URSHIFTEQUAL,
    ANDEQUAL,
    MODEQUAL,
    XOREQUAL,
    OREQUAL,
    BITOR,
    BITAND,
    BITXOR,
    PLUS,
    MINUS,
    TIMES,
    DIVIDE,
    MOD,
    OPENBRACE,
    CLOSEBRACE,
    OPENPAREN,
    CLOSEPAREN,
    OPENBRACKET,
    CLOSEBRACKET,
    COMMA,
    QUESTION,
    TILDE,
    EXCLAMATION,
    NUMBER,
    IDENT,
    STRING,
    AUTOPLUSPLUS,
    AUTOMINUSMINUS,
    SEMICOLON,
    COLON,
    DOT,
    ERRORTOK,
    EOFTOK
};

union JSTokenData {
    int intValue;
    double doubleValue;
    const Identifier* ident;
};

struct JSTokenInfo {
    JSTokenInfo() : line(0) {}
    int line;
    int startOffset;
    int endOffset;
};

struct JSToken {
    JSTokenType m_type;
    JSTokenData m_data;
    JSTokenInfo m_info;
};

int jsParse(JSGlobalData*, const SourceCode*);
}
#endif // JSParser_h
