/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef ExecutionHarness_h
#define ExecutionHarness_h

#include <wtf/Platform.h>

#if ENABLE(JIT)

#include "CompilationResult.h"
#include "DFGDriver.h"
#include "DFGPlan.h"
#include "JITDriver.h"
#include "LLIntEntrypoints.h"

namespace JSC {

template<typename CodeBlockType>
inline CompilationResult prepareForExecutionImpl(
    ExecState* exec, CodeBlockType* codeBlock, RefPtr<JITCode>& jitCode,
    JITCode::JITType jitType, unsigned bytecodeIndex)
{
#if ENABLE(LLINT)
    if (JITCode::isBaselineCode(jitType)) {
        // Start off in the low level interpreter.
        LLInt::getEntrypoint(exec->vm(), codeBlock, jitCode);
        if (exec->vm().m_perBytecodeProfiler)
            exec->vm().m_perBytecodeProfiler->ensureBytecodesFor(codeBlock);
        return CompilationSuccessful;
    }
#endif // ENABLE(LLINT)
#if ENABLE(JIT)
    return jitCompileIfAppropriateImpl(
        exec, codeBlock, jitCode, jitType, bytecodeIndex,
        JITCode::isBaselineCode(jitType) ? JITCompilationMustSucceed : JITCompilationCanFail);
#endif
    RELEASE_ASSERT_NOT_REACHED();
    return CompilationFailed;
}

inline CompilationResult prepareFunctionForExecutionImpl(
    ExecState* exec, FunctionCodeBlock* codeBlock, RefPtr<JITCode>& jitCode,
    MacroAssemblerCodePtr& jitCodeWithArityCheck, JITCode::JITType jitType,
    unsigned bytecodeIndex, CodeSpecializationKind kind)
{
#if ENABLE(LLINT)
    if (JITCode::isBaselineCode(jitType)) {
        // Start off in the low level interpreter.
        LLInt::getFunctionEntrypoint(exec->vm(), kind, jitCode, jitCodeWithArityCheck);
        if (exec->vm().m_perBytecodeProfiler)
            exec->vm().m_perBytecodeProfiler->ensureBytecodesFor(codeBlock);
        return CompilationSuccessful;
    }
#else
    UNUSED_PARAM(kind);
#endif // ENABLE(LLINT)
#if ENABLE(JIT)
    return jitCompileFunctionIfAppropriateImpl(
        exec, codeBlock, jitCode, jitCodeWithArityCheck, jitType, bytecodeIndex,
        JITCode::isBaselineCode(jitType) ? JITCompilationMustSucceed : JITCompilationCanFail);
#endif
    RELEASE_ASSERT_NOT_REACHED();
    return CompilationFailed;
}

template<typename CodeBlockType>
inline CompilationResult installOptimizedCode(
    CompilationResult result, RefPtr<CodeBlockType>& sink, CodeBlockType* codeBlock,
    PassRefPtr<JITCode> jitCode, MacroAssemblerCodePtr jitCodeWithArityCheck,
    int* numParameters)
{
    if (result != CompilationSuccessful)
        return result;
    
    codeBlock->setJITCode(jitCode, jitCodeWithArityCheck);
    
    sink = codeBlock;
    
    if (numParameters)
        *numParameters = codeBlock->numParameters();
    
    if (jitCode)
        codeBlock->vm()->heap.reportExtraMemoryCost(sizeof(*codeBlock) + jitCode->size());
    else
        codeBlock->vm()->heap.reportExtraMemoryCost(sizeof(*codeBlock));
    
    if (sink != codeBlock) {
        // This can happen if we GC and decide that the code is invalid.
        return CompilationInvalidated;
    }
    
    return result;
}

template<typename CodeBlockType>
inline CompilationResult prepareForExecution(
    ExecState* exec, RefPtr<CodeBlockType>& sink, CodeBlockType* codeBlock,
    RefPtr<JITCode>& jitCode, JITCode::JITType jitType, unsigned bytecodeIndex)
{
    return installOptimizedCode(
        prepareForExecutionImpl(
            exec, codeBlock, jitCode, jitType, bytecodeIndex),
        sink, codeBlock, jitCode, MacroAssemblerCodePtr(), 0);
}

inline CompilationResult prepareFunctionForExecution(
    ExecState* exec, RefPtr<FunctionCodeBlock>& sink, FunctionCodeBlock* codeBlock,
    RefPtr<JITCode>& jitCode, MacroAssemblerCodePtr& jitCodeWithArityCheck,
    int& numParameters, JITCode::JITType jitType, unsigned bytecodeIndex,
    CodeSpecializationKind kind)
{
    return installOptimizedCode(
        prepareFunctionForExecutionImpl(
            exec, codeBlock, jitCode, jitCodeWithArityCheck, jitType,
            bytecodeIndex, kind),
        sink, codeBlock, jitCode, jitCodeWithArityCheck, &numParameters);
}

#if ENABLE(DFG_JIT)
template<typename CodeBlockType>
inline CompilationResult replaceWithDeferredOptimizedCode(
    PassRefPtr<DFG::Plan> passedPlan, RefPtr<CodeBlockType>& sink, RefPtr<JITCode>& jitCode,
    MacroAssemblerCodePtr* jitCodeWithArityCheck, int* numParameters)
{
    RefPtr<DFG::Plan> plan = passedPlan;
    CompilationResult result = DFG::tryFinalizePlan(plan, jitCode, jitCodeWithArityCheck);
    if (Options::verboseOSR()) {
        dataLog(
            "Deferred optimizing compilation ", *plan->key(), " -> ", *plan->codeBlock,
            " result: ", result, "\n");
    }
    if (result == CompilationSuccessful)
        plan->codeBlock->alternative()->unlinkIncomingCalls();
    return installOptimizedCode(
        result, sink, static_cast<CodeBlockType*>(plan->codeBlock.get()), jitCode,
        jitCodeWithArityCheck ? *jitCodeWithArityCheck : MacroAssemblerCodePtr(),
        numParameters);
}
#endif // ENABLE(DFG_JIT)

} // namespace JSC

#endif // ENABLE(JIT)

#endif // ExecutionHarness_h

