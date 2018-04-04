/*
 * Copyright (C) 2013, 2018 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ParserTokens.h"
#include <wtf/text/WTFString.h>

namespace JSC {

class ParserError {
public:
    enum SyntaxErrorType {
        SyntaxErrorNone,
        SyntaxErrorIrrecoverable,
        SyntaxErrorUnterminatedLiteral,
        SyntaxErrorRecoverable
    };

    enum ErrorType {
        ErrorNone,
        StackOverflow,
        EvalError,
        OutOfMemory,
        SyntaxError
    };

    ParserError()
        : m_type(ErrorNone)
        , m_syntaxErrorType(SyntaxErrorNone)
    {
    }
    
    explicit ParserError(ErrorType type)
        : m_type(type)
        , m_syntaxErrorType(SyntaxErrorNone)
    {
    }

    ParserError(ErrorType type, SyntaxErrorType syntaxError, JSToken token)
        : m_token(token)
        , m_type(type)
        , m_syntaxErrorType(syntaxError)
    {
    }

    ParserError(ErrorType type, SyntaxErrorType syntaxError, JSToken token, const String& msg, int line)
        : m_token(token)
        , m_message(msg)
        , m_line(line)
        , m_type(type)
        , m_syntaxErrorType(syntaxError)
    {
    }

    bool isValid() const { return m_type != ErrorNone; }
    SyntaxErrorType syntaxErrorType() const { return m_syntaxErrorType; }
    const JSToken& token() const { return m_token; }
    const String& message() const { return m_message; }
    int line() const { return m_line; }

    JSObject* toErrorObject(JSGlobalObject*, const SourceCode&, int overrideLineNumber = -1);
    JS_EXPORT_PRIVATE JSObject* throwStackOverflowOrOutOfMemory(ExecState* = nullptr);

private:
    JSToken m_token;
    String m_message;
    int m_line { -1 };
    ErrorType m_type;
    SyntaxErrorType m_syntaxErrorType;
};

} // namespace JSC

namespace WTF {
void printInternal(PrintStream&, JSC::ParserError::SyntaxErrorType);
void printInternal(PrintStream&, JSC::ParserError::ErrorType);
} // namespace WTF
