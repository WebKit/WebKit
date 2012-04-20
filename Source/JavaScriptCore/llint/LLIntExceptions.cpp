/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "LLIntExceptions.h"

#if ENABLE(LLINT)

#include "CallFrame.h"
#include "CodeBlock.h"
#include "Instruction.h"
#include "JITExceptions.h"
#include "LLIntCommon.h"
#include "LowLevelInterpreter.h"

namespace JSC { namespace LLInt {

void interpreterThrowInCaller(ExecState* exec, ReturnAddressPtr pc)
{
    JSGlobalData* globalData = &exec->globalData();
    NativeCallFrameTracer tracer(globalData, exec);
#if LLINT_SLOW_PATH_TRACING
    dataLog("Throwing exception %s.\n", globalData->exception.description());
#endif
    genericThrow(
        globalData, exec, globalData->exception,
        exec->codeBlock()->bytecodeOffset(exec, pc));
}

Instruction* returnToThrowForThrownException(ExecState* exec)
{
    return exec->globalData().llintData.exceptionInstructions();
}

Instruction* returnToThrow(ExecState* exec, Instruction* pc)
{
    JSGlobalData* globalData = &exec->globalData();
    NativeCallFrameTracer tracer(globalData, exec);
#if LLINT_SLOW_PATH_TRACING
    dataLog("Throwing exception %s (returnToThrow).\n", globalData->exception.description());
#endif
    genericThrow(globalData, exec, globalData->exception, pc - exec->codeBlock()->instructions().begin());
    
    return globalData->llintData.exceptionInstructions();
}

void* callToThrow(ExecState* exec, Instruction* pc)
{
    JSGlobalData* globalData = &exec->globalData();
    NativeCallFrameTracer tracer(globalData, exec);
#if LLINT_SLOW_PATH_TRACING
    dataLog("Throwing exception %s (callToThrow).\n", globalData->exception.description());
#endif
    genericThrow(globalData, exec, globalData->exception, pc - exec->codeBlock()->instructions().begin());
    
    return bitwise_cast<void*>(&llint_throw_during_call_trampoline);
}

} } // namespace JSC::LLInt

#endif // ENABLE(LLINT)
