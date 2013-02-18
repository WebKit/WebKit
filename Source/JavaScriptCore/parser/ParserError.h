/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
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

#ifndef ParserError_h
#define ParserError_h

#include "Error.h"
#include "ExceptionHelpers.h"
#include <wtf/text/WTFString.h>

namespace JSC {

struct ParserError {
    enum ErrorType { ErrorNone, StackOverflow, SyntaxError, EvalError, OutOfMemory } m_type;
    String m_message;
    int m_line;
    ParserError()
        : m_type(ErrorNone)
        , m_line(-1)
    {
    }

    ParserError(ErrorType type)
        : m_type(type)
        , m_line(-1)
    {
    }

    ParserError(ErrorType type, String msg, int line)
        : m_type(type)
        , m_message(msg)
        , m_line(line)
    {
    }

    JSObject* toErrorObject(JSGlobalObject* globalObject, const SourceCode& source)
    {
        switch (m_type) {
        case ErrorNone:
            return 0;
        case SyntaxError:
            return addErrorInfo(globalObject->globalExec(), createSyntaxError(globalObject, m_message), m_line, source);
        case EvalError:
            return createSyntaxError(globalObject, m_message);
        case StackOverflow:
            return createStackOverflowError(globalObject);
        case OutOfMemory:
            return createOutOfMemoryError(globalObject);
        }
        CRASH();
        return createOutOfMemoryError(globalObject); // Appease Qt bot
    }
};

} // namespace JSC

#endif // ParserError_h
