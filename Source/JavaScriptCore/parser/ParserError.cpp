/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "ParserError.h"

#include "ErrorHandlingScope.h"
#include "ExceptionHelpers.h"
#include "HeapCellInlines.h"
#include <wtf/text/WTFString.h>

namespace JSC {
JSObject* ParserError::toErrorObject(JSGlobalObject* globalObject, const SourceCode& source, int overrideLineNumber)
{
    ExecState* exec = globalObject->globalExec();
    switch (m_type) {
    case ErrorNone:
        return nullptr;
    case SyntaxError: {
        auto syntaxError = createSyntaxError(exec, m_message);
        auto line = overrideLineNumber == -1 ? m_line : overrideLineNumber;
        return addErrorInfo(exec, syntaxError, line, source);
    }
    case EvalError:
        return createSyntaxError(exec, m_message);
    case StackOverflow: {
        ErrorHandlingScope errorScope(globalObject->vm());
        return createStackOverflowError(exec);
    }
    case OutOfMemory:
        return createOutOfMemoryError(exec);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

JSObject* ParserError::throwStackOverflowOrOutOfMemory(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    switch (m_type) {
    case ErrorNone:
    case EvalError:
    case SyntaxError:
        RELEASE_ASSERT_NOT_REACHED();
    case StackOverflow:
        return throwStackOverflowError(exec, scope);
    case OutOfMemory:
        return throwOutOfMemoryError(exec, scope);
    }
    RELEASE_ASSERT_NOT_REACHED();
}
} // namespace JSC

namespace WTF {
void printInternal(PrintStream& out, JSC::ParserError::SyntaxErrorType type)
{
    switch (type) {
    case JSC::ParserError::SyntaxErrorNone:
        out.print("SyntaxErrorNone");
        return;
    case JSC::ParserError::SyntaxErrorIrrecoverable:
        out.print("SyntaxErrorIrrecoverable");
        return;
    case JSC::ParserError::SyntaxErrorUnterminatedLiteral:
        out.print("SyntaxErrorUnterminatedLiteral");
        return;
    case JSC::ParserError::SyntaxErrorRecoverable:
        out.print("SyntaxErrorRecoverable");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, JSC::ParserError::ErrorType type)
{
    switch (type) {
    case JSC::ParserError::ErrorNone:
        out.print("ErrorNone");
        return;
    case JSC::ParserError::StackOverflow:
        out.print("StackOverflow");
        return;
    case JSC::ParserError::EvalError:
        out.print("EvalError");
        return;
    case JSC::ParserError::OutOfMemory:
        out.print("OutOfMemory");
        return;
    case JSC::ParserError::SyntaxError:
        out.print("SyntaxError");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}
} // namespace WTF
