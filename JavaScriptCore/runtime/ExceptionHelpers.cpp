/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ExceptionHelpers.h"

#include "CodeBlock.h"
#include "CallFrame.h"
#include "JSGlobalObjectFunctions.h"
#include "JSObject.h"
#include "JSNotAnObject.h"
#include "Interpreter.h"
#include "Nodes.h"
#include "StringConcatenate.h"

namespace JSC {

class InterruptedExecutionError : public JSObject {
public:
    InterruptedExecutionError(JSGlobalData* globalData)
        : JSObject(globalData->interruptedExecutionErrorStructure)
    {
    }

    virtual ComplType exceptionType() const { return Interrupted; }

    virtual UString toString(ExecState*) const { return "JavaScript execution exceeded timeout."; }
};

JSValue createInterruptedExecutionException(JSGlobalData* globalData)
{
    return new (globalData) InterruptedExecutionError(globalData);
}

class TerminatedExecutionError : public JSObject {
public:
    TerminatedExecutionError(JSGlobalData* globalData)
        : JSObject(globalData->terminatedExecutionErrorStructure)
    {
    }

    virtual ComplType exceptionType() const { return Terminated; }

    virtual UString toString(ExecState*) const { return "JavaScript execution terminated."; }
};

JSValue createTerminatedExecutionException(JSGlobalData* globalData)
{
    return new (globalData) TerminatedExecutionError(globalData);
}

JSObject* createStackOverflowError(ExecState* exec)
{
    return createRangeError(exec, "Maximum call stack size exceeded.");
}

JSObject* createStackOverflowError(JSGlobalObject* globalObject)
{
    return createRangeError(globalObject, "Maximum call stack size exceeded.");
}

JSValue createUndefinedVariableError(ExecState* exec, const Identifier& ident, unsigned bytecodeOffset, CodeBlock* codeBlock)
{
    int startOffset = 0;
    int endOffset = 0;
    int divotPoint = 0;
    int line = codeBlock->expressionRangeForBytecodeOffset(exec, bytecodeOffset, divotPoint, startOffset, endOffset);
    UString message(makeString("Can't find variable: ", ident.ustring()));
    JSObject* exception = addErrorInfo(exec, createReferenceError(exec, message), line, codeBlock->ownerExecutable()->source(), divotPoint, startOffset, endOffset);
    return exception;
}
    
static UString createErrorMessage(ExecState* exec, CodeBlock* codeBlock, int, int expressionStart, int expressionStop, JSValue value, UString error)
{
    if (!expressionStop || expressionStart > codeBlock->source()->length())
        return makeString(value.toString(exec), " is ", error);
    if (expressionStart < expressionStop)
        return makeString("Result of expression '", codeBlock->source()->getRange(expressionStart, expressionStop), "' [", value.toString(exec), "] is ", error, ".");

    // No range information, so give a few characters of context
    const UChar* data = codeBlock->source()->data();
    int dataLength = codeBlock->source()->length();
    int start = expressionStart;
    int stop = expressionStart;
    // Get up to 20 characters of context to the left and right of the divot, clamping to the line.
    // then strip whitespace.
    while (start > 0 && (expressionStart - start < 20) && data[start - 1] != '\n')
        start--;
    while (start < (expressionStart - 1) && isStrWhiteSpace(data[start]))
        start++;
    while (stop < dataLength && (stop - expressionStart < 20) && data[stop] != '\n')
        stop++;
    while (stop > expressionStart && isStrWhiteSpace(data[stop]))
        stop--;
    return makeString("Result of expression near '...", codeBlock->source()->getRange(start, stop), "...' [", value.toString(exec), "] is ", error, ".");
}

JSObject* createInvalidParamError(ExecState* exec, const char* op, JSValue value, unsigned bytecodeOffset, CodeBlock* codeBlock)
{
    int startOffset = 0;
    int endOffset = 0;
    int divotPoint = 0;
    int line = codeBlock->expressionRangeForBytecodeOffset(exec, bytecodeOffset, divotPoint, startOffset, endOffset);
    UString errorMessage = createErrorMessage(exec, codeBlock, line, divotPoint, divotPoint + endOffset, value, makeString("not a valid argument for '", op, "'"));
    JSObject* exception = addErrorInfo(exec, createTypeError(exec, errorMessage), line, codeBlock->ownerExecutable()->source(), divotPoint, startOffset, endOffset);
    return exception;
}

JSObject* createNotAConstructorError(ExecState* exec, JSValue value, unsigned bytecodeOffset, CodeBlock* codeBlock)
{
    int startOffset = 0;
    int endOffset = 0;
    int divotPoint = 0;
    int line = codeBlock->expressionRangeForBytecodeOffset(exec, bytecodeOffset, divotPoint, startOffset, endOffset);

    // We're in a "new" expression, so we need to skip over the "new.." part
    int startPoint = divotPoint - (startOffset ? startOffset - 4 : 0); // -4 for "new "
    const UChar* data = codeBlock->source()->data();
    while (startPoint < divotPoint && isStrWhiteSpace(data[startPoint]))
        startPoint++;
    
    UString errorMessage = createErrorMessage(exec, codeBlock, line, startPoint, divotPoint, value, "not a constructor");
    JSObject* exception = addErrorInfo(exec, createTypeError(exec, errorMessage), line, codeBlock->ownerExecutable()->source(), divotPoint, startOffset, endOffset);
    return exception;
}

JSValue createNotAFunctionError(ExecState* exec, JSValue value, unsigned bytecodeOffset, CodeBlock* codeBlock)
{
    int startOffset = 0;
    int endOffset = 0;
    int divotPoint = 0;
    int line = codeBlock->expressionRangeForBytecodeOffset(exec, bytecodeOffset, divotPoint, startOffset, endOffset);
    UString errorMessage = createErrorMessage(exec, codeBlock, line, divotPoint - startOffset, divotPoint, value, "not a function");
    JSObject* exception = addErrorInfo(exec, createTypeError(exec, errorMessage), line, codeBlock->ownerExecutable()->source(), divotPoint, startOffset, endOffset);
    return exception;
}

JSNotAnObjectErrorStub* createNotAnObjectErrorStub(ExecState* exec, bool isNull)
{
    return new (exec) JSNotAnObjectErrorStub(exec, isNull);
}

JSObject* createNotAnObjectError(ExecState* exec, JSNotAnObjectErrorStub* error, unsigned bytecodeOffset, CodeBlock* codeBlock)
{
    // Both op_create_this and op_instanceof require a use of op_get_by_id to get
    // the prototype property from an object. The exception messages for exceptions
    // thrown by these instances op_get_by_id need to reflect this.
    OpcodeID followingOpcodeID;
    if (codeBlock->getByIdExceptionInfoForBytecodeOffset(exec, bytecodeOffset, followingOpcodeID)) {
        ASSERT(followingOpcodeID == op_create_this || followingOpcodeID == op_instanceof);
        if (followingOpcodeID == op_create_this)
            return createNotAConstructorError(exec, error->isNull() ? jsNull() : jsUndefined(), bytecodeOffset, codeBlock);
        return createInvalidParamError(exec, "instanceof", error->isNull() ? jsNull() : jsUndefined(), bytecodeOffset, codeBlock);
    }

    int startOffset = 0;
    int endOffset = 0;
    int divotPoint = 0;
    int line = codeBlock->expressionRangeForBytecodeOffset(exec, bytecodeOffset, divotPoint, startOffset, endOffset);
    UString errorMessage = createErrorMessage(exec, codeBlock, line, divotPoint - startOffset, divotPoint, error->isNull() ? jsNull() : jsUndefined(), "not an object");
    JSObject* exception = addErrorInfo(exec, createTypeError(exec, errorMessage), line, codeBlock->ownerExecutable()->source(), divotPoint, startOffset, endOffset);
    return exception;
}

JSValue throwOutOfMemoryError(ExecState* exec)
{
    return throwError(exec, createError(exec, "Out of memory"));
}

} // namespace JSC
